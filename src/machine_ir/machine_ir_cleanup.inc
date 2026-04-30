#if defined(__INTELLISENSE__) && !defined(MACHINE_IR_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_IR_SPLIT_AGGREGATOR)
#define MACHINE_IR_SPLIT_INTELLISENSE_CONTEXT
#include "machine_ir.c"
#else

static int machine_ir_instruction_is_trivial_move(const MachineIrInstruction *instruction) {
    if (!instruction || instruction->kind != MACHINE_IR_INSTR_MOV || !instruction->has_result) {
        return 0;
    }
    return machine_ir_operands_equal(instruction->result, instruction->as.mov_value);
}

static int machine_ir_slots_equal(MachineIrSlotRef lhs, MachineIrSlotRef rhs) {
    return lhs.kind == rhs.kind && lhs.id == rhs.id;
}

static size_t machine_ir_resolve_threaded_jump_target(const MachineIrFunction *function,
    size_t target_block_id);

static int machine_ir_instructions_equal(const MachineIrInstruction *lhs,
    const MachineIrInstruction *rhs) {
    size_t arg_index;

    if (!lhs || !rhs) {
        return 0;
    }
    if (lhs->kind != rhs->kind ||
        lhs->has_result != rhs->has_result ||
        !machine_ir_operands_equal(lhs->result, rhs->result)) {
        return 0;
    }

    switch (lhs->kind) {
        case MACHINE_IR_INSTR_MOV:
            return machine_ir_operands_equal(lhs->as.mov_value, rhs->as.mov_value);
        case MACHINE_IR_INSTR_BINARY:
            return lhs->as.binary.op == rhs->as.binary.op &&
                machine_ir_operands_equal(lhs->as.binary.lhs, rhs->as.binary.lhs) &&
                machine_ir_operands_equal(lhs->as.binary.rhs, rhs->as.binary.rhs);
        case MACHINE_IR_INSTR_CALL:
            if (!lhs->as.call.callee_name || !rhs->as.call.callee_name ||
                strcmp(lhs->as.call.callee_name, rhs->as.call.callee_name) != 0 ||
                lhs->as.call.arg_count != rhs->as.call.arg_count) {
                return 0;
            }
            for (arg_index = 0; arg_index < lhs->as.call.arg_count; ++arg_index) {
                if (!machine_ir_operands_equal(lhs->as.call.args[arg_index], rhs->as.call.args[arg_index])) {
                    return 0;
                }
            }
            return 1;
        case MACHINE_IR_INSTR_LOAD_LOCAL:
        case MACHINE_IR_INSTR_LOAD_GLOBAL:
            return machine_ir_slots_equal(lhs->as.load_slot, rhs->as.load_slot);
        case MACHINE_IR_INSTR_STORE_LOCAL:
        case MACHINE_IR_INSTR_STORE_GLOBAL:
            return machine_ir_slots_equal(lhs->as.store.slot, rhs->as.store.slot) &&
                machine_ir_operands_equal(lhs->as.store.value, rhs->as.store.value);
        default:
            return 0;
    }
}

static int machine_ir_instructions_equivalent_ignoring_result(const MachineIrInstruction *lhs,
    const MachineIrInstruction *rhs) {
    size_t arg_index;

    if (!lhs || !rhs || lhs->kind != rhs->kind || lhs->has_result != rhs->has_result) {
        return 0;
    }

    switch (lhs->kind) {
        case MACHINE_IR_INSTR_MOV:
            return machine_ir_operands_equal(lhs->as.mov_value, rhs->as.mov_value);
        case MACHINE_IR_INSTR_BINARY:
            return lhs->as.binary.op == rhs->as.binary.op &&
                machine_ir_operands_equal(lhs->as.binary.lhs, rhs->as.binary.lhs) &&
                machine_ir_operands_equal(lhs->as.binary.rhs, rhs->as.binary.rhs);
        case MACHINE_IR_INSTR_CALL:
            if (!lhs->as.call.callee_name || !rhs->as.call.callee_name ||
                strcmp(lhs->as.call.callee_name, rhs->as.call.callee_name) != 0 ||
                lhs->as.call.arg_count != rhs->as.call.arg_count) {
                return 0;
            }
            for (arg_index = 0; arg_index < lhs->as.call.arg_count; ++arg_index) {
                if (!machine_ir_operands_equal(lhs->as.call.args[arg_index], rhs->as.call.args[arg_index])) {
                    return 0;
                }
            }
            return 1;
        case MACHINE_IR_INSTR_LOAD_LOCAL:
        case MACHINE_IR_INSTR_LOAD_GLOBAL:
            return machine_ir_slots_equal(lhs->as.load_slot, rhs->as.load_slot);
        case MACHINE_IR_INSTR_STORE_LOCAL:
        case MACHINE_IR_INSTR_STORE_GLOBAL:
            return machine_ir_slots_equal(lhs->as.store.slot, rhs->as.store.slot) &&
                machine_ir_operands_equal(lhs->as.store.value, rhs->as.store.value);
        default:
            return 0;
    }
}

static int machine_ir_terminators_equal(const MachineIrTerminator *lhs,
    const MachineIrTerminator *rhs) {
    if (!lhs || !rhs || lhs->kind != rhs->kind) {
        return 0;
    }

    switch (lhs->kind) {
        case MACHINE_IR_TERM_RETURN:
            return machine_ir_operands_equal(lhs->as.return_value, rhs->as.return_value);
        case MACHINE_IR_TERM_JUMP:
            return lhs->as.jump_target == rhs->as.jump_target;
        case MACHINE_IR_TERM_BRANCH:
            return machine_ir_operands_equal(lhs->as.branch.condition, rhs->as.branch.condition) &&
                lhs->as.branch.then_target == rhs->as.branch.then_target &&
                lhs->as.branch.else_target == rhs->as.branch.else_target;
        default:
            return 0;
    }
}

static int machine_ir_blocks_are_identical_successors(const MachineIrBasicBlock *lhs,
    const MachineIrBasicBlock *rhs) {
    size_t instruction_index;

    if (!lhs || !rhs) {
        return 0;
    }
    if (lhs->phi_count != 0 || rhs->phi_count != 0 ||
        lhs->instruction_count != rhs->instruction_count ||
        lhs->has_terminator != rhs->has_terminator) {
        return 0;
    }

    for (instruction_index = 0; instruction_index < lhs->instruction_count; ++instruction_index) {
        if (!machine_ir_instructions_equal(&lhs->instructions[instruction_index], &rhs->instructions[instruction_index])) {
            return 0;
        }
    }

    if (!lhs->has_terminator) {
        return 1;
    }
    return machine_ir_terminators_equal(&lhs->terminator, &rhs->terminator);
}

static int machine_ir_simplify_trivial_branches(MachineIrProgram *program,
    int *out_changed,
    MachineIrError *error) {
    size_t function_index;

    if (out_changed) {
        *out_changed = 0;
    }
    if (!program) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-521: invalid branch-simplify contract");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        MachineIrFunction *function = &program->functions[function_index];
        size_t block_index;

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            MachineIrBasicBlock *block = &function->blocks[block_index];

            if (!block->has_terminator || block->terminator.kind != MACHINE_IR_TERM_BRANCH) {
                continue;
            }
            if (block->terminator.as.branch.then_target == block->terminator.as.branch.else_target) {
                size_t jump_target = block->terminator.as.branch.then_target;

                memset(&block->terminator, 0, sizeof(block->terminator));
                block->terminator.kind = MACHINE_IR_TERM_JUMP;
                block->terminator.as.jump_target = jump_target;
                if (out_changed) {
                    *out_changed = 1;
                }
                continue;
            }
            if (block->terminator.as.branch.condition.kind == MACHINE_IR_OPERAND_IMMEDIATE) {
                size_t jump_target = block->terminator.as.branch.condition.immediate != 0
                    ? block->terminator.as.branch.then_target
                    : block->terminator.as.branch.else_target;

                memset(&block->terminator, 0, sizeof(block->terminator));
                block->terminator.kind = MACHINE_IR_TERM_JUMP;
                block->terminator.as.jump_target = jump_target;
                if (out_changed) {
                    *out_changed = 1;
                }
            }
        }
    }

    return 1;
}

static int machine_ir_fold_identical_branch_successors(MachineIrFunction *function,
    int *out_changed,
    MachineIrError *error) {
    size_t block_index;

    if (out_changed) {
        *out_changed = 0;
    }
    if (!function) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-548: invalid identical-successor cleanup contract");
        return 0;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        MachineIrBasicBlock *block = &function->blocks[block_index];
        size_t then_target_id;
        size_t else_target_id;
        const MachineIrBasicBlock *then_target;
        const MachineIrBasicBlock *else_target;

        if (!block->has_terminator || block->terminator.kind != MACHINE_IR_TERM_BRANCH) {
            continue;
        }

        then_target_id = block->terminator.as.branch.then_target;
        else_target_id = block->terminator.as.branch.else_target;
        if (then_target_id >= function->block_count || else_target_id >= function->block_count ||
            then_target_id == else_target_id) {
            continue;
        }
        then_target_id = machine_ir_resolve_threaded_jump_target(function, then_target_id);
        else_target_id = machine_ir_resolve_threaded_jump_target(function, else_target_id);
        if (then_target_id == else_target_id) {
            continue;
        }

        then_target = &function->blocks[then_target_id];
        else_target = &function->blocks[else_target_id];
        if (!machine_ir_blocks_are_identical_successors(then_target, else_target)) {
            continue;
        }

        block->terminator.kind = MACHINE_IR_TERM_JUMP;
        block->terminator.as.jump_target = then_target_id;
        if (out_changed) {
            *out_changed = 1;
        }
    }

    return 1;
}

static size_t machine_ir_resolve_threaded_jump_target(const MachineIrFunction *function,
    size_t target_block_id) {
    size_t current = target_block_id;
    size_t hop_count = 0;

    if (!function || current >= function->block_count) {
        return target_block_id;
    }

    while (hop_count < function->block_count) {
        const MachineIrBasicBlock *block = &function->blocks[current];

        if (block->id == 0 ||
            block->phi_count != 0 ||
            block->instruction_count != 0 ||
            !block->has_terminator ||
            block->terminator.kind != MACHINE_IR_TERM_JUMP ||
            block->terminator.as.jump_target == current) {
            break;
        }
        current = block->terminator.as.jump_target;
        ++hop_count;
    }

    return current;
}

static int machine_ir_thread_jump_targets(MachineIrFunction *function,
    int *out_changed,
    MachineIrError *error) {
    size_t block_index;

    if (out_changed) {
        *out_changed = 0;
    }
    if (!function) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-527: invalid jump-thread cleanup contract");
        return 0;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        MachineIrBasicBlock *block = &function->blocks[block_index];

        if (!block->has_terminator) {
            continue;
        }

        switch (block->terminator.kind) {
            case MACHINE_IR_TERM_JUMP: {
                size_t old_target = block->terminator.as.jump_target;
                size_t new_target = machine_ir_resolve_threaded_jump_target(function, old_target);

                if (new_target != old_target) {
                    block->terminator.as.jump_target = new_target;
                    if (out_changed) {
                        *out_changed = 1;
                    }
                }
                break;
            }
            case MACHINE_IR_TERM_BRANCH: {
                size_t old_then_target = block->terminator.as.branch.then_target;
                size_t old_else_target = block->terminator.as.branch.else_target;
                size_t new_then_target =
                    machine_ir_resolve_threaded_jump_target(function, old_then_target);
                size_t new_else_target =
                    machine_ir_resolve_threaded_jump_target(function, old_else_target);

                if (new_then_target != old_then_target) {
                    block->terminator.as.branch.then_target = new_then_target;
                    if (out_changed) {
                        *out_changed = 1;
                    }
                }
                if (new_else_target != old_else_target) {
                    block->terminator.as.branch.else_target = new_else_target;
                    if (out_changed) {
                        *out_changed = 1;
                    }
                }
                break;
            }
            default:
                break;
        }
    }

    return 1;
}

static int machine_ir_fold_return_tails(MachineIrFunction *function,
    int *out_changed,
    MachineIrError *error) {
    size_t block_index;

    if (out_changed) {
        *out_changed = 0;
    }
    if (!function) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-545: invalid return-tail cleanup contract");
        return 0;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        MachineIrBasicBlock *block = &function->blocks[block_index];

        if (!block->has_terminator) {
            continue;
        }

        if (block->terminator.kind == MACHINE_IR_TERM_JUMP) {
            size_t target_block_id = block->terminator.as.jump_target;
            const MachineIrBasicBlock *target;

            if (target_block_id >= function->block_count) {
                continue;
            }
            target = &function->blocks[target_block_id];
            if (target->phi_count != 0 ||
                target->instruction_count != 0 ||
                !target->has_terminator ||
                target->terminator.kind != MACHINE_IR_TERM_RETURN) {
                continue;
            }

            block->terminator.kind = MACHINE_IR_TERM_RETURN;
            block->terminator.as.return_value = target->terminator.as.return_value;
            if (out_changed) {
                *out_changed = 1;
            }
            continue;
        }

        if (block->terminator.kind == MACHINE_IR_TERM_BRANCH) {
            size_t then_target_id = block->terminator.as.branch.then_target;
            size_t else_target_id = block->terminator.as.branch.else_target;
            const MachineIrBasicBlock *then_target;
            const MachineIrBasicBlock *else_target;

            if (then_target_id >= function->block_count || else_target_id >= function->block_count) {
                continue;
            }
            then_target = &function->blocks[then_target_id];
            else_target = &function->blocks[else_target_id];
            if (then_target->phi_count != 0 || else_target->phi_count != 0 ||
                then_target->instruction_count != 0 || else_target->instruction_count != 0 ||
                !then_target->has_terminator || !else_target->has_terminator ||
                then_target->terminator.kind != MACHINE_IR_TERM_RETURN ||
                else_target->terminator.kind != MACHINE_IR_TERM_RETURN) {
                continue;
            }
            if (!machine_ir_operands_equal(
                    then_target->terminator.as.return_value,
                    else_target->terminator.as.return_value)) {
                continue;
            }

            block->terminator.kind = MACHINE_IR_TERM_RETURN;
            block->terminator.as.return_value = then_target->terminator.as.return_value;
            if (out_changed) {
                *out_changed = 1;
            }
        }
    }

    return 1;
}

static int machine_ir_block_is_thin_return_wrapper(const MachineIrBasicBlock *block,
    MachineIrOperand *out_wrapped_value) {
    const MachineIrInstruction *instruction;

    if (out_wrapped_value) {
        *out_wrapped_value = machine_ir_operand_none();
    }
    if (!block ||
        block->phi_count != 0 ||
        block->instruction_count != 1 ||
        !block->has_terminator ||
        block->terminator.kind != MACHINE_IR_TERM_RETURN) {
        return 0;
    }

    instruction = &block->instructions[0];
    if (instruction->kind != MACHINE_IR_INSTR_MOV || !instruction->has_result) {
        return 0;
    }
    if (!machine_ir_operands_equal(block->terminator.as.return_value, instruction->result)) {
        return 0;
    }
    if (out_wrapped_value) {
        *out_wrapped_value = instruction->as.mov_value;
    }
    return 1;
}

static int machine_ir_instruction_is_inlineable_pure_value_producer(const MachineIrInstruction *instruction) {
    if (!instruction || !instruction->has_result) {
        return 0;
    }

    switch (instruction->kind) {
        case MACHINE_IR_INSTR_MOV:
        case MACHINE_IR_INSTR_LOAD_LOCAL:
        case MACHINE_IR_INSTR_LOAD_GLOBAL:
            return 1;
        case MACHINE_IR_INSTR_BINARY:
            switch (instruction->as.binary.op) {
                case MACHINE_IR_BINARY_DIV:
                case MACHINE_IR_BINARY_MOD:
                case MACHINE_IR_BINARY_SHIFT_LEFT:
                case MACHINE_IR_BINARY_SHIFT_RIGHT:
                    return 0;
                default:
                    return 1;
            }
        default:
            return 0;
    }
}

static int machine_ir_block_is_inlineable_return_wrapper(const MachineIrBasicBlock *block,
    MachineIrInstruction *out_instruction) {
    const MachineIrInstruction *instruction;

    if (out_instruction) {
        memset(out_instruction, 0, sizeof(*out_instruction));
    }
    if (!block ||
        block->phi_count != 0 ||
        block->instruction_count != 1 ||
        !block->has_terminator ||
        block->terminator.kind != MACHINE_IR_TERM_RETURN) {
        return 0;
    }

    instruction = &block->instructions[0];
    if (!machine_ir_instruction_is_inlineable_pure_value_producer(instruction) ||
        !machine_ir_operands_equal(block->terminator.as.return_value, instruction->result)) {
        return 0;
    }

    if (out_instruction) {
        *out_instruction = *instruction;
        if (out_instruction->kind == MACHINE_IR_INSTR_CALL) {
            out_instruction->as.call.callee_name = NULL;
            out_instruction->as.call.args = NULL;
        }
    }
    return 1;
}

static int machine_ir_block_is_single_call_return_wrapper(const MachineIrBasicBlock *block,
    MachineIrInstruction *out_instruction) {
    const MachineIrInstruction *instruction;

    if (out_instruction) {
        memset(out_instruction, 0, sizeof(*out_instruction));
    }
    if (!block ||
        block->phi_count != 0 ||
        block->instruction_count != 1 ||
        !block->has_terminator ||
        block->terminator.kind != MACHINE_IR_TERM_RETURN) {
        return 0;
    }

    instruction = &block->instructions[0];
    if (instruction->kind != MACHINE_IR_INSTR_CALL ||
        !instruction->has_result ||
        !machine_ir_operands_equal(block->terminator.as.return_value, instruction->result)) {
        return 0;
    }

    if (out_instruction) {
        *out_instruction = *instruction;
    }
    return 1;
}

static int machine_ir_inlineable_return_wrapper_instructions_equivalent(
    const MachineIrProgram *program,
    const MachineIrFunction *function,
    const MachineIrInstruction *lhs,
    const MachineIrTerminator *lhs_terminator,
    const MachineIrInstruction *rhs,
    const MachineIrTerminator *rhs_terminator) {
    if (!program || !function || !lhs || !rhs || !lhs_terminator || !rhs_terminator) {
        return 0;
    }
    if (!lhs->has_result || !rhs->has_result ||
        lhs_terminator->kind != MACHINE_IR_TERM_RETURN ||
        rhs_terminator->kind != MACHINE_IR_TERM_RETURN) {
        return 0;
    }
    if (!machine_ir_operands_equal(lhs_terminator->as.return_value, lhs->result) ||
        !machine_ir_operands_equal(rhs_terminator->as.return_value, rhs->result)) {
        return 0;
    }
    if (!machine_ir_result_operands_have_compatible_constraints(program, function, lhs->result, rhs->result)) {
        return 0;
    }
    if (lhs->kind != rhs->kind) {
        return 0;
    }

    switch (lhs->kind) {
        case MACHINE_IR_INSTR_MOV:
            return machine_ir_operands_equal(lhs->as.mov_value, rhs->as.mov_value);
        case MACHINE_IR_INSTR_LOAD_LOCAL:
        case MACHINE_IR_INSTR_LOAD_GLOBAL:
            return machine_ir_slots_equal(lhs->as.load_slot, rhs->as.load_slot);
        case MACHINE_IR_INSTR_BINARY:
            return lhs->as.binary.op == rhs->as.binary.op &&
                machine_ir_operands_equal(lhs->as.binary.lhs, rhs->as.binary.lhs) &&
                machine_ir_operands_equal(lhs->as.binary.rhs, rhs->as.binary.rhs);
        default:
            return 0;
    }
}

static int machine_ir_block_is_safe_single_return_wrapper(const MachineIrBasicBlock *block,
    MachineIrOperand *out_wrapped_value) {
    if (!machine_ir_block_is_thin_return_wrapper(block, out_wrapped_value)) {
        return 0;
    }
    if (block->instructions[0].kind != MACHINE_IR_INSTR_MOV) {
        return 0;
    }
    return 1;
}

static int machine_ir_block_is_inlineable_branch_wrapper(const MachineIrBasicBlock *block,
    MachineIrInstruction *out_instruction,
    MachineIrTerminator *out_branch_terminator) {
    const MachineIrInstruction *instruction;

    if (out_instruction) {
        memset(out_instruction, 0, sizeof(*out_instruction));
    }
    if (out_branch_terminator) {
        memset(out_branch_terminator, 0, sizeof(*out_branch_terminator));
    }
    if (!block ||
        block->phi_count != 0 ||
        block->instruction_count != 1 ||
        !block->has_terminator ||
        block->terminator.kind != MACHINE_IR_TERM_BRANCH) {
        return 0;
    }

    instruction = &block->instructions[0];
    if (!machine_ir_instruction_is_inlineable_pure_value_producer(instruction) ||
        !machine_ir_operands_equal(block->terminator.as.branch.condition, instruction->result)) {
        return 0;
    }

    if (out_instruction) {
        *out_instruction = *instruction;
    }
    if (out_branch_terminator) {
        *out_branch_terminator = block->terminator;
    }
    return 1;
}

static int machine_ir_block_is_single_call_branch_wrapper(const MachineIrBasicBlock *block,
    MachineIrInstruction *out_instruction,
    MachineIrTerminator *out_branch_terminator) {
    const MachineIrInstruction *instruction;

    if (out_instruction) {
        memset(out_instruction, 0, sizeof(*out_instruction));
    }
    if (out_branch_terminator) {
        memset(out_branch_terminator, 0, sizeof(*out_branch_terminator));
    }
    if (!block ||
        block->phi_count != 0 ||
        block->instruction_count != 1 ||
        !block->has_terminator ||
        block->terminator.kind != MACHINE_IR_TERM_BRANCH) {
        return 0;
    }

    instruction = &block->instructions[0];
    if (instruction->kind != MACHINE_IR_INSTR_CALL ||
        !instruction->has_result ||
        !machine_ir_operands_equal(block->terminator.as.branch.condition, instruction->result)) {
        return 0;
    }

    if (out_instruction) {
        *out_instruction = *instruction;
    }
    if (out_branch_terminator) {
        *out_branch_terminator = block->terminator;
    }
    return 1;
}

static int machine_ir_block_is_thin_jump_wrapper(const MachineIrBasicBlock *block,
    MachineIrInstruction *out_wrapper_instruction,
    size_t *out_jump_target) {
    if (out_wrapper_instruction) {
        memset(out_wrapper_instruction, 0, sizeof(*out_wrapper_instruction));
    }
    if (out_jump_target) {
        *out_jump_target = (size_t)-1;
    }
    if (!block ||
        block->phi_count != 0 ||
        block->instruction_count != 1 ||
        !block->has_terminator ||
        block->terminator.kind != MACHINE_IR_TERM_JUMP) {
        return 0;
    }
    if (block->instructions[0].kind != MACHINE_IR_INSTR_MOV || !block->instructions[0].has_result) {
        return 0;
    }
    if (out_wrapper_instruction) {
        *out_wrapper_instruction = block->instructions[0];
        if (out_wrapper_instruction->kind == MACHINE_IR_INSTR_CALL) {
            out_wrapper_instruction->as.call.callee_name = NULL;
            out_wrapper_instruction->as.call.args = NULL;
        }
    }
    if (out_jump_target) {
        *out_jump_target = block->terminator.as.jump_target;
    }
    return 1;
}

static int machine_ir_blocks_share_same_single_instruction_wrapper(
    const MachineIrProgram *program,
    const MachineIrFunction *function,
    const MachineIrBasicBlock *lhs,
    const MachineIrBasicBlock *rhs,
    MachineIrInstruction *out_shared_instruction,
    MachineIrTerminator *out_shared_terminator) {
    if (out_shared_instruction) {
        memset(out_shared_instruction, 0, sizeof(*out_shared_instruction));
    }
    if (out_shared_terminator) {
        memset(out_shared_terminator, 0, sizeof(*out_shared_terminator));
    }
    if (!program || !function || !lhs || !rhs ||
        lhs->phi_count != 0 || rhs->phi_count != 0 ||
        lhs->instruction_count != 1 || rhs->instruction_count != 1 ||
        !lhs->has_terminator || !rhs->has_terminator) {
        return 0;
    }
    if (!machine_ir_instructions_equal(&lhs->instructions[0], &rhs->instructions[0]) ||
        !machine_ir_terminators_equal(&lhs->terminator, &rhs->terminator)) {
        if (!machine_ir_instructions_equivalent_ignoring_result(&lhs->instructions[0], &rhs->instructions[0]) ||
            !lhs->instructions[0].has_result ||
            !rhs->instructions[0].has_result ||
            !machine_ir_result_operands_have_compatible_constraints(
                program, function, lhs->instructions[0].result, rhs->instructions[0].result)) {
            return 0;
        }

        if (lhs->terminator.kind == MACHINE_IR_TERM_RETURN &&
            rhs->terminator.kind == MACHINE_IR_TERM_RETURN &&
            machine_ir_operands_equal(lhs->terminator.as.return_value, lhs->instructions[0].result) &&
            machine_ir_operands_equal(rhs->terminator.as.return_value, rhs->instructions[0].result)) {
            /* equivalent result-returning wrappers may canonicalize to the lhs placement */
        } else if (lhs->terminator.kind == MACHINE_IR_TERM_BRANCH &&
            rhs->terminator.kind == MACHINE_IR_TERM_BRANCH &&
            lhs->terminator.as.branch.then_target == rhs->terminator.as.branch.then_target &&
            lhs->terminator.as.branch.else_target == rhs->terminator.as.branch.else_target &&
            machine_ir_operands_equal(lhs->terminator.as.branch.condition, lhs->instructions[0].result) &&
            machine_ir_operands_equal(rhs->terminator.as.branch.condition, rhs->instructions[0].result)) {
            /* equivalent result-branching wrappers may canonicalize to the lhs placement */
        } else {
            return 0;
        }
    }
    if (out_shared_instruction) {
        *out_shared_instruction = lhs->instructions[0];
    }
    if (out_shared_terminator) {
        *out_shared_terminator = lhs->terminator;
    }
    return 1;
}

static int machine_ir_fold_thin_return_wrapper_tails(MachineIrFunction *function,
    int *out_changed,
    MachineIrError *error) {
    size_t block_index;

    if (out_changed) {
        *out_changed = 0;
    }
    if (!function) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-546: invalid thin-return-tail cleanup contract");
        return 0;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        MachineIrBasicBlock *block = &function->blocks[block_index];

        if (!block->has_terminator) {
            continue;
        }

        if (block->terminator.kind == MACHINE_IR_TERM_JUMP) {
            size_t target_block_id = block->terminator.as.jump_target;
            const MachineIrBasicBlock *target;
            MachineIrOperand wrapped_value;

            if (target_block_id >= function->block_count) {
                continue;
            }
            target_block_id = machine_ir_resolve_threaded_jump_target(function, target_block_id);
            target = &function->blocks[target_block_id];
            if (!machine_ir_block_is_safe_single_return_wrapper(target, &wrapped_value)) {
                continue;
            }

            block->terminator.kind = MACHINE_IR_TERM_RETURN;
            block->terminator.as.return_value = wrapped_value;
            if (out_changed) {
                *out_changed = 1;
            }
            continue;
        }

        if (block->terminator.kind == MACHINE_IR_TERM_BRANCH) {
            size_t then_target_id = block->terminator.as.branch.then_target;
            size_t else_target_id = block->terminator.as.branch.else_target;
            const MachineIrBasicBlock *then_target;
            const MachineIrBasicBlock *else_target;
            MachineIrOperand then_wrapped_value;
            MachineIrOperand else_wrapped_value;

            if (then_target_id >= function->block_count || else_target_id >= function->block_count) {
                continue;
            }
            then_target = &function->blocks[then_target_id];
            else_target = &function->blocks[else_target_id];
            if (!machine_ir_block_is_safe_single_return_wrapper(then_target, &then_wrapped_value) ||
                !machine_ir_block_is_safe_single_return_wrapper(else_target, &else_wrapped_value)) {
                continue;
            }
            if (!machine_ir_operands_equal(then_wrapped_value, else_wrapped_value)) {
                continue;
            }

            block->terminator.kind = MACHINE_IR_TERM_RETURN;
            block->terminator.as.return_value = then_wrapped_value;
            if (out_changed) {
                *out_changed = 1;
            }
        }
    }

    return 1;
}

static int machine_ir_fold_inlineable_return_wrapper_tails(MachineIrFunction *function,
    int *out_changed,
    MachineIrError *error) {
    size_t block_index;

    if (out_changed) {
        *out_changed = 0;
    }
    if (!function) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-551: invalid inlineable-return-tail cleanup contract");
        return 0;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        MachineIrBasicBlock *block = &function->blocks[block_index];

        if (!block->has_terminator) {
            continue;
        }

        if (block->terminator.kind == MACHINE_IR_TERM_JUMP) {
            size_t target_block_id = block->terminator.as.jump_target;
            const MachineIrBasicBlock *target;
            MachineIrInstruction wrapper_instruction;
            int saved_has_terminator;
            MachineIrTerminator saved_terminator;

            if (target_block_id >= function->block_count) {
                continue;
            }
            target_block_id = machine_ir_resolve_threaded_jump_target(function, target_block_id);
            target = &function->blocks[target_block_id];
            if (!machine_ir_block_is_inlineable_return_wrapper(target, &wrapper_instruction)) {
                continue;
            }

            saved_has_terminator = block->has_terminator;
            saved_terminator = block->terminator;
            block->has_terminator = 0;
            if (!machine_ir_block_append_instruction(block, &wrapper_instruction, error)) {
                block->terminator = saved_terminator;
                block->has_terminator = saved_has_terminator;
                return 0;
            }
            block->terminator.kind = MACHINE_IR_TERM_RETURN;
            block->terminator.as.return_value = wrapper_instruction.result;
            block->has_terminator = 1;
            if (out_changed) {
                *out_changed = 1;
            }
            continue;
        }

        if (block->terminator.kind == MACHINE_IR_TERM_BRANCH) {
            size_t then_target_id = block->terminator.as.branch.then_target;
            size_t else_target_id = block->terminator.as.branch.else_target;
            const MachineIrBasicBlock *then_target;
            const MachineIrBasicBlock *else_target;
            MachineIrInstruction then_instruction;
            MachineIrInstruction else_instruction;

            if (then_target_id >= function->block_count || else_target_id >= function->block_count) {
                continue;
            }
            then_target = &function->blocks[then_target_id];
            else_target = &function->blocks[else_target_id];
            if (!machine_ir_block_is_inlineable_return_wrapper(then_target, &then_instruction) ||
                !machine_ir_block_is_inlineable_return_wrapper(else_target, &else_instruction) ||
                !machine_ir_instructions_equal(&then_instruction, &else_instruction)) {
                continue;
            }

            block->has_terminator = 0;
            if (!machine_ir_block_append_instruction(block, &then_instruction, error)) {
                block->has_terminator = 1;
                return 0;
            }
            block->terminator.kind = MACHINE_IR_TERM_RETURN;
            block->terminator.as.return_value = then_instruction.result;
            block->has_terminator = 1;
            if (out_changed) {
                *out_changed = 1;
            }
        }
    }

    return 1;
}

static int machine_ir_fold_call_return_wrapper_tails(MachineIrFunction *function,
    int *out_changed,
    MachineIrError *error) {
    size_t block_index;

    if (out_changed) {
        *out_changed = 0;
    }
    if (!function) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-556: invalid call-return-tail cleanup contract");
        return 0;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        MachineIrBasicBlock *block = &function->blocks[block_index];
        size_t target_block_id;
        const MachineIrBasicBlock *target;
        MachineIrInstruction wrapper_instruction;
        int saved_has_terminator;
        MachineIrTerminator saved_terminator;

        if (!block->has_terminator || block->terminator.kind != MACHINE_IR_TERM_JUMP) {
            continue;
        }

        target_block_id = block->terminator.as.jump_target;
        if (target_block_id >= function->block_count) {
            continue;
        }
        target_block_id = machine_ir_resolve_threaded_jump_target(function, target_block_id);
        target = &function->blocks[target_block_id];
        if (!machine_ir_block_is_single_call_return_wrapper(target, &wrapper_instruction)) {
            continue;
        }

        saved_has_terminator = block->has_terminator;
        saved_terminator = block->terminator;
        block->has_terminator = 0;
        if (!machine_ir_block_append_instruction(block, &wrapper_instruction, error)) {
            block->terminator = saved_terminator;
            block->has_terminator = saved_has_terminator;
            return 0;
        }
        block->terminator.kind = MACHINE_IR_TERM_RETURN;
        block->terminator.as.return_value = wrapper_instruction.result;
        block->has_terminator = 1;
        if (out_changed) {
            *out_changed = 1;
        }
    }

    return 1;
}

static int machine_ir_fold_thin_jump_wrapper_tails(MachineIrFunction *function,
    int *out_changed,
    MachineIrError *error) {
    size_t block_index;

    if (out_changed) {
        *out_changed = 0;
    }
    if (!function) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-547: invalid thin-jump-wrapper cleanup contract");
        return 0;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        MachineIrBasicBlock *block = &function->blocks[block_index];

        if (!block->has_terminator || block->terminator.kind != MACHINE_IR_TERM_JUMP) {
            continue;
        }

        {
            size_t target_block_id = block->terminator.as.jump_target;
            const MachineIrBasicBlock *target;
            MachineIrInstruction wrapper_instruction;
            size_t wrapper_jump_target;
            int saved_has_terminator;
            MachineIrTerminator saved_terminator;

            if (target_block_id >= function->block_count) {
                continue;
            }
            target = &function->blocks[target_block_id];
            if (!machine_ir_block_is_thin_jump_wrapper(target, &wrapper_instruction, &wrapper_jump_target)) {
                continue;
            }

            saved_has_terminator = block->has_terminator;
            saved_terminator = block->terminator;
            block->has_terminator = 0;
            if (!machine_ir_block_append_instruction(block, &wrapper_instruction, error)) {
                block->terminator = saved_terminator;
                block->has_terminator = saved_has_terminator;
                return 0;
            }
            block->terminator = saved_terminator;
            block->has_terminator = saved_has_terminator;
            block->terminator.as.jump_target = wrapper_jump_target;
            if (out_changed) {
                *out_changed = 1;
            }
        }
    }

    return 1;
}

static int machine_ir_fold_same_single_instruction_branch_successors(const MachineIrProgram *program,
    MachineIrFunction *function,
    int *out_changed,
    MachineIrError *error) {
    size_t block_index;

    if (out_changed) {
        *out_changed = 0;
    }
    if (!program || !function) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-549: invalid same-wrapper-branch cleanup contract");
        return 0;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        MachineIrBasicBlock *block = &function->blocks[block_index];
        size_t then_target_id;
        size_t else_target_id;
        const MachineIrBasicBlock *then_target;
        const MachineIrBasicBlock *else_target;
        MachineIrInstruction shared_instruction;
        MachineIrTerminator shared_terminator;

        if (!block->has_terminator || block->terminator.kind != MACHINE_IR_TERM_BRANCH) {
            continue;
        }

        then_target_id = block->terminator.as.branch.then_target;
        else_target_id = block->terminator.as.branch.else_target;
        if (then_target_id >= function->block_count ||
            else_target_id >= function->block_count ||
            then_target_id == else_target_id) {
            continue;
        }
        then_target_id = machine_ir_resolve_threaded_jump_target(function, then_target_id);
        else_target_id = machine_ir_resolve_threaded_jump_target(function, else_target_id);
        if (then_target_id == else_target_id) {
            continue;
        }

        then_target = &function->blocks[then_target_id];
        else_target = &function->blocks[else_target_id];
        if (!machine_ir_blocks_share_same_single_instruction_wrapper(
                program,
                function,
                then_target,
                else_target,
                &shared_instruction,
                &shared_terminator)) {
            continue;
        }

        block->has_terminator = 0;
        if (!machine_ir_block_append_instruction(block, &shared_instruction, error)) {
            block->has_terminator = 1;
            return 0;
        }
        block->terminator = shared_terminator;
        block->has_terminator = 1;
        if (out_changed) {
            *out_changed = 1;
        }
    }

    return 1;
}

static int machine_ir_fold_empty_branch_wrapper_tails(MachineIrFunction *function,
    int *out_changed,
    MachineIrError *error) {
    size_t block_index;

    if (out_changed) {
        *out_changed = 0;
    }
    if (!function) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-553: invalid empty-branch-wrapper cleanup contract");
        return 0;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        MachineIrBasicBlock *block = &function->blocks[block_index];
        size_t target_block_id;
        const MachineIrBasicBlock *target;

        if (!block->has_terminator || block->terminator.kind != MACHINE_IR_TERM_JUMP) {
            continue;
        }

        target_block_id = block->terminator.as.jump_target;
        if (target_block_id >= function->block_count) {
            continue;
        }
        target = &function->blocks[target_block_id];
        if (target->phi_count != 0 ||
            target->instruction_count != 0 ||
            !target->has_terminator ||
            target->terminator.kind != MACHINE_IR_TERM_BRANCH) {
            continue;
        }

        block->terminator = target->terminator;
        if (out_changed) {
            *out_changed = 1;
        }
    }

    return 1;
}

static int machine_ir_fold_equivalent_inlineable_return_branch_successors(const MachineIrProgram *program,
    MachineIrFunction *function,
    int *out_changed,
    MachineIrError *error) {
    size_t block_index;

    if (out_changed) {
        *out_changed = 0;
    }
    if (!program || !function) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-552: invalid equivalent-inlineable-return cleanup contract");
        return 0;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        MachineIrBasicBlock *block = &function->blocks[block_index];
        size_t then_target_id;
        size_t else_target_id;
        const MachineIrBasicBlock *then_target;
        const MachineIrBasicBlock *else_target;
        MachineIrInstruction then_instruction;

        if (!block->has_terminator || block->terminator.kind != MACHINE_IR_TERM_BRANCH) {
            continue;
        }

        then_target_id = block->terminator.as.branch.then_target;
        else_target_id = block->terminator.as.branch.else_target;
        if (then_target_id >= function->block_count ||
            else_target_id >= function->block_count ||
            then_target_id == else_target_id) {
            continue;
        }
        then_target_id = machine_ir_resolve_threaded_jump_target(function, then_target_id);
        else_target_id = machine_ir_resolve_threaded_jump_target(function, else_target_id);
        if (then_target_id == else_target_id) {
            continue;
        }

        then_target = &function->blocks[then_target_id];
        else_target = &function->blocks[else_target_id];
        if (!machine_ir_block_is_inlineable_return_wrapper(then_target, &then_instruction) ||
            !machine_ir_block_is_inlineable_return_wrapper(else_target, NULL) ||
            !machine_ir_inlineable_return_wrapper_instructions_equivalent(
                program,
                function,
                &then_target->instructions[0],
                &then_target->terminator,
                &else_target->instructions[0],
                &else_target->terminator)) {
            continue;
        }

        block->has_terminator = 0;
        if (!machine_ir_block_append_instruction(block, &then_instruction, error)) {
            block->has_terminator = 1;
            return 0;
        }
        block->terminator.kind = MACHINE_IR_TERM_RETURN;
        block->terminator.as.return_value = then_instruction.result;
        block->has_terminator = 1;
        if (out_changed) {
            *out_changed = 1;
        }
    }

    return 1;
}

static int machine_ir_equivalent_inlineable_branch_wrappers(
    const MachineIrProgram *program,
    const MachineIrFunction *function,
    const MachineIrBasicBlock *lhs,
    const MachineIrBasicBlock *rhs,
    MachineIrInstruction *out_instruction,
    MachineIrTerminator *out_branch_terminator) {
    if (!program || !function || !lhs || !rhs || !out_instruction || !out_branch_terminator) {
        return 0;
    }
    if (!machine_ir_block_is_inlineable_branch_wrapper(lhs, out_instruction, out_branch_terminator) ||
        !machine_ir_block_is_inlineable_branch_wrapper(rhs, NULL, NULL)) {
        return 0;
    }
    return machine_ir_inlineable_return_wrapper_instructions_equivalent(
        program,
        function,
        &lhs->instructions[0],
        (const MachineIrTerminator *)&(MachineIrTerminator){
            .kind = MACHINE_IR_TERM_RETURN,
            .as.return_value = lhs->terminator.as.branch.condition
        },
        &rhs->instructions[0],
        (const MachineIrTerminator *)&(MachineIrTerminator){
            .kind = MACHINE_IR_TERM_RETURN,
            .as.return_value = rhs->terminator.as.branch.condition
        }) &&
        lhs->terminator.as.branch.then_target == rhs->terminator.as.branch.then_target &&
        lhs->terminator.as.branch.else_target == rhs->terminator.as.branch.else_target;
}

static int machine_ir_fold_equivalent_inlineable_branch_successors(const MachineIrProgram *program,
    MachineIrFunction *function,
    int *out_changed,
    MachineIrError *error) {
    size_t block_index;

    if (out_changed) {
        *out_changed = 0;
    }
    if (!program || !function) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-555: invalid equivalent-inlineable-branch cleanup contract");
        return 0;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        MachineIrBasicBlock *block = &function->blocks[block_index];
        size_t then_target_id;
        size_t else_target_id;
        const MachineIrBasicBlock *then_target;
        const MachineIrBasicBlock *else_target;
        MachineIrInstruction shared_instruction;
        MachineIrTerminator shared_branch;

        if (!block->has_terminator || block->terminator.kind != MACHINE_IR_TERM_BRANCH) {
            continue;
        }

        then_target_id = block->terminator.as.branch.then_target;
        else_target_id = block->terminator.as.branch.else_target;
        if (then_target_id >= function->block_count ||
            else_target_id >= function->block_count ||
            then_target_id == else_target_id) {
            continue;
        }
        then_target_id = machine_ir_resolve_threaded_jump_target(function, then_target_id);
        else_target_id = machine_ir_resolve_threaded_jump_target(function, else_target_id);
        if (then_target_id == else_target_id) {
            continue;
        }

        then_target = &function->blocks[then_target_id];
        else_target = &function->blocks[else_target_id];
        if (!machine_ir_equivalent_inlineable_branch_wrappers(
                program,
                function,
                then_target, else_target, &shared_instruction, &shared_branch)) {
            continue;
        }

        block->has_terminator = 0;
        if (!machine_ir_block_append_instruction(block, &shared_instruction, error)) {
            block->has_terminator = 1;
            return 0;
        }
        block->terminator = shared_branch;
        block->has_terminator = 1;
        if (out_changed) {
            *out_changed = 1;
        }
    }

    return 1;
}

static int machine_ir_fold_inlineable_branch_wrapper_tails(MachineIrFunction *function,
    int *out_changed,
    MachineIrError *error) {
    size_t block_index;

    if (out_changed) {
        *out_changed = 0;
    }
    if (!function) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-554: invalid inlineable-branch-wrapper cleanup contract");
        return 0;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        MachineIrBasicBlock *block = &function->blocks[block_index];
        size_t target_block_id;
        const MachineIrBasicBlock *target;
        MachineIrInstruction wrapper_instruction;
        int saved_has_terminator;
        MachineIrTerminator saved_terminator;

        if (!block->has_terminator || block->terminator.kind != MACHINE_IR_TERM_JUMP) {
            continue;
        }

        target_block_id = block->terminator.as.jump_target;
        if (target_block_id >= function->block_count) {
            continue;
        }
        target = &function->blocks[target_block_id];
        if (!target->has_terminator ||
            target->terminator.kind != MACHINE_IR_TERM_BRANCH ||
            !machine_ir_block_is_inlineable_branch_wrapper(target, &wrapper_instruction, NULL)) {
            continue;
        }

        saved_has_terminator = block->has_terminator;
        saved_terminator = block->terminator;
        block->has_terminator = 0;
        if (!machine_ir_block_append_instruction(block, &wrapper_instruction, error)) {
            block->terminator = saved_terminator;
            block->has_terminator = saved_has_terminator;
            return 0;
        }
        block->terminator = target->terminator;
        block->has_terminator = 1;
        if (out_changed) {
            *out_changed = 1;
        }
    }

    return 1;
}

static int machine_ir_fold_call_branch_wrapper_tails(MachineIrFunction *function,
    int *out_changed,
    MachineIrError *error) {
    size_t block_index;

    if (out_changed) {
        *out_changed = 0;
    }
    if (!function) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-557: invalid call-branch-wrapper cleanup contract");
        return 0;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        MachineIrBasicBlock *block = &function->blocks[block_index];
        size_t target_block_id;
        const MachineIrBasicBlock *target;
        MachineIrInstruction wrapper_instruction;
        MachineIrTerminator wrapper_branch;
        int saved_has_terminator;
        MachineIrTerminator saved_terminator;

        if (!block->has_terminator || block->terminator.kind != MACHINE_IR_TERM_JUMP) {
            continue;
        }

        target_block_id = block->terminator.as.jump_target;
        if (target_block_id >= function->block_count) {
            continue;
        }
        target_block_id = machine_ir_resolve_threaded_jump_target(function, target_block_id);
        target = &function->blocks[target_block_id];
        if (!machine_ir_block_is_single_call_branch_wrapper(target, &wrapper_instruction, &wrapper_branch)) {
            continue;
        }

        saved_has_terminator = block->has_terminator;
        saved_terminator = block->terminator;
        block->has_terminator = 0;
        if (!machine_ir_block_append_instruction(block, &wrapper_instruction, error)) {
            block->terminator = saved_terminator;
            block->has_terminator = saved_has_terminator;
            return 0;
        }
        block->terminator = wrapper_branch;
        block->has_terminator = 1;
        if (out_changed) {
            *out_changed = 1;
        }
    }

    return 1;
}

static int machine_ir_compute_predecessor_counts(const MachineIrFunction *function,
    size_t **out_predecessor_counts,
    MachineIrError *error) {
    size_t *predecessor_counts = NULL;
    size_t block_index;

    if (out_predecessor_counts) {
        *out_predecessor_counts = NULL;
    }
    if (!function || !out_predecessor_counts) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-528: invalid predecessor-count cleanup contract");
        return 0;
    }

    predecessor_counts = function->block_count > 0
        ? (size_t *)calloc(function->block_count, sizeof(size_t))
        : NULL;
    if (function->block_count > 0 && !predecessor_counts) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-529: out of memory building predecessor counts");
        return 0;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        const MachineIrBasicBlock *block = &function->blocks[block_index];

        if (!block->has_terminator) {
            continue;
        }
        switch (block->terminator.kind) {
            case MACHINE_IR_TERM_JUMP:
                if (block->terminator.as.jump_target < function->block_count) {
                    ++predecessor_counts[block->terminator.as.jump_target];
                }
                break;
            case MACHINE_IR_TERM_BRANCH:
                if (block->terminator.as.branch.then_target < function->block_count) {
                    ++predecessor_counts[block->terminator.as.branch.then_target];
                }
                if (block->terminator.as.branch.else_target < function->block_count) {
                    ++predecessor_counts[block->terminator.as.branch.else_target];
                }
                break;
            default:
                break;
        }
    }

    *out_predecessor_counts = predecessor_counts;
    return 1;
}

static int machine_ir_remove_unreachable_blocks(MachineIrFunction *function,
    int *out_changed,
    MachineIrError *error) {
    unsigned char *reachable = NULL;
    size_t *stack = NULL;
    size_t *new_ids = NULL;
    size_t stack_size = 0;
    size_t block_id;
    size_t old_count;
    size_t write_index = 0;

    if (out_changed) {
        *out_changed = 0;
    }
    if (!function) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-522: invalid unreachable-block cleanup contract");
        return 0;
    }
    if (!function->has_body || function->block_count == 0) {
        return 1;
    }

    old_count = function->block_count;
    reachable = (unsigned char *)calloc(old_count, sizeof(unsigned char));
    stack = (size_t *)malloc(old_count * sizeof(size_t));
    new_ids = (size_t *)malloc(old_count * sizeof(size_t));
    if (!reachable || !stack || !new_ids) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-523: out of memory computing unreachable-block cleanup");
        free(new_ids);
        free(stack);
        free(reachable);
        return 0;
    }

    for (block_id = 0; block_id < old_count; ++block_id) {
        new_ids[block_id] = (size_t)-1;
    }

    stack[stack_size++] = 0;
    reachable[0] = 1u;
    while (stack_size > 0) {
        size_t current = stack[--stack_size];
        const MachineIrBasicBlock *block = &function->blocks[current];

        if (!block->has_terminator) {
            machine_ir_set_error(error, 0, 0, "MACHINE-IR-524: unreachable cleanup requires terminated reachable blocks");
            free(new_ids);
            free(stack);
            free(reachable);
            return 0;
        }

        switch (block->terminator.kind) {
            case MACHINE_IR_TERM_JUMP: {
                size_t target = block->terminator.as.jump_target;

                if (target >= old_count) {
                    machine_ir_set_error(error, 0, 0, "MACHINE-IR-525: unreachable cleanup saw out-of-range jump target");
                    free(new_ids);
                    free(stack);
                    free(reachable);
                    return 0;
                }
                if (!reachable[target]) {
                    reachable[target] = 1u;
                    stack[stack_size++] = target;
                }
                break;
            }
            case MACHINE_IR_TERM_BRANCH: {
                size_t then_target = block->terminator.as.branch.then_target;
                size_t else_target = block->terminator.as.branch.else_target;

                if (then_target >= old_count || else_target >= old_count) {
                    machine_ir_set_error(error, 0, 0, "MACHINE-IR-526: unreachable cleanup saw out-of-range branch target");
                    free(new_ids);
                    free(stack);
                    free(reachable);
                    return 0;
                }
                if (!reachable[then_target]) {
                    reachable[then_target] = 1u;
                    stack[stack_size++] = then_target;
                }
                if (!reachable[else_target]) {
                    reachable[else_target] = 1u;
                    stack[stack_size++] = else_target;
                }
                break;
            }
            default:
                break;
        }
    }

    for (block_id = 0; block_id < old_count; ++block_id) {
        if (reachable[block_id]) {
            new_ids[block_id] = write_index++;
        }
    }
    if (write_index == old_count) {
        free(new_ids);
        free(stack);
        free(reachable);
        return 1;
    }

    if (out_changed) {
        *out_changed = 1;
    }

    write_index = 0;
    for (block_id = 0; block_id < old_count; ++block_id) {
        if (!reachable[block_id]) {
            machine_ir_basic_block_free(&function->blocks[block_id]);
            memset(&function->blocks[block_id], 0, sizeof(function->blocks[block_id]));
            continue;
        }
        if (write_index != block_id) {
            function->blocks[write_index] = function->blocks[block_id];
            memset(&function->blocks[block_id], 0, sizeof(function->blocks[block_id]));
        }
        function->blocks[write_index].id = write_index;
        ++write_index;
    }
    function->block_count = write_index;

    for (block_id = 0; block_id < function->block_count; ++block_id) {
        MachineIrBasicBlock *block = &function->blocks[block_id];
        size_t phi_index;

        if (block->has_terminator) {
            switch (block->terminator.kind) {
                case MACHINE_IR_TERM_JUMP:
                    block->terminator.as.jump_target = new_ids[block->terminator.as.jump_target];
                    break;
                case MACHINE_IR_TERM_BRANCH:
                    block->terminator.as.branch.then_target = new_ids[block->terminator.as.branch.then_target];
                    block->terminator.as.branch.else_target = new_ids[block->terminator.as.branch.else_target];
                    break;
                default:
                    break;
            }
        }

        for (phi_index = 0; phi_index < block->phi_count; ++phi_index) {
            MachineIrPhi *phi = &block->phis[phi_index];
            size_t input_index;
            size_t input_write_index = 0;

            for (input_index = 0; input_index < phi->input_count; ++input_index) {
                size_t predecessor_block_id = phi->inputs[input_index].predecessor_block_id;

                if (predecessor_block_id >= old_count || new_ids[predecessor_block_id] == (size_t)-1) {
                    continue;
                }
                if (input_write_index != input_index) {
                    phi->inputs[input_write_index] = phi->inputs[input_index];
                }
                phi->inputs[input_write_index].predecessor_block_id = new_ids[predecessor_block_id];
                ++input_write_index;
            }
            phi->input_count = input_write_index;
        }
    }

    free(new_ids);
    free(stack);
    free(reachable);
    return 1;
}

static int machine_ir_remove_block_with_predecessor_rewrite(MachineIrFunction *function,
    size_t remove_block_id,
    size_t predecessor_rewrite_id,
    MachineIrError *error) {
    size_t old_count;
    size_t block_index;
    size_t write_index;

    if (!function || remove_block_id >= function->block_count || predecessor_rewrite_id >= function->block_count) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-530: invalid block-removal cleanup contract");
        return 0;
    }

    old_count = function->block_count;
    machine_ir_basic_block_free(&function->blocks[remove_block_id]);
    for (write_index = remove_block_id; write_index + 1 < function->block_count; ++write_index) {
        function->blocks[write_index] = function->blocks[write_index + 1];
        function->blocks[write_index].id = write_index;
    }
    --function->block_count;
    memset(&function->blocks[function->block_count], 0, sizeof(function->blocks[function->block_count]));

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        MachineIrBasicBlock *block = &function->blocks[block_index];
        size_t phi_index;

        if (block->has_terminator) {
            switch (block->terminator.kind) {
                case MACHINE_IR_TERM_JUMP:
                    if (block->terminator.as.jump_target == remove_block_id) {
                        machine_ir_set_error(error, 0, 0, "MACHINE-IR-531: stale jump target remained during block merge");
                        return 0;
                    }
                    if (block->terminator.as.jump_target > remove_block_id) {
                        --block->terminator.as.jump_target;
                    }
                    break;
                case MACHINE_IR_TERM_BRANCH:
                    if (block->terminator.as.branch.then_target == remove_block_id ||
                        block->terminator.as.branch.else_target == remove_block_id) {
                        machine_ir_set_error(error, 0, 0, "MACHINE-IR-532: stale branch target remained during block merge");
                        return 0;
                    }
                    if (block->terminator.as.branch.then_target > remove_block_id) {
                        --block->terminator.as.branch.then_target;
                    }
                    if (block->terminator.as.branch.else_target > remove_block_id) {
                        --block->terminator.as.branch.else_target;
                    }
                    break;
                default:
                    break;
            }
        }

        for (phi_index = 0; phi_index < block->phi_count; ++phi_index) {
            MachineIrPhi *phi = &block->phis[phi_index];
            size_t input_index;

            for (input_index = 0; input_index < phi->input_count; ++input_index) {
                size_t predecessor_block_id = phi->inputs[input_index].predecessor_block_id;

                if (predecessor_block_id == remove_block_id) {
                    predecessor_block_id = predecessor_rewrite_id;
                }
                if (predecessor_block_id > remove_block_id) {
                    --predecessor_block_id;
                }
                if (predecessor_block_id >= old_count - 1) {
                    phi->inputs[input_index].predecessor_block_id = predecessor_block_id;
                } else {
                    phi->inputs[input_index].predecessor_block_id = predecessor_block_id;
                }
            }
        }
    }

    return 1;
}

static int machine_ir_merge_linear_jump_blocks(MachineIrFunction *function,
    int *out_changed,
    MachineIrError *error) {
    size_t *predecessor_counts = NULL;
    size_t block_index;

    if (out_changed) {
        *out_changed = 0;
    }
    if (!function) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-533: invalid linear-merge cleanup contract");
        return 0;
    }
    if (!machine_ir_compute_predecessor_counts(function, &predecessor_counts, error)) {
        return 0;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        MachineIrBasicBlock *block = &function->blocks[block_index];
        size_t successor_block_id;
        MachineIrBasicBlock *successor;
        MachineIrTerminator successor_terminator;
        size_t instruction_index;

        if (!block->has_terminator || block->terminator.kind != MACHINE_IR_TERM_JUMP) {
            continue;
        }

        successor_block_id = block->terminator.as.jump_target;
        if (successor_block_id >= function->block_count || successor_block_id == block_index) {
            continue;
        }
        successor = &function->blocks[successor_block_id];
        if (successor->id == 0 || successor->phi_count != 0 || predecessor_counts[successor_block_id] != 1) {
            continue;
        }

        successor_terminator = successor->terminator;
        block->has_terminator = 0;
        memset(&block->terminator, 0, sizeof(block->terminator));
        for (instruction_index = 0; instruction_index < successor->instruction_count; ++instruction_index) {
            if (!machine_ir_block_append_instruction(block, &successor->instructions[instruction_index], error)) {
                free(predecessor_counts);
                return 0;
            }
        }
        block->terminator = successor_terminator;
        block->has_terminator = successor->has_terminator;

        if (!machine_ir_remove_block_with_predecessor_rewrite(function,
                successor_block_id,
                block_index,
                error)) {
            free(predecessor_counts);
            return 0;
        }
        if (out_changed) {
            *out_changed = 1;
        }
        free(predecessor_counts);
        return 1;
    }

    free(predecessor_counts);
    return 1;
}

int machine_ir_eliminate_trivial_moves(MachineIrProgram *program,
    MachineIrError *error) {
    size_t function_index;

    if (!program) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-513: invalid trivial-move cleanup contract");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        MachineIrFunction *function = &program->functions[function_index];
        size_t block_index;

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            MachineIrBasicBlock *block = &function->blocks[block_index];
            size_t read_index = 0;
            size_t write_index = 0;

            while (read_index < block->instruction_count) {
                if (!machine_ir_instruction_is_trivial_move(&block->instructions[read_index])) {
                    if (write_index != read_index) {
                        block->instructions[write_index] = block->instructions[read_index];
                        memset(&block->instructions[read_index], 0, sizeof(block->instructions[read_index]));
                    }
                    ++write_index;
                } else {
                    machine_ir_instruction_free(&block->instructions[read_index]);
                    memset(&block->instructions[read_index], 0, sizeof(block->instructions[read_index]));
                }
                ++read_index;
            }
            block->instruction_count = write_index;
        }
    }

    return machine_ir_verify_program(program, error);
}

int machine_ir_eliminate_trivial_moves_in_report(MachineIrAllocateRewriteReport *report,
    MachineIrError *error) {
    if (!report) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-514: invalid trivial-move cleanup report contract");
        return 0;
    }
    if (!machine_ir_eliminate_trivial_moves(&report->program, error)) {
        return 0;
    }
    return machine_ir_allocate_rewrite_report_refresh_shape(report, error);
}

int machine_ir_eliminate_trivial_moves_dump(const MachineIrProgram *program,
    char **out_text,
    MachineIrError *error) {
    MachineIrProgram copy;
    int ok = 0;

    if (out_text) {
        *out_text = NULL;
    }
    if (!program || !out_text) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-515: invalid trivial-move cleanup dump contract");
        return 0;
    }

    machine_ir_program_init(&copy);
    if (!machine_ir_clone_program(program, &copy, error)) {
        return 0;
    }
    ok = machine_ir_eliminate_trivial_moves(&copy, error) &&
        machine_ir_dump_program(&copy, out_text, error);
    machine_ir_program_free(&copy);
    return ok;
}

int machine_ir_canonicalize_program(MachineIrProgram *program,
    MachineIrError *error) {
    int changed = 0;

    if (!program) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-516: invalid machine-ir canonicalization contract");
        return 0;
    }
    if (!machine_ir_eliminate_phi_nodes(program, error)) {
        return 0;
    }
    if (!machine_ir_eliminate_trivial_moves(program, error)) {
        return 0;
    }
    do {
        size_t function_index;

        changed = 0;
        if (!machine_ir_simplify_trivial_branches(program, &changed, error)) {
            return 0;
        }
        if (!machine_ir_cleanup_known_slot_values(program, &changed, error)) {
            return 0;
        }
        if (!machine_ir_cleanup_machine_values(program, &changed, error)) {
            return 0;
        }
        for (function_index = 0; function_index < program->function_count; ++function_index) {
            int function_changed = 0;

            if (!machine_ir_cleanup_local_copies_in_function(
                    program, &program->functions[function_index], &function_changed, error)) {
                return 0;
            }
            if (function_changed) {
                changed = 1;
            }
            function_changed = 0;
            if (!machine_ir_fold_identical_branch_successors(
                    &program->functions[function_index], &function_changed, error)) {
                return 0;
            }
            if (function_changed) {
                changed = 1;
            }
            function_changed = 0;
            if (!machine_ir_fold_empty_branch_wrapper_tails(
                    &program->functions[function_index], &function_changed, error)) {
                return 0;
            }
            if (function_changed) {
                changed = 1;
            }
            function_changed = 0;
            if (!machine_ir_fold_inlineable_branch_wrapper_tails(
                    &program->functions[function_index], &function_changed, error)) {
                return 0;
            }
            if (function_changed) {
                changed = 1;
            }
            function_changed = 0;
            if (!machine_ir_fold_call_branch_wrapper_tails(
                    &program->functions[function_index], &function_changed, error)) {
                return 0;
            }
            if (function_changed) {
                changed = 1;
            }
            function_changed = 0;
            if (!machine_ir_fold_same_single_instruction_branch_successors(
                    program, &program->functions[function_index], &function_changed, error)) {
                return 0;
            }
            if (function_changed) {
                changed = 1;
            }
            function_changed = 0;
            if (!machine_ir_fold_equivalent_inlineable_branch_successors(
                    program, &program->functions[function_index], &function_changed, error)) {
                return 0;
            }
            if (function_changed) {
                changed = 1;
            }
            function_changed = 0;
            if (!machine_ir_fold_equivalent_inlineable_return_branch_successors(
                    program, &program->functions[function_index], &function_changed, error)) {
                return 0;
            }
            if (function_changed) {
                changed = 1;
            }
            function_changed = 0;
            if (!machine_ir_fold_return_tails(&program->functions[function_index], &function_changed, error)) {
                return 0;
            }
            if (function_changed) {
                changed = 1;
            }
            function_changed = 0;
            if (!machine_ir_fold_thin_return_wrapper_tails(
                    &program->functions[function_index], &function_changed, error)) {
                return 0;
            }
            if (function_changed) {
                changed = 1;
            }
            function_changed = 0;
            if (!machine_ir_fold_inlineable_return_wrapper_tails(
                    &program->functions[function_index], &function_changed, error)) {
                return 0;
            }
            if (function_changed) {
                changed = 1;
            }
            function_changed = 0;
            if (!machine_ir_fold_call_return_wrapper_tails(
                    &program->functions[function_index], &function_changed, error)) {
                return 0;
            }
            if (function_changed) {
                changed = 1;
            }
            function_changed = 0;
            if (!machine_ir_fold_thin_jump_wrapper_tails(
                    &program->functions[function_index], &function_changed, error)) {
                return 0;
            }
            if (function_changed) {
                changed = 1;
            }
            function_changed = 0;
            if (!machine_ir_thread_jump_targets(&program->functions[function_index], &function_changed, error)) {
                return 0;
            }
            if (function_changed) {
                changed = 1;
            }
            function_changed = 0;
            if (!machine_ir_compact_empty_jump_blocks(&program->functions[function_index], &function_changed, error)) {
                return 0;
            }
            if (function_changed) {
                changed = 1;
            }
            function_changed = 0;
            if (!machine_ir_remove_unreachable_blocks(&program->functions[function_index], &function_changed, error)) {
                return 0;
            }
            if (function_changed) {
                changed = 1;
            }
            function_changed = 0;
            if (!machine_ir_merge_linear_jump_blocks(&program->functions[function_index], &function_changed, error)) {
                return 0;
            }
            if (function_changed) {
                changed = 1;
            }
        }
    } while (changed);
    return machine_ir_verify_program(program, error);
}

int machine_ir_canonicalize_program_in_report(MachineIrAllocateRewriteReport *report,
    MachineIrError *error) {
    if (!report) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-517: invalid machine-ir canonicalization report contract");
        return 0;
    }
    if (!machine_ir_canonicalize_program(&report->program, error)) {
        return 0;
    }
    return machine_ir_allocate_rewrite_report_refresh_shape(report, error);
}

int machine_ir_canonicalize_program_dump(const MachineIrProgram *program,
    char **out_text,
    MachineIrError *error) {
    MachineIrProgram copy;
    int ok = 0;

    if (out_text) {
        *out_text = NULL;
    }
    if (!program || !out_text) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-518: invalid machine-ir canonicalization dump contract");
        return 0;
    }

    machine_ir_program_init(&copy);
    if (!machine_ir_clone_program(program, &copy, error)) {
        return 0;
    }
    ok = machine_ir_canonicalize_program(&copy, error) &&
        machine_ir_dump_program(&copy, out_text, error);
    machine_ir_program_free(&copy);
    return ok;
}

int machine_ir_cleanup_after_phi_elimination(MachineIrProgram *program,
    MachineIrError *error) {
    return machine_ir_canonicalize_program(program, error);
}

int machine_ir_cleanup_after_phi_elimination_in_report(MachineIrAllocateRewriteReport *report,
    MachineIrError *error) {
    return machine_ir_canonicalize_program_in_report(report, error);
}

int machine_ir_cleanup_after_phi_elimination_dump(const MachineIrProgram *program,
    char **out_text,
    MachineIrError *error) {
    return machine_ir_canonicalize_program_dump(program, out_text, error);
}

#endif
