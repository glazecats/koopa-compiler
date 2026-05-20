#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_SPLIT_AGGREGATOR)
#define VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa_pass.c"
#else

typedef enum {
    VALUE_SSA_AVAILABLE_VALUE_BINARY = 0,
    VALUE_SSA_AVAILABLE_VALUE_MOV,
    VALUE_SSA_AVAILABLE_VALUE_ADDR_LOCAL,
    VALUE_SSA_AVAILABLE_VALUE_ADDR_GLOBAL,
    VALUE_SSA_AVAILABLE_VALUE_LOAD_LOCAL,
    VALUE_SSA_AVAILABLE_VALUE_LOAD_GLOBAL,
    VALUE_SSA_AVAILABLE_VALUE_LOAD_INDIRECT,
} ValueSsaAvailableValueKind;

typedef struct {
    ValueSsaAvailableValueKind kind;
    ValueSsaBinaryOp op;
    ValueSsaValueRef lhs;
    ValueSsaValueRef rhs;
    ValueSsaValueRef mov_value;
    size_t slot_id;
    ValueSsaValueRef result;
} ValueSsaAvailableBinary;

typedef struct {
    ValueSsaAvailableBinary *entries;
    size_t entry_count;
    size_t entry_capacity;
} ValueSsaAvailableValueSet;

typedef struct {
    ValueSsaAvailableBinary key;
    size_t phi_index;
    ValueSsaValueRef result;
} ValueSsaAvailableMergePhi;

typedef struct {
    ValueSsaAvailableValueSet entry_values;
    ValueSsaAvailableValueSet exit_values;
    ValueSsaAvailableMergePhi *merge_phis;
    size_t merge_phi_count;
    size_t merge_phi_capacity;
} ValueSsaAvailableValueBlockState;

typedef struct {
    const ValueSsaProgram *program;
    ValueSsaFunction *function;
    ValueSsaDefUseAnalysis def_use;
    unsigned char *readonly_globals;
    unsigned char *address_taken_locals;
    ValueSsaAvailableValueBlockState *block_states;
} ValueSsaRedundantBinaryContext;

static void value_ssa_available_value_set_init(ValueSsaAvailableValueSet *set) {
    if (!set) {
        return;
    }

    set->entries = NULL;
    set->entry_count = 0;
    set->entry_capacity = 0;
}

static void value_ssa_available_value_set_free(ValueSsaAvailableValueSet *set) {
    if (!set) {
        return;
    }

    free(set->entries);
    value_ssa_available_value_set_init(set);
}

static void value_ssa_available_value_block_state_init(ValueSsaAvailableValueBlockState *state) {
    if (!state) {
        return;
    }

    value_ssa_available_value_set_init(&state->entry_values);
    value_ssa_available_value_set_init(&state->exit_values);
    state->merge_phis = NULL;
    state->merge_phi_count = 0;
    state->merge_phi_capacity = 0;
}

static void value_ssa_available_value_block_state_free(ValueSsaAvailableValueBlockState *state) {
    if (!state) {
        return;
    }

    value_ssa_available_value_set_free(&state->entry_values);
    value_ssa_available_value_set_free(&state->exit_values);
    free(state->merge_phis);
    state->merge_phis = NULL;
    state->merge_phi_count = 0;
    state->merge_phi_capacity = 0;
}

static void value_ssa_redundant_binary_context_init(ValueSsaRedundantBinaryContext *context) {
    if (!context) {
        return;
    }

    context->program = NULL;
    context->function = NULL;
    value_ssa_def_use_analysis_init(&context->def_use);
    context->readonly_globals = NULL;
    context->address_taken_locals = NULL;
    context->block_states = NULL;
}

static void value_ssa_redundant_binary_context_free(ValueSsaRedundantBinaryContext *context) {
    size_t block_index;

    if (!context) {
        return;
    }

    if (context->block_states && context->function) {
        for (block_index = 0; block_index < context->function->block_count; ++block_index) {
            value_ssa_available_value_block_state_free(&context->block_states[block_index]);
        }
    }

    free(context->block_states);
    free(context->readonly_globals);
    free(context->address_taken_locals);
    value_ssa_def_use_analysis_free(&context->def_use);
    value_ssa_redundant_binary_context_init(context);
}

static int value_ssa_available_value_key_equal(const ValueSsaRedundantBinaryContext *context,
    const ValueSsaAvailableBinary *lhs,
    const ValueSsaAvailableBinary *rhs) {
    if (!lhs || !rhs || lhs->kind != rhs->kind || lhs->op != rhs->op || lhs->slot_id != rhs->slot_id) {
        return 0;
    }

    switch (lhs->kind) {
    case VALUE_SSA_AVAILABLE_VALUE_BINARY:
        return value_ssa_value_refs_equal(lhs->lhs, rhs->lhs) &&
            value_ssa_value_refs_equal(lhs->rhs, rhs->rhs);
    case VALUE_SSA_AVAILABLE_VALUE_MOV:
        return value_ssa_value_refs_equal(lhs->mov_value, rhs->mov_value);
    case VALUE_SSA_AVAILABLE_VALUE_ADDR_LOCAL:
    case VALUE_SSA_AVAILABLE_VALUE_ADDR_GLOBAL:
    case VALUE_SSA_AVAILABLE_VALUE_LOAD_LOCAL:
    case VALUE_SSA_AVAILABLE_VALUE_LOAD_GLOBAL:
        return 1;
    case VALUE_SSA_AVAILABLE_VALUE_LOAD_INDIRECT:
        if (value_ssa_value_refs_equal(lhs->lhs, rhs->lhs)) {
            return 1;
        }
        if (!context || !context->function) {
            return 0;
        }
        return value_ssa_pass_same_pure_address_value(
            context->function, &context->def_use, lhs->lhs, rhs->lhs, 0u);
    default:
        return 0;
    }
}

static void value_ssa_available_value_fill(ValueSsaAvailableBinary *entry,
    ValueSsaAvailableValueKind kind,
    ValueSsaBinaryOp op,
    ValueSsaValueRef lhs,
    ValueSsaValueRef rhs,
    ValueSsaValueRef mov_value,
    size_t slot_id,
    ValueSsaValueRef result) {
    if (!entry) {
        return;
    }

    entry->kind = kind;
    entry->op = op;
    entry->lhs = lhs;
    entry->rhs = rhs;
    entry->mov_value = mov_value;
    entry->slot_id = slot_id;
    entry->result = result;
}

static int value_ssa_available_value_set_reserve(ValueSsaAvailableValueSet *set,
    size_t minimum_capacity,
    ValueSsaError *error) {
    size_t next_capacity;
    ValueSsaAvailableBinary *new_entries;

    if (!set) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-211: invalid available-value reserve contract");
        return 0;
    }
    if (set->entry_capacity >= minimum_capacity) {
        return 1;
    }

    next_capacity = set->entry_capacity;
    while (next_capacity < minimum_capacity) {
        if (!value_ssa_next_growth_capacity(next_capacity,
                16,
                sizeof(ValueSsaAvailableBinary),
                &next_capacity)) {
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-212: available-value capacity overflow");
            return 0;
        }
    }

    new_entries = (ValueSsaAvailableBinary *)realloc(set->entries, next_capacity * sizeof(ValueSsaAvailableBinary));
    if (!new_entries) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-213: out of memory growing available-value table");
        return 0;
    }

    set->entries = new_entries;
    set->entry_capacity = next_capacity;
    return 1;
}

static int value_ssa_available_value_set_append(ValueSsaAvailableValueSet *set,
    const ValueSsaAvailableBinary *entry,
    ValueSsaError *error) {
    if (!set || !entry) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-214: invalid available-value append contract");
        return 0;
    }

    if (!value_ssa_available_value_set_reserve(set, set->entry_count + 1u, error)) {
        return 0;
    }

    set->entries[set->entry_count++] = *entry;
    return 1;
}

static int value_ssa_available_value_set_lookup_entry(const ValueSsaAvailableValueSet *set,
    const ValueSsaRedundantBinaryContext *context,
    const ValueSsaAvailableBinary *needle,
    ValueSsaValueRef *out_result) {
    size_t index;

    if (!set || !needle) {
        return 0;
    }

    for (index = set->entry_count; index > 0; --index) {
        const ValueSsaAvailableBinary *entry = &set->entries[index - 1u];

        if (value_ssa_available_value_key_equal(context, entry, needle)) {
            if (out_result) {
                *out_result = entry->result;
            }
            return 1;
        }
    }

    return 0;
}

static int value_ssa_available_value_set_copy_from(ValueSsaAvailableValueSet *dst,
    const ValueSsaAvailableValueSet *src,
    ValueSsaError *error) {
    if (!dst || !src) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-215: invalid available-value copy contract");
        return 0;
    }

    if (!value_ssa_available_value_set_reserve(dst, src->entry_count, error)) {
        return 0;
    }

    if (src->entry_count > 0u) {
        memcpy(dst->entries, src->entries, src->entry_count * sizeof(ValueSsaAvailableBinary));
    }
    dst->entry_count = src->entry_count;
    return 1;
}

static int value_ssa_available_value_set_equals(const ValueSsaAvailableValueSet *lhs,
    const ValueSsaRedundantBinaryContext *context,
    const ValueSsaAvailableValueSet *rhs) {
    size_t index;

    if (lhs == rhs) {
        return 1;
    }
    if (!lhs || !rhs || lhs->entry_count != rhs->entry_count) {
        return 0;
    }

    for (index = 0; index < lhs->entry_count; ++index) {
        if (!value_ssa_available_value_key_equal(context, &lhs->entries[index], &rhs->entries[index]) ||
            !value_ssa_value_refs_equal(lhs->entries[index].result, rhs->entries[index].result)) {
            return 0;
        }
    }

    return 1;
}

static int value_ssa_available_value_append_phi(ValueSsaBasicBlock *block,
    size_t result_id,
    const ValueSsaPhiInput *inputs,
    size_t input_count,
    size_t *out_phi_index,
    ValueSsaError *error) {
    size_t next_capacity;
    ValueSsaPhi *phis;
    ValueSsaPhi *phi;

    if (!block || (input_count > 0u && !inputs)) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-216: invalid available-value phi append contract");
        return 0;
    }

    if (block->phi_count == block->phi_capacity) {
        if (!value_ssa_next_growth_capacity(block->phi_capacity, 4, sizeof(ValueSsaPhi), &next_capacity)) {
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-217: available-value phi capacity overflow");
            return 0;
        }
        phis = (ValueSsaPhi *)realloc(block->phis, next_capacity * sizeof(ValueSsaPhi));
        if (!phis) {
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-218: out of memory growing available-value phi table");
            return 0;
        }
        block->phis = phis;
        block->phi_capacity = next_capacity;
    }

    phi = &block->phis[block->phi_count];
    phi->result_id = result_id;
    phi->inputs = NULL;
    phi->input_count = input_count;
    phi->input_capacity = input_count;
    if (input_count > 0u) {
        phi->inputs = (ValueSsaPhiInput *)malloc(input_count * sizeof(ValueSsaPhiInput));
        if (!phi->inputs) {
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-219: out of memory copying available-value phi inputs");
            return 0;
        }
        memcpy(phi->inputs, inputs, input_count * sizeof(ValueSsaPhiInput));
    }

    if (out_phi_index) {
        *out_phi_index = block->phi_count;
    }
    ++block->phi_count;
    return 1;
}

static int value_ssa_available_value_merge_phi_reserve(ValueSsaAvailableValueBlockState *state,
    size_t minimum_capacity,
    ValueSsaError *error) {
    size_t next_capacity;
    ValueSsaAvailableMergePhi *new_entries;

    if (!state) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-220: invalid merge-phi reserve contract");
        return 0;
    }
    if (state->merge_phi_capacity >= minimum_capacity) {
        return 1;
    }

    next_capacity = state->merge_phi_capacity;
    while (next_capacity < minimum_capacity) {
        if (!value_ssa_next_growth_capacity(next_capacity,
                4,
                sizeof(ValueSsaAvailableMergePhi),
                &next_capacity)) {
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-221: merge-phi capacity overflow");
            return 0;
        }
    }

    new_entries = (ValueSsaAvailableMergePhi *)realloc(
        state->merge_phis, next_capacity * sizeof(ValueSsaAvailableMergePhi));
    if (!new_entries) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-222: out of memory growing merge-phi table");
        return 0;
    }

    state->merge_phis = new_entries;
    state->merge_phi_capacity = next_capacity;
    return 1;
}

static int value_ssa_collect_redundant_binary_readonly_globals(const ValueSsaProgram *program,
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

static int value_ssa_available_value_build_from_instruction(const ValueSsaRedundantBinaryContext *context,
    const ValueSsaInstruction *instruction,
    ValueSsaAvailableBinary *out_entry) {
    if (!context || !instruction || !out_entry || !instruction->has_result) {
        return 0;
    }

    switch (instruction->kind) {
    case VALUE_SSA_INSTR_BINARY:
        {
            ValueSsaBinaryOp key_op = instruction->as.binary.op;
            ValueSsaValueRef key_lhs = instruction->as.binary.lhs;
            ValueSsaValueRef key_rhs = instruction->as.binary.rhs;
            ValueSsaBinaryOp swapped_op;

            if (value_ssa_should_normalize_binary_operands(key_op, key_lhs, key_rhs, &swapped_op)) {
                ValueSsaValueRef old_lhs = key_lhs;

                key_lhs = key_rhs;
                key_rhs = old_lhs;
                key_op = swapped_op;
            }
        if (!value_ssa_binary_op_is_safe_reusable_pure_binary(
                key_op,
                key_lhs,
                key_rhs)) {
            return 0;
        }
        value_ssa_available_value_fill(out_entry,
            VALUE_SSA_AVAILABLE_VALUE_BINARY,
            key_op,
            key_lhs,
            key_rhs,
            value_ssa_value_immediate(0),
            0u,
            instruction->result);
        return 1;
        }
    case VALUE_SSA_INSTR_MOV:
        value_ssa_available_value_fill(out_entry,
            VALUE_SSA_AVAILABLE_VALUE_MOV,
            0,
            value_ssa_value_immediate(0),
            value_ssa_value_immediate(0),
            instruction->as.mov_value,
            0u,
            instruction->result);
        return 1;
    case VALUE_SSA_INSTR_ADDR_LOCAL:
        value_ssa_available_value_fill(out_entry,
            VALUE_SSA_AVAILABLE_VALUE_ADDR_LOCAL,
            0,
            value_ssa_value_immediate(0),
            value_ssa_value_immediate(0),
            value_ssa_value_immediate(0),
            instruction->as.addr_slot.id,
            instruction->result);
        return 1;
    case VALUE_SSA_INSTR_ADDR_GLOBAL:
        value_ssa_available_value_fill(out_entry,
            VALUE_SSA_AVAILABLE_VALUE_ADDR_GLOBAL,
            0,
            value_ssa_value_immediate(0),
            value_ssa_value_immediate(0),
            value_ssa_value_immediate(0),
            instruction->as.addr_slot.id,
            instruction->result);
        return 1;
    case VALUE_SSA_INSTR_LOAD_LOCAL:
        if (instruction->as.load_slot.kind != VALUE_SSA_SLOT_LOCAL ||
            instruction->as.load_slot.id >= context->function->local_count ||
            (context->address_taken_locals && context->address_taken_locals[instruction->as.load_slot.id])) {
            return 0;
        }
        value_ssa_available_value_fill(out_entry,
            VALUE_SSA_AVAILABLE_VALUE_LOAD_LOCAL,
            0,
            value_ssa_value_immediate(0),
            value_ssa_value_immediate(0),
            value_ssa_value_immediate(0),
            instruction->as.load_slot.id,
            instruction->result);
        return 1;
    case VALUE_SSA_INSTR_LOAD_GLOBAL:
        if (instruction->as.load_slot.id >= context->program->global_count ||
            !context->readonly_globals[instruction->as.load_slot.id]) {
            return 0;
        }
        value_ssa_available_value_fill(out_entry,
            VALUE_SSA_AVAILABLE_VALUE_LOAD_GLOBAL,
            0,
            value_ssa_value_immediate(0),
            value_ssa_value_immediate(0),
            value_ssa_value_immediate(0),
            instruction->as.load_slot.id,
            instruction->result);
        return 1;
    case VALUE_SSA_INSTR_LOAD_INDIRECT:
        value_ssa_available_value_fill(out_entry,
            VALUE_SSA_AVAILABLE_VALUE_LOAD_INDIRECT,
            0,
            instruction->as.load_indirect_addr,
            value_ssa_value_immediate(0),
            value_ssa_value_immediate(0),
            0u,
            instruction->result);
        return 1;
    default:
        return 0;
    }
}

static int value_ssa_available_value_instruction_kills_load_indirect(
    const ValueSsaRedundantBinaryContext *context,
    const ValueSsaAvailableBinary *entry,
    const ValueSsaInstruction *instruction) {
    ValueSsaPassAddressRoot load_root;
    int has_load_root;

    if (!context || !context->function || !entry || !instruction ||
        !entry || !instruction) {
        return 0;
    }

    if (entry->kind == VALUE_SSA_AVAILABLE_VALUE_LOAD_LOCAL) {
        return instruction->kind == VALUE_SSA_INSTR_STORE_LOCAL &&
            instruction->as.store.slot.kind == VALUE_SSA_SLOT_LOCAL &&
            instruction->as.store.slot.id == entry->slot_id;
    }

    if (entry->kind != VALUE_SSA_AVAILABLE_VALUE_LOAD_INDIRECT) {
        return 0;
    }

    if (instruction->kind == VALUE_SSA_INSTR_CALL) {
        return 1;
    }

    if (value_ssa_pass_instruction_may_invalidate_address_value(
            context->function, &context->def_use, entry->lhs, instruction)) {
        return 1;
    }

    has_load_root = value_ssa_pass_extract_address_root(
        context->function, &context->def_use, entry->lhs, &load_root, 0u);
    if (has_load_root) {
        return !value_ssa_pass_store_cannot_alias_loaded_address(
            context->function, &context->def_use, &load_root, instruction);
    }

    return instruction->kind == VALUE_SSA_INSTR_STORE_LOCAL ||
        instruction->kind == VALUE_SSA_INSTR_STORE_GLOBAL ||
        instruction->kind == VALUE_SSA_INSTR_STORE_INDIRECT;
}

static void value_ssa_available_value_set_kill_invalidated_entries(
    ValueSsaAvailableValueSet *set,
    const ValueSsaRedundantBinaryContext *context,
    const ValueSsaInstruction *instruction) {
    size_t read_index;
    size_t write_index = 0u;

    if (!set || !context || !instruction) {
        return;
    }

    for (read_index = 0; read_index < set->entry_count; ++read_index) {
        if (value_ssa_available_value_instruction_kills_load_indirect(
                context, &set->entries[read_index], instruction)) {
            continue;
        }
        if (write_index != read_index) {
            set->entries[write_index] = set->entries[read_index];
        }
        ++write_index;
    }

    set->entry_count = write_index;
}

static int value_ssa_available_value_ensure_merge_phi(ValueSsaRedundantBinaryContext *context,
    size_t block_id,
    const ValueSsaAvailableBinary *key,
    const size_t *predecessor_ids,
    const ValueSsaValueRef *predecessor_results,
    size_t predecessor_count,
    ValueSsaValueRef *out_result,
    ValueSsaError *error) {
    ValueSsaAvailableValueBlockState *state;
    ValueSsaBasicBlock *block;
    size_t merge_index;

    if (!context || !context->function || !key || !predecessor_ids || !predecessor_results ||
        predecessor_count == 0u || !out_result) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-223: invalid merge-phi contract");
        return 0;
    }

    state = &context->block_states[block_id];
    block = &context->function->blocks[block_id];
    for (merge_index = 0; merge_index < state->merge_phi_count; ++merge_index) {
        ValueSsaAvailableMergePhi *merge_phi = &state->merge_phis[merge_index];
        ValueSsaPhi *phi;
        ValueSsaPhiInput *new_inputs;
        size_t input_index;

        if (!value_ssa_available_value_key_equal(context, &merge_phi->key, key)) {
            continue;
        }
        if (merge_phi->phi_index >= block->phi_count) {
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-224: stale merge-phi index");
            return 0;
        }

        phi = &block->phis[merge_phi->phi_index];
        if (phi->input_capacity < predecessor_count) {
            new_inputs =
                (ValueSsaPhiInput *)realloc(phi->inputs, predecessor_count * sizeof(ValueSsaPhiInput));
            if (!new_inputs) {
                value_ssa_set_error(error, 0, 0, "VALUE-SSA-225: out of memory growing merge-phi inputs");
                return 0;
            }
            phi->inputs = new_inputs;
            phi->input_capacity = predecessor_count;
        }
        phi->input_count = predecessor_count;
        for (input_index = 0; input_index < predecessor_count; ++input_index) {
            phi->inputs[input_index].predecessor_block_id = predecessor_ids[input_index];
            phi->inputs[input_index].value = predecessor_results[input_index];
        }
        *out_result = merge_phi->result;
        return 1;
    }

    if (!value_ssa_available_value_merge_phi_reserve(state, state->merge_phi_count + 1u, error)) {
        return 0;
    }

    {
        size_t result_id = value_ssa_function_allocate_value(context->function);
        size_t phi_index;
        ValueSsaAvailableMergePhi *merge_phi;

        if (result_id == (size_t)-1) {
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-226: unable to allocate merge-phi value");
            return 0;
        }

        if (!value_ssa_available_value_append_phi(block, result_id, NULL, 0u, &phi_index, error)) {
            return 0;
        }

        merge_phi = &state->merge_phis[state->merge_phi_count++];
        merge_phi->key = *key;
        merge_phi->phi_index = phi_index;
        merge_phi->result = value_ssa_value_id(result_id);
        *out_result = merge_phi->result;
    }

    return value_ssa_available_value_ensure_merge_phi(
        context, block_id, key, predecessor_ids, predecessor_results, predecessor_count, out_result, error);
}

static int value_ssa_available_value_compute_block_entry(ValueSsaRedundantBinaryContext *context,
    const ValueSsaCfgAnalysis *analysis,
    size_t block_id,
    ValueSsaAvailableValueSet *out_entry,
    ValueSsaError *error) {
    size_t reachable_predecessor_count = 0u;
    size_t predecessor_id;
    size_t *predecessor_ids = NULL;
    ValueSsaValueRef *predecessor_results = NULL;
    ValueSsaAvailableValueSet scratch;
    int ok = 0;

    if (!context || !analysis || !out_entry) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-227: invalid available-value entry contract");
        return 0;
    }

    value_ssa_available_value_set_init(&scratch);
    out_entry->entry_count = 0u;

    if (block_id == 0u) {
        ok = 1;
        goto done;
    }

    for (predecessor_id = 0; predecessor_id < context->function->block_count; ++predecessor_id) {
        if (analysis->predecessor_matrix[predecessor_id * context->function->block_count + block_id] &&
            (!analysis->reachable || analysis->reachable[predecessor_id])) {
            ++reachable_predecessor_count;
        }
    }
    if (reachable_predecessor_count == 0u) {
        ok = 1;
        goto done;
    }

    predecessor_ids = (size_t *)malloc(reachable_predecessor_count * sizeof(size_t));
    predecessor_results = (ValueSsaValueRef *)malloc(reachable_predecessor_count * sizeof(ValueSsaValueRef));
    if (!predecessor_ids || !predecessor_results) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-228: out of memory collecting predecessors");
        goto done;
    }

    reachable_predecessor_count = 0u;
    for (predecessor_id = 0; predecessor_id < context->function->block_count; ++predecessor_id) {
        if (analysis->predecessor_matrix[predecessor_id * context->function->block_count + block_id] &&
            (!analysis->reachable || analysis->reachable[predecessor_id])) {
            predecessor_ids[reachable_predecessor_count++] = predecessor_id;
        }
    }

    {
        const ValueSsaAvailableValueSet *first_exit =
            &context->block_states[predecessor_ids[0]].exit_values;
        size_t entry_index;

        if (!value_ssa_available_value_set_reserve(out_entry, first_exit->entry_count, error)) {
            goto done;
        }

        for (entry_index = 0; entry_index < first_exit->entry_count; ++entry_index) {
            const ValueSsaAvailableBinary *candidate = &first_exit->entries[entry_index];
            ValueSsaAvailableBinary merged = *candidate;
            size_t other_index;
            int all_match = 1;

            predecessor_results[0] = candidate->result;
            for (other_index = 1u; other_index < reachable_predecessor_count; ++other_index) {
                const ValueSsaAvailableValueSet *other_exit =
                    &context->block_states[predecessor_ids[other_index]].exit_values;

                if (!value_ssa_available_value_set_lookup_entry(
                        other_exit, context, candidate, &predecessor_results[other_index])) {
                    all_match = 0;
                    break;
                }
            }
            if (!all_match) {
                continue;
            }

            for (other_index = 1u; other_index < reachable_predecessor_count; ++other_index) {
                if (!value_ssa_value_refs_equal(predecessor_results[0], predecessor_results[other_index])) {
                    break;
                }
            }

            if (other_index == reachable_predecessor_count) {
                merged.result = predecessor_results[0];
            } else if (candidate->kind == VALUE_SSA_AVAILABLE_VALUE_LOAD_LOCAL ||
                       candidate->kind == VALUE_SSA_AVAILABLE_VALUE_LOAD_GLOBAL ||
                       candidate->kind == VALUE_SSA_AVAILABLE_VALUE_LOAD_INDIRECT ||
                       candidate->kind == VALUE_SSA_AVAILABLE_VALUE_ADDR_LOCAL ||
                       candidate->kind == VALUE_SSA_AVAILABLE_VALUE_ADDR_GLOBAL ||
                       candidate->kind == VALUE_SSA_AVAILABLE_VALUE_BINARY ||
                       candidate->kind == VALUE_SSA_AVAILABLE_VALUE_MOV) {
                const ValueSsaBasicBlock *block = &context->function->blocks[block_id];
                size_t phi_index;
                int matched_existing_phi = 0;

                for (phi_index = 0; phi_index < block->phi_count; ++phi_index) {
                    const ValueSsaPhi *phi = &block->phis[phi_index];
                    size_t input_index;

                    if (phi->input_count != reachable_predecessor_count) {
                        continue;
                    }
                    for (input_index = 0; input_index < reachable_predecessor_count; ++input_index) {
                        if (phi->inputs[input_index].predecessor_block_id != predecessor_ids[input_index] ||
                            !value_ssa_value_refs_equal(phi->inputs[input_index].value, predecessor_results[input_index])) {
                            break;
                        }
                    }
                    if (input_index == reachable_predecessor_count) {
                        merged.result = value_ssa_value_id(phi->result_id);
                        matched_existing_phi = 1;
                        break;
                    }
                }

                if (!matched_existing_phi &&
                    !value_ssa_available_value_ensure_merge_phi(context,
                        block_id,
                        candidate,
                        predecessor_ids,
                        predecessor_results,
                        reachable_predecessor_count,
                        &merged.result,
                        error)) {
                    goto done;
                }
            } else if (!value_ssa_available_value_ensure_merge_phi(context,
                           block_id,
                           candidate,
                           predecessor_ids,
                           predecessor_results,
                           reachable_predecessor_count,
                           &merged.result,
                           error)) {
                goto done;
            }

            if (!value_ssa_available_value_set_append(&scratch, &merged, error)) {
                goto done;
            }
        }
    }

    if (!value_ssa_available_value_set_copy_from(out_entry, &scratch, error)) {
        goto done;
    }

    ok = 1;

done:
    free(predecessor_ids);
    free(predecessor_results);
    value_ssa_available_value_set_free(&scratch);
    return ok;
}

static int value_ssa_available_value_rewrite_block(ValueSsaRedundantBinaryContext *context,
    size_t block_id,
    const ValueSsaAvailableValueSet *entry_values,
    ValueSsaAvailableValueSet *out_exit_values,
    ValueSsaError *error) {
    ValueSsaAvailableValueSet current;
    ValueSsaBasicBlock *block;
    size_t instruction_index;
    int ok = 0;

    if (!context || !context->function || !entry_values || !out_exit_values) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-229: invalid available-value rewrite contract");
        return 0;
    }

    value_ssa_available_value_set_init(&current);
    block = &context->function->blocks[block_id];
    if (!value_ssa_available_value_set_copy_from(&current, entry_values, error)) {
        goto done;
    }

    for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
        ValueSsaInstruction *instruction = &block->instructions[instruction_index];
        ValueSsaAvailableBinary key;
        ValueSsaValueRef existing;

        value_ssa_available_value_set_kill_invalidated_entries(&current, context, instruction);

        if (!value_ssa_available_value_build_from_instruction(context, instruction, &key)) {
            continue;
        }

        if (value_ssa_available_value_set_lookup_entry(&current, context, &key, &existing)) {
            instruction->kind = VALUE_SSA_INSTR_MOV;
            instruction->as.mov_value = existing;
            continue;
        }

        if (!value_ssa_available_value_set_append(&current, &key, error)) {
            goto done;
        }
    }

    if (!value_ssa_available_value_set_copy_from(out_exit_values, &current, error)) {
        goto done;
    }

    ok = 1;

done:
    value_ssa_available_value_set_free(&current);
    return ok;
}

int value_ssa_eliminate_redundant_binaries(ValueSsaProgram *program, ValueSsaError *error) {
    size_t function_index;

    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-210: invalid redundant-binary contract");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunction *function = &program->functions[function_index];
        ValueSsaCfgAnalysis analysis;
        ValueSsaRedundantBinaryContext context;
        size_t block_index;
        int changed;
        int ok;

        if (!function->has_body) {
            continue;
        }

        value_ssa_cfg_analysis_init(&analysis);
        value_ssa_redundant_binary_context_init(&context);
        context.program = program;
        context.function = function;

        if (program->global_count > 0u) {
            context.readonly_globals = (unsigned char *)calloc(program->global_count, sizeof(unsigned char));
            if (!context.readonly_globals) {
                value_ssa_redundant_binary_context_free(&context);
                value_ssa_set_error(error, 0, 0, "VALUE-SSA-230: out of memory preparing redundant-binary globals");
                return 0;
            }
            if (!value_ssa_collect_redundant_binary_readonly_globals(program, context.readonly_globals)) {
                value_ssa_redundant_binary_context_free(&context);
                value_ssa_set_error(error, 0, 0, "VALUE-SSA-231: failed collecting redundant-binary globals");
                return 0;
            }
        }
        if (function->local_count > 0u) {
            size_t block_index;
            context.address_taken_locals = (unsigned char *)calloc(function->local_count, sizeof(unsigned char));
            if (!context.address_taken_locals) {
                value_ssa_cfg_analysis_free(&analysis);
                value_ssa_redundant_binary_context_free(&context);
                value_ssa_set_error(error, 0, 0, "VALUE-SSA-230A: out of memory preparing local-address flags");
                return 0;
            }
            for (block_index = 0; block_index < function->block_count; ++block_index) {
                const ValueSsaBasicBlock *block = &function->blocks[block_index];
                size_t instruction_index;

                for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                    const ValueSsaInstruction *instruction = &block->instructions[instruction_index];

                    if (instruction->kind == VALUE_SSA_INSTR_ADDR_LOCAL &&
                        instruction->as.addr_slot.kind == VALUE_SSA_SLOT_LOCAL &&
                        instruction->as.addr_slot.id < function->local_count) {
                        context.address_taken_locals[instruction->as.addr_slot.id] = 1u;
                    }
                }
            }
        }

        context.block_states = (ValueSsaAvailableValueBlockState *)calloc(
            function->block_count, sizeof(ValueSsaAvailableValueBlockState));
        if (!context.block_states) {
            value_ssa_cfg_analysis_free(&analysis);
            value_ssa_redundant_binary_context_free(&context);
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-232: out of memory preparing GVN block states");
            return 0;
        }
        for (block_index = 0; block_index < function->block_count; ++block_index) {
            value_ssa_available_value_block_state_init(&context.block_states[block_index]);
        }

        ok = value_ssa_compute_cfg_analysis(function, &analysis, error) &&
            value_ssa_compute_def_use_analysis(function, &context.def_use, error);
        while (ok) {
            changed = 0;
            for (block_index = 0; block_index < function->block_count; ++block_index) {
                ValueSsaAvailableValueSet new_entry;
                ValueSsaAvailableValueSet new_exit;
                int entry_changed;
                int exit_changed;

                if (analysis.reachable && !analysis.reachable[block_index]) {
                    continue;
                }

                value_ssa_available_value_set_init(&new_entry);
                value_ssa_available_value_set_init(&new_exit);

                ok = value_ssa_available_value_compute_block_entry(
                    &context, &analysis, block_index, &new_entry, error) &&
                    value_ssa_available_value_rewrite_block(
                        &context, block_index, &new_entry, &new_exit, error);
                if (!ok) {
                    value_ssa_available_value_set_free(&new_entry);
                    value_ssa_available_value_set_free(&new_exit);
                    break;
                }

                entry_changed = !value_ssa_available_value_set_equals(
                    &context.block_states[block_index].entry_values, &context, &new_entry);
                exit_changed = !value_ssa_available_value_set_equals(
                    &context.block_states[block_index].exit_values, &context, &new_exit);

                if (entry_changed &&
                    !value_ssa_available_value_set_copy_from(
                        &context.block_states[block_index].entry_values, &new_entry, error)) {
                    ok = 0;
                    value_ssa_available_value_set_free(&new_entry);
                    value_ssa_available_value_set_free(&new_exit);
                    break;
                }
                if (exit_changed &&
                    !value_ssa_available_value_set_copy_from(
                        &context.block_states[block_index].exit_values, &new_exit, error)) {
                    ok = 0;
                    value_ssa_available_value_set_free(&new_entry);
                    value_ssa_available_value_set_free(&new_exit);
                    break;
                }

                changed |= entry_changed || exit_changed;
                value_ssa_available_value_set_free(&new_entry);
                value_ssa_available_value_set_free(&new_exit);
            }

            if (!ok || !changed) {
                break;
            }
        }

        value_ssa_redundant_binary_context_free(&context);
        value_ssa_cfg_analysis_free(&analysis);
        if (!ok) {
            return 0;
        }
    }

    return value_ssa_verify_program(program, error);
}

int value_ssa_eliminate_redundant_indirect_loads(ValueSsaProgram *program, ValueSsaError *error) {
    size_t function_index;

    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-222: invalid redundant-indirect-load contract");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunction *function = &program->functions[function_index];
        ValueSsaDefUseAnalysis def_use;
        int changed = 0;
        size_t block_index;

        if (!function->has_body) {
            continue;
        }

        value_ssa_def_use_analysis_init(&def_use);
        if (!value_ssa_compute_def_use_analysis(function, &def_use, error)) {
            value_ssa_def_use_analysis_free(&def_use);
            return 0;
        }

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            ValueSsaBasicBlock *block = &function->blocks[block_index];
            size_t instruction_index;

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                ValueSsaInstruction *load = &block->instructions[instruction_index];
                ValueSsaPassAddressRoot load_root;
                int has_load_root;
                size_t candidate_index;

                if (load->kind != VALUE_SSA_INSTR_LOAD_INDIRECT ||
                    !load->has_result ||
                    load->result.kind != VALUE_SSA_VALUE_ID) {
                    continue;
                }

                has_load_root = value_ssa_pass_extract_address_root(
                    function, &def_use, load->as.load_indirect_addr, &load_root, 0u);

                for (candidate_index = instruction_index; candidate_index > 0; --candidate_index) {
                    ValueSsaInstruction *candidate = &block->instructions[candidate_index - 1u];
                    size_t middle_index;
                    int blocked = 0;

                    if (candidate->kind != VALUE_SSA_INSTR_LOAD_INDIRECT ||
                        !candidate->has_result ||
                        candidate->result.kind != VALUE_SSA_VALUE_ID) {
                        continue;
                    }
                    if (!value_ssa_value_refs_equal(candidate->as.load_indirect_addr, load->as.load_indirect_addr) &&
                        !value_ssa_pass_same_pure_address_value(
                            function, &def_use, candidate->as.load_indirect_addr, load->as.load_indirect_addr, 0u)) {
                        continue;
                    }

                    for (middle_index = candidate_index; middle_index < instruction_index; ++middle_index) {
                        ValueSsaInstruction *middle = &block->instructions[middle_index];

                        if (middle->kind == VALUE_SSA_INSTR_CALL) {
                            blocked = 1;
                            break;
                        }
                        if (value_ssa_pass_instruction_may_invalidate_address_value(
                                function, &def_use, load->as.load_indirect_addr, middle)) {
                            blocked = 1;
                            break;
                        }
                        if (has_load_root) {
                            if (!value_ssa_pass_store_cannot_alias_loaded_address(
                                    function, &def_use, &load_root, middle)) {
                                blocked = 1;
                                break;
                            }
                        } else if (middle->kind == VALUE_SSA_INSTR_STORE_LOCAL ||
                                   middle->kind == VALUE_SSA_INSTR_STORE_GLOBAL ||
                                   middle->kind == VALUE_SSA_INSTR_STORE_INDIRECT) {
                            blocked = 1;
                            break;
                        }
                    }
                    if (blocked) {
                        continue;
                    }

                    load->kind = VALUE_SSA_INSTR_MOV;
                    load->as.mov_value = candidate->result;
                    changed = 1;
                    break;
                }
            }
        }

        value_ssa_def_use_analysis_free(&def_use);
        if (changed &&
            (!value_ssa_simplify_trivial_values(program, error) ||
                !value_ssa_eliminate_dead_value_defs(program, error))) {
            return 0;
        }
    }

    return value_ssa_verify_program(program, error);
}

#endif
