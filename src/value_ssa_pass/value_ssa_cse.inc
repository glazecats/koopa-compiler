#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_SPLIT_AGGREGATOR)
#define VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa_pass.c"
#else

typedef struct {
    ValueSsaBinaryOp op;
    ValueSsaValueRef lhs;
    ValueSsaValueRef rhs;
    ValueSsaValueRef result;
} ValueSsaAvailableBinary;

typedef struct {
    ValueSsaFunction *function;
    ValueSsaAvailableBinary *entries;
    size_t entry_count;
    size_t entry_capacity;
    size_t *scope_starts;
    size_t scope_count;
    size_t scope_capacity;
} ValueSsaRedundantBinaryContext;

static void value_ssa_redundant_binary_context_init(ValueSsaRedundantBinaryContext *context) {
    if (!context) {
        return;
    }

    context->function = NULL;
    context->entries = NULL;
    context->entry_count = 0;
    context->entry_capacity = 0;
    context->scope_starts = NULL;
    context->scope_count = 0;
    context->scope_capacity = 0;
}

static void value_ssa_redundant_binary_context_free(ValueSsaRedundantBinaryContext *context) {
    if (!context) {
        return;
    }

    free(context->entries);
    free(context->scope_starts);
    value_ssa_redundant_binary_context_init(context);
}

static int value_ssa_redundant_binary_begin_scope(ValueSsaRedundantBinaryContext *context,
    ValueSsaError *error) {
    size_t next_capacity;
    size_t *new_scopes;

    if (!context) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-211: invalid redundant-binary scope contract");
        return 0;
    }

    if (context->scope_count == context->scope_capacity) {
        if (!value_ssa_next_growth_capacity(context->scope_capacity,
                8,
                sizeof(size_t),
                &next_capacity)) {
            value_ssa_set_error(error,
                0,
                0,
                "VALUE-SSA-212: scope capacity overflow during redundant-binary elimination");
            return 0;
        }

        new_scopes = (size_t *)realloc(context->scope_starts, next_capacity * sizeof(size_t));
        if (!new_scopes) {
            value_ssa_set_error(error,
                0,
                0,
                "VALUE-SSA-213: out of memory growing redundant-binary scopes");
            return 0;
        }

        context->scope_starts = new_scopes;
        context->scope_capacity = next_capacity;
    }

    context->scope_starts[context->scope_count++] = context->entry_count;
    return 1;
}

static int value_ssa_redundant_binary_end_scope(ValueSsaRedundantBinaryContext *context,
    ValueSsaError *error) {
    if (!context || context->scope_count == 0) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-214: redundant-binary scope underflow");
        return 0;
    }

    context->entry_count = context->scope_starts[--context->scope_count];
    return 1;
}

static int value_ssa_redundant_binary_append(ValueSsaRedundantBinaryContext *context,
    ValueSsaBinaryOp op,
    ValueSsaValueRef lhs,
    ValueSsaValueRef rhs,
    ValueSsaValueRef result,
    ValueSsaError *error) {
    size_t next_capacity;
    ValueSsaAvailableBinary *new_entries;

    if (!context) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-215: invalid redundant-binary append contract");
        return 0;
    }

    if (context->entry_count == context->entry_capacity) {
        if (!value_ssa_next_growth_capacity(context->entry_capacity,
                16,
                sizeof(ValueSsaAvailableBinary),
                &next_capacity)) {
            value_ssa_set_error(error,
                0,
                0,
                "VALUE-SSA-216: entry capacity overflow during redundant-binary elimination");
            return 0;
        }

        new_entries = (ValueSsaAvailableBinary *)realloc(
            context->entries, next_capacity * sizeof(ValueSsaAvailableBinary));
        if (!new_entries) {
            value_ssa_set_error(error,
                0,
                0,
                "VALUE-SSA-217: out of memory growing redundant-binary table");
            return 0;
        }

        context->entries = new_entries;
        context->entry_capacity = next_capacity;
    }

    context->entries[context->entry_count].op = op;
    context->entries[context->entry_count].lhs = lhs;
    context->entries[context->entry_count].rhs = rhs;
    context->entries[context->entry_count].result = result;
    ++context->entry_count;
    return 1;
}

static int value_ssa_redundant_binary_lookup(const ValueSsaRedundantBinaryContext *context,
    ValueSsaBinaryOp op,
    ValueSsaValueRef lhs,
    ValueSsaValueRef rhs,
    ValueSsaValueRef *out_result) {
    size_t index;

    if (!context || !out_result) {
        return 0;
    }

    for (index = context->entry_count; index > 0; --index) {
        const ValueSsaAvailableBinary *entry = &context->entries[index - 1];

        if (entry->op == op &&
            value_ssa_value_refs_equal(entry->lhs, lhs) &&
            value_ssa_value_refs_equal(entry->rhs, rhs)) {
            *out_result = entry->result;
            return 1;
        }
    }

    return 0;
}

static int value_ssa_redundant_binary_enter_block(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *analysis,
    size_t block_id,
    void *user_data,
    ValueSsaError *error) {
    ValueSsaRedundantBinaryContext *context = (ValueSsaRedundantBinaryContext *)user_data;
    ValueSsaBasicBlock *block;
    size_t instruction_index;

    (void)function;
    (void)analysis;

    if (!context || !context->function) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-218: invalid redundant-binary walk contract");
        return 0;
    }

    block = &context->function->blocks[block_id];
    if (!value_ssa_redundant_binary_begin_scope(context, error)) {
        return 0;
    }

    for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
        ValueSsaInstruction *instruction = &block->instructions[instruction_index];
        ValueSsaValueRef existing;

        if (instruction->kind != VALUE_SSA_INSTR_BINARY ||
            !value_ssa_binary_op_is_safe_reusable_pure_binary(
                instruction->as.binary.op,
                instruction->as.binary.lhs,
                instruction->as.binary.rhs)) {
            continue;
        }

        if (value_ssa_redundant_binary_lookup(context,
                instruction->as.binary.op,
                instruction->as.binary.lhs,
                instruction->as.binary.rhs,
                &existing)) {
            instruction->kind = VALUE_SSA_INSTR_MOV;
            instruction->as.mov_value = existing;
            continue;
        }

        if (!value_ssa_redundant_binary_append(context,
                instruction->as.binary.op,
                instruction->as.binary.lhs,
                instruction->as.binary.rhs,
                instruction->result,
                error)) {
            return 0;
        }
    }

    return 1;
}

static int value_ssa_redundant_binary_leave_block(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *analysis,
    size_t block_id,
    void *user_data,
    ValueSsaError *error) {
    ValueSsaRedundantBinaryContext *context = (ValueSsaRedundantBinaryContext *)user_data;

    (void)function;
    (void)analysis;
    (void)block_id;

    return value_ssa_redundant_binary_end_scope(context, error);
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
        int ok;

        if (!function->has_body) {
            continue;
        }

        value_ssa_cfg_analysis_init(&analysis);
        value_ssa_redundant_binary_context_init(&context);
        context.function = function;
        ok = value_ssa_compute_cfg_analysis(function, &analysis, error) &&
            value_ssa_walk_dominator_tree(function,
                &analysis,
                value_ssa_redundant_binary_enter_block,
                value_ssa_redundant_binary_leave_block,
                &context,
                error);
        value_ssa_redundant_binary_context_free(&context);
        value_ssa_cfg_analysis_free(&analysis);
        if (!ok) {
            return 0;
        }
    }

    return value_ssa_verify_program(program, error);
}

#endif
