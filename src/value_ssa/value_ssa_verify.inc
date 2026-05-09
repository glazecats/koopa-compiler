#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_SPLIT_AGGREGATOR)
#define VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa.c"
#else

static int value_ssa_verify_value_ref(ValueSsaValueRef value,
    size_t next_value_id,
    const char *role,
    ValueSsaError *error) {
    switch (value.kind) {
    case VALUE_SSA_VALUE_IMMEDIATE:
        return 1;
    case VALUE_SSA_VALUE_ID:
        if (value.value_id >= next_value_id) {
            value_ssa_set_error(error,
                0,
                0,
                "VALUE-SSA-035: %s references ssa.%zu outside value range",
                role ? role : "value",
                value.value_id);
            return 0;
        }
        return 1;
    default:
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-036: %s uses unknown value kind %d",
            role ? role : "value",
            (int)value.kind);
        return 0;
    }
}

static int value_ssa_verify_slot_ref(const ValueSsaProgram *program,
    const ValueSsaFunction *function,
    ValueSsaSlotRef slot,
    ValueSsaSlotKind expected_kind,
    const char *role,
    ValueSsaError *error) {
    if (slot.kind != expected_kind) {
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-037: %s uses wrong slot kind %d",
            role ? role : "slot",
            (int)slot.kind);
        return 0;
    }

    switch (slot.kind) {
    case VALUE_SSA_SLOT_LOCAL:
        if (!function || slot.id >= function->local_count) {
            value_ssa_set_error(error,
                0,
                0,
                "VALUE-SSA-038: %s references local.%zu outside local table",
                role ? role : "slot",
                slot.id);
            return 0;
        }
        return 1;
    case VALUE_SSA_SLOT_GLOBAL:
        if (!program || slot.id >= program->global_count) {
            value_ssa_set_error(error,
                0,
                0,
                "VALUE-SSA-039: %s references global.%zu outside global table",
                role ? role : "slot",
                slot.id);
            return 0;
        }
        return 1;
    default:
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-040: %s uses unknown slot kind %d",
            role ? role : "slot",
            (int)slot.kind);
        return 0;
    }
}

static int value_ssa_verify_binary_op(ValueSsaBinaryOp op,
    size_t block_id,
    size_t instruction_index,
    ValueSsaError *error) {
    switch (op) {
    case VALUE_SSA_BINARY_ADD:
    case VALUE_SSA_BINARY_SUB:
    case VALUE_SSA_BINARY_MUL:
    case VALUE_SSA_BINARY_DIV:
    case VALUE_SSA_BINARY_MOD:
    case VALUE_SSA_BINARY_BIT_AND:
    case VALUE_SSA_BINARY_BIT_XOR:
    case VALUE_SSA_BINARY_BIT_OR:
    case VALUE_SSA_BINARY_SHIFT_LEFT:
    case VALUE_SSA_BINARY_SHIFT_RIGHT:
    case VALUE_SSA_BINARY_EQ:
    case VALUE_SSA_BINARY_NE:
    case VALUE_SSA_BINARY_LT:
    case VALUE_SSA_BINARY_LE:
    case VALUE_SSA_BINARY_GT:
    case VALUE_SSA_BINARY_GE:
        return 1;
    default:
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-041: bb.%zu instruction %zu uses unknown binary op %d",
            block_id,
            instruction_index,
            (int)op);
        return 0;
    }
}

static const ValueSsaFunction *value_ssa_find_program_function(const ValueSsaProgram *program, const char *name) {
    size_t i;

    if (!program || !name || name[0] == '\0') {
        return NULL;
    }

    for (i = 0; i < program->function_count; ++i) {
        const ValueSsaFunction *function = &program->functions[i];

        if (function->name && strcmp(function->name, name) == 0) {
            return function;
        }
    }

    return NULL;
}

static int value_ssa_verify_instruction(const ValueSsaProgram *program,
    const ValueSsaFunction *function,
    const ValueSsaBasicBlock *block,
    size_t instruction_index,
    const ValueSsaInstruction *instruction,
    ValueSsaError *error) {
    size_t arg_index;

    if (!program || !function || !block || !instruction) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-042: invalid verifier contract");
        return 0;
    }

    switch (instruction->kind) {
    case VALUE_SSA_INSTR_MOV:
        if (!instruction->has_result || instruction->result.kind != VALUE_SSA_VALUE_ID) {
            value_ssa_set_error(error,
                0,
                0,
                "VALUE-SSA-043: bb.%zu instruction %zu mov must define an SSA result",
                block->id,
                instruction_index);
            return 0;
        }
        return value_ssa_verify_value_ref(instruction->as.mov_value,
            function->next_value_id,
            "mov operand",
            error);
    case VALUE_SSA_INSTR_BINARY:
        if (!instruction->has_result || instruction->result.kind != VALUE_SSA_VALUE_ID) {
            value_ssa_set_error(error,
                0,
                0,
                "VALUE-SSA-044: bb.%zu instruction %zu binary must define an SSA result",
                block->id,
                instruction_index);
            return 0;
        }
        if (!value_ssa_verify_binary_op(instruction->as.binary.op, block->id, instruction_index, error) ||
            !value_ssa_verify_value_ref(instruction->as.binary.lhs,
                function->next_value_id,
                "binary lhs",
                error) ||
            !value_ssa_verify_value_ref(instruction->as.binary.rhs,
                function->next_value_id,
                "binary rhs",
                error)) {
            return 0;
        }
        return 1;
    case VALUE_SSA_INSTR_CALL:
        if (instruction->has_result && instruction->result.kind != VALUE_SSA_VALUE_ID) {
            value_ssa_set_error(error,
                0,
                0,
                "VALUE-SSA-045: bb.%zu instruction %zu call result must be an SSA value id",
                block->id,
                instruction_index);
            return 0;
        }
        if (!instruction->as.call.callee_name || instruction->as.call.callee_name[0] == '\0') {
            value_ssa_set_error(error,
                0,
                0,
                "VALUE-SSA-046: bb.%zu instruction %zu call must name a callee",
                block->id,
                instruction_index);
            return 0;
        }
        if (instruction->as.call.arg_count > 0 && !instruction->as.call.args) {
            value_ssa_set_error(error,
                0,
                0,
                "VALUE-SSA-047: bb.%zu instruction %zu call has arg_count=%zu but args table is NULL",
                block->id,
                instruction_index,
                instruction->as.call.arg_count);
            return 0;
        }
        if (instruction->as.call.arg_count == 0 && instruction->as.call.args) {
            value_ssa_set_error(error,
                0,
                0,
                "VALUE-SSA-058: bb.%zu instruction %zu call has arg_count=0 but non-NULL args table",
                block->id,
                instruction_index);
            return 0;
        }
        for (arg_index = 0; arg_index < instruction->as.call.arg_count; ++arg_index) {
            if (!value_ssa_verify_value_ref(instruction->as.call.args[arg_index],
                    function->next_value_id,
                    "call arg",
                    error)) {
                return 0;
            }
        }
        {
            const ValueSsaFunction *callee =
                value_ssa_find_program_function(program, instruction->as.call.callee_name);

            if (!callee) {
                value_ssa_set_error(error,
                    0,
                    0,
                    "VALUE-SSA-048: bb.%zu instruction %zu calls unknown function '%s'",
                    block->id,
                    instruction_index,
                    instruction->as.call.callee_name);
                return 0;
            }
            if (instruction->as.call.arg_count != callee->parameter_count) {
                value_ssa_set_error(error,
                    0,
                    0,
                    "VALUE-SSA-049: bb.%zu instruction %zu calls '%s' with %zu args but signature expects %zu",
                    block->id,
                    instruction_index,
                    instruction->as.call.callee_name,
                    instruction->as.call.arg_count,
                    callee->parameter_count);
                return 0;
            }
        }
        return 1;
    case VALUE_SSA_INSTR_ADDR_LOCAL:
    case VALUE_SSA_INSTR_ADDR_GLOBAL:
        if (!instruction->has_result || instruction->result.kind != VALUE_SSA_VALUE_ID) {
            value_ssa_set_error(error,
                0,
                0,
                "VALUE-SSA-055: bb.%zu instruction %zu addr_* must define an SSA result",
                block->id,
                instruction_index);
            return 0;
        }
        return value_ssa_verify_slot_ref(program,
            function,
            instruction->as.addr_slot,
            instruction->kind == VALUE_SSA_INSTR_ADDR_LOCAL
                ? VALUE_SSA_SLOT_LOCAL
                : VALUE_SSA_SLOT_GLOBAL,
            instruction->kind == VALUE_SSA_INSTR_ADDR_LOCAL
                ? "addr_local slot"
                : "addr_global slot",
            error);
    case VALUE_SSA_INSTR_LOAD_LOCAL:
        if (!instruction->has_result || instruction->result.kind != VALUE_SSA_VALUE_ID) {
            value_ssa_set_error(error,
                0,
                0,
                "VALUE-SSA-050: bb.%zu instruction %zu load_local must define an SSA result",
                block->id,
                instruction_index);
            return 0;
        }
        return value_ssa_verify_slot_ref(program,
            function,
            instruction->as.load_slot,
            VALUE_SSA_SLOT_LOCAL,
            "load_local slot",
            error);
    case VALUE_SSA_INSTR_STORE_LOCAL:
        if (instruction->has_result) {
            value_ssa_set_error(error,
                0,
                0,
                "VALUE-SSA-051: bb.%zu instruction %zu store_local must not define a result",
                block->id,
                instruction_index);
            return 0;
        }
        return value_ssa_verify_slot_ref(program,
                function,
                instruction->as.store.slot,
                VALUE_SSA_SLOT_LOCAL,
                "store_local slot",
                error) &&
            value_ssa_verify_value_ref(instruction->as.store.value,
                function->next_value_id,
                "store_local value",
                error);
    case VALUE_SSA_INSTR_LOAD_GLOBAL:
        if (!instruction->has_result || instruction->result.kind != VALUE_SSA_VALUE_ID) {
            value_ssa_set_error(error,
                0,
                0,
                "VALUE-SSA-052: bb.%zu instruction %zu load_global must define an SSA result",
                block->id,
                instruction_index);
            return 0;
        }
        return value_ssa_verify_slot_ref(program,
            function,
            instruction->as.load_slot,
            VALUE_SSA_SLOT_GLOBAL,
            "load_global slot",
            error);
    case VALUE_SSA_INSTR_STORE_GLOBAL:
        if (instruction->has_result) {
            value_ssa_set_error(error,
                0,
                0,
                "VALUE-SSA-053: bb.%zu instruction %zu store_global must not define a result",
                block->id,
                instruction_index);
            return 0;
        }
        return value_ssa_verify_slot_ref(program,
                function,
                instruction->as.store.slot,
                VALUE_SSA_SLOT_GLOBAL,
                "store_global slot",
                error) &&
            value_ssa_verify_value_ref(instruction->as.store.value,
                function->next_value_id,
                "store_global value",
                error);
    case VALUE_SSA_INSTR_LOAD_INDIRECT:
        if (!instruction->has_result || instruction->result.kind != VALUE_SSA_VALUE_ID) {
            value_ssa_set_error(error,
                0,
                0,
                "VALUE-SSA-056: bb.%zu instruction %zu load_indirect must define an SSA result",
                block->id,
                instruction_index);
            return 0;
        }
        return value_ssa_verify_value_ref(instruction->as.load_indirect_addr,
            function->next_value_id,
            "load_indirect addr",
            error);
    case VALUE_SSA_INSTR_STORE_INDIRECT:
        if (instruction->has_result) {
            value_ssa_set_error(error,
                0,
                0,
                "VALUE-SSA-057: bb.%zu instruction %zu store_indirect must not define a result",
                block->id,
                instruction_index);
            return 0;
        }
        return value_ssa_verify_value_ref(instruction->as.store_indirect.addr,
                function->next_value_id,
                "store_indirect addr",
                error) &&
            value_ssa_verify_value_ref(instruction->as.store_indirect.value,
                function->next_value_id,
                "store_indirect value",
                error);
    default:
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-054: bb.%zu instruction %zu uses unknown instruction kind %d",
            block->id,
            instruction_index,
            (int)instruction->kind);
        return 0;
    }
}

static int value_ssa_verify_terminator(const ValueSsaFunction *function,
    const ValueSsaBasicBlock *block,
    const ValueSsaTerminator *terminator,
    ValueSsaError *error) {
    if (!function || !block || !terminator) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-042: invalid verifier contract");
        return 0;
    }

    switch (terminator->kind) {
    case VALUE_SSA_TERM_RETURN:
        if (!terminator->has_return_value) {
            return 1;
        }
        return value_ssa_verify_value_ref(terminator->as.return_value,
            function->next_value_id,
            "return value",
            error);
    case VALUE_SSA_TERM_JUMP:
        if (terminator->as.jump_target >= function->block_count) {
            value_ssa_set_error(error,
                0,
                0,
                "VALUE-SSA-055: bb.%zu jumps to bb.%zu outside block table",
                block->id,
                terminator->as.jump_target);
            return 0;
        }
        return 1;
    case VALUE_SSA_TERM_BRANCH:
        if (!value_ssa_verify_value_ref(terminator->as.branch.condition,
                function->next_value_id,
                "branch condition",
                error)) {
            return 0;
        }
        if (terminator->as.branch.then_target >= function->block_count ||
            terminator->as.branch.else_target >= function->block_count) {
            value_ssa_set_error(error,
                0,
                0,
                "VALUE-SSA-056: bb.%zu branches to target outside block table",
                block->id);
            return 0;
        }
        return 1;
    default:
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-057: bb.%zu uses unknown terminator kind %d",
            block->id,
            (int)terminator->kind);
        return 0;
    }
}

static int value_ssa_build_predecessor_matrix(const ValueSsaFunction *function,
    unsigned char *predecessor_matrix,
    size_t *predecessor_counts,
    ValueSsaError *error) {
    size_t i;
    size_t block_count;

    if (!function || !predecessor_matrix || !predecessor_counts) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-042: invalid verifier contract");
        return 0;
    }

    block_count = function->block_count;
    for (i = 0; i < block_count; ++i) {
        const ValueSsaBasicBlock *block = &function->blocks[i];
        size_t target;

        switch (block->terminator.kind) {
        case VALUE_SSA_TERM_RETURN:
            break;
        case VALUE_SSA_TERM_JUMP:
            target = block->terminator.as.jump_target;
            if (!predecessor_matrix[i * block_count + target]) {
                predecessor_matrix[i * block_count + target] = 1;
                ++predecessor_counts[target];
            }
            break;
        case VALUE_SSA_TERM_BRANCH:
            target = block->terminator.as.branch.then_target;
            if (!predecessor_matrix[i * block_count + target]) {
                predecessor_matrix[i * block_count + target] = 1;
                ++predecessor_counts[target];
            }
            target = block->terminator.as.branch.else_target;
            if (!predecessor_matrix[i * block_count + target]) {
                predecessor_matrix[i * block_count + target] = 1;
                ++predecessor_counts[target];
            }
            break;
        default:
            value_ssa_set_error(error,
                0,
                0,
                "VALUE-SSA-057: bb.%zu uses unknown terminator kind %d",
                block->id,
                (int)block->terminator.kind);
            return 0;
        }
    }

    return 1;
}

static int value_ssa_mark_reachable_blocks(const ValueSsaFunction *function,
    unsigned char *reachable,
    ValueSsaError *error) {
    size_t *stack = NULL;
    size_t stack_size = 0;
    size_t block_count;

    if (!function || !reachable) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-042: invalid verifier contract");
        return 0;
    }
    if (function->block_count == 0) {
        return 1;
    }

    block_count = function->block_count;
    stack = (size_t *)malloc(block_count * sizeof(size_t));
    if (!stack) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-058: out of memory while walking CFG reachability");
        return 0;
    }

    stack[stack_size++] = 0;
    reachable[0] = 1;

    while (stack_size > 0) {
        const ValueSsaBasicBlock *block = &function->blocks[stack[--stack_size]];

        switch (block->terminator.kind) {
        case VALUE_SSA_TERM_RETURN:
            break;
        case VALUE_SSA_TERM_JUMP:
            if (!reachable[block->terminator.as.jump_target]) {
                reachable[block->terminator.as.jump_target] = 1;
                stack[stack_size++] = block->terminator.as.jump_target;
            }
            break;
        case VALUE_SSA_TERM_BRANCH:
            if (!reachable[block->terminator.as.branch.then_target]) {
                reachable[block->terminator.as.branch.then_target] = 1;
                stack[stack_size++] = block->terminator.as.branch.then_target;
            }
            if (!reachable[block->terminator.as.branch.else_target]) {
                reachable[block->terminator.as.branch.else_target] = 1;
                stack[stack_size++] = block->terminator.as.branch.else_target;
            }
            break;
        default:
            free(stack);
            value_ssa_set_error(error,
                0,
                0,
                "VALUE-SSA-057: bb.%zu uses unknown terminator kind %d",
                block->id,
                (int)block->terminator.kind);
            return 0;
        }
    }

    free(stack);
    return 1;
}

static int value_ssa_verify_reachability(const ValueSsaFunction *function,
    const unsigned char *reachable,
    ValueSsaError *error) {
    size_t i;

    if (!function || !reachable) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-042: invalid verifier contract");
        return 0;
    }

    for (i = 0; i < function->block_count; ++i) {
        if (!reachable[i]) {
            value_ssa_set_error(error,
                0,
                0,
                "VALUE-SSA-059: function '%s' contains unreachable bb.%zu",
                function->name ? function->name : "<anon>",
                function->blocks[i].id);
            return 0;
        }
    }

    return 1;
}

static int value_ssa_verify_value_definitions(const ValueSsaFunction *function, ValueSsaError *error) {
    size_t *definition_counts = NULL;
    size_t i;

    if (!function) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-042: invalid verifier contract");
        return 0;
    }
    if (function->next_value_id == 0) {
        return 1;
    }

    definition_counts = (size_t *)calloc(function->next_value_id, sizeof(size_t));
    if (!definition_counts) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-060: out of memory while tracking SSA definitions");
        return 0;
    }

    for (i = 0; i < function->block_count; ++i) {
        const ValueSsaBasicBlock *block = &function->blocks[i];
        size_t j;

        for (j = 0; j < block->phi_count; ++j) {
            const ValueSsaPhi *phi = &block->phis[j];

            if (phi->result_id >= function->next_value_id) {
                free(definition_counts);
                value_ssa_set_error(error,
                    0,
                    0,
                    "VALUE-SSA-061: bb.%zu phi %zu defines ssa.%zu outside value range",
                    block->id,
                    j,
                    phi->result_id);
                return 0;
            }
            ++definition_counts[phi->result_id];
            if (definition_counts[phi->result_id] > 1) {
                free(definition_counts);
                value_ssa_set_error(error,
                    0,
                    0,
                    "VALUE-SSA-062: function '%s' defines ssa.%zu more than once",
                    function->name ? function->name : "<anon>",
                    phi->result_id);
                return 0;
            }
        }

        for (j = 0; j < block->instruction_count; ++j) {
            const ValueSsaInstruction *instruction = &block->instructions[j];

            if (!instruction->has_result) {
                continue;
            }
            if (instruction->result.kind != VALUE_SSA_VALUE_ID ||
                instruction->result.value_id >= function->next_value_id) {
                free(definition_counts);
                value_ssa_set_error(error,
                    0,
                    0,
                    "VALUE-SSA-063: bb.%zu instruction %zu defines invalid SSA result",
                    block->id,
                    j);
                return 0;
            }
            ++definition_counts[instruction->result.value_id];
            if (definition_counts[instruction->result.value_id] > 1) {
                free(definition_counts);
                value_ssa_set_error(error,
                    0,
                    0,
                    "VALUE-SSA-062: function '%s' defines ssa.%zu more than once",
                    function->name ? function->name : "<anon>",
                    instruction->result.value_id);
                return 0;
            }
        }
    }

    for (i = 0; i < function->next_value_id; ++i) {
        if (definition_counts[i] != 1) {
            free(definition_counts);
            value_ssa_set_error(error,
                0,
                0,
                "VALUE-SSA-064: function '%s' does not define ssa.%zu exactly once",
                function->name ? function->name : "<anon>",
                i);
            return 0;
        }
    }

    free(definition_counts);
    return 1;
}

static int value_ssa_verify_phi_structure(const ValueSsaFunction *function,
    const unsigned char *reachable,
    const unsigned char *predecessor_matrix,
    const size_t *predecessor_counts,
    ValueSsaError *error) {
    size_t i;

    if (!function || !reachable || !predecessor_matrix || !predecessor_counts) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-042: invalid verifier contract");
        return 0;
    }

    for (i = 0; i < function->block_count; ++i) {
        const ValueSsaBasicBlock *block = &function->blocks[i];
        size_t j;

        if (!reachable[i]) {
            continue;
        }
        if (i == 0 && block->phi_count > 0) {
            value_ssa_set_error(error,
                0,
                0,
                "VALUE-SSA-065: entry block bb.0 must not contain phi nodes");
            return 0;
        }

        for (j = 0; j < block->phi_count; ++j) {
            const ValueSsaPhi *phi = &block->phis[j];
            unsigned char *seen_predecessors = NULL;
            size_t input_index;

            if (phi->input_count != predecessor_counts[i]) {
                value_ssa_set_error(error,
                    0,
                    0,
                    "VALUE-SSA-066: bb.%zu phi %zu has %zu inputs but CFG predecessor count is %zu",
                    block->id,
                    j,
                    phi->input_count,
                    predecessor_counts[i]);
                return 0;
            }
            if (phi->input_count > 0 && !phi->inputs) {
                value_ssa_set_error(error,
                    0,
                    0,
                    "VALUE-SSA-067: bb.%zu phi %zu has input_count=%zu but input table is NULL",
                    block->id,
                    j,
                    phi->input_count);
                return 0;
            }

            seen_predecessors = (unsigned char *)calloc(function->block_count, sizeof(unsigned char));
            if (!seen_predecessors) {
                value_ssa_set_error(error, 0, 0, "VALUE-SSA-068: out of memory while validating phi predecessors");
                return 0;
            }

            for (input_index = 0; input_index < phi->input_count; ++input_index) {
                const ValueSsaPhiInput *input = &phi->inputs[input_index];

                if (input->predecessor_block_id >= function->block_count ||
                    !predecessor_matrix[input->predecessor_block_id * function->block_count + i]) {
                    free(seen_predecessors);
                    value_ssa_set_error(error,
                        0,
                        0,
                        "VALUE-SSA-069: bb.%zu phi %zu references non-predecessor bb.%zu",
                        block->id,
                        j,
                        input->predecessor_block_id);
                    return 0;
                }
                if (seen_predecessors[input->predecessor_block_id]) {
                    free(seen_predecessors);
                    value_ssa_set_error(error,
                        0,
                        0,
                        "VALUE-SSA-070: bb.%zu phi %zu repeats predecessor bb.%zu",
                        block->id,
                        j,
                        input->predecessor_block_id);
                    return 0;
                }
                seen_predecessors[input->predecessor_block_id] = 1;
                if (!value_ssa_verify_value_ref(input->value,
                        function->next_value_id,
                        "phi input",
                        error)) {
                    free(seen_predecessors);
                    return 0;
                }
            }

            free(seen_predecessors);
        }
    }

    return 1;
}

static int value_ssa_verify_value_availability(const ValueSsaFunction *function,
    const unsigned char *reachable,
    const unsigned char *predecessor_matrix,
    ValueSsaError *error) {
    unsigned char *dominates = NULL;
    unsigned char *scratch = NULL;
    size_t *def_block_ids = NULL;
    size_t *def_instruction_indices = NULL;
    unsigned char *def_is_phi = NULL;
    size_t block_count;
    size_t value_count;
    size_t block_index;
    int changed;

    if (!function || !reachable || !predecessor_matrix) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-042: invalid verifier contract");
        return 0;
    }

    block_count = function->block_count;
    value_count = function->next_value_id;
    if (block_count == 0 || value_count == 0) {
        return 1;
    }

    dominates = (unsigned char *)calloc(block_count * block_count, sizeof(unsigned char));
    scratch = (unsigned char *)malloc(block_count * sizeof(unsigned char));
    def_block_ids = (size_t *)malloc(value_count * sizeof(size_t));
    def_instruction_indices = (size_t *)malloc(value_count * sizeof(size_t));
    def_is_phi = (unsigned char *)calloc(value_count, sizeof(unsigned char));
    if (!dominates || !scratch || !def_block_ids || !def_instruction_indices || !def_is_phi) {
        free(dominates);
        free(scratch);
        free(def_block_ids);
        free(def_instruction_indices);
        free(def_is_phi);
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-072: out of memory while computing SSA availability");
        return 0;
    }

    for (block_index = 0; block_index < block_count; ++block_index) {
        size_t dom_index;

        for (dom_index = 0; dom_index < block_count; ++dom_index) {
            dominates[dom_index * block_count + block_index] = 0u;
        }
        if (!reachable[block_index]) {
            continue;
        }
        if (block_index == 0) {
            dominates[0] = 1u;
        } else {
            for (dom_index = 0; dom_index < block_count; ++dom_index) {
                dominates[dom_index * block_count + block_index] = reachable[dom_index] ? 1u : 0u;
            }
            dominates[block_index * block_count + block_index] = 1u;
        }
    }

    do {
        changed = 0;
        for (block_index = 0; block_index < block_count; ++block_index) {
            size_t dom_index;

            if (!reachable[block_index]) {
                continue;
            }
            if (block_index == 0) {
                continue;
            } else {
                int has_predecessor = 0;
                size_t pred_index;

                for (dom_index = 0; dom_index < block_count; ++dom_index) {
                    scratch[dom_index] = reachable[dom_index] ? 1u : 0u;
                }
                for (pred_index = 0; pred_index < block_count; ++pred_index) {
                    if (!predecessor_matrix[pred_index * block_count + block_index]) {
                        continue;
                    }
                    has_predecessor = 1;
                    for (dom_index = 0; dom_index < block_count; ++dom_index) {
                        scratch[dom_index] = (unsigned char)(
                            scratch[dom_index] && dominates[dom_index * block_count + pred_index]);
                    }
                }
                if (!has_predecessor) {
                    memset(scratch, 0, block_count);
                }
                scratch[block_index] = 1u;
                for (dom_index = 0; dom_index < block_count; ++dom_index) {
                    if (dominates[dom_index * block_count + block_index] != scratch[dom_index]) {
                        dominates[dom_index * block_count + block_index] = scratch[dom_index];
                        changed = 1;
                    }
                }
            }
        }
    } while (changed);

    for (block_index = 0; block_index < value_count; ++block_index) {
        def_block_ids[block_index] = (size_t)-1;
        def_instruction_indices[block_index] = (size_t)-1;
    }
    for (block_index = 0; block_index < function->block_count; ++block_index) {
        const ValueSsaBasicBlock *block = &function->blocks[block_index];
        size_t phi_index;
        size_t instruction_index;

        for (phi_index = 0; phi_index < block->phi_count; ++phi_index) {
            def_block_ids[block->phis[phi_index].result_id] = block_index;
            def_instruction_indices[block->phis[phi_index].result_id] = 0u;
            def_is_phi[block->phis[phi_index].result_id] = 1u;
        }
        for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
            const ValueSsaInstruction *instruction = &block->instructions[instruction_index];

            if (instruction->has_result && instruction->result.kind == VALUE_SSA_VALUE_ID) {
                def_block_ids[instruction->result.value_id] = block_index;
                def_instruction_indices[instruction->result.value_id] = instruction_index;
            }
        }
    }

#define VALUE_SSA_REQUIRE_AVAILABLE_IN_BLOCK(_block_id, _instruction_index, _value, _role)                               \
    do {                                                                                                                   \
        if ((_value).kind == VALUE_SSA_VALUE_ID) {                                                                         \
            size_t _required_value_id = (_value).value_id;                                                                 \
            if (_required_value_id >= value_count || def_block_ids[_required_value_id] == (size_t)-1) {                  \
                value_ssa_set_error(error, 0, 0, "VALUE-SSA-071: bb.%zu uses unavailable ssa.%zu as %s",                \
                    (_block_id), _required_value_id, (_role));                                                             \
                free(dominates);                                                                                           \
                free(scratch);                                                                                             \
                free(def_block_ids);                                                                                       \
                free(def_instruction_indices);                                                                             \
                free(def_is_phi);                                                                                          \
                return 0;                                                                                                  \
            }                                                                                                              \
            if (def_block_ids[_required_value_id] == (_block_id)) {                                                       \
                if (!def_is_phi[_required_value_id] && def_instruction_indices[_required_value_id] >= (_instruction_index)) { \
                    value_ssa_set_error(error, 0, 0, "VALUE-SSA-071: bb.%zu uses unavailable ssa.%zu as %s",            \
                        (_block_id), _required_value_id, (_role));                                                         \
                    free(dominates);                                                                                       \
                    free(scratch);                                                                                         \
                    free(def_block_ids);                                                                                   \
                    free(def_instruction_indices);                                                                         \
                    free(def_is_phi);                                                                                      \
                    return 0;                                                                                              \
                }                                                                                                          \
            } else if (!dominates[def_block_ids[_required_value_id] * block_count + (_block_id)]) {                      \
                value_ssa_set_error(error, 0, 0, "VALUE-SSA-071: bb.%zu uses unavailable ssa.%zu as %s",                \
                    (_block_id), _required_value_id, (_role));                                                             \
                free(dominates);                                                                                           \
                free(scratch);                                                                                             \
                free(def_block_ids);                                                                                       \
                free(def_instruction_indices);                                                                             \
                free(def_is_phi);                                                                                          \
                return 0;                                                                                                  \
            }                                                                                                              \
        }                                                                                                                  \
    } while (0)

    for (block_index = 0; block_index < block_count; ++block_index) {
        const ValueSsaBasicBlock *block = &function->blocks[block_index];
        size_t phi_index;
        size_t instruction_index;

        if (!reachable[block_index]) {
            continue;
        }

        for (phi_index = 0; phi_index < block->phi_count; ++phi_index) {
            const ValueSsaPhi *phi = &block->phis[phi_index];
            size_t input_index;

            for (input_index = 0; input_index < phi->input_count; ++input_index) {
                const ValueSsaPhiInput *input = &phi->inputs[input_index];

                if (input->value.kind == VALUE_SSA_VALUE_ID) {
                    size_t input_value_id = input->value.value_id;

                    if (input_value_id >= value_count || def_block_ids[input_value_id] == (size_t)-1) {
                        value_ssa_set_error(error,
                            0,
                            0,
                            "VALUE-SSA-071: bb.%zu uses unavailable ssa.%zu as phi input",
                            block->id,
                            input_value_id);
                        free(dominates);
                        free(scratch);
                        free(def_block_ids);
                        free(def_instruction_indices);
                        free(def_is_phi);
                        return 0;
                    }
                    if (def_block_ids[input_value_id] != input->predecessor_block_id &&
                        !dominates[def_block_ids[input_value_id] * block_count + input->predecessor_block_id]) {
                        value_ssa_set_error(error,
                            0,
                            0,
                            "VALUE-SSA-071: bb.%zu uses unavailable ssa.%zu as phi input",
                            block->id,
                            input_value_id);
                        free(dominates);
                        free(scratch);
                        free(def_block_ids);
                        free(def_instruction_indices);
                        free(def_is_phi);
                        return 0;
                    }
                }
            }
        }

        for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
            const ValueSsaInstruction *instruction = &block->instructions[instruction_index];

            switch (instruction->kind) {
            case VALUE_SSA_INSTR_MOV:
                VALUE_SSA_REQUIRE_AVAILABLE_IN_BLOCK(block->id, instruction_index, instruction->as.mov_value, "mov operand");
                break;
            case VALUE_SSA_INSTR_BINARY:
                VALUE_SSA_REQUIRE_AVAILABLE_IN_BLOCK(block->id, instruction_index, instruction->as.binary.lhs, "binary lhs");
                VALUE_SSA_REQUIRE_AVAILABLE_IN_BLOCK(block->id, instruction_index, instruction->as.binary.rhs, "binary rhs");
                break;
            case VALUE_SSA_INSTR_CALL:
                {
                    size_t arg_index;

                    for (arg_index = 0; arg_index < instruction->as.call.arg_count; ++arg_index) {
                        VALUE_SSA_REQUIRE_AVAILABLE_IN_BLOCK(
                            block->id, instruction_index, instruction->as.call.args[arg_index], "call arg");
                    }
                }
                break;
            case VALUE_SSA_INSTR_ADDR_LOCAL:
            case VALUE_SSA_INSTR_ADDR_GLOBAL:
            case VALUE_SSA_INSTR_LOAD_LOCAL:
            case VALUE_SSA_INSTR_LOAD_GLOBAL:
                break;
            case VALUE_SSA_INSTR_LOAD_INDIRECT:
                VALUE_SSA_REQUIRE_AVAILABLE_IN_BLOCK(
                    block->id, instruction_index, instruction->as.load_indirect_addr, "load_indirect addr");
                break;
            case VALUE_SSA_INSTR_STORE_LOCAL:
            case VALUE_SSA_INSTR_STORE_GLOBAL:
                VALUE_SSA_REQUIRE_AVAILABLE_IN_BLOCK(block->id, instruction_index, instruction->as.store.value, "store value");
                break;
            case VALUE_SSA_INSTR_STORE_INDIRECT:
                VALUE_SSA_REQUIRE_AVAILABLE_IN_BLOCK(
                    block->id, instruction_index, instruction->as.store_indirect.addr, "store_indirect addr");
                VALUE_SSA_REQUIRE_AVAILABLE_IN_BLOCK(
                    block->id, instruction_index, instruction->as.store_indirect.value, "store_indirect value");
                break;
            default:
                free(dominates);
                free(scratch);
                free(def_block_ids);
                free(def_instruction_indices);
                free(def_is_phi);
                value_ssa_set_error(error, 0, 0, "VALUE-SSA-073: unknown instruction kind in availability verifier");
                return 0;
            }
        }

        switch (block->terminator.kind) {
        case VALUE_SSA_TERM_RETURN:
            if (block->terminator.has_return_value) {
                VALUE_SSA_REQUIRE_AVAILABLE_IN_BLOCK(
                    block->id, block->instruction_count, block->terminator.as.return_value, "return value");
            }
            break;
        case VALUE_SSA_TERM_BRANCH:
            VALUE_SSA_REQUIRE_AVAILABLE_IN_BLOCK(
                block->id, block->instruction_count, block->terminator.as.branch.condition, "branch condition");
            break;
        case VALUE_SSA_TERM_JUMP:
            break;
        default:
            free(dominates);
            free(scratch);
            free(def_block_ids);
            free(def_instruction_indices);
            free(def_is_phi);
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-074: unknown terminator kind in availability verifier");
            return 0;
        }
    }

    free(dominates);
    free(scratch);
    free(def_block_ids);
    free(def_instruction_indices);
    free(def_is_phi);
    return 1;
}

int value_ssa_verify_program(const ValueSsaProgram *program, ValueSsaError *error) {
    size_t i;

    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-073: program is NULL");
        return 0;
    }
    if (program->global_count > program->global_capacity) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-074: global_count exceeds global_capacity");
        return 0;
    }
    if (program->function_count > program->function_capacity) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-075: function_count exceeds function_capacity");
        return 0;
    }
    if (program->global_count > 0 && !program->globals) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-076: globals table is NULL while global_count=%zu", program->global_count);
        return 0;
    }
    if (program->function_count > 0 && !program->functions) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-077: functions table is NULL while function_count=%zu", program->function_count);
        return 0;
    }

    for (i = 0; i < program->global_count; ++i) {
        const ValueSsaGlobal *global = &program->globals[i];
        size_t j;

        if (global->id != i) {
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-078: global table is not dense at index %zu", i);
            return 0;
        }
        if (!global->name || global->name[0] == '\0') {
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-079: global.%zu has empty name", global->id);
            return 0;
        }
        for (j = i + 1; j < program->global_count; ++j) {
            if (strcmp(global->name, program->globals[j].name) == 0) {
                value_ssa_set_error(error,
                    0,
                    0,
                    "VALUE-SSA-080: duplicate global name '%s'",
                    global->name);
                return 0;
            }
        }
    }

    for (i = 0; i < program->function_count; ++i) {
        const ValueSsaFunction *function = &program->functions[i];
        size_t j;
        unsigned char *predecessor_matrix = NULL;
        size_t *predecessor_counts = NULL;
        unsigned char *reachable = NULL;

        if (!function->name || function->name[0] == '\0') {
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-081: function %zu has empty name", i);
            return 0;
        }
        for (j = i + 1; j < program->function_count; ++j) {
            if (strcmp(function->name, program->functions[j].name) == 0) {
                value_ssa_set_error(error,
                    0,
                    0,
                    "VALUE-SSA-082: duplicate function name '%s'",
                    function->name);
                return 0;
            }
        }
        for (j = 0; j < program->global_count; ++j) {
            if (strcmp(function->name, program->globals[j].name) == 0) {
                value_ssa_set_error(error,
                    0,
                    0,
                    "VALUE-SSA-083: function '%s' collides with global name",
                    function->name);
                return 0;
            }
        }
        if (function->parameter_count > function->local_count) {
            value_ssa_set_error(error,
                0,
                0,
                "VALUE-SSA-084: function '%s' has parameter_count=%zu beyond local_count=%zu",
                function->name,
                function->parameter_count,
                function->local_count);
            return 0;
        }
        if (function->local_count > function->local_capacity) {
            value_ssa_set_error(error,
                0,
                0,
                "VALUE-SSA-085: function '%s' local_count exceeds local_capacity",
                function->name);
            return 0;
        }
        if (function->block_count > function->block_capacity) {
            value_ssa_set_error(error,
                0,
                0,
                "VALUE-SSA-086: function '%s' block_count exceeds block_capacity",
                function->name);
            return 0;
        }
        if (function->local_count > 0 && !function->locals) {
            value_ssa_set_error(error,
                0,
                0,
                "VALUE-SSA-087: function '%s' locals table is NULL while local_count=%zu",
                function->name,
                function->local_count);
            return 0;
        }
        if (function->block_count > 0 && !function->blocks) {
            value_ssa_set_error(error,
                0,
                0,
                "VALUE-SSA-088: function '%s' blocks table is NULL while block_count=%zu",
                function->name,
                function->block_count);
            return 0;
        }

        for (j = 0; j < function->local_count; ++j) {
            const ValueSsaLocal *local = &function->locals[j];

            if (local->id != j) {
                value_ssa_set_error(error,
                    0,
                    0,
                    "VALUE-SSA-089: function '%s' local table is not dense at index %zu",
                    function->name,
                    j);
                return 0;
            }
            if (!local->source_name || local->source_name[0] == '\0') {
                value_ssa_set_error(error,
                    0,
                    0,
                    "VALUE-SSA-090: function '%s' local.%zu has empty source name",
                    function->name,
                    local->id);
                return 0;
            }
            if ((j < function->parameter_count) != (local->is_parameter != 0)) {
                value_ssa_set_error(error,
                    0,
                    0,
                    "VALUE-SSA-091: function '%s' local parameter prefix is malformed at local.%zu",
                    function->name,
                    local->id);
                return 0;
            }
        }

        if (!function->has_body) {
            if (function->block_count != 0 || function->next_value_id != 0) {
                value_ssa_set_error(error,
                    0,
                    0,
                    "VALUE-SSA-092: declaration '%s' must not carry body blocks or SSA values",
                    function->name);
                return 0;
            }
            continue;
        }

        if (function->block_count == 0) {
            value_ssa_set_error(error,
                0,
                0,
                "VALUE-SSA-093: function '%s' has a body but no basic blocks",
                function->name);
            return 0;
        }

        for (j = 0; j < function->block_count; ++j) {
            const ValueSsaBasicBlock *block = &function->blocks[j];
            size_t k;

            if (block->id != j) {
                value_ssa_set_error(error,
                    0,
                    0,
                    "VALUE-SSA-094: function '%s' block table is not dense at index %zu",
                    function->name,
                    j);
                return 0;
            }
            if (block->phi_count > block->phi_capacity) {
                value_ssa_set_error(error,
                    0,
                    0,
                    "VALUE-SSA-095: bb.%zu phi_count exceeds phi_capacity",
                    block->id);
                return 0;
            }
            if (block->instruction_count > block->instruction_capacity) {
                value_ssa_set_error(error,
                    0,
                    0,
                    "VALUE-SSA-096: bb.%zu instruction_count exceeds instruction_capacity",
                    block->id);
                return 0;
            }
            if (block->phi_count > 0 && !block->phis) {
                value_ssa_set_error(error,
                    0,
                    0,
                    "VALUE-SSA-097: bb.%zu phi table is NULL while phi_count=%zu",
                    block->id,
                    block->phi_count);
                return 0;
            }
            if (block->instruction_count > 0 && !block->instructions) {
                value_ssa_set_error(error,
                    0,
                    0,
                    "VALUE-SSA-098: bb.%zu instruction table is NULL while instruction_count=%zu",
                    block->id,
                    block->instruction_count);
                return 0;
            }
            if (!block->has_terminator) {
                value_ssa_set_error(error,
                    0,
                    0,
                    "VALUE-SSA-099: bb.%zu is missing a terminator",
                    block->id);
                return 0;
            }
            for (k = 0; k < block->instruction_count; ++k) {
                if (!value_ssa_verify_instruction(program, function, block, k, &block->instructions[k], error)) {
                    return 0;
                }
            }
            if (!value_ssa_verify_terminator(function, block, &block->terminator, error)) {
                return 0;
            }
        }

        predecessor_matrix = (unsigned char *)calloc(function->block_count * function->block_count,
            sizeof(unsigned char));
        predecessor_counts = (size_t *)calloc(function->block_count, sizeof(size_t));
        reachable = (unsigned char *)calloc(function->block_count, sizeof(unsigned char));
        if (!predecessor_matrix || !predecessor_counts || !reachable) {
            free(predecessor_matrix);
            free(predecessor_counts);
            free(reachable);
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-100: out of memory while verifying function '%s'", function->name);
            return 0;
        }

        if (!value_ssa_build_predecessor_matrix(function, predecessor_matrix, predecessor_counts, error) ||
            !value_ssa_mark_reachable_blocks(function, reachable, error) ||
            !value_ssa_verify_reachability(function, reachable, error) ||
            !value_ssa_verify_value_definitions(function, error) ||
            !value_ssa_verify_phi_structure(function, reachable, predecessor_matrix, predecessor_counts, error) ||
            !value_ssa_verify_value_availability(function, reachable, predecessor_matrix, error)) {
            free(predecessor_matrix);
            free(predecessor_counts);
            free(reachable);
            return 0;
        }

        free(predecessor_matrix);
        free(predecessor_counts);
        free(reachable);
    }

    return 1;
}

#endif
