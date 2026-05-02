#if defined(__INTELLISENSE__) && !defined(MACHINE_SELECT_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_SELECT_SPLIT_AGGREGATOR)
#define MACHINE_SELECT_SPLIT_INTELLISENSE_CONTEXT
#include "machine_select.c"
#else

static int machine_select_operand_matches_result(MachineSelectOperand operand, MachineSelectOperand result) {
    if (operand.kind != result.kind) {
        return 0;
    }
    switch (operand.kind) {
        case MACHINE_SELECT_OPERAND_REGISTER:
            return operand.machine_register_id == result.machine_register_id;
        case MACHINE_SELECT_OPERAND_SPILL_SLOT:
            return operand.spill_slot == result.spill_slot;
        default:
            return 0;
    }
}

static int machine_select_op_redefines_result(const MachineSelectOp *op, MachineSelectOperand result) {
    if (!op || !op->has_result) {
        return 0;
    }
    return machine_select_operand_matches_result(op->result, result);
}

static size_t machine_select_count_op_result_uses(const MachineSelectOp *op, MachineSelectOperand result) {
    size_t count = 0;
    size_t arg_index;

    if (!op) {
        return 0;
    }
    switch (op->kind) {
        case MACHINE_SELECT_OP_COPY:
        case MACHINE_SELECT_OP_MATERIALIZE_IMM:
            if (machine_select_operand_matches_result(op->as.copy_value, result)) {
                count += 1;
            }
            break;
        case MACHINE_SELECT_OP_ALU:
        case MACHINE_SELECT_OP_ALU_IMM:
        case MACHINE_SELECT_OP_CMP:
        case MACHINE_SELECT_OP_CMP_IMM:
            if (machine_select_operand_matches_result(op->as.binary.lhs, result)) {
                count += 1;
            }
            if (machine_select_operand_matches_result(op->as.binary.rhs, result)) {
                count += 1;
            }
            break;
        case MACHINE_SELECT_OP_CALL:
        case MACHINE_SELECT_OP_CALL_IMM:
        case MACHINE_SELECT_OP_CALL_SPILL:
        case MACHINE_SELECT_OP_CALL_IMM_SPILL:
        case MACHINE_SELECT_OP_CALL_VOID:
        case MACHINE_SELECT_OP_CALL_VOID_IMM:
            for (arg_index = 0; arg_index < op->as.call.arg_count; ++arg_index) {
                if (machine_select_operand_matches_result(op->as.call.args[arg_index], result)) {
                    count += 1;
                }
            }
            break;
        case MACHINE_SELECT_OP_STORE_LOCAL:
        case MACHINE_SELECT_OP_STORE_LOCAL_IMM:
        case MACHINE_SELECT_OP_STORE_GLOBAL:
        case MACHINE_SELECT_OP_STORE_GLOBAL_IMM:
            if (machine_select_operand_matches_result(op->as.store.value, result)) {
                count += 1;
            }
            break;
        default:
            break;
    }
    return count;
}

static size_t machine_select_count_term_result_uses(const MachineSelectTerminator *terminator,
    MachineSelectOperand result) {
    size_t count = 0;

    if (!terminator) {
        return 0;
    }
    switch (terminator->kind) {
        case MACHINE_SELECT_TERM_RETURN:
        case MACHINE_SELECT_TERM_RETURN_IMM:
        case MACHINE_SELECT_TERM_RETURN_SPILL:
            if (machine_select_operand_matches_result(terminator->as.return_value, result)) {
                count += 1;
            }
            break;
        case MACHINE_SELECT_TERM_BRANCH:
            if (machine_select_operand_matches_result(terminator->as.branch.condition, result)) {
                count += 1;
            }
            break;
        case MACHINE_SELECT_TERM_COMPARE_BRANCH:
        case MACHINE_SELECT_TERM_COMPARE_BRANCH_IMM:
            if (machine_select_operand_matches_result(terminator->as.compare_branch.lhs, result)) {
                count += 1;
            }
            if (machine_select_operand_matches_result(terminator->as.compare_branch.rhs, result)) {
                count += 1;
            }
            break;
        default:
            break;
    }
    return count;
}

static int machine_select_cleanup_op_shape(MachineSelectOp *op);

static size_t machine_select_count_visible_result_uses_in_block(const MachineSelectBasicBlock *block,
    size_t producer_index,
    MachineSelectOperand result,
    int *out_redefined_before_terminator) {
    size_t use_count = 0;
    size_t op_index;

    if (out_redefined_before_terminator) {
        *out_redefined_before_terminator = 0;
    }
    if (!block || producer_index >= block->op_count) {
        return 0;
    }

    for (op_index = producer_index + 1; op_index < block->op_count; ++op_index) {
        use_count += machine_select_count_op_result_uses(&block->ops[op_index], result);
        if (machine_select_op_redefines_result(&block->ops[op_index], result)) {
            if (out_redefined_before_terminator) {
                *out_redefined_before_terminator = 1;
            }
            return use_count;
        }
    }

    use_count += machine_select_count_term_result_uses(&block->terminator, result);
    return use_count;
}

static void machine_select_rewrite_op_operand(MachineSelectOp *op,
    MachineSelectOperand result,
    MachineSelectOperand replacement) {
    size_t arg_index;

    if (!op) {
        return;
    }
    switch (op->kind) {
        case MACHINE_SELECT_OP_COPY:
        case MACHINE_SELECT_OP_MATERIALIZE_IMM:
            if (machine_select_operand_matches_result(op->as.copy_value, result)) {
                op->as.copy_value = replacement;
            }
            break;
        case MACHINE_SELECT_OP_ALU:
        case MACHINE_SELECT_OP_ALU_IMM:
        case MACHINE_SELECT_OP_CMP:
        case MACHINE_SELECT_OP_CMP_IMM:
            if (machine_select_operand_matches_result(op->as.binary.lhs, result)) {
                op->as.binary.lhs = replacement;
            }
            if (machine_select_operand_matches_result(op->as.binary.rhs, result)) {
                op->as.binary.rhs = replacement;
            }
            break;
        case MACHINE_SELECT_OP_CALL:
        case MACHINE_SELECT_OP_CALL_IMM:
        case MACHINE_SELECT_OP_CALL_SPILL:
        case MACHINE_SELECT_OP_CALL_IMM_SPILL:
        case MACHINE_SELECT_OP_CALL_VOID:
        case MACHINE_SELECT_OP_CALL_VOID_IMM:
            for (arg_index = 0; arg_index < op->as.call.arg_count; ++arg_index) {
                if (machine_select_operand_matches_result(op->as.call.args[arg_index], result)) {
                    op->as.call.args[arg_index] = replacement;
                }
            }
            break;
        case MACHINE_SELECT_OP_STORE_LOCAL:
        case MACHINE_SELECT_OP_STORE_LOCAL_IMM:
        case MACHINE_SELECT_OP_STORE_GLOBAL:
        case MACHINE_SELECT_OP_STORE_GLOBAL_IMM:
            if (machine_select_operand_matches_result(op->as.store.value, result)) {
                op->as.store.value = replacement;
            }
            break;
        default:
            break;
    }
}

static void machine_select_rewrite_term_operand(MachineSelectTerminator *terminator,
    MachineSelectOperand result,
    MachineSelectOperand replacement) {
    if (!terminator) {
        return;
    }
    switch (terminator->kind) {
        case MACHINE_SELECT_TERM_RETURN:
        case MACHINE_SELECT_TERM_RETURN_IMM:
        case MACHINE_SELECT_TERM_RETURN_SPILL:
            if (machine_select_operand_matches_result(terminator->as.return_value, result)) {
                terminator->as.return_value = replacement;
            }
            break;
        case MACHINE_SELECT_TERM_BRANCH:
            if (machine_select_operand_matches_result(terminator->as.branch.condition, result)) {
                terminator->as.branch.condition = replacement;
            }
            break;
        case MACHINE_SELECT_TERM_COMPARE_BRANCH:
        case MACHINE_SELECT_TERM_COMPARE_BRANCH_IMM:
            if (machine_select_operand_matches_result(terminator->as.compare_branch.lhs, result)) {
                terminator->as.compare_branch.lhs = replacement;
            }
            if (machine_select_operand_matches_result(terminator->as.compare_branch.rhs, result)) {
                terminator->as.compare_branch.rhs = replacement;
            }
            break;
        default:
            break;
    }
}

static int machine_select_rewrite_visible_uses_in_block(MachineSelectBasicBlock *block,
    size_t producer_index,
    MachineSelectOperand result,
    MachineSelectOperand replacement) {
    size_t op_index;
    int changed = 0;

    if (!block || producer_index >= block->op_count) {
        return 0;
    }

    for (op_index = producer_index + 1; op_index < block->op_count; ++op_index) {
        size_t before_uses = machine_select_count_op_result_uses(&block->ops[op_index], result);

        if (before_uses != 0) {
            machine_select_rewrite_op_operand(&block->ops[op_index], result, replacement);
            machine_select_cleanup_op_shape(&block->ops[op_index]);
            changed = 1;
        }
        if (machine_select_op_redefines_result(&block->ops[op_index], result)) {
            return changed;
        }
    }

    if (machine_select_count_term_result_uses(&block->terminator, result) != 0) {
        machine_select_rewrite_term_operand(&block->terminator, result, replacement);
        changed = 1;
    }
    return changed;
}

static int machine_select_cleanup_op_shape(MachineSelectOp *op) {
    size_t arg_index;
    long long folded_value;

    if (!op) {
        return 0;
    }

    switch (op->kind) {
        case MACHINE_SELECT_OP_COPY:
        case MACHINE_SELECT_OP_MATERIALIZE_IMM:
            if (op->as.copy_value.kind == MACHINE_SELECT_OPERAND_IMMEDIATE) {
                if (op->kind != MACHINE_SELECT_OP_MATERIALIZE_IMM) {
                    op->kind = MACHINE_SELECT_OP_MATERIALIZE_IMM;
                    return 1;
                }
                return 0;
            }
            if (op->kind != MACHINE_SELECT_OP_COPY) {
                op->kind = MACHINE_SELECT_OP_COPY;
                return 1;
            }
            return 0;
        case MACHINE_SELECT_OP_ALU:
        case MACHINE_SELECT_OP_ALU_IMM:
            if (machine_select_try_eval_selected_binary_immediate(
                    op->as.binary.op,
                    op->as.binary.lhs,
                    op->as.binary.rhs,
                    &folded_value)) {
                op->kind = MACHINE_SELECT_OP_MATERIALIZE_IMM;
                op->as.copy_value = machine_select_operand_immediate(folded_value);
                return 1;
            }
            if (op->as.binary.lhs.kind == MACHINE_SELECT_OPERAND_IMMEDIATE ||
                op->as.binary.rhs.kind == MACHINE_SELECT_OPERAND_IMMEDIATE) {
                if (op->kind != MACHINE_SELECT_OP_ALU_IMM) {
                    op->kind = MACHINE_SELECT_OP_ALU_IMM;
                    return 1;
                }
                return 0;
            }
            if (op->kind != MACHINE_SELECT_OP_ALU) {
                op->kind = MACHINE_SELECT_OP_ALU;
                return 1;
            }
            return 0;
        case MACHINE_SELECT_OP_CMP:
        case MACHINE_SELECT_OP_CMP_IMM:
            if (machine_select_try_eval_selected_binary_immediate(
                    op->as.binary.op,
                    op->as.binary.lhs,
                    op->as.binary.rhs,
                    &folded_value)) {
                op->kind = MACHINE_SELECT_OP_MATERIALIZE_IMM;
                op->as.copy_value = machine_select_operand_immediate(folded_value);
                return 1;
            }
            if (op->as.binary.lhs.kind == MACHINE_SELECT_OPERAND_IMMEDIATE ||
                op->as.binary.rhs.kind == MACHINE_SELECT_OPERAND_IMMEDIATE) {
                if (op->kind != MACHINE_SELECT_OP_CMP_IMM) {
                    op->kind = MACHINE_SELECT_OP_CMP_IMM;
                    return 1;
                }
                return 0;
            }
            if (op->kind != MACHINE_SELECT_OP_CMP) {
                op->kind = MACHINE_SELECT_OP_CMP;
                return 1;
            }
            return 0;
        case MACHINE_SELECT_OP_STORE_LOCAL:
        case MACHINE_SELECT_OP_STORE_LOCAL_IMM:
            if (op->as.store.value.kind == MACHINE_SELECT_OPERAND_IMMEDIATE) {
                if (op->kind != MACHINE_SELECT_OP_STORE_LOCAL_IMM) {
                    op->kind = MACHINE_SELECT_OP_STORE_LOCAL_IMM;
                    return 1;
                }
                return 0;
            }
            if (op->kind != MACHINE_SELECT_OP_STORE_LOCAL) {
                op->kind = MACHINE_SELECT_OP_STORE_LOCAL;
                return 1;
            }
            return 0;
        case MACHINE_SELECT_OP_STORE_GLOBAL:
        case MACHINE_SELECT_OP_STORE_GLOBAL_IMM:
            if (op->as.store.value.kind == MACHINE_SELECT_OPERAND_IMMEDIATE) {
                if (op->kind != MACHINE_SELECT_OP_STORE_GLOBAL_IMM) {
                    op->kind = MACHINE_SELECT_OP_STORE_GLOBAL_IMM;
                    return 1;
                }
                return 0;
            }
            if (op->kind != MACHINE_SELECT_OP_STORE_GLOBAL) {
                op->kind = MACHINE_SELECT_OP_STORE_GLOBAL;
                return 1;
            }
            return 0;
        case MACHINE_SELECT_OP_CALL:
        case MACHINE_SELECT_OP_CALL_IMM:
        case MACHINE_SELECT_OP_CALL_SPILL:
        case MACHINE_SELECT_OP_CALL_IMM_SPILL:
            for (arg_index = 0; arg_index < op->as.call.arg_count; ++arg_index) {
                if (op->as.call.args[arg_index].kind == MACHINE_SELECT_OPERAND_IMMEDIATE) {
                    MachineSelectOpKind expected_kind = op->result.kind == MACHINE_SELECT_OPERAND_SPILL_SLOT
                        ? MACHINE_SELECT_OP_CALL_IMM_SPILL
                        : MACHINE_SELECT_OP_CALL_IMM;

                    if (op->kind != expected_kind) {
                        op->kind = expected_kind;
                        return 1;
                    }
                    return 0;
                }
            }
            if (op->kind != (op->result.kind == MACHINE_SELECT_OPERAND_SPILL_SLOT
                    ? MACHINE_SELECT_OP_CALL_SPILL
                    : MACHINE_SELECT_OP_CALL)) {
                op->kind = op->result.kind == MACHINE_SELECT_OPERAND_SPILL_SLOT
                    ? MACHINE_SELECT_OP_CALL_SPILL
                    : MACHINE_SELECT_OP_CALL;
                return 1;
            }
            return 0;
        case MACHINE_SELECT_OP_CALL_VOID:
        case MACHINE_SELECT_OP_CALL_VOID_IMM:
            for (arg_index = 0; arg_index < op->as.call.arg_count; ++arg_index) {
                if (op->as.call.args[arg_index].kind == MACHINE_SELECT_OPERAND_IMMEDIATE) {
                    if (op->kind != MACHINE_SELECT_OP_CALL_VOID_IMM) {
                        op->kind = MACHINE_SELECT_OP_CALL_VOID_IMM;
                        return 1;
                    }
                    return 0;
                }
            }
            if (op->kind != MACHINE_SELECT_OP_CALL_VOID) {
                op->kind = MACHINE_SELECT_OP_CALL_VOID;
                return 1;
            }
            return 0;
        default:
            return 0;
    }
}

static int machine_select_op_is_pure_value_op(const MachineSelectOp *op) {
    if (!op || !op->has_result) {
        return 0;
    }
    switch (op->kind) {
        case MACHINE_SELECT_OP_COPY:
        case MACHINE_SELECT_OP_MATERIALIZE_IMM:
        case MACHINE_SELECT_OP_ALU:
        case MACHINE_SELECT_OP_ALU_IMM:
        case MACHINE_SELECT_OP_CMP:
        case MACHINE_SELECT_OP_CMP_IMM:
        case MACHINE_SELECT_OP_LOAD_LOCAL:
        case MACHINE_SELECT_OP_LOAD_GLOBAL:
            return 1;
        default:
            return 0;
    }
}

static int machine_select_cleanup_canonicalize_ops(MachineSelectBasicBlock *block) {
    size_t op_index;
    int changed = 0;

    if (!block) {
        return 0;
    }

    for (op_index = 0; op_index < block->op_count; ++op_index) {
        if (machine_select_cleanup_op_shape(&block->ops[op_index])) {
            changed = 1;
        }
    }
    return changed;
}

static void machine_select_remove_op_at(MachineSelectBasicBlock *block, size_t op_index) {
    size_t next_index;

    if (!block || op_index >= block->op_count) {
        return;
    }
    machine_select_op_free(&block->ops[op_index]);
    for (next_index = op_index + 1; next_index < block->op_count; ++next_index) {
        block->ops[next_index - 1] = block->ops[next_index];
    }
    block->op_count -= 1;
    if (block->op_count == 0) {
        free(block->ops);
        block->ops = NULL;
        block->op_capacity = 0;
    } else {
        memset(&block->ops[block->op_count], 0, sizeof(block->ops[block->op_count]));
    }
}

static int machine_select_cleanup_terminator_shape(MachineSelectBasicBlock *block) {
    long long folded_value;

    if (!block) {
        return 0;
    }

    if (block->terminator.kind == MACHINE_SELECT_TERM_RETURN ||
        block->terminator.kind == MACHINE_SELECT_TERM_RETURN_IMM ||
        block->terminator.kind == MACHINE_SELECT_TERM_RETURN_SPILL) {
        MachineSelectTerminatorKind expected_kind = MACHINE_SELECT_TERM_RETURN;

        if (block->terminator.as.return_value.kind == MACHINE_SELECT_OPERAND_IMMEDIATE) {
            expected_kind = MACHINE_SELECT_TERM_RETURN_IMM;
        } else if (block->terminator.as.return_value.kind == MACHINE_SELECT_OPERAND_SPILL_SLOT) {
            expected_kind = MACHINE_SELECT_TERM_RETURN_SPILL;
        }
        if (block->terminator.kind != expected_kind) {
            block->terminator.kind = expected_kind;
            return 1;
        }
        return 0;
    }

    if (block->terminator.kind == MACHINE_SELECT_TERM_BRANCH &&
        block->terminator.as.branch.condition.kind == MACHINE_SELECT_OPERAND_IMMEDIATE) {
        block->terminator.kind = MACHINE_SELECT_TERM_JUMP;
        block->terminator.as.jump_target = block->terminator.as.branch.condition.immediate != 0
            ? block->terminator.as.branch.then_target
            : block->terminator.as.branch.else_target;
        return 1;
    }

    if ((block->terminator.kind == MACHINE_SELECT_TERM_COMPARE_BRANCH ||
            block->terminator.kind == MACHINE_SELECT_TERM_COMPARE_BRANCH_IMM) &&
        machine_select_try_eval_selected_binary_immediate(
            block->terminator.as.compare_branch.op,
            block->terminator.as.compare_branch.lhs,
            block->terminator.as.compare_branch.rhs,
            &folded_value)) {
        size_t then_target = block->terminator.as.compare_branch.then_target;
        size_t else_target = block->terminator.as.compare_branch.else_target;

        block->terminator.kind = MACHINE_SELECT_TERM_JUMP;
        block->terminator.as.jump_target = folded_value != 0 ? then_target : else_target;
        return 1;
    }

    if (block->terminator.kind == MACHINE_SELECT_TERM_COMPARE_BRANCH) {
        if (block->terminator.as.compare_branch.lhs.kind == MACHINE_SELECT_OPERAND_IMMEDIATE ||
            block->terminator.as.compare_branch.rhs.kind == MACHINE_SELECT_OPERAND_IMMEDIATE) {
            block->terminator.kind = MACHINE_SELECT_TERM_COMPARE_BRANCH_IMM;
            return 1;
        }
    }

    return 0;
}

static int machine_select_cleanup_forward_trivial_defs_in_block(MachineSelectBasicBlock *block) {
    int changed = 0;
    int local_changed = 1;

    if (!block) {
        return 0;
    }

    while (local_changed) {
        size_t op_index;

        local_changed = 0;
        for (op_index = block->op_count; op_index > 0; --op_index) {
            MachineSelectOp *op = &block->ops[op_index - 1];
            MachineSelectOperand replacement;
            size_t total_uses;

            if (!op->has_result ||
                (op->kind != MACHINE_SELECT_OP_COPY && op->kind != MACHINE_SELECT_OP_MATERIALIZE_IMM)) {
                continue;
            }

            total_uses = machine_select_count_visible_result_uses_in_block(block, op_index - 1, op->result, NULL);
            if (total_uses == 0) {
                continue;
            }

            replacement = op->as.copy_value;
            machine_select_rewrite_visible_uses_in_block(block, op_index - 1, op->result, replacement);
            machine_select_remove_op_at(block, op_index - 1);
            changed = 1;
            local_changed = 1;
            machine_select_cleanup_terminator_shape(block);
            break;
        }
    }

    return changed;
}

static int machine_select_cleanup_remove_dead_defs_in_block(MachineSelectBasicBlock *block) {
    int changed = 0;
    int local_changed = 1;

    if (!block) {
        return 0;
    }

    while (local_changed) {
        size_t op_index;

        local_changed = 0;
        for (op_index = block->op_count; op_index > 0; --op_index) {
            MachineSelectOp *op = &block->ops[op_index - 1];
            size_t total_uses;

            if (!machine_select_op_is_pure_value_op(op)) {
                continue;
            }

            total_uses = machine_select_count_visible_result_uses_in_block(block, op_index - 1, op->result, NULL);
            if (total_uses != 0) {
                continue;
            }

            machine_select_remove_op_at(block, op_index - 1);
            changed = 1;
            local_changed = 1;
            break;
        }
    }

    return changed;
}

static int machine_select_cleanup_program(MachineSelectProgram *program, MachineSelectError *error) {
    size_t function_index;

    (void)error;
    if (!program) {
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        MachineSelectFunction *function = &program->functions[function_index];
        size_t block_index;

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            MachineSelectBasicBlock *block = &function->blocks[block_index];
            int local_changed = 1;

            while (local_changed) {
                local_changed = 0;
                if (machine_select_cleanup_canonicalize_ops(block)) {
                    local_changed = 1;
                }
                if (machine_select_cleanup_forward_trivial_defs_in_block(block)) {
                    local_changed = 1;
                }
                if (machine_select_cleanup_remove_dead_defs_in_block(block)) {
                    local_changed = 1;
                }
                if (machine_select_cleanup_terminator_shape(block)) {
                    local_changed = 1;
                }
            }
        }
    }
    return 1;
}

#endif
