#include "value_ssa.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int expect_live_row(const char *case_id,
    const ValueSsaLivenessAnalysis *analysis,
    size_t block_id,
    int use_live_in,
    const size_t *expected_values,
    size_t expected_count) {
    size_t value_id;
    int ok = 1;

    if (!analysis || block_id >= analysis->block_count) {
        fprintf(stderr, "[value-ssa-analysis] FAIL: %s invalid analysis row request\n", case_id);
        return 0;
    }

    for (value_id = 0; value_id < analysis->value_count; ++value_id) {
        const unsigned char *matrix = use_live_in ? analysis->live_in : analysis->live_out;
        int expected = 0;
        size_t index;

        for (index = 0; index < expected_count; ++index) {
            if (expected_values[index] == value_id) {
                expected = 1;
                break;
            }
        }

        if ((int)matrix[block_id * analysis->value_count + value_id] != expected) {
            fprintf(stderr,
                "[value-ssa-analysis] FAIL: %s bb.%zu %s mismatch for ssa.%zu\n",
                case_id,
                block_id,
                use_live_in ? "live-in" : "live-out",
                value_id);
            ok = 0;
        }
    }

    return ok;
}

static int expect_interference(const char *case_id,
    const ValueSsaInterferenceGraph *graph,
    size_t lhs,
    size_t rhs,
    int expected) {
    int actual;

    if (!graph || !graph->interferes || lhs >= graph->value_count || rhs >= graph->value_count) {
        fprintf(stderr, "[value-ssa-analysis] FAIL: %s invalid interference query\n", case_id);
        return 0;
    }

    actual = (int)graph->interferes[lhs * graph->value_count + rhs];
    if (actual != expected ||
        (int)graph->interferes[rhs * graph->value_count + lhs] != expected) {
        fprintf(stderr,
            "[value-ssa-analysis] FAIL: %s expected ssa.%zu <-> ssa.%zu = %d, got %d\n",
            case_id,
            lhs,
            rhs,
            expected,
            actual);
        return 0;
    }

    return 1;
}

static int expect_affinity_weight(const char *case_id,
    const ValueSsaCopyAffinityGraph *graph,
    size_t lhs,
    size_t rhs,
    size_t expected) {
    size_t actual;

    if (!graph || !graph->weights || lhs >= graph->value_count || rhs >= graph->value_count) {
        fprintf(stderr, "[value-ssa-analysis] FAIL: %s invalid affinity query\n", case_id);
        return 0;
    }

    actual = graph->weights[lhs * graph->value_count + rhs];
    if (actual != expected || graph->weights[rhs * graph->value_count + lhs] != expected) {
        fprintf(stderr,
            "[value-ssa-analysis] FAIL: %s expected ssa.%zu <-> ssa.%zu weight %zu, got %zu\n",
            case_id,
            lhs,
            rhs,
            expected,
            actual);
        return 0;
    }

    return 1;
}

static int expect_allocation_candidate(const char *case_id,
    const ValueSsaAllocationPrep *prep,
    size_t candidate_index,
    size_t lhs,
    size_t rhs,
    size_t weight) {
    const ValueSsaCopyAffinityCandidate *candidate;

    if (!prep || candidate_index >= prep->candidate_count) {
        fprintf(stderr, "[value-ssa-analysis] FAIL: %s invalid allocation candidate index\n", case_id);
        return 0;
    }

    candidate = &prep->candidates[candidate_index];
    if (candidate->lhs != lhs || candidate->rhs != rhs || candidate->weight != weight) {
        fprintf(stderr,
            "[value-ssa-analysis] FAIL: %s expected candidate[%zu] = (%zu,%zu,%zu), got (%zu,%zu,%zu)\n",
            case_id,
            candidate_index,
            lhs,
            rhs,
            weight,
            candidate->lhs,
            candidate->rhs,
            candidate->weight);
        return 0;
    }

    return 1;
}

static int expect_allocation_live_blocks(const char *case_id,
    const ValueSsaAllocationPrep *prep,
    size_t value_id,
    const size_t *expected_blocks,
    size_t expected_count) {
    size_t offset;
    size_t index;

    if (!prep || value_id >= prep->value_count) {
        fprintf(stderr, "[value-ssa-analysis] FAIL: %s invalid allocation live-block query\n", case_id);
        return 0;
    }
    if (prep->live_block_counts[value_id] != expected_count) {
        fprintf(stderr,
            "[value-ssa-analysis] FAIL: %s expected live-block count %zu, got %zu\n",
            case_id,
            expected_count,
            prep->live_block_counts[value_id]);
        return 0;
    }

    offset = prep->live_block_offsets[value_id];
    for (index = 0; index < expected_count; ++index) {
        if (prep->live_blocks[offset + index] != expected_blocks[index]) {
            fprintf(stderr,
                "[value-ssa-analysis] FAIL: %s expected live_blocks[%zu] = %zu, got %zu\n",
                case_id,
                index,
                expected_blocks[index],
                prep->live_blocks[offset + index]);
            return 0;
        }
    }

    return 1;
}

static int expect_worklist_item(const char *case_id,
    const ValueSsaAllocationWorklist *worklist,
    size_t item_index,
    size_t value_id,
    ValueSsaAllocationWorkClass work_class,
    size_t priority) {
    const ValueSsaAllocationWorkItem *item;

    if (!worklist || item_index >= worklist->item_count) {
        fprintf(stderr, "[value-ssa-analysis] FAIL: %s invalid worklist item index\n", case_id);
        return 0;
    }

    item = &worklist->items[item_index];
    if (item->value_id != value_id || item->work_class != work_class || item->priority != priority) {
        fprintf(stderr,
            "[value-ssa-analysis] FAIL: %s expected item[%zu] = (v=%zu,c=%d,p=%zu), got (v=%zu,c=%d,p=%zu)\n",
            case_id,
            item_index,
            value_id,
            (int)work_class,
            priority,
            item->value_id,
            (int)item->work_class,
            item->priority);
        return 0;
    }

    return 1;
}

static int find_worklist_item_index(const ValueSsaAllocationWorklist *worklist,
    size_t value_id,
    size_t *out_index) {
    size_t index;

    if (!worklist || !out_index) {
        return 0;
    }

    for (index = 0; index < worklist->item_count; ++index) {
        if (worklist->items[index].value_id == value_id) {
            *out_index = index;
            return 1;
        }
    }

    return 0;
}

static int build_phi_diamond_program(ValueSsaProgram *program,
    size_t *out_then_value,
    size_t *out_else_value,
    ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaInstruction instruction;
    ValueSsaPhiInput phi_inputs[2];
    size_t cond_local_id;
    size_t cond_value;
    size_t then_value;
    size_t else_value;
    size_t phi_value;
    size_t block_ids[4];

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "cond", 1, &cond_local_id, error) ||
        !value_ssa_function_append_block(function, &block_ids[0], NULL, error) ||
        !value_ssa_function_append_block(function, &block_ids[1], NULL, error) ||
        !value_ssa_function_append_block(function, &block_ids[2], NULL, error) ||
        !value_ssa_function_append_block(function, &block_ids[3], NULL, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    cond_value = value_ssa_function_allocate_value(function);
    then_value = value_ssa_function_allocate_value(function);
    else_value = value_ssa_function_allocate_value(function);
    phi_value = value_ssa_function_allocate_value(function);
    if (cond_value == (size_t)-1 || then_value == (size_t)-1 || else_value == (size_t)-1 ||
        phi_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_value);
    instruction.as.load_slot = value_ssa_slot_local(cond_local_id);
    if (!value_ssa_block_append_instruction(&function->blocks[block_ids[0]], &instruction, error) ||
        !value_ssa_block_set_branch(&function->blocks[block_ids[0]],
            value_ssa_value_id(cond_value),
            block_ids[1],
            block_ids[2],
            error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(then_value);
    instruction.as.mov_value = value_ssa_value_immediate(11);
    if (!value_ssa_block_append_instruction(&function->blocks[block_ids[1]], &instruction, error) ||
        !value_ssa_block_set_jump(&function->blocks[block_ids[1]], block_ids[3], error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(else_value);
    instruction.as.mov_value = value_ssa_value_immediate(22);
    if (!value_ssa_block_append_instruction(&function->blocks[block_ids[2]], &instruction, error) ||
        !value_ssa_block_set_jump(&function->blocks[block_ids[2]], block_ids[3], error)) {
        value_ssa_program_free(program);
        return 0;
    }

    phi_inputs[0].predecessor_block_id = block_ids[1];
    phi_inputs[0].value = value_ssa_value_id(then_value);
    phi_inputs[1].predecessor_block_id = block_ids[2];
    phi_inputs[1].value = value_ssa_value_id(else_value);
    if (!value_ssa_block_append_phi(&function->blocks[block_ids[3]], phi_value, phi_inputs, 2, error) ||
        !value_ssa_block_set_return(&function->blocks[block_ids[3]], value_ssa_value_id(phi_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    *out_then_value = then_value;
    *out_else_value = else_value;
    return 1;
}

static int build_loop_program(ValueSsaProgram *program,
    size_t *out_init_value,
    size_t *out_phi_value,
    size_t *out_step_value,
    ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaInstruction instruction;
    ValueSsaPhiInput phi_inputs[2];
    size_t cond_local_id;
    size_t init_value;
    size_t phi_value;
    size_t cond_value;
    size_t step_value;
    size_t block_ids[4];

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "cond", 1, &cond_local_id, error) ||
        !value_ssa_function_append_block(function, &block_ids[0], NULL, error) ||
        !value_ssa_function_append_block(function, &block_ids[1], NULL, error) ||
        !value_ssa_function_append_block(function, &block_ids[2], NULL, error) ||
        !value_ssa_function_append_block(function, &block_ids[3], NULL, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    init_value = value_ssa_function_allocate_value(function);
    phi_value = value_ssa_function_allocate_value(function);
    cond_value = value_ssa_function_allocate_value(function);
    step_value = value_ssa_function_allocate_value(function);
    if (init_value == (size_t)-1 || phi_value == (size_t)-1 || cond_value == (size_t)-1 ||
        step_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(init_value);
    instruction.as.mov_value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(&function->blocks[block_ids[0]], &instruction, error) ||
        !value_ssa_block_set_jump(&function->blocks[block_ids[0]], block_ids[1], error)) {
        value_ssa_program_free(program);
        return 0;
    }

    phi_inputs[0].predecessor_block_id = block_ids[0];
    phi_inputs[0].value = value_ssa_value_id(init_value);
    phi_inputs[1].predecessor_block_id = block_ids[2];
    phi_inputs[1].value = value_ssa_value_id(step_value);
    if (!value_ssa_block_append_phi(&function->blocks[block_ids[1]], phi_value, phi_inputs, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_value);
    instruction.as.load_slot = value_ssa_slot_local(cond_local_id);
    if (!value_ssa_block_append_instruction(&function->blocks[block_ids[1]], &instruction, error) ||
        !value_ssa_block_set_branch(&function->blocks[block_ids[1]],
            value_ssa_value_id(cond_value),
            block_ids[2],
            block_ids[3],
            error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(step_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(phi_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(&function->blocks[block_ids[2]], &instruction, error) ||
        !value_ssa_block_set_jump(&function->blocks[block_ids[2]], block_ids[1], error) ||
        !value_ssa_block_set_return(&function->blocks[block_ids[3]], value_ssa_value_id(phi_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    *out_init_value = init_value;
    *out_phi_value = phi_value;
    *out_step_value = step_value;
    return 1;
}

static int build_straight_interference_program(ValueSsaProgram *program,
    size_t *out_left_value,
    size_t *out_right_value,
    size_t *out_sum_value,
    ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaInstruction instruction;
    size_t local_a_id;
    size_t local_b_id;
    size_t left_value;
    size_t right_value;
    size_t sum_value;
    size_t block_id;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 1, &local_a_id, error) ||
        !value_ssa_function_append_local(function, "b", 1, &local_b_id, error) ||
        !value_ssa_function_append_block(function, &block_id, NULL, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    left_value = value_ssa_function_allocate_value(function);
    right_value = value_ssa_function_allocate_value(function);
    sum_value = value_ssa_function_allocate_value(function);
    if (left_value == (size_t)-1 || right_value == (size_t)-1 || sum_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(left_value);
    instruction.as.load_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(&function->blocks[block_id], &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(right_value);
    instruction.as.load_slot = value_ssa_slot_local(local_b_id);
    if (!value_ssa_block_append_instruction(&function->blocks[block_id], &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(sum_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(left_value);
    instruction.as.binary.rhs = value_ssa_value_id(right_value);
    if (!value_ssa_block_append_instruction(&function->blocks[block_id], &instruction, error) ||
        !value_ssa_block_set_return(&function->blocks[block_id], value_ssa_value_id(sum_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    *out_left_value = left_value;
    *out_right_value = right_value;
    *out_sum_value = sum_value;
    return 1;
}

static int build_loop_interference_program(ValueSsaProgram *program,
    size_t *out_invariant_value,
    size_t *out_init_value,
    size_t *out_phi_value,
    size_t *out_step_value,
    ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaInstruction instruction;
    ValueSsaPhiInput phi_inputs[2];
    size_t cond_local_id;
    size_t invariant_value;
    size_t init_value;
    size_t phi_value;
    size_t cond_value;
    size_t body_sum_value;
    size_t step_value;
    size_t block_ids[4];

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "cond", 1, &cond_local_id, error) ||
        !value_ssa_function_append_block(function, &block_ids[0], NULL, error) ||
        !value_ssa_function_append_block(function, &block_ids[1], NULL, error) ||
        !value_ssa_function_append_block(function, &block_ids[2], NULL, error) ||
        !value_ssa_function_append_block(function, &block_ids[3], NULL, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    invariant_value = value_ssa_function_allocate_value(function);
    init_value = value_ssa_function_allocate_value(function);
    phi_value = value_ssa_function_allocate_value(function);
    cond_value = value_ssa_function_allocate_value(function);
    body_sum_value = value_ssa_function_allocate_value(function);
    step_value = value_ssa_function_allocate_value(function);
    if (invariant_value == (size_t)-1 || init_value == (size_t)-1 || phi_value == (size_t)-1 ||
        cond_value == (size_t)-1 || body_sum_value == (size_t)-1 || step_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(invariant_value);
    instruction.as.mov_value = value_ssa_value_immediate(100);
    if (!value_ssa_block_append_instruction(&function->blocks[block_ids[0]], &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(init_value);
    instruction.as.mov_value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(&function->blocks[block_ids[0]], &instruction, error) ||
        !value_ssa_block_set_jump(&function->blocks[block_ids[0]], block_ids[1], error)) {
        value_ssa_program_free(program);
        return 0;
    }

    phi_inputs[0].predecessor_block_id = block_ids[0];
    phi_inputs[0].value = value_ssa_value_id(init_value);
    phi_inputs[1].predecessor_block_id = block_ids[2];
    phi_inputs[1].value = value_ssa_value_id(step_value);
    if (!value_ssa_block_append_phi(&function->blocks[block_ids[1]], phi_value, phi_inputs, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_value);
    instruction.as.load_slot = value_ssa_slot_local(cond_local_id);
    if (!value_ssa_block_append_instruction(&function->blocks[block_ids[1]], &instruction, error) ||
        !value_ssa_block_set_branch(&function->blocks[block_ids[1]],
            value_ssa_value_id(cond_value),
            block_ids[2],
            block_ids[3],
            error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(body_sum_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(invariant_value);
    instruction.as.binary.rhs = value_ssa_value_id(phi_value);
    if (!value_ssa_block_append_instruction(&function->blocks[block_ids[2]], &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(step_value);
    instruction.as.binary.lhs = value_ssa_value_id(phi_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(&function->blocks[block_ids[2]], &instruction, error) ||
        !value_ssa_block_set_jump(&function->blocks[block_ids[2]], block_ids[1], error) ||
        !value_ssa_block_set_return(&function->blocks[block_ids[3]], value_ssa_value_id(invariant_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    *out_invariant_value = invariant_value;
    *out_init_value = init_value;
    *out_phi_value = phi_value;
    *out_step_value = step_value;
    return 1;
}

static int build_copy_affinity_program(ValueSsaProgram *program,
    size_t *out_source_value,
    size_t *out_copy_value,
    ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaInstruction instruction;
    size_t local_a_id;
    size_t source_value;
    size_t copy_value;
    size_t block_id;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 1, &local_a_id, error) ||
        !value_ssa_function_append_block(function, &block_id, NULL, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    source_value = value_ssa_function_allocate_value(function);
    copy_value = value_ssa_function_allocate_value(function);
    if (source_value == (size_t)-1 || copy_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(source_value);
    instruction.as.load_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(&function->blocks[block_id], &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(copy_value);
    instruction.as.mov_value = value_ssa_value_id(source_value);
    if (!value_ssa_block_append_instruction(&function->blocks[block_id], &instruction, error) ||
        !value_ssa_block_set_return(&function->blocks[block_id], value_ssa_value_id(copy_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    *out_source_value = source_value;
    *out_copy_value = copy_value;
    return 1;
}

static int build_two_block_range_program(ValueSsaProgram *program,
    size_t *out_value,
    ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaInstruction instruction;
    size_t local_a_id;
    size_t value_id;
    size_t bb0_id;
    size_t bb1_id;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 1, &local_a_id, error) ||
        !value_ssa_function_append_block(function, &bb0_id, NULL, error) ||
        !value_ssa_function_append_block(function, &bb1_id, NULL, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    value_id = value_ssa_function_allocate_value(function);
    if (value_id == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value_id);
    instruction.as.load_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(&function->blocks[bb0_id], &instruction, error) ||
        !value_ssa_block_set_jump(&function->blocks[bb0_id], bb1_id, error) ||
        !value_ssa_block_set_return(&function->blocks[bb1_id], value_ssa_value_id(value_id), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    *out_value = value_id;
    return 1;
}

static int build_interfering_copy_program(ValueSsaProgram *program,
    size_t *out_source_value,
    size_t *out_copy_value,
    ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaInstruction instruction;
    size_t local_a_id;
    size_t source_value;
    size_t copy_value;
    size_t plus_value;
    size_t merge_value;
    size_t block_id;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 1, &local_a_id, error) ||
        !value_ssa_function_append_block(function, &block_id, NULL, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    source_value = value_ssa_function_allocate_value(function);
    copy_value = value_ssa_function_allocate_value(function);
    plus_value = value_ssa_function_allocate_value(function);
    merge_value = value_ssa_function_allocate_value(function);
    if (source_value == (size_t)-1 || copy_value == (size_t)-1 ||
        plus_value == (size_t)-1 || merge_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(source_value);
    instruction.as.load_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(&function->blocks[block_id], &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(copy_value);
    instruction.as.mov_value = value_ssa_value_id(source_value);
    if (!value_ssa_block_append_instruction(&function->blocks[block_id], &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(plus_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(source_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(&function->blocks[block_id], &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(merge_value);
    instruction.as.binary.lhs = value_ssa_value_id(copy_value);
    instruction.as.binary.rhs = value_ssa_value_id(plus_value);
    if (!value_ssa_block_append_instruction(&function->blocks[block_id], &instruction, error) ||
        !value_ssa_block_set_return(&function->blocks[block_id], value_ssa_value_id(merge_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    *out_source_value = source_value;
    *out_copy_value = copy_value;
    return 1;
}

static int test_value_ssa_liveness_phi_diamond(void) {
    ValueSsaProgram program;
    ValueSsaLivenessAnalysis analysis;
    ValueSsaError error;
    size_t then_value;
    size_t else_value;
    int ok = 1;

    value_ssa_liveness_analysis_init(&analysis);
    if (!build_phi_diamond_program(&program, &then_value, &else_value, &error)) {
        fprintf(stderr,
            "[value-ssa-analysis] FAIL: diamond setup failed at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
        return 0;
    }

    if (!value_ssa_compute_liveness_analysis(&program.functions[0], NULL, &analysis, &error)) {
        fprintf(stderr,
            "[value-ssa-analysis] FAIL: diamond liveness failed at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_live_row("LIVENESS-DIAMOND-bb0-live-in", &analysis, 0, 1, NULL, 0);
    ok &= expect_live_row("LIVENESS-DIAMOND-bb0-live-out", &analysis, 0, 0, NULL, 0);
    ok &= expect_live_row("LIVENESS-DIAMOND-bb1-live-in", &analysis, 1, 1, NULL, 0);
    ok &= expect_live_row("LIVENESS-DIAMOND-bb1-live-out", &analysis, 1, 0, &then_value, 1);
    ok &= expect_live_row("LIVENESS-DIAMOND-bb2-live-in", &analysis, 2, 1, NULL, 0);
    ok &= expect_live_row("LIVENESS-DIAMOND-bb2-live-out", &analysis, 2, 0, &else_value, 1);
    ok &= expect_live_row("LIVENESS-DIAMOND-bb3-live-in", &analysis, 3, 1, NULL, 0);
    ok &= expect_live_row("LIVENESS-DIAMOND-bb3-live-out", &analysis, 3, 0, NULL, 0);

cleanup:
    value_ssa_liveness_analysis_free(&analysis);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_liveness_loop(void) {
    ValueSsaProgram program;
    ValueSsaCfgAnalysis cfg_analysis;
    ValueSsaLivenessAnalysis analysis;
    ValueSsaError error;
    size_t init_value;
    size_t phi_value;
    size_t step_value;
    int ok = 1;

    value_ssa_cfg_analysis_init(&cfg_analysis);
    value_ssa_liveness_analysis_init(&analysis);
    if (!build_loop_program(&program, &init_value, &phi_value, &step_value, &error)) {
        fprintf(stderr,
            "[value-ssa-analysis] FAIL: loop setup failed at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
        return 0;
    }

    if (!value_ssa_compute_cfg_analysis(&program.functions[0], &cfg_analysis, &error) ||
        !value_ssa_compute_liveness_analysis(&program.functions[0], &cfg_analysis, &analysis, &error)) {
        fprintf(stderr,
            "[value-ssa-analysis] FAIL: loop liveness failed at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_live_row("LIVENESS-LOOP-bb0-live-in", &analysis, 0, 1, NULL, 0);
    ok &= expect_live_row("LIVENESS-LOOP-bb0-live-out", &analysis, 0, 0, &init_value, 1);
    ok &= expect_live_row("LIVENESS-LOOP-bb1-live-in", &analysis, 1, 1, NULL, 0);
    ok &= expect_live_row("LIVENESS-LOOP-bb1-live-out", &analysis, 1, 0, &phi_value, 1);
    ok &= expect_live_row("LIVENESS-LOOP-bb2-live-in", &analysis, 2, 1, &phi_value, 1);
    ok &= expect_live_row("LIVENESS-LOOP-bb2-live-out", &analysis, 2, 0, &step_value, 1);
    ok &= expect_live_row("LIVENESS-LOOP-bb3-live-in", &analysis, 3, 1, &phi_value, 1);
    ok &= expect_live_row("LIVENESS-LOOP-bb3-live-out", &analysis, 3, 0, NULL, 0);

cleanup:
    value_ssa_liveness_analysis_free(&analysis);
    value_ssa_cfg_analysis_free(&cfg_analysis);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_interference_straight_line(void) {
    ValueSsaProgram program;
    ValueSsaInterferenceGraph graph;
    ValueSsaError error;
    size_t left_value;
    size_t right_value;
    size_t sum_value;
    int ok = 1;

    value_ssa_interference_graph_init(&graph);
    if (!build_straight_interference_program(&program, &left_value, &right_value, &sum_value, &error)) {
        fprintf(stderr,
            "[value-ssa-analysis] FAIL: straight interference setup failed at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
        return 0;
    }

    if (!value_ssa_compute_interference_graph(&program.functions[0], NULL, NULL, &graph, &error)) {
        fprintf(stderr,
            "[value-ssa-analysis] FAIL: straight interference failed at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_interference("INTERF-STRAIGHT-left-right", &graph, left_value, right_value, 1);
    ok &= expect_interference("INTERF-STRAIGHT-left-sum", &graph, left_value, sum_value, 0);
    ok &= expect_interference("INTERF-STRAIGHT-right-sum", &graph, right_value, sum_value, 0);

cleanup:
    value_ssa_interference_graph_free(&graph);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_interference_phi_and_loop(void) {
    ValueSsaProgram diamond_program;
    ValueSsaProgram loop_program;
    ValueSsaCfgAnalysis cfg_analysis;
    ValueSsaLivenessAnalysis liveness;
    ValueSsaInterferenceGraph graph;
    ValueSsaError error;
    size_t then_value;
    size_t else_value;
    size_t invariant_value;
    size_t init_value;
    size_t phi_value;
    size_t step_value;
    int ok = 1;

    value_ssa_program_init(&diamond_program);
    value_ssa_program_init(&loop_program);
    value_ssa_cfg_analysis_init(&cfg_analysis);
    value_ssa_liveness_analysis_init(&liveness);
    value_ssa_interference_graph_init(&graph);

    if (!build_phi_diamond_program(&diamond_program, &then_value, &else_value, &error)) {
        fprintf(stderr,
            "[value-ssa-analysis] FAIL: diamond interference setup failed at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
        return 0;
    }

    if (!value_ssa_compute_interference_graph(&diamond_program.functions[0], NULL, NULL, &graph, &error)) {
        fprintf(stderr,
            "[value-ssa-analysis] FAIL: diamond interference failed at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= expect_interference("INTERF-DIAMOND-then-else", &graph, then_value, else_value, 0);
    value_ssa_interference_graph_free(&graph);

    if (!build_loop_interference_program(
            &loop_program, &invariant_value, &init_value, &phi_value, &step_value, &error)) {
        fprintf(stderr,
            "[value-ssa-analysis] FAIL: loop interference setup failed at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
        return 0;
    }

    if (!value_ssa_compute_cfg_analysis(&loop_program.functions[0], &cfg_analysis, &error) ||
        !value_ssa_compute_liveness_analysis(&loop_program.functions[0], &cfg_analysis, &liveness, &error) ||
        !value_ssa_compute_interference_graph(&loop_program.functions[0], &cfg_analysis, &liveness, &graph, &error)) {
        fprintf(stderr,
            "[value-ssa-analysis] FAIL: loop interference failed at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_interference("INTERF-LOOP-invariant-init", &graph, invariant_value, init_value, 1);
    ok &= expect_interference("INTERF-LOOP-invariant-phi", &graph, invariant_value, phi_value, 1);
    ok &= expect_interference("INTERF-LOOP-invariant-step", &graph, invariant_value, step_value, 1);
    ok &= expect_interference("INTERF-LOOP-phi-step", &graph, phi_value, step_value, 0);

cleanup:
    value_ssa_interference_graph_free(&graph);
    value_ssa_liveness_analysis_free(&liveness);
    value_ssa_cfg_analysis_free(&cfg_analysis);
    value_ssa_program_free(&diamond_program);
    value_ssa_program_free(&loop_program);
    return ok;
}

static int test_value_ssa_copy_affinity_graph(void) {
    ValueSsaProgram affinity_program;
    ValueSsaProgram interfering_program;
    ValueSsaInterferenceGraph interference;
    ValueSsaCopyAffinityGraph affinity;
    ValueSsaError error;
    size_t source_value;
    size_t copy_value;
    size_t interfering_source;
    size_t interfering_copy;
    int ok = 1;

    value_ssa_program_init(&affinity_program);
    value_ssa_program_init(&interfering_program);
    value_ssa_interference_graph_init(&interference);
    value_ssa_copy_affinity_graph_init(&affinity);

    if (!build_copy_affinity_program(&affinity_program, &source_value, &copy_value, &error)) {
        fprintf(stderr,
            "[value-ssa-analysis] FAIL: affinity setup failed at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
        return 0;
    }

    if (!value_ssa_compute_copy_affinity_graph(&affinity_program.functions[0], NULL, &affinity, &error)) {
        fprintf(stderr,
            "[value-ssa-analysis] FAIL: affinity graph failed at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= expect_affinity_weight("AFFINITY-simple-copy", &affinity, source_value, copy_value, 1);

    value_ssa_copy_affinity_graph_free(&affinity);
    if (!build_interfering_copy_program(&interfering_program, &interfering_source, &interfering_copy, &error)) {
        fprintf(stderr,
            "[value-ssa-analysis] FAIL: interfering affinity setup failed at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
        return 0;
    }

    if (!value_ssa_compute_interference_graph(
            &interfering_program.functions[0], NULL, NULL, &interference, &error) ||
        !value_ssa_compute_copy_affinity_graph(
            &interfering_program.functions[0], &interference, &affinity, &error)) {
        fprintf(stderr,
            "[value-ssa-analysis] FAIL: interfering affinity graph failed at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= expect_interference(
        "AFFINITY-interfering-copy-interference", &interference, interfering_source, interfering_copy, 1);
    ok &= expect_affinity_weight("AFFINITY-interfering-copy-filtered",
        &affinity,
        interfering_source,
        interfering_copy,
        0);

cleanup:
    value_ssa_copy_affinity_graph_free(&affinity);
    value_ssa_interference_graph_free(&interference);
    value_ssa_program_free(&affinity_program);
    value_ssa_program_free(&interfering_program);
    return ok;
}

static int test_value_ssa_allocation_prep(void) {
    ValueSsaProgram program;
    ValueSsaAllocationPrep prep;
    ValueSsaError error;
    size_t source_value;
    size_t copy_value;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    if (!build_copy_affinity_program(&program, &source_value, &copy_value, &error)) {
        fprintf(stderr,
            "[value-ssa-analysis] FAIL: allocation prep setup failed at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
        return 0;
    }

    if (!value_ssa_compute_allocation_prep(&program.functions[0], NULL, NULL, NULL, NULL, &prep, &error)) {
        fprintf(stderr,
            "[value-ssa-analysis] FAIL: allocation prep failed at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
        ok = 0;
        goto cleanup;
    }

    if (prep.value_count != program.functions[0].next_value_id) {
        fprintf(stderr,
            "[value-ssa-analysis] FAIL: allocation prep value_count mismatch (%zu vs %zu)\n",
            prep.value_count,
            program.functions[0].next_value_id);
        ok = 0;
        goto cleanup;
    }

    if (prep.candidate_count != 1) {
        fprintf(stderr,
            "[value-ssa-analysis] FAIL: expected 1 affinity candidate, got %zu\n",
            prep.candidate_count);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_allocation_candidate("ALLOC-PREP-candidate", &prep, 0, source_value, copy_value, 1);
    ok &= prep.interference_degrees[source_value] == 0 && prep.interference_degrees[copy_value] == 0;
    if (!(prep.interference_degrees[source_value] == 0 && prep.interference_degrees[copy_value] == 0)) {
        fprintf(stderr, "[value-ssa-analysis] FAIL: unexpected interference degrees in allocation prep\n");
        ok = 0;
    }
    ok &= prep.affinity_sums[source_value] == 1 && prep.affinity_sums[copy_value] == 1;
    if (!(prep.affinity_sums[source_value] == 1 && prep.affinity_sums[copy_value] == 1)) {
        fprintf(stderr, "[value-ssa-analysis] FAIL: unexpected affinity sums in allocation prep\n");
        ok = 0;
    }
    ok &= prep.move_related[source_value] == 1 && prep.move_related[copy_value] == 1;
    if (!(prep.move_related[source_value] == 1 && prep.move_related[copy_value] == 1)) {
        fprintf(stderr, "[value-ssa-analysis] FAIL: expected move-related flags in allocation prep\n");
        ok = 0;
    }
    ok &= prep.def_block_ids[source_value] == 0 && prep.def_block_ids[copy_value] == 0;
    if (!(prep.def_block_ids[source_value] == 0 && prep.def_block_ids[copy_value] == 0)) {
        fprintf(stderr, "[value-ssa-analysis] FAIL: unexpected def_block_ids in allocation prep\n");
        ok = 0;
    }
    ok &= prep.use_counts[source_value] == 1 && prep.use_counts[copy_value] == 1;
    if (!(prep.use_counts[source_value] == 1 && prep.use_counts[copy_value] == 1)) {
        fprintf(stderr, "[value-ssa-analysis] FAIL: unexpected use_counts in allocation prep\n");
        ok = 0;
    }
    ok &= prep.single_block_live_ranges[source_value] == 1 && prep.single_block_live_ranges[copy_value] == 1;
    if (!(prep.single_block_live_ranges[source_value] == 1 && prep.single_block_live_ranges[copy_value] == 1)) {
        fprintf(stderr, "[value-ssa-analysis] FAIL: expected single-block flags in allocation prep\n");
        ok = 0;
    }
    {
        const size_t expected_blocks[] = {0};

        ok &= expect_allocation_live_blocks("ALLOC-PREP-source-live-blocks",
            &prep,
            source_value,
            expected_blocks,
            1);
        ok &= expect_allocation_live_blocks("ALLOC-PREP-copy-live-blocks",
            &prep,
            copy_value,
            expected_blocks,
            1);
    }

cleanup:
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocation_prep_two_block_range(void) {
    ValueSsaProgram program;
    ValueSsaDefUseAnalysis def_use;
    ValueSsaLivenessAnalysis liveness;
    ValueSsaInterferenceGraph interference;
    ValueSsaCopyAffinityGraph affinity;
    ValueSsaAllocationPrep prep;
    ValueSsaError error;
    size_t value_id;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_def_use_analysis_init(&def_use);
    value_ssa_liveness_analysis_init(&liveness);
    value_ssa_interference_graph_init(&interference);
    value_ssa_copy_affinity_graph_init(&affinity);
    value_ssa_allocation_prep_init(&prep);
    if (!build_two_block_range_program(&program, &value_id, &error)) {
        fprintf(stderr,
            "[value-ssa-analysis] FAIL: two-block allocation prep setup failed at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
        return 0;
    }

    if (!value_ssa_compute_def_use_analysis(&program.functions[0], &def_use, &error) ||
        !value_ssa_compute_liveness_analysis(&program.functions[0], NULL, &liveness, &error) ||
        !value_ssa_compute_interference_graph(&program.functions[0], NULL, &liveness, &interference, &error) ||
        !value_ssa_compute_copy_affinity_graph(&program.functions[0], &interference, &affinity, &error) ||
        !value_ssa_compute_allocation_prep(
            &program.functions[0], &def_use, &liveness, &interference, &affinity, &prep, &error)) {
        fprintf(stderr,
            "[value-ssa-analysis] FAIL: two-block allocation prep failed at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
        ok = 0;
        goto cleanup;
    }

    if (prep.candidate_count != 0 || prep.live_blocks_count != 2) {
        fprintf(stderr,
            "[value-ssa-analysis] FAIL: unexpected two-block prep aggregate counts (%zu candidates, %zu live blocks)\n",
            prep.candidate_count,
            prep.live_blocks_count);
        ok = 0;
        goto cleanup;
    }
    ok &= prep.def_block_ids[value_id] == 0;
    ok &= prep.use_counts[value_id] == 1;
    ok &= prep.live_block_counts[value_id] == 2;
    ok &= prep.single_block_live_ranges[value_id] == 0;
    ok &= prep.interference_degrees[value_id] == 0;
    ok &= prep.affinity_sums[value_id] == 0;
    ok &= prep.move_related[value_id] == 0;
    if (!(prep.def_block_ids[value_id] == 0 && prep.use_counts[value_id] == 1 &&
            prep.live_block_counts[value_id] == 2 && prep.single_block_live_ranges[value_id] == 0 &&
            prep.interference_degrees[value_id] == 0 && prep.affinity_sums[value_id] == 0 &&
            prep.move_related[value_id] == 0)) {
        fprintf(stderr, "[value-ssa-analysis] FAIL: unexpected two-block allocation prep value summary\n");
        ok = 0;
    }
    {
        const size_t expected_blocks[] = {0, 1};

        ok &= expect_allocation_live_blocks("ALLOC-PREP-two-block-live-blocks",
            &prep,
            value_id,
            expected_blocks,
            2);
    }

cleanup:
    value_ssa_allocation_prep_free(&prep);
    value_ssa_copy_affinity_graph_free(&affinity);
    value_ssa_interference_graph_free(&interference);
    value_ssa_liveness_analysis_free(&liveness);
    value_ssa_def_use_analysis_free(&def_use);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocation_worklist(void) {
    ValueSsaProgram copy_program;
    ValueSsaProgram loop_program;
    ValueSsaAllocationPrep prep;
    ValueSsaAllocationWorklist worklist;
    ValueSsaError error;
    size_t source_value;
    size_t copy_value;
    size_t invariant_value;
    size_t init_value;
    size_t phi_value;
    size_t step_value;
    size_t invariant_index;
    size_t phi_index;
    size_t init_index;
    int ok = 1;

    value_ssa_program_init(&copy_program);
    value_ssa_program_init(&loop_program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_allocation_worklist_init(&worklist);

    if (!build_copy_affinity_program(&copy_program, &source_value, &copy_value, &error)) {
        fprintf(stderr,
            "[value-ssa-analysis] FAIL: worklist copy setup failed at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
        return 0;
    }

    if (!value_ssa_compute_allocation_prep(&copy_program.functions[0], NULL, NULL, NULL, NULL, &prep, &error) ||
        !value_ssa_compute_allocation_worklist(&copy_program.functions[0], &prep, &worklist, &error)) {
        fprintf(stderr,
            "[value-ssa-analysis] FAIL: worklist copy build failed at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
        ok = 0;
        goto cleanup;
    }

    if (worklist.item_count != 2) {
        fprintf(stderr,
            "[value-ssa-analysis] FAIL: expected 2 worklist items for copy program, got %zu\n",
            worklist.item_count);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_worklist_item("WORKLIST-copy-source",
        &worklist,
        0,
        source_value,
        VALUE_SSA_ALLOC_WORK_MOVE_HINT,
        11);
    ok &= expect_worklist_item("WORKLIST-copy-copy",
        &worklist,
        1,
        copy_value,
        VALUE_SSA_ALLOC_WORK_MOVE_HINT,
        11);

    value_ssa_allocation_worklist_free(&worklist);
    value_ssa_allocation_prep_free(&prep);

    if (!build_loop_interference_program(
            &loop_program, &invariant_value, &init_value, &phi_value, &step_value, &error)) {
        fprintf(stderr,
            "[value-ssa-analysis] FAIL: worklist loop setup failed at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
        return 0;
    }

    if (!value_ssa_compute_allocation_worklist(&loop_program.functions[0], NULL, &worklist, &error)) {
        fprintf(stderr,
            "[value-ssa-analysis] FAIL: worklist loop build failed at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
        ok = 0;
        goto cleanup;
    }

    if (worklist.item_count != loop_program.functions[0].next_value_id) {
        fprintf(stderr,
            "[value-ssa-analysis] FAIL: loop worklist item count mismatch (%zu vs %zu)\n",
            worklist.item_count,
            loop_program.functions[0].next_value_id);
        ok = 0;
        goto cleanup;
    }

    if (!find_worklist_item_index(&worklist, invariant_value, &invariant_index) ||
        !find_worklist_item_index(&worklist, phi_value, &phi_index) ||
        !find_worklist_item_index(&worklist, init_value, &init_index)) {
        fprintf(stderr, "[value-ssa-analysis] FAIL: could not locate expected loop worklist items\n");
        ok = 0;
        goto cleanup;
    }

    if (worklist.items[invariant_index].work_class != VALUE_SSA_ALLOC_WORK_CONSTRAINED ||
        worklist.items[phi_index].work_class != VALUE_SSA_ALLOC_WORK_CONSTRAINED) {
        fprintf(stderr, "[value-ssa-analysis] FAIL: unexpected loop worklist classes\n");
        ok = 0;
        goto cleanup;
    }
    if (!(invariant_index < init_index && phi_index < init_index &&
            worklist.items[invariant_index].priority > worklist.items[phi_index].priority)) {
        fprintf(stderr, "[value-ssa-analysis] FAIL: unexpected loop worklist ordering\n");
        ok = 0;
        goto cleanup;
    }
    if (worklist.items[0].work_class != VALUE_SSA_ALLOC_WORK_CONSTRAINED ||
        worklist.items[init_index].work_class == VALUE_SSA_ALLOC_WORK_MOVE_HINT) {
        fprintf(stderr, "[value-ssa-analysis] FAIL: unexpected loop worklist class boundaries\n");
        ok = 0;
        goto cleanup;
    }

cleanup:
    value_ssa_allocation_worklist_free(&worklist);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&copy_program);
    value_ssa_program_free(&loop_program);
    return ok;
}

static int test_value_ssa_allocation_dump_helpers(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    char *prep_text = NULL;
    char *worklist_text = NULL;
    char *move_hint_text = NULL;
    char *constrained_text = NULL;
    ValueSsaAllocationPrep prep;
    ValueSsaAllocationWorklist worklist;
    const size_t *live_blocks = NULL;
    const ValueSsaCopyAffinityCandidate *candidate = NULL;
    const ValueSsaAllocationWorkItem *item = NULL;
    size_t neighbor_ids[2];
    size_t neighbor_weights[2];
    size_t live_block_count = 0;
    size_t neighbor_count = 0;
    size_t item_index = 0;
    size_t candidate_index = 0;
    size_t class_begin = 0;
    size_t class_end = 0;
    size_t source_value;
    size_t copy_value;
    int ok = 1;
    const char *expected_prep =
        "alloc-prep main:\n"
        "  ssa.0: def=bb.0 uses=1 live_blocks=[bb.0] single_block=yes degree=0 affinity=1 move_related=yes\n"
        "  ssa.1: def=bb.0 uses=1 live_blocks=[bb.0] single_block=yes degree=0 affinity=1 move_related=yes\n"
        "  candidates:\n"
        "    ssa.0 <-> ssa.1 weight=1\n";
    const char *expected_worklist =
        "alloc-worklist main:\n"
        "  0: ssa.0 class=move-hint priority=11 blocks=1 degree=0 affinity=1\n"
        "  1: ssa.1 class=move-hint priority=11 blocks=1 degree=0 affinity=1\n";
    const char *expected_constrained =
        "alloc-worklist main:\n";

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_allocation_worklist_init(&worklist);
    if (!build_copy_affinity_program(&program, &source_value, &copy_value, &error)) {
        fprintf(stderr,
            "[value-ssa-analysis] FAIL: allocation dump setup failed at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
        return 0;
    }
    (void)source_value;
    (void)copy_value;

    if (strcmp(value_ssa_allocation_work_class_name(VALUE_SSA_ALLOC_WORK_MOVE_HINT), "move-hint") != 0 ||
        strcmp(value_ssa_allocation_work_class_name(VALUE_SSA_ALLOC_WORK_CONSTRAINED), "constrained") != 0 ||
        strcmp(value_ssa_allocation_work_class_name(VALUE_SSA_ALLOC_WORK_SINGLE_BLOCK), "single-block") != 0 ||
        strcmp(value_ssa_allocation_work_class_name(VALUE_SSA_ALLOC_WORK_GLOBAL), "global") != 0 ||
        strcmp(value_ssa_allocation_work_class_name(VALUE_SSA_ALLOC_WORK_ISOLATED), "isolated") != 0) {
        fprintf(stderr, "[value-ssa-analysis] FAIL: allocation work class names mismatch\n");
        ok = 0;
        goto cleanup;
    }

    if (!value_ssa_compute_allocation_prep(&program.functions[0], NULL, NULL, NULL, NULL, &prep, &error) ||
        !value_ssa_compute_allocation_worklist(&program.functions[0], &prep, &worklist, &error) ||
        !value_ssa_dump_allocation_prep(&program.functions[0], &prep, &prep_text, &error) ||
        !value_ssa_dump_allocation_worklist(&program.functions[0], &prep, &worklist, &worklist_text, &error) ||
        !value_ssa_dump_allocation_worklist_for_class(&program.functions[0],
            &prep,
            &worklist,
            VALUE_SSA_ALLOC_WORK_MOVE_HINT,
            &move_hint_text,
            &error) ||
        !value_ssa_dump_allocation_worklist_for_class(&program.functions[0],
            &prep,
            &worklist,
            VALUE_SSA_ALLOC_WORK_CONSTRAINED,
            &constrained_text,
            &error)) {
        fprintf(stderr,
            "[value-ssa-analysis] FAIL: allocation dump failed at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
        ok = 0;
        goto cleanup;
    }

    if (strcmp(prep_text, expected_prep) != 0) {
        fprintf(stderr,
            "[value-ssa-analysis] FAIL: allocation prep dump mismatch\nexpected:\n%sactual:\n%s",
            expected_prep,
            prep_text);
        ok = 0;
    }
    if (strcmp(worklist_text, expected_worklist) != 0) {
        fprintf(stderr,
            "[value-ssa-analysis] FAIL: allocation worklist dump mismatch\nexpected:\n%sactual:\n%s",
            expected_worklist,
            worklist_text);
        ok = 0;
    }
    if (strcmp(move_hint_text, expected_worklist) != 0) {
        fprintf(stderr,
            "[value-ssa-analysis] FAIL: filtered move-hint dump mismatch\nexpected:\n%sactual:\n%s",
            expected_worklist,
            move_hint_text);
        ok = 0;
    }
    if (strcmp(constrained_text, expected_constrained) != 0) {
        fprintf(stderr,
            "[value-ssa-analysis] FAIL: filtered constrained dump mismatch\nexpected:\n%sactual:\n%s",
            expected_constrained,
            constrained_text);
        ok = 0;
    }

    if (!value_ssa_allocation_prep_get_live_blocks(&prep, source_value, &live_blocks, &live_block_count) ||
        live_block_count != 1 || !live_blocks || live_blocks[0] != 0) {
        fprintf(stderr, "[value-ssa-analysis] FAIL: allocation prep live-block helper mismatch\n");
        ok = 0;
    }
    if (value_ssa_allocation_prep_get_affinity_weight(&prep, source_value, copy_value) != 1 ||
        value_ssa_allocation_prep_get_affinity_weight(&prep, source_value, source_value) != 0) {
        fprintf(stderr, "[value-ssa-analysis] FAIL: allocation prep affinity helper mismatch\n");
        ok = 0;
    }
    if (!value_ssa_allocation_worklist_find_value(&worklist, copy_value, &item_index, &item) ||
        !item || item_index != 1 || item->work_class != VALUE_SSA_ALLOC_WORK_MOVE_HINT ||
        item->priority != 11) {
        fprintf(stderr, "[value-ssa-analysis] FAIL: allocation worklist find helper mismatch\n");
        ok = 0;
    }
    if (!value_ssa_allocation_prep_find_affinity_candidate(
            &prep, source_value, copy_value, &candidate_index, &candidate) ||
        !candidate || candidate_index != 0 || candidate->weight != 1) {
        fprintf(stderr, "[value-ssa-analysis] FAIL: allocation prep candidate helper mismatch\n");
        ok = 0;
    }
    if (!value_ssa_allocation_prep_get_top_affinity_neighbors(
            &prep, source_value, 2, neighbor_ids, neighbor_weights, &neighbor_count) ||
        neighbor_count != 1 || neighbor_ids[0] != copy_value || neighbor_weights[0] != 1) {
        fprintf(stderr, "[value-ssa-analysis] FAIL: allocation prep top-neighbor helper mismatch\n");
        ok = 0;
    }
    if (!value_ssa_allocation_worklist_get_class_range(
            &worklist, VALUE_SSA_ALLOC_WORK_MOVE_HINT, &class_begin, &class_end) ||
        class_begin != 0 || class_end != 2) {
        fprintf(stderr, "[value-ssa-analysis] FAIL: allocation worklist class-range helper mismatch\n");
        ok = 0;
    }
    if (!value_ssa_allocation_worklist_get_class_range(
            &worklist, VALUE_SSA_ALLOC_WORK_CONSTRAINED, &class_begin, &class_end) ||
        class_begin != 0 || class_end != 0) {
        fprintf(stderr, "[value-ssa-analysis] FAIL: allocation worklist empty class-range mismatch\n");
        ok = 0;
    }

cleanup:
    free(prep_text);
    free(worklist_text);
    free(move_hint_text);
    free(constrained_text);
    value_ssa_allocation_worklist_free(&worklist);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

int main(void) {
    int ok = 1;

    ok &= test_value_ssa_liveness_phi_diamond();
    ok &= test_value_ssa_liveness_loop();
    ok &= test_value_ssa_interference_straight_line();
    ok &= test_value_ssa_interference_phi_and_loop();
    ok &= test_value_ssa_copy_affinity_graph();
    ok &= test_value_ssa_allocation_prep();
    ok &= test_value_ssa_allocation_prep_two_block_range();
    ok &= test_value_ssa_allocation_worklist();
    ok &= test_value_ssa_allocation_dump_helpers();
    if (!ok) {
        return 1;
    }

    printf("[value-ssa-analysis] PASS\n");
    return 0;
}
