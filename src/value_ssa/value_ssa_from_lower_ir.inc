#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_SPLIT_AGGREGATOR)
#define VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa.c"
#else

static ValueSsaBinaryOp value_ssa_binary_op_from_lower_ir(LowerIrBinaryOp op) {
    switch (op) {
    case LOWER_IR_BINARY_ADD:
        return VALUE_SSA_BINARY_ADD;
    case LOWER_IR_BINARY_SUB:
        return VALUE_SSA_BINARY_SUB;
    case LOWER_IR_BINARY_MUL:
        return VALUE_SSA_BINARY_MUL;
    case LOWER_IR_BINARY_DIV:
        return VALUE_SSA_BINARY_DIV;
    case LOWER_IR_BINARY_MOD:
        return VALUE_SSA_BINARY_MOD;
    case LOWER_IR_BINARY_BIT_AND:
        return VALUE_SSA_BINARY_BIT_AND;
    case LOWER_IR_BINARY_BIT_XOR:
        return VALUE_SSA_BINARY_BIT_XOR;
    case LOWER_IR_BINARY_BIT_OR:
        return VALUE_SSA_BINARY_BIT_OR;
    case LOWER_IR_BINARY_SHIFT_LEFT:
        return VALUE_SSA_BINARY_SHIFT_LEFT;
    case LOWER_IR_BINARY_SHIFT_RIGHT:
        return VALUE_SSA_BINARY_SHIFT_RIGHT;
    case LOWER_IR_BINARY_EQ:
        return VALUE_SSA_BINARY_EQ;
    case LOWER_IR_BINARY_NE:
        return VALUE_SSA_BINARY_NE;
    case LOWER_IR_BINARY_LT:
        return VALUE_SSA_BINARY_LT;
    case LOWER_IR_BINARY_LE:
        return VALUE_SSA_BINARY_LE;
    case LOWER_IR_BINARY_GT:
        return VALUE_SSA_BINARY_GT;
    case LOWER_IR_BINARY_GE:
        return VALUE_SSA_BINARY_GE;
    default:
        return VALUE_SSA_BINARY_ADD;
    }
}

typedef struct {
    const LowerIrFunction *input_function;
    const LowerIrCfgAnalysis *input_analysis;
    const size_t *phi_candidate_counts;
    const size_t *phi_candidate_temps;
    const size_t *phi_candidate_offsets;
    ValueSsaFunction *output_function;
    ValueSsaRenameState *rename_state;
    size_t *phi_result_value_ids;
    ValueSsaError conversion_error;
    int conversion_failed;
} ValueSsaLowerIrConversionState;

static ValueSsaPhi *value_ssa_find_phi_by_result_id(ValueSsaBasicBlock *block, size_t result_id);

static ValueSsaPhi *value_ssa_find_phi_by_result_id(ValueSsaBasicBlock *block, size_t result_id) {
    size_t phi_index;

    if (!block) {
        return NULL;
    }

    for (phi_index = 0; phi_index < block->phi_count; ++phi_index) {
        if (block->phis[phi_index].result_id == result_id) {
            return &block->phis[phi_index];
        }
    }

    return NULL;
}

static int value_ssa_phi_is_complete_for_block(const ValueSsaBasicBlock *block,
    size_t phi_value_id,
    size_t predecessor_count) {
    const ValueSsaPhi *phi;

    if (!block) {
        return 0;
    }

    phi = value_ssa_find_phi_by_result_id((ValueSsaBasicBlock *)block, phi_value_id);
    if (!phi) {
        return 0;
    }

    return phi->input_count == predecessor_count;
}

static int value_ssa_set_phi_input_for_predecessor(ValueSsaBasicBlock *block,
    size_t phi_value_id,
    size_t predecessor_block_id,
    ValueSsaValueRef value,
    ValueSsaError *error) {
    ValueSsaPhi *phi;
    size_t input_index;
    ValueSsaPhiInput *new_inputs;
    size_t insert_index;

    if (!block) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-101: invalid lower-ir conversion contract");
        return 0;
    }

    phi = value_ssa_find_phi_by_result_id(block, phi_value_id);
    if (!phi) {
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-120: internal phi bookkeeping lost ssa.%zu for bb.%zu",
            phi_value_id,
            block->id);
        return 0;
    }

    for (input_index = 0; input_index < phi->input_count; ++input_index) {
        if (phi->inputs[input_index].predecessor_block_id == predecessor_block_id) {
            phi->inputs[input_index].value = value;
            return 1;
        }
    }

    new_inputs = (ValueSsaPhiInput *)realloc(phi->inputs, (phi->input_count + 1) * sizeof(ValueSsaPhiInput));
    if (!new_inputs) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-121: out of memory while extending phi inputs");
        return 0;
    }
    phi->inputs = new_inputs;
    insert_index = phi->input_count;
    while (insert_index > 0 && phi->inputs[insert_index - 1].predecessor_block_id > predecessor_block_id) {
        phi->inputs[insert_index] = phi->inputs[insert_index - 1];
        --insert_index;
    }
    phi->inputs[insert_index].predecessor_block_id = predecessor_block_id;
    phi->inputs[insert_index].value = value;
    ++phi->input_count;
    phi->input_capacity = phi->input_count;
    return 1;
}

static int value_ssa_map_value_from_lower_ir_rename_state(LowerIrValueRef input,
    const ValueSsaRenameState *rename_state,
    size_t temp_count,
    ValueSsaValueRef *out_value,
    ValueSsaError *error);
static int value_ssa_bind_block_phi_results_from_lower_ir(const LowerIrFunction *input_function,
    size_t block_index,
    const size_t *phi_candidate_counts,
    const size_t *phi_candidate_temps,
    const size_t *phi_candidate_offsets,
    const size_t *phi_result_value_ids,
    ValueSsaRenameState *rename_state,
    ValueSsaError *error);

static int value_ssa_convert_block_from_lower_ir_state(ValueSsaLowerIrConversionState *state,
    size_t block_index,
    ValueSsaError *error) {
    const LowerIrFunction *input_function = state->input_function;
    const LowerIrBasicBlock *input_block = &input_function->blocks[block_index];
    ValueSsaFunction *output_function = state->output_function;
    ValueSsaBasicBlock *output_block = &output_function->blocks[block_index];
    size_t temp_count = input_function->next_temp_id;
    size_t instruction_index;

    for (instruction_index = 0; instruction_index < input_block->instruction_count; ++instruction_index) {
        const LowerIrInstruction *input_instruction = &input_block->instructions[instruction_index];
        ValueSsaInstruction output_instruction;
        size_t new_value_id = (size_t)-1;

        memset(&output_instruction, 0, sizeof(output_instruction));
        switch (input_instruction->kind) {
        case LOWER_IR_INSTR_MOV:
            output_instruction.kind = VALUE_SSA_INSTR_MOV;
            output_instruction.has_result = 1;
            new_value_id = value_ssa_function_allocate_value(output_function);
            if (new_value_id == (size_t)-1 ||
                !value_ssa_map_value_from_lower_ir_rename_state(input_instruction->as.mov_value,
                    state->rename_state,
                    temp_count,
                    &output_instruction.as.mov_value,
                    error)) {
                if (new_value_id == (size_t)-1) {
                    value_ssa_set_error(error, 0, 0, "VALUE-SSA-111: SSA value allocation overflow while converting mov");
                }
                return 0;
            }
            output_instruction.result = value_ssa_value_id(new_value_id);
            break;
        case LOWER_IR_INSTR_BINARY:
            output_instruction.kind = VALUE_SSA_INSTR_BINARY;
            output_instruction.has_result = 1;
            new_value_id = value_ssa_function_allocate_value(output_function);
            if (new_value_id == (size_t)-1 ||
                !value_ssa_map_value_from_lower_ir_rename_state(input_instruction->as.binary.lhs,
                    state->rename_state,
                    temp_count,
                    &output_instruction.as.binary.lhs,
                    error) ||
                !value_ssa_map_value_from_lower_ir_rename_state(input_instruction->as.binary.rhs,
                    state->rename_state,
                    temp_count,
                    &output_instruction.as.binary.rhs,
                    error)) {
                if (new_value_id == (size_t)-1) {
                    value_ssa_set_error(error, 0, 0, "VALUE-SSA-112: SSA value allocation overflow while converting binary");
                }
                return 0;
            }
            output_instruction.result = value_ssa_value_id(new_value_id);
            output_instruction.as.binary.op = value_ssa_binary_op_from_lower_ir(input_instruction->as.binary.op);
            break;
        case LOWER_IR_INSTR_CALL:
            {
                ValueSsaValueRef *args = NULL;
                size_t arg_index;

                output_instruction.kind = VALUE_SSA_INSTR_CALL;
                output_instruction.has_result = input_instruction->has_result;
                if (input_instruction->has_result) {
                    new_value_id = value_ssa_function_allocate_value(output_function);
                    if (new_value_id == (size_t)-1) {
                        value_ssa_set_error(error, 0, 0, "VALUE-SSA-113: SSA value allocation overflow while converting call");
                        return 0;
                    }
                    output_instruction.result = value_ssa_value_id(new_value_id);
                }
                output_instruction.as.call.callee_name = input_instruction->as.call.callee_name;
                output_instruction.as.call.arg_count = input_instruction->as.call.arg_count;
                if (input_instruction->as.call.arg_count > 0) {
                    args = (ValueSsaValueRef *)malloc(input_instruction->as.call.arg_count * sizeof(ValueSsaValueRef));
                    if (!args) {
                        value_ssa_set_error(error, 0, 0, "VALUE-SSA-114: out of memory while converting call args");
                        return 0;
                    }
                    for (arg_index = 0; arg_index < input_instruction->as.call.arg_count; ++arg_index) {
                        if (!value_ssa_map_value_from_lower_ir_rename_state(input_instruction->as.call.args[arg_index],
                                state->rename_state,
                                temp_count,
                                &args[arg_index],
                                error)) {
                            free(args);
                            return 0;
                        }
                    }
                }
                output_instruction.as.call.args = args;
                if (!value_ssa_block_append_instruction(output_block, &output_instruction, error)) {
                    free(args);
                    return 0;
                }
                free(args);
                goto after_instruction_append;
            }
        case LOWER_IR_INSTR_LOAD_LOCAL:
            output_instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
            output_instruction.has_result = 1;
            new_value_id = value_ssa_function_allocate_value(output_function);
            if (new_value_id == (size_t)-1) {
                value_ssa_set_error(error, 0, 0, "VALUE-SSA-115: SSA value allocation overflow while converting load_local");
                return 0;
            }
            output_instruction.result = value_ssa_value_id(new_value_id);
            output_instruction.as.load_slot = value_ssa_slot_local(input_instruction->as.load_slot.id);
            break;
        case LOWER_IR_INSTR_STORE_LOCAL:
            output_instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
            output_instruction.has_result = 0;
            output_instruction.as.store.slot = value_ssa_slot_local(input_instruction->as.store.slot.id);
            if (!value_ssa_map_value_from_lower_ir_rename_state(input_instruction->as.store.value,
                    state->rename_state,
                    temp_count,
                    &output_instruction.as.store.value,
                    error)) {
                return 0;
            }
            break;
        case LOWER_IR_INSTR_LOAD_GLOBAL:
            output_instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
            output_instruction.has_result = 1;
            new_value_id = value_ssa_function_allocate_value(output_function);
            if (new_value_id == (size_t)-1) {
                value_ssa_set_error(error, 0, 0, "VALUE-SSA-116: SSA value allocation overflow while converting load_global");
                return 0;
            }
            output_instruction.result = value_ssa_value_id(new_value_id);
            output_instruction.as.load_slot = value_ssa_slot_global(input_instruction->as.load_slot.id);
            break;
        case LOWER_IR_INSTR_STORE_GLOBAL:
            output_instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
            output_instruction.has_result = 0;
            output_instruction.as.store.slot = value_ssa_slot_global(input_instruction->as.store.slot.id);
            if (!value_ssa_map_value_from_lower_ir_rename_state(input_instruction->as.store.value,
                    state->rename_state,
                    temp_count,
                    &output_instruction.as.store.value,
                    error)) {
                return 0;
            }
            break;
        default:
            value_ssa_set_error(error,
                0,
                0,
                "VALUE-SSA-117: unsupported lower-ir instruction kind %d during conversion",
                (int)input_instruction->kind);
            return 0;
        }

        if (!value_ssa_block_append_instruction(output_block, &output_instruction, error)) {
            return 0;
        }

after_instruction_append:
        if (input_instruction->has_result) {
            if (!value_ssa_rename_state_bind(state->rename_state,
                    input_instruction->result.temp_id,
                    value_ssa_value_id(new_value_id),
                    error)) {
                return 0;
            }
        }
    }

    switch (input_block->terminator.kind) {
    case LOWER_IR_TERM_RETURN:
        {
            ValueSsaValueRef return_value;

            if (!value_ssa_map_value_from_lower_ir_rename_state(input_block->terminator.as.return_value,
                    state->rename_state,
                    temp_count,
                    &return_value,
                    error) ||
                !value_ssa_block_set_return(output_block, return_value, error)) {
                return 0;
            }
        }
        break;
    case LOWER_IR_TERM_JUMP:
        if (!value_ssa_block_set_jump(output_block, input_block->terminator.as.jump_target, error)) {
            return 0;
        }
        break;
    case LOWER_IR_TERM_BRANCH:
        {
            ValueSsaValueRef condition;

            if (!value_ssa_map_value_from_lower_ir_rename_state(input_block->terminator.as.branch.condition,
                    state->rename_state,
                    temp_count,
                    &condition,
                    error) ||
                !value_ssa_block_set_branch(output_block,
                    condition,
                    input_block->terminator.as.branch.then_target,
                    input_block->terminator.as.branch.else_target,
                    error)) {
                return 0;
            }
        }
        break;
    default:
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-102: lower-ir bb.%zu uses unknown terminator kind %d during conversion",
            input_block->id,
            (int)input_block->terminator.kind);
        return 0;
    }

    (void)temp_count;
    return 1;
}

static int value_ssa_fill_successor_phi_inputs_from_current_state(ValueSsaLowerIrConversionState *state,
    size_t predecessor_block_id,
    ValueSsaError *error) {
    size_t successor_index;
    size_t block_count;
    const size_t *successor_counts;
    const size_t *successors;

    if (!state || !state->input_function || !state->input_analysis || !state->rename_state ||
        !state->output_function || !state->phi_candidate_counts || !state->phi_candidate_temps ||
        !state->phi_candidate_offsets || !state->phi_result_value_ids) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-101: invalid lower-ir conversion contract");
        return 0;
    }

    block_count = state->input_function->block_count;
    successor_counts = state->input_analysis->successor_counts;
    successors = state->input_analysis->successors;
    for (successor_index = 0; successor_index < successor_counts[predecessor_block_id]; ++successor_index) {
        size_t successor_block = successors[predecessor_block_id * block_count + successor_index];
        size_t candidate_index;

        for (candidate_index = 0; candidate_index < state->phi_candidate_counts[successor_block]; ++candidate_index) {
            size_t temp_id = state->phi_candidate_temps[state->phi_candidate_offsets[successor_block] + candidate_index];
            size_t phi_slot = state->phi_candidate_offsets[successor_block] + candidate_index;
            ValueSsaValueRef current_value;
            size_t phi_value_id = state->phi_result_value_ids[phi_slot];

            if (phi_value_id == (size_t)-1 ||
                value_ssa_phi_is_complete_for_block(&state->output_function->blocks[successor_block],
                    phi_value_id,
                    state->input_analysis->predecessor_counts[successor_block])) {
                continue;
            }
            if (!state->rename_state->has_current_value[temp_id] ||
                state->rename_state->current_values[temp_id].kind != VALUE_SSA_VALUE_ID) {
                continue;
            }
            current_value = state->rename_state->current_values[temp_id];
            if (!value_ssa_set_phi_input_for_predecessor(&state->output_function->blocks[successor_block],
                    phi_value_id,
                    predecessor_block_id,
                    current_value,
                    error)) {
                return 0;
            }
        }
    }

    return 1;
}

static int value_ssa_convert_block_from_lower_ir_enter(const LowerIrFunction *function,
    const LowerIrCfgAnalysis *analysis,
    size_t block_id,
    void *user_data,
    LowerIrError *error) {
    ValueSsaLowerIrConversionState *state = (ValueSsaLowerIrConversionState *)user_data;
    ValueSsaError conversion_error;
    (void)function;
    (void)analysis;

    if (!state) {
        if (error) {
            error->line = 0;
            error->column = 0;
            snprintf(error->message,
                sizeof(error->message),
                "LOWER-IR-061: invalid conversion callback state during dominator-tree walk");
        }
        return 0;
    }

    memset(&conversion_error, 0, sizeof(conversion_error));
    if (!state->rename_state ||
        !value_ssa_rename_state_begin_scope(state->rename_state, &conversion_error) ||
        !value_ssa_bind_block_phi_results_from_lower_ir(state->input_function,
            block_id,
            state->phi_candidate_counts,
            state->phi_candidate_temps,
            state->phi_candidate_offsets,
            state->phi_result_value_ids,
            state->rename_state,
            &conversion_error) ||
        !value_ssa_convert_block_from_lower_ir_state(state, block_id, &conversion_error)) {
        state->conversion_error = conversion_error;
        state->conversion_failed = 1;
        if (error) {
            error->line = conversion_error.line;
            error->column = conversion_error.column;
            snprintf(error->message,
                sizeof(error->message),
                "LOWER-IR-062: value-ssa conversion callback failed");
        }
        return 0;
    }

    return 1;
}

static int value_ssa_convert_block_from_lower_ir_leave(const LowerIrFunction *function,
    const LowerIrCfgAnalysis *analysis,
    size_t block_id,
    void *user_data,
    LowerIrError *error) {
    ValueSsaLowerIrConversionState *state = (ValueSsaLowerIrConversionState *)user_data;
    ValueSsaError conversion_error;

    (void)function;
    (void)analysis;
    (void)block_id;

    if (!state || !state->rename_state) {
        if (error) {
            error->line = 0;
            error->column = 0;
            snprintf(error->message,
                sizeof(error->message),
                "LOWER-IR-063: invalid conversion leave-callback state during dominator-tree walk");
        }
        return 0;
    }

    memset(&conversion_error, 0, sizeof(conversion_error));
    if (!value_ssa_fill_successor_phi_inputs_from_current_state(state, block_id, &conversion_error)) {
        state->conversion_error = conversion_error;
        state->conversion_failed = 1;
        if (error) {
            error->line = conversion_error.line;
            error->column = conversion_error.column;
            snprintf(error->message,
                sizeof(error->message),
                "LOWER-IR-064: value-ssa conversion leave callback failed");
        }
        return 0;
    }
    if (!value_ssa_rename_state_end_scope(state->rename_state, &conversion_error)) {
        state->conversion_error = conversion_error;
        state->conversion_failed = 1;
        if (error) {
            error->line = conversion_error.line;
            error->column = conversion_error.column;
            snprintf(error->message,
                sizeof(error->message),
                "LOWER-IR-064: value-ssa conversion leave callback failed");
        }
        return 0;
    }

    return 1;
}

static int value_ssa_map_value_from_lower_ir_rename_state(LowerIrValueRef input,
    const ValueSsaRenameState *rename_state,
    size_t next_temp_id,
    ValueSsaValueRef *out_value,
    ValueSsaError *error) {
    if (!out_value) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-101: invalid lower-ir conversion contract");
        return 0;
    }

    switch (input.kind) {
    case LOWER_IR_VALUE_IMMEDIATE:
        *out_value = value_ssa_value_immediate(input.immediate);
        return 1;
    case LOWER_IR_VALUE_TEMP:
        if (!rename_state || input.temp_id >= next_temp_id ||
            !value_ssa_rename_state_lookup(rename_state, input.temp_id, out_value, error)) {
            value_ssa_set_error(error,
                0,
                0,
                "VALUE-SSA-105: lower-ir temp tmp.%zu is unavailable during value-ssa conversion",
                input.temp_id);
            return 0;
        }
        return 1;
    default:
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-106: lower-ir value uses unknown kind %d during conversion",
            (int)input.kind);
        return 0;
    }
}

static int value_ssa_bind_block_phi_results_from_lower_ir(const LowerIrFunction *input_function,
    size_t block_index,
    const size_t *phi_candidate_counts,
    const size_t *phi_candidate_temps,
    const size_t *phi_candidate_offsets,
    const size_t *phi_result_value_ids,
    ValueSsaRenameState *rename_state,
    ValueSsaError *error) {
    size_t candidate_index;

    if (!input_function || !phi_candidate_counts || !phi_candidate_temps ||
        !phi_candidate_offsets || !phi_result_value_ids || !rename_state) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-101: invalid lower-ir conversion contract");
        return 0;
    }

    for (candidate_index = 0; candidate_index < phi_candidate_counts[block_index]; ++candidate_index) {
        size_t temp_id = phi_candidate_temps[phi_candidate_offsets[block_index] + candidate_index];
        size_t phi_value_id = phi_result_value_ids[phi_candidate_offsets[block_index] + candidate_index];

        if (phi_value_id == (size_t)-1) {
            value_ssa_set_error(error,
                0,
                0,
                "VALUE-SSA-184: missing precreated phi for lower-ir tmp.%zu in bb.%zu during strict conversion",
                temp_id,
                block_index);
            return 0;
        }
        if (!value_ssa_rename_state_bind(rename_state,
                temp_id,
                value_ssa_value_id(phi_value_id),
                error)) {
            return 0;
        }
    }

    return 1;
}

static int value_ssa_finalize_pending_phis_from_lower_ir(const LowerIrFunction *input_function,
    const LowerIrCfgAnalysis *input_analysis,
    const size_t *phi_candidate_counts,
    const size_t *phi_candidate_temps,
    const size_t *phi_candidate_offsets,
    const size_t *phi_result_value_ids,
    ValueSsaFunction *output_function,
    ValueSsaError *error) {
    size_t block_index;
    const size_t *predecessor_counts;

    if (!input_function || !input_analysis ||
        !phi_candidate_counts || !phi_candidate_temps || !phi_candidate_offsets ||
        !phi_result_value_ids || !output_function) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-101: invalid lower-ir conversion contract");
        return 0;
    }

    predecessor_counts = input_analysis->predecessor_counts;
    for (block_index = 0; block_index < input_function->block_count; ++block_index) {
        ValueSsaBasicBlock *output_block = &output_function->blocks[block_index];

        for (size_t candidate_index = 0; candidate_index < phi_candidate_counts[block_index]; ++candidate_index) {
            ValueSsaPhi *phi = NULL;
            size_t phi_value_id;
            size_t phi_index;
            size_t slot_index;
            size_t temp_id = phi_candidate_temps[phi_candidate_offsets[block_index] + candidate_index];

            slot_index = phi_candidate_offsets[block_index] + candidate_index;
            if (phi_result_value_ids[slot_index] == (size_t)-1) {
                continue;
            }

            phi_value_id = phi_result_value_ids[slot_index];
            for (phi_index = 0; phi_index < output_block->phi_count; ++phi_index) {
                if (output_block->phis[phi_index].result_id == phi_value_id) {
                    phi = &output_block->phis[phi_index];
                    break;
                }
            }
            if (!phi) {
                value_ssa_set_error(error,
                    0,
                    0,
                    "VALUE-SSA-120: internal phi bookkeeping lost ssa.%zu for bb.%zu",
                    phi_value_id,
                    block_index);
                return 0;
            }
            if (phi->input_count == predecessor_counts[block_index]) {
                continue;
            }
            value_ssa_set_error(error,
                0,
                0,
                "VALUE-SSA-182: phi ssa.%zu for lower-ir tmp.%zu in bb.%zu still misses %zu incoming edge(s) after dominator conversion",
                phi_value_id,
                temp_id,
                block_index,
                predecessor_counts[block_index] - phi->input_count);
            return 0;
        }
    }

    return 1;
}

static int value_ssa_precreate_block_phis_from_lower_ir(ValueSsaFunction *output_function,
    const LowerIrFunction *input_function,
    const size_t *phi_candidate_counts,
    const size_t *phi_candidate_offsets,
    size_t *phi_result_value_ids,
    ValueSsaError *error) {
    size_t block_index;

    if (!output_function || !input_function || !phi_candidate_counts || !phi_candidate_offsets || !phi_result_value_ids) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-101: invalid lower-ir conversion contract");
        return 0;
    }

    for (block_index = 0; block_index < input_function->block_count; ++block_index) {
        size_t candidate_index;

        for (candidate_index = 0; candidate_index < phi_candidate_counts[block_index]; ++candidate_index) {
            size_t slot_index = phi_candidate_offsets[block_index] + candidate_index;
            size_t phi_value_id = value_ssa_function_allocate_value(output_function);

            if (phi_value_id == (size_t)-1 ||
                !value_ssa_block_append_phi(&output_function->blocks[block_index], phi_value_id, NULL, 0, error)) {
                if (phi_value_id == (size_t)-1) {
                    value_ssa_set_error(error,
                        0,
                        0,
                        "VALUE-SSA-108: SSA value allocation overflow while precreating phis for '%s'",
                        input_function->name ? input_function->name : "<anon>");
                }
                return 0;
            }
            phi_result_value_ids[slot_index] = phi_value_id;
        }
    }

    return 1;
}

static int value_ssa_copy_globals_from_lower_ir(const LowerIrProgram *input_program,
    ValueSsaProgram *output_program,
    ValueSsaError *error) {
    size_t i;

    for (i = 0; i < input_program->global_count; ++i) {
        const LowerIrGlobal *input_global = &input_program->globals[i];
        ValueSsaGlobal *output_global = NULL;

        if (!value_ssa_program_append_global(output_program,
                input_global->name ? input_global->name : "global",
                &output_global,
                error)) {
            return 0;
        }
        output_global->has_initializer = input_global->has_initializer;
        output_global->initializer_value = input_global->initializer_value;
        output_global->has_runtime_initializer = input_global->has_runtime_initializer;
    }

    return 1;
}

static int value_ssa_convert_function_from_lower_ir(const LowerIrProgram *input_program,
    const LowerIrFunction *input_function,
    ValueSsaProgram *output_program,
    ValueSsaError *error) {
    ValueSsaFunction *output_function = NULL;
    ValueSsaLowerIrConversionState conversion_state;
    ValueSsaRenameState rename_state;
    LowerIrCfgAnalysis input_analysis;
    LowerIrError lower_error;
    size_t *phi_candidate_counts = NULL;
    size_t *phi_candidate_temps = NULL;
    size_t *raw_phi_candidate_temps = NULL;
    size_t *phi_candidate_offsets = NULL;
    size_t *phi_result_value_ids = NULL;
    size_t total_phi_candidate_count = 0;
    size_t i;

    if (!value_ssa_program_append_function(output_program,
            input_function->name ? input_function->name : "<anon>",
            input_function->has_body,
            &output_function,
            error)) {
        return 0;
    }

    for (i = 0; i < input_function->local_count; ++i) {
        if (!value_ssa_function_append_local(output_function,
                input_function->locals[i].source_name ? input_function->locals[i].source_name : "local",
                input_function->locals[i].is_parameter,
                NULL,
                error)) {
            return 0;
        }
    }

    if (!input_function->has_body) {
        return 1;
    }

    memset(&conversion_state, 0, sizeof(conversion_state));
    value_ssa_rename_state_init(&rename_state);
    lower_ir_cfg_analysis_init(&input_analysis);
    phi_candidate_counts = (size_t *)calloc(input_function->block_count, sizeof(size_t));
    raw_phi_candidate_temps = (size_t *)calloc(input_function->block_count * (input_function->next_temp_id == 0 ? 1 : input_function->next_temp_id),
        sizeof(size_t));
    phi_candidate_offsets = (size_t *)calloc(input_function->block_count == 0 ? 1 : input_function->block_count, sizeof(size_t));
    if (!phi_candidate_counts || !raw_phi_candidate_temps || !phi_candidate_offsets) {
        value_ssa_rename_state_free(&rename_state);
        lower_ir_cfg_analysis_free(&input_analysis);
        free(phi_candidate_counts);
        free(raw_phi_candidate_temps);
        free(phi_candidate_offsets);
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-109: out of memory while preparing lower-ir to value-ssa conversion for '%s'",
            input_function->name ? input_function->name : "<anon>");
        return 0;
    }

    if (!lower_ir_compute_cfg_analysis(input_function, &input_analysis, &lower_error)) {
        value_ssa_rename_state_free(&rename_state);
        lower_ir_cfg_analysis_free(&input_analysis);
        free(phi_candidate_counts);
        free(raw_phi_candidate_temps);
        free(phi_candidate_offsets);
        value_ssa_set_error(error,
            lower_error.line,
            lower_error.column,
            "VALUE-SSA-127: lower-ir CFG analysis failed during conversion: %s",
            lower_error.message);
        return 0;
    }
    if (!lower_ir_compute_pruned_temp_phi_candidate_lists(input_function,
            &input_analysis,
            phi_candidate_counts,
            raw_phi_candidate_temps,
            &lower_error)) {
        value_ssa_rename_state_free(&rename_state);
        lower_ir_cfg_analysis_free(&input_analysis);
        free(phi_candidate_counts);
        free(raw_phi_candidate_temps);
        free(phi_candidate_offsets);
        value_ssa_set_error(error,
            lower_error.line,
            lower_error.column,
            "VALUE-SSA-181: lower-ir pruned temp phi-candidate list precompute failed during conversion: %s",
            lower_error.message);
        return 0;
    }

    for (i = 0; i < input_function->block_count; ++i) {
        phi_candidate_offsets[i] = total_phi_candidate_count;
        total_phi_candidate_count += phi_candidate_counts[i];
    }
    phi_candidate_temps = (size_t *)malloc((total_phi_candidate_count == 0 ? 1 : total_phi_candidate_count) * sizeof(size_t));
    if (!phi_candidate_temps) {
        value_ssa_rename_state_free(&rename_state);
        lower_ir_cfg_analysis_free(&input_analysis);
        free(phi_candidate_counts);
        free(raw_phi_candidate_temps);
        free(phi_candidate_offsets);
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-109: out of memory while compacting phi-candidate temps for '%s'",
            input_function->name ? input_function->name : "<anon>");
        return 0;
    }
    for (i = 0; i < input_function->block_count; ++i) {
        size_t candidate_index;
        size_t temp_count = input_function->next_temp_id;

        for (candidate_index = 0; candidate_index < phi_candidate_counts[i]; ++candidate_index) {
            phi_candidate_temps[phi_candidate_offsets[i] + candidate_index] =
                raw_phi_candidate_temps[i * temp_count + candidate_index];
        }
    }
    free(raw_phi_candidate_temps);
    raw_phi_candidate_temps = NULL;
    phi_result_value_ids = (size_t *)malloc((total_phi_candidate_count == 0 ? 1 : total_phi_candidate_count) * sizeof(size_t));
    if (!phi_result_value_ids) {
        value_ssa_rename_state_free(&rename_state);
        lower_ir_cfg_analysis_free(&input_analysis);
        free(phi_candidate_counts);
        free(phi_candidate_temps);
        free(phi_candidate_offsets);
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-109: out of memory while preparing phi result state for '%s'",
            input_function->name ? input_function->name : "<anon>");
        return 0;
    }
    for (i = 0; i < (total_phi_candidate_count == 0 ? 1 : total_phi_candidate_count); ++i) {
        phi_result_value_ids[i] = (size_t)-1;
    }

    for (i = 0; i < input_function->block_count; ++i) {
        if (!value_ssa_function_append_block(output_function, NULL, NULL, error)) {
            value_ssa_rename_state_free(&rename_state);
            lower_ir_cfg_analysis_free(&input_analysis);
            free(phi_candidate_counts);
            free(phi_candidate_temps);
            free(phi_candidate_offsets);
            free(phi_result_value_ids);
            return 0;
        }
    }
    if (!value_ssa_rename_state_prepare(&rename_state, input_function->next_temp_id, error)) {
        value_ssa_rename_state_free(&rename_state);
        lower_ir_cfg_analysis_free(&input_analysis);
        free(phi_candidate_counts);
        free(phi_candidate_temps);
        free(phi_candidate_offsets);
        free(phi_result_value_ids);
        return 0;
    }
    if (!value_ssa_precreate_block_phis_from_lower_ir(output_function,
            input_function,
            phi_candidate_counts,
            phi_candidate_offsets,
            phi_result_value_ids,
            error)) {
        value_ssa_rename_state_free(&rename_state);
        lower_ir_cfg_analysis_free(&input_analysis);
        free(phi_candidate_counts);
        free(phi_candidate_temps);
        free(phi_candidate_offsets);
        free(phi_result_value_ids);
        return 0;
    }
    conversion_state.input_function = input_function;
    conversion_state.input_analysis = &input_analysis;
    conversion_state.phi_candidate_counts = phi_candidate_counts;
    conversion_state.phi_candidate_temps = phi_candidate_temps;
    conversion_state.phi_candidate_offsets = phi_candidate_offsets;
    conversion_state.output_function = output_function;
    conversion_state.rename_state = &rename_state;
    conversion_state.phi_result_value_ids = phi_result_value_ids;
    if (!lower_ir_walk_dominator_tree(input_function,
            &input_analysis,
            value_ssa_convert_block_from_lower_ir_enter,
            value_ssa_convert_block_from_lower_ir_leave,
            &conversion_state,
            &lower_error)) {
        value_ssa_rename_state_free(&rename_state);
        lower_ir_cfg_analysis_free(&input_analysis);
        free(phi_candidate_counts);
        free(phi_candidate_temps);
        free(phi_candidate_offsets);
        free(phi_result_value_ids);
        if (conversion_state.conversion_failed) {
            *error = conversion_state.conversion_error;
        } else {
            value_ssa_set_error(error,
                lower_error.line,
                lower_error.column,
                "VALUE-SSA-130: lower-ir dominator walk failed while converting function '%s': %s",
                input_function->name ? input_function->name : "<anon>",
                lower_error.message);
        }
        return 0;
    }
    if (!value_ssa_finalize_pending_phis_from_lower_ir(input_function,
            &input_analysis,
            phi_candidate_counts,
            phi_candidate_temps,
            phi_candidate_offsets,
            phi_result_value_ids,
            output_function,
            error)) {
        value_ssa_rename_state_free(&rename_state);
        lower_ir_cfg_analysis_free(&input_analysis);
        free(phi_candidate_counts);
        free(phi_candidate_temps);
        free(phi_candidate_offsets);
        free(phi_result_value_ids);
        return 0;
    }
    {
        ValueSsaCfgAnalysis output_analysis;
        int ok;

        value_ssa_cfg_analysis_init(&output_analysis);
        ok = value_ssa_compute_cfg_analysis(output_function, &output_analysis, error) &&
            value_ssa_rename_function_values(output_function, &output_analysis, error);
        value_ssa_cfg_analysis_free(&output_analysis);
        if (!ok) {
            value_ssa_rename_state_free(&rename_state);
            lower_ir_cfg_analysis_free(&input_analysis);
            free(phi_candidate_counts);
            free(phi_candidate_temps);
            free(phi_candidate_offsets);
            free(phi_result_value_ids);
            return 0;
        }
    }

    value_ssa_rename_state_free(&rename_state);
    lower_ir_cfg_analysis_free(&input_analysis);
    free(phi_candidate_counts);
    free(phi_candidate_temps);
    free(phi_candidate_offsets);
    free(phi_result_value_ids);
    (void)input_program;
    return 1;
}

int value_ssa_build_from_lower_ir(const LowerIrProgram *program,
    ValueSsaProgram *out_program,
    ValueSsaError *error) {
    LowerIrError lower_error;
    size_t i;

    if (!program || !out_program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-118: invalid lower-ir conversion entry contract");
        return 0;
    }

    value_ssa_program_init(out_program);

    if (!lower_ir_verify_program(program, &lower_error)) {
        value_ssa_set_error(error,
            lower_error.line,
            lower_error.column,
            "VALUE-SSA-119: input lower IR rejected verifier: %s",
            lower_error.message);
        return 0;
    }

    if (!value_ssa_copy_globals_from_lower_ir(program, out_program, error)) {
        value_ssa_program_free(out_program);
        return 0;
    }

    for (i = 0; i < program->function_count; ++i) {
        if (!value_ssa_convert_function_from_lower_ir(program, &program->functions[i], out_program, error)) {
            value_ssa_program_free(out_program);
            return 0;
        }
    }

    if (!value_ssa_verify_program(out_program, error)) {
        value_ssa_program_free(out_program);
        return 0;
    }

    return 1;
}

#endif
