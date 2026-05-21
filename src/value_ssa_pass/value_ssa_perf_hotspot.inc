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

    if (!value_ssa_recursive_helper_reuse_same_block_repeated_pure_internal_calls(program, error)) {
        free(foldable_globals);
        return 0;
    }
    if (!value_ssa_recursive_helper_strength_reduce_divmod2(program, error)) {
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
    if (!value_ssa_perf_hoist_shared_branch_successor_local_loads(program, error)) {
        free(foldable_globals);
        return 0;
    }
    if (!value_ssa_perf_hoist_hot_parameter_local_loads(program, error)) {
        free(foldable_globals);
        return 0;
    }
    if (!value_ssa_perf_hoist_simple_loop_invariant_loads(program, error)) {
        free(foldable_globals);
        return 0;
    }
    if (!value_ssa_perf_reuse_spmv_loop_exit_indirect_loads(program, error)) {
        free(foldable_globals);
        return 0;
    }
    if (!value_ssa_perf_fuse_spmv_repeated_j_loops(program, error)) {
        free(foldable_globals);
        return 0;
    }
    if (!value_ssa_perf_rebalance_integer_dispatch_chains(program, error)) {
        free(foldable_globals);
        return 0;
    }
    if (!value_ssa_perf_strength_reduce_simple_induction_address_exprs(program, error)) {
        free(foldable_globals);
        return 0;
    }
    if (!value_ssa_perf_strength_reduce_simple_induction_shifts(program, error)) {
        free(foldable_globals);
        return 0;
    }
    if (!value_ssa_perf_strength_reduce_non_negative_induction_divmods(program, error)) {
        free(foldable_globals);
        return 0;
    }

    ok = value_ssa_verify_program(program, error);
    free(foldable_globals);
    return ok;
}

#endif
