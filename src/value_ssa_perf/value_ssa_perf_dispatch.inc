#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_SPLIT_AGGREGATOR)
#define VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT
#include "../value_ssa_pass/value_ssa_pass.c"
#else

typedef struct {
    long long constant;
    size_t case_target;
} ValueSsaPerfDispatchCase;

static int value_ssa_perf_dispatch_try_parse_compare_block(const ValueSsaBasicBlock *block,
    ValueSsaValueRef *out_compared_value,
    long long *out_constant,
    size_t *out_case_target,
    size_t *out_next_target) {
    const ValueSsaInstruction *instruction;
    ValueSsaValueRef variable;
    long long immediate;

    if (out_compared_value) {
        *out_compared_value = value_ssa_value_immediate(0);
    }
    if (out_constant) {
        *out_constant = 0;
    }
    if (out_case_target) {
        *out_case_target = 0u;
    }
    if (out_next_target) {
        *out_next_target = 0u;
    }
    if (!block || block->phi_count != 0u || block->instruction_count != 1u ||
        !block->has_terminator || block->terminator.kind != VALUE_SSA_TERM_BRANCH) {
        return 0;
    }

    instruction = &block->instructions[0];
    if (instruction->kind != VALUE_SSA_INSTR_BINARY ||
        !instruction->has_result ||
        !value_ssa_value_refs_equal(block->terminator.as.branch.condition, instruction->result) ||
        (instruction->as.binary.op != VALUE_SSA_BINARY_EQ &&
            instruction->as.binary.op != VALUE_SSA_BINARY_NE)) {
        return 0;
    }

    if (instruction->as.binary.lhs.kind == VALUE_SSA_VALUE_IMMEDIATE &&
        instruction->as.binary.rhs.kind != VALUE_SSA_VALUE_IMMEDIATE) {
        immediate = instruction->as.binary.lhs.immediate;
        variable = instruction->as.binary.rhs;
    } else if (instruction->as.binary.rhs.kind == VALUE_SSA_VALUE_IMMEDIATE &&
               instruction->as.binary.lhs.kind != VALUE_SSA_VALUE_IMMEDIATE) {
        immediate = instruction->as.binary.rhs.immediate;
        variable = instruction->as.binary.lhs;
    } else {
        return 0;
    }

    if (instruction->as.binary.op == VALUE_SSA_BINARY_EQ) {
        if (out_case_target) {
            *out_case_target = block->terminator.as.branch.then_target;
        }
        if (out_next_target) {
            *out_next_target = block->terminator.as.branch.else_target;
        }
    } else {
        if (out_case_target) {
            *out_case_target = block->terminator.as.branch.else_target;
        }
        if (out_next_target) {
            *out_next_target = block->terminator.as.branch.then_target;
        }
    }
    if (out_compared_value) {
        *out_compared_value = variable;
    }
    if (out_constant) {
        *out_constant = immediate;
    }
    return 1;
}

static int value_ssa_perf_dispatch_retarget_successor(ValueSsaBasicBlock *block,
    size_t old_target,
    size_t new_target) {
    if (!block || !block->has_terminator) {
        return 0;
    }

    switch (block->terminator.kind) {
    case VALUE_SSA_TERM_JUMP:
        if (block->terminator.as.jump_target != old_target) {
            return 0;
        }
        block->terminator.as.jump_target = new_target;
        return 1;
    case VALUE_SSA_TERM_BRANCH:
        if (block->terminator.as.branch.then_target == old_target) {
            block->terminator.as.branch.then_target = new_target;
            return 1;
        }
        if (block->terminator.as.branch.else_target == old_target) {
            block->terminator.as.branch.else_target = new_target;
            return 1;
        }
        return 0;
    default:
        return 0;
    }
}

static int value_ssa_perf_dispatch_sort_cases(ValueSsaPerfDispatchCase *cases, size_t count) {
    size_t i;
    size_t j;

    if (!cases) {
        return 0;
    }
    for (i = 0; i < count; ++i) {
        for (j = i + 1u; j < count; ++j) {
            if (cases[j].constant < cases[i].constant) {
                ValueSsaPerfDispatchCase tmp = cases[i];
                cases[i] = cases[j];
                cases[j] = tmp;
            }
        }
    }
    return 1;
}

static int value_ssa_perf_dispatch_collect_chain(const ValueSsaFunction *function,
    size_t root_block_id,
    ValueSsaValueRef *out_compared_value,
    ValueSsaPerfDispatchCase *cases,
    size_t case_capacity,
    size_t *out_case_count,
    size_t *out_default_target) {
    unsigned char *visited = NULL;
    ValueSsaValueRef compared_value;
    size_t current_block_id = root_block_id;
    size_t case_count = 0u;

    if (out_case_count) {
        *out_case_count = 0u;
    }
    if (out_default_target) {
        *out_default_target = 0u;
    }
    if (!function || !out_compared_value || !cases || !out_case_count || !out_default_target ||
        root_block_id >= function->block_count) {
        return 0;
    }

    visited = (unsigned char *)calloc(function->block_count, sizeof(unsigned char));
    if (!visited) {
        return 0;
    }

    while (current_block_id < function->block_count) {
        ValueSsaValueRef block_value;
        long long block_constant = 0;
        size_t case_target = 0u;
        size_t next_target = 0u;
        size_t existing_index;

        if (visited[current_block_id] ||
            !value_ssa_perf_dispatch_try_parse_compare_block(
                &function->blocks[current_block_id],
                &block_value,
                &block_constant,
                &case_target,
                &next_target)) {
            break;
        }
        visited[current_block_id] = 1u;

        if (case_count == 0u) {
            compared_value = block_value;
        } else if (!value_ssa_value_refs_equal(compared_value, block_value)) {
            free(visited);
            return 0;
        }

        for (existing_index = 0; existing_index < case_count; ++existing_index) {
            if (cases[existing_index].constant == block_constant) {
                free(visited);
                return 0;
            }
        }
        if (case_count >= case_capacity) {
            free(visited);
            return 0;
        }

        cases[case_count].constant = block_constant;
        cases[case_count].case_target = case_target;
        ++case_count;

        if (next_target >= function->block_count ||
            !value_ssa_perf_dispatch_try_parse_compare_block(
                &function->blocks[next_target],
                NULL,
                NULL,
                NULL,
                NULL)) {
            *out_case_count = case_count;
            *out_default_target = next_target;
            *out_compared_value = compared_value;
            free(visited);
            return case_count >= 2u;
        }
        current_block_id = next_target;
    }

    free(visited);
    return 0;
}

static int value_ssa_perf_dispatch_build_tree(ValueSsaFunction *function,
    ValueSsaValueRef compared_value,
    const ValueSsaPerfDispatchCase *cases,
    size_t begin,
    size_t end,
    size_t default_target,
    size_t *out_root_block_id,
    ValueSsaError *error) {
    size_t block_id;
    ValueSsaBasicBlock *block = NULL;

    if (!function || !cases || !out_root_block_id || begin >= end) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-346: invalid dispatch-tree build contract");
        return 0;
    }

    if (!value_ssa_function_append_block(function, &block_id, &block, error)) {
        return 0;
    }
    *out_root_block_id = block_id;

    if (end - begin == 1u) {
        size_t cmp_id = value_ssa_function_allocate_value(function);
        ValueSsaInstruction instruction;

        if (cmp_id == (size_t)-1) {
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-347: failed allocating dispatch leaf compare");
            return 0;
        }

        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = VALUE_SSA_INSTR_BINARY;
        instruction.has_result = 1;
        instruction.result = value_ssa_value_id(cmp_id);
        instruction.as.binary.op = VALUE_SSA_BINARY_EQ;
        instruction.as.binary.lhs = compared_value;
        instruction.as.binary.rhs = value_ssa_value_immediate(cases[begin].constant);
        if (!value_ssa_block_append_instruction(block, &instruction, error) ||
            !value_ssa_block_set_branch(
                block,
                value_ssa_value_id(cmp_id),
                cases[begin].case_target,
                default_target,
                error)) {
            return 0;
        }

        return 1;
    }

    {
        size_t mid = begin + (end - begin) / 2u;
        size_t cmp_id = value_ssa_function_allocate_value(function);
        size_t left_root = 0u;
        size_t right_root = 0u;
        ValueSsaInstruction instruction;

        if (cmp_id == (size_t)-1) {
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-348: failed allocating dispatch interior compare");
            return 0;
        }
        if (!value_ssa_perf_dispatch_build_tree(
                function, compared_value, cases, begin, mid, default_target, &left_root, error) ||
            !value_ssa_perf_dispatch_build_tree(
                function, compared_value, cases, mid, end, default_target, &right_root, error)) {
            return 0;
        }
        block = &function->blocks[block_id];

        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = VALUE_SSA_INSTR_BINARY;
        instruction.has_result = 1;
        instruction.result = value_ssa_value_id(cmp_id);
        instruction.as.binary.op = VALUE_SSA_BINARY_LT;
        instruction.as.binary.lhs = compared_value;
        instruction.as.binary.rhs = value_ssa_value_immediate(cases[mid].constant);
        if (!value_ssa_block_append_instruction(block, &instruction, error) ||
            !value_ssa_block_set_branch(
                block,
                value_ssa_value_id(cmp_id),
                left_root,
                right_root,
                error)) {
            return 0;
        }
    }

    return 1;
}

static int value_ssa_perf_rebalance_integer_dispatch_chains(ValueSsaProgram *program,
    ValueSsaError *error) {
    size_t function_index;
    int changed_any = 0;

    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-349: invalid dispatch-chain rebalance contract");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunction *function = &program->functions[function_index];
        size_t original_block_count = function->block_count;
        size_t predecessor_index;
        int changed_function = 0;

        if (!function->has_body) {
            continue;
        }

        for (predecessor_index = 0; predecessor_index < original_block_count; ++predecessor_index) {
            ValueSsaBasicBlock *predecessor = &function->blocks[predecessor_index];
            size_t root_block_id;
            ValueSsaPerfDispatchCase cases[16];
            size_t case_count = 0u;
            size_t default_target = 0u;
            ValueSsaValueRef compared_value;
            size_t new_root_id = 0u;
            size_t target_root;

            if (!predecessor->has_terminator) {
                continue;
            }
            if (predecessor->terminator.kind == VALUE_SSA_TERM_JUMP) {
                target_root = predecessor->terminator.as.jump_target;
            } else if (predecessor->terminator.kind == VALUE_SSA_TERM_BRANCH) {
                size_t then_root = predecessor->terminator.as.branch.then_target;
                size_t else_root = predecessor->terminator.as.branch.else_target;

                if (then_root < function->block_count &&
                    value_ssa_perf_dispatch_try_parse_compare_block(
                        &function->blocks[then_root], NULL, NULL, NULL, NULL)) {
                    target_root = then_root;
                } else if (else_root < function->block_count &&
                           value_ssa_perf_dispatch_try_parse_compare_block(
                               &function->blocks[else_root], NULL, NULL, NULL, NULL)) {
                    target_root = else_root;
                } else {
                    continue;
                }
            } else {
                continue;
            }

            root_block_id = target_root;
            if (!value_ssa_perf_dispatch_collect_chain(
                    function,
                    root_block_id,
                    &compared_value,
                    cases,
                    sizeof(cases) / sizeof(cases[0]),
                    &case_count,
                    &default_target)) {
                continue;
            }
            if (!value_ssa_perf_dispatch_sort_cases(cases, case_count) ||
                !value_ssa_perf_dispatch_build_tree(
                    function,
                    compared_value,
                    cases,
                    0u,
                    case_count,
                    default_target,
                    &new_root_id,
                    error) ||
                !value_ssa_perf_dispatch_retarget_successor(predecessor, root_block_id, new_root_id)) {
                return 0;
            }

            changed_function = 1;
            changed_any = 1;
        }

        if (changed_function &&
            (!value_ssa_simplify_cfg(program, error) ||
                !value_ssa_simplify_trivial_values(program, error) ||
                !value_ssa_eliminate_dead_value_defs(program, error))) {
            return 0;
        }
    }

    (void)changed_any;
    return value_ssa_verify_program(program, error);
}

#endif
