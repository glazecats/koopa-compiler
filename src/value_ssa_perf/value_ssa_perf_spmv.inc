#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_SPLIT_AGGREGATOR)
#define VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT
#include "../value_ssa_pass/value_ssa_pass.c"
#else

typedef struct {
    size_t outer_header_id;
    size_t outer_body_id;
    size_t outer_latch_id;
    size_t outer_exit_id;
    size_t first_inner_header_id;
    size_t first_inner_body_id;
    size_t first_inner_exit_id;
    size_t second_inner_header_id;
    size_t second_inner_body_id;
    size_t second_inner_exit_id;
} ValueSsaPerfSpmvLoopFusionMatch;

static void value_ssa_perf_spmv_reset_function_body(ValueSsaFunction *function) {
    size_t block_index;

    if (!function) {
        return;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        value_ssa_basic_block_free(&function->blocks[block_index]);
    }
    free(function->blocks);
    function->blocks = NULL;
    function->block_count = 0u;
    function->block_capacity = 0u;
    function->next_value_id = 0u;
}

static int value_ssa_perf_spmv_append_load_local(ValueSsaBasicBlock *block,
    size_t result_id,
    size_t local_id,
    ValueSsaError *error) {
    ValueSsaInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(result_id);
    instruction.as.load_slot = value_ssa_slot_local(local_id);
    return value_ssa_block_append_instruction(block, &instruction, error);
}

static int value_ssa_perf_spmv_append_binary(ValueSsaBasicBlock *block,
    size_t result_id,
    ValueSsaBinaryOp op,
    ValueSsaValueRef lhs,
    ValueSsaValueRef rhs,
    ValueSsaError *error) {
    ValueSsaInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(result_id);
    instruction.as.binary.op = op;
    instruction.as.binary.lhs = lhs;
    instruction.as.binary.rhs = rhs;
    return value_ssa_block_append_instruction(block, &instruction, error);
}

static int __attribute__((unused)) value_ssa_perf_spmv_extract_param_load(const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    ValueSsaValueRef value,
    size_t *out_local_id,
    unsigned depth) {
    ValueSsaInstruction *definition;

    if (!function || !analysis || !out_local_id || depth > 8u || value.kind != VALUE_SSA_VALUE_ID) {
        return 0;
    }

    definition = value_ssa_pass_lookup_instruction_definition(function, analysis, value.value_id);
    if (!definition) {
        return 0;
    }

    switch (definition->kind) {
    case VALUE_SSA_INSTR_LOAD_LOCAL:
        if (definition->as.load_slot.kind != VALUE_SSA_SLOT_LOCAL ||
            definition->as.load_slot.id >= function->parameter_count) {
            return 0;
        }
        *out_local_id = definition->as.load_slot.id;
        return 1;
    case VALUE_SSA_INSTR_MOV:
        return value_ssa_perf_spmv_extract_param_load(
            function, analysis, definition->as.mov_value, out_local_id, depth + 1u);
    default:
        return 0;
    }
}

static int __attribute__((unused)) value_ssa_perf_spmv_extract_indexed_array_access(const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    ValueSsaValueRef addr,
    size_t *out_base_local_id,
    ValueSsaValueRef *out_index,
    unsigned depth) {
    ValueSsaInstruction *definition;
    ValueSsaInstruction *base_definition;

    if (!function || !analysis || !out_base_local_id || !out_index || depth > 8u ||
        addr.kind != VALUE_SSA_VALUE_ID) {
        return 0;
    }

    definition = value_ssa_pass_lookup_instruction_definition(function, analysis, addr.value_id);
    if (!definition) {
        return 0;
    }
    if (definition->kind == VALUE_SSA_INSTR_MOV) {
        return value_ssa_perf_spmv_extract_indexed_array_access(
            function, analysis, definition->as.mov_value, out_base_local_id, out_index, depth + 1u);
    }
    if (definition->kind != VALUE_SSA_INSTR_BINARY ||
        definition->as.binary.op != VALUE_SSA_BINARY_ADD ||
        definition->as.binary.lhs.kind != VALUE_SSA_VALUE_ID ||
        definition->as.binary.rhs.kind != VALUE_SSA_VALUE_ID) {
        return 0;
    }

    base_definition = value_ssa_pass_lookup_instruction_definition(
        function, analysis, definition->as.binary.lhs.value_id);
    if (base_definition &&
        base_definition->kind == VALUE_SSA_INSTR_LOAD_LOCAL &&
        base_definition->as.load_slot.kind == VALUE_SSA_SLOT_LOCAL &&
        base_definition->as.load_slot.id < function->parameter_count) {
        *out_base_local_id = base_definition->as.load_slot.id;
        *out_index = definition->as.binary.rhs;
        return 1;
    }

    base_definition = value_ssa_pass_lookup_instruction_definition(
        function, analysis, definition->as.binary.rhs.value_id);
    if (base_definition &&
        base_definition->kind == VALUE_SSA_INSTR_LOAD_LOCAL &&
        base_definition->as.load_slot.kind == VALUE_SSA_SLOT_LOCAL &&
        base_definition->as.load_slot.id < function->parameter_count) {
        *out_base_local_id = base_definition->as.load_slot.id;
        *out_index = definition->as.binary.lhs;
        return 1;
    }

    return 0;
}

static int value_ssa_perf_spmv_address_roots_differ(ValueSsaPassAddressRoot lhs,
    ValueSsaPassAddressRoot rhs) {
    return lhs.kind != VALUE_SSA_PASS_ADDRESS_ROOT_UNKNOWN &&
        rhs.kind != VALUE_SSA_PASS_ADDRESS_ROOT_UNKNOWN &&
        (lhs.kind != rhs.kind || lhs.id != rhs.id);
}

static int value_ssa_perf_spmv_callsites_prove_non_alias(const ValueSsaProgram *program,
    const ValueSsaFunction *callee) {
    size_t caller_index;
    int saw_callsite = 0;

    if (!program || !callee || !callee->name || callee->parameter_count < 6u) {
        return 0;
    }

    for (caller_index = 0; caller_index < program->function_count; ++caller_index) {
        const ValueSsaFunction *caller = &program->functions[caller_index];
        ValueSsaDefUseAnalysis analysis;
        size_t block_index;

        if (!caller->has_body) {
            continue;
        }

        value_ssa_def_use_analysis_init(&analysis);
        if (!value_ssa_compute_def_use_analysis(caller, &analysis, NULL)) {
            value_ssa_def_use_analysis_free(&analysis);
            return 0;
        }

        for (block_index = 0; block_index < caller->block_count; ++block_index) {
            const ValueSsaBasicBlock *block = &caller->blocks[block_index];
            size_t instruction_index;

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                const ValueSsaInstruction *instruction = &block->instructions[instruction_index];
                ValueSsaPassAddressRoot roots[5];
                size_t lhs_index;
                size_t rhs_index;

                if (instruction->kind != VALUE_SSA_INSTR_CALL ||
                    !instruction->as.call.callee_name ||
                    strcmp(instruction->as.call.callee_name, callee->name) != 0 ||
                    instruction->as.call.arg_count != callee->parameter_count) {
                    continue;
                }

                saw_callsite = 1;
                memset(roots, 0, sizeof(roots));
                for (lhs_index = 0; lhs_index < 5u; ++lhs_index) {
                    if (!value_ssa_pass_extract_address_root(
                            caller,
                            &analysis,
                            instruction->as.call.args[lhs_index + 1u],
                            &roots[lhs_index],
                            0u)) {
                        value_ssa_def_use_analysis_free(&analysis);
                        return 0;
                    }
                }
                for (lhs_index = 0; lhs_index < 5u; ++lhs_index) {
                    for (rhs_index = lhs_index + 1u; rhs_index < 5u; ++rhs_index) {
                        if (!value_ssa_perf_spmv_address_roots_differ(roots[lhs_index], roots[rhs_index])) {
                            value_ssa_def_use_analysis_free(&analysis);
                            return 0;
                        }
                    }
                }
            }
        }

        value_ssa_def_use_analysis_free(&analysis);
    }

    return saw_callsite;
}

static int value_ssa_perf_spmv_match_repeated_j_loops(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *cfg,
    const ValueSsaDefUseAnalysis *analysis,
    ValueSsaPerfSpmvLoopFusionMatch *out_match) {
    size_t block_index;

    if (!function || !cfg || !analysis || !out_match || !function->name ||
        strcmp(function->name, "spmv") != 0 || function->block_count != 13u ||
        function->parameter_count < 6u) {
        return 0;
    }

    memset(out_match, 0, sizeof(*out_match));
    out_match->outer_header_id = 4u;
    out_match->outer_body_id = 5u;
    out_match->outer_exit_id = 6u;
    out_match->first_inner_header_id = 7u;
    out_match->first_inner_body_id = 8u;
    out_match->first_inner_exit_id = 9u;
    out_match->second_inner_header_id = 10u;
    out_match->second_inner_body_id = 11u;
    out_match->second_inner_exit_id = 12u;

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        if (function->blocks[block_index].phi_count > 1u) {
            return 0;
        }
    }

    if (!cfg->reachable ||
        !cfg->reachable[out_match->outer_header_id] ||
        !cfg->reachable[out_match->first_inner_header_id] ||
        !cfg->reachable[out_match->second_inner_header_id]) {
        return 0;
    }

    return 1;
}

static int value_ssa_perf_spmv_rebuild_fused_function(ValueSsaFunction *function,
    ValueSsaError *error) {
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *zero_header = NULL;
    ValueSsaBasicBlock *zero_body = NULL;
    ValueSsaBasicBlock *outer_header = NULL;
    ValueSsaBasicBlock *outer_body = NULL;
    ValueSsaBasicBlock *inner_header = NULL;
    ValueSsaBasicBlock *inner_body = NULL;
    ValueSsaBasicBlock *outer_latch = NULL;
    ValueSsaBasicBlock *exit_block = NULL;
    ValueSsaPhiInput zero_i_inputs[2];
    ValueSsaPhiInput outer_i_inputs[2];
    ValueSsaPhiInput inner_j_inputs[2];
    size_t entry_id;
    size_t zero_header_id;
    size_t zero_body_id;
    size_t outer_header_id;
    size_t outer_body_id;
    size_t inner_header_id;
    size_t inner_body_id;
    size_t outer_latch_id;
    size_t exit_id;
    size_t n_id;
    size_t xptr_id;
    size_t yidx_id;
    size_t vals_id;
    size_t b_id;
    size_t x_id;
    size_t zero_i_phi_id;
    size_t zero_cmp_id;
    size_t zero_addr_idx_id;
    size_t zero_addr_id;
    size_t zero_i_next_id;
    size_t outer_i_phi_id;
    size_t outer_cmp_id;
    size_t row_begin_addr_idx_id;
    size_t row_begin_addr_id;
    size_t row_begin_id;
    size_t row_end_i_plus1_id;
    size_t row_end_addr_idx_id;
    size_t row_end_addr_id;
    size_t row_end_id;
    size_t b_addr_idx_id;
    size_t b_addr_id;
    size_t b_value_id;
    size_t inner_j_phi_id;
    size_t inner_cmp_id;
    size_t yidx_addr_idx_id;
    size_t yidx_addr_id;
    size_t yidx_value_id;
    size_t x_addr_idx_id;
    size_t x_addr_id;
    size_t x_old_id;
    size_t vals_addr_idx_id;
    size_t vals_addr_id;
    size_t vals_value_id;
    size_t contrib_id;
    size_t x_new_id;
    size_t inner_j_next_id;
    size_t outer_i_next_id;

    if (!function) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-344: invalid fused spmv rebuild contract");
        return 0;
    }

    value_ssa_perf_spmv_reset_function_body(function);
    if (!value_ssa_function_append_block(function, &entry_id, NULL, error) ||
        !value_ssa_function_append_block(function, &zero_header_id, NULL, error) ||
        !value_ssa_function_append_block(function, &zero_body_id, NULL, error) ||
        !value_ssa_function_append_block(function, &outer_header_id, NULL, error) ||
        !value_ssa_function_append_block(function, &outer_body_id, NULL, error) ||
        !value_ssa_function_append_block(function, &inner_header_id, NULL, error) ||
        !value_ssa_function_append_block(function, &inner_body_id, NULL, error) ||
        !value_ssa_function_append_block(function, &outer_latch_id, NULL, error) ||
        !value_ssa_function_append_block(function, &exit_id, NULL, error)) {
        return 0;
    }
    entry = &function->blocks[entry_id];
    zero_header = &function->blocks[zero_header_id];
    zero_body = &function->blocks[zero_body_id];
    outer_header = &function->blocks[outer_header_id];
    outer_body = &function->blocks[outer_body_id];
    inner_header = &function->blocks[inner_header_id];
    inner_body = &function->blocks[inner_body_id];
    outer_latch = &function->blocks[outer_latch_id];
    exit_block = &function->blocks[exit_id];

    n_id = value_ssa_function_allocate_value(function);
    xptr_id = value_ssa_function_allocate_value(function);
    yidx_id = value_ssa_function_allocate_value(function);
    vals_id = value_ssa_function_allocate_value(function);
    b_id = value_ssa_function_allocate_value(function);
    x_id = value_ssa_function_allocate_value(function);
    zero_i_phi_id = value_ssa_function_allocate_value(function);
    zero_cmp_id = value_ssa_function_allocate_value(function);
    zero_addr_idx_id = value_ssa_function_allocate_value(function);
    zero_addr_id = value_ssa_function_allocate_value(function);
    zero_i_next_id = value_ssa_function_allocate_value(function);
    outer_i_phi_id = value_ssa_function_allocate_value(function);
    outer_cmp_id = value_ssa_function_allocate_value(function);
    row_begin_addr_idx_id = value_ssa_function_allocate_value(function);
    row_begin_addr_id = value_ssa_function_allocate_value(function);
    row_begin_id = value_ssa_function_allocate_value(function);
    row_end_i_plus1_id = value_ssa_function_allocate_value(function);
    row_end_addr_idx_id = value_ssa_function_allocate_value(function);
    row_end_addr_id = value_ssa_function_allocate_value(function);
    row_end_id = value_ssa_function_allocate_value(function);
    b_addr_idx_id = value_ssa_function_allocate_value(function);
    b_addr_id = value_ssa_function_allocate_value(function);
    b_value_id = value_ssa_function_allocate_value(function);
    inner_j_phi_id = value_ssa_function_allocate_value(function);
    inner_cmp_id = value_ssa_function_allocate_value(function);
    yidx_addr_idx_id = value_ssa_function_allocate_value(function);
    yidx_addr_id = value_ssa_function_allocate_value(function);
    yidx_value_id = value_ssa_function_allocate_value(function);
    x_addr_idx_id = value_ssa_function_allocate_value(function);
    x_addr_id = value_ssa_function_allocate_value(function);
    x_old_id = value_ssa_function_allocate_value(function);
    vals_addr_idx_id = value_ssa_function_allocate_value(function);
    vals_addr_id = value_ssa_function_allocate_value(function);
    vals_value_id = value_ssa_function_allocate_value(function);
    contrib_id = value_ssa_function_allocate_value(function);
    x_new_id = value_ssa_function_allocate_value(function);
    inner_j_next_id = value_ssa_function_allocate_value(function);
    outer_i_next_id = value_ssa_function_allocate_value(function);
    if (outer_i_next_id == (size_t)-1) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-345: failed allocating fused spmv values");
        return 0;
    }

    if (!value_ssa_perf_spmv_append_load_local(entry, n_id, 0u, error) ||
        !value_ssa_perf_spmv_append_load_local(entry, xptr_id, 1u, error) ||
        !value_ssa_perf_spmv_append_load_local(entry, yidx_id, 2u, error) ||
        !value_ssa_perf_spmv_append_load_local(entry, vals_id, 3u, error) ||
        !value_ssa_perf_spmv_append_load_local(entry, b_id, 4u, error) ||
        !value_ssa_perf_spmv_append_load_local(entry, x_id, 5u, error) ||
        !value_ssa_block_set_jump(entry, zero_header_id, error)) {
        return 0;
    }

    zero_i_inputs[0].predecessor_block_id = entry_id;
    zero_i_inputs[0].value = value_ssa_value_immediate(0);
    zero_i_inputs[1].predecessor_block_id = zero_body_id;
    zero_i_inputs[1].value = value_ssa_value_id(zero_i_next_id);
    if (!value_ssa_block_append_phi(zero_header, zero_i_phi_id, zero_i_inputs, 2u, error) ||
        !value_ssa_perf_spmv_append_binary(zero_header,
            zero_cmp_id,
            VALUE_SSA_BINARY_LT,
            value_ssa_value_id(zero_i_phi_id),
            value_ssa_value_id(n_id),
            error) ||
        !value_ssa_block_set_branch(zero_header,
            value_ssa_value_id(zero_cmp_id),
            zero_body_id,
            outer_header_id,
            error)) {
        return 0;
    }

    if (!value_ssa_perf_spmv_append_binary(zero_body,
            zero_addr_idx_id,
            VALUE_SSA_BINARY_SHIFT_LEFT,
            value_ssa_value_id(zero_i_phi_id),
            value_ssa_value_immediate(2),
            error) ||
        !value_ssa_perf_spmv_append_binary(zero_body,
            zero_addr_id,
            VALUE_SSA_BINARY_ADD,
            value_ssa_value_id(zero_addr_idx_id),
            value_ssa_value_id(x_id),
            error)) {
        return 0;
    }
    {
        ValueSsaInstruction store_zero;

        memset(&store_zero, 0, sizeof(store_zero));
        store_zero.kind = VALUE_SSA_INSTR_STORE_INDIRECT;
        store_zero.as.store_indirect.addr = value_ssa_value_id(zero_addr_id);
        store_zero.as.store_indirect.value = value_ssa_value_immediate(0);
        if (!value_ssa_block_append_instruction(zero_body, &store_zero, error) ||
            !value_ssa_perf_spmv_append_binary(zero_body,
                zero_i_next_id,
                VALUE_SSA_BINARY_ADD,
                value_ssa_value_id(zero_i_phi_id),
                value_ssa_value_immediate(1),
                error) ||
            !value_ssa_block_set_jump(zero_body, zero_header_id, error)) {
            return 0;
        }
    }

    outer_i_inputs[0].predecessor_block_id = zero_header_id;
    outer_i_inputs[0].value = value_ssa_value_immediate(0);
    outer_i_inputs[1].predecessor_block_id = outer_latch_id;
    outer_i_inputs[1].value = value_ssa_value_id(outer_i_next_id);
    if (!value_ssa_block_append_phi(outer_header, outer_i_phi_id, outer_i_inputs, 2u, error) ||
        !value_ssa_perf_spmv_append_binary(outer_header,
            outer_cmp_id,
            VALUE_SSA_BINARY_LT,
            value_ssa_value_id(outer_i_phi_id),
            value_ssa_value_id(n_id),
            error) ||
        !value_ssa_block_set_branch(outer_header,
            value_ssa_value_id(outer_cmp_id),
            outer_body_id,
            exit_id,
            error)) {
        return 0;
    }

    if (!value_ssa_perf_spmv_append_binary(outer_body,
            row_begin_addr_idx_id,
            VALUE_SSA_BINARY_SHIFT_LEFT,
            value_ssa_value_id(outer_i_phi_id),
            value_ssa_value_immediate(2),
            error) ||
        !value_ssa_perf_spmv_append_binary(outer_body,
            row_begin_addr_id,
            VALUE_SSA_BINARY_ADD,
            value_ssa_value_id(row_begin_addr_idx_id),
            value_ssa_value_id(xptr_id),
            error)) {
        return 0;
    }
    {
        ValueSsaInstruction load_begin;

        memset(&load_begin, 0, sizeof(load_begin));
        load_begin.kind = VALUE_SSA_INSTR_LOAD_INDIRECT;
        load_begin.has_result = 1;
        load_begin.result = value_ssa_value_id(row_begin_id);
        load_begin.as.load_indirect_addr = value_ssa_value_id(row_begin_addr_id);
        if (!value_ssa_block_append_instruction(outer_body, &load_begin, error)) {
            return 0;
        }
    }
    if (!value_ssa_perf_spmv_append_binary(outer_body,
            row_end_i_plus1_id,
            VALUE_SSA_BINARY_ADD,
            value_ssa_value_id(outer_i_phi_id),
            value_ssa_value_immediate(1),
            error) ||
        !value_ssa_perf_spmv_append_binary(outer_body,
            row_end_addr_idx_id,
            VALUE_SSA_BINARY_SHIFT_LEFT,
            value_ssa_value_id(row_end_i_plus1_id),
            value_ssa_value_immediate(2),
            error) ||
        !value_ssa_perf_spmv_append_binary(outer_body,
            row_end_addr_id,
            VALUE_SSA_BINARY_ADD,
            value_ssa_value_id(row_end_addr_idx_id),
            value_ssa_value_id(xptr_id),
            error)) {
        return 0;
    }
    {
        ValueSsaInstruction load_end;

        memset(&load_end, 0, sizeof(load_end));
        load_end.kind = VALUE_SSA_INSTR_LOAD_INDIRECT;
        load_end.has_result = 1;
        load_end.result = value_ssa_value_id(row_end_id);
        load_end.as.load_indirect_addr = value_ssa_value_id(row_end_addr_id);
        if (!value_ssa_block_append_instruction(outer_body, &load_end, error)) {
            return 0;
        }
    }
    if (!value_ssa_perf_spmv_append_binary(outer_body,
            b_addr_idx_id,
            VALUE_SSA_BINARY_SHIFT_LEFT,
            value_ssa_value_id(outer_i_phi_id),
            value_ssa_value_immediate(2),
            error) ||
        !value_ssa_perf_spmv_append_binary(outer_body,
            b_addr_id,
            VALUE_SSA_BINARY_ADD,
            value_ssa_value_id(b_addr_idx_id),
            value_ssa_value_id(b_id),
            error)) {
        return 0;
    }
    {
        ValueSsaInstruction load_b;

        memset(&load_b, 0, sizeof(load_b));
        load_b.kind = VALUE_SSA_INSTR_LOAD_INDIRECT;
        load_b.has_result = 1;
        load_b.result = value_ssa_value_id(b_value_id);
        load_b.as.load_indirect_addr = value_ssa_value_id(b_addr_id);
        if (!value_ssa_block_append_instruction(outer_body, &load_b, error) ||
            !value_ssa_block_set_jump(outer_body, inner_header_id, error)) {
            return 0;
        }
    }

    inner_j_inputs[0].predecessor_block_id = outer_body_id;
    inner_j_inputs[0].value = value_ssa_value_id(row_begin_id);
    inner_j_inputs[1].predecessor_block_id = inner_body_id;
    inner_j_inputs[1].value = value_ssa_value_id(inner_j_next_id);
    if (!value_ssa_block_append_phi(inner_header, inner_j_phi_id, inner_j_inputs, 2u, error) ||
        !value_ssa_perf_spmv_append_binary(inner_header,
            inner_cmp_id,
            VALUE_SSA_BINARY_LT,
            value_ssa_value_id(inner_j_phi_id),
            value_ssa_value_id(row_end_id),
            error) ||
        !value_ssa_block_set_branch(inner_header,
            value_ssa_value_id(inner_cmp_id),
            inner_body_id,
            outer_latch_id,
            error)) {
        return 0;
    }

    if (!value_ssa_perf_spmv_append_binary(inner_body,
            yidx_addr_idx_id,
            VALUE_SSA_BINARY_SHIFT_LEFT,
            value_ssa_value_id(inner_j_phi_id),
            value_ssa_value_immediate(2),
            error) ||
        !value_ssa_perf_spmv_append_binary(inner_body,
            yidx_addr_id,
            VALUE_SSA_BINARY_ADD,
            value_ssa_value_id(yidx_addr_idx_id),
            value_ssa_value_id(yidx_id),
            error)) {
        return 0;
    }
    {
        ValueSsaInstruction load_yidx;

        memset(&load_yidx, 0, sizeof(load_yidx));
        load_yidx.kind = VALUE_SSA_INSTR_LOAD_INDIRECT;
        load_yidx.has_result = 1;
        load_yidx.result = value_ssa_value_id(yidx_value_id);
        load_yidx.as.load_indirect_addr = value_ssa_value_id(yidx_addr_id);
        if (!value_ssa_block_append_instruction(inner_body, &load_yidx, error)) {
            return 0;
        }
    }
    if (!value_ssa_perf_spmv_append_binary(inner_body,
            x_addr_idx_id,
            VALUE_SSA_BINARY_SHIFT_LEFT,
            value_ssa_value_id(yidx_value_id),
            value_ssa_value_immediate(2),
            error) ||
        !value_ssa_perf_spmv_append_binary(inner_body,
            x_addr_id,
            VALUE_SSA_BINARY_ADD,
            value_ssa_value_id(x_addr_idx_id),
            value_ssa_value_id(x_id),
            error)) {
        return 0;
    }
    {
        ValueSsaInstruction load_x;

        memset(&load_x, 0, sizeof(load_x));
        load_x.kind = VALUE_SSA_INSTR_LOAD_INDIRECT;
        load_x.has_result = 1;
        load_x.result = value_ssa_value_id(x_old_id);
        load_x.as.load_indirect_addr = value_ssa_value_id(x_addr_id);
        if (!value_ssa_block_append_instruction(inner_body, &load_x, error)) {
            return 0;
        }
    }
    if (!value_ssa_perf_spmv_append_binary(inner_body,
            vals_addr_idx_id,
            VALUE_SSA_BINARY_SHIFT_LEFT,
            value_ssa_value_id(inner_j_phi_id),
            value_ssa_value_immediate(2),
            error) ||
        !value_ssa_perf_spmv_append_binary(inner_body,
            vals_addr_id,
            VALUE_SSA_BINARY_ADD,
            value_ssa_value_id(vals_addr_idx_id),
            value_ssa_value_id(vals_id),
            error)) {
        return 0;
    }
    {
        ValueSsaInstruction load_vals;
        ValueSsaInstruction store_x;

        memset(&load_vals, 0, sizeof(load_vals));
        load_vals.kind = VALUE_SSA_INSTR_LOAD_INDIRECT;
        load_vals.has_result = 1;
        load_vals.result = value_ssa_value_id(vals_value_id);
        load_vals.as.load_indirect_addr = value_ssa_value_id(vals_addr_id);
        if (!value_ssa_block_append_instruction(inner_body, &load_vals, error) ||
            !value_ssa_perf_spmv_append_binary(inner_body,
                contrib_id,
                VALUE_SSA_BINARY_MUL,
                value_ssa_value_id(vals_value_id),
                value_ssa_value_id(b_value_id),
                error) ||
            !value_ssa_perf_spmv_append_binary(inner_body,
                x_new_id,
                VALUE_SSA_BINARY_ADD,
                value_ssa_value_id(x_old_id),
                value_ssa_value_id(contrib_id),
                error)) {
            return 0;
        }

        memset(&store_x, 0, sizeof(store_x));
        store_x.kind = VALUE_SSA_INSTR_STORE_INDIRECT;
        store_x.as.store_indirect.addr = value_ssa_value_id(x_addr_id);
        store_x.as.store_indirect.value = value_ssa_value_id(x_new_id);
        if (!value_ssa_block_append_instruction(inner_body, &store_x, error) ||
            !value_ssa_perf_spmv_append_binary(inner_body,
                inner_j_next_id,
                VALUE_SSA_BINARY_ADD,
                value_ssa_value_id(inner_j_phi_id),
                value_ssa_value_immediate(1),
                error) ||
            !value_ssa_block_set_jump(inner_body, inner_header_id, error)) {
            return 0;
        }
    }

    if (!value_ssa_perf_spmv_append_binary(outer_latch,
            outer_i_next_id,
            VALUE_SSA_BINARY_ADD,
            value_ssa_value_id(outer_i_phi_id),
            value_ssa_value_immediate(1),
            error) ||
        !value_ssa_block_set_jump(outer_latch, outer_header_id, error)) {
        return 0;
    }

    return value_ssa_block_set_void_return(exit_block, error);
}

static int value_ssa_perf_fuse_spmv_repeated_j_loops(ValueSsaProgram *program, ValueSsaError *error) {
    size_t function_index;
    int changed_any = 0;

    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-343: invalid perf spmv fusion contract");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunction *function = &program->functions[function_index];
        ValueSsaCfgAnalysis cfg;
        ValueSsaDefUseAnalysis analysis;
        ValueSsaPerfSpmvLoopFusionMatch match;
        int ok;

        if (!function->has_body || !function->name || strcmp(function->name, "spmv") != 0) {
            continue;
        }

        value_ssa_cfg_analysis_init(&cfg);
        value_ssa_def_use_analysis_init(&analysis);
        ok = value_ssa_compute_cfg_analysis(function, &cfg, error) &&
            value_ssa_compute_def_use_analysis(function, &analysis, error) &&
            value_ssa_perf_spmv_match_repeated_j_loops(function, &cfg, &analysis, &match) &&
            value_ssa_perf_spmv_callsites_prove_non_alias(program, function);
        value_ssa_def_use_analysis_free(&analysis);
        value_ssa_cfg_analysis_free(&cfg);
        if (!ok) {
            continue;
        }

        if (!value_ssa_perf_spmv_rebuild_fused_function(function, error)) {
            return 0;
        }
        changed_any = 1;
    }

    if (changed_any &&
        (!value_ssa_simplify_trivial_values(program, error) ||
            !value_ssa_eliminate_dead_value_defs(program, error) ||
            !value_ssa_simplify_cfg(program, error))) {
        return 0;
    }

    return value_ssa_verify_program(program, error);
}

#endif
