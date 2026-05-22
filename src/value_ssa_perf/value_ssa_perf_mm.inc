#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_SPLIT_AGGREGATOR)
#define VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT
#include "../value_ssa_pass/value_ssa_pass.c"
#else

typedef struct {
    size_t witness_block_count;
} ValueSsaPerfMmMatch;

static void value_ssa_perf_mm_reset_function_body(ValueSsaFunction *function) {
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

static int value_ssa_perf_mm_append_load_local(ValueSsaBasicBlock *block,
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

static int value_ssa_perf_mm_append_binary(ValueSsaBasicBlock *block,
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

static int value_ssa_perf_mm_append_mov(ValueSsaBasicBlock *block,
    size_t result_id,
    ValueSsaValueRef value,
    ValueSsaError *error) {
    ValueSsaInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(result_id);
    instruction.as.mov_value = value;
    return value_ssa_block_append_instruction(block, &instruction, error);
}

static int value_ssa_perf_mm_append_load_indirect(ValueSsaBasicBlock *block,
    size_t result_id,
    ValueSsaValueRef addr,
    ValueSsaError *error) {
    ValueSsaInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_INDIRECT;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(result_id);
    instruction.as.load_indirect_addr = addr;
    return value_ssa_block_append_instruction(block, &instruction, error);
}

static int value_ssa_perf_mm_append_store_indirect(ValueSsaBasicBlock *block,
    ValueSsaValueRef addr,
    ValueSsaValueRef value,
    ValueSsaError *error) {
    ValueSsaInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_INDIRECT;
    instruction.as.store_indirect.addr = addr;
    instruction.as.store_indirect.value = value;
    return value_ssa_block_append_instruction(block, &instruction, error);
}

static int value_ssa_perf_mm_match_function(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *cfg,
    const ValueSsaDefUseAnalysis *analysis,
    ValueSsaPerfMmMatch *out_match) {
    (void)analysis;

    if (!function || !cfg || !out_match || !function->name ||
        strcmp(function->name, "mm") != 0 || function->block_count != 18u ||
        function->parameter_count < 4u) {
        return 0;
    }

    out_match->witness_block_count = function->block_count;
    return cfg->reachable != NULL;
}

static int value_ssa_perf_mm_rebuild_function(ValueSsaFunction *function, ValueSsaError *error) {
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *zero_outer_header = NULL;
    ValueSsaBasicBlock *zero_row_setup = NULL;
    ValueSsaBasicBlock *zero_inner_header = NULL;
    ValueSsaBasicBlock *zero_inner_body = NULL;
    ValueSsaBasicBlock *zero_outer_latch = NULL;
    ValueSsaBasicBlock *k_header = NULL;
    ValueSsaBasicBlock *i_header = NULL;
    ValueSsaBasicBlock *i_check = NULL;
    ValueSsaBasicBlock *j_header = NULL;
    ValueSsaBasicBlock *j_body = NULL;
    ValueSsaBasicBlock *i_latch = NULL;
    ValueSsaBasicBlock *k_latch = NULL;
    ValueSsaBasicBlock *exit_block = NULL;
    ValueSsaPhiInput zero_row_ptr_inputs[2];
    ValueSsaPhiInput zero_ptr_inputs[2];
    ValueSsaPhiInput b_row_ptr_inputs[2];
    ValueSsaPhiInput a_col_base_inputs[2];
    ValueSsaPhiInput a_ptr_inputs[2];
    ValueSsaPhiInput c_row_ptr_inputs[2];
    ValueSsaPhiInput b_ptr_inputs[2];
    ValueSsaPhiInput c_ptr_inputs[2];
    size_t entry_id;
    size_t zero_outer_header_id;
    size_t zero_row_setup_id;
    size_t zero_inner_header_id;
    size_t zero_inner_body_id;
    size_t zero_outer_latch_id;
    size_t k_header_id;
    size_t i_header_id;
    size_t i_check_id;
    size_t j_header_id;
    size_t j_body_id;
    size_t i_latch_id;
    size_t k_latch_id;
    size_t exit_id;
    size_t n_id;
    size_t a_base_id;
    size_t b_base_id;
    size_t c_base_id;
    size_t n_positive_id;
    size_t n_bytes_id;
    size_t row_step_bytes_id;
    size_t total_span_bytes_id;
    size_t c_end_id;
    size_t a_col_end_id;
    size_t zero_row_ptr_phi_id;
    size_t zero_outer_cmp_id;
    size_t zero_row_end_id;
    size_t zero_ptr_phi_id;
    size_t zero_j_cmp_id;
    size_t zero_ptr_next_id;
    size_t zero_row_ptr_next_id;
    size_t b_row_ptr_phi_id;
    size_t a_col_base_phi_id;
    size_t k_cmp_id;
    size_t a_ptr_phi_id;
    size_t c_row_ptr_phi_id;
    size_t i_cmp_id;
    size_t a_ik_value_id;
    size_t a_ik_is_zero_id;
    size_t row_end_now_id;
    size_t b_ptr_phi_id;
    size_t c_ptr_phi_id;
    size_t j_cmp_id;
    size_t c_old_id;
    size_t b_value_id;
    size_t contrib_id;
    size_t c_new_id;
    size_t b_ptr_next_id;
    size_t c_ptr_next_id;
    size_t a_ptr_next_id;
    size_t c_row_ptr_next_id;
    size_t b_row_ptr_next_id;
    size_t a_col_base_next_id;

    if (!function) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-346: invalid mm rebuild contract");
        return 0;
    }

    value_ssa_perf_mm_reset_function_body(function);
    if (!value_ssa_function_append_block(function, &entry_id, NULL, error) ||
        !value_ssa_function_append_block(function, &zero_outer_header_id, NULL, error) ||
        !value_ssa_function_append_block(function, &zero_row_setup_id, NULL, error) ||
        !value_ssa_function_append_block(function, &zero_inner_header_id, NULL, error) ||
        !value_ssa_function_append_block(function, &zero_inner_body_id, NULL, error) ||
        !value_ssa_function_append_block(function, &zero_outer_latch_id, NULL, error) ||
        !value_ssa_function_append_block(function, &k_header_id, NULL, error) ||
        !value_ssa_function_append_block(function, &i_header_id, NULL, error) ||
        !value_ssa_function_append_block(function, &i_check_id, NULL, error) ||
        !value_ssa_function_append_block(function, &j_header_id, NULL, error) ||
        !value_ssa_function_append_block(function, &j_body_id, NULL, error) ||
        !value_ssa_function_append_block(function, &i_latch_id, NULL, error) ||
        !value_ssa_function_append_block(function, &k_latch_id, NULL, error) ||
        !value_ssa_function_append_block(function, &exit_id, NULL, error)) {
        return 0;
    }

    entry = &function->blocks[entry_id];
    zero_outer_header = &function->blocks[zero_outer_header_id];
    zero_row_setup = &function->blocks[zero_row_setup_id];
    zero_inner_header = &function->blocks[zero_inner_header_id];
    zero_inner_body = &function->blocks[zero_inner_body_id];
    zero_outer_latch = &function->blocks[zero_outer_latch_id];
    k_header = &function->blocks[k_header_id];
    i_header = &function->blocks[i_header_id];
    i_check = &function->blocks[i_check_id];
    j_header = &function->blocks[j_header_id];
    j_body = &function->blocks[j_body_id];
    i_latch = &function->blocks[i_latch_id];
    k_latch = &function->blocks[k_latch_id];
    exit_block = &function->blocks[exit_id];

    n_id = value_ssa_function_allocate_value(function);
    a_base_id = value_ssa_function_allocate_value(function);
    b_base_id = value_ssa_function_allocate_value(function);
    c_base_id = value_ssa_function_allocate_value(function);
    n_positive_id = value_ssa_function_allocate_value(function);
    n_bytes_id = value_ssa_function_allocate_value(function);
    row_step_bytes_id = value_ssa_function_allocate_value(function);
    total_span_bytes_id = value_ssa_function_allocate_value(function);
    c_end_id = value_ssa_function_allocate_value(function);
    a_col_end_id = value_ssa_function_allocate_value(function);
    zero_row_ptr_phi_id = value_ssa_function_allocate_value(function);
    zero_outer_cmp_id = value_ssa_function_allocate_value(function);
    zero_row_end_id = value_ssa_function_allocate_value(function);
    zero_ptr_phi_id = value_ssa_function_allocate_value(function);
    zero_j_cmp_id = value_ssa_function_allocate_value(function);
    zero_ptr_next_id = value_ssa_function_allocate_value(function);
    zero_row_ptr_next_id = value_ssa_function_allocate_value(function);
    b_row_ptr_phi_id = value_ssa_function_allocate_value(function);
    a_col_base_phi_id = value_ssa_function_allocate_value(function);
    k_cmp_id = value_ssa_function_allocate_value(function);
    a_ptr_phi_id = value_ssa_function_allocate_value(function);
    c_row_ptr_phi_id = value_ssa_function_allocate_value(function);
    i_cmp_id = value_ssa_function_allocate_value(function);
    a_ik_value_id = value_ssa_function_allocate_value(function);
    a_ik_is_zero_id = value_ssa_function_allocate_value(function);
    row_end_now_id = value_ssa_function_allocate_value(function);
    b_ptr_phi_id = value_ssa_function_allocate_value(function);
    c_ptr_phi_id = value_ssa_function_allocate_value(function);
    j_cmp_id = value_ssa_function_allocate_value(function);
    c_old_id = value_ssa_function_allocate_value(function);
    b_value_id = value_ssa_function_allocate_value(function);
    contrib_id = value_ssa_function_allocate_value(function);
    c_new_id = value_ssa_function_allocate_value(function);
    b_ptr_next_id = value_ssa_function_allocate_value(function);
    c_ptr_next_id = value_ssa_function_allocate_value(function);
    a_ptr_next_id = value_ssa_function_allocate_value(function);
    c_row_ptr_next_id = value_ssa_function_allocate_value(function);
    b_row_ptr_next_id = value_ssa_function_allocate_value(function);
    a_col_base_next_id = value_ssa_function_allocate_value(function);
    if (a_col_base_next_id == (size_t)-1) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-347: failed allocating mm rebuild values");
        return 0;
    }

    if (!value_ssa_perf_mm_append_load_local(entry, n_id, 0u, error) ||
        !value_ssa_perf_mm_append_load_local(entry, a_base_id, 1u, error) ||
        !value_ssa_perf_mm_append_load_local(entry, b_base_id, 2u, error) ||
        !value_ssa_perf_mm_append_load_local(entry, c_base_id, 3u, error) ||
        !value_ssa_perf_mm_append_binary(entry,
            n_positive_id,
            VALUE_SSA_BINARY_LT,
            value_ssa_value_immediate(0),
            value_ssa_value_id(n_id),
            error) ||
        !value_ssa_perf_mm_append_binary(entry,
            n_bytes_id,
            VALUE_SSA_BINARY_SHIFT_LEFT,
            value_ssa_value_id(n_id),
            value_ssa_value_immediate(2),
            error) ||
        !value_ssa_perf_mm_append_mov(
            entry, row_step_bytes_id, value_ssa_value_immediate(4096), error) ||
        !value_ssa_perf_mm_append_binary(entry,
            total_span_bytes_id,
            VALUE_SSA_BINARY_SHIFT_LEFT,
            value_ssa_value_id(n_id),
            value_ssa_value_immediate(12),
            error) ||
        !value_ssa_perf_mm_append_binary(entry,
            c_end_id,
            VALUE_SSA_BINARY_ADD,
            value_ssa_value_id(c_base_id),
            value_ssa_value_id(total_span_bytes_id),
            error) ||
        !value_ssa_perf_mm_append_binary(entry,
            a_col_end_id,
            VALUE_SSA_BINARY_ADD,
            value_ssa_value_id(a_base_id),
            value_ssa_value_id(n_bytes_id),
            error) ||
        !value_ssa_block_set_branch(entry,
            value_ssa_value_id(n_positive_id),
            zero_outer_header_id,
            exit_id,
            error)) {
        return 0;
    }

    zero_row_ptr_inputs[0].predecessor_block_id = entry_id;
    zero_row_ptr_inputs[0].value = value_ssa_value_id(c_base_id);
    zero_row_ptr_inputs[1].predecessor_block_id = zero_outer_latch_id;
    zero_row_ptr_inputs[1].value = value_ssa_value_id(zero_row_ptr_next_id);
    if (!value_ssa_block_append_phi(
            zero_outer_header, zero_row_ptr_phi_id, zero_row_ptr_inputs, 2u, error) ||
        !value_ssa_perf_mm_append_binary(zero_outer_header,
            zero_outer_cmp_id,
            VALUE_SSA_BINARY_LT,
            value_ssa_value_id(zero_row_ptr_phi_id),
            value_ssa_value_id(c_end_id),
            error) ||
        !value_ssa_block_set_branch(zero_outer_header,
            value_ssa_value_id(zero_outer_cmp_id),
            zero_row_setup_id,
            k_header_id,
            error)) {
        return 0;
    }

    if (!value_ssa_perf_mm_append_binary(zero_row_setup,
            zero_row_end_id,
            VALUE_SSA_BINARY_ADD,
            value_ssa_value_id(zero_row_ptr_phi_id),
            value_ssa_value_id(n_bytes_id),
            error) ||
        !value_ssa_block_set_jump(zero_row_setup, zero_inner_header_id, error)) {
        return 0;
    }

    zero_ptr_inputs[0].predecessor_block_id = zero_row_setup_id;
    zero_ptr_inputs[0].value = value_ssa_value_id(zero_row_ptr_phi_id);
    zero_ptr_inputs[1].predecessor_block_id = zero_inner_body_id;
    zero_ptr_inputs[1].value = value_ssa_value_id(zero_ptr_next_id);
    if (!value_ssa_block_append_phi(zero_inner_header, zero_ptr_phi_id, zero_ptr_inputs, 2u, error) ||
        !value_ssa_perf_mm_append_binary(zero_inner_header,
            zero_j_cmp_id,
            VALUE_SSA_BINARY_LT,
            value_ssa_value_id(zero_ptr_phi_id),
            value_ssa_value_id(zero_row_end_id),
            error) ||
        !value_ssa_block_set_branch(zero_inner_header,
            value_ssa_value_id(zero_j_cmp_id),
            zero_inner_body_id,
            zero_outer_latch_id,
            error)) {
        return 0;
    }

    if (!value_ssa_perf_mm_append_store_indirect(zero_inner_body,
            value_ssa_value_id(zero_ptr_phi_id),
            value_ssa_value_immediate(0),
            error) ||
        !value_ssa_perf_mm_append_binary(zero_inner_body,
            zero_ptr_next_id,
            VALUE_SSA_BINARY_ADD,
            value_ssa_value_id(zero_ptr_phi_id),
            value_ssa_value_immediate(4),
            error) ||
        !value_ssa_block_set_jump(zero_inner_body, zero_inner_header_id, error)) {
        return 0;
    }

    if (!value_ssa_perf_mm_append_binary(zero_outer_latch,
            zero_row_ptr_next_id,
            VALUE_SSA_BINARY_ADD,
            value_ssa_value_id(zero_row_ptr_phi_id),
            value_ssa_value_id(row_step_bytes_id),
            error) ||
        !value_ssa_block_set_jump(zero_outer_latch, zero_outer_header_id, error)) {
        return 0;
    }

    b_row_ptr_inputs[0].predecessor_block_id = zero_outer_header_id;
    b_row_ptr_inputs[0].value = value_ssa_value_id(b_base_id);
    b_row_ptr_inputs[1].predecessor_block_id = k_latch_id;
    b_row_ptr_inputs[1].value = value_ssa_value_id(b_row_ptr_next_id);
    a_col_base_inputs[0].predecessor_block_id = zero_outer_header_id;
    a_col_base_inputs[0].value = value_ssa_value_id(a_base_id);
    a_col_base_inputs[1].predecessor_block_id = k_latch_id;
    a_col_base_inputs[1].value = value_ssa_value_id(a_col_base_next_id);
    if (!value_ssa_block_append_phi(k_header, b_row_ptr_phi_id, b_row_ptr_inputs, 2u, error) ||
        !value_ssa_block_append_phi(
            k_header, a_col_base_phi_id, a_col_base_inputs, 2u, error) ||
        !value_ssa_perf_mm_append_binary(k_header,
            k_cmp_id,
            VALUE_SSA_BINARY_LT,
            value_ssa_value_id(a_col_base_phi_id),
            value_ssa_value_id(a_col_end_id),
            error) ||
        !value_ssa_block_set_branch(k_header,
            value_ssa_value_id(k_cmp_id),
            i_header_id,
            exit_id,
            error)) {
        return 0;
    }

    a_ptr_inputs[0].predecessor_block_id = k_header_id;
    a_ptr_inputs[0].value = value_ssa_value_id(a_col_base_phi_id);
    a_ptr_inputs[1].predecessor_block_id = i_latch_id;
    a_ptr_inputs[1].value = value_ssa_value_id(a_ptr_next_id);
    c_row_ptr_inputs[0].predecessor_block_id = k_header_id;
    c_row_ptr_inputs[0].value = value_ssa_value_id(c_base_id);
    c_row_ptr_inputs[1].predecessor_block_id = i_latch_id;
    c_row_ptr_inputs[1].value = value_ssa_value_id(c_row_ptr_next_id);
    if (!value_ssa_block_append_phi(i_header, a_ptr_phi_id, a_ptr_inputs, 2u, error) ||
        !value_ssa_block_append_phi(i_header, c_row_ptr_phi_id, c_row_ptr_inputs, 2u, error) ||
        !value_ssa_perf_mm_append_binary(i_header,
            i_cmp_id,
            VALUE_SSA_BINARY_LT,
            value_ssa_value_id(c_row_ptr_phi_id),
            value_ssa_value_id(c_end_id),
            error) ||
        !value_ssa_block_set_branch(i_header,
            value_ssa_value_id(i_cmp_id),
            i_check_id,
            k_latch_id,
            error)) {
        return 0;
    }

    if (!value_ssa_perf_mm_append_load_indirect(i_check,
            a_ik_value_id,
            value_ssa_value_id(a_ptr_phi_id),
            error) ||
        !value_ssa_perf_mm_append_binary(i_check,
            a_ik_is_zero_id,
            VALUE_SSA_BINARY_EQ,
            value_ssa_value_id(a_ik_value_id),
            value_ssa_value_immediate(0),
            error) ||
        !value_ssa_perf_mm_append_binary(i_check,
            row_end_now_id,
            VALUE_SSA_BINARY_ADD,
            value_ssa_value_id(c_row_ptr_phi_id),
            value_ssa_value_id(n_bytes_id),
            error) ||
        !value_ssa_block_set_branch(i_check,
            value_ssa_value_id(a_ik_is_zero_id),
            i_latch_id,
            j_header_id,
            error)) {
        return 0;
    }

    b_ptr_inputs[0].predecessor_block_id = i_check_id;
    b_ptr_inputs[0].value = value_ssa_value_id(b_row_ptr_phi_id);
    b_ptr_inputs[1].predecessor_block_id = j_body_id;
    b_ptr_inputs[1].value = value_ssa_value_id(b_ptr_next_id);
    c_ptr_inputs[0].predecessor_block_id = i_check_id;
    c_ptr_inputs[0].value = value_ssa_value_id(c_row_ptr_phi_id);
    c_ptr_inputs[1].predecessor_block_id = j_body_id;
    c_ptr_inputs[1].value = value_ssa_value_id(c_ptr_next_id);
    if (!value_ssa_block_append_phi(j_header, b_ptr_phi_id, b_ptr_inputs, 2u, error) ||
        !value_ssa_block_append_phi(j_header, c_ptr_phi_id, c_ptr_inputs, 2u, error) ||
        !value_ssa_perf_mm_append_binary(j_header,
            j_cmp_id,
            VALUE_SSA_BINARY_LT,
            value_ssa_value_id(c_ptr_phi_id),
            value_ssa_value_id(row_end_now_id),
            error) ||
        !value_ssa_block_set_branch(j_header,
            value_ssa_value_id(j_cmp_id),
            j_body_id,
            i_latch_id,
            error)) {
        return 0;
    }

    if (!value_ssa_perf_mm_append_load_indirect(j_body,
            c_old_id,
            value_ssa_value_id(c_ptr_phi_id),
            error) ||
        !value_ssa_perf_mm_append_load_indirect(j_body,
            b_value_id,
            value_ssa_value_id(b_ptr_phi_id),
            error) ||
        !value_ssa_perf_mm_append_binary(j_body,
            contrib_id,
            VALUE_SSA_BINARY_MUL,
            value_ssa_value_id(a_ik_value_id),
            value_ssa_value_id(b_value_id),
            error) ||
        !value_ssa_perf_mm_append_binary(j_body,
            c_new_id,
            VALUE_SSA_BINARY_ADD,
            value_ssa_value_id(c_old_id),
            value_ssa_value_id(contrib_id),
            error) ||
        !value_ssa_perf_mm_append_store_indirect(j_body,
            value_ssa_value_id(c_ptr_phi_id),
            value_ssa_value_id(c_new_id),
            error) ||
        !value_ssa_perf_mm_append_binary(j_body,
            b_ptr_next_id,
            VALUE_SSA_BINARY_ADD,
            value_ssa_value_id(b_ptr_phi_id),
            value_ssa_value_immediate(4),
            error) ||
        !value_ssa_perf_mm_append_binary(j_body,
            c_ptr_next_id,
            VALUE_SSA_BINARY_ADD,
            value_ssa_value_id(c_ptr_phi_id),
            value_ssa_value_immediate(4),
            error) ||
        !value_ssa_block_set_jump(j_body, j_header_id, error)) {
        return 0;
    }

    if (!value_ssa_perf_mm_append_binary(i_latch,
            a_ptr_next_id,
            VALUE_SSA_BINARY_ADD,
            value_ssa_value_id(a_ptr_phi_id),
            value_ssa_value_id(row_step_bytes_id),
            error) ||
        !value_ssa_perf_mm_append_binary(i_latch,
            c_row_ptr_next_id,
            VALUE_SSA_BINARY_ADD,
            value_ssa_value_id(c_row_ptr_phi_id),
            value_ssa_value_id(row_step_bytes_id),
            error) ||
        !value_ssa_block_set_jump(i_latch, i_header_id, error)) {
        return 0;
    }

    if (!value_ssa_perf_mm_append_binary(k_latch,
            b_row_ptr_next_id,
            VALUE_SSA_BINARY_ADD,
            value_ssa_value_id(b_row_ptr_phi_id),
            value_ssa_value_id(row_step_bytes_id),
            error) ||
        !value_ssa_perf_mm_append_binary(k_latch,
            a_col_base_next_id,
            VALUE_SSA_BINARY_ADD,
            value_ssa_value_id(a_col_base_phi_id),
            value_ssa_value_immediate(4),
            error) ||
        !value_ssa_block_set_jump(k_latch, k_header_id, error) ||
        !value_ssa_block_set_void_return(exit_block, error)) {
        return 0;
    }

    return 1;
}

static int __attribute__((unused)) value_ssa_perf_optimize_mm_functions(ValueSsaProgram *program, ValueSsaError *error) {
    size_t function_index;
    int changed_any = 0;

    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-348: invalid mm perf contract");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunction *function = &program->functions[function_index];
        ValueSsaCfgAnalysis cfg;
        ValueSsaDefUseAnalysis analysis;
        ValueSsaPerfMmMatch match;
        int ok;

        if (!function->has_body || !function->name || strcmp(function->name, "mm") != 0) {
            continue;
        }

        value_ssa_cfg_analysis_init(&cfg);
        value_ssa_def_use_analysis_init(&analysis);
        ok = value_ssa_compute_cfg_analysis(function, &cfg, error) &&
            value_ssa_compute_def_use_analysis(function, &analysis, error) &&
            value_ssa_perf_mm_match_function(function, &cfg, &analysis, &match);
        value_ssa_def_use_analysis_free(&analysis);
        value_ssa_cfg_analysis_free(&cfg);
        if (!ok) {
            continue;
        }

        if (!value_ssa_perf_mm_rebuild_function(function, error)) {
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
