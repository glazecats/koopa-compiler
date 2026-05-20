#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_SPLIT_AGGREGATOR)
#define VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa_pass.c"
#else

typedef enum {
    VALUE_SSA_SCCP_VALUE_UNKNOWN = 0,
    VALUE_SSA_SCCP_VALUE_CONSTANT,
    VALUE_SSA_SCCP_VALUE_ADDRESS_SYMBOL,
    VALUE_SSA_SCCP_VALUE_OVERDEFINED,
} ValueSsaSccpValueKind;

typedef enum {
    VALUE_SSA_SCCP_ADDRESS_LOCAL_SLOT = 0,
    VALUE_SSA_SCCP_ADDRESS_GLOBAL_SLOT,
    VALUE_SSA_SCCP_ADDRESS_PARAMETER_POINTER,
} ValueSsaSccpAddressSymbolKind;

typedef struct {
    ValueSsaSccpAddressSymbolKind kind;
    size_t id;
    long long offset;
} ValueSsaSccpAddressSymbol;

typedef struct {
    ValueSsaSccpValueKind kind;
    long long constant;
    ValueSsaSccpAddressSymbol address_symbol;
} ValueSsaSccpValue;

static ValueSsaSccpValue value_ssa_sccp_make_unknown(void) {
    ValueSsaSccpValue value;
    value.kind = VALUE_SSA_SCCP_VALUE_UNKNOWN;
    value.constant = 0;
    value.address_symbol.kind = VALUE_SSA_SCCP_ADDRESS_LOCAL_SLOT;
    value.address_symbol.id = 0u;
    value.address_symbol.offset = 0;
    return value;
}

static ValueSsaSccpValue value_ssa_sccp_make_constant(long long constant) {
    ValueSsaSccpValue value;
    value.kind = VALUE_SSA_SCCP_VALUE_CONSTANT;
    value.constant = value_ssa_normalize_sysy_int_value(constant);
    value.address_symbol.kind = VALUE_SSA_SCCP_ADDRESS_LOCAL_SLOT;
    value.address_symbol.id = 0u;
    value.address_symbol.offset = 0;
    return value;
}

static ValueSsaSccpValue value_ssa_sccp_make_address_symbol(
    ValueSsaSccpAddressSymbolKind kind,
    size_t id,
    long long offset) {
    ValueSsaSccpValue value;
    value.kind = VALUE_SSA_SCCP_VALUE_ADDRESS_SYMBOL;
    value.constant = 0;
    value.address_symbol.kind = kind;
    value.address_symbol.id = id;
    value.address_symbol.offset = value_ssa_normalize_sysy_int_value(offset);
    return value;
}

static ValueSsaSccpValue value_ssa_sccp_make_overdefined(void) {
    ValueSsaSccpValue value;
    value.kind = VALUE_SSA_SCCP_VALUE_OVERDEFINED;
    value.constant = 0;
    value.address_symbol.kind = VALUE_SSA_SCCP_ADDRESS_LOCAL_SLOT;
    value.address_symbol.id = 0u;
    value.address_symbol.offset = 0;
    return value;
}

static int value_ssa_sccp_value_equals(ValueSsaSccpValue lhs, ValueSsaSccpValue rhs) {
    return lhs.kind == rhs.kind &&
        (lhs.kind != VALUE_SSA_SCCP_VALUE_CONSTANT || lhs.constant == rhs.constant) &&
        (lhs.kind != VALUE_SSA_SCCP_VALUE_ADDRESS_SYMBOL ||
            (lhs.address_symbol.kind == rhs.address_symbol.kind &&
                lhs.address_symbol.id == rhs.address_symbol.id &&
                lhs.address_symbol.offset == rhs.address_symbol.offset));
}

static ValueSsaSccpValue value_ssa_sccp_meet(ValueSsaSccpValue lhs, ValueSsaSccpValue rhs) {
    if (lhs.kind == VALUE_SSA_SCCP_VALUE_UNKNOWN) {
        return rhs;
    }
    if (rhs.kind == VALUE_SSA_SCCP_VALUE_UNKNOWN) {
        return lhs;
    }
    if (lhs.kind == VALUE_SSA_SCCP_VALUE_OVERDEFINED || rhs.kind == VALUE_SSA_SCCP_VALUE_OVERDEFINED) {
        return value_ssa_sccp_make_overdefined();
    }
    if (lhs.kind == VALUE_SSA_SCCP_VALUE_ADDRESS_SYMBOL || rhs.kind == VALUE_SSA_SCCP_VALUE_ADDRESS_SYMBOL) {
        if (lhs.kind == VALUE_SSA_SCCP_VALUE_ADDRESS_SYMBOL &&
            rhs.kind == VALUE_SSA_SCCP_VALUE_ADDRESS_SYMBOL &&
            lhs.address_symbol.kind == rhs.address_symbol.kind &&
            lhs.address_symbol.id == rhs.address_symbol.id &&
            lhs.address_symbol.offset == rhs.address_symbol.offset) {
            return lhs;
        }
        return value_ssa_sccp_make_overdefined();
    }
    if (lhs.constant == rhs.constant) {
        return lhs;
    }
    return value_ssa_sccp_make_overdefined();
}

static ValueSsaSccpValue value_ssa_sccp_value_for_ref(ValueSsaValueRef ref,
    const ValueSsaSccpValue *values,
    size_t value_count) {
    if (ref.kind == VALUE_SSA_VALUE_IMMEDIATE) {
        return value_ssa_sccp_make_constant(ref.immediate);
    }
    if (ref.kind != VALUE_SSA_VALUE_ID || !values || ref.value_id >= value_count) {
        return value_ssa_sccp_make_overdefined();
    }
    return values[ref.value_id];
}

static int value_ssa_sccp_collect_immutable_globals(const ValueSsaProgram *program,
    unsigned char *immutable_globals,
    ValueSsaError *error) {
    size_t function_index;
    size_t global_index;

    if (!program || (program->global_count != 0u && !immutable_globals)) {
        return 0;
    }

    for (global_index = 0; global_index < program->global_count; ++global_index) {
        const ValueSsaGlobal *global = &program->globals[global_index];

        immutable_globals[global_index] =
            (unsigned char)(global->byte_size == 4u &&
                global->has_initializer &&
                !global->has_runtime_initializer);
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const ValueSsaFunction *function = &program->functions[function_index];
        ValueSsaDefUseAnalysis def_use;
        size_t block_index;

        value_ssa_def_use_analysis_init(&def_use);
        if (function->has_body && !value_ssa_compute_def_use_analysis(function, &def_use, error)) {
            value_ssa_def_use_analysis_free(&def_use);
            return 0;
        }

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            const ValueSsaBasicBlock *block = &function->blocks[block_index];
            size_t instruction_index;

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                const ValueSsaInstruction *instruction = &block->instructions[instruction_index];

                if (instruction->kind == VALUE_SSA_INSTR_STORE_GLOBAL &&
                    instruction->as.store.slot.id < program->global_count) {
                    immutable_globals[instruction->as.store.slot.id] = 0u;
                } else if (instruction->kind == VALUE_SSA_INSTR_STORE_INDIRECT) {
                    for (global_index = 0; global_index < program->global_count; ++global_index) {
                        if (!immutable_globals[global_index]) {
                            continue;
                        }
                        if (value_ssa_pass_store_indirect_may_alias_global(
                                function, &def_use, instruction->as.store_indirect.addr, global_index)) {
                            immutable_globals[global_index] = 0u;
                        }
                    }
                }
            }
        }

        value_ssa_def_use_analysis_free(&def_use);
    }

    return 1;
}

static int value_ssa_sccp_address_symbols_proven_distinct(ValueSsaSccpAddressSymbol lhs,
    ValueSsaSccpAddressSymbol rhs) {
    if (lhs.kind == VALUE_SSA_SCCP_ADDRESS_LOCAL_SLOT &&
        rhs.kind == VALUE_SSA_SCCP_ADDRESS_LOCAL_SLOT) {
        return lhs.id != rhs.id;
    }
    if (lhs.kind == VALUE_SSA_SCCP_ADDRESS_GLOBAL_SLOT &&
        rhs.kind == VALUE_SSA_SCCP_ADDRESS_GLOBAL_SLOT) {
        return lhs.id != rhs.id;
    }
    if ((lhs.kind == VALUE_SSA_SCCP_ADDRESS_LOCAL_SLOT &&
            rhs.kind == VALUE_SSA_SCCP_ADDRESS_GLOBAL_SLOT) ||
        (lhs.kind == VALUE_SSA_SCCP_ADDRESS_GLOBAL_SLOT &&
            rhs.kind == VALUE_SSA_SCCP_ADDRESS_LOCAL_SLOT)) {
        return 1;
    }
    if ((lhs.kind == VALUE_SSA_SCCP_ADDRESS_LOCAL_SLOT &&
            rhs.kind == VALUE_SSA_SCCP_ADDRESS_PARAMETER_POINTER) ||
        (lhs.kind == VALUE_SSA_SCCP_ADDRESS_PARAMETER_POINTER &&
            rhs.kind == VALUE_SSA_SCCP_ADDRESS_LOCAL_SLOT)) {
        return 1;
    }

    return 0;
}

static int value_ssa_sccp_try_evaluate_binary(ValueSsaBinaryOp op,
    ValueSsaSccpValue lhs,
    ValueSsaSccpValue rhs,
    ValueSsaSccpValue *out_value) {
    long long folded = 0;

    if (!out_value) {
        return 0;
    }
    if (lhs.kind == VALUE_SSA_SCCP_VALUE_OVERDEFINED || rhs.kind == VALUE_SSA_SCCP_VALUE_OVERDEFINED) {
        *out_value = value_ssa_sccp_make_overdefined();
        return 1;
    }
    if (lhs.kind == VALUE_SSA_SCCP_VALUE_ADDRESS_SYMBOL &&
        rhs.kind == VALUE_SSA_SCCP_VALUE_ADDRESS_SYMBOL &&
        lhs.address_symbol.kind == rhs.address_symbol.kind &&
        lhs.address_symbol.id == rhs.address_symbol.id) {
        switch (op) {
        case VALUE_SSA_BINARY_SUB:
            *out_value = value_ssa_sccp_make_constant(
                lhs.address_symbol.offset - rhs.address_symbol.offset);
            return 1;
        case VALUE_SSA_BINARY_EQ:
            *out_value = value_ssa_sccp_make_constant(
                lhs.address_symbol.offset == rhs.address_symbol.offset ? 1 : 0);
            return 1;
        case VALUE_SSA_BINARY_NE:
            *out_value = value_ssa_sccp_make_constant(
                lhs.address_symbol.offset != rhs.address_symbol.offset ? 1 : 0);
            return 1;
        default:
            break;
        }
    }
    if (lhs.kind == VALUE_SSA_SCCP_VALUE_ADDRESS_SYMBOL &&
        rhs.kind == VALUE_SSA_SCCP_VALUE_ADDRESS_SYMBOL &&
        value_ssa_sccp_address_symbols_proven_distinct(lhs.address_symbol, rhs.address_symbol)) {
        switch (op) {
        case VALUE_SSA_BINARY_EQ:
            *out_value = value_ssa_sccp_make_constant(0);
            return 1;
        case VALUE_SSA_BINARY_NE:
            *out_value = value_ssa_sccp_make_constant(1);
            return 1;
        default:
            break;
        }
    }
    if (lhs.kind == VALUE_SSA_SCCP_VALUE_ADDRESS_SYMBOL &&
        rhs.kind == VALUE_SSA_SCCP_VALUE_CONSTANT &&
        (op == VALUE_SSA_BINARY_ADD || op == VALUE_SSA_BINARY_SUB || op == VALUE_SSA_BINARY_EQ || op == VALUE_SSA_BINARY_NE)) {
        if (op == VALUE_SSA_BINARY_ADD || op == VALUE_SSA_BINARY_SUB) {
            *out_value = value_ssa_sccp_make_address_symbol(
                lhs.address_symbol.kind,
                lhs.address_symbol.id,
                op == VALUE_SSA_BINARY_ADD
                    ? lhs.address_symbol.offset + rhs.constant
                    : lhs.address_symbol.offset - rhs.constant);
            return 1;
        }
        if (op == VALUE_SSA_BINARY_EQ) {
            *out_value = value_ssa_sccp_make_constant(0);
            return 1;
        }
        if (op == VALUE_SSA_BINARY_NE) {
            *out_value = value_ssa_sccp_make_constant(1);
            return 1;
        }
    }
    if (lhs.kind == VALUE_SSA_SCCP_VALUE_CONSTANT &&
        rhs.kind == VALUE_SSA_SCCP_VALUE_ADDRESS_SYMBOL &&
        (op == VALUE_SSA_BINARY_ADD || op == VALUE_SSA_BINARY_EQ || op == VALUE_SSA_BINARY_NE)) {
        if (op == VALUE_SSA_BINARY_ADD) {
            *out_value = value_ssa_sccp_make_address_symbol(
                rhs.address_symbol.kind,
                rhs.address_symbol.id,
                rhs.address_symbol.offset + lhs.constant);
            return 1;
        }
        if (op == VALUE_SSA_BINARY_EQ) {
            *out_value = value_ssa_sccp_make_constant(0);
            return 1;
        }
        if (op == VALUE_SSA_BINARY_NE) {
            *out_value = value_ssa_sccp_make_constant(1);
            return 1;
        }
    }
    if (lhs.kind != VALUE_SSA_SCCP_VALUE_CONSTANT || rhs.kind != VALUE_SSA_SCCP_VALUE_CONSTANT) {
        *out_value = value_ssa_sccp_make_unknown();
        return 1;
    }
    if (!value_ssa_try_fold_binary_immediates(op, lhs.constant, rhs.constant, &folded)) {
        *out_value = value_ssa_sccp_make_overdefined();
        return 1;
    }
    *out_value = value_ssa_sccp_make_constant(folded);
    return 1;
}

static int value_ssa_sccp_rewrite_use_site(ValueSsaFunction *function,
    const ValueSsaUseSite *site,
    ValueSsaValueRef replacement,
    ValueSsaError *error) {
    ValueSsaBasicBlock *block;

    if (!function || !site || site->block_id >= function->block_count) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-340: invalid SCCP use-site contract");
        return 0;
    }

    block = &function->blocks[site->block_id];
    switch (site->kind) {
    case VALUE_SSA_USE_PHI:
        if (site->phi_index >= block->phi_count ||
            site->operand_index >= block->phis[site->phi_index].input_count) {
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-341: invalid SCCP phi use-site");
            return 0;
        }
        block->phis[site->phi_index].inputs[site->operand_index].value = replacement;
        return 1;
    case VALUE_SSA_USE_INSTR:
        if (site->instruction_index >= block->instruction_count) {
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-342: invalid SCCP instruction use-site");
            return 0;
        }
        switch (site->role) {
        case VALUE_SSA_USE_ROLE_MOV_VALUE:
            block->instructions[site->instruction_index].as.mov_value = replacement;
            return 1;
        case VALUE_SSA_USE_ROLE_BINARY_LHS:
            block->instructions[site->instruction_index].as.binary.lhs = replacement;
            return 1;
        case VALUE_SSA_USE_ROLE_BINARY_RHS:
            block->instructions[site->instruction_index].as.binary.rhs = replacement;
            return 1;
        case VALUE_SSA_USE_ROLE_CALL_ARG:
            if (site->operand_index >= block->instructions[site->instruction_index].as.call.arg_count) {
                value_ssa_set_error(error, 0, 0, "VALUE-SSA-343: invalid SCCP call arg use-site");
                return 0;
            }
            block->instructions[site->instruction_index].as.call.args[site->operand_index] = replacement;
            return 1;
        case VALUE_SSA_USE_ROLE_STORE_VALUE:
            block->instructions[site->instruction_index].as.store.value = replacement;
            return 1;
        case VALUE_SSA_USE_ROLE_LOAD_INDIRECT_ADDR:
            block->instructions[site->instruction_index].as.load_indirect_addr = replacement;
            return 1;
        case VALUE_SSA_USE_ROLE_STORE_INDIRECT_ADDR:
            block->instructions[site->instruction_index].as.store_indirect.addr = replacement;
            return 1;
        case VALUE_SSA_USE_ROLE_STORE_INDIRECT_VALUE:
            block->instructions[site->instruction_index].as.store_indirect.value = replacement;
            return 1;
        default:
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-344: invalid SCCP instruction use-role");
            return 0;
        }
    case VALUE_SSA_USE_TERM:
        switch (site->role) {
        case VALUE_SSA_USE_ROLE_RETURN_VALUE:
            block->terminator.as.return_value = replacement;
            return 1;
        case VALUE_SSA_USE_ROLE_BRANCH_CONDITION:
            block->terminator.as.branch.condition = replacement;
            return 1;
        default:
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-345: invalid SCCP terminator use-role");
            return 0;
        }
    default:
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-346: invalid SCCP use kind");
        return 0;
    }
}

int value_ssa_sparse_conditional_constant_propagation(ValueSsaProgram *program, ValueSsaError *error) {
    size_t function_index;
    unsigned char *immutable_globals = NULL;

    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-347: invalid SCCP contract");
        return 0;
    }

    if (program->global_count != 0u) {
        immutable_globals = (unsigned char *)calloc(program->global_count, sizeof(unsigned char));
        if (!immutable_globals) {
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-349: out of memory preparing SCCP globals");
            return 0;
        }
        if (!value_ssa_sccp_collect_immutable_globals(program, immutable_globals, error)) {
            free(immutable_globals);
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-350: failed collecting SCCP globals");
            return 0;
        }
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunction *function = &program->functions[function_index];
        ValueSsaCfgAnalysis cfg;
        ValueSsaDefUseAnalysis def_use;
        ValueSsaSccpValue *values = NULL;
        unsigned char *executable_blocks = NULL;
        unsigned char *executable_edges = NULL;
        int progress = 1;
        int changed = 0;

        if (!function->has_body || function->block_count == 0u || function->next_value_id == 0u) {
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

        values = (ValueSsaSccpValue *)calloc(function->next_value_id, sizeof(ValueSsaSccpValue));
        executable_blocks = (unsigned char *)calloc(function->block_count, sizeof(unsigned char));
        executable_edges =
            (unsigned char *)calloc(function->block_count * function->block_count, sizeof(unsigned char));
        if (!values || !executable_blocks || !executable_edges) {
            free(values);
            free(executable_blocks);
            free(executable_edges);
            value_ssa_cfg_analysis_free(&cfg);
            value_ssa_def_use_analysis_free(&def_use);
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-348: out of memory preparing SCCP");
            return 0;
        }

        {
            size_t value_id;
            for (value_id = 0; value_id < function->next_value_id; ++value_id) {
                values[value_id] = value_ssa_sccp_make_unknown();
            }
        }

        executable_blocks[0] = 1u;
        while (progress) {
            size_t block_index;

            progress = 0;
            for (block_index = 0; block_index < function->block_count; ++block_index) {
                ValueSsaBasicBlock *block = &function->blocks[block_index];
                size_t phi_index;
                size_t instruction_index;

                if (!executable_blocks[block_index]) {
                    continue;
                }

                for (phi_index = 0; phi_index < block->phi_count; ++phi_index) {
                    ValueSsaPhi *phi = &block->phis[phi_index];
                    ValueSsaSccpValue merged = value_ssa_sccp_make_unknown();
                    int saw_executable_input = 0;
                    size_t input_index;

                    for (input_index = 0; input_index < phi->input_count; ++input_index) {
                        const ValueSsaPhiInput *input = &phi->inputs[input_index];
                        ValueSsaSccpValue input_value;

                        if (input->predecessor_block_id >= function->block_count ||
                            !executable_edges[input->predecessor_block_id * function->block_count + block_index]) {
                            continue;
                        }

                        input_value =
                            value_ssa_sccp_value_for_ref(input->value, values, function->next_value_id);
                        if (!saw_executable_input) {
                            merged = input_value;
                            saw_executable_input = 1;
                        } else {
                            merged = value_ssa_sccp_meet(merged, input_value);
                        }
                    }

                    if (saw_executable_input) {
                        ValueSsaSccpValue next_value =
                            value_ssa_sccp_meet(values[phi->result_id], merged);

                        if (!value_ssa_sccp_value_equals(values[phi->result_id], next_value)) {
                            values[phi->result_id] = next_value;
                            progress = 1;
                        }
                    }
                }

                for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                    ValueSsaInstruction *instruction = &block->instructions[instruction_index];
                    ValueSsaSccpValue result = value_ssa_sccp_make_overdefined();
                    ValueSsaSccpValue next_value;

                    if (!instruction->has_result || instruction->result.kind != VALUE_SSA_VALUE_ID) {
                        continue;
                    }

                    switch (instruction->kind) {
                    case VALUE_SSA_INSTR_MOV:
                        result = value_ssa_sccp_value_for_ref(
                            instruction->as.mov_value, values, function->next_value_id);
                        break;
                    case VALUE_SSA_INSTR_ADDR_GLOBAL:
                    case VALUE_SSA_INSTR_ADDR_LOCAL:
                        result = value_ssa_sccp_make_address_symbol(
                            instruction->as.addr_slot.kind == VALUE_SSA_SLOT_GLOBAL
                                ? VALUE_SSA_SCCP_ADDRESS_GLOBAL_SLOT
                                : VALUE_SSA_SCCP_ADDRESS_LOCAL_SLOT,
                            instruction->as.addr_slot.id,
                            0);
                        break;
                    case VALUE_SSA_INSTR_LOAD_LOCAL:
                        if (instruction->as.load_slot.kind == VALUE_SSA_SLOT_LOCAL &&
                            instruction->as.load_slot.id < function->local_count &&
                            function->locals[instruction->as.load_slot.id].is_parameter &&
                            function->locals[instruction->as.load_slot.id].array_rank > 0u) {
                            result = value_ssa_sccp_make_address_symbol(
                                VALUE_SSA_SCCP_ADDRESS_PARAMETER_POINTER,
                                instruction->as.load_slot.id,
                                0);
                        } else {
                            result = value_ssa_sccp_make_overdefined();
                        }
                        break;
                    case VALUE_SSA_INSTR_LOAD_GLOBAL:
                        if (instruction->as.load_slot.kind == VALUE_SSA_SLOT_GLOBAL &&
                            instruction->as.load_slot.id < program->global_count &&
                            immutable_globals &&
                            immutable_globals[instruction->as.load_slot.id]) {
                            result = value_ssa_sccp_make_constant(
                                program->globals[instruction->as.load_slot.id].initializer_value);
                        } else {
                            result = value_ssa_sccp_make_overdefined();
                        }
                        break;
                    case VALUE_SSA_INSTR_LOAD_INDIRECT:
                        {
                            ValueSsaSccpValue address_value = value_ssa_sccp_value_for_ref(
                                instruction->as.load_indirect_addr, values, function->next_value_id);

                            if (address_value.kind == VALUE_SSA_SCCP_VALUE_ADDRESS_SYMBOL &&
                                address_value.address_symbol.kind == VALUE_SSA_SCCP_ADDRESS_GLOBAL_SLOT &&
                                address_value.address_symbol.id < program->global_count &&
                                address_value.address_symbol.offset == 0 &&
                                immutable_globals &&
                                immutable_globals[address_value.address_symbol.id]) {
                                result = value_ssa_sccp_make_constant(
                                    program->globals[address_value.address_symbol.id].initializer_value);
                            } else if (address_value.kind == VALUE_SSA_SCCP_VALUE_UNKNOWN) {
                                result = value_ssa_sccp_make_unknown();
                            } else {
                                result = value_ssa_sccp_make_overdefined();
                            }
                        }
                        break;
                    case VALUE_SSA_INSTR_BINARY:
                        if (!value_ssa_sccp_try_evaluate_binary(
                                instruction->as.binary.op,
                                value_ssa_sccp_value_for_ref(
                                    instruction->as.binary.lhs, values, function->next_value_id),
                                value_ssa_sccp_value_for_ref(
                                    instruction->as.binary.rhs, values, function->next_value_id),
                                &result)) {
                            result = value_ssa_sccp_make_overdefined();
                        }
                        break;
                    default:
                        result = value_ssa_sccp_make_overdefined();
                        break;
                    }

                    next_value = value_ssa_sccp_meet(values[instruction->result.value_id], result);
                    if (!value_ssa_sccp_value_equals(values[instruction->result.value_id], next_value)) {
                        values[instruction->result.value_id] = next_value;
                        progress = 1;
                    }
                }

                switch (block->terminator.kind) {
                case VALUE_SSA_TERM_JUMP:
                    if (block->terminator.as.jump_target < function->block_count &&
                        !executable_edges[block_index * function->block_count + block->terminator.as.jump_target]) {
                        executable_edges[block_index * function->block_count + block->terminator.as.jump_target] = 1u;
                        executable_blocks[block->terminator.as.jump_target] = 1u;
                        progress = 1;
                    }
                    break;
                case VALUE_SSA_TERM_BRANCH:
                    {
                        ValueSsaSccpValue condition = value_ssa_sccp_value_for_ref(
                            block->terminator.as.branch.condition,
                            values,
                            function->next_value_id);
                        size_t then_target = block->terminator.as.branch.then_target;
                        size_t else_target = block->terminator.as.branch.else_target;

                        if (condition.kind == VALUE_SSA_SCCP_VALUE_CONSTANT ||
                            condition.kind == VALUE_SSA_SCCP_VALUE_ADDRESS_SYMBOL) {
                            size_t taken = condition.constant != 0 ? then_target : else_target;

                            if (condition.kind == VALUE_SSA_SCCP_VALUE_ADDRESS_SYMBOL) {
                                taken = then_target;
                            }

                            if (taken < function->block_count &&
                                !executable_edges[block_index * function->block_count + taken]) {
                                executable_edges[block_index * function->block_count + taken] = 1u;
                                executable_blocks[taken] = 1u;
                                progress = 1;
                            }
                        } else {
                            if (then_target < function->block_count &&
                                !executable_edges[block_index * function->block_count + then_target]) {
                                executable_edges[block_index * function->block_count + then_target] = 1u;
                                executable_blocks[then_target] = 1u;
                                progress = 1;
                            }
                            if (else_target < function->block_count &&
                                !executable_edges[block_index * function->block_count + else_target]) {
                                executable_edges[block_index * function->block_count + else_target] = 1u;
                                executable_blocks[else_target] = 1u;
                                progress = 1;
                            }
                        }
                    }
                    break;
                case VALUE_SSA_TERM_RETURN:
                default:
                    break;
                }
            }
        }

        {
            size_t value_id;
            for (value_id = 0; value_id < function->next_value_id; ++value_id) {
                size_t use_index;

                if (value_id >= def_use.value_count || !def_use.has_def[value_id] ||
                    (values[value_id].kind != VALUE_SSA_SCCP_VALUE_CONSTANT &&
                        values[value_id].kind != VALUE_SSA_SCCP_VALUE_ADDRESS_SYMBOL)) {
                    continue;
                }

                for (use_index = 0; use_index < def_use.use_counts[value_id]; ++use_index) {
                    const ValueSsaUseSite *site = &def_use.use_sites[def_use.use_offsets[value_id] + use_index];
                    ValueSsaValueRef replacement;

                    if (site->role == VALUE_SSA_USE_ROLE_BRANCH_CONDITION) {
                        continue;
                    }
                    if (values[value_id].kind == VALUE_SSA_SCCP_VALUE_ADDRESS_SYMBOL) {
                        continue;
                    }

                    replacement = value_ssa_value_immediate(values[value_id].constant);

                    if (!value_ssa_sccp_rewrite_use_site(
                            function,
                            site,
                            replacement,
                            error)) {
                        free(values);
                        free(executable_blocks);
                        free(executable_edges);
                        value_ssa_cfg_analysis_free(&cfg);
                        value_ssa_def_use_analysis_free(&def_use);
                        return 0;
                    }
                    changed = 1;
                }
            }
        }

        free(values);
        free(executable_blocks);
        free(executable_edges);
        value_ssa_cfg_analysis_free(&cfg);
        value_ssa_def_use_analysis_free(&def_use);

        if (changed &&
            (!value_ssa_simplify_trivial_values(program, error) ||
                !value_ssa_fold_constants(program, error) ||
                !value_ssa_eliminate_redundant_binaries(program, error) ||
                !value_ssa_simplify_cfg(program, error) ||
                !value_ssa_eliminate_dead_value_defs(program, error))) {
            return 0;
        }
    }

    free(immutable_globals);
    return value_ssa_verify_program(program, error);
}

#endif
