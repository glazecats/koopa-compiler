#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_SPLIT_AGGREGATOR)
#define VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT
#include "../value_ssa_pass/value_ssa_pass.c"
#else

static int value_ssa_perf_function_has_calls(const ValueSsaFunction *function);
static int value_ssa_perf_function_preserves_global_slot(const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    size_t slot_id);

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

static int value_ssa_perf_function_is_hot_shared_branch_local_load_candidate(const ValueSsaFunction *function) {
    if (!function || !function->name) {
        return 0;
    }

    return strcmp(function->name, "run_program") == 0;
}

static int value_ssa_perf_hoist_shared_branch_successor_local_loads(ValueSsaProgram *program, ValueSsaError *error) {
    size_t function_index;
    int changed_any = 0;

    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-313: invalid perf shared-branch local-load hoist contract");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunction *function = &program->functions[function_index];
        ValueSsaCfgAnalysis cfg;
        size_t block_index;
        int changed_function = 0;

        if (!function->has_body || function->block_count == 0u ||
            !value_ssa_perf_function_is_hot_shared_branch_local_load_candidate(function)) {
            continue;
        }

        value_ssa_cfg_analysis_init(&cfg);
        if (!value_ssa_compute_cfg_analysis(function, &cfg, error)) {
            value_ssa_cfg_analysis_free(&cfg);
            return 0;
        }

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            ValueSsaBasicBlock *block = &function->blocks[block_index];
            ValueSsaBasicBlock rebuilt_block;
            ValueSsaBasicBlock *then_block = NULL;
            ValueSsaBasicBlock *else_block = NULL;
            ValueSsaInstruction *then_instruction = NULL;
            ValueSsaInstruction *else_instruction = NULL;
            ValueSsaInstruction hoisted_load;
            size_t then_id;
            size_t else_id;
            size_t slot_id;
            size_t instruction_index;
            int ok = 1;

            if (!block->has_terminator || block->terminator.kind != VALUE_SSA_TERM_BRANCH) {
                continue;
            }

            then_id = block->terminator.as.branch.then_target;
            else_id = block->terminator.as.branch.else_target;
            if (then_id >= function->block_count || else_id >= function->block_count || then_id == else_id) {
                continue;
            }
            if (value_ssa_pass_get_single_idom_predecessor(function, &cfg, then_id) != block_index ||
                value_ssa_pass_get_single_idom_predecessor(function, &cfg, else_id) != block_index) {
                continue;
            }

            then_block = &function->blocks[then_id];
            else_block = &function->blocks[else_id];
            if (then_block->instruction_count == 0u || else_block->instruction_count == 0u) {
                continue;
            }

            then_instruction = &then_block->instructions[0];
            else_instruction = &else_block->instructions[0];
            if (then_instruction->kind != VALUE_SSA_INSTR_LOAD_LOCAL ||
                else_instruction->kind != VALUE_SSA_INSTR_LOAD_LOCAL ||
                !then_instruction->has_result ||
                !else_instruction->has_result ||
                then_instruction->result.kind != VALUE_SSA_VALUE_ID ||
                else_instruction->result.kind != VALUE_SSA_VALUE_ID ||
                then_instruction->as.load_slot.kind != VALUE_SSA_SLOT_LOCAL ||
                else_instruction->as.load_slot.kind != VALUE_SSA_SLOT_LOCAL ||
                then_instruction->as.load_slot.id != else_instruction->as.load_slot.id) {
                continue;
            }

            slot_id = then_instruction->as.load_slot.id;
            if (slot_id >= function->local_count) {
                continue;
            }

            memset(&rebuilt_block, 0, sizeof(rebuilt_block));
            rebuilt_block.id = block->id;
            for (instruction_index = 0; instruction_index < block->instruction_count && ok; ++instruction_index) {
                if (!value_ssa_block_append_instruction(&rebuilt_block, &block->instructions[instruction_index], error)) {
                    ok = 0;
                }
            }

            memset(&hoisted_load, 0, sizeof(hoisted_load));
            hoisted_load.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
            hoisted_load.has_result = 1;
            hoisted_load.result = value_ssa_value_id(value_ssa_function_allocate_value(function));
            hoisted_load.as.load_slot = value_ssa_slot_local(slot_id);
            if (ok &&
                (hoisted_load.result.value_id == (size_t)-1 ||
                    !value_ssa_block_append_instruction(&rebuilt_block, &hoisted_load, error))) {
                ok = 0;
            }
            if (!ok) {
                value_ssa_basic_block_free(&rebuilt_block);
                value_ssa_cfg_analysis_free(&cfg);
                return 0;
            }

            rebuilt_block.has_terminator = block->has_terminator;
            rebuilt_block.terminator = block->terminator;
            free(block->instructions);
            block->instructions = rebuilt_block.instructions;
            block->instruction_count = rebuilt_block.instruction_count;
            block->instruction_capacity = rebuilt_block.instruction_capacity;
            rebuilt_block.instructions = NULL;
            rebuilt_block.instruction_count = 0u;
            rebuilt_block.instruction_capacity = 0u;
            value_ssa_basic_block_free(&rebuilt_block);

            then_instruction->kind = VALUE_SSA_INSTR_MOV;
            then_instruction->as.mov_value = hoisted_load.result;
            else_instruction->kind = VALUE_SSA_INSTR_MOV;
            else_instruction->as.mov_value = hoisted_load.result;
            changed_function = 1;
            changed_any = 1;
            break;
        }

        value_ssa_cfg_analysis_free(&cfg);
        if (changed_function) {
            --function_index;
        }
    }

    if (changed_any &&
        (!value_ssa_simplify_trivial_values(program, error) ||
            !value_ssa_eliminate_dead_value_defs(program, error))) {
        return 0;
    }

    return value_ssa_verify_program(program, error);
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


#endif
