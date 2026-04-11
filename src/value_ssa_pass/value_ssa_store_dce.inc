#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_SPLIT_AGGREGATOR)
#define VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa_pass.c"
#else

typedef struct {
    ValueSsaFunction *function;
    const ValueSsaCfgAnalysis *analysis;
    ValueSsaRenameState *state;
} ValueSsaLocalRedundantStoreContext;

typedef struct {
    const ValueSsaProgram *program;
    ValueSsaFunction *function;
    const ValueSsaCfgAnalysis *analysis;
    ValueSsaRenameState *state;
} ValueSsaGlobalRedundantStoreContext;

typedef struct {
    size_t block_id;
    size_t instruction_index;
    unsigned char valid;
} ValueSsaPendingStoreSite;

static int value_ssa_compact_removed_instructions(ValueSsaBasicBlock *block,
    const unsigned char *remove_flags) {
    size_t read_index;
    size_t write_index = 0;

    if (!block || !remove_flags) {
        return 0;
    }

    for (read_index = 0; read_index < block->instruction_count; ++read_index) {
        if (remove_flags[read_index]) {
            value_ssa_instruction_free(&block->instructions[read_index]);
            continue;
        }

        if (write_index != read_index) {
            block->instructions[write_index] = block->instructions[read_index];
        }
        ++write_index;
    }

    block->instruction_count = write_index;
    return 1;
}

static int value_ssa_eliminate_redundant_local_stores_enter_block(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *analysis,
    size_t block_id,
    void *user_data,
    ValueSsaError *error) {
    ValueSsaLocalRedundantStoreContext *context = (ValueSsaLocalRedundantStoreContext *)user_data;
    ValueSsaBasicBlock *block;
    unsigned char *remove_flags = NULL;
    size_t instruction_index;
    size_t local_index;

    (void)function;

    if (!context || !context->function || !context->analysis || !context->state) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-230: invalid redundant-local-store contract");
        return 0;
    }

    if (!value_ssa_rename_state_begin_scope(context->state, error)) {
        return 0;
    }

    if (!value_ssa_pass_has_single_idom_entry(context->function, analysis, block_id)) {
        for (local_index = 0; local_index < context->function->local_count; ++local_index) {
            if (!value_ssa_rename_state_clear(context->state, local_index, error)) {
                return 0;
            }
        }
    }

    block = &context->function->blocks[block_id];
    if (block->instruction_count == 0) {
        return 1;
    }

    remove_flags = (unsigned char *)calloc(block->instruction_count, sizeof(unsigned char));
    if (!remove_flags) {
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-231: out of memory while eliminating redundant local stores in function '%s'",
            context->function->name ? context->function->name : "<anon>");
        return 0;
    }

    for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
        ValueSsaInstruction *instruction = &block->instructions[instruction_index];

        switch (instruction->kind) {
        case VALUE_SSA_INSTR_STORE_LOCAL:
            if (instruction->as.store.slot.id < context->function->local_count) {
                ValueSsaValueRef known_value;
                size_t slot_id = instruction->as.store.slot.id;

                if (context->state->has_current_value[slot_id]) {
                    if (!value_ssa_rename_state_lookup(context->state, slot_id, &known_value, error)) {
                        free(remove_flags);
                        return 0;
                    }
                    if (value_ssa_value_refs_equal(known_value, instruction->as.store.value)) {
                        remove_flags[instruction_index] = 1;
                        break;
                    }
                }

                if (!value_ssa_rename_state_bind(context->state,
                        slot_id,
                        instruction->as.store.value,
                        error)) {
                    free(remove_flags);
                    return 0;
                }
            }
            break;
        case VALUE_SSA_INSTR_LOAD_LOCAL:
            if (instruction->as.load_slot.id < context->function->local_count &&
                instruction->result.kind == VALUE_SSA_VALUE_ID &&
                !value_ssa_rename_state_bind(context->state,
                    instruction->as.load_slot.id,
                    instruction->result,
                    error)) {
                free(remove_flags);
                return 0;
            }
            break;
        default:
            break;
        }
    }

    if (!value_ssa_compact_removed_instructions(block, remove_flags)) {
        free(remove_flags);
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-232: failed to compact redundant local-store cleanup results");
        return 0;
    }

    free(remove_flags);
    return 1;
}

static int value_ssa_eliminate_redundant_local_stores_leave_block(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *analysis,
    size_t block_id,
    void *user_data,
    ValueSsaError *error) {
    ValueSsaLocalRedundantStoreContext *context = (ValueSsaLocalRedundantStoreContext *)user_data;

    (void)function;
    (void)analysis;
    (void)block_id;

    if (!context || !context->state) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-230: invalid redundant-local-store contract");
        return 0;
    }

    return value_ssa_rename_state_end_scope(context->state, error);
}

static int value_ssa_eliminate_redundant_global_stores_enter_block(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *analysis,
    size_t block_id,
    void *user_data,
    ValueSsaError *error) {
    ValueSsaGlobalRedundantStoreContext *context = (ValueSsaGlobalRedundantStoreContext *)user_data;
    ValueSsaBasicBlock *block;
    unsigned char *remove_flags = NULL;
    size_t instruction_index;
    size_t global_index;

    (void)function;

    if (!context || !context->program || !context->function || !context->analysis || !context->state) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-233: invalid redundant-global-store contract");
        return 0;
    }

    if (!value_ssa_rename_state_begin_scope(context->state, error)) {
        return 0;
    }

    if (!value_ssa_pass_has_single_idom_entry(context->function, analysis, block_id)) {
        for (global_index = 0; global_index < context->program->global_count; ++global_index) {
            if (!value_ssa_rename_state_clear(context->state, global_index, error)) {
                return 0;
            }
        }
    }

    block = &context->function->blocks[block_id];
    if (block->instruction_count == 0) {
        return 1;
    }

    remove_flags = (unsigned char *)calloc(block->instruction_count, sizeof(unsigned char));
    if (!remove_flags) {
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-234: out of memory while eliminating redundant global stores in function '%s'",
            context->function->name ? context->function->name : "<anon>");
        return 0;
    }

    for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
        ValueSsaInstruction *instruction = &block->instructions[instruction_index];

        switch (instruction->kind) {
        case VALUE_SSA_INSTR_STORE_GLOBAL:
            if (instruction->as.store.slot.id < context->program->global_count) {
                ValueSsaValueRef known_value;
                size_t slot_id = instruction->as.store.slot.id;

                if (context->state->has_current_value[slot_id]) {
                    if (!value_ssa_rename_state_lookup(context->state, slot_id, &known_value, error)) {
                        free(remove_flags);
                        return 0;
                    }
                    if (value_ssa_value_refs_equal(known_value, instruction->as.store.value)) {
                        remove_flags[instruction_index] = 1;
                        break;
                    }
                }

                if (!value_ssa_rename_state_bind(context->state,
                        slot_id,
                        instruction->as.store.value,
                        error)) {
                    free(remove_flags);
                    return 0;
                }
            }
            break;
        case VALUE_SSA_INSTR_LOAD_GLOBAL:
            if (instruction->as.load_slot.id < context->program->global_count &&
                instruction->result.kind == VALUE_SSA_VALUE_ID &&
                !value_ssa_rename_state_bind(context->state,
                    instruction->as.load_slot.id,
                    instruction->result,
                    error)) {
                free(remove_flags);
                return 0;
            }
            break;
        case VALUE_SSA_INSTR_CALL:
            for (global_index = 0; global_index < context->program->global_count; ++global_index) {
                if (!value_ssa_rename_state_clear(context->state, global_index, error)) {
                    free(remove_flags);
                    return 0;
                }
            }
            break;
        default:
            break;
        }
    }

    if (!value_ssa_compact_removed_instructions(block, remove_flags)) {
        free(remove_flags);
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-235: failed to compact redundant global-store cleanup results");
        return 0;
    }

    free(remove_flags);
    return 1;
}

static int value_ssa_eliminate_redundant_global_stores_leave_block(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *analysis,
    size_t block_id,
    void *user_data,
    ValueSsaError *error) {
    ValueSsaGlobalRedundantStoreContext *context = (ValueSsaGlobalRedundantStoreContext *)user_data;

    (void)function;
    (void)analysis;
    (void)block_id;

    if (!context || !context->state) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-233: invalid redundant-global-store contract");
        return 0;
    }

    return value_ssa_rename_state_end_scope(context->state, error);
}

static size_t value_ssa_dead_store_get_linear_predecessor(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *analysis,
    size_t block_id) {
    size_t predecessor_id = value_ssa_pass_get_single_idom_predecessor(function, analysis, block_id);
    const ValueSsaBasicBlock *predecessor;

    if (predecessor_id == (size_t)-1) {
        return (size_t)-1;
    }

    predecessor = &function->blocks[predecessor_id];
    if (predecessor->terminator.kind != VALUE_SSA_TERM_JUMP ||
        predecessor->terminator.as.jump_target != block_id) {
        return (size_t)-1;
    }

    return predecessor_id;
}

static int value_ssa_allocate_cross_block_remove_flags(const ValueSsaFunction *function,
    unsigned char ***out_remove_flags,
    ValueSsaError *error,
    const char *oom_error_code,
    const char *what) {
    unsigned char **remove_flags = NULL;
    size_t block_index;

    if (!function || !out_remove_flags || !oom_error_code || !what) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-237: invalid cross-block store cleanup contract");
        return 0;
    }

    remove_flags = (unsigned char **)calloc(function->block_count, sizeof(unsigned char *));
    if (!remove_flags) {
        value_ssa_set_error(error,
            0,
            0,
            "%s: out of memory while preparing %s in function '%s'",
            oom_error_code,
            what,
            function->name ? function->name : "<anon>");
        return 0;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        const ValueSsaBasicBlock *block = &function->blocks[block_index];

        if (block->instruction_count == 0) {
            continue;
        }

        remove_flags[block_index] = (unsigned char *)calloc(block->instruction_count, sizeof(unsigned char));
        if (!remove_flags[block_index]) {
            size_t free_index;

            for (free_index = 0; free_index < block_index; ++free_index) {
                free(remove_flags[free_index]);
            }
            free(remove_flags);
            value_ssa_set_error(error,
                0,
                0,
                "%s: out of memory while preparing %s in function '%s'",
                oom_error_code,
                what,
                function->name ? function->name : "<anon>");
            return 0;
        }
    }

    *out_remove_flags = remove_flags;
    return 1;
}

static void value_ssa_free_cross_block_remove_flags(const ValueSsaFunction *function,
    unsigned char **remove_flags) {
    size_t block_index;

    if (!function || !remove_flags) {
        return;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        free(remove_flags[block_index]);
    }
    free(remove_flags);
}

static int value_ssa_compact_cross_block_removed_instructions(ValueSsaFunction *function,
    unsigned char **remove_flags,
    ValueSsaError *error,
    const char *compact_error_code,
    const char *what) {
    size_t block_index;

    if (!function || !remove_flags || !compact_error_code || !what) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-237: invalid cross-block store cleanup contract");
        return 0;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        if (!remove_flags[block_index]) {
            continue;
        }
        if (!value_ssa_compact_removed_instructions(&function->blocks[block_index], remove_flags[block_index])) {
            value_ssa_set_error(error, 0, 0, "%s: failed to compact %s results", compact_error_code, what);
            return 0;
        }
    }

    return 1;
}

static int value_ssa_eliminate_dead_local_stores_across_straight_chain(ValueSsaFunction *function,
    ValueSsaError *error) {
    ValueSsaCfgAnalysis analysis;
    size_t *preorder = NULL;
    size_t preorder_count = 0;
    ValueSsaPendingStoreSite *pending_sites = NULL;
    unsigned char **remove_flags = NULL;
    size_t order_index;
    int ok = 0;

    if (!function) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-238: invalid cross-block local store DCE contract");
        return 0;
    }
    if (function->local_count == 0 || function->block_count == 0) {
        return 1;
    }

    value_ssa_cfg_analysis_init(&analysis);
    if (!value_ssa_compute_cfg_analysis(function, &analysis, error)) {
        value_ssa_cfg_analysis_free(&analysis);
        return 0;
    }

    preorder = (size_t *)malloc(function->block_count * sizeof(size_t));
    pending_sites = (ValueSsaPendingStoreSite *)calloc(function->block_count * function->local_count,
        sizeof(ValueSsaPendingStoreSite));
    if (!preorder || !pending_sites ||
        !value_ssa_allocate_cross_block_remove_flags(function,
            &remove_flags,
            error,
            "VALUE-SSA-239",
            "cross-block local store DCE remove flags") ||
        !value_ssa_compute_dominator_tree_preorder(function, &analysis, preorder, &preorder_count, error)) {
        goto cleanup_local_chain;
    }

    for (order_index = 0; order_index < preorder_count; ++order_index) {
        size_t block_id = preorder[order_index];
        size_t predecessor_id = value_ssa_dead_store_get_linear_predecessor(function, &analysis, block_id);
        ValueSsaPendingStoreSite *state_row = &pending_sites[block_id * function->local_count];
        ValueSsaBasicBlock *block = &function->blocks[block_id];
        size_t instruction_index;

        if (predecessor_id != (size_t)-1) {
            memcpy(state_row,
                &pending_sites[predecessor_id * function->local_count],
                function->local_count * sizeof(ValueSsaPendingStoreSite));
        } else {
            memset(state_row, 0, function->local_count * sizeof(ValueSsaPendingStoreSite));
        }

        for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
            ValueSsaInstruction *instruction = &block->instructions[instruction_index];

            switch (instruction->kind) {
            case VALUE_SSA_INSTR_LOAD_LOCAL:
                if (instruction->as.load_slot.id < function->local_count) {
                    state_row[instruction->as.load_slot.id].valid = 0;
                }
                break;
            case VALUE_SSA_INSTR_STORE_LOCAL:
                if (instruction->as.store.slot.id < function->local_count) {
                    ValueSsaPendingStoreSite *site = &state_row[instruction->as.store.slot.id];

                    if (site->valid) {
                        remove_flags[site->block_id][site->instruction_index] = 1;
                    }
                    site->valid = 1;
                    site->block_id = block_id;
                    site->instruction_index = instruction_index;
                }
                break;
            default:
                break;
            }
        }
    }

    ok = value_ssa_compact_cross_block_removed_instructions(function,
        remove_flags,
        error,
        "VALUE-SSA-240",
        "cross-block local store DCE");

cleanup_local_chain:
    value_ssa_free_cross_block_remove_flags(function, remove_flags);
    free(pending_sites);
    free(preorder);
    value_ssa_cfg_analysis_free(&analysis);
    return ok;
}

static int value_ssa_eliminate_dead_global_stores_across_straight_chain(const ValueSsaProgram *program,
    ValueSsaFunction *function,
    ValueSsaError *error) {
    ValueSsaCfgAnalysis analysis;
    size_t *preorder = NULL;
    size_t preorder_count = 0;
    ValueSsaPendingStoreSite *pending_sites = NULL;
    unsigned char **remove_flags = NULL;
    size_t order_index;
    int ok = 0;

    if (!program || !function) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-241: invalid cross-block global store DCE contract");
        return 0;
    }
    if (program->global_count == 0 || function->block_count == 0) {
        return 1;
    }

    value_ssa_cfg_analysis_init(&analysis);
    if (!value_ssa_compute_cfg_analysis(function, &analysis, error)) {
        value_ssa_cfg_analysis_free(&analysis);
        return 0;
    }

    preorder = (size_t *)malloc(function->block_count * sizeof(size_t));
    pending_sites = (ValueSsaPendingStoreSite *)calloc(function->block_count * program->global_count,
        sizeof(ValueSsaPendingStoreSite));
    if (!preorder || !pending_sites ||
        !value_ssa_allocate_cross_block_remove_flags(function,
            &remove_flags,
            error,
            "VALUE-SSA-242",
            "cross-block global store DCE remove flags") ||
        !value_ssa_compute_dominator_tree_preorder(function, &analysis, preorder, &preorder_count, error)) {
        goto cleanup_global_chain;
    }

    for (order_index = 0; order_index < preorder_count; ++order_index) {
        size_t block_id = preorder[order_index];
        size_t predecessor_id = value_ssa_dead_store_get_linear_predecessor(function, &analysis, block_id);
        ValueSsaPendingStoreSite *state_row = &pending_sites[block_id * program->global_count];
        ValueSsaBasicBlock *block = &function->blocks[block_id];
        size_t instruction_index;

        if (predecessor_id != (size_t)-1) {
            memcpy(state_row,
                &pending_sites[predecessor_id * program->global_count],
                program->global_count * sizeof(ValueSsaPendingStoreSite));
        } else {
            memset(state_row, 0, program->global_count * sizeof(ValueSsaPendingStoreSite));
        }

        for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
            ValueSsaInstruction *instruction = &block->instructions[instruction_index];

            switch (instruction->kind) {
            case VALUE_SSA_INSTR_LOAD_GLOBAL:
                if (instruction->as.load_slot.id < program->global_count) {
                    state_row[instruction->as.load_slot.id].valid = 0;
                }
                break;
            case VALUE_SSA_INSTR_STORE_GLOBAL:
                if (instruction->as.store.slot.id < program->global_count) {
                    ValueSsaPendingStoreSite *site = &state_row[instruction->as.store.slot.id];

                    if (site->valid) {
                        remove_flags[site->block_id][site->instruction_index] = 1;
                    }
                    site->valid = 1;
                    site->block_id = block_id;
                    site->instruction_index = instruction_index;
                }
                break;
            case VALUE_SSA_INSTR_CALL:
                memset(state_row, 0, program->global_count * sizeof(ValueSsaPendingStoreSite));
                break;
            default:
                break;
            }
        }
    }

    ok = value_ssa_compact_cross_block_removed_instructions(function,
        remove_flags,
        error,
        "VALUE-SSA-243",
        "cross-block global store DCE");

cleanup_global_chain:
    value_ssa_free_cross_block_remove_flags(function, remove_flags);
    free(pending_sites);
    free(preorder);
    value_ssa_cfg_analysis_free(&analysis);
    return ok;
}

static int value_ssa_eliminate_dead_local_stores_in_block(ValueSsaFunction *function,
    ValueSsaBasicBlock *block,
    ValueSsaError *error) {
    unsigned char *slot_observed = NULL;
    unsigned char *remove_flags = NULL;
    size_t instruction_index;
    size_t local_index;

    if (!function || !block) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-223: invalid local store DCE contract");
        return 0;
    }
    if (function->local_count == 0 || block->instruction_count == 0) {
        return 1;
    }

    slot_observed = (unsigned char *)calloc(function->local_count, sizeof(unsigned char));
    remove_flags = (unsigned char *)calloc(block->instruction_count, sizeof(unsigned char));
    if (!slot_observed || !remove_flags) {
        free(slot_observed);
        free(remove_flags);
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-224: out of memory while eliminating dead local stores in function '%s'",
            function->name ? function->name : "<anon>");
        return 0;
    }

    if (block->terminator.kind != VALUE_SSA_TERM_RETURN) {
        for (local_index = 0; local_index < function->local_count; ++local_index) {
            slot_observed[local_index] = 1;
        }
    }

    for (instruction_index = block->instruction_count; instruction_index > 0; --instruction_index) {
        ValueSsaInstruction *instruction = &block->instructions[instruction_index - 1];

        switch (instruction->kind) {
        case VALUE_SSA_INSTR_LOAD_LOCAL:
            if (instruction->as.load_slot.id < function->local_count) {
                slot_observed[instruction->as.load_slot.id] = 1;
            }
            break;
        case VALUE_SSA_INSTR_STORE_LOCAL:
            if (instruction->as.store.slot.id < function->local_count) {
                size_t slot_id = instruction->as.store.slot.id;

                if (!slot_observed[slot_id]) {
                    remove_flags[instruction_index - 1] = 1;
                } else {
                    slot_observed[slot_id] = 0;
                }
            }
            break;
        default:
            break;
        }
    }

    if (!value_ssa_compact_removed_instructions(block, remove_flags)) {
        free(slot_observed);
        free(remove_flags);
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-225: failed to compact local store DCE results");
        return 0;
    }

    free(slot_observed);
    free(remove_flags);
    return 1;
}

static int value_ssa_eliminate_overwritten_global_stores_in_block(const ValueSsaProgram *program,
    ValueSsaFunction *function,
    ValueSsaBasicBlock *block,
    ValueSsaError *error) {
    size_t *last_store_index = NULL;
    unsigned char *remove_flags = NULL;
    size_t instruction_index;
    size_t global_index;

    if (!program || !function || !block) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-226: invalid global store DCE contract");
        return 0;
    }
    if (program->global_count == 0 || block->instruction_count == 0) {
        return 1;
    }

    last_store_index = (size_t *)malloc(program->global_count * sizeof(size_t));
    remove_flags = (unsigned char *)calloc(block->instruction_count, sizeof(unsigned char));
    if (!last_store_index || !remove_flags) {
        free(last_store_index);
        free(remove_flags);
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-227: out of memory while eliminating overwritten global stores in function '%s'",
            function->name ? function->name : "<anon>");
        return 0;
    }

    for (global_index = 0; global_index < program->global_count; ++global_index) {
        last_store_index[global_index] = (size_t)-1;
    }

    for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
        ValueSsaInstruction *instruction = &block->instructions[instruction_index];

        switch (instruction->kind) {
        case VALUE_SSA_INSTR_STORE_GLOBAL:
            if (instruction->as.store.slot.id < program->global_count) {
                size_t slot_id = instruction->as.store.slot.id;

                if (last_store_index[slot_id] != (size_t)-1) {
                    remove_flags[last_store_index[slot_id]] = 1;
                }
                last_store_index[slot_id] = instruction_index;
            }
            break;
        case VALUE_SSA_INSTR_LOAD_GLOBAL:
            if (instruction->as.load_slot.id < program->global_count) {
                last_store_index[instruction->as.load_slot.id] = (size_t)-1;
            }
            break;
        case VALUE_SSA_INSTR_CALL:
            for (global_index = 0; global_index < program->global_count; ++global_index) {
                last_store_index[global_index] = (size_t)-1;
            }
            break;
        default:
            break;
        }
    }

    if (!value_ssa_compact_removed_instructions(block, remove_flags)) {
        free(last_store_index);
        free(remove_flags);
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-228: failed to compact overwritten global store DCE results");
        return 0;
    }

    free(last_store_index);
    free(remove_flags);
    return 1;
}

int value_ssa_eliminate_redundant_stores(ValueSsaProgram *program, ValueSsaError *error) {
    size_t function_index;

    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-236: invalid redundant-store contract");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunction *function = &program->functions[function_index];
        ValueSsaCfgAnalysis analysis;
        ValueSsaRenameState local_state;
        ValueSsaRenameState global_state;
        ValueSsaLocalRedundantStoreContext local_context;
        ValueSsaGlobalRedundantStoreContext global_context;
        int ok = 1;

        if (!function->has_body || (function->local_count == 0 && program->global_count == 0)) {
            continue;
        }

        value_ssa_cfg_analysis_init(&analysis);
        value_ssa_rename_state_init(&local_state);
        value_ssa_rename_state_init(&global_state);

        ok = value_ssa_compute_cfg_analysis(function, &analysis, error);

        if (ok && function->local_count > 0) {
            local_context.function = function;
            local_context.analysis = &analysis;
            local_context.state = &local_state;
            ok = value_ssa_rename_state_prepare(&local_state, function->local_count, error) &&
                value_ssa_walk_dominator_tree(function,
                    &analysis,
                    value_ssa_eliminate_redundant_local_stores_enter_block,
                    value_ssa_eliminate_redundant_local_stores_leave_block,
                    &local_context,
                    error);
        }

        if (ok && program->global_count > 0) {
            global_context.program = program;
            global_context.function = function;
            global_context.analysis = &analysis;
            global_context.state = &global_state;
            ok = value_ssa_rename_state_prepare(&global_state, program->global_count, error) &&
                value_ssa_walk_dominator_tree(function,
                    &analysis,
                    value_ssa_eliminate_redundant_global_stores_enter_block,
                    value_ssa_eliminate_redundant_global_stores_leave_block,
                    &global_context,
                    error);
        }

        value_ssa_rename_state_free(&global_state);
        value_ssa_rename_state_free(&local_state);
        value_ssa_cfg_analysis_free(&analysis);
        if (!ok) {
            return 0;
        }
    }

    return value_ssa_verify_program(program, error);
}

int value_ssa_eliminate_dead_stores(ValueSsaProgram *program, ValueSsaError *error) {
    size_t function_index;

    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-229: invalid dead-store contract");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunction *function = &program->functions[function_index];
        size_t block_index;

        if (!function->has_body) {
            continue;
        }

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            if (!value_ssa_eliminate_dead_local_stores_in_block(function, &function->blocks[block_index], error) ||
                !value_ssa_eliminate_overwritten_global_stores_in_block(program,
                    function,
                    &function->blocks[block_index],
                    error)) {
                return 0;
            }
        }

        if (!value_ssa_eliminate_dead_local_stores_across_straight_chain(function, error) ||
            !value_ssa_eliminate_dead_global_stores_across_straight_chain(program, function, error)) {
            return 0;
        }
    }

    return value_ssa_verify_program(program, error);
}

#endif
