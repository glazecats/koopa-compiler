#if defined(__INTELLISENSE__) && !defined(MACHINE_IR_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_IR_SPLIT_AGGREGATOR)
#define MACHINE_IR_SPLIT_INTELLISENSE_CONTEXT
#include "machine_ir.c"
#else

typedef struct {
    MachineIrSlotRef slot;
    unsigned char known;
    MachineIrOperand value;
} MachineIrKnownSlotValue;

typedef struct {
    MachineIrSlotRef slot;
    unsigned char observed;
} MachineIrObservedSlotState;

static size_t machine_ir_compute_slot_fact_capacity(const MachineIrProgram *program,
    const MachineIrFunction *function) {
    if (!program || !function) {
        return 0;
    }
    return function->local_count + program->global_count;
}

static size_t machine_ir_slot_fact_index(const MachineIrProgram *program,
    const MachineIrFunction *function,
    MachineIrSlotRef slot) {
    if (!program || !function) {
        return (size_t)-1;
    }
    switch (slot.kind) {
        case MACHINE_IR_SLOT_LOCAL:
            if (slot.id >= function->local_count) {
                return (size_t)-1;
            }
            return slot.id;
        case MACHINE_IR_SLOT_GLOBAL:
            if (slot.id >= program->global_count) {
                return (size_t)-1;
            }
            return function->local_count + slot.id;
        default:
            return (size_t)-1;
    }
}

static void machine_ir_clear_known_slot_values(size_t slot_count,
    MachineIrKnownSlotValue *facts) {
    size_t index;

    if (!facts) {
        return;
    }
    for (index = 0; index < slot_count; ++index) {
        facts[index].known = 0u;
        facts[index].value = machine_ir_operand_none();
    }
}

static void machine_ir_initialize_known_slot_values(const MachineIrProgram *program,
    const MachineIrFunction *function,
    size_t slot_count,
    MachineIrKnownSlotValue *facts) {
    size_t local_index;
    size_t global_index;

    machine_ir_clear_known_slot_values(slot_count, facts);
    if (!program || !function || !facts) {
        return;
    }

    for (local_index = 0; local_index < function->local_count; ++local_index) {
        facts[local_index].slot = machine_ir_slot_local(local_index);
    }
    for (global_index = 0; global_index < program->global_count; ++global_index) {
        facts[function->local_count + global_index].slot = machine_ir_slot_global(global_index);
    }
}

static void machine_ir_clear_observed_slot_states(size_t slot_count,
    MachineIrObservedSlotState *states) {
    size_t index;

    if (!states) {
        return;
    }
    for (index = 0; index < slot_count; ++index) {
        states[index].observed = 0u;
    }
}

static void machine_ir_initialize_observed_slot_states(const MachineIrProgram *program,
    const MachineIrFunction *function,
    size_t slot_count,
    MachineIrObservedSlotState *states) {
    size_t local_index;
    size_t global_index;

    machine_ir_clear_observed_slot_states(slot_count, states);
    if (!program || !function || !states) {
        return;
    }

    for (local_index = 0; local_index < function->local_count; ++local_index) {
        states[local_index].slot = machine_ir_slot_local(local_index);
    }
    for (global_index = 0; global_index < program->global_count; ++global_index) {
        states[function->local_count + global_index].slot = machine_ir_slot_global(global_index);
    }
}

static int machine_ir_observed_slot_states_equal(size_t slot_count,
    const MachineIrObservedSlotState *lhs,
    const MachineIrObservedSlotState *rhs) {
    size_t slot_index;

    if (!lhs || !rhs) {
        return 0;
    }
    for (slot_index = 0; slot_index < slot_count; ++slot_index) {
        if (lhs[slot_index].observed != rhs[slot_index].observed) {
            return 0;
        }
    }
    return 1;
}

static void machine_ir_copy_observed_slot_states(size_t slot_count,
    MachineIrObservedSlotState *destination,
    const MachineIrObservedSlotState *source) {
    if (!destination || !source || slot_count == 0) {
        return;
    }
    memcpy(destination, source, slot_count * sizeof(MachineIrObservedSlotState));
}

static void machine_ir_mark_global_slots_observed(size_t slot_count,
    MachineIrObservedSlotState *states) {
    size_t slot_index;

    if (!states) {
        return;
    }
    for (slot_index = 0; slot_index < slot_count; ++slot_index) {
        if (states[slot_index].slot.kind == MACHINE_IR_SLOT_GLOBAL) {
            states[slot_index].observed = 1u;
        }
    }
}

static int machine_ir_known_slot_values_equal(size_t slot_count,
    const MachineIrKnownSlotValue *lhs,
    const MachineIrKnownSlotValue *rhs) {
    size_t slot_index;

    if (!lhs || !rhs) {
        return 0;
    }
    for (slot_index = 0; slot_index < slot_count; ++slot_index) {
        if (lhs[slot_index].known != rhs[slot_index].known) {
            return 0;
        }
        if (lhs[slot_index].known &&
            !machine_ir_operands_equal(lhs[slot_index].value, rhs[slot_index].value)) {
            return 0;
        }
    }
    return 1;
}

static void machine_ir_copy_known_slot_values(size_t slot_count,
    MachineIrKnownSlotValue *destination,
    const MachineIrKnownSlotValue *source) {
    if (!destination || !source || slot_count == 0) {
        return;
    }
    memcpy(destination, source, slot_count * sizeof(MachineIrKnownSlotValue));
}

static void machine_ir_apply_call_global_effects_to_known_slot_values(const MachineIrProgram *program,
    const MachineIrGlobalCallEffectSummary *summary,
    const MachineIrInstruction *instruction,
    size_t slot_count,
    MachineIrKnownSlotValue *facts) {
    size_t slot_index;

    if (!program || !instruction || !facts || instruction->kind != MACHINE_IR_INSTR_CALL) {
        return;
    }
    for (slot_index = 0; slot_index < slot_count; ++slot_index) {
        if (facts[slot_index].slot.kind != MACHINE_IR_SLOT_GLOBAL) {
            continue;
        }
        if (machine_ir_call_may_write_global(program, summary, instruction, facts[slot_index].slot.id)) {
            facts[slot_index].known = 0u;
            facts[slot_index].value = machine_ir_operand_none();
        }
    }
}

static void machine_ir_invalidate_known_slot_values_using_operand(size_t slot_count,
    MachineIrKnownSlotValue *facts,
    MachineIrOperand operand) {
    size_t slot_index;

    if (!facts) {
        return;
    }
    for (slot_index = 0; slot_index < slot_count; ++slot_index) {
        if (!facts[slot_index].known) {
            continue;
        }
        if (machine_ir_operands_equal(facts[slot_index].value, operand)) {
            facts[slot_index].known = 0u;
            facts[slot_index].value = machine_ir_operand_none();
        }
    }
}

static void machine_ir_invalidate_known_slot_values_for_result(size_t slot_count,
    MachineIrKnownSlotValue *facts,
    const MachineIrInstruction *instruction) {
    if (!instruction || !instruction->has_result) {
        return;
    }
    machine_ir_invalidate_known_slot_values_using_operand(slot_count, facts, instruction->result);
}

static void machine_ir_invalidate_known_slot_values_for_caller_clobbers(const MachineIrProgram *program,
    size_t slot_count,
    MachineIrKnownSlotValue *facts) {
    size_t register_index;

    if (!program || !facts) {
        return;
    }
    for (register_index = 0; register_index < program->register_bank.register_count; ++register_index) {
        if (!program->register_bank.registers[register_index].caller_clobbered) {
            continue;
        }
        machine_ir_invalidate_known_slot_values_using_operand(
            slot_count,
            facts,
            machine_ir_operand_register(register_index));
    }
}

static MachineIrOperand machine_ir_resolve_known_slot_operand(size_t resource_count,
    const MachineIrProgram *program,
    const MachineIrFunction *function,
    size_t slot_count,
    const MachineIrKnownSlotValue *facts,
    MachineIrOperand operand) {
    size_t hop_count = 0;

    if (!facts) {
        return operand;
    }

    while (hop_count < resource_count + slot_count) {
        size_t slot_index;
        int found = 0;
        MachineIrOperand next_operand = operand;

        for (slot_index = 0; slot_index < slot_count; ++slot_index) {
            if (!facts[slot_index].known) {
                continue;
            }
            if (!machine_ir_operands_equal(facts[slot_index].value, operand)) {
                continue;
            }
            next_operand = facts[slot_index].value;
            found = 1;
            break;
        }

        if (!found) {
            return operand;
        }
        if (machine_ir_operands_equal(next_operand, operand)) {
            return operand;
        }
        operand = next_operand;
        ++hop_count;
        (void)program;
        (void)function;
    }

    return operand;
}

static int machine_ir_block_is_load_return_wrapper(const MachineIrBasicBlock *block,
    MachineIrSlotRef *out_slot) {
    const MachineIrInstruction *instruction;

    if (out_slot) {
        memset(out_slot, 0, sizeof(*out_slot));
    }
    if (!block ||
        block->phi_count != 0 ||
        block->instruction_count != 1 ||
        !block->has_terminator ||
        block->terminator.kind != MACHINE_IR_TERM_RETURN) {
        return 0;
    }

    instruction = &block->instructions[0];
    if (!instruction->has_result ||
        !machine_ir_operands_equal(block->terminator.as.return_value, instruction->result)) {
        return 0;
    }

    if (instruction->kind != MACHINE_IR_INSTR_LOAD_LOCAL &&
        instruction->kind != MACHINE_IR_INSTR_LOAD_GLOBAL) {
        return 0;
    }

    if (out_slot) {
        *out_slot = instruction->as.load_slot;
    }
    return 1;
}

static int machine_ir_block_is_load_branch_wrapper(const MachineIrBasicBlock *block,
    MachineIrSlotRef *out_slot,
    MachineIrTerminator *out_branch) {
    const MachineIrInstruction *instruction;

    if (out_slot) {
        memset(out_slot, 0, sizeof(*out_slot));
    }
    if (out_branch) {
        memset(out_branch, 0, sizeof(*out_branch));
    }
    if (!block ||
        block->phi_count != 0 ||
        block->instruction_count != 1 ||
        !block->has_terminator ||
        block->terminator.kind != MACHINE_IR_TERM_BRANCH) {
        return 0;
    }

    instruction = &block->instructions[0];
    if (!instruction->has_result ||
        !machine_ir_operands_equal(block->terminator.as.branch.condition, instruction->result)) {
        return 0;
    }
    if (instruction->kind != MACHINE_IR_INSTR_LOAD_LOCAL &&
        instruction->kind != MACHINE_IR_INSTR_LOAD_GLOBAL) {
        return 0;
    }

    if (out_slot) {
        *out_slot = instruction->as.load_slot;
    }
    if (out_branch) {
        *out_branch = block->terminator;
    }
    return 1;
}

static int machine_ir_instruction_uses_operand(const MachineIrInstruction *instruction,
    MachineIrOperand load_result,
    int *out_uses) {
    int uses = 0;

    if (out_uses) {
        *out_uses = 0;
    }
    if (!instruction) {
        return 0;
    }
    if (instruction->kind == MACHINE_IR_INSTR_MOV) {
        uses = machine_ir_operands_equal(instruction->as.mov_value, load_result);
    } else if (instruction->kind == MACHINE_IR_INSTR_BINARY) {
        uses = machine_ir_operands_equal(instruction->as.binary.lhs, load_result) ||
            machine_ir_operands_equal(instruction->as.binary.rhs, load_result);
    } else {
        return 0;
    }

    if (out_uses) {
        *out_uses = uses;
    }
    return 1;
}

static int machine_ir_rewrite_inlineable_instruction_from_known_operand(MachineIrInstruction *instruction,
    MachineIrOperand replaced_operand,
    MachineIrOperand resolved_value) {
    if (!instruction) {
        return 0;
    }
    if (instruction->kind == MACHINE_IR_INSTR_MOV) {
        if (!machine_ir_operands_equal(instruction->as.mov_value, replaced_operand)) {
            return 0;
        }
        instruction->as.mov_value = resolved_value;
        return 1;
    }

    if (instruction->kind == MACHINE_IR_INSTR_BINARY) {
        int changed = 0;
        if (machine_ir_operands_equal(instruction->as.binary.lhs, replaced_operand)) {
            instruction->as.binary.lhs = resolved_value;
            changed = 1;
        }
        if (machine_ir_operands_equal(instruction->as.binary.rhs, replaced_operand)) {
            instruction->as.binary.rhs = resolved_value;
            changed = 1;
        }
        return changed;
    }

    return 0;
}

static int machine_ir_slot_inlineable_return_instructions_equivalent(
    const MachineIrProgram *program,
    const MachineIrFunction *function,
    const MachineIrInstruction *lhs,
    const MachineIrInstruction *rhs) {
    MachineIrTerminator lhs_return;
    MachineIrTerminator rhs_return;

    if (!lhs || !rhs || !lhs->has_result || !rhs->has_result) {
        return 0;
    }

    lhs_return.kind = MACHINE_IR_TERM_RETURN;
    lhs_return.as.return_value = lhs->result;
    rhs_return.kind = MACHINE_IR_TERM_RETURN;
    rhs_return.as.return_value = rhs->result;
    return machine_ir_inlineable_return_wrapper_instructions_equivalent(
        program,
        function,
        lhs,
        &lhs_return,
        rhs,
        &rhs_return);
}

static int machine_ir_slot_inlineable_branch_instructions_equivalent(
    const MachineIrProgram *program,
    const MachineIrFunction *function,
    const MachineIrInstruction *lhs,
    const MachineIrTerminator *lhs_branch,
    const MachineIrInstruction *rhs,
    const MachineIrTerminator *rhs_branch) {
    MachineIrTerminator lhs_return;
    MachineIrTerminator rhs_return;

    if (!lhs || !rhs || !lhs_branch || !rhs_branch ||
        lhs_branch->kind != MACHINE_IR_TERM_BRANCH ||
        rhs_branch->kind != MACHINE_IR_TERM_BRANCH ||
        !lhs->has_result ||
        !rhs->has_result ||
        !machine_ir_operands_equal(lhs_branch->as.branch.condition, lhs->result) ||
        !machine_ir_operands_equal(rhs_branch->as.branch.condition, rhs->result)) {
        return 0;
    }

    if (lhs_branch->as.branch.then_target != rhs_branch->as.branch.then_target ||
        lhs_branch->as.branch.else_target != rhs_branch->as.branch.else_target) {
        return 0;
    }

    lhs_return.kind = MACHINE_IR_TERM_RETURN;
    lhs_return.as.return_value = lhs->result;
    rhs_return.kind = MACHINE_IR_TERM_RETURN;
    rhs_return.as.return_value = rhs->result;
    return machine_ir_inlineable_return_wrapper_instructions_equivalent(
        program,
        function,
        lhs,
        &lhs_return,
        rhs,
        &rhs_return);
}

static int machine_ir_block_is_load_inlineable_return_wrapper(const MachineIrBasicBlock *block,
    MachineIrSlotRef *out_slot,
    MachineIrOperand *out_load_result,
    MachineIrInstruction *out_instruction) {
    MachineIrSlotRef slot;
    MachineIrOperand load_result;
    MachineIrInstruction instruction;
    int uses = 0;

    if (out_slot) {
        memset(out_slot, 0, sizeof(*out_slot));
    }
    if (out_load_result) {
        *out_load_result = machine_ir_operand_none();
    }
    if (out_instruction) {
        memset(out_instruction, 0, sizeof(*out_instruction));
    }
    if (!block ||
        block->phi_count != 0 ||
        block->instruction_count != 2 ||
        !block->has_terminator ||
        block->terminator.kind != MACHINE_IR_TERM_RETURN) {
        return 0;
    }
    if (!machine_ir_instruction_is_inlineable_pure_value_producer(&block->instructions[1]) ||
        !block->instructions[1].has_result ||
        !machine_ir_operands_equal(block->terminator.as.return_value, block->instructions[1].result)) {
        return 0;
    }
    if (!machine_ir_block_is_load_return_wrapper(
            &(MachineIrBasicBlock){
                .phi_count = 0,
                .instruction_count = 1,
                .instructions = (MachineIrInstruction *)&block->instructions[0],
                .has_terminator = 1,
                .terminator = (MachineIrTerminator){
                    .kind = MACHINE_IR_TERM_RETURN,
                    .as.return_value = block->instructions[0].result
                }
            },
            &slot)) {
        return 0;
    }

    load_result = block->instructions[0].result;
    instruction = block->instructions[1];
    if (!machine_ir_instruction_uses_operand(&instruction, load_result, &uses) || !uses) {
        return 0;
    }

    if (out_slot) {
        *out_slot = slot;
    }
    if (out_load_result) {
        *out_load_result = load_result;
    }
    if (out_instruction) {
        *out_instruction = instruction;
    }
    return 1;
}

static int machine_ir_block_is_load_inlineable_branch_wrapper(const MachineIrBasicBlock *block,
    MachineIrSlotRef *out_slot,
    MachineIrOperand *out_load_result,
    MachineIrInstruction *out_instruction,
    MachineIrTerminator *out_branch) {
    MachineIrSlotRef slot;
    MachineIrOperand load_result;
    MachineIrInstruction instruction;
    MachineIrTerminator branch;
    int uses = 0;

    if (out_slot) {
        memset(out_slot, 0, sizeof(*out_slot));
    }
    if (out_load_result) {
        *out_load_result = machine_ir_operand_none();
    }
    if (out_instruction) {
        memset(out_instruction, 0, sizeof(*out_instruction));
    }
    if (out_branch) {
        memset(out_branch, 0, sizeof(*out_branch));
    }
    if (!block ||
        block->phi_count != 0 ||
        block->instruction_count != 2 ||
        !block->has_terminator ||
        block->terminator.kind != MACHINE_IR_TERM_BRANCH) {
        return 0;
    }
    if (!machine_ir_instruction_is_inlineable_pure_value_producer(&block->instructions[1]) ||
        !block->instructions[1].has_result ||
        !machine_ir_operands_equal(block->terminator.as.branch.condition, block->instructions[1].result)) {
        return 0;
    }
    if (!machine_ir_block_is_load_branch_wrapper(
            &(MachineIrBasicBlock){
                .phi_count = 0,
                .instruction_count = 1,
                .instructions = (MachineIrInstruction *)&block->instructions[0],
                .has_terminator = 1,
                .terminator = (MachineIrTerminator){
                    .kind = MACHINE_IR_TERM_BRANCH,
                    .as.branch = {
                        .condition = block->instructions[0].result,
                        .then_target = block->terminator.as.branch.then_target,
                        .else_target = block->terminator.as.branch.else_target
                    }
                }
            },
            &slot,
            &branch)) {
        return 0;
    }

    load_result = block->instructions[0].result;
    instruction = block->instructions[1];
    if (!machine_ir_instruction_uses_operand(&instruction, load_result, &uses) || !uses) {
        return 0;
    }

    if (out_slot) {
        *out_slot = slot;
    }
    if (out_load_result) {
        *out_load_result = load_result;
    }
    if (out_instruction) {
        *out_instruction = instruction;
    }
    if (out_branch) {
        *out_branch = branch;
    }
    return 1;
}

static void machine_ir_try_fold_known_slot_return_branch_successors(const MachineIrProgram *program,
    const MachineIrFunction *function,
    size_t resource_count,
    size_t slot_count,
    const MachineIrKnownSlotValue *block_exit_facts,
    MachineIrBasicBlock *block,
    MachineIrError *error,
    int *out_changed) {
    size_t block_id;
    size_t then_target_id;
    size_t else_target_id;
    MachineIrSlotRef then_slot;
    MachineIrSlotRef else_slot;
    MachineIrOperand then_load_result;
    MachineIrOperand else_load_result;
    MachineIrInstruction then_instruction;
    MachineIrInstruction else_instruction;
    size_t slot_index;
    MachineIrInstruction rewritten_instruction;
    MachineIrTerminator rewritten_terminator;

    if (!program || !function || !block || !block_exit_facts || !block->has_terminator ||
        block->terminator.kind != MACHINE_IR_TERM_BRANCH) {
        return;
    }

    block_id = block->id;
    if (block_id >= function->block_count) {
        return;
    }

    then_target_id = block->terminator.as.branch.then_target;
    else_target_id = block->terminator.as.branch.else_target;
    if (then_target_id >= function->block_count ||
        else_target_id >= function->block_count ||
        then_target_id == else_target_id) {
        return;
    }
    then_target_id = machine_ir_resolve_threaded_jump_target(function, then_target_id);
    else_target_id = machine_ir_resolve_threaded_jump_target(function, else_target_id);

    if (!machine_ir_block_is_load_return_wrapper(&function->blocks[then_target_id], &then_slot) ||
        !machine_ir_block_is_load_return_wrapper(&function->blocks[else_target_id], &else_slot) ||
        !machine_ir_slots_equal(then_slot, else_slot)) {
        if (!machine_ir_block_is_load_inlineable_return_wrapper(
                &function->blocks[then_target_id], &then_slot, &then_load_result, &then_instruction) ||
            !machine_ir_block_is_load_inlineable_return_wrapper(
                &function->blocks[else_target_id], &else_slot, &else_load_result, &else_instruction) ||
            !machine_ir_slots_equal(then_slot, else_slot) ||
            then_instruction.kind != else_instruction.kind ||
            !machine_ir_operands_equal(then_load_result, function->blocks[then_target_id].instructions[0].result) ||
            !machine_ir_operands_equal(else_load_result, function->blocks[else_target_id].instructions[0].result)) {
            return;
        }

        slot_index = machine_ir_slot_fact_index(program, function, then_slot);
        if (slot_index == (size_t)-1 ||
            !block_exit_facts[(block_id * slot_count) + slot_index].known) {
            return;
        }

        rewritten_instruction = then_instruction;
        if (!machine_ir_rewrite_inlineable_instruction_from_known_operand(
                &rewritten_instruction,
                then_load_result,
                machine_ir_resolve_known_slot_operand(
                    resource_count,
                    program,
                    function,
                    slot_count,
                    block_exit_facts + (block_id * slot_count),
                    block_exit_facts[(block_id * slot_count) + slot_index].value)) ||
            !machine_ir_slot_inlineable_return_instructions_equivalent(
                program, function, &rewritten_instruction, &else_instruction)) {
            return;
        }

        rewritten_terminator.kind = MACHINE_IR_TERM_RETURN;
        rewritten_terminator.as.return_value = rewritten_instruction.result;
        if (!machine_ir_replace_block_with_single_instruction_and_terminator(
                block, &rewritten_instruction, &rewritten_terminator, error)) {
            return;
        }
        if (out_changed) {
            *out_changed = 1;
        }
        return;
    }

    slot_index = machine_ir_slot_fact_index(program, function, then_slot);
    if (slot_index == (size_t)-1 ||
        !block_exit_facts[(block_id * slot_count) + slot_index].known) {
        return;
    }

    block->terminator.kind = MACHINE_IR_TERM_RETURN;
    block->terminator.as.return_value = machine_ir_resolve_known_slot_operand(
        resource_count,
        program,
        function,
        slot_count,
        block_exit_facts + (block_id * slot_count),
        block_exit_facts[(block_id * slot_count) + slot_index].value);
    if (out_changed) {
        *out_changed = 1;
    }
}

static void machine_ir_try_fold_known_slot_branch_branch_successors(const MachineIrProgram *program,
    const MachineIrFunction *function,
    size_t resource_count,
    size_t slot_count,
    const MachineIrKnownSlotValue *block_exit_facts,
    MachineIrBasicBlock *block,
    MachineIrError *error,
    int *out_changed) {
    size_t block_id;
    size_t then_target_id;
    size_t else_target_id;
    MachineIrSlotRef then_slot;
    MachineIrSlotRef else_slot;
    MachineIrOperand then_load_result;
    MachineIrOperand else_load_result;
    MachineIrInstruction then_instruction;
    MachineIrInstruction else_instruction;
    MachineIrTerminator then_branch;
    MachineIrTerminator else_branch;
    size_t slot_index;
    MachineIrInstruction rewritten_instruction;

    if (!program || !function || !block || !block_exit_facts || !block->has_terminator ||
        block->terminator.kind != MACHINE_IR_TERM_BRANCH) {
        return;
    }

    block_id = block->id;
    if (block_id >= function->block_count) {
        return;
    }

    then_target_id = block->terminator.as.branch.then_target;
    else_target_id = block->terminator.as.branch.else_target;
    if (then_target_id >= function->block_count ||
        else_target_id >= function->block_count ||
        then_target_id == else_target_id) {
        return;
    }
    then_target_id = machine_ir_resolve_threaded_jump_target(function, then_target_id);
    else_target_id = machine_ir_resolve_threaded_jump_target(function, else_target_id);

    if (!machine_ir_block_is_load_branch_wrapper(
            &function->blocks[then_target_id], &then_slot, &then_branch) ||
        !machine_ir_block_is_load_branch_wrapper(
            &function->blocks[else_target_id], &else_slot, &else_branch) ||
        !machine_ir_slots_equal(then_slot, else_slot) ||
        then_branch.as.branch.then_target != else_branch.as.branch.then_target ||
        then_branch.as.branch.else_target != else_branch.as.branch.else_target) {
        if (!machine_ir_block_is_load_inlineable_branch_wrapper(
                &function->blocks[then_target_id], &then_slot, &then_load_result, &then_instruction, &then_branch) ||
            !machine_ir_block_is_load_inlineable_branch_wrapper(
                &function->blocks[else_target_id], &else_slot, &else_load_result, &else_instruction, &else_branch) ||
            !machine_ir_slots_equal(then_slot, else_slot) ||
            then_branch.as.branch.then_target != else_branch.as.branch.then_target ||
            then_branch.as.branch.else_target != else_branch.as.branch.else_target) {
            return;
        }

        slot_index = machine_ir_slot_fact_index(program, function, then_slot);
        if (slot_index == (size_t)-1 ||
            !block_exit_facts[(block_id * slot_count) + slot_index].known) {
            return;
        }

        rewritten_instruction = then_instruction;
        if (!machine_ir_rewrite_inlineable_instruction_from_known_operand(
                &rewritten_instruction,
                then_load_result,
                machine_ir_resolve_known_slot_operand(
                    resource_count,
                    program,
                    function,
                    slot_count,
                    block_exit_facts + (block_id * slot_count),
                    block_exit_facts[(block_id * slot_count) + slot_index].value)) ||
            !machine_ir_slot_inlineable_branch_instructions_equivalent(
                program, function, &rewritten_instruction, &then_branch, &else_instruction, &else_branch) ||
            !machine_ir_replace_block_with_single_instruction_and_terminator(
                block, &rewritten_instruction, &then_branch, error)) {
            return;
        }
        if (out_changed) {
            *out_changed = 1;
        }
        return;
    }

    slot_index = machine_ir_slot_fact_index(program, function, then_slot);
    if (slot_index == (size_t)-1 ||
        !block_exit_facts[(block_id * slot_count) + slot_index].known) {
        return;
    }

    block->terminator.kind = MACHINE_IR_TERM_BRANCH;
    block->terminator.as.branch.condition = machine_ir_resolve_known_slot_operand(
        resource_count,
        program,
        function,
        slot_count,
        block_exit_facts + (block_id * slot_count),
        block_exit_facts[(block_id * slot_count) + slot_index].value);
    block->terminator.as.branch.then_target = then_branch.as.branch.then_target;
    block->terminator.as.branch.else_target = then_branch.as.branch.else_target;
    if (out_changed) {
        *out_changed = 1;
    }
}

static void machine_ir_try_fold_known_slot_return_tails(const MachineIrProgram *program,
    const MachineIrFunction *function,
    size_t resource_count,
    size_t slot_count,
    const MachineIrKnownSlotValue *block_exit_facts,
    MachineIrBasicBlock *block,
    MachineIrError *error,
    int *out_changed) {
    size_t block_id;
    size_t slot_index;
    MachineIrSlotRef slot;
    MachineIrOperand load_result;
    MachineIrInstruction inline_instruction;
    MachineIrInstruction rewritten_instruction;
    MachineIrTerminator rewritten_terminator;

    if (!program || !function || !block || !block_exit_facts || !block->has_terminator) {
        return;
    }
    block_id = block->id;
    if (block_id >= function->block_count) {
        return;
    }

    if (block->terminator.kind != MACHINE_IR_TERM_JUMP) {
        return;
    }
    if (block->terminator.as.jump_target >= function->block_count) {
        return;
    }
    block->terminator.as.jump_target =
        machine_ir_resolve_threaded_jump_target(function, block->terminator.as.jump_target);
    if (machine_ir_block_is_load_return_wrapper(&function->blocks[block->terminator.as.jump_target], &slot)) {
        slot_index = machine_ir_slot_fact_index(program, function, slot);
        if (slot_index == (size_t)-1 ||
            !block_exit_facts[(block_id * slot_count) + slot_index].known) {
            return;
        }

        block->terminator.kind = MACHINE_IR_TERM_RETURN;
        block->terminator.as.return_value = machine_ir_resolve_known_slot_operand(
            resource_count,
            program,
            function,
            slot_count,
            block_exit_facts + (block_id * slot_count),
            block_exit_facts[(block_id * slot_count) + slot_index].value);
    } else if (machine_ir_block_is_load_inlineable_return_wrapper(
            &function->blocks[block->terminator.as.jump_target],
            &slot,
            &load_result,
            &inline_instruction)) {
        slot_index = machine_ir_slot_fact_index(program, function, slot);
        if (slot_index == (size_t)-1 ||
            !block_exit_facts[(block_id * slot_count) + slot_index].known) {
            return;
        }

        rewritten_instruction = inline_instruction;
        if (!machine_ir_rewrite_inlineable_instruction_from_known_operand(
                &rewritten_instruction,
                load_result,
                machine_ir_resolve_known_slot_operand(
                    resource_count,
                    program,
                    function,
                    slot_count,
                    block_exit_facts + (block_id * slot_count),
                    block_exit_facts[(block_id * slot_count) + slot_index].value))) {
            return;
        }

        rewritten_terminator.kind = MACHINE_IR_TERM_RETURN;
        rewritten_terminator.as.return_value = rewritten_instruction.result;
        if (!machine_ir_replace_block_with_single_instruction_and_terminator(
                block, &rewritten_instruction, &rewritten_terminator, error)) {
            return;
        }
    } else {
        return;
    }
    if (out_changed) {
        *out_changed = 1;
    }
}

static void machine_ir_try_fold_known_slot_branch_tails(const MachineIrProgram *program,
    const MachineIrFunction *function,
    size_t resource_count,
    size_t slot_count,
    const MachineIrKnownSlotValue *block_exit_facts,
    MachineIrBasicBlock *block,
    MachineIrError *error,
    int *out_changed) {
    size_t block_id;
    size_t slot_index;
    MachineIrSlotRef slot;
    MachineIrOperand load_result;
    MachineIrInstruction inline_instruction;
    MachineIrInstruction rewritten_instruction;
    MachineIrTerminator branch_terminator;
    MachineIrOperand resolved_value;

    if (!program || !function || !block || !block_exit_facts || !block->has_terminator) {
        return;
    }
    block_id = block->id;
    if (block_id >= function->block_count) {
        return;
    }

    if (block->terminator.kind != MACHINE_IR_TERM_JUMP) {
        return;
    }
    if (block->terminator.as.jump_target >= function->block_count) {
        return;
    }
    block->terminator.as.jump_target =
        machine_ir_resolve_threaded_jump_target(function, block->terminator.as.jump_target);
    if (machine_ir_block_is_load_branch_wrapper(
            &function->blocks[block->terminator.as.jump_target], &slot, &branch_terminator)) {
        slot_index = machine_ir_slot_fact_index(program, function, slot);
        if (slot_index == (size_t)-1 ||
            !block_exit_facts[(block_id * slot_count) + slot_index].known) {
            return;
        }

        resolved_value = machine_ir_resolve_known_slot_operand(
            resource_count,
            program,
            function,
            slot_count,
            block_exit_facts + (block_id * slot_count),
            block_exit_facts[(block_id * slot_count) + slot_index].value);

        block->terminator.kind = MACHINE_IR_TERM_BRANCH;
        block->terminator.as.branch.condition = resolved_value;
        block->terminator.as.branch.then_target = branch_terminator.as.branch.then_target;
        block->terminator.as.branch.else_target = branch_terminator.as.branch.else_target;
    } else if (machine_ir_block_is_load_inlineable_branch_wrapper(
            &function->blocks[block->terminator.as.jump_target],
            &slot,
            &load_result,
            &inline_instruction,
            &branch_terminator)) {
        slot_index = machine_ir_slot_fact_index(program, function, slot);
        if (slot_index == (size_t)-1 ||
            !block_exit_facts[(block_id * slot_count) + slot_index].known) {
            return;
        }

        rewritten_instruction = inline_instruction;
        if (!machine_ir_rewrite_inlineable_instruction_from_known_operand(
                &rewritten_instruction,
                load_result,
                machine_ir_resolve_known_slot_operand(
                    resource_count,
                    program,
                    function,
                    slot_count,
                    block_exit_facts + (block_id * slot_count),
                    block_exit_facts[(block_id * slot_count) + slot_index].value))) {
            return;
        }

        if (!machine_ir_replace_block_with_single_instruction_and_terminator(
                block, &rewritten_instruction, &branch_terminator, error)) {
            return;
        }
    } else {
        return;
    }
    if (out_changed) {
        *out_changed = 1;
    }
}

static void machine_ir_rewrite_instruction_operands_with_known_slot_values(size_t resource_count,
    const MachineIrProgram *program,
    const MachineIrFunction *function,
    size_t slot_count,
    const MachineIrKnownSlotValue *facts,
    MachineIrInstruction *instruction,
    int *out_changed) {
    MachineIrOperand before;
    MachineIrOperand after;
    size_t arg_index;

    if (!instruction) {
        return;
    }

    switch (instruction->kind) {
        case MACHINE_IR_INSTR_MOV:
            before = instruction->as.mov_value;
            after = machine_ir_resolve_known_slot_operand(
                resource_count, program, function, slot_count, facts, before);
            if (!machine_ir_operands_equal(before, after)) {
                instruction->as.mov_value = after;
                if (out_changed) {
                    *out_changed = 1;
                }
            }
            break;
        case MACHINE_IR_INSTR_BINARY:
            before = instruction->as.binary.lhs;
            after = machine_ir_resolve_known_slot_operand(
                resource_count, program, function, slot_count, facts, before);
            if (!machine_ir_operands_equal(before, after)) {
                instruction->as.binary.lhs = after;
                if (out_changed) {
                    *out_changed = 1;
                }
            }
            before = instruction->as.binary.rhs;
            after = machine_ir_resolve_known_slot_operand(
                resource_count, program, function, slot_count, facts, before);
            if (!machine_ir_operands_equal(before, after)) {
                instruction->as.binary.rhs = after;
                if (out_changed) {
                    *out_changed = 1;
                }
            }
            break;
        case MACHINE_IR_INSTR_CALL:
            for (arg_index = 0; arg_index < instruction->as.call.arg_count; ++arg_index) {
                before = instruction->as.call.args[arg_index];
                after = machine_ir_resolve_known_slot_operand(
                    resource_count, program, function, slot_count, facts, before);
                if (!machine_ir_operands_equal(before, after)) {
                    instruction->as.call.args[arg_index] = after;
                    if (out_changed) {
                        *out_changed = 1;
                    }
                }
            }
            break;
        case MACHINE_IR_INSTR_STORE_LOCAL:
        case MACHINE_IR_INSTR_STORE_GLOBAL:
            before = instruction->as.store.value;
            after = machine_ir_resolve_known_slot_operand(
                resource_count, program, function, slot_count, facts, before);
            if (!machine_ir_operands_equal(before, after)) {
                instruction->as.store.value = after;
                if (out_changed) {
                    *out_changed = 1;
                }
            }
            break;
        default:
            break;
    }
}

static void machine_ir_rewrite_terminator_operands_with_known_slot_values(size_t resource_count,
    const MachineIrProgram *program,
    const MachineIrFunction *function,
    size_t slot_count,
    const MachineIrKnownSlotValue *facts,
    MachineIrTerminator *terminator,
    int *out_changed) {
    MachineIrOperand before;
    MachineIrOperand after;

    if (!terminator) {
        return;
    }

    switch (terminator->kind) {
        case MACHINE_IR_TERM_RETURN:
            before = terminator->as.return_value;
            after = machine_ir_resolve_known_slot_operand(
                resource_count, program, function, slot_count, facts, before);
            if (!machine_ir_operands_equal(before, after)) {
                terminator->as.return_value = after;
                if (out_changed) {
                    *out_changed = 1;
                }
            }
            break;
        case MACHINE_IR_TERM_BRANCH:
            before = terminator->as.branch.condition;
            after = machine_ir_resolve_known_slot_operand(
                resource_count, program, function, slot_count, facts, before);
            if (!machine_ir_operands_equal(before, after)) {
                terminator->as.branch.condition = after;
                if (out_changed) {
                    *out_changed = 1;
                }
            }
            break;
        default:
            break;
    }
}

static void machine_ir_compute_block_entry_known_slot_values(const MachineIrProgram *program,
    const MachineIrFunction *function,
    size_t slot_count,
    size_t block_id,
    const size_t *predecessor_counts,
    const MachineIrKnownSlotValue *block_exit_facts,
    MachineIrKnownSlotValue *entry_facts) {
    size_t predecessor_block_id;
    int seeded = 0;

    if (!program || !function || !predecessor_counts || !block_exit_facts || !entry_facts ||
        block_id >= function->block_count) {
        return;
    }

    machine_ir_initialize_known_slot_values(program, function, slot_count, entry_facts);
    if (block_id == 0 || predecessor_counts[block_id] == 0) {
        return;
    }

    for (predecessor_block_id = 0; predecessor_block_id < function->block_count; ++predecessor_block_id) {
        const MachineIrBasicBlock *predecessor = &function->blocks[predecessor_block_id];
        const MachineIrKnownSlotValue *pred_exit = block_exit_facts + (predecessor_block_id * slot_count);
        size_t slot_index;

        if (!machine_ir_block_reaches_target(predecessor, block_id)) {
            continue;
        }

        if (!seeded) {
            machine_ir_copy_known_slot_values(slot_count, entry_facts, pred_exit);
            seeded = 1;
            continue;
        }

        for (slot_index = 0; slot_index < slot_count; ++slot_index) {
            if (!entry_facts[slot_index].known || !pred_exit[slot_index].known) {
                entry_facts[slot_index].known = 0u;
                entry_facts[slot_index].value = machine_ir_operand_none();
                continue;
            }
            if (!machine_ir_operands_equal(entry_facts[slot_index].value, pred_exit[slot_index].value)) {
                entry_facts[slot_index].known = 0u;
                entry_facts[slot_index].value = machine_ir_operand_none();
            }
        }
    }

    if (!seeded) {
        machine_ir_clear_known_slot_values(slot_count, entry_facts);
    }
}

static int machine_ir_compute_block_exit_known_slot_values(const MachineIrProgram *program,
    const MachineIrFunction *function,
    size_t resource_count,
    size_t slot_count,
    const size_t *predecessor_counts,
    const MachineIrGlobalCallEffectSummary *call_effects,
    const unsigned char *block_exit_alias_valid,
    const MachineIrOperand *block_exit_alias_values,
    MachineIrKnownSlotValue *block_exit_facts,
    MachineIrError *error) {
    MachineIrKnownSlotValue *entry_facts = NULL;
    MachineIrKnownSlotValue *local_facts = NULL;
    unsigned char *entry_alias_valid = NULL;
    MachineIrOperand *entry_alias_values = NULL;
    unsigned char *local_alias_valid = NULL;
    MachineIrOperand *local_alias_values = NULL;
    int changed = 1;
    size_t block_index;

    if (!program || !function || !predecessor_counts || !block_exit_facts ||
        !block_exit_alias_valid || !block_exit_alias_values) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-533: invalid block-exit known-slot computation contract");
        return 0;
    }

    entry_facts = (MachineIrKnownSlotValue *)calloc(slot_count, sizeof(MachineIrKnownSlotValue));
    local_facts = (MachineIrKnownSlotValue *)calloc(slot_count, sizeof(MachineIrKnownSlotValue));
    entry_alias_valid = (unsigned char *)calloc(resource_count, sizeof(unsigned char));
    entry_alias_values = (MachineIrOperand *)calloc(resource_count, sizeof(MachineIrOperand));
    local_alias_valid = (unsigned char *)calloc(resource_count, sizeof(unsigned char));
    local_alias_values = (MachineIrOperand *)calloc(resource_count, sizeof(MachineIrOperand));
    if (!entry_facts || !local_facts ||
        (resource_count > 0 &&
            (!entry_alias_valid || !entry_alias_values || !local_alias_valid || !local_alias_values))) {
        free(local_alias_values);
        free(local_alias_valid);
        free(entry_alias_values);
        free(entry_alias_valid);
        free(local_facts);
        free(entry_facts);
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-534: out of memory computing block-exit known-slot facts");
        return 0;
    }
    machine_ir_initialize_known_slot_values(program, function, slot_count, entry_facts);
    machine_ir_initialize_known_slot_values(program, function, slot_count, local_facts);

    while (changed) {
        changed = 0;
        for (block_index = 0; block_index < function->block_count; ++block_index) {
            const MachineIrBasicBlock *block = &function->blocks[block_index];
            MachineIrKnownSlotValue *exit_facts = block_exit_facts + (block_index * slot_count);
            size_t instruction_index;

            machine_ir_compute_block_entry_known_slot_values(
                program, function, slot_count, block_index, predecessor_counts, block_exit_facts, entry_facts);
            machine_ir_copy_known_slot_values(slot_count, local_facts, entry_facts);
            if (resource_count > 0) {
                machine_ir_compute_block_entry_aliases(
                    function,
                    block_index,
                    resource_count,
                    predecessor_counts,
                    block_exit_alias_valid,
                    block_exit_alias_values,
                    entry_alias_valid,
                    entry_alias_values);
                memcpy(local_alias_valid, entry_alias_valid, resource_count * sizeof(unsigned char));
                memcpy(local_alias_values, entry_alias_values, resource_count * sizeof(MachineIrOperand));
            }

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                const MachineIrInstruction *instruction = &block->instructions[instruction_index];
                size_t result_index;
                size_t slot_index;

                machine_ir_invalidate_known_slot_values_for_result(slot_count, local_facts, instruction);
                if (resource_count > 0 &&
                    instruction->has_result &&
                    machine_ir_operand_get_resource_index(program, function, instruction->result, &result_index)) {
                    machine_ir_invalidate_local_copy_aliases(
                        program, function, resource_count, local_alias_valid, local_alias_values, instruction->result);
                }

                if (instruction->kind == MACHINE_IR_INSTR_CALL) {
                    machine_ir_invalidate_known_slot_values_for_caller_clobbers(program, slot_count, local_facts);
                    if (resource_count > 0) {
                        machine_ir_invalidate_caller_clobbered_aliases(
                            program, function, resource_count, local_alias_valid, local_alias_values);
                    }
                    machine_ir_apply_call_global_effects_to_known_slot_values(
                        program, call_effects, instruction, slot_count, local_facts);
                    continue;
                }

                if (resource_count > 0 &&
                    instruction->kind == MACHINE_IR_INSTR_MOV &&
                    instruction->has_result &&
                    machine_ir_operand_get_resource_index(program, function, instruction->result, &result_index) &&
                    !machine_ir_operands_equal(instruction->result, instruction->as.mov_value)) {
                    local_alias_valid[result_index] = 1u;
                    local_alias_values[result_index] =
                        machine_ir_resolve_local_copy_alias(
                            resource_count,
                            program,
                            function,
                            local_alias_valid,
                            local_alias_values,
                            instruction->as.mov_value);
                }

                if ((instruction->kind == MACHINE_IR_INSTR_LOAD_LOCAL ||
                        instruction->kind == MACHINE_IR_INSTR_LOAD_GLOBAL) &&
                    instruction->has_result) {
                    slot_index = machine_ir_slot_fact_index(program, function, instruction->as.load_slot);
                    if (slot_index != (size_t)-1) {
                        if (local_facts[slot_index].known) {
                            local_facts[slot_index].value = machine_ir_resolve_known_slot_operand(
                                resource_count,
                                program,
                                function,
                                slot_count,
                                local_facts,
                                local_facts[slot_index].value);
                        } else {
                            local_facts[slot_index].known = 1u;
                            local_facts[slot_index].value = instruction->result;
                        }
                    }
                    continue;
                }

                if (instruction->kind == MACHINE_IR_INSTR_STORE_LOCAL ||
                    instruction->kind == MACHINE_IR_INSTR_STORE_GLOBAL) {
                    slot_index = machine_ir_slot_fact_index(program, function, instruction->as.store.slot);
                    if (slot_index != (size_t)-1) {
                        local_facts[slot_index].known = 1u;
                        local_facts[slot_index].value = resource_count > 0
                            ? machine_ir_resolve_local_copy_alias(
                                  resource_count,
                                  program,
                                  function,
                                  local_alias_valid,
                                  local_alias_values,
                                  instruction->as.store.value)
                            : instruction->as.store.value;
                    }
                }
            }

            if (!machine_ir_known_slot_values_equal(slot_count, exit_facts, local_facts)) {
                machine_ir_copy_known_slot_values(slot_count, exit_facts, local_facts);
                changed = 1;
            }
        }
    }

    free(local_alias_values);
    free(local_alias_valid);
    free(entry_alias_values);
    free(entry_alias_valid);
    free(local_facts);
    free(entry_facts);
    return 1;
}

static void machine_ir_compute_block_exit_observed_slot_states(const MachineIrProgram *program,
    const MachineIrFunction *function,
    size_t slot_count,
    size_t block_id,
    const MachineIrObservedSlotState *block_entry_states,
    MachineIrObservedSlotState *exit_states) {
    const MachineIrBasicBlock *block;
    size_t successor_block_id;
    int seeded = 0;

    if (!program || !function || !block_entry_states || !exit_states || block_id >= function->block_count) {
        return;
    }

    machine_ir_initialize_observed_slot_states(program, function, slot_count, exit_states);
    block = &function->blocks[block_id];
    if (!block->has_terminator) {
        return;
    }

    for (successor_block_id = 0; successor_block_id < function->block_count; ++successor_block_id) {
        size_t slot_index;
        const MachineIrObservedSlotState *succ_entry;

        if (!machine_ir_block_reaches_target(block, successor_block_id)) {
            continue;
        }
        succ_entry = block_entry_states + (successor_block_id * slot_count);

        if (!seeded) {
            machine_ir_copy_observed_slot_states(slot_count, exit_states, succ_entry);
            seeded = 1;
            continue;
        }

        for (slot_index = 0; slot_index < slot_count; ++slot_index) {
            if (succ_entry[slot_index].observed) {
                exit_states[slot_index].observed = 1u;
            }
        }
    }

    if (!seeded) {
        machine_ir_clear_observed_slot_states(slot_count, exit_states);
    }
}

static int machine_ir_compute_block_entry_observed_slot_states(const MachineIrProgram *program,
    const MachineIrFunction *function,
    size_t slot_count,
    const MachineIrGlobalCallEffectSummary *call_effects,
    MachineIrObservedSlotState *block_entry_states,
    MachineIrObservedSlotState *block_exit_states,
    MachineIrError *error) {
    MachineIrObservedSlotState *local_states = NULL;
    int changed = 1;
    size_t block_index;

    if (!program || !function || !block_entry_states || !block_exit_states) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-535: invalid observed-slot backward analysis contract");
        return 0;
    }

    local_states = (MachineIrObservedSlotState *)calloc(slot_count, sizeof(MachineIrObservedSlotState));
    if (!local_states) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-536: out of memory computing observed-slot facts");
        return 0;
    }
    machine_ir_initialize_observed_slot_states(program, function, slot_count, local_states);

    while (changed) {
        changed = 0;
        for (block_index = function->block_count; block_index > 0; --block_index) {
            const MachineIrBasicBlock *block = &function->blocks[block_index - 1];
            MachineIrObservedSlotState *entry_states =
                block_entry_states + ((block_index - 1) * slot_count);
            MachineIrObservedSlotState *exit_states =
                block_exit_states + ((block_index - 1) * slot_count);
            size_t instruction_index;

            machine_ir_compute_block_exit_observed_slot_states(
                program, function, slot_count, block_index - 1, block_entry_states, exit_states);
            machine_ir_copy_observed_slot_states(slot_count, local_states, exit_states);

            if (block->has_terminator && block->terminator.kind == MACHINE_IR_TERM_RETURN) {
                machine_ir_mark_global_slots_observed(slot_count, local_states);
            }

            for (instruction_index = block->instruction_count; instruction_index > 0; --instruction_index) {
                const MachineIrInstruction *instruction = &block->instructions[instruction_index - 1];
                size_t slot_index;

                switch (instruction->kind) {
                    case MACHINE_IR_INSTR_LOAD_LOCAL:
                    case MACHINE_IR_INSTR_LOAD_GLOBAL:
                        slot_index = machine_ir_slot_fact_index(program, function, instruction->as.load_slot);
                        if (slot_index != (size_t)-1) {
                            local_states[slot_index].observed = 1u;
                        }
                        break;
                    case MACHINE_IR_INSTR_STORE_LOCAL:
                    case MACHINE_IR_INSTR_STORE_GLOBAL:
                        slot_index = machine_ir_slot_fact_index(program, function, instruction->as.store.slot);
                        if (slot_index != (size_t)-1) {
                            local_states[slot_index].observed = 0u;
                        }
                        break;
                    case MACHINE_IR_INSTR_CALL:
                        for (slot_index = 0; slot_index < slot_count; ++slot_index) {
                            if (local_states[slot_index].slot.kind == MACHINE_IR_SLOT_GLOBAL &&
                                machine_ir_call_may_read_global(
                                    program, call_effects, instruction, local_states[slot_index].slot.id)) {
                                local_states[slot_index].observed = 1u;
                            }
                        }
                        break;
                    default:
                        break;
                }
            }

            if (!machine_ir_observed_slot_states_equal(slot_count, entry_states, local_states)) {
                machine_ir_copy_observed_slot_states(slot_count, entry_states, local_states);
                changed = 1;
            }
        }
    }

    free(local_states);
    return 1;
}

static int machine_ir_eliminate_dead_slot_stores(const MachineIrProgram *program,
    MachineIrFunction *function,
    size_t slot_count,
    const MachineIrObservedSlotState *block_exit_states,
    int *out_changed,
    MachineIrError *error) {
    size_t block_index;

    if (!program || !function || !block_exit_states) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-537: invalid dead-slot-store cleanup contract");
        return 0;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        MachineIrBasicBlock *block = &function->blocks[block_index];
        MachineIrObservedSlotState *local_states = NULL;
        unsigned char *keep = NULL;
        size_t instruction_index;
        size_t write_index;

        local_states = (MachineIrObservedSlotState *)calloc(slot_count, sizeof(MachineIrObservedSlotState));
        keep = (unsigned char *)calloc(block->instruction_count, sizeof(unsigned char));
        if ((slot_count > 0 && !local_states) || (block->instruction_count > 0 && !keep)) {
            free(keep);
            free(local_states);
            machine_ir_set_error(error, 0, 0, "MACHINE-IR-538: out of memory eliminating dead slot stores");
            return 0;
        }

        machine_ir_initialize_observed_slot_states(program, function, slot_count, local_states);
        machine_ir_copy_observed_slot_states(
            slot_count, local_states, block_exit_states + (block_index * slot_count));
        if (block->has_terminator && block->terminator.kind == MACHINE_IR_TERM_RETURN) {
            machine_ir_mark_global_slots_observed(slot_count, local_states);
        }

        for (instruction_index = block->instruction_count; instruction_index > 0; --instruction_index) {
            MachineIrInstruction *instruction = &block->instructions[instruction_index - 1];
            size_t slot_index;

            keep[instruction_index - 1] = 1u;
            switch (instruction->kind) {
                case MACHINE_IR_INSTR_LOAD_LOCAL:
                case MACHINE_IR_INSTR_LOAD_GLOBAL:
                    slot_index = machine_ir_slot_fact_index(program, function, instruction->as.load_slot);
                    if (slot_index != (size_t)-1) {
                        local_states[slot_index].observed = 1u;
                    }
                    break;
                case MACHINE_IR_INSTR_STORE_LOCAL:
                case MACHINE_IR_INSTR_STORE_GLOBAL:
                    slot_index = machine_ir_slot_fact_index(program, function, instruction->as.store.slot);
                    if (slot_index != (size_t)-1) {
                        if (!local_states[slot_index].observed) {
                            keep[instruction_index - 1] = 0u;
                            if (out_changed) {
                                *out_changed = 1;
                            }
                        } else {
                            local_states[slot_index].observed = 0u;
                        }
                    }
                    break;
                case MACHINE_IR_INSTR_CALL:
                    for (slot_index = 0; slot_index < slot_count; ++slot_index) {
                        if (local_states[slot_index].slot.kind == MACHINE_IR_SLOT_GLOBAL) {
                            local_states[slot_index].observed = 1u;
                        }
                    }
                    break;
                default:
                    break;
            }
        }

        write_index = 0;
        for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
            if (!keep[instruction_index]) {
                continue;
            }
            if (write_index != instruction_index) {
                block->instructions[write_index] = block->instructions[instruction_index];
            }
            ++write_index;
        }
        block->instruction_count = write_index;

        free(keep);
        free(local_states);
    }

    return 1;
}

static int machine_ir_cleanup_known_slot_values(MachineIrProgram *program,
    int *out_changed,
    MachineIrError *error) {
    size_t function_index;
    MachineIrGlobalCallEffectSummary call_effects;

    if (out_changed) {
        *out_changed = 0;
    }
    if (!program) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-531: invalid known-slot cleanup contract");
        return 0;
    }

    machine_ir_global_call_effect_summary_init(&call_effects);
    if (!machine_ir_compute_global_call_effect_summary(program, &call_effects, error)) {
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        MachineIrFunction *function = &program->functions[function_index];
        size_t slot_count = machine_ir_compute_slot_fact_capacity(program, function);
        size_t resource_count = program->register_bank.register_count + function->spill_slot_count;
        size_t *predecessor_counts = NULL;
        unsigned char *block_exit_alias_valid = NULL;
        MachineIrOperand *block_exit_alias_values = NULL;
        MachineIrKnownSlotValue *block_exit_facts = NULL;
        MachineIrObservedSlotState *block_entry_observed_states = NULL;
        MachineIrObservedSlotState *block_exit_observed_states = NULL;
        MachineIrKnownSlotValue *entry_facts = NULL;
        MachineIrKnownSlotValue *local_facts = NULL;
        unsigned char *entry_alias_valid = NULL;
        MachineIrOperand *entry_alias_values = NULL;
        unsigned char *local_alias_valid = NULL;
        MachineIrOperand *local_alias_values = NULL;
        size_t block_index;

        if (slot_count == 0) {
            continue;
        }

        if (!machine_ir_compute_predecessor_count_map(function, &predecessor_counts, error)) {
            return 0;
        }

        block_exit_alias_valid =
            resource_count > 0 ? (unsigned char *)calloc(function->block_count * resource_count, sizeof(unsigned char)) : NULL;
        block_exit_alias_values =
            resource_count > 0 ? (MachineIrOperand *)calloc(function->block_count * resource_count, sizeof(MachineIrOperand)) : NULL;
        block_exit_facts =
            (MachineIrKnownSlotValue *)calloc(function->block_count * slot_count, sizeof(MachineIrKnownSlotValue));
        block_entry_observed_states =
            (MachineIrObservedSlotState *)calloc(function->block_count * slot_count, sizeof(MachineIrObservedSlotState));
        block_exit_observed_states =
            (MachineIrObservedSlotState *)calloc(function->block_count * slot_count, sizeof(MachineIrObservedSlotState));
        entry_facts = (MachineIrKnownSlotValue *)calloc(slot_count, sizeof(MachineIrKnownSlotValue));
        local_facts = (MachineIrKnownSlotValue *)calloc(slot_count, sizeof(MachineIrKnownSlotValue));
        entry_alias_valid = resource_count > 0 ? (unsigned char *)calloc(resource_count, sizeof(unsigned char)) : NULL;
        entry_alias_values = resource_count > 0 ? (MachineIrOperand *)calloc(resource_count, sizeof(MachineIrOperand)) : NULL;
        local_alias_valid = resource_count > 0 ? (unsigned char *)calloc(resource_count, sizeof(unsigned char)) : NULL;
        local_alias_values = resource_count > 0 ? (MachineIrOperand *)calloc(resource_count, sizeof(MachineIrOperand)) : NULL;
        if ((function->block_count > 0 &&
                ((resource_count > 0 && (!block_exit_alias_valid || !block_exit_alias_values)) ||
                    !block_exit_facts || !block_entry_observed_states || !block_exit_observed_states)) ||
            !entry_facts || !local_facts ||
            (resource_count > 0 && (!entry_alias_valid || !entry_alias_values || !local_alias_valid || !local_alias_values))) {
            free(local_alias_values);
            free(local_alias_valid);
            free(entry_alias_values);
            free(entry_alias_valid);
            free(local_facts);
            free(entry_facts);
            free(block_exit_observed_states);
            free(block_entry_observed_states);
            free(block_exit_facts);
            free(block_exit_alias_values);
            free(block_exit_alias_valid);
            free(predecessor_counts);
            machine_ir_set_error(error, 0, 0, "MACHINE-IR-532: out of memory building known-slot cleanup state");
            return 0;
        }

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            machine_ir_initialize_known_slot_values(
                program, function, slot_count, block_exit_facts + (block_index * slot_count));
        }
        machine_ir_initialize_known_slot_values(program, function, slot_count, entry_facts);
        machine_ir_initialize_known_slot_values(program, function, slot_count, local_facts);

        if (resource_count > 0 &&
            !machine_ir_compute_block_exit_aliases(
                program,
                function,
                resource_count,
                predecessor_counts,
                block_exit_alias_valid,
                block_exit_alias_values,
                error)) {
            free(local_alias_values);
            free(local_alias_valid);
            free(entry_alias_values);
            free(entry_alias_valid);
            free(local_facts);
            free(entry_facts);
            free(block_exit_observed_states);
            free(block_entry_observed_states);
            free(block_exit_facts);
            free(block_exit_alias_values);
            free(block_exit_alias_valid);
            free(predecessor_counts);
            return 0;
        }

        if (!machine_ir_compute_block_exit_known_slot_values(
                program,
                function,
                resource_count,
                slot_count,
                predecessor_counts,
                &call_effects,
                block_exit_alias_valid,
                block_exit_alias_values,
                block_exit_facts,
                error)) {
            free(local_alias_values);
            free(local_alias_valid);
            free(entry_alias_values);
            free(entry_alias_valid);
            free(local_facts);
            free(entry_facts);
            free(block_exit_observed_states);
            free(block_entry_observed_states);
            free(block_exit_facts);
            free(block_exit_alias_values);
            free(block_exit_alias_valid);
            free(predecessor_counts);
            return 0;
        }

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            MachineIrBasicBlock *block = &function->blocks[block_index];
            size_t read_index;
            size_t write_index = 0;

            machine_ir_compute_block_entry_known_slot_values(
                program, function, slot_count, block_index, predecessor_counts, block_exit_facts, entry_facts);
            machine_ir_copy_known_slot_values(slot_count, local_facts, entry_facts);
            if (resource_count > 0) {
                machine_ir_compute_block_entry_aliases(
                    function,
                    block_index,
                    resource_count,
                    predecessor_counts,
                    block_exit_alias_valid,
                    block_exit_alias_values,
                    entry_alias_valid,
                    entry_alias_values);
                memcpy(local_alias_valid, entry_alias_valid, resource_count * sizeof(unsigned char));
                memcpy(local_alias_values, entry_alias_values, resource_count * sizeof(MachineIrOperand));
            }

            for (read_index = 0; read_index < block->instruction_count; ++read_index) {
                MachineIrInstruction *instruction = &block->instructions[read_index];
                size_t result_index;
                size_t slot_index;

                machine_ir_rewrite_instruction_operands_with_known_slot_values(
                    resource_count, program, function, slot_count, local_facts, instruction, out_changed);

                if ((instruction->kind == MACHINE_IR_INSTR_LOAD_LOCAL ||
                        instruction->kind == MACHINE_IR_INSTR_LOAD_GLOBAL) &&
                    instruction->has_result) {
                    slot_index = machine_ir_slot_fact_index(program, function, instruction->as.load_slot);
                    if (slot_index != (size_t)-1 && local_facts[slot_index].known) {
                        instruction->kind = MACHINE_IR_INSTR_MOV;
                        instruction->as.mov_value = resource_count > 0
                            ? machine_ir_resolve_local_copy_alias(
                                  resource_count,
                                  program,
                                  function,
                                  local_alias_valid,
                                  local_alias_values,
                                  local_facts[slot_index].value)
                            : local_facts[slot_index].value;
                        if (out_changed) {
                            *out_changed = 1;
                        }
                    }
                }

                if (instruction->kind == MACHINE_IR_INSTR_STORE_LOCAL ||
                    instruction->kind == MACHINE_IR_INSTR_STORE_GLOBAL) {
                    slot_index = machine_ir_slot_fact_index(program, function, instruction->as.store.slot);
                    if (slot_index != (size_t)-1 &&
                        local_facts[slot_index].known &&
                        machine_ir_operands_equal(local_facts[slot_index].value, instruction->as.store.value)) {
                        if (out_changed) {
                            *out_changed = 1;
                        }
                        continue;
                    }
                }

                if (write_index != read_index) {
                    block->instructions[write_index] = block->instructions[read_index];
                }

                instruction = &block->instructions[write_index];
                machine_ir_invalidate_known_slot_values_for_result(slot_count, local_facts, instruction);
                if (resource_count > 0 &&
                    instruction->has_result &&
                    machine_ir_operand_get_resource_index(program, function, instruction->result, &result_index)) {
                    machine_ir_invalidate_local_copy_aliases(
                        program, function, resource_count, local_alias_valid, local_alias_values, instruction->result);
                }

                if (instruction->kind == MACHINE_IR_INSTR_CALL) {
                    machine_ir_invalidate_known_slot_values_for_caller_clobbers(program, slot_count, local_facts);
                    if (resource_count > 0) {
                        machine_ir_invalidate_caller_clobbered_aliases(
                            program, function, resource_count, local_alias_valid, local_alias_values);
                    }
                    machine_ir_apply_call_global_effects_to_known_slot_values(
                        program, &call_effects, instruction, slot_count, local_facts);
                } else if (resource_count > 0 &&
                    instruction->kind == MACHINE_IR_INSTR_MOV &&
                    instruction->has_result &&
                    machine_ir_operand_get_resource_index(program, function, instruction->result, &result_index) &&
                    !machine_ir_operands_equal(instruction->result, instruction->as.mov_value)) {
                    local_alias_valid[result_index] = 1u;
                    local_alias_values[result_index] =
                        machine_ir_resolve_local_copy_alias(
                            resource_count,
                            program,
                            function,
                            local_alias_valid,
                            local_alias_values,
                            instruction->as.mov_value);
                } else if ((instruction->kind == MACHINE_IR_INSTR_LOAD_LOCAL ||
                        instruction->kind == MACHINE_IR_INSTR_LOAD_GLOBAL) &&
                    instruction->has_result) {
                    slot_index = machine_ir_slot_fact_index(program, function, instruction->as.load_slot);
                    if (slot_index != (size_t)-1) {
                        local_facts[slot_index].known = 1u;
                        local_facts[slot_index].value = instruction->result;
                    }
                } else if (instruction->kind == MACHINE_IR_INSTR_STORE_LOCAL ||
                    instruction->kind == MACHINE_IR_INSTR_STORE_GLOBAL) {
                    slot_index = machine_ir_slot_fact_index(program, function, instruction->as.store.slot);
                    if (slot_index != (size_t)-1) {
                        local_facts[slot_index].known = 1u;
                        local_facts[slot_index].value = instruction->as.store.value;
                    }
                }

                ++write_index;
            }

            block->instruction_count = write_index;
            machine_ir_try_fold_known_slot_return_tails(
                program,
                function,
                resource_count,
                slot_count,
                block_exit_facts,
                block,
                error,
                out_changed);
            machine_ir_try_fold_known_slot_branch_tails(
                program,
                function,
                resource_count,
                slot_count,
                block_exit_facts,
                block,
                error,
                out_changed);
            machine_ir_try_fold_known_slot_return_branch_successors(
                program,
                function,
                resource_count,
                slot_count,
                block_exit_facts,
                block,
                error,
                out_changed);
            machine_ir_try_fold_known_slot_branch_branch_successors(
                program,
                function,
                resource_count,
                slot_count,
                block_exit_facts,
                block,
                error,
                out_changed);
            machine_ir_rewrite_terminator_operands_with_known_slot_values(
                resource_count, program, function, slot_count, local_facts, &block->terminator, out_changed);
        }

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            machine_ir_initialize_observed_slot_states(
                program, function, slot_count, block_entry_observed_states + (block_index * slot_count));
            machine_ir_initialize_observed_slot_states(
                program, function, slot_count, block_exit_observed_states + (block_index * slot_count));
        }

        if (!machine_ir_compute_block_entry_observed_slot_states(
                program,
                function,
                slot_count,
                &call_effects,
                block_entry_observed_states,
                block_exit_observed_states,
                error) ||
            !machine_ir_eliminate_dead_slot_stores(
                program,
                function,
                slot_count,
                block_exit_observed_states,
                out_changed,
                error)) {
            free(local_alias_values);
            free(local_alias_valid);
            free(entry_alias_values);
            free(entry_alias_valid);
            free(local_facts);
            free(entry_facts);
            free(block_exit_observed_states);
            free(block_entry_observed_states);
            free(block_exit_facts);
            free(block_exit_alias_values);
            free(block_exit_alias_valid);
            free(predecessor_counts);
            machine_ir_global_call_effect_summary_free(&call_effects);
            return 0;
        }

        free(local_alias_values);
        free(local_alias_valid);
        free(entry_alias_values);
        free(entry_alias_valid);
        free(local_facts);
        free(entry_facts);
        free(block_exit_observed_states);
        free(block_entry_observed_states);
        free(block_exit_facts);
        free(block_exit_alias_values);
        free(block_exit_alias_valid);
        free(predecessor_counts);
    }

    machine_ir_global_call_effect_summary_free(&call_effects);
    return 1;
}

#endif
