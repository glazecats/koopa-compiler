#if defined(__INTELLISENSE__) && !defined(MEMORY_SSA_PASS_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MEMORY_SSA_PASS_SPLIT_AGGREGATOR)
#define MEMORY_SSA_PASS_SPLIT_INTELLISENSE_CONTEXT
#include "memory_ssa_pass.c"
#else

#define MEMORY_SSA_PASS_MEMORY_BINARY_CSE_MAX_ITERATIONS 256

static int memory_ssa_pass_binary_op_is_join_gvn_safe(ValueSsaBinaryOp op) {
    switch (op) {
    case VALUE_SSA_BINARY_DIV:
    case VALUE_SSA_BINARY_MOD:
    case VALUE_SSA_BINARY_SHIFT_LEFT:
    case VALUE_SSA_BINARY_SHIFT_RIGHT:
        return 0;
    default:
        return 1;
    }
}

static int memory_ssa_pass_join_binary_op_is_commutative(ValueSsaBinaryOp op) {
    switch (op) {
    case VALUE_SSA_BINARY_ADD:
    case VALUE_SSA_BINARY_MUL:
    case VALUE_SSA_BINARY_BIT_AND:
    case VALUE_SSA_BINARY_BIT_XOR:
    case VALUE_SSA_BINARY_BIT_OR:
    case VALUE_SSA_BINARY_EQ:
    case VALUE_SSA_BINARY_NE:
        return 1;
    default:
        return 0;
    }
}

static int memory_ssa_pass_join_binary_op_swap_operands(ValueSsaBinaryOp op, ValueSsaBinaryOp *out_swapped_op) {
    if (!out_swapped_op) {
        return 0;
    }

    switch (op) {
    case VALUE_SSA_BINARY_LT:
        *out_swapped_op = VALUE_SSA_BINARY_GT;
        return 1;
    case VALUE_SSA_BINARY_LE:
        *out_swapped_op = VALUE_SSA_BINARY_GE;
        return 1;
    case VALUE_SSA_BINARY_GT:
        *out_swapped_op = VALUE_SSA_BINARY_LT;
        return 1;
    case VALUE_SSA_BINARY_GE:
        *out_swapped_op = VALUE_SSA_BINARY_LE;
        return 1;
    default:
        if (memory_ssa_pass_join_binary_op_is_commutative(op)) {
            *out_swapped_op = op;
            return 1;
        }
        return 0;
    }
}

static int memory_ssa_pass_normalize_join_binary_operands(ValueSsaBinaryOp *op,
    ValueSsaValueRef *lhs,
    ValueSsaValueRef *rhs) {
    ValueSsaBinaryOp swapped_op;
    ValueSsaValueRef old_lhs;

    if (!op || !lhs || !rhs) {
        return 0;
    }

    if (!memory_ssa_pass_join_binary_op_swap_operands(*op, &swapped_op)) {
        return 1;
    }
    if (lhs->kind != VALUE_SSA_VALUE_IMMEDIATE && rhs->kind != VALUE_SSA_VALUE_IMMEDIATE) {
        if (!memory_ssa_pass_join_binary_op_is_commutative(*op) || lhs->kind != VALUE_SSA_VALUE_ID ||
            rhs->kind != VALUE_SSA_VALUE_ID || lhs->value_id <= rhs->value_id) {
            return 1;
        }
    } else if (lhs->kind != VALUE_SSA_VALUE_IMMEDIATE || rhs->kind == VALUE_SSA_VALUE_IMMEDIATE) {
        return 1;
    }

    old_lhs = *lhs;
    *lhs = *rhs;
    *rhs = old_lhs;
    *op = swapped_op;
    return 1;
}

static int memory_ssa_pass_fold_join_binary_immediates(ValueSsaBinaryOp op,
    long long lhs,
    long long rhs,
    int *out_has_value,
    long long *out_value) {
    if (!out_has_value || !out_value) {
        return 0;
    }

    *out_has_value = 1;
    switch (op) {
    case VALUE_SSA_BINARY_ADD:
        *out_value = lhs + rhs;
        return 1;
    case VALUE_SSA_BINARY_SUB:
        *out_value = lhs - rhs;
        return 1;
    case VALUE_SSA_BINARY_MUL:
        *out_value = lhs * rhs;
        return 1;
    case VALUE_SSA_BINARY_BIT_AND:
        *out_value = lhs & rhs;
        return 1;
    case VALUE_SSA_BINARY_BIT_XOR:
        *out_value = lhs ^ rhs;
        return 1;
    case VALUE_SSA_BINARY_BIT_OR:
        *out_value = lhs | rhs;
        return 1;
    case VALUE_SSA_BINARY_EQ:
        *out_value = lhs == rhs;
        return 1;
    case VALUE_SSA_BINARY_NE:
        *out_value = lhs != rhs;
        return 1;
    case VALUE_SSA_BINARY_LT:
        *out_value = lhs < rhs;
        return 1;
    case VALUE_SSA_BINARY_LE:
        *out_value = lhs <= rhs;
        return 1;
    case VALUE_SSA_BINARY_GT:
        *out_value = lhs > rhs;
        return 1;
    case VALUE_SSA_BINARY_GE:
        *out_value = lhs >= rhs;
        return 1;
    default:
        *out_has_value = 0;
        return 1;
    }
}

typedef struct {
    ValueSsaFunction *function;
    const ValueSsaDefUseAnalysis *def_use;
    const ValueSsaCfgAnalysis *cfg_analysis;
    size_t block_id;
    size_t predecessor_block_id;
    int allow_materialize;
} MemorySsaPassProjectionContext;

static int memory_ssa_pass_choose_join_binary_input_value(const ValueSsaFunction *function,
    size_t predecessor_block_id,
    ValueSsaBinaryOp op,
    ValueSsaValueRef lhs,
    ValueSsaValueRef rhs,
    int *out_has_value,
    ValueSsaValueRef *out_value,
    ValueSsaError *error);

static int memory_ssa_pass_project_value_for_predecessor(const MemorySsaPassProjectionContext *context,
    ValueSsaValueRef value,
    int *out_has_value,
    ValueSsaValueRef *out_value,
    int *out_materialized,
    ValueSsaError *error);

static int memory_ssa_pass_phi_input_for_predecessor(const ValueSsaPhi *phi,
    size_t predecessor_block_id,
    int *out_has_value,
    ValueSsaValueRef *out_value,
    ValueSsaError *error) {
    size_t input_index;

    if (!phi || !out_has_value || !out_value) {
        memory_ssa_pass_set_error(error, 0, 0, "MEMORY-SSA-PASS-068: invalid phi-input lookup contract");
        return 0;
    }

    *out_has_value = 0;
    *out_value = value_ssa_value_immediate(0);
    for (input_index = 0; input_index < phi->input_count; ++input_index) {
        if (phi->inputs[input_index].predecessor_block_id == predecessor_block_id) {
            *out_has_value = 1;
            *out_value = phi->inputs[input_index].value;
            return 1;
        }
    }

    return 1;
}

static int memory_ssa_pass_binary_operand_for_predecessor(ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *def_use,
    size_t block_id,
    ValueSsaValueRef operand,
    size_t predecessor_block_id,
    int *out_has_value,
    ValueSsaValueRef *out_value,
    int *out_materialized,
    ValueSsaError *error) {
    MemorySsaPassProjectionContext context;

    if (!function || !def_use || !out_has_value || !out_value) {
        memory_ssa_pass_set_error(error, 0, 0, "MEMORY-SSA-PASS-069: invalid join-binary operand contract");
        return 0;
    }

    context.function = function;
    context.def_use = def_use;
    context.cfg_analysis = NULL;
    context.block_id = block_id;
    context.predecessor_block_id = predecessor_block_id;
    context.allow_materialize = 1;
    return memory_ssa_pass_project_value_for_predecessor(
        &context, operand, out_has_value, out_value, out_materialized, error);
}

static int memory_ssa_pass_find_block_binary_value(const ValueSsaFunction *function,
    size_t block_id,
    ValueSsaBinaryOp op,
    ValueSsaValueRef lhs,
    ValueSsaValueRef rhs,
    int *out_has_value,
    ValueSsaValueRef *out_value,
    ValueSsaError *error) {
    const ValueSsaBasicBlock *block;
    size_t instruction_index;

    if (!function || !out_has_value || !out_value) {
        memory_ssa_pass_set_error(error, 0, 0, "MEMORY-SSA-PASS-070: invalid predecessor-binary lookup contract");
        return 0;
    }

    *out_has_value = 0;
    *out_value = value_ssa_value_immediate(0);
    if (block_id >= function->block_count) {
        memory_ssa_pass_set_error(error, 0, 0, "MEMORY-SSA-PASS-071: invalid predecessor-binary block");
        return 0;
    }

    block = &function->blocks[block_id];
    for (instruction_index = block->instruction_count; instruction_index > 0; --instruction_index) {
        const ValueSsaInstruction *instruction = &block->instructions[instruction_index - 1];

        if (instruction->kind != VALUE_SSA_INSTR_BINARY || !instruction->has_result ||
            !memory_ssa_pass_binary_op_is_join_gvn_safe(instruction->as.binary.op)) {
            continue;
        }
        if (instruction->as.binary.op == op &&
            memory_ssa_pass_value_refs_equal(instruction->as.binary.lhs, lhs) &&
            memory_ssa_pass_value_refs_equal(instruction->as.binary.rhs, rhs)) {
            *out_has_value = 1;
            *out_value = instruction->result;
            return 1;
        }
    }

    return 1;
}

static int memory_ssa_pass_choose_join_binary_input_value(const ValueSsaFunction *function,
    size_t predecessor_block_id,
    ValueSsaBinaryOp op,
    ValueSsaValueRef lhs,
    ValueSsaValueRef rhs,
    int *out_has_value,
    ValueSsaValueRef *out_value,
    ValueSsaError *error) {
    int has_pred_result = 0;
    ValueSsaValueRef pred_result = value_ssa_value_immediate(0);
    int has_folded_result = 0;
    long long folded_result = 0;

    if (!function || !out_has_value || !out_value) {
        memory_ssa_pass_set_error(
            error, 0, 0, "MEMORY-SSA-PASS-077: invalid join-binary predecessor-result contract");
        return 0;
    }

    if (!memory_ssa_pass_normalize_join_binary_operands(&op, &lhs, &rhs)) {
        memory_ssa_pass_set_error(
            error, 0, 0, "MEMORY-SSA-PASS-078: failed to normalize join-binary predecessor operands");
        return 0;
    }

    *out_has_value = 0;
    *out_value = value_ssa_value_immediate(0);
    if (!memory_ssa_pass_find_block_binary_value(
            function, predecessor_block_id, op, lhs, rhs, &has_pred_result, &pred_result, error)) {
        return 0;
    }
    if (has_pred_result) {
        *out_has_value = 1;
        *out_value = pred_result;
        return 1;
    }

    if (lhs.kind == VALUE_SSA_VALUE_IMMEDIATE && rhs.kind == VALUE_SSA_VALUE_IMMEDIATE &&
        memory_ssa_pass_fold_join_binary_immediates(
            op, lhs.immediate, rhs.immediate, &has_folded_result, &folded_result) &&
        has_folded_result) {
        *out_has_value = 1;
        *out_value = value_ssa_value_immediate(folded_result);
    }

    return 1;
}

static int memory_ssa_pass_try_materialize_join_binary_in_predecessor(const MemorySsaPassProjectionContext *context,
    ValueSsaBinaryOp op,
    ValueSsaValueRef lhs,
    ValueSsaValueRef rhs,
    int *out_has_value,
    ValueSsaValueRef *out_value,
    ValueSsaError *error) {
    ValueSsaBasicBlock *block;
    ValueSsaInstruction instruction;
    size_t result_id;

    if (!context || !context->function || !out_has_value || !out_value) {
        memory_ssa_pass_set_error(
            error, 0, 0, "MEMORY-SSA-PASS-079: invalid predecessor binary materialization contract");
        return 0;
    }
    if (context->predecessor_block_id >= context->function->block_count ||
        context->block_id >= context->function->block_count) {
        memory_ssa_pass_set_error(
            error, 0, 0, "MEMORY-SSA-PASS-080: invalid predecessor binary materialization block");
        return 0;
    }

    *out_has_value = 0;
    *out_value = value_ssa_value_immediate(0);
    if (!memory_ssa_pass_normalize_join_binary_operands(&op, &lhs, &rhs)) {
        memory_ssa_pass_set_error(
            error, 0, 0, "MEMORY-SSA-PASS-081: failed to normalize predecessor binary materialization");
        return 0;
    }

    block = &context->function->blocks[context->predecessor_block_id];
    if (context->cfg_analysis && context->cfg_analysis->dominates &&
        context->block_id < context->cfg_analysis->block_count &&
        context->predecessor_block_id < context->cfg_analysis->block_count &&
        context->cfg_analysis
            ->dominates[context->block_id * context->cfg_analysis->block_count + context->predecessor_block_id]) {
        return 1;
    }
    if (!block->has_terminator) {
        return 1;
    }
    if (block->terminator.kind == VALUE_SSA_TERM_JUMP) {
        if (block->terminator.as.jump_target != context->block_id) {
            return 1;
        }
    } else if (block->terminator.kind == VALUE_SSA_TERM_BRANCH) {
        if (block->terminator.as.branch.then_target != context->block_id &&
            block->terminator.as.branch.else_target != context->block_id) {
            return 1;
        }
    } else {
        return 1;
    }

    result_id = value_ssa_function_allocate_value(context->function);
    if (result_id == (size_t)-1) {
        memory_ssa_pass_set_error(
            error, 0, 0, "MEMORY-SSA-PASS-082: out of value ids for predecessor binary materialization");
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(result_id);
    instruction.as.binary.op = op;
    instruction.as.binary.lhs = lhs;
    instruction.as.binary.rhs = rhs;
    if (!memory_ssa_pass_append_instruction_ignoring_terminator(block, &instruction, error)) {
        return 0;
    }

    *out_has_value = 1;
    *out_value = value_ssa_value_id(result_id);
    return 1;
}

static int memory_ssa_pass_project_value_for_predecessor(const MemorySsaPassProjectionContext *context,
    ValueSsaValueRef value,
    int *out_has_value,
    ValueSsaValueRef *out_value,
    int *out_materialized,
    ValueSsaError *error) {
    const ValueSsaPhi *phi;
    MemorySsaPassProjectionContext child_context;

    if (!context || !context->function || !context->def_use || !out_has_value || !out_value) {
        memory_ssa_pass_set_error(error, 0, 0, "MEMORY-SSA-PASS-083: invalid predecessor value projection");
        return 0;
    }

    *out_has_value = 0;
    *out_value = value_ssa_value_immediate(0);
    if (out_materialized) {
        *out_materialized = 0;
    }

    if (value.kind != VALUE_SSA_VALUE_ID) {
        *out_has_value = 1;
        *out_value = value;
        return 1;
    }
    if (value.value_id >= context->def_use->value_count || !context->def_use->has_def ||
        !context->def_use->has_def[value.value_id]) {
        return 1;
    }
    if (context->def_use->def_block_ids[value.value_id] != context->block_id) {
        *out_has_value = 1;
        *out_value = value;
        return 1;
    }

    if (context->def_use->def_phi_indices[value.value_id] != (size_t)-1 &&
        context->def_use->def_phi_indices[value.value_id] < context->function->blocks[context->block_id].phi_count) {
        phi = &context->function->blocks[context->block_id].phis[context->def_use->def_phi_indices[value.value_id]];
        return memory_ssa_pass_phi_input_for_predecessor(
            phi, context->predecessor_block_id, out_has_value, out_value, error);
    }

    child_context = *context;

    {
        const ValueSsaInstruction *instruction;
        size_t instruction_index = context->def_use->def_instruction_indices[value.value_id];

        if (instruction_index >= context->function->blocks[context->block_id].instruction_count) {
            return 1;
        }

        instruction = &context->function->blocks[context->block_id].instructions[instruction_index];
        if (!instruction->has_result || instruction->result.kind != VALUE_SSA_VALUE_ID ||
            instruction->result.value_id != value.value_id) {
            return 1;
        }

        if (instruction->kind == VALUE_SSA_INSTR_MOV) {
            return memory_ssa_pass_project_value_for_predecessor(
                &child_context, instruction->as.mov_value, out_has_value, out_value, out_materialized, error);
        }

        if (instruction->kind == VALUE_SSA_INSTR_BINARY &&
            memory_ssa_pass_binary_op_is_join_gvn_safe(instruction->as.binary.op)) {
            ValueSsaValueRef pred_lhs;
            ValueSsaValueRef pred_rhs;
            int has_pred_lhs = 0;
            int has_pred_rhs = 0;
            int lhs_materialized = 0;
            int rhs_materialized = 0;

            if (!memory_ssa_pass_project_value_for_predecessor(&child_context,
                    instruction->as.binary.lhs,
                    &has_pred_lhs,
                    &pred_lhs,
                    &lhs_materialized,
                    error) ||
                !memory_ssa_pass_project_value_for_predecessor(&child_context,
                    instruction->as.binary.rhs,
                    &has_pred_rhs,
                    &pred_rhs,
                    &rhs_materialized,
                    error)) {
                return 0;
            }
            if (out_materialized && (lhs_materialized || rhs_materialized)) {
                *out_materialized = 1;
            }
            if (!has_pred_lhs || !has_pred_rhs) {
                return 1;
            }

            if (!memory_ssa_pass_choose_join_binary_input_value(context->function,
                    context->predecessor_block_id,
                    instruction->as.binary.op,
                    pred_lhs,
                    pred_rhs,
                    out_has_value,
                    out_value,
                    error)) {
                return 0;
            }
            if (*out_has_value || !context->allow_materialize) {
                return 1;
            }

            if (!memory_ssa_pass_try_materialize_join_binary_in_predecessor(context,
                    instruction->as.binary.op,
                    pred_lhs,
                    pred_rhs,
                    out_has_value,
                    out_value,
                    error)) {
                return 0;
            }
            if (out_materialized && *out_has_value) {
                *out_materialized = 1;
            }
            return 1;
        }
    }

    return 1;
}

static int memory_ssa_pass_expose_memory_values_for_cse(ValueSsaProgram *program, ValueSsaError *error) {
    char *before_text = NULL;
    char *after_text = NULL;
    size_t iteration;
    int ok = 0;

    if (!program) {
        memory_ssa_pass_set_error(error, 0, 0, "MEMORY-SSA-PASS-072: invalid scalar-replace-expose contract");
        return 0;
    }
    if (!value_ssa_verify_program(program, error)) {
        return 0;
    }

    for (iteration = 0; iteration < MEMORY_SSA_PASS_MEMORY_BINARY_CSE_MAX_ITERATIONS; ++iteration) {
        free(before_text);
        free(after_text);
        before_text = NULL;
        after_text = NULL;

        if (!memory_ssa_pass_dump_program_copy(program, &before_text, error) ||
            !memory_ssa_pass_run_one_step("promote-local-slots", memory_ssa_pass_promote_local_slots, program, error) ||
            !memory_ssa_pass_run_one_step("promote-global-slots", memory_ssa_pass_promote_global_slots, program, error) ||
            !memory_ssa_pass_run_one_step("forward-local-loads", memory_ssa_pass_forward_local_loads, program, error) ||
            !memory_ssa_pass_run_one_step("forward-global-loads", memory_ssa_pass_forward_global_loads, program, error) ||
            !memory_ssa_pass_run_one_step("eliminate-redundant-stores", memory_ssa_pass_eliminate_redundant_stores, program, error) ||
            !memory_ssa_pass_run_one_step("eliminate-dead-stores", memory_ssa_pass_eliminate_dead_stores, program, error) ||
            !memory_ssa_pass_run_one_step("simplify-trivial-values", value_ssa_simplify_trivial_values, program, error) ||
            !memory_ssa_pass_dump_program_copy(program, &after_text, error)) {
            goto cleanup;
        }

        if (strcmp(before_text, after_text) == 0) {
            ok = 1;
            goto cleanup;
        }
    }

    memory_ssa_pass_set_error(error,
        0,
        0,
        "MEMORY-SSA-PASS-073: scalar-replace expose did not converge within %d iterations",
        MEMORY_SSA_PASS_MEMORY_BINARY_CSE_MAX_ITERATIONS);

cleanup:
    free(before_text);
    free(after_text);
    return ok;
}

static int memory_ssa_pass_run_detectable_step(const char *step_name,
    int (*pass_fn)(ValueSsaProgram *, ValueSsaError *),
    ValueSsaProgram *program,
    int *out_changed,
    ValueSsaError *error) {
    char *before_text = NULL;
    char *after_text = NULL;
    int ok = 0;

    if (!step_name || !pass_fn || !program || !out_changed) {
        memory_ssa_pass_set_error(error, 0, 0, "MEMORY-SSA-PASS-084: invalid detectable-step contract");
        return 0;
    }

    *out_changed = 0;
    if (!memory_ssa_pass_dump_program_copy(program, &before_text, error) ||
        !memory_ssa_pass_run_one_step(step_name, pass_fn, program, error) ||
        !memory_ssa_pass_dump_program_copy(program, &after_text, error)) {
        goto cleanup;
    }

    *out_changed = strcmp(before_text, after_text) != 0;
    ok = 1;

cleanup:
    free(before_text);
    free(after_text);
    return ok;
}

static int memory_ssa_pass_eliminate_redundant_join_binaries(ValueSsaProgram *program, ValueSsaError *error) {
    size_t iteration;
    char **seen_states = NULL;
    size_t seen_count = 0;
    int ok = 0;

    if (!program) {
        memory_ssa_pass_set_error(error, 0, 0, "MEMORY-SSA-PASS-074: invalid join-binary-gvn contract");
        return 0;
    }
    if (!value_ssa_verify_program(program, error)) {
        return 0;
    }
    seen_states =
        (char **)calloc((size_t)MEMORY_SSA_PASS_MEMORY_BINARY_CSE_MAX_ITERATIONS, sizeof(char *));
    if (!seen_states) {
        memory_ssa_pass_set_error(
            error, 0, 0, "MEMORY-SSA-PASS-075: out of memory while tracking join-binary states");
        return 0;
    }

    for (iteration = 0; iteration < MEMORY_SSA_PASS_MEMORY_BINARY_CSE_MAX_ITERATIONS; ++iteration) {
        char *current_text = NULL;
        size_t function_index;
        int changed = 0;

        if (!memory_ssa_pass_dump_program_copy(program, &current_text, error)) {
            goto cleanup;
        }
        for (function_index = 0; function_index < seen_count; ++function_index) {
            if (strcmp(seen_states[function_index], current_text) == 0) {
                free(current_text);
                ok = value_ssa_verify_program(program, error);
                goto cleanup;
            }
        }
        seen_states[seen_count++] = current_text;

        for (function_index = 0; function_index < program->function_count; ++function_index) {
            ValueSsaFunction *function = &program->functions[function_index];
            ValueSsaCfgAnalysis cfg;
            ValueSsaDefUseAnalysis def_use;
            size_t block_index;
            int ok = 1;
            int restart = 0;

            if (!function->has_body) {
                continue;
            }

            value_ssa_cfg_analysis_init(&cfg);
            value_ssa_def_use_analysis_init(&def_use);
            if (!value_ssa_compute_cfg_analysis(function, &cfg, error) ||
                !value_ssa_compute_def_use_analysis(function, &def_use, error)) {
                value_ssa_cfg_analysis_free(&cfg);
                value_ssa_def_use_analysis_free(&def_use);
                return 0;
            }

            for (block_index = 0; block_index < function->block_count && ok && !restart; ++block_index) {
                ValueSsaBasicBlock *block = &function->blocks[block_index];
                size_t predecessor_count = cfg.predecessor_counts[block_index];
                size_t instruction_index;

                if (predecessor_count < 2) {
                    continue;
                }

                for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                    ValueSsaInstruction *instruction = &block->instructions[instruction_index];
                    ValueSsaPhiInput *phi_inputs = NULL;
                    size_t predecessor_block_id;
                    size_t input_index = 0;
                    int materialized_any = 0;

                    if (instruction->kind != VALUE_SSA_INSTR_BINARY ||
                        !instruction->has_result ||
                        !memory_ssa_pass_binary_op_is_join_gvn_safe(instruction->as.binary.op)) {
                        continue;
                    }

                    phi_inputs = (ValueSsaPhiInput *)calloc(predecessor_count, sizeof(ValueSsaPhiInput));
                    if (!phi_inputs) {
                        memory_ssa_pass_set_error(error,
                            0,
                            0,
                            "MEMORY-SSA-PASS-075: out of memory while preparing join-binary rewrite");
                        ok = 0;
                        break;
                    }

                    for (predecessor_block_id = 0; predecessor_block_id < cfg.block_count; ++predecessor_block_id) {
                        MemorySsaPassProjectionContext projection_context;
                        ValueSsaValueRef pred_lhs;
                        ValueSsaValueRef pred_rhs;
                        ValueSsaValueRef pred_result = value_ssa_value_immediate(0);
                        int has_pred_lhs = 0;
                        int has_pred_rhs = 0;
                        int has_pred_result = 0;
                        int lhs_materialized = 0;
                        int rhs_materialized = 0;

                        if (!cfg.predecessor_matrix[predecessor_block_id * cfg.block_count + block_index]) {
                            continue;
                        }

                        projection_context.function = function;
                        projection_context.def_use = &def_use;
                        projection_context.cfg_analysis = &cfg;
                        projection_context.block_id = block_index;
                        projection_context.predecessor_block_id = predecessor_block_id;
                        projection_context.allow_materialize =
                            !cfg.dominates[block_index * cfg.block_count + predecessor_block_id];

                        if (!memory_ssa_pass_binary_operand_for_predecessor(function,
                                &def_use,
                                block_index,
                                instruction->as.binary.lhs,
                                predecessor_block_id,
                                &has_pred_lhs,
                                &pred_lhs,
                                &lhs_materialized,
                                error) ||
                            !memory_ssa_pass_binary_operand_for_predecessor(function,
                                &def_use,
                                block_index,
                                instruction->as.binary.rhs,
                                predecessor_block_id,
                                &has_pred_rhs,
                                &pred_rhs,
                                &rhs_materialized,
                                error)) {
                            ok = 0;
                            break;
                        }
                        if (lhs_materialized || rhs_materialized) {
                            materialized_any = 1;
                        }
                        if (!has_pred_lhs || !has_pred_rhs) {
                            break;
                        }

                        if (!memory_ssa_pass_choose_join_binary_input_value(function,
                                predecessor_block_id,
                                instruction->as.binary.op,
                                pred_lhs,
                                pred_rhs,
                                &has_pred_result,
                                &pred_result,
                                error)) {
                            ok = 0;
                            break;
                        }
                        if (!has_pred_result) {
                            if (!memory_ssa_pass_try_materialize_join_binary_in_predecessor(&projection_context,
                                    instruction->as.binary.op,
                                    pred_lhs,
                                    pred_rhs,
                                    &has_pred_result,
                                    &pred_result,
                                    error)) {
                                ok = 0;
                                break;
                            }
                            if (has_pred_result) {
                                materialized_any = 1;
                            } else {
                                break;
                            }
                        }

                        phi_inputs[input_index].predecessor_block_id = predecessor_block_id;
                        phi_inputs[input_index].value = pred_result;
                        ++input_index;
                    }

                    if (!ok) {
                        free(phi_inputs);
                        break;
                    }
                    if (input_index != predecessor_count) {
                        if (materialized_any) {
                            free(phi_inputs);
                            changed = 1;
                            restart = 1;
                            break;
                        }
                        free(phi_inputs);
                        continue;
                    }

                    {
                        int all_inputs_equal = 1;
                        ValueSsaValueRef common_value = phi_inputs[0].value;
                        size_t phi_input_index;

                        for (phi_input_index = 1; phi_input_index < predecessor_count; ++phi_input_index) {
                            if (!memory_ssa_pass_value_refs_equal(common_value, phi_inputs[phi_input_index].value)) {
                                all_inputs_equal = 0;
                                break;
                            }
                        }

                        if (all_inputs_equal) {
                            free(phi_inputs);
                            instruction->kind = VALUE_SSA_INSTR_MOV;
                            instruction->as.mov_value = common_value;
                            changed = 1;
                            if (materialized_any) {
                                restart = 1;
                                break;
                            }
                            continue;
                        }
                    }

                    {
                        size_t phi_result_id = value_ssa_function_allocate_value(function);

                        if (phi_result_id == (size_t)-1 ||
                            !memory_ssa_pass_append_phi(block, phi_result_id, phi_inputs, predecessor_count, error)) {
                            free(phi_inputs);
                            ok = 0;
                            break;
                        }

                        free(phi_inputs);
                        instruction->kind = VALUE_SSA_INSTR_MOV;
                        instruction->as.mov_value = value_ssa_value_id(phi_result_id);
                        changed = 1;
                        restart = 1;
                        break;
                    }
                }
            }

            value_ssa_cfg_analysis_free(&cfg);
            value_ssa_def_use_analysis_free(&def_use);
            if (!ok) {
                return 0;
            }
        }

        if (!changed) {
            ok = value_ssa_verify_program(program, error);
            goto cleanup;
        }
    }

    ok = value_ssa_verify_program(program, error);
cleanup:
    if (seen_states) {
        for (iteration = 0; iteration < seen_count; ++iteration) {
            free(seen_states[iteration]);
        }
    }
    free(seen_states);
    return ok;
}

static int memory_ssa_pass_run_value_cleanup_closure(ValueSsaProgram *program,
    int *out_changed,
    ValueSsaError *error) {
    char *before_text = NULL;
    char *after_text = NULL;
    char *step_before = NULL;
    char *step_after = NULL;
    char **seen_states = NULL;
    size_t seen_count = 0;
    size_t iteration;
    int changed_any = 0;
    int ok = 0;

    if (!program || !out_changed) {
        memory_ssa_pass_set_error(error, 0, 0, "MEMORY-SSA-PASS-085: invalid value-cleanup closure contract");
        return 0;
    }
    if (!value_ssa_verify_program(program, error)) {
        return 0;
    }

    seen_states =
        (char **)calloc((size_t)MEMORY_SSA_PASS_MEMORY_BINARY_CSE_MAX_ITERATIONS, sizeof(char *));
    if (!seen_states) {
        memory_ssa_pass_set_error(
            error, 0, 0, "MEMORY-SSA-PASS-085: out of memory while tracking value-cleanup states");
        return 0;
    }

    *out_changed = 0;
    for (iteration = 0; iteration < MEMORY_SSA_PASS_MEMORY_BINARY_CSE_MAX_ITERATIONS; ++iteration) {
        size_t seen_index;

        free(before_text);
        free(after_text);
        free(step_before);
        free(step_after);
        before_text = NULL;
        after_text = NULL;
        step_before = NULL;
        step_after = NULL;

        if (!memory_ssa_pass_dump_program_copy(program, &before_text, error)) {
            goto cleanup;
        }
        memory_ssa_pass_trace("value-cleanup iter=%zu begin hash=%llu",
            iteration,
            memory_ssa_pass_text_hash(before_text));
        for (seen_index = 0; seen_index < seen_count; ++seen_index) {
            if (strcmp(seen_states[seen_index], before_text) == 0) {
                memory_ssa_pass_trace("value-cleanup iter=%zu repeated-prior-state prior_iter=%zu hash=%llu",
                    iteration,
                    seen_index,
                    memory_ssa_pass_text_hash(before_text));
                *out_changed = changed_any;
                ok = 1;
                goto cleanup;
            }
        }
        seen_states[seen_count++] = before_text;
        before_text = NULL;

        {
            static const struct {
                const char *name;
                int (*fn)(ValueSsaProgram *, ValueSsaError *);
            } steps[] = {
                {"simplify-trivial-values-pre-fold", value_ssa_simplify_trivial_values},
                {"normalize-binary-operands", value_ssa_normalize_binary_operands},
                {"simplify-trivial-values-mid-fold", value_ssa_simplify_trivial_values},
                {"fold-constants", value_ssa_fold_constants},
                {"simplify-algebraic-identities", value_ssa_simplify_algebraic_identities},
                {"simplify-trivial-values", value_ssa_simplify_trivial_values},
                {"eliminate-redundant-binaries", value_ssa_eliminate_redundant_binaries},
                {"simplify-trivial-values-post-cse", value_ssa_simplify_trivial_values},
                {"eliminate-dead-value-defs", value_ssa_eliminate_dead_value_defs},
                {"simplify-cfg", value_ssa_simplify_cfg},
                {"eliminate-dead-value-defs-post-cfg", value_ssa_eliminate_dead_value_defs},
            };
            size_t step_index;

            for (step_index = 0; step_index < sizeof(steps) / sizeof(steps[0]); ++step_index) {
                free(step_before);
                free(step_after);
                step_before = NULL;
                step_after = NULL;
                if (!memory_ssa_pass_dump_program_copy(program, &step_before, error) ||
                    !memory_ssa_pass_run_one_step(steps[step_index].name, steps[step_index].fn, program, error) ||
                    !memory_ssa_pass_dump_program_copy(program, &step_after, error)) {
                    goto cleanup;
                }
                if (strcmp(step_before, step_after) != 0) {
                    memory_ssa_pass_trace("value-cleanup iter=%zu step=%s changed before=%llu after=%llu",
                        iteration,
                        steps[step_index].name,
                        memory_ssa_pass_text_hash(step_before),
                        memory_ssa_pass_text_hash(step_after));
                }
            }
            if (!memory_ssa_pass_dump_program_copy(program, &after_text, error)) {
                goto cleanup;
            }
        }

        if (strcmp(seen_states[seen_count - 1u], after_text) == 0) {
            memory_ssa_pass_trace("value-cleanup iter=%zu stabilized-in-one-iteration hash=%llu",
                iteration,
                memory_ssa_pass_text_hash(after_text));
            *out_changed = changed_any;
            ok = 1;
            goto cleanup;
        }

        memory_ssa_pass_trace("value-cleanup iter=%zu end hash=%llu",
            iteration,
            memory_ssa_pass_text_hash(after_text));
        changed_any = 1;
    }

    *out_changed = changed_any;
    ok = value_ssa_verify_program(program, error);

cleanup:
    if (seen_states) {
        for (iteration = 0; iteration < seen_count; ++iteration) {
            free(seen_states[iteration]);
        }
    }
    free(seen_states);
    free(before_text);
    free(after_text);
    free(step_before);
    free(step_after);
    return ok;
}

int memory_ssa_pass_eliminate_redundant_memory_binaries(ValueSsaProgram *program, ValueSsaError *error) {
    size_t iteration;
    int pending_expose = 1;
    int pending_join_gvn = 1;
    int pending_value_cleanup = 1;
    char *before_text = NULL;
    char *after_text = NULL;
    int ok = 0;

    if (!program) {
        memory_ssa_pass_set_error(error, 0, 0, "MEMORY-SSA-PASS-066: invalid memory-binary-cse contract");
        return 0;
    }
    if (!value_ssa_verify_program(program, error)) {
        return 0;
    }

    for (iteration = 0; iteration < MEMORY_SSA_PASS_MEMORY_BINARY_CSE_MAX_ITERATIONS; ++iteration) {
        free(before_text);
        free(after_text);
        before_text = NULL;
        after_text = NULL;
        if (!memory_ssa_pass_dump_program_copy(program, &before_text, error)) {
            goto cleanup;
        }

        if (pending_expose) {
            int step_changed = 0;

            pending_expose = 0;
            if (!memory_ssa_pass_run_detectable_step("scalar-replace-expose-values",
                    memory_ssa_pass_expose_memory_values_for_cse,
                    program,
                    &step_changed,
                    error)) {
                return 0;
            }
            if (step_changed) {
                pending_join_gvn = 1;
                pending_value_cleanup = 1;
            }
        }

        if (pending_join_gvn) {
            int step_changed = 0;

            pending_join_gvn = 0;
            if (!memory_ssa_pass_run_detectable_step("eliminate-redundant-join-binaries",
                    memory_ssa_pass_eliminate_redundant_join_binaries,
                    program,
                    &step_changed,
                    error)) {
                return 0;
            }
            if (step_changed) {
                pending_value_cleanup = 1;
            }
        }

        if (pending_value_cleanup) {
            int step_changed = 0;

            pending_value_cleanup = 0;
            if (!memory_ssa_pass_run_value_cleanup_closure(program, &step_changed, error)) {
                return 0;
            }
            if (step_changed) {
                pending_expose = 1;
                pending_join_gvn = 1;
            }
        }

        if (!memory_ssa_pass_dump_program_copy(program, &after_text, error)) {
            goto cleanup;
        }

        if (strcmp(before_text, after_text) == 0) {
            ok = value_ssa_verify_program(program, error);
            goto cleanup;
        }
    }

    memory_ssa_pass_set_error(error,
        0,
        0,
        "MEMORY-SSA-PASS-067: memory-binary CSE did not converge within %d iterations",
        MEMORY_SSA_PASS_MEMORY_BINARY_CSE_MAX_ITERATIONS);
cleanup:
    free(before_text);
    free(after_text);
    return ok;
}

#endif
