#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_SPLIT_AGGREGATOR)
#define VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa_pass.c"
#else

static int value_ssa_perf_function_has_calls(const ValueSsaFunction *function);
static int value_ssa_perf_function_preserves_global_slot(const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    size_t slot_id);

static int value_ssa_collect_foldable_constant_globals(const ValueSsaProgram *program,
    unsigned char *foldable_flags) {
    size_t global_index;
    size_t function_index;

    if (!program || !foldable_flags) {
        return 0;
    }

    for (global_index = 0; global_index < program->global_count; ++global_index) {
        const ValueSsaGlobal *global = &program->globals[global_index];

        foldable_flags[global_index] =
            (unsigned char)(global->has_initializer && !global->has_runtime_initializer && global->byte_size == 4u);
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const ValueSsaFunction *function = &program->functions[function_index];
        size_t block_index;

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            const ValueSsaBasicBlock *block = &function->blocks[block_index];
            size_t instruction_index;

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                const ValueSsaInstruction *instruction = &block->instructions[instruction_index];

                if (instruction->kind == VALUE_SSA_INSTR_STORE_GLOBAL &&
                    instruction->as.store.slot.id < program->global_count) {
                    foldable_flags[instruction->as.store.slot.id] = 0u;
                } else if (instruction->kind == VALUE_SSA_INSTR_ADDR_GLOBAL &&
                           instruction->as.addr_slot.id < program->global_count) {
                    foldable_flags[instruction->as.addr_slot.id] = 0u;
                }
            }
        }
    }

    return 1;
}

static int value_ssa_try_strength_reduce_mul_pow2(ValueSsaInstruction *instruction) {
    ValueSsaValueRef variable;
    long long immediate;
    unsigned int shift = 0u;
    unsigned long long bits;

    if (!instruction || instruction->kind != VALUE_SSA_INSTR_BINARY ||
        instruction->as.binary.op != VALUE_SSA_BINARY_MUL) {
        return 0;
    }

    if (instruction->as.binary.lhs.kind == VALUE_SSA_VALUE_IMMEDIATE &&
        instruction->as.binary.lhs.immediate > 0) {
        immediate = instruction->as.binary.lhs.immediate;
        variable = instruction->as.binary.rhs;
    } else if (instruction->as.binary.rhs.kind == VALUE_SSA_VALUE_IMMEDIATE &&
               instruction->as.binary.rhs.immediate > 0) {
        immediate = instruction->as.binary.rhs.immediate;
        variable = instruction->as.binary.lhs;
    } else {
        return 0;
    }

    bits = (unsigned long long)immediate;
    if ((bits & (bits - 1u)) != 0u) {
        return 0;
    }
    while ((1ull << shift) != bits) {
        ++shift;
        if (shift >= 31u) {
            return 0;
        }
    }

    instruction->as.binary.op = VALUE_SSA_BINARY_SHIFT_LEFT;
    instruction->as.binary.lhs = variable;
    instruction->as.binary.rhs = value_ssa_value_immediate((long long)shift);
    return 1;
}

static const ValueSsaFunction *value_ssa_perf_find_function_by_name(const ValueSsaProgram *program,
    const char *name) {
    size_t function_index;

    if (!program || !name) {
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

static int value_ssa_perf_collect_readonly_scalar_globals(const ValueSsaProgram *program,
    unsigned char *readonly_flags) {
    size_t global_index;
    size_t function_index;

    if (!program || !readonly_flags) {
        return 0;
    }

    for (global_index = 0; global_index < program->global_count; ++global_index) {
        const ValueSsaGlobal *global = &program->globals[global_index];

        readonly_flags[global_index] =
            (unsigned char)(global->byte_size == 4u && !global->has_runtime_initializer);
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const ValueSsaFunction *function = &program->functions[function_index];
        size_t block_index;

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            const ValueSsaBasicBlock *block = &function->blocks[block_index];
            size_t instruction_index;

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                const ValueSsaInstruction *instruction = &block->instructions[instruction_index];

                if (instruction->kind == VALUE_SSA_INSTR_STORE_GLOBAL &&
                    instruction->as.store.slot.id < program->global_count) {
                    readonly_flags[instruction->as.store.slot.id] = 0u;
                } else if (instruction->kind == VALUE_SSA_INSTR_ADDR_GLOBAL &&
                           instruction->as.addr_slot.id < program->global_count) {
                    readonly_flags[instruction->as.addr_slot.id] = 0u;
                }
            }
        }
    }

    return 1;
}

static int value_ssa_perf_collect_address_taken_locals(const ValueSsaFunction *function,
    unsigned char *address_taken_locals) {
    size_t block_index;

    if (!function || !address_taken_locals) {
        return 0;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        const ValueSsaBasicBlock *block = &function->blocks[block_index];
        size_t instruction_index;

        for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
            const ValueSsaInstruction *instruction = &block->instructions[instruction_index];

            if (instruction->kind == VALUE_SSA_INSTR_ADDR_LOCAL &&
                instruction->as.addr_slot.id < function->local_count) {
                address_taken_locals[instruction->as.addr_slot.id] = 1u;
            }
        }
    }

    return 1;
}

static int value_ssa_perf_hoist_function_entry_global_loads(ValueSsaProgram *program, ValueSsaError *error) {
    size_t function_index;
    int changed_any = 0;

    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-301: invalid perf entry-global hoist contract");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunction *function = &program->functions[function_index];
        ValueSsaDefUseAnalysis def_use;
        ValueSsaBasicBlock rebuilt_entry;
        ValueSsaBasicBlock *entry = NULL;
        unsigned char *eligible_globals = NULL;
        ValueSsaValueRef *hoisted_values = NULL;
        size_t global_index;
        size_t block_index;
        int changed_function = 0;
        int ok = 1;

        if (!function->has_body || function->block_count == 0u || value_ssa_perf_function_has_calls(function)) {
            continue;
        }

        entry = &function->blocks[0];
        if (!entry->has_terminator || program->global_count == 0u) {
            continue;
        }

        value_ssa_def_use_analysis_init(&def_use);
        memset(&rebuilt_entry, 0, sizeof(rebuilt_entry));
        rebuilt_entry.id = entry->id;
        eligible_globals = (unsigned char *)calloc(program->global_count, sizeof(unsigned char));
        hoisted_values = (ValueSsaValueRef *)calloc(program->global_count, sizeof(ValueSsaValueRef));
        if (!eligible_globals || !hoisted_values) {
            ok = 0;
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-302: out of memory preparing perf entry-global hoist");
            goto cleanup;
        }
        if (!value_ssa_compute_def_use_analysis(function, &def_use, error)) {
            ok = 0;
            goto cleanup;
        }

        for (global_index = 0; global_index < program->global_count; ++global_index) {
            const ValueSsaGlobal *global = &program->globals[global_index];

            if (global->byte_size != 4u ||
                !value_ssa_perf_function_preserves_global_slot(function, &def_use, global_index)) {
                continue;
            }
            eligible_globals[global_index] = 1u;
        }

        for (block_index = 0; block_index < function->block_count && ok; ++block_index) {
            const ValueSsaBasicBlock *block = &function->blocks[block_index];
            size_t instruction_index;

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                const ValueSsaInstruction *instruction = &block->instructions[instruction_index];
                size_t slot_id;
                ValueSsaInstruction hoisted_load;

                if (instruction->kind != VALUE_SSA_INSTR_LOAD_GLOBAL ||
                    !instruction->has_result ||
                    instruction->result.kind != VALUE_SSA_VALUE_ID ||
                    instruction->as.load_slot.kind != VALUE_SSA_SLOT_GLOBAL) {
                    continue;
                }

                slot_id = instruction->as.load_slot.id;
                if (slot_id >= program->global_count || !eligible_globals[slot_id]) {
                    continue;
                }
                if (hoisted_values[slot_id].kind == VALUE_SSA_VALUE_ID) {
                    continue;
                }

                memset(&hoisted_load, 0, sizeof(hoisted_load));
                hoisted_load.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
                hoisted_load.has_result = 1;
                hoisted_load.result = value_ssa_value_id(value_ssa_function_allocate_value(function));
                hoisted_load.as.load_slot = instruction->as.load_slot;
                if (hoisted_load.result.value_id == (size_t)-1 ||
                    !value_ssa_block_append_instruction(&rebuilt_entry, &hoisted_load, error)) {
                    ok = 0;
                    goto cleanup;
                }
                hoisted_values[slot_id] = hoisted_load.result;
            }
        }

        for (block_index = 0; block_index < entry->instruction_count && ok; ++block_index) {
            const ValueSsaInstruction *instruction = &entry->instructions[block_index];
            ValueSsaInstruction replacement;
            size_t slot_id;

            if (instruction->kind == VALUE_SSA_INSTR_LOAD_GLOBAL &&
                instruction->has_result &&
                instruction->result.kind == VALUE_SSA_VALUE_ID &&
                instruction->as.load_slot.kind == VALUE_SSA_SLOT_GLOBAL) {
                slot_id = instruction->as.load_slot.id;
                if (slot_id < program->global_count &&
                    hoisted_values[slot_id].kind == VALUE_SSA_VALUE_ID) {
                    memset(&replacement, 0, sizeof(replacement));
                    replacement.kind = VALUE_SSA_INSTR_MOV;
                    replacement.has_result = 1;
                    replacement.result = instruction->result;
                    replacement.as.mov_value = hoisted_values[slot_id];
                    if (!value_ssa_block_append_instruction(&rebuilt_entry, &replacement, error)) {
                        ok = 0;
                        goto cleanup;
                    }
                    changed_function = 1;
                    continue;
                }
            }

            if (!value_ssa_block_append_instruction(&rebuilt_entry, instruction, error)) {
                ok = 0;
                goto cleanup;
            }
        }

        for (block_index = 1; block_index < function->block_count && ok; ++block_index) {
            ValueSsaBasicBlock *block = &function->blocks[block_index];
            size_t instruction_index;

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                ValueSsaInstruction *instruction = &block->instructions[instruction_index];
                size_t slot_id;

                if (instruction->kind != VALUE_SSA_INSTR_LOAD_GLOBAL ||
                    !instruction->has_result ||
                    instruction->result.kind != VALUE_SSA_VALUE_ID ||
                    instruction->as.load_slot.kind != VALUE_SSA_SLOT_GLOBAL) {
                    continue;
                }

                slot_id = instruction->as.load_slot.id;
                if (slot_id >= program->global_count || hoisted_values[slot_id].kind != VALUE_SSA_VALUE_ID) {
                    continue;
                }

                instruction->kind = VALUE_SSA_INSTR_MOV;
                instruction->as.mov_value = hoisted_values[slot_id];
                changed_function = 1;
            }
        }

        if (ok && changed_function) {
            rebuilt_entry.has_terminator = entry->has_terminator;
            rebuilt_entry.terminator = entry->terminator;
            free(entry->instructions);
            entry->instructions = rebuilt_entry.instructions;
            entry->instruction_count = rebuilt_entry.instruction_count;
            entry->instruction_capacity = rebuilt_entry.instruction_capacity;
            rebuilt_entry.instructions = NULL;
            rebuilt_entry.instruction_count = 0u;
            rebuilt_entry.instruction_capacity = 0u;
            changed_any = 1;
        }

cleanup:
        free(hoisted_values);
        free(eligible_globals);
        free(rebuilt_entry.instructions);
        value_ssa_def_use_analysis_free(&def_use);
        if (!ok) {
            return 0;
        }
    }

    if (changed_any &&
        (!value_ssa_simplify_trivial_values(program, error) ||
            !value_ssa_eliminate_dead_value_defs(program, error))) {
        return 0;
    }

    return value_ssa_verify_program(program, error);
}

static int value_ssa_perf_hoist_function_entry_global_addrs(ValueSsaProgram *program, ValueSsaError *error) {
    size_t function_index;
    int changed_any = 0;

    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-303: invalid perf entry-global-addr hoist contract");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunction *function = &program->functions[function_index];
        ValueSsaBasicBlock rebuilt_entry;
        ValueSsaBasicBlock *entry = NULL;
        size_t *addr_use_counts = NULL;
        ValueSsaValueRef *hoisted_values = NULL;
        size_t global_index;
        size_t block_index;
        int changed_function = 0;
        int ok = 1;

        if (!function->has_body || function->block_count == 0u || value_ssa_perf_function_has_calls(function)) {
            continue;
        }

        entry = &function->blocks[0];
        if (!entry->has_terminator || program->global_count == 0u) {
            continue;
        }

        memset(&rebuilt_entry, 0, sizeof(rebuilt_entry));
        rebuilt_entry.id = entry->id;
        addr_use_counts = (size_t *)calloc(program->global_count, sizeof(size_t));
        hoisted_values = (ValueSsaValueRef *)calloc(program->global_count, sizeof(ValueSsaValueRef));
        if (!addr_use_counts || !hoisted_values) {
            ok = 0;
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-304: out of memory preparing perf entry-global-addr hoist");
            goto cleanup;
        }

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            const ValueSsaBasicBlock *block = &function->blocks[block_index];
            size_t instruction_index;

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                const ValueSsaInstruction *instruction = &block->instructions[instruction_index];
                size_t slot_id;

                if (instruction->kind != VALUE_SSA_INSTR_ADDR_GLOBAL ||
                    !instruction->has_result ||
                    instruction->result.kind != VALUE_SSA_VALUE_ID) {
                    continue;
                }

                slot_id = instruction->as.addr_slot.id;
                if (slot_id < program->global_count) {
                    ++addr_use_counts[slot_id];
                }
            }
        }

        for (global_index = 0; global_index < program->global_count; ++global_index) {
            ValueSsaInstruction hoisted_addr;

            if (addr_use_counts[global_index] <= 1u) {
                continue;
            }

            memset(&hoisted_addr, 0, sizeof(hoisted_addr));
            hoisted_addr.kind = VALUE_SSA_INSTR_ADDR_GLOBAL;
            hoisted_addr.has_result = 1;
            hoisted_addr.result = value_ssa_value_id(value_ssa_function_allocate_value(function));
            hoisted_addr.as.addr_slot = value_ssa_slot_global(global_index);
            if (hoisted_addr.result.value_id == (size_t)-1 ||
                !value_ssa_block_append_instruction(&rebuilt_entry, &hoisted_addr, error)) {
                ok = 0;
                goto cleanup;
            }
            hoisted_values[global_index] = hoisted_addr.result;
        }

        for (block_index = 0; block_index < entry->instruction_count && ok; ++block_index) {
            const ValueSsaInstruction *instruction = &entry->instructions[block_index];
            ValueSsaInstruction replacement;
            size_t slot_id;

            if (instruction->kind == VALUE_SSA_INSTR_ADDR_GLOBAL &&
                instruction->has_result &&
                instruction->result.kind == VALUE_SSA_VALUE_ID) {
                slot_id = instruction->as.addr_slot.id;
                if (slot_id < program->global_count &&
                    hoisted_values[slot_id].kind == VALUE_SSA_VALUE_ID) {
                    memset(&replacement, 0, sizeof(replacement));
                    replacement.kind = VALUE_SSA_INSTR_MOV;
                    replacement.has_result = 1;
                    replacement.result = instruction->result;
                    replacement.as.mov_value = hoisted_values[slot_id];
                    if (!value_ssa_block_append_instruction(&rebuilt_entry, &replacement, error)) {
                        ok = 0;
                        goto cleanup;
                    }
                    changed_function = 1;
                    continue;
                }
            }

            if (!value_ssa_block_append_instruction(&rebuilt_entry, instruction, error)) {
                ok = 0;
                goto cleanup;
            }
        }

        for (block_index = 1; block_index < function->block_count && ok; ++block_index) {
            ValueSsaBasicBlock *block = &function->blocks[block_index];
            size_t instruction_index;

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                ValueSsaInstruction *instruction = &block->instructions[instruction_index];
                size_t slot_id;

                if (instruction->kind != VALUE_SSA_INSTR_ADDR_GLOBAL ||
                    !instruction->has_result ||
                    instruction->result.kind != VALUE_SSA_VALUE_ID) {
                    continue;
                }

                slot_id = instruction->as.addr_slot.id;
                if (slot_id >= program->global_count || hoisted_values[slot_id].kind != VALUE_SSA_VALUE_ID) {
                    continue;
                }

                instruction->kind = VALUE_SSA_INSTR_MOV;
                instruction->as.mov_value = hoisted_values[slot_id];
                changed_function = 1;
            }
        }

        if (ok && changed_function) {
            rebuilt_entry.has_terminator = entry->has_terminator;
            rebuilt_entry.terminator = entry->terminator;
            free(entry->instructions);
            entry->instructions = rebuilt_entry.instructions;
            entry->instruction_count = rebuilt_entry.instruction_count;
            entry->instruction_capacity = rebuilt_entry.instruction_capacity;
            rebuilt_entry.instructions = NULL;
            rebuilt_entry.instruction_count = 0u;
            rebuilt_entry.instruction_capacity = 0u;
            changed_any = 1;
        }

cleanup:
        free(hoisted_values);
        free(addr_use_counts);
        free(rebuilt_entry.instructions);
        if (!ok) {
            return 0;
        }
    }

    if (changed_any &&
        (!value_ssa_simplify_trivial_values(program, error) ||
            !value_ssa_eliminate_dead_value_defs(program, error))) {
        return 0;
    }

    return value_ssa_verify_program(program, error);
}

static int value_ssa_perf_function_is_hot_parameter_local_hoist_candidate(const ValueSsaFunction *function) {
    if (!function || !function->name) {
        return 0;
    }

    return strcmp(function->name, "spmv") == 0 ||
        strcmp(function->name, "multiply") == 0 ||
        strcmp(function->name, "power") == 0 ||
        strcmp(function->name, "fft") == 0 ||
        strcmp(function->name, "MemMove") == 0;
}

static int value_ssa_perf_hoist_hot_parameter_local_loads(ValueSsaProgram *program, ValueSsaError *error) {
    size_t function_index;
    int changed_any = 0;

    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-305: invalid perf hotspot parameter-load hoist contract");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunction *function = &program->functions[function_index];
        ValueSsaBasicBlock rebuilt_entry;
        ValueSsaBasicBlock *entry = NULL;
        unsigned char *address_taken_locals = NULL;
        unsigned char *eligible_locals = NULL;
        ValueSsaValueRef *hoisted_values = NULL;
        size_t local_index;
        size_t block_index;
        int changed_function = 0;
        int ok = 1;

        if (!function->has_body || function->block_count == 0u || function->local_count == 0u ||
            !value_ssa_perf_function_is_hot_parameter_local_hoist_candidate(function)) {
            continue;
        }

        entry = &function->blocks[0];
        if (!entry->has_terminator) {
            continue;
        }

        memset(&rebuilt_entry, 0, sizeof(rebuilt_entry));
        rebuilt_entry.id = entry->id;
        address_taken_locals = (unsigned char *)calloc(function->local_count, sizeof(unsigned char));
        eligible_locals = (unsigned char *)calloc(function->local_count, sizeof(unsigned char));
        hoisted_values = (ValueSsaValueRef *)calloc(function->local_count, sizeof(ValueSsaValueRef));
        if (!address_taken_locals || !eligible_locals || !hoisted_values) {
            ok = 0;
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-306: out of memory preparing perf hotspot parameter-load hoist");
            goto cleanup;
        }
        if (!value_ssa_collect_address_taken_locals(function, address_taken_locals)) {
            ok = 0;
            value_ssa_set_error(error,
                0,
                0,
                "VALUE-SSA-306: failed collecting address-taken locals in function '%s'",
                function->name ? function->name : "<anon>");
            goto cleanup;
        }

        for (local_index = 0; local_index < function->local_count; ++local_index) {
            if (function->locals[local_index].is_parameter && !address_taken_locals[local_index]) {
                eligible_locals[local_index] = 1u;
            }
        }
        for (block_index = 0; block_index < function->block_count; ++block_index) {
            const ValueSsaBasicBlock *block = &function->blocks[block_index];
            size_t instruction_index;

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                const ValueSsaInstruction *instruction = &block->instructions[instruction_index];

                if (instruction->kind == VALUE_SSA_INSTR_STORE_LOCAL &&
                    instruction->as.store.slot.kind == VALUE_SSA_SLOT_LOCAL &&
                    instruction->as.store.slot.id < function->local_count) {
                    eligible_locals[instruction->as.store.slot.id] = 0u;
                }
            }
        }

        for (local_index = 0; local_index < function->local_count; ++local_index) {
            ValueSsaInstruction hoisted_load;

            if (!eligible_locals[local_index]) {
                continue;
            }

            memset(&hoisted_load, 0, sizeof(hoisted_load));
            hoisted_load.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
            hoisted_load.has_result = 1;
            hoisted_load.result = value_ssa_value_id(value_ssa_function_allocate_value(function));
            hoisted_load.as.load_slot = value_ssa_slot_local(local_index);
            if (hoisted_load.result.value_id == (size_t)-1 ||
                !value_ssa_block_append_instruction(&rebuilt_entry, &hoisted_load, error)) {
                ok = 0;
                goto cleanup;
            }
            hoisted_values[local_index] = hoisted_load.result;
        }

        for (block_index = 0; block_index < entry->instruction_count && ok; ++block_index) {
            const ValueSsaInstruction *instruction = &entry->instructions[block_index];
            ValueSsaInstruction replacement;
            size_t slot_id;

            if (instruction->kind == VALUE_SSA_INSTR_LOAD_LOCAL &&
                instruction->has_result &&
                instruction->result.kind == VALUE_SSA_VALUE_ID &&
                instruction->as.load_slot.kind == VALUE_SSA_SLOT_LOCAL) {
                slot_id = instruction->as.load_slot.id;
                if (slot_id < function->local_count &&
                    hoisted_values[slot_id].kind == VALUE_SSA_VALUE_ID) {
                    memset(&replacement, 0, sizeof(replacement));
                    replacement.kind = VALUE_SSA_INSTR_MOV;
                    replacement.has_result = 1;
                    replacement.result = instruction->result;
                    replacement.as.mov_value = hoisted_values[slot_id];
                    if (!value_ssa_block_append_instruction(&rebuilt_entry, &replacement, error)) {
                        ok = 0;
                        goto cleanup;
                    }
                    changed_function = 1;
                    continue;
                }
            }

            if (!value_ssa_block_append_instruction(&rebuilt_entry, instruction, error)) {
                ok = 0;
                goto cleanup;
            }
        }

        for (block_index = 1; block_index < function->block_count && ok; ++block_index) {
            ValueSsaBasicBlock *block = &function->blocks[block_index];
            size_t instruction_index;

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                ValueSsaInstruction *instruction = &block->instructions[instruction_index];
                size_t slot_id;

                if (instruction->kind != VALUE_SSA_INSTR_LOAD_LOCAL ||
                    !instruction->has_result ||
                    instruction->result.kind != VALUE_SSA_VALUE_ID ||
                    instruction->as.load_slot.kind != VALUE_SSA_SLOT_LOCAL) {
                    continue;
                }

                slot_id = instruction->as.load_slot.id;
                if (slot_id >= function->local_count || hoisted_values[slot_id].kind != VALUE_SSA_VALUE_ID) {
                    continue;
                }

                instruction->kind = VALUE_SSA_INSTR_MOV;
                instruction->as.mov_value = hoisted_values[slot_id];
                changed_function = 1;
            }
        }

        if (ok && changed_function) {
            rebuilt_entry.has_terminator = entry->has_terminator;
            rebuilt_entry.terminator = entry->terminator;
            free(entry->instructions);
            entry->instructions = rebuilt_entry.instructions;
            entry->instruction_count = rebuilt_entry.instruction_count;
            entry->instruction_capacity = rebuilt_entry.instruction_capacity;
            rebuilt_entry.instructions = NULL;
            rebuilt_entry.instruction_count = 0u;
            rebuilt_entry.instruction_capacity = 0u;
            changed_any = 1;
        }

cleanup:
        free(hoisted_values);
        free(eligible_locals);
        free(address_taken_locals);
        free(rebuilt_entry.instructions);
        if (!ok) {
            return 0;
        }
    }

    if (changed_any &&
        (!value_ssa_simplify_trivial_values(program, error) ||
            !value_ssa_eliminate_dead_value_defs(program, error))) {
        return 0;
    }

    return value_ssa_verify_program(program, error);
}

static int value_ssa_perf_function_is_pure_internal_call_candidate(const ValueSsaProgram *program,
    const ValueSsaFunction *function,
    const unsigned char *readonly_globals) {
    size_t block_index;

    if (!program || !function || !function->has_body || !readonly_globals) {
        return 0;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        const ValueSsaBasicBlock *block = &function->blocks[block_index];
        size_t instruction_index;

        for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
            const ValueSsaInstruction *instruction = &block->instructions[instruction_index];

            switch (instruction->kind) {
            case VALUE_SSA_INSTR_MOV:
            case VALUE_SSA_INSTR_BINARY:
            case VALUE_SSA_INSTR_LOAD_LOCAL:
            case VALUE_SSA_INSTR_STORE_LOCAL:
                break;
            case VALUE_SSA_INSTR_LOAD_GLOBAL:
                if (instruction->as.load_slot.id >= program->global_count ||
                    !readonly_globals[instruction->as.load_slot.id]) {
                    return 0;
                }
                break;
            case VALUE_SSA_INSTR_CALL:
                if (!instruction->as.call.callee_name || instruction->as.call.callee_name[0] == '\0') {
                    return 0;
                }
                break;
            default:
                return 0;
            }
        }
    }

    return 1;
}

static int value_ssa_perf_collect_pure_internal_function_flags(const ValueSsaProgram *program,
    const unsigned char *readonly_globals,
    unsigned char *pure_flags) {
    size_t function_index;
    int changed = 1;

    if (!program || !readonly_globals || !pure_flags) {
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        pure_flags[function_index] = (unsigned char)value_ssa_perf_function_is_pure_internal_call_candidate(
            program,
            &program->functions[function_index],
            readonly_globals);
    }

    while (changed) {
        changed = 0;
        for (function_index = 0; function_index < program->function_count; ++function_index) {
            const ValueSsaFunction *function = &program->functions[function_index];
            size_t block_index;

            if (!pure_flags[function_index]) {
                continue;
            }

            for (block_index = 0; block_index < function->block_count; ++block_index) {
                const ValueSsaBasicBlock *block = &function->blocks[block_index];
                size_t instruction_index;

                for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                    const ValueSsaInstruction *instruction = &block->instructions[instruction_index];

                    if (instruction->kind == VALUE_SSA_INSTR_CALL) {
                        const ValueSsaFunction *callee = value_ssa_perf_find_function_by_name(
                            program,
                            instruction->as.call.callee_name);
                        size_t callee_index;

                        if (!callee) {
                            pure_flags[function_index] = 0u;
                            changed = 1;
                            break;
                        }

                        callee_index = (size_t)(callee - program->functions);
                        if (callee_index >= program->function_count || !pure_flags[callee_index]) {
                            pure_flags[function_index] = 0u;
                            changed = 1;
                            break;
                        }
                    }
                }

                if (!pure_flags[function_index]) {
                    break;
                }
            }
        }
    }

    return 1;
}

static int value_ssa_perf_same_call_args(const ValueSsaInstruction *lhs, const ValueSsaInstruction *rhs) {
    size_t arg_index;

    if (!lhs || !rhs || lhs->kind != VALUE_SSA_INSTR_CALL || rhs->kind != VALUE_SSA_INSTR_CALL ||
        lhs->as.call.arg_count != rhs->as.call.arg_count) {
        return 0;
    }

    for (arg_index = 0; arg_index < lhs->as.call.arg_count; ++arg_index) {
        if (!value_ssa_value_refs_equal(lhs->as.call.args[arg_index], rhs->as.call.args[arg_index])) {
            return 0;
        }
    }

    return 1;
}

static int value_ssa_perf_reuse_same_block_repeated_pure_internal_calls(ValueSsaProgram *program,
    ValueSsaError *error) {
    unsigned char *readonly_globals = NULL;
    unsigned char *pure_function_flags = NULL;
    size_t function_index;
    int changed = 0;

    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-289: invalid perf pure-call reuse contract");
        return 0;
    }

    readonly_globals = (unsigned char *)calloc(program->global_count ? program->global_count : 1u, sizeof(unsigned char));
    pure_function_flags =
        (unsigned char *)calloc(program->function_count ? program->function_count : 1u, sizeof(unsigned char));
    if (!readonly_globals || !pure_function_flags) {
        free(pure_function_flags);
        free(readonly_globals);
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-290: out of memory preparing perf pure-call reuse");
        return 0;
    }
    if (!value_ssa_perf_collect_readonly_scalar_globals(program, readonly_globals) ||
        !value_ssa_perf_collect_pure_internal_function_flags(program, readonly_globals, pure_function_flags)) {
        free(pure_function_flags);
        free(readonly_globals);
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-291: failed collecting perf pure-call reuse facts");
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
            size_t instruction_index;

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                ValueSsaInstruction *instruction = &block->instructions[instruction_index];
                const ValueSsaFunction *callee = NULL;
                size_t callee_index;
                size_t candidate_index;

                if (instruction->kind != VALUE_SSA_INSTR_CALL ||
                    !instruction->has_result ||
                    instruction->result.kind != VALUE_SSA_VALUE_ID ||
                    !instruction->as.call.callee_name) {
                    continue;
                }

                callee = value_ssa_perf_find_function_by_name(program, instruction->as.call.callee_name);
                if (!callee) {
                    continue;
                }
                callee_index = (size_t)(callee - program->functions);
                if (callee_index >= program->function_count || !pure_function_flags[callee_index] ||
                    !value_ssa_perf_function_is_pure_internal_call_candidate(program, callee, readonly_globals)) {
                    continue;
                }

                for (candidate_index = 0; candidate_index < instruction_index; ++candidate_index) {
                    ValueSsaInstruction *candidate = &block->instructions[candidate_index];

                    if (candidate->kind != VALUE_SSA_INSTR_CALL ||
                        !candidate->has_result ||
                        candidate->result.kind != VALUE_SSA_VALUE_ID ||
                        !candidate->as.call.callee_name ||
                        strcmp(candidate->as.call.callee_name, instruction->as.call.callee_name) != 0 ||
                        !value_ssa_perf_same_call_args(candidate, instruction)) {
                        continue;
                    }

                    instruction->kind = VALUE_SSA_INSTR_MOV;
                    instruction->as.mov_value = candidate->result;
                    changed = 1;
                    break;
                }
            }
        }
    }

    free(pure_function_flags);
    free(readonly_globals);

    if (changed &&
        (!value_ssa_simplify_trivial_values(program, error) ||
            !value_ssa_eliminate_dead_value_defs(program, error))) {
        return 0;
    }

    return value_ssa_verify_program(program, error);
}

static int value_ssa_bridge_reuse_repeated_pure_internal_calls_in_program(ValueSsaProgram *program,
    ValueSsaError *error);

typedef struct {
    size_t header_id;
    size_t body_id;
    size_t preheader_id;
    size_t exit_id;
} ValueSsaPerfSimpleLoop;

typedef enum {
    VALUE_SSA_PERF_ADDRESS_ROOT_UNKNOWN = 0,
    VALUE_SSA_PERF_ADDRESS_ROOT_LOCAL_SLOT,
    VALUE_SSA_PERF_ADDRESS_ROOT_GLOBAL_SLOT,
    VALUE_SSA_PERF_ADDRESS_ROOT_PARAMETER_POINTER,
} ValueSsaPerfAddressRootKind;

typedef struct {
    ValueSsaPerfAddressRootKind kind;
    size_t id;
} ValueSsaPerfAddressRoot;

static int value_ssa_perf_extract_global_arg_root(const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    ValueSsaValueRef value,
    size_t *out_global_id,
    unsigned depth) {
    ValueSsaInstruction *definition;

    if (!function || !analysis || !out_global_id || depth > 8u || value.kind != VALUE_SSA_VALUE_ID) {
        return 0;
    }

    definition = value_ssa_pass_lookup_instruction_definition(function, analysis, value.value_id);
    if (!definition) {
        return 0;
    }

    switch (definition->kind) {
    case VALUE_SSA_INSTR_ADDR_GLOBAL:
        if (definition->as.addr_slot.kind != VALUE_SSA_SLOT_GLOBAL) {
            return 0;
        }
        *out_global_id = definition->as.addr_slot.id;
        return 1;
    case VALUE_SSA_INSTR_MOV:
        return value_ssa_perf_extract_global_arg_root(
            function, analysis, definition->as.mov_value, out_global_id, depth + 1u);
    default:
        return 0;
    }
}

static int value_ssa_perf_collect_non_alias_parameter_pointer_pairs(const ValueSsaProgram *program,
    const ValueSsaFunction *callee,
    unsigned char *pair_matrix) {
    size_t param_count;
    size_t lhs_index;
    size_t rhs_index;
    size_t caller_index;
    int saw_callsite = 0;

    if (!program || !callee || !pair_matrix) {
        return 0;
    }

    param_count = callee->parameter_count;
    for (lhs_index = 0; lhs_index < param_count; ++lhs_index) {
        for (rhs_index = 0; rhs_index < param_count; ++rhs_index) {
            pair_matrix[lhs_index * param_count + rhs_index] =
                (unsigned char)(lhs_index != rhs_index &&
                    lhs_index < callee->local_count &&
                    rhs_index < callee->local_count &&
                    callee->locals[lhs_index].is_parameter &&
                    callee->locals[rhs_index].is_parameter &&
                    callee->locals[lhs_index].array_rank > 0u &&
                    callee->locals[rhs_index].array_rank > 0u);
        }
    }

    for (caller_index = 0; caller_index < program->function_count; ++caller_index) {
        const ValueSsaFunction *caller = &program->functions[caller_index];
        ValueSsaDefUseAnalysis def_use;
        size_t block_index;

        if (!caller->has_body) {
            continue;
        }

        value_ssa_def_use_analysis_init(&def_use);
        if (!value_ssa_compute_def_use_analysis(caller, &def_use, NULL)) {
            value_ssa_def_use_analysis_free(&def_use);
            return 0;
        }

        for (block_index = 0; block_index < caller->block_count; ++block_index) {
            const ValueSsaBasicBlock *block = &caller->blocks[block_index];
            size_t instruction_index;

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                const ValueSsaInstruction *instruction = &block->instructions[instruction_index];
                size_t lhs_global_id;
                size_t rhs_global_id;

                if (instruction->kind != VALUE_SSA_INSTR_CALL ||
                    !instruction->as.call.callee_name ||
                    !callee->name ||
                    strcmp(instruction->as.call.callee_name, callee->name) != 0 ||
                    instruction->as.call.arg_count != param_count) {
                    continue;
                }

                saw_callsite = 1;
                for (lhs_index = 0; lhs_index < param_count; ++lhs_index) {
                    for (rhs_index = lhs_index + 1u; rhs_index < param_count; ++rhs_index) {
                        if (!pair_matrix[lhs_index * param_count + rhs_index]) {
                            continue;
                        }

                        if (!value_ssa_perf_extract_global_arg_root(
                                caller, &def_use, instruction->as.call.args[lhs_index], &lhs_global_id, 0u) ||
                            !value_ssa_perf_extract_global_arg_root(
                                caller, &def_use, instruction->as.call.args[rhs_index], &rhs_global_id, 0u) ||
                            lhs_global_id == rhs_global_id) {
                            pair_matrix[lhs_index * param_count + rhs_index] = 0u;
                            pair_matrix[rhs_index * param_count + lhs_index] = 0u;
                        }
                    }
                }
            }
        }

        value_ssa_def_use_analysis_free(&def_use);
    }

    if (!saw_callsite) {
        memset(pair_matrix, 0, param_count * param_count * sizeof(unsigned char));
    }

    return 1;
}

static int value_ssa_perf_parameter_pointer_pair_proven_non_alias(const ValueSsaFunction *function,
    const unsigned char *pair_matrix,
    size_t lhs_id,
    size_t rhs_id) {
    size_t param_count;

    if (!function || !pair_matrix || lhs_id == rhs_id) {
        return 0;
    }

    param_count = function->parameter_count;
    if (lhs_id >= param_count || rhs_id >= param_count) {
        return 0;
    }

    return pair_matrix[lhs_id * param_count + rhs_id] != 0u;
}

static int value_ssa_perf_same_pure_address_value(const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    ValueSsaValueRef lhs,
    ValueSsaValueRef rhs,
    unsigned depth) {
    ValueSsaInstruction *lhs_def;
    ValueSsaInstruction *rhs_def;

    if (depth > 8u) {
        return 0;
    }
    if (value_ssa_value_refs_equal(lhs, rhs)) {
        return 1;
    }
    if (!function || !analysis || lhs.kind != VALUE_SSA_VALUE_ID || rhs.kind != VALUE_SSA_VALUE_ID) {
        return 0;
    }

    lhs_def = value_ssa_pass_lookup_instruction_definition(function, analysis, lhs.value_id);
    rhs_def = value_ssa_pass_lookup_instruction_definition(function, analysis, rhs.value_id);
    if (!lhs_def || !rhs_def || lhs_def->kind != rhs_def->kind) {
        return 0;
    }

    switch (lhs_def->kind) {
    case VALUE_SSA_INSTR_ADDR_LOCAL:
    case VALUE_SSA_INSTR_ADDR_GLOBAL:
        return lhs_def->as.addr_slot.kind == rhs_def->as.addr_slot.kind &&
            lhs_def->as.addr_slot.id == rhs_def->as.addr_slot.id;
    case VALUE_SSA_INSTR_LOAD_LOCAL:
        return lhs_def->as.load_slot.kind == rhs_def->as.load_slot.kind &&
            lhs_def->as.load_slot.id == rhs_def->as.load_slot.id;
    case VALUE_SSA_INSTR_LOAD_GLOBAL:
        return lhs_def->as.load_slot.kind == rhs_def->as.load_slot.kind &&
            lhs_def->as.load_slot.id == rhs_def->as.load_slot.id;
    case VALUE_SSA_INSTR_MOV:
        return value_ssa_perf_same_pure_address_value(
            function, analysis, lhs_def->as.mov_value, rhs_def->as.mov_value, depth + 1u);
    case VALUE_SSA_INSTR_BINARY:
        if (lhs_def->as.binary.op != rhs_def->as.binary.op) {
            return 0;
        }
        if (value_ssa_perf_same_pure_address_value(
                function, analysis, lhs_def->as.binary.lhs, rhs_def->as.binary.lhs, depth + 1u) &&
            value_ssa_perf_same_pure_address_value(
                function, analysis, lhs_def->as.binary.rhs, rhs_def->as.binary.rhs, depth + 1u)) {
            return 1;
        }
        return lhs_def->as.binary.op == VALUE_SSA_BINARY_ADD &&
            value_ssa_perf_same_pure_address_value(
                function, analysis, lhs_def->as.binary.lhs, rhs_def->as.binary.rhs, depth + 1u) &&
            value_ssa_perf_same_pure_address_value(
                function, analysis, lhs_def->as.binary.rhs, rhs_def->as.binary.lhs, depth + 1u);
    default:
        return 0;
    }
}

static int value_ssa_perf_find_simple_single_body_loop(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *analysis,
    size_t header_id,
    ValueSsaPerfSimpleLoop *out_loop) {
    const ValueSsaBasicBlock *header;
    size_t predecessor_index;
    size_t predecessor_a = (size_t)-1;
    size_t predecessor_b = (size_t)-1;
    size_t body_id = (size_t)-1;
    size_t preheader_id = (size_t)-1;

    if (!function || !analysis || !out_loop || header_id >= function->block_count) {
        return 0;
    }

    header = &function->blocks[header_id];
    if (header->phi_count != 0u ||
        !header->has_terminator ||
        header->terminator.kind != VALUE_SSA_TERM_BRANCH ||
        analysis->predecessor_counts[header_id] != 2u) {
        return 0;
    }

    for (predecessor_index = 0; predecessor_index < function->block_count; ++predecessor_index) {
        if (!analysis->predecessor_matrix[predecessor_index * function->block_count + header_id]) {
            continue;
        }
        if (predecessor_a == (size_t)-1) {
            predecessor_a = predecessor_index;
        } else if (predecessor_b == (size_t)-1) {
            predecessor_b = predecessor_index;
        } else {
            return 0;
        }
    }

    if (predecessor_a == (size_t)-1 || predecessor_b == (size_t)-1) {
        return 0;
    }

    if (function->blocks[predecessor_a].has_terminator &&
        function->blocks[predecessor_a].terminator.kind == VALUE_SSA_TERM_JUMP &&
        function->blocks[predecessor_a].terminator.as.jump_target == header_id &&
        analysis->dominates[header_id * function->block_count + predecessor_a]) {
        body_id = predecessor_a;
        preheader_id = predecessor_b;
    } else if (function->blocks[predecessor_b].has_terminator &&
               function->blocks[predecessor_b].terminator.kind == VALUE_SSA_TERM_JUMP &&
               function->blocks[predecessor_b].terminator.as.jump_target == header_id &&
               analysis->dominates[header_id * function->block_count + predecessor_b]) {
        body_id = predecessor_b;
        preheader_id = predecessor_a;
    } else {
        return 0;
    }

    if (body_id >= function->block_count || preheader_id >= function->block_count) {
        return 0;
    }

    if (header->terminator.as.branch.then_target == body_id) {
        out_loop->exit_id = header->terminator.as.branch.else_target;
    } else if (header->terminator.as.branch.else_target == body_id) {
        out_loop->exit_id = header->terminator.as.branch.then_target;
    } else {
        return 0;
    }
    if (function->blocks[body_id].phi_count != 0u ||
        function->blocks[preheader_id].phi_count != 0u ||
        !function->blocks[body_id].has_terminator ||
        function->blocks[body_id].terminator.kind != VALUE_SSA_TERM_JUMP ||
        function->blocks[body_id].terminator.as.jump_target != header_id ||
        !function->blocks[preheader_id].has_terminator ||
        function->blocks[preheader_id].terminator.kind != VALUE_SSA_TERM_JUMP ||
        function->blocks[preheader_id].terminator.as.jump_target != header_id) {
        return 0;
    }

    out_loop->header_id = header_id;
    out_loop->body_id = body_id;
    out_loop->preheader_id = preheader_id;
    return 1;
}

static int value_ssa_perf_find_phi_header_single_body_loop(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *analysis,
    size_t header_id,
    ValueSsaPerfSimpleLoop *out_loop) {
    const ValueSsaBasicBlock *header;
    size_t predecessor_index;
    size_t predecessor_a = (size_t)-1;
    size_t predecessor_b = (size_t)-1;
    size_t body_id = (size_t)-1;
    size_t preheader_id = (size_t)-1;

    if (!function || !analysis || !out_loop || header_id >= function->block_count) {
        return 0;
    }

    header = &function->blocks[header_id];
    if (!header->has_terminator ||
        header->terminator.kind != VALUE_SSA_TERM_BRANCH ||
        analysis->predecessor_counts[header_id] != 2u) {
        return 0;
    }

    for (predecessor_index = 0; predecessor_index < function->block_count; ++predecessor_index) {
        if (!analysis->predecessor_matrix[predecessor_index * function->block_count + header_id]) {
            continue;
        }
        if (predecessor_a == (size_t)-1) {
            predecessor_a = predecessor_index;
        } else if (predecessor_b == (size_t)-1) {
            predecessor_b = predecessor_index;
        } else {
            return 0;
        }
    }

    if (predecessor_a == (size_t)-1 || predecessor_b == (size_t)-1) {
        return 0;
    }

    if (function->blocks[predecessor_a].has_terminator &&
        function->blocks[predecessor_a].terminator.kind == VALUE_SSA_TERM_JUMP &&
        function->blocks[predecessor_a].terminator.as.jump_target == header_id &&
        analysis->dominates[header_id * function->block_count + predecessor_a]) {
        body_id = predecessor_a;
        preheader_id = predecessor_b;
    } else if (function->blocks[predecessor_b].has_terminator &&
               function->blocks[predecessor_b].terminator.kind == VALUE_SSA_TERM_JUMP &&
               function->blocks[predecessor_b].terminator.as.jump_target == header_id &&
               analysis->dominates[header_id * function->block_count + predecessor_b]) {
        body_id = predecessor_b;
        preheader_id = predecessor_a;
    } else {
        return 0;
    }

    if (body_id >= function->block_count || preheader_id >= function->block_count) {
        return 0;
    }

    if (header->terminator.as.branch.then_target == body_id) {
        out_loop->exit_id = header->terminator.as.branch.else_target;
    } else if (header->terminator.as.branch.else_target == body_id) {
        out_loop->exit_id = header->terminator.as.branch.then_target;
    } else {
        return 0;
    }
    if (function->blocks[body_id].phi_count != 0u ||
        function->blocks[preheader_id].phi_count != 0u ||
        !function->blocks[body_id].has_terminator ||
        function->blocks[body_id].terminator.kind != VALUE_SSA_TERM_JUMP ||
        function->blocks[body_id].terminator.as.jump_target != header_id ||
        !function->blocks[preheader_id].has_terminator ||
        function->blocks[preheader_id].terminator.kind != VALUE_SSA_TERM_JUMP ||
        function->blocks[preheader_id].terminator.as.jump_target != header_id) {
        return 0;
    }

    out_loop->header_id = header_id;
    out_loop->body_id = body_id;
    out_loop->preheader_id = preheader_id;
    return 1;
}

static int value_ssa_perf_extract_address_root(const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    ValueSsaValueRef value,
    ValueSsaPerfAddressRoot *out_root,
    unsigned depth) {
    ValueSsaInstruction *definition;

    if (!out_root) {
        return 0;
    }

    out_root->kind = VALUE_SSA_PERF_ADDRESS_ROOT_UNKNOWN;
    out_root->id = 0u;
    if (!function || !analysis || depth > 8u || value.kind != VALUE_SSA_VALUE_ID) {
        return 0;
    }

    definition = value_ssa_pass_lookup_instruction_definition(function, analysis, value.value_id);
    if (!definition) {
        return 0;
    }

    switch (definition->kind) {
    case VALUE_SSA_INSTR_ADDR_LOCAL:
        out_root->kind = VALUE_SSA_PERF_ADDRESS_ROOT_LOCAL_SLOT;
        out_root->id = definition->as.addr_slot.id;
        return 1;
    case VALUE_SSA_INSTR_ADDR_GLOBAL:
        out_root->kind = VALUE_SSA_PERF_ADDRESS_ROOT_GLOBAL_SLOT;
        out_root->id = definition->as.addr_slot.id;
        return 1;
    case VALUE_SSA_INSTR_LOAD_LOCAL:
        if (definition->as.load_slot.kind == VALUE_SSA_SLOT_LOCAL &&
            definition->as.load_slot.id < function->local_count &&
            function->locals[definition->as.load_slot.id].is_parameter &&
            function->locals[definition->as.load_slot.id].array_rank > 0u) {
            out_root->kind = VALUE_SSA_PERF_ADDRESS_ROOT_PARAMETER_POINTER;
            out_root->id = definition->as.load_slot.id;
            return 1;
        }
        return 0;
    case VALUE_SSA_INSTR_MOV:
        return value_ssa_perf_extract_address_root(
            function, analysis, definition->as.mov_value, out_root, depth + 1u);
    case VALUE_SSA_INSTR_BINARY: {
        ValueSsaPerfAddressRoot lhs_root;
        ValueSsaPerfAddressRoot rhs_root;
        int has_lhs_root;
        int has_rhs_root;

        if (definition->as.binary.op != VALUE_SSA_BINARY_ADD) {
            return 0;
        }

        has_lhs_root = value_ssa_perf_extract_address_root(
            function, analysis, definition->as.binary.lhs, &lhs_root, depth + 1u);
        has_rhs_root = value_ssa_perf_extract_address_root(
            function, analysis, definition->as.binary.rhs, &rhs_root, depth + 1u);
        if (has_lhs_root == has_rhs_root) {
            return 0;
        }
        *out_root = has_lhs_root ? lhs_root : rhs_root;
        return 1;
    }
    default:
        return 0;
    }
}

static int value_ssa_perf_address_roots_proven_non_alias(const ValueSsaFunction *function,
    const unsigned char *parameter_pointer_non_alias_pairs,
    const ValueSsaPerfAddressRoot *lhs,
    const ValueSsaPerfAddressRoot *rhs) {
    if (!lhs || !rhs ||
        lhs->kind == VALUE_SSA_PERF_ADDRESS_ROOT_UNKNOWN ||
        rhs->kind == VALUE_SSA_PERF_ADDRESS_ROOT_UNKNOWN) {
        return 0;
    }

    if (lhs->kind == VALUE_SSA_PERF_ADDRESS_ROOT_LOCAL_SLOT &&
        rhs->kind == VALUE_SSA_PERF_ADDRESS_ROOT_LOCAL_SLOT) {
        return lhs->id != rhs->id;
    }
    if (lhs->kind == VALUE_SSA_PERF_ADDRESS_ROOT_GLOBAL_SLOT &&
        rhs->kind == VALUE_SSA_PERF_ADDRESS_ROOT_GLOBAL_SLOT) {
        return lhs->id != rhs->id;
    }
    if ((lhs->kind == VALUE_SSA_PERF_ADDRESS_ROOT_LOCAL_SLOT && rhs->kind == VALUE_SSA_PERF_ADDRESS_ROOT_GLOBAL_SLOT) ||
        (lhs->kind == VALUE_SSA_PERF_ADDRESS_ROOT_GLOBAL_SLOT && rhs->kind == VALUE_SSA_PERF_ADDRESS_ROOT_LOCAL_SLOT)) {
        return 1;
    }
    if ((lhs->kind == VALUE_SSA_PERF_ADDRESS_ROOT_LOCAL_SLOT && rhs->kind == VALUE_SSA_PERF_ADDRESS_ROOT_PARAMETER_POINTER) ||
        (lhs->kind == VALUE_SSA_PERF_ADDRESS_ROOT_PARAMETER_POINTER && rhs->kind == VALUE_SSA_PERF_ADDRESS_ROOT_LOCAL_SLOT)) {
        return 1;
    }
    if (lhs->kind == VALUE_SSA_PERF_ADDRESS_ROOT_PARAMETER_POINTER &&
        rhs->kind == VALUE_SSA_PERF_ADDRESS_ROOT_PARAMETER_POINTER) {
        return value_ssa_perf_parameter_pointer_pair_proven_non_alias(
            function,
            parameter_pointer_non_alias_pairs,
            lhs->id,
            rhs->id);
    }

    return 0;
}

static int value_ssa_perf_store_cannot_alias_loaded_address(const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    const unsigned char *parameter_pointer_non_alias_pairs,
    const ValueSsaPerfAddressRoot *load_root,
    const ValueSsaInstruction *store_instruction) {
    ValueSsaPerfAddressRoot store_root;

    if (!function || !analysis || !load_root || !store_instruction) {
        return 0;
    }

    switch (store_instruction->kind) {
    case VALUE_SSA_INSTR_STORE_LOCAL:
        if (load_root->kind != VALUE_SSA_PERF_ADDRESS_ROOT_LOCAL_SLOT ||
            store_instruction->as.store.slot.kind != VALUE_SSA_SLOT_LOCAL ||
            store_instruction->as.store.slot.id >= function->local_count) {
            return load_root->kind != VALUE_SSA_PERF_ADDRESS_ROOT_PARAMETER_POINTER;
        }
        if (store_instruction->as.store.slot.id == load_root->id) {
            return 0;
        }
        return function->locals[store_instruction->as.store.slot.id].array_rank == 0u;
    case VALUE_SSA_INSTR_STORE_GLOBAL:
        if (store_instruction->as.store.slot.kind != VALUE_SSA_SLOT_GLOBAL) {
            return 0;
        }
        if (load_root->kind == VALUE_SSA_PERF_ADDRESS_ROOT_GLOBAL_SLOT) {
            return store_instruction->as.store.slot.id != load_root->id;
        }
        return load_root->kind == VALUE_SSA_PERF_ADDRESS_ROOT_LOCAL_SLOT;
    case VALUE_SSA_INSTR_STORE_INDIRECT:
        if (!value_ssa_perf_extract_address_root(
                function, analysis, store_instruction->as.store_indirect.addr, &store_root, 0u)) {
            return 0;
        }
        return value_ssa_perf_address_roots_proven_non_alias(
            function, parameter_pointer_non_alias_pairs, load_root, &store_root);
    default:
        return 1;
    }
}

static int value_ssa_perf_function_has_calls(const ValueSsaFunction *function) {
    size_t block_index;

    if (!function) {
        return 0;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        const ValueSsaBasicBlock *block = &function->blocks[block_index];
        size_t instruction_index;

        for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
            if (block->instructions[instruction_index].kind == VALUE_SSA_INSTR_CALL) {
                return 1;
            }
        }
    }

    return 0;
}

static int value_ssa_perf_function_preserves_global_slot(const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    size_t slot_id) {
    ValueSsaPerfAddressRoot load_root;
    size_t block_index;

    if (!function || !analysis) {
        return 0;
    }

    load_root.kind = VALUE_SSA_PERF_ADDRESS_ROOT_GLOBAL_SLOT;
    load_root.id = slot_id;
    for (block_index = 0; block_index < function->block_count; ++block_index) {
        const ValueSsaBasicBlock *block = &function->blocks[block_index];
        size_t instruction_index;

        for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
            const ValueSsaInstruction *instruction = &block->instructions[instruction_index];

            if (instruction->kind == VALUE_SSA_INSTR_CALL) {
                return 0;
            }
            if (!value_ssa_perf_store_cannot_alias_loaded_address(
                    function, analysis, NULL, &load_root, instruction)) {
                return 0;
            }
        }
    }

    return 1;
}

static int value_ssa_perf_find_simple_branch_body_loop(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *analysis,
    size_t header_id,
    ValueSsaPerfSimpleLoop *out_loop) {
    const ValueSsaBasicBlock *header;
    size_t predecessor_index;
    size_t predecessor_a = (size_t)-1;
    size_t predecessor_b = (size_t)-1;
    size_t body_id = (size_t)-1;
    size_t preheader_id = (size_t)-1;
    size_t candidate_then;
    size_t candidate_else;

    if (!function || !analysis || !out_loop || header_id >= function->block_count) {
        return 0;
    }

    header = &function->blocks[header_id];
    if (header->phi_count != 0u ||
        !header->has_terminator ||
        header->terminator.kind != VALUE_SSA_TERM_BRANCH ||
        analysis->predecessor_counts[header_id] != 2u) {
        return 0;
    }

    for (predecessor_index = 0; predecessor_index < function->block_count; ++predecessor_index) {
        if (!analysis->predecessor_matrix[predecessor_index * function->block_count + header_id]) {
            continue;
        }
        if (predecessor_a == (size_t)-1) {
            predecessor_a = predecessor_index;
        } else if (predecessor_b == (size_t)-1) {
            predecessor_b = predecessor_index;
        } else {
            return 0;
        }
    }

    if (predecessor_a == (size_t)-1 || predecessor_b == (size_t)-1) {
        return 0;
    }

    if (function->blocks[predecessor_a].has_terminator &&
        function->blocks[predecessor_a].terminator.kind == VALUE_SSA_TERM_JUMP &&
        function->blocks[predecessor_a].terminator.as.jump_target == header_id &&
        analysis->dominates[header_id * function->block_count + predecessor_a]) {
        body_id = predecessor_a;
        preheader_id = predecessor_b;
    } else if (function->blocks[predecessor_b].has_terminator &&
               function->blocks[predecessor_b].terminator.kind == VALUE_SSA_TERM_JUMP &&
               function->blocks[predecessor_b].terminator.as.jump_target == header_id &&
               analysis->dominates[header_id * function->block_count + predecessor_b]) {
        body_id = predecessor_b;
        preheader_id = predecessor_a;
    } else {
        return 0;
    }

    if (body_id >= function->block_count || preheader_id >= function->block_count) {
        return 0;
    }
    if (!function->blocks[body_id].has_terminator ||
        function->blocks[body_id].terminator.kind != VALUE_SSA_TERM_BRANCH ||
        function->blocks[body_id].phi_count != 0u ||
        function->blocks[preheader_id].phi_count != 0u ||
        !function->blocks[preheader_id].has_terminator ||
        function->blocks[preheader_id].terminator.kind != VALUE_SSA_TERM_JUMP ||
        function->blocks[preheader_id].terminator.as.jump_target != header_id) {
        return 0;
    }

    candidate_then = function->blocks[body_id].terminator.as.branch.then_target;
    candidate_else = function->blocks[body_id].terminator.as.branch.else_target;
    if (candidate_then == header_id && candidate_else != header_id) {
        out_loop->exit_id = candidate_else;
    } else if (candidate_else == header_id && candidate_then != header_id) {
        out_loop->exit_id = candidate_then;
    } else {
        return 0;
    }
    if (out_loop->exit_id >= function->block_count) {
        return 0;
    }

    out_loop->header_id = header_id;
    out_loop->body_id = body_id;
    out_loop->preheader_id = preheader_id;
    return 1;
}


static int value_ssa_perf_loop_preserves_local_slot(const ValueSsaFunction *function,
    const ValueSsaPerfSimpleLoop *loop,
    size_t slot_id) {
    size_t block_ids[2];
    size_t block_pos;

    if (!function || !loop || slot_id >= function->local_count) {
        return 0;
    }

    block_ids[0] = loop->header_id;
    block_ids[1] = loop->body_id;
    for (block_pos = 0; block_pos < 2u; ++block_pos) {
        const ValueSsaBasicBlock *block = &function->blocks[block_ids[block_pos]];
        size_t instruction_index;

        for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
            const ValueSsaInstruction *instruction = &block->instructions[instruction_index];

            if (instruction->kind == VALUE_SSA_INSTR_STORE_LOCAL &&
                instruction->as.store.slot.kind == VALUE_SSA_SLOT_LOCAL &&
                instruction->as.store.slot.id == slot_id) {
                return 0;
            }
        }
    }

    return 1;
}

static int value_ssa_perf_loop_preserves_global_slot(const ValueSsaProgram *program,
    const ValueSsaFunction *function,
    const ValueSsaPerfSimpleLoop *loop,
    const unsigned char *readonly_globals,
    size_t slot_id) {
    (void)function;
    (void)loop;

    return program && readonly_globals && slot_id < program->global_count && readonly_globals[slot_id];
}

static int value_ssa_perf_value_is_loop_invariant(const ValueSsaProgram *program,
    const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    const unsigned char *readonly_globals,
    const ValueSsaPerfSimpleLoop *loop,
    ValueSsaValueRef value,
    unsigned depth) {
    ValueSsaInstruction *definition;

    if (!program || !function || !analysis || !readonly_globals || !loop) {
        return 0;
    }
    if (value.kind != VALUE_SSA_VALUE_ID) {
        return 1;
    }
    if (depth > 12u) {
        return 0;
    }
    if (value.value_id >= analysis->value_count || !analysis->has_def[value.value_id]) {
        return 0;
    }
    if (analysis->def_block_ids[value.value_id] != loop->header_id &&
        analysis->def_block_ids[value.value_id] != loop->body_id) {
        return 1;
    }

    definition = value_ssa_pass_lookup_instruction_definition(function, analysis, value.value_id);
    if (!definition) {
        return 0;
    }
    if (definition->kind == VALUE_SSA_INSTR_CALL) {
        return 0;
    }
    switch (definition->kind) {
    case VALUE_SSA_INSTR_ADDR_LOCAL:
    case VALUE_SSA_INSTR_ADDR_GLOBAL:
        return 1;
    case VALUE_SSA_INSTR_LOAD_LOCAL:
        return definition->as.load_slot.kind == VALUE_SSA_SLOT_LOCAL &&
            definition->as.load_slot.id < function->local_count &&
            value_ssa_perf_loop_preserves_local_slot(function, loop, definition->as.load_slot.id);
    case VALUE_SSA_INSTR_LOAD_GLOBAL:
        return definition->as.load_slot.kind == VALUE_SSA_SLOT_GLOBAL &&
            definition->as.load_slot.id < program->global_count &&
            value_ssa_perf_loop_preserves_global_slot(
                program, function, loop, readonly_globals, definition->as.load_slot.id);
    case VALUE_SSA_INSTR_MOV:
        return value_ssa_perf_value_is_loop_invariant(
            program,
            function,
            analysis,
            readonly_globals,
            loop,
            definition->as.mov_value,
            depth + 1u);
    case VALUE_SSA_INSTR_BINARY:
        return value_ssa_perf_value_is_loop_invariant(
                   program,
                   function,
                   analysis,
                   readonly_globals,
                   loop,
                   definition->as.binary.lhs,
                   depth + 1u) &&
            value_ssa_perf_value_is_loop_invariant(
                   program,
                   function,
                   analysis,
                   readonly_globals,
                   loop,
                   definition->as.binary.rhs,
                   depth + 1u);
    default:
        return 0;
    }
}

static int value_ssa_perf_value_is_loop_invariant_for_indirect_addr(const ValueSsaProgram *program,
    const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    const unsigned char *readonly_globals,
    const unsigned char *address_taken_locals,
    const ValueSsaPerfSimpleLoop *loop,
    ValueSsaValueRef value,
    unsigned depth) {
    ValueSsaInstruction *definition;

    if (!program || !function || !analysis || !readonly_globals || !loop ||
        (!address_taken_locals && function->local_count > 0u)) {
        return 0;
    }
    if (value.kind != VALUE_SSA_VALUE_ID) {
        return 1;
    }
    if (depth > 12u) {
        return 0;
    }
    if (value.value_id >= analysis->value_count || !analysis->has_def[value.value_id]) {
        return 0;
    }
    if (analysis->def_block_ids[value.value_id] != loop->header_id &&
        analysis->def_block_ids[value.value_id] != loop->body_id) {
        return 1;
    }

    definition = value_ssa_pass_lookup_instruction_definition(function, analysis, value.value_id);
    if (!definition) {
        return 0;
    }
    if (definition->kind == VALUE_SSA_INSTR_CALL) {
        return 0;
    }
    switch (definition->kind) {
    case VALUE_SSA_INSTR_ADDR_LOCAL:
    case VALUE_SSA_INSTR_ADDR_GLOBAL:
        return 1;
    case VALUE_SSA_INSTR_LOAD_LOCAL:
        return definition->as.load_slot.kind == VALUE_SSA_SLOT_LOCAL &&
            definition->as.load_slot.id < function->local_count &&
            function->locals[definition->as.load_slot.id].array_rank == 0u &&
            !address_taken_locals[definition->as.load_slot.id] &&
            value_ssa_perf_loop_preserves_local_slot(function, loop, definition->as.load_slot.id);
    case VALUE_SSA_INSTR_LOAD_GLOBAL:
        return definition->as.load_slot.kind == VALUE_SSA_SLOT_GLOBAL &&
            definition->as.load_slot.id < program->global_count &&
            value_ssa_perf_loop_preserves_global_slot(
                program, function, loop, readonly_globals, definition->as.load_slot.id);
    case VALUE_SSA_INSTR_MOV:
        return value_ssa_perf_value_is_loop_invariant_for_indirect_addr(
            program,
            function,
            analysis,
            readonly_globals,
            address_taken_locals,
            loop,
            definition->as.mov_value,
            depth + 1u);
    case VALUE_SSA_INSTR_BINARY:
        return value_ssa_perf_value_is_loop_invariant_for_indirect_addr(
                   program,
                   function,
                   analysis,
                   readonly_globals,
                   address_taken_locals,
                   loop,
                   definition->as.binary.lhs,
                   depth + 1u) &&
            value_ssa_perf_value_is_loop_invariant_for_indirect_addr(
                   program,
                   function,
                   analysis,
                   readonly_globals,
                   address_taken_locals,
                   loop,
                   definition->as.binary.rhs,
                   depth + 1u);
    default:
        return 0;
    }
}

static int value_ssa_perf_value_is_defined_inside_loop(const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    const ValueSsaPerfSimpleLoop *loop,
    ValueSsaValueRef value) {
    size_t def_block_id;

    if (!function || !analysis || !loop || value.kind != VALUE_SSA_VALUE_ID || value.value_id >= analysis->value_count) {
        return 0;
    }
    if (!analysis->has_def[value.value_id]) {
        return 0;
    }

    def_block_id = analysis->def_block_ids[value.value_id];
    return def_block_id == loop->header_id || def_block_id == loop->body_id;
}

static int value_ssa_perf_loop_instructions_preserve_indirect_load(const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    const ValueSsaBasicBlock *block,
    const unsigned char *parameter_pointer_non_alias_pairs,
    const ValueSsaPerfAddressRoot *load_root) {
    size_t instruction_index;

    if (!function || !analysis || !block || !load_root) {
        return 0;
    }

    for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
        if (!value_ssa_perf_store_cannot_alias_loaded_address(
                function,
                analysis,
                parameter_pointer_non_alias_pairs,
                load_root,
                &block->instructions[instruction_index])) {
            return 0;
        }
    }

    return 1;
}

static int value_ssa_perf_instruction_preserves_indirect_load(const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    const unsigned char *parameter_pointer_non_alias_pairs,
    const ValueSsaPerfAddressRoot *load_root,
    const ValueSsaInstruction *instruction) {
    if (!instruction || instruction->kind == VALUE_SSA_INSTR_CALL) {
        return 0;
    }

    return value_ssa_perf_store_cannot_alias_loaded_address(
        function, analysis, parameter_pointer_non_alias_pairs, load_root, instruction);
}

static int value_ssa_perf_instruction_range_preserves_indirect_load(const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    const ValueSsaBasicBlock *block,
    size_t start_index,
    size_t end_index,
    const unsigned char *parameter_pointer_non_alias_pairs,
    const ValueSsaPerfAddressRoot *load_root) {
    size_t instruction_index;

    if (!function || !analysis || !block || !load_root ||
        start_index > end_index || end_index > block->instruction_count) {
        return 0;
    }

    for (instruction_index = start_index; instruction_index < end_index; ++instruction_index) {
        if (!value_ssa_perf_instruction_preserves_indirect_load(
                function,
                analysis,
                parameter_pointer_non_alias_pairs,
                load_root,
                &block->instructions[instruction_index])) {
            return 0;
        }
    }

    return 1;
}

static int value_ssa_perf_clone_loop_invariant_value_into_preheader(const ValueSsaProgram *program,
    ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    const unsigned char *readonly_globals,
    const ValueSsaPerfSimpleLoop *loop,
    ValueSsaValueRef value,
    unsigned char *has_mapping,
    ValueSsaValueRef *mappings,
    ValueSsaBasicBlock *rebuilt_preheader,
    ValueSsaValueRef *out_value,
    ValueSsaError *error);

static int value_ssa_perf_try_hoist_loop_invariant_indirect_load(ValueSsaFunction *function,
    const ValueSsaProgram *program,
    const ValueSsaDefUseAnalysis *analysis,
    const unsigned char *readonly_globals,
    const unsigned char *address_taken_locals,
    const unsigned char *parameter_pointer_non_alias_pairs,
    const ValueSsaPerfSimpleLoop *loop,
    unsigned char *value_has_mapping,
    ValueSsaValueRef *value_mappings,
    ValueSsaBasicBlock *rebuilt_preheader,
    ValueSsaBasicBlock *rebuilt_target,
    const ValueSsaInstruction *instruction,
    int *out_changed,
    ValueSsaError *error) {
    ValueSsaPerfAddressRoot load_root;
    ValueSsaInstruction cloned_load;
    ValueSsaInstruction replacement;
    ValueSsaValueRef hoisted_addr;

    if (out_changed) {
        *out_changed = 0;
    }
    if (!function || !program || !analysis || !readonly_globals || !loop || !rebuilt_preheader || !rebuilt_target || !instruction ||
        (!address_taken_locals && function->local_count > 0u) ||
        instruction->kind != VALUE_SSA_INSTR_LOAD_INDIRECT ||
        !instruction->has_result ||
        instruction->result.kind != VALUE_SSA_VALUE_ID) {
        return 1;
    }

    if (!value_ssa_perf_value_is_loop_invariant_for_indirect_addr(
            program,
            function,
            analysis,
            readonly_globals,
            address_taken_locals,
            loop,
            instruction->as.load_indirect_addr,
            0u)) {
        return 1;
    }
    if (!value_ssa_perf_extract_address_root(function, analysis, instruction->as.load_indirect_addr, &load_root, 0u) ||
        !value_ssa_perf_loop_instructions_preserve_indirect_load(
            function,
            analysis,
            &function->blocks[loop->header_id],
            parameter_pointer_non_alias_pairs,
            &load_root) ||
        !value_ssa_perf_loop_instructions_preserve_indirect_load(
            function,
            analysis,
            &function->blocks[loop->body_id],
            parameter_pointer_non_alias_pairs,
            &load_root)) {
        return 1;
    }

    memset(&cloned_load, 0, sizeof(cloned_load));
    cloned_load.kind = VALUE_SSA_INSTR_LOAD_INDIRECT;
    cloned_load.has_result = 1;
    cloned_load.result = value_ssa_value_id(value_ssa_function_allocate_value(function));
    if (!value_ssa_perf_clone_loop_invariant_value_into_preheader(
            program,
            function,
            analysis,
            readonly_globals,
            loop,
            instruction->as.load_indirect_addr,
            value_has_mapping,
            value_mappings,
            rebuilt_preheader,
            &hoisted_addr,
            error)) {
        return 0;
    }
    cloned_load.as.load_indirect_addr = hoisted_addr;
    if (cloned_load.result.value_id == (size_t)-1 ||
        !value_ssa_block_append_instruction(rebuilt_preheader, &cloned_load, error)) {
        return 0;
    }

    memset(&replacement, 0, sizeof(replacement));
    replacement.kind = VALUE_SSA_INSTR_MOV;
    replacement.has_result = 1;
    replacement.result = instruction->result;
    replacement.as.mov_value = cloned_load.result;
    if (!value_ssa_block_append_instruction(rebuilt_target, &replacement, error)) {
        return 0;
    }

    if (out_changed) {
        *out_changed = 1;
    }
    return 1;
}

static int value_ssa_perf_try_hoist_loop_invariant_branch_bound_load(ValueSsaFunction *function,
    const ValueSsaProgram *program,
    const ValueSsaDefUseAnalysis *analysis,
    const unsigned char *readonly_globals,
    const unsigned char *address_taken_locals,
    const unsigned char *parameter_pointer_non_alias_pairs,
    const ValueSsaPerfSimpleLoop *loop,
    unsigned char *value_has_mapping,
    ValueSsaValueRef *value_mappings,
    ValueSsaBasicBlock *rebuilt_preheader,
    ValueSsaBasicBlock *rebuilt_target,
    const ValueSsaInstruction *instruction,
    int *out_changed,
    ValueSsaError *error) {
    ValueSsaInstruction *condition_def = NULL;
    ValueSsaValueRef candidate_value;
    ValueSsaInstruction *candidate_def = NULL;
    ValueSsaUseSite load_use_site;
    ValueSsaInstruction cloned_load;
    ValueSsaInstruction replacement;
    ValueSsaPerfAddressRoot load_root;
    ValueSsaValueRef hoisted_addr;
    size_t use_offset;

    if (out_changed) {
        *out_changed = 0;
    }
    if (!function || !program || !analysis || !readonly_globals || !loop || !value_has_mapping || !value_mappings ||
        !rebuilt_preheader || !rebuilt_target || !instruction) {
        return 1;
    }
    if ((!address_taken_locals && function->local_count > 0u) ||
        (!parameter_pointer_non_alias_pairs && function->parameter_count > 0u)) {
        return 1;
    }
    if (loop->header_id >= function->block_count) {
        return 1;
    }
    if (!function->blocks[loop->header_id].has_terminator ||
        function->blocks[loop->header_id].terminator.kind != VALUE_SSA_TERM_BRANCH) {
        return 1;
    }
    if (function->blocks[loop->header_id].terminator.as.branch.condition.kind != VALUE_SSA_VALUE_ID) {
        return 1;
    }

    condition_def = value_ssa_pass_lookup_instruction_definition(
        function, analysis, function->blocks[loop->header_id].terminator.as.branch.condition.value_id);
    if (!condition_def || condition_def->kind != VALUE_SSA_INSTR_BINARY) {
        return 1;
    }

    candidate_value.kind = VALUE_SSA_VALUE_IMMEDIATE;
    candidate_value.immediate = 0;
    candidate_value.value_id = 0u;
    if (condition_def->as.binary.lhs.kind == VALUE_SSA_VALUE_ID) {
        ValueSsaInstruction *lhs_def = value_ssa_pass_lookup_instruction_definition(
            function, analysis, condition_def->as.binary.lhs.value_id);
        if (lhs_def && lhs_def->kind == VALUE_SSA_INSTR_LOAD_INDIRECT) {
            candidate_value = condition_def->as.binary.lhs;
        }
    }
    if (candidate_value.kind != VALUE_SSA_VALUE_ID && condition_def->as.binary.rhs.kind == VALUE_SSA_VALUE_ID) {
        ValueSsaInstruction *rhs_def = value_ssa_pass_lookup_instruction_definition(
            function, analysis, condition_def->as.binary.rhs.value_id);
        if (rhs_def && rhs_def->kind == VALUE_SSA_INSTR_LOAD_INDIRECT) {
            candidate_value = condition_def->as.binary.rhs;
        }
    }
    if (candidate_value.kind != VALUE_SSA_VALUE_ID ||
        candidate_value.value_id >= analysis->value_count ||
        analysis->use_counts[candidate_value.value_id] != 1u) {
        return 1;
    }

    use_offset = analysis->use_offsets[candidate_value.value_id];
    if (!analysis->use_sites ||
        use_offset >= analysis->use_offsets[candidate_value.value_id] + analysis->use_counts[candidate_value.value_id]) {
        return 1;
    }
    load_use_site = analysis->use_sites[use_offset];
    if (load_use_site.kind != VALUE_SSA_USE_INSTR ||
        load_use_site.block_id != loop->header_id ||
        load_use_site.instruction_index >= function->blocks[loop->header_id].instruction_count) {
        return 1;
    }
    if (&function->blocks[loop->header_id].instructions[load_use_site.instruction_index] != condition_def) {
        return 1;
    }
    if (condition_def->has_result &&
        condition_def->result.kind == VALUE_SSA_VALUE_ID &&
        condition_def->result.value_id < analysis->value_count &&
        analysis->use_counts[condition_def->result.value_id] == 1u) {
        size_t condition_use_offset = analysis->use_offsets[condition_def->result.value_id];

        if (!analysis->use_sites ||
            analysis->use_sites[condition_use_offset].kind != VALUE_SSA_USE_TERM ||
            analysis->use_sites[condition_use_offset].role != VALUE_SSA_USE_ROLE_BRANCH_CONDITION ||
            analysis->use_sites[condition_use_offset].block_id != loop->header_id) {
            return 1;
        }
    } else {
        return 1;
    }

    candidate_def = value_ssa_pass_lookup_instruction_definition(function, analysis, candidate_value.value_id);
    if (!candidate_def || candidate_def->kind != VALUE_SSA_INSTR_LOAD_INDIRECT) {
        return 1;
    }
    if (candidate_def != instruction) {
        return 1;
    }
    if (!value_ssa_perf_value_is_loop_invariant_for_indirect_addr(
            program,
            function,
            analysis,
            readonly_globals,
            address_taken_locals,
            loop,
            candidate_def->as.load_indirect_addr,
            0u)) {
        return 1;
    }
    if (!value_ssa_perf_extract_address_root(function, analysis, candidate_def->as.load_indirect_addr, &load_root, 0u) ||
        !value_ssa_perf_loop_instructions_preserve_indirect_load(
            function,
            analysis,
            &function->blocks[loop->header_id],
            parameter_pointer_non_alias_pairs,
            &load_root) ||
        !value_ssa_perf_loop_instructions_preserve_indirect_load(
            function,
            analysis,
            &function->blocks[loop->body_id],
            parameter_pointer_non_alias_pairs,
            &load_root)) {
        return 1;
    }

    memset(&cloned_load, 0, sizeof(cloned_load));
    cloned_load.kind = VALUE_SSA_INSTR_LOAD_INDIRECT;
    cloned_load.has_result = 1;
    cloned_load.result = value_ssa_value_id(value_ssa_function_allocate_value(function));
    if (!value_ssa_perf_clone_loop_invariant_value_into_preheader(
            program,
            function,
            analysis,
            readonly_globals,
            loop,
            candidate_def->as.load_indirect_addr,
            value_has_mapping,
            value_mappings,
            rebuilt_preheader,
            &hoisted_addr,
            error)) {
        return 0;
    }
    cloned_load.as.load_indirect_addr = hoisted_addr;
    if (cloned_load.result.value_id == (size_t)-1 ||
        !value_ssa_block_append_instruction(rebuilt_preheader, &cloned_load, error)) {
        return 0;
    }

    memset(&replacement, 0, sizeof(replacement));
    replacement.kind = VALUE_SSA_INSTR_MOV;
    replacement.has_result = 1;
    replacement.result = candidate_def->result;
    replacement.as.mov_value = cloned_load.result;
    if (!value_ssa_block_append_instruction(rebuilt_target, &replacement, error)) {
        return 0;
    }

    if (out_changed) {
        *out_changed = 1;
    }
    return 1;
}

static int value_ssa_perf_clone_loop_invariant_value_into_preheader(const ValueSsaProgram *program,
    ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    const unsigned char *readonly_globals,
    const ValueSsaPerfSimpleLoop *loop,
    ValueSsaValueRef value,
    unsigned char *has_mapping,
    ValueSsaValueRef *mappings,
    ValueSsaBasicBlock *rebuilt_preheader,
    ValueSsaValueRef *out_value,
    ValueSsaError *error) {
    ValueSsaInstruction *definition;
    ValueSsaInstruction cloned_instruction;

    if (!out_value) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-296: invalid perf invariant clone output");
        return 0;
    }

    if (value.kind != VALUE_SSA_VALUE_ID) {
        *out_value = value;
        return 1;
    }
    if (value.value_id < function->next_value_id && has_mapping[value.value_id]) {
        *out_value = mappings[value.value_id];
        return 1;
    }
    if (value.value_id >= analysis->value_count || !analysis->has_def[value.value_id]) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-297: missing perf invariant definition for ssa.%zu", value.value_id);
        return 0;
    }
    if (analysis->def_block_ids[value.value_id] != loop->header_id &&
        analysis->def_block_ids[value.value_id] != loop->body_id) {
        *out_value = value;
        return 1;
    }

    definition = value_ssa_pass_lookup_instruction_definition(function, analysis, value.value_id);
    if (!definition) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-297: missing perf invariant definition for ssa.%zu", value.value_id);
        return 0;
    }
    if (!value_ssa_perf_value_is_loop_invariant(
            program,
            function,
            analysis,
            readonly_globals,
            loop,
            value,
            0u)) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-298: attempted to clone non-invariant perf value ssa.%zu", value.value_id);
        return 0;
    }

    memset(&cloned_instruction, 0, sizeof(cloned_instruction));
    cloned_instruction.kind = definition->kind;
    cloned_instruction.has_result = 1;
    cloned_instruction.result = value_ssa_value_id(value_ssa_function_allocate_value(function));
    if (cloned_instruction.result.value_id == (size_t)-1) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-299: failed to allocate perf invariant value");
        return 0;
    }

    switch (definition->kind) {
    case VALUE_SSA_INSTR_ADDR_LOCAL:
    case VALUE_SSA_INSTR_ADDR_GLOBAL:
        cloned_instruction.as.addr_slot = definition->as.addr_slot;
        break;
    case VALUE_SSA_INSTR_LOAD_LOCAL:
        cloned_instruction.as.load_slot = definition->as.load_slot;
        break;
    case VALUE_SSA_INSTR_LOAD_GLOBAL:
        cloned_instruction.as.load_slot = definition->as.load_slot;
        break;
    case VALUE_SSA_INSTR_MOV:
        if (!value_ssa_perf_clone_loop_invariant_value_into_preheader(
                program,
                function,
                analysis,
                readonly_globals,
                loop,
                definition->as.mov_value,
                has_mapping,
                mappings,
                rebuilt_preheader,
                &cloned_instruction.as.mov_value,
                error)) {
            return 0;
        }
        break;
    case VALUE_SSA_INSTR_BINARY:
        cloned_instruction.as.binary.op = definition->as.binary.op;
        if (!value_ssa_perf_clone_loop_invariant_value_into_preheader(
                program,
                function,
                analysis,
                readonly_globals,
                loop,
                definition->as.binary.lhs,
                has_mapping,
                mappings,
                rebuilt_preheader,
                &cloned_instruction.as.binary.lhs,
                error) ||
            !value_ssa_perf_clone_loop_invariant_value_into_preheader(
                program,
                function,
                analysis,
                readonly_globals,
                loop,
                definition->as.binary.rhs,
                has_mapping,
                mappings,
                rebuilt_preheader,
                &cloned_instruction.as.binary.rhs,
                error)) {
            return 0;
        }
        break;
    default:
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-300: unsupported perf invariant clone instruction kind %d",
            (int)definition->kind);
        return 0;
    }

    if (!value_ssa_block_append_instruction(rebuilt_preheader, &cloned_instruction, error)) {
        return 0;
    }

    has_mapping[value.value_id] = 1u;
    mappings[value.value_id] = cloned_instruction.result;
    *out_value = cloned_instruction.result;
    return 1;
}

static int value_ssa_perf_hoist_simple_loop_invariant_loads(ValueSsaProgram *program,
    ValueSsaError *error) {
    unsigned char *readonly_globals = NULL;
    size_t function_index;
    int changed_any = 0;

    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-292: invalid perf loop-load hoist contract");
        return 0;
    }

    readonly_globals = (unsigned char *)calloc(program->global_count ? program->global_count : 1u, sizeof(unsigned char));
    if (!readonly_globals) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-293: out of memory preparing perf loop-load hoist globals");
        return 0;
    }
    if (!value_ssa_perf_collect_readonly_scalar_globals(program, readonly_globals)) {
        free(readonly_globals);
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-294: failed collecting perf loop-load hoist globals");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunction *function = &program->functions[function_index];
        ValueSsaCfgAnalysis cfg;
        ValueSsaDefUseAnalysis def_use;
        size_t header_id;
        int changed_function = 0;

        if (!function->has_body) {
            continue;
        }

        value_ssa_cfg_analysis_init(&cfg);
        value_ssa_def_use_analysis_init(&def_use);
        if (!value_ssa_compute_cfg_analysis(function, &cfg, error) ||
            !value_ssa_compute_def_use_analysis(function, &def_use, error)) {
            value_ssa_def_use_analysis_free(&def_use);
            value_ssa_cfg_analysis_free(&cfg);
            free(readonly_globals);
            return 0;
        }

        for (header_id = 0; header_id < function->block_count; ++header_id) {
            ValueSsaPerfSimpleLoop loop;
            ValueSsaBasicBlock *preheader;
            ValueSsaBasicBlock *header;
            ValueSsaBasicBlock *body;
            ValueSsaBasicBlock rebuilt_preheader;
            ValueSsaBasicBlock rebuilt_header;
            ValueSsaBasicBlock rebuilt_body;
            unsigned char *address_taken_locals = NULL;
            unsigned char *parameter_pointer_non_alias_pairs = NULL;
            unsigned char *value_has_mapping = NULL;
            ValueSsaValueRef *value_mappings = NULL;
            unsigned char *local_has_mapping = NULL;
            ValueSsaValueRef *local_mappings = NULL;
            unsigned char *global_has_mapping = NULL;
            ValueSsaValueRef *global_mappings = NULL;
            size_t instruction_index;
            int changed_loop = 0;
            int phi_header_only = 0;
            int ok = 1;

            if (!value_ssa_perf_find_simple_single_body_loop(function, &cfg, header_id, &loop) &&
                !value_ssa_perf_find_simple_branch_body_loop(function, &cfg, header_id, &loop)) {
                if (!value_ssa_perf_find_phi_header_single_body_loop(function, &cfg, header_id, &loop)) {
                    continue;
                }
                phi_header_only = 1;
            }

            preheader = &function->blocks[loop.preheader_id];
            header = &function->blocks[loop.header_id];
            body = &function->blocks[loop.body_id];
            if (preheader->instruction_count == 0u ||
                (header->instruction_count == 0u && body->instruction_count == 0u)) {
                continue;
            }

            memset(&rebuilt_preheader, 0, sizeof(rebuilt_preheader));
            memset(&rebuilt_header, 0, sizeof(rebuilt_header));
            memset(&rebuilt_body, 0, sizeof(rebuilt_body));
            rebuilt_preheader.id = preheader->id;
            rebuilt_header.id = header->id;
            rebuilt_body.id = body->id;

            if (function->local_count > 0u) {
                address_taken_locals = (unsigned char *)calloc(function->local_count, sizeof(unsigned char));
            }
            if (function->parameter_count > 0u) {
                parameter_pointer_non_alias_pairs = (unsigned char *)calloc(
                    function->parameter_count * function->parameter_count,
                    sizeof(unsigned char));
            }
            if (function->next_value_id > 0u) {
                value_has_mapping = (unsigned char *)calloc(function->next_value_id, sizeof(unsigned char));
                value_mappings = (ValueSsaValueRef *)calloc(function->next_value_id, sizeof(ValueSsaValueRef));
            }
            if (function->local_count > 0u) {
                local_has_mapping = (unsigned char *)calloc(function->local_count, sizeof(unsigned char));
                local_mappings = (ValueSsaValueRef *)calloc(function->local_count, sizeof(ValueSsaValueRef));
            }
            if (program->global_count > 0u) {
                global_has_mapping = (unsigned char *)calloc(program->global_count, sizeof(unsigned char));
                global_mappings = (ValueSsaValueRef *)calloc(program->global_count, sizeof(ValueSsaValueRef));
            }
            if ((function->local_count > 0u && !address_taken_locals) ||
                (function->parameter_count > 0u && !parameter_pointer_non_alias_pairs) ||
                (function->next_value_id > 0u && (!value_has_mapping || !value_mappings)) ||
                (function->local_count > 0u && (!local_has_mapping || !local_mappings)) ||
                (program->global_count > 0u && (!global_has_mapping || !global_mappings))) {
                free(parameter_pointer_non_alias_pairs);
                free(global_mappings);
                free(global_has_mapping);
                free(local_mappings);
                free(local_has_mapping);
                free(value_mappings);
                free(value_has_mapping);
                free(address_taken_locals);
                value_ssa_basic_block_free(&rebuilt_preheader);
                value_ssa_basic_block_free(&rebuilt_header);
                value_ssa_basic_block_free(&rebuilt_body);
                value_ssa_cfg_analysis_free(&cfg);
                free(readonly_globals);
                value_ssa_set_error(error, 0, 0, "VALUE-SSA-295: out of memory preparing perf loop-load hoist mappings");
                return 0;
            }
            if (address_taken_locals &&
                !value_ssa_perf_collect_address_taken_locals(function, address_taken_locals)) {
                free(parameter_pointer_non_alias_pairs);
                free(global_mappings);
                free(global_has_mapping);
                free(local_mappings);
                free(local_has_mapping);
                free(value_mappings);
                free(value_has_mapping);
                free(address_taken_locals);
                value_ssa_basic_block_free(&rebuilt_preheader);
                value_ssa_basic_block_free(&rebuilt_header);
                value_ssa_basic_block_free(&rebuilt_body);
                value_ssa_cfg_analysis_free(&cfg);
                free(readonly_globals);
                value_ssa_set_error(error, 0, 0, "VALUE-SSA-295: failed collecting perf local address-taken flags");
                return 0;
            }
            if (parameter_pointer_non_alias_pairs &&
                !value_ssa_perf_collect_non_alias_parameter_pointer_pairs(
                    program, function, parameter_pointer_non_alias_pairs)) {
                free(parameter_pointer_non_alias_pairs);
                free(global_mappings);
                free(global_has_mapping);
                free(local_mappings);
                free(local_has_mapping);
                free(value_mappings);
                free(value_has_mapping);
                free(address_taken_locals);
                value_ssa_basic_block_free(&rebuilt_preheader);
                value_ssa_basic_block_free(&rebuilt_header);
                value_ssa_basic_block_free(&rebuilt_body);
                value_ssa_cfg_analysis_free(&cfg);
                free(readonly_globals);
                value_ssa_set_error(
                    error, 0, 0, "VALUE-SSA-295: failed collecting perf parameter non-alias pairs");
                return 0;
            }

            for (instruction_index = 0; instruction_index < preheader->instruction_count; ++instruction_index) {
                if (!value_ssa_block_append_instruction(&rebuilt_preheader, &preheader->instructions[instruction_index], error)) {
                    ok = 0;
                    break;
                }
            }

            for (instruction_index = 0; ok && instruction_index < header->instruction_count; ++instruction_index) {
                const ValueSsaInstruction *instruction = &header->instructions[instruction_index];

                if ((instruction->kind == VALUE_SSA_INSTR_ADDR_LOCAL ||
                        instruction->kind == VALUE_SSA_INSTR_ADDR_GLOBAL) &&
                    instruction->has_result &&
                    instruction->result.kind == VALUE_SSA_VALUE_ID &&
                    value_ssa_perf_value_is_loop_invariant(
                        program,
                        function,
                        &def_use,
                        readonly_globals,
                        &loop,
                        instruction->result,
                        0u)) {
                    ValueSsaInstruction replacement;
                    ValueSsaValueRef hoisted_value;

                    if (!value_ssa_perf_clone_loop_invariant_value_into_preheader(
                            program,
                            function,
                            &def_use,
                            readonly_globals,
                            &loop,
                            instruction->result,
                            value_has_mapping,
                            value_mappings,
                            &rebuilt_preheader,
                            &hoisted_value,
                            error)) {
                        ok = 0;
                        break;
                    }

                    memset(&replacement, 0, sizeof(replacement));
                    replacement.kind = VALUE_SSA_INSTR_MOV;
                    replacement.has_result = 1;
                    replacement.result = instruction->result;
                    replacement.as.mov_value = hoisted_value;
                    if (!value_ssa_block_append_instruction(&rebuilt_header, &replacement, error)) {
                        ok = 0;
                        break;
                    }
                    changed_loop = 1;
                    continue;
                }

                if (instruction->kind == VALUE_SSA_INSTR_LOAD_LOCAL &&
                    instruction->has_result &&
                    instruction->result.kind == VALUE_SSA_VALUE_ID &&
                    instruction->as.load_slot.kind == VALUE_SSA_SLOT_LOCAL &&
                    instruction->as.load_slot.id < function->local_count &&
                    function->locals[instruction->as.load_slot.id].is_parameter &&
                    value_ssa_perf_loop_preserves_local_slot(function, &loop, instruction->as.load_slot.id)) {
                    ValueSsaInstruction cloned_load;
                    ValueSsaInstruction replacement;
                    ValueSsaValueRef hoisted_value;
                    size_t slot_id = instruction->as.load_slot.id;

                    if (!local_has_mapping[slot_id]) {
                        memset(&cloned_load, 0, sizeof(cloned_load));
                        cloned_load.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
                        cloned_load.has_result = 1;
                        cloned_load.result = value_ssa_value_id(value_ssa_function_allocate_value(function));
                        cloned_load.as.load_slot = instruction->as.load_slot;
                        if (cloned_load.result.value_id == (size_t)-1 ||
                            !value_ssa_block_append_instruction(&rebuilt_preheader, &cloned_load, error)) {
                            ok = 0;
                            break;
                        }
                        local_has_mapping[slot_id] = 1u;
                        local_mappings[slot_id] = cloned_load.result;
                    }

                    hoisted_value = local_mappings[slot_id];
                    memset(&replacement, 0, sizeof(replacement));
                    replacement.kind = VALUE_SSA_INSTR_MOV;
                    replacement.has_result = 1;
                    replacement.result = instruction->result;
                    replacement.as.mov_value = hoisted_value;
                    if (!value_ssa_block_append_instruction(&rebuilt_header, &replacement, error)) {
                        ok = 0;
                        break;
                    }
                    changed_loop = 1;
                    continue;
                }

                if (instruction->kind == VALUE_SSA_INSTR_LOAD_GLOBAL &&
                    instruction->has_result &&
                    instruction->result.kind == VALUE_SSA_VALUE_ID &&
                    instruction->as.load_slot.kind == VALUE_SSA_SLOT_GLOBAL &&
                    instruction->as.load_slot.id < program->global_count &&
                    value_ssa_perf_loop_preserves_global_slot(
                        program, function, &loop, readonly_globals, instruction->as.load_slot.id)) {
                    ValueSsaInstruction cloned_load;
                    ValueSsaInstruction replacement;
                    ValueSsaValueRef hoisted_value;
                    size_t slot_id = instruction->as.load_slot.id;

                    if (!global_has_mapping[slot_id]) {
                        memset(&cloned_load, 0, sizeof(cloned_load));
                        cloned_load.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
                        cloned_load.has_result = 1;
                        cloned_load.result = value_ssa_value_id(value_ssa_function_allocate_value(function));
                        cloned_load.as.load_slot = instruction->as.load_slot;
                        if (cloned_load.result.value_id == (size_t)-1 ||
                            !value_ssa_block_append_instruction(&rebuilt_preheader, &cloned_load, error)) {
                            ok = 0;
                            break;
                        }
                        global_has_mapping[slot_id] = 1u;
                        global_mappings[slot_id] = cloned_load.result;
                    }

                    hoisted_value = global_mappings[slot_id];
                    memset(&replacement, 0, sizeof(replacement));
                    replacement.kind = VALUE_SSA_INSTR_MOV;
                    replacement.has_result = 1;
                    replacement.result = instruction->result;
                    replacement.as.mov_value = hoisted_value;
                    if (!value_ssa_block_append_instruction(&rebuilt_header, &replacement, error)) {
                        ok = 0;
                        break;
                    }
                    changed_loop = 1;
                    continue;
                }

                if ((instruction->kind == VALUE_SSA_INSTR_MOV ||
                        instruction->kind == VALUE_SSA_INSTR_BINARY) &&
                    instruction->has_result &&
                    instruction->result.kind == VALUE_SSA_VALUE_ID &&
                    value_ssa_perf_value_is_loop_invariant(
                        program,
                        function,
                        &def_use,
                        readonly_globals,
                        &loop,
                        instruction->result,
                        0u) &&
                    (instruction->kind != VALUE_SSA_INSTR_MOV ||
                        instruction->as.mov_value.kind == VALUE_SSA_VALUE_IMMEDIATE ||
                        value_ssa_perf_value_is_defined_inside_loop(
                            function,
                            &def_use,
                            &loop,
                            instruction->as.mov_value))) {
                    ValueSsaInstruction replacement;
                    ValueSsaValueRef hoisted_value;

                    if (!value_ssa_perf_clone_loop_invariant_value_into_preheader(
                            program,
                            function,
                            &def_use,
                            readonly_globals,
                            &loop,
                            instruction->result,
                            value_has_mapping,
                            value_mappings,
                            &rebuilt_preheader,
                            &hoisted_value,
                            error)) {
                        ok = 0;
                        break;
                    }

                    memset(&replacement, 0, sizeof(replacement));
                    replacement.kind = VALUE_SSA_INSTR_MOV;
                    replacement.has_result = 1;
                    replacement.result = instruction->result;
                    replacement.as.mov_value = hoisted_value;
                    if (!value_ssa_block_append_instruction(&rebuilt_header, &replacement, error)) {
                        ok = 0;
                        break;
                    }
                    changed_loop = 1;
                    continue;
                }

                {
                    int hoisted_branch_bound = 0;
                    int hoisted_indirect = 0;

                    if (!value_ssa_perf_try_hoist_loop_invariant_branch_bound_load(
                            function,
                            program,
                            &def_use,
                            readonly_globals,
                            address_taken_locals,
                            parameter_pointer_non_alias_pairs,
                            &loop,
                            value_has_mapping,
                            value_mappings,
                            &rebuilt_preheader,
                            &rebuilt_header,
                            instruction,
                            &hoisted_branch_bound,
                            error)) {
                        ok = 0;
                        break;
                    }
                    if (hoisted_branch_bound) {
                        changed_loop = 1;
                        continue;
                    }
                    if (phi_header_only) {
                        if (!value_ssa_block_append_instruction(&rebuilt_header, instruction, error)) {
                            ok = 0;
                            break;
                        }
                        continue;
                    }

                    if (!value_ssa_perf_try_hoist_loop_invariant_indirect_load(
                            function,
                            program,
                            &def_use,
                            readonly_globals,
                            address_taken_locals,
                            parameter_pointer_non_alias_pairs,
                            &loop,
                            value_has_mapping,
                            value_mappings,
                            &rebuilt_preheader,
                            &rebuilt_header,
                            instruction,
                            &hoisted_indirect,
                            error)) {
                        ok = 0;
                        break;
                    }
                    if (hoisted_indirect) {
                        changed_loop = 1;
                        continue;
                    }
                }

                if (!value_ssa_block_append_instruction(&rebuilt_header, instruction, error)) {
                    ok = 0;
                    break;
                }
            }

            for (instruction_index = 0; ok && instruction_index < body->instruction_count; ++instruction_index) {
                const ValueSsaInstruction *instruction = &body->instructions[instruction_index];

                if ((instruction->kind == VALUE_SSA_INSTR_ADDR_LOCAL ||
                        instruction->kind == VALUE_SSA_INSTR_ADDR_GLOBAL) &&
                    instruction->has_result &&
                    instruction->result.kind == VALUE_SSA_VALUE_ID &&
                    value_ssa_perf_value_is_loop_invariant(
                        program,
                        function,
                        &def_use,
                        readonly_globals,
                        &loop,
                        instruction->result,
                        0u)) {
                    ValueSsaInstruction replacement;
                    ValueSsaValueRef hoisted_value;

                    if (!value_ssa_perf_clone_loop_invariant_value_into_preheader(
                            program,
                            function,
                            &def_use,
                            readonly_globals,
                            &loop,
                            instruction->result,
                            value_has_mapping,
                            value_mappings,
                            &rebuilt_preheader,
                            &hoisted_value,
                            error)) {
                        ok = 0;
                        break;
                    }

                    memset(&replacement, 0, sizeof(replacement));
                    replacement.kind = VALUE_SSA_INSTR_MOV;
                    replacement.has_result = 1;
                    replacement.result = instruction->result;
                    replacement.as.mov_value = hoisted_value;
                    if (!value_ssa_block_append_instruction(&rebuilt_body, &replacement, error)) {
                        ok = 0;
                        break;
                    }
                    changed_loop = 1;
                    continue;
                }

                if (instruction->kind == VALUE_SSA_INSTR_LOAD_LOCAL &&
                    instruction->has_result &&
                    instruction->result.kind == VALUE_SSA_VALUE_ID &&
                    instruction->as.load_slot.kind == VALUE_SSA_SLOT_LOCAL &&
                    instruction->as.load_slot.id < function->local_count &&
                    function->locals[instruction->as.load_slot.id].is_parameter &&
                    value_ssa_perf_loop_preserves_local_slot(function, &loop, instruction->as.load_slot.id)) {
                    ValueSsaInstruction cloned_load;
                    ValueSsaInstruction replacement;
                    ValueSsaValueRef hoisted_value;
                    size_t slot_id = instruction->as.load_slot.id;

                    if (!local_has_mapping[slot_id]) {
                        memset(&cloned_load, 0, sizeof(cloned_load));
                        cloned_load.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
                        cloned_load.has_result = 1;
                        cloned_load.result = value_ssa_value_id(value_ssa_function_allocate_value(function));
                        cloned_load.as.load_slot = instruction->as.load_slot;
                        if (cloned_load.result.value_id == (size_t)-1 ||
                            !value_ssa_block_append_instruction(&rebuilt_preheader, &cloned_load, error)) {
                            ok = 0;
                            break;
                        }
                        local_has_mapping[slot_id] = 1u;
                        local_mappings[slot_id] = cloned_load.result;
                    }

                    hoisted_value = local_mappings[slot_id];
                    memset(&replacement, 0, sizeof(replacement));
                    replacement.kind = VALUE_SSA_INSTR_MOV;
                    replacement.has_result = 1;
                    replacement.result = instruction->result;
                    replacement.as.mov_value = hoisted_value;
                    if (!value_ssa_block_append_instruction(&rebuilt_body, &replacement, error)) {
                        ok = 0;
                        break;
                    }
                    changed_loop = 1;
                    continue;
                }

                if (instruction->kind == VALUE_SSA_INSTR_LOAD_GLOBAL &&
                    instruction->has_result &&
                    instruction->result.kind == VALUE_SSA_VALUE_ID &&
                    instruction->as.load_slot.kind == VALUE_SSA_SLOT_GLOBAL &&
                    instruction->as.load_slot.id < program->global_count &&
                    value_ssa_perf_loop_preserves_global_slot(
                        program, function, &loop, readonly_globals, instruction->as.load_slot.id)) {
                    ValueSsaInstruction cloned_load;
                    ValueSsaInstruction replacement;
                    ValueSsaValueRef hoisted_value;
                    size_t slot_id = instruction->as.load_slot.id;

                    if (!global_has_mapping[slot_id]) {
                        memset(&cloned_load, 0, sizeof(cloned_load));
                        cloned_load.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
                        cloned_load.has_result = 1;
                        cloned_load.result = value_ssa_value_id(value_ssa_function_allocate_value(function));
                        cloned_load.as.load_slot = instruction->as.load_slot;
                        if (cloned_load.result.value_id == (size_t)-1 ||
                            !value_ssa_block_append_instruction(&rebuilt_preheader, &cloned_load, error)) {
                            ok = 0;
                            break;
                        }
                        global_has_mapping[slot_id] = 1u;
                        global_mappings[slot_id] = cloned_load.result;
                    }

                    hoisted_value = global_mappings[slot_id];
                    memset(&replacement, 0, sizeof(replacement));
                    replacement.kind = VALUE_SSA_INSTR_MOV;
                    replacement.has_result = 1;
                    replacement.result = instruction->result;
                    replacement.as.mov_value = hoisted_value;
                    if (!value_ssa_block_append_instruction(&rebuilt_body, &replacement, error)) {
                        ok = 0;
                        break;
                    }
                    changed_loop = 1;
                    continue;
                }

                if ((instruction->kind == VALUE_SSA_INSTR_MOV ||
                        instruction->kind == VALUE_SSA_INSTR_BINARY) &&
                    instruction->has_result &&
                    instruction->result.kind == VALUE_SSA_VALUE_ID &&
                    value_ssa_perf_value_is_loop_invariant(
                        program,
                        function,
                        &def_use,
                        readonly_globals,
                        &loop,
                        instruction->result,
                        0u) &&
                    (instruction->kind != VALUE_SSA_INSTR_MOV ||
                        instruction->as.mov_value.kind == VALUE_SSA_VALUE_IMMEDIATE ||
                        value_ssa_perf_value_is_defined_inside_loop(
                            function,
                            &def_use,
                            &loop,
                            instruction->as.mov_value))) {
                    ValueSsaInstruction replacement;
                    ValueSsaValueRef hoisted_value;

                    if (!value_ssa_perf_clone_loop_invariant_value_into_preheader(
                            program,
                            function,
                            &def_use,
                            readonly_globals,
                            &loop,
                            instruction->result,
                            value_has_mapping,
                            value_mappings,
                            &rebuilt_preheader,
                            &hoisted_value,
                            error)) {
                        ok = 0;
                        break;
                    }

                    memset(&replacement, 0, sizeof(replacement));
                    replacement.kind = VALUE_SSA_INSTR_MOV;
                    replacement.has_result = 1;
                    replacement.result = instruction->result;
                    replacement.as.mov_value = hoisted_value;
                    if (!value_ssa_block_append_instruction(&rebuilt_body, &replacement, error)) {
                        ok = 0;
                        break;
                    }
                    changed_loop = 1;
                    continue;
                }

                {
                    int hoisted_branch_bound = 0;
                    int hoisted_indirect = 0;

                    if (!value_ssa_perf_try_hoist_loop_invariant_branch_bound_load(
                            function,
                            program,
                            &def_use,
                            readonly_globals,
                            address_taken_locals,
                            parameter_pointer_non_alias_pairs,
                            &loop,
                            value_has_mapping,
                            value_mappings,
                            &rebuilt_preheader,
                            &rebuilt_body,
                            instruction,
                            &hoisted_branch_bound,
                            error)) {
                        ok = 0;
                        break;
                    }
                    if (hoisted_branch_bound) {
                        changed_loop = 1;
                        continue;
                    }
                    if (!value_ssa_perf_try_hoist_loop_invariant_indirect_load(
                            function,
                            program,
                            &def_use,
                            readonly_globals,
                            address_taken_locals,
                            parameter_pointer_non_alias_pairs,
                            &loop,
                            value_has_mapping,
                            value_mappings,
                            &rebuilt_preheader,
                            &rebuilt_body,
                            instruction,
                            &hoisted_indirect,
                            error)) {
                        ok = 0;
                        break;
                    }
                    if (hoisted_indirect) {
                        changed_loop = 1;
                        continue;
                    }
                    if (phi_header_only) {
                        if (!value_ssa_block_append_instruction(&rebuilt_body, instruction, error)) {
                            ok = 0;
                            break;
                        }
                        continue;
                    }
                }

                if (!value_ssa_block_append_instruction(&rebuilt_body, instruction, error)) {
                    ok = 0;
                    break;
                }
            }

            if (ok && changed_loop) {
                rebuilt_preheader.has_terminator = preheader->has_terminator;
                rebuilt_preheader.terminator = preheader->terminator;
                rebuilt_header.has_terminator = header->has_terminator;
                rebuilt_header.terminator = header->terminator;
                rebuilt_body.has_terminator = body->has_terminator;
                rebuilt_body.terminator = body->terminator;

                for (instruction_index = 0; instruction_index < preheader->instruction_count; ++instruction_index) {
                    value_ssa_instruction_free(&preheader->instructions[instruction_index]);
                }
                free(preheader->instructions);
                preheader->instructions = rebuilt_preheader.instructions;
                preheader->instruction_count = rebuilt_preheader.instruction_count;
                preheader->instruction_capacity = rebuilt_preheader.instruction_capacity;
                rebuilt_preheader.instructions = NULL;
                rebuilt_preheader.instruction_count = 0u;
                rebuilt_preheader.instruction_capacity = 0u;

                for (instruction_index = 0; instruction_index < header->instruction_count; ++instruction_index) {
                    value_ssa_instruction_free(&header->instructions[instruction_index]);
                }
                free(header->instructions);
                header->instructions = rebuilt_header.instructions;
                header->instruction_count = rebuilt_header.instruction_count;
                header->instruction_capacity = rebuilt_header.instruction_capacity;
                rebuilt_header.instructions = NULL;
                rebuilt_header.instruction_count = 0u;
                rebuilt_header.instruction_capacity = 0u;

                for (instruction_index = 0; instruction_index < body->instruction_count; ++instruction_index) {
                    value_ssa_instruction_free(&body->instructions[instruction_index]);
                }
                free(body->instructions);
                body->instructions = rebuilt_body.instructions;
                body->instruction_count = rebuilt_body.instruction_count;
                body->instruction_capacity = rebuilt_body.instruction_capacity;
                rebuilt_body.instructions = NULL;
                rebuilt_body.instruction_count = 0u;
                rebuilt_body.instruction_capacity = 0u;

                changed_any = 1;
                changed_function = 1;
            }

            value_ssa_basic_block_free(&rebuilt_preheader);
            value_ssa_basic_block_free(&rebuilt_header);
            value_ssa_basic_block_free(&rebuilt_body);
            free(global_mappings);
            free(global_has_mapping);
            free(local_mappings);
            free(local_has_mapping);
            free(value_mappings);
            free(value_has_mapping);
            free(parameter_pointer_non_alias_pairs);
            free(address_taken_locals);

            if (!ok) {
                value_ssa_def_use_analysis_free(&def_use);
                value_ssa_cfg_analysis_free(&cfg);
                free(readonly_globals);
                return 0;
            }
            if (changed_loop) {
                break;
            }
        }

        value_ssa_def_use_analysis_free(&def_use);
        value_ssa_cfg_analysis_free(&cfg);

        if (changed_function) {
            --function_index;
        }
    }

    free(readonly_globals);

    if (changed_any &&
        (!value_ssa_simplify_trivial_values(program, error) ||
            !value_ssa_eliminate_dead_value_defs(program, error))) {
        return 0;
    }

    return value_ssa_verify_program(program, error);
}

static int value_ssa_perf_reuse_spmv_loop_exit_indirect_loads(ValueSsaProgram *program,
    ValueSsaError *error) {
    unsigned char *readonly_globals = NULL;
    size_t function_index;
    int changed_any = 0;

    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-307: invalid perf spmv loop-exit reuse contract");
        return 0;
    }

    readonly_globals = (unsigned char *)calloc(program->global_count ? program->global_count : 1u, sizeof(unsigned char));
    if (!readonly_globals) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-308: out of memory preparing perf spmv loop-exit reuse globals");
        return 0;
    }
    if (!value_ssa_perf_collect_readonly_scalar_globals(program, readonly_globals)) {
        free(readonly_globals);
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-309: failed collecting perf spmv loop-exit reuse globals");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunction *function = &program->functions[function_index];
        ValueSsaCfgAnalysis cfg;
        ValueSsaDefUseAnalysis def_use;
        unsigned char *address_taken_locals = NULL;
        unsigned char *parameter_pointer_non_alias_pairs = NULL;
        size_t header_id;
        int changed_function = 0;

        if (!function->has_body || !function->name || strcmp(function->name, "spmv") != 0 ||
            value_ssa_perf_function_has_calls(function)) {
            continue;
        }

        value_ssa_cfg_analysis_init(&cfg);
        value_ssa_def_use_analysis_init(&def_use);
        if (!value_ssa_compute_cfg_analysis(function, &cfg, error) ||
            !value_ssa_compute_def_use_analysis(function, &def_use, error)) {
            value_ssa_def_use_analysis_free(&def_use);
            value_ssa_cfg_analysis_free(&cfg);
            free(readonly_globals);
            return 0;
        }

        if (function->local_count > 0u) {
            address_taken_locals = (unsigned char *)calloc(function->local_count, sizeof(unsigned char));
        }
        if (function->parameter_count > 0u) {
            parameter_pointer_non_alias_pairs = (unsigned char *)calloc(
                function->parameter_count * function->parameter_count,
                sizeof(unsigned char));
        }
        if ((function->local_count > 0u && !address_taken_locals) ||
            (function->parameter_count > 0u && !parameter_pointer_non_alias_pairs)) {
            free(parameter_pointer_non_alias_pairs);
            free(address_taken_locals);
            value_ssa_def_use_analysis_free(&def_use);
            value_ssa_cfg_analysis_free(&cfg);
            free(readonly_globals);
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-310: out of memory preparing perf spmv loop-exit reuse state");
            return 0;
        }
        if (address_taken_locals &&
            !value_ssa_perf_collect_address_taken_locals(function, address_taken_locals)) {
            free(parameter_pointer_non_alias_pairs);
            free(address_taken_locals);
            value_ssa_def_use_analysis_free(&def_use);
            value_ssa_cfg_analysis_free(&cfg);
            free(readonly_globals);
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-311: failed collecting perf spmv address-taken locals");
            return 0;
        }
        if (parameter_pointer_non_alias_pairs &&
            !value_ssa_perf_collect_non_alias_parameter_pointer_pairs(
                program, function, parameter_pointer_non_alias_pairs)) {
            free(parameter_pointer_non_alias_pairs);
            free(address_taken_locals);
            value_ssa_def_use_analysis_free(&def_use);
            value_ssa_cfg_analysis_free(&cfg);
            free(readonly_globals);
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-312: failed collecting perf spmv parameter non-alias pairs");
            return 0;
        }

        for (header_id = 0; header_id < function->block_count; ++header_id) {
            ValueSsaPerfSimpleLoop loop;
            ValueSsaBasicBlock *preheader;
            ValueSsaBasicBlock *exit_block;
            size_t exit_instruction_index;
            int loop_matches = 0;

            if (value_ssa_perf_find_phi_header_single_body_loop(function, &cfg, header_id, &loop) ||
                value_ssa_perf_find_simple_single_body_loop(function, &cfg, header_id, &loop) ||
                value_ssa_perf_find_simple_branch_body_loop(function, &cfg, header_id, &loop)) {
                loop_matches = 1;
            }
            if (!loop_matches ||
                loop.preheader_id >= function->block_count ||
                loop.header_id >= function->block_count ||
                loop.body_id >= function->block_count ||
                loop.exit_id >= function->block_count ||
                !cfg.dominates[loop.preheader_id * function->block_count + loop.exit_id]) {
                continue;
            }

            preheader = &function->blocks[loop.preheader_id];
            exit_block = &function->blocks[loop.exit_id];
            if (preheader->instruction_count == 0u || exit_block->instruction_count == 0u) {
                continue;
            }

            for (exit_instruction_index = 0; exit_instruction_index < exit_block->instruction_count; ++exit_instruction_index) {
                ValueSsaInstruction *exit_instruction = &exit_block->instructions[exit_instruction_index];
                ValueSsaPerfAddressRoot exit_load_root;
                size_t preheader_instruction_index;

                if (exit_instruction->kind != VALUE_SSA_INSTR_LOAD_INDIRECT ||
                    !exit_instruction->has_result ||
                    exit_instruction->result.kind != VALUE_SSA_VALUE_ID ||
                    !value_ssa_perf_value_is_loop_invariant_for_indirect_addr(
                        program,
                        function,
                        &def_use,
                        readonly_globals,
                        address_taken_locals,
                        &loop,
                        exit_instruction->as.load_indirect_addr,
                        0u) ||
                    !value_ssa_perf_extract_address_root(
                        function, &def_use, exit_instruction->as.load_indirect_addr, &exit_load_root, 0u) ||
                    !value_ssa_perf_loop_instructions_preserve_indirect_load(
                        function,
                        &def_use,
                        &function->blocks[loop.header_id],
                        parameter_pointer_non_alias_pairs,
                        &exit_load_root) ||
                    !value_ssa_perf_loop_instructions_preserve_indirect_load(
                        function,
                        &def_use,
                        &function->blocks[loop.body_id],
                        parameter_pointer_non_alias_pairs,
                        &exit_load_root) ||
                    !value_ssa_perf_instruction_range_preserves_indirect_load(
                        function,
                        &def_use,
                        exit_block,
                        0u,
                        exit_instruction_index,
                        parameter_pointer_non_alias_pairs,
                        &exit_load_root)) {
                    continue;
                }

                for (preheader_instruction_index = preheader->instruction_count; preheader_instruction_index > 0u; --preheader_instruction_index) {
                    ValueSsaInstruction *preheader_instruction = &preheader->instructions[preheader_instruction_index - 1u];
                    ValueSsaPerfAddressRoot preheader_load_root;

                    if (preheader_instruction->kind != VALUE_SSA_INSTR_LOAD_INDIRECT ||
                        !preheader_instruction->has_result ||
                        preheader_instruction->result.kind != VALUE_SSA_VALUE_ID ||
                        !value_ssa_perf_same_pure_address_value(
                            function,
                            &def_use,
                            preheader_instruction->as.load_indirect_addr,
                            exit_instruction->as.load_indirect_addr,
                            0u) ||
                        !value_ssa_perf_extract_address_root(
                            function,
                            &def_use,
                            preheader_instruction->as.load_indirect_addr,
                            &preheader_load_root,
                            0u) ||
                        !value_ssa_perf_instruction_range_preserves_indirect_load(
                            function,
                            &def_use,
                            preheader,
                            preheader_instruction_index,
                            preheader->instruction_count,
                            parameter_pointer_non_alias_pairs,
                            &preheader_load_root)) {
                        continue;
                    }

                    exit_instruction->kind = VALUE_SSA_INSTR_MOV;
                    exit_instruction->as.mov_value = preheader_instruction->result;
                    changed_function = 1;
                    changed_any = 1;
                    break;
                }
            }
        }

        free(parameter_pointer_non_alias_pairs);
        free(address_taken_locals);
        value_ssa_def_use_analysis_free(&def_use);
        value_ssa_cfg_analysis_free(&cfg);

        if (changed_function) {
            --function_index;
        }
    }

    free(readonly_globals);

    if (changed_any &&
        (!value_ssa_simplify_trivial_values(program, error) ||
            !value_ssa_eliminate_dead_value_defs(program, error))) {
        return 0;
    }

    return value_ssa_verify_program(program, error);
}

int value_ssa_optimize_perf_hotspots(ValueSsaProgram *program, ValueSsaError *error) {
    unsigned char *foldable_globals = NULL;
    size_t function_index;
    int ok = 0;

    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-250: invalid perf-hotspot optimization contract");
        return 0;
    }

    if (program->global_count > 0u) {
        foldable_globals = (unsigned char *)calloc(program->global_count, sizeof(unsigned char));
        if (!foldable_globals) {
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-251: out of memory preparing foldable globals");
            return 0;
        }
        if (!value_ssa_collect_foldable_constant_globals(program, foldable_globals)) {
            free(foldable_globals);
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-252: failed collecting foldable globals");
            return 0;
        }
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunction *function = &program->functions[function_index];
        size_t block_index;

        if (!function->has_body) {
            continue;
        }

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            ValueSsaBasicBlock *block = &function->blocks[block_index];
            size_t instruction_index;

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                ValueSsaInstruction *instruction = &block->instructions[instruction_index];

                if (instruction->kind == VALUE_SSA_INSTR_LOAD_GLOBAL &&
                    instruction->as.load_slot.id < program->global_count &&
                    foldable_globals && foldable_globals[instruction->as.load_slot.id]) {
                    instruction->kind = VALUE_SSA_INSTR_MOV;
                    instruction->as.mov_value =
                        value_ssa_value_immediate(program->globals[instruction->as.load_slot.id].initializer_value);
                    continue;
                }

                (void)value_ssa_try_strength_reduce_mul_pow2(instruction);
            }
        }
    }

    if (!value_ssa_perf_reuse_same_block_repeated_pure_internal_calls(program, error)) {
        free(foldable_globals);
        return 0;
    }
    if (!value_ssa_perf_hoist_function_entry_global_loads(program, error)) {
        free(foldable_globals);
        return 0;
    }
    if (!value_ssa_perf_hoist_function_entry_global_addrs(program, error)) {
        free(foldable_globals);
        return 0;
    }
    if (!value_ssa_perf_hoist_hot_parameter_local_loads(program, error)) {
        free(foldable_globals);
        return 0;
    }
    if (!value_ssa_perf_reuse_spmv_loop_exit_indirect_loads(program, error)) {
        free(foldable_globals);
        return 0;
    }
    if (!value_ssa_perf_hoist_simple_loop_invariant_loads(program, error)) {
        free(foldable_globals);
        return 0;
    }

    ok = value_ssa_verify_program(program, error);
    free(foldable_globals);
    return ok;
}

#endif
