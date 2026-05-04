#if defined(__INTELLISENSE__) && !defined(MACHINE_IR_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_IR_SPLIT_AGGREGATOR)
#define MACHINE_IR_SPLIT_INTELLISENSE_CONTEXT
#include "machine_ir.c"
#else

typedef struct {
    size_t predecessor_block_id;
    MachineIrOperand dst;
    MachineIrOperand src;
} MachineIrPhiCopy;

static int machine_ir_operands_equal(MachineIrOperand lhs, MachineIrOperand rhs) {
    if (lhs.kind != rhs.kind) {
        return 0;
    }
    switch (lhs.kind) {
        case MACHINE_IR_OPERAND_NONE:
            return 1;
        case MACHINE_IR_OPERAND_IMMEDIATE:
            return lhs.immediate == rhs.immediate;
        case MACHINE_IR_OPERAND_REGISTER:
            return lhs.machine_register_id == rhs.machine_register_id;
        case MACHINE_IR_OPERAND_SPILL_SLOT:
            return lhs.spill_slot == rhs.spill_slot;
        default:
            return 0;
    }
}

static void machine_ir_rewrite_terminator_target(MachineIrBasicBlock *block,
    size_t old_target,
    size_t new_target) {
    if (!block || !block->has_terminator || old_target == new_target) {
        return;
    }

    switch (block->terminator.kind) {
        case MACHINE_IR_TERM_JUMP:
            if (block->terminator.as.jump_target == old_target) {
                block->terminator.as.jump_target = new_target;
            }
            break;
        case MACHINE_IR_TERM_BRANCH:
            if (block->terminator.as.branch.then_target == old_target) {
                block->terminator.as.branch.then_target = new_target;
            }
            if (block->terminator.as.branch.else_target == old_target) {
                block->terminator.as.branch.else_target = new_target;
            }
            break;
        default:
            break;
    }
}

static int machine_ir_phi_copy_equals(const MachineIrPhiCopy *lhs, const MachineIrPhiCopy *rhs) {
    if (!lhs || !rhs) {
        return 0;
    }

    return lhs->predecessor_block_id == rhs->predecessor_block_id &&
        machine_ir_operands_equal(lhs->dst, rhs->dst) &&
        machine_ir_operands_equal(lhs->src, rhs->src);
}

static int machine_ir_block_contains_critical_edge_target(const MachineIrFunction *function,
    size_t predecessor_block_id,
    size_t successor_block_id) {
    size_t incoming_count = 0;
    size_t block_index;
    const MachineIrBasicBlock *predecessor;

    if (!function || predecessor_block_id >= function->block_count || successor_block_id >= function->block_count) {
        return 0;
    }

    predecessor = &function->blocks[predecessor_block_id];
    if (!predecessor->has_terminator || predecessor->terminator.kind != MACHINE_IR_TERM_BRANCH) {
        return 0;
    }
    for (block_index = 0; block_index < function->block_count; ++block_index) {
        const MachineIrBasicBlock *candidate = &function->blocks[block_index];
        int reaches = 0;

        if (!candidate->has_terminator) {
            continue;
        }
        if (candidate->terminator.kind == MACHINE_IR_TERM_JUMP) {
            reaches = candidate->terminator.as.jump_target == successor_block_id;
        } else if (candidate->terminator.kind == MACHINE_IR_TERM_BRANCH) {
            reaches = candidate->terminator.as.branch.then_target == successor_block_id ||
                candidate->terminator.as.branch.else_target == successor_block_id;
        }
        if (reaches) {
            ++incoming_count;
        }
    }
    return incoming_count > 1;
}

static int machine_ir_ensure_edge_block(MachineIrFunction *function,
    size_t predecessor_block_id,
    size_t successor_block_id,
    size_t *out_edge_block_id,
    MachineIrError *error) {
    MachineIrBasicBlock *predecessor;

    if (out_edge_block_id) {
        *out_edge_block_id = (size_t)-1;
    }
    if (!function || predecessor_block_id >= function->block_count || successor_block_id >= function->block_count ||
        !out_edge_block_id) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-503: invalid edge-block contract");
        return 0;
    }

    if (!machine_ir_block_contains_critical_edge_target(function, predecessor_block_id, successor_block_id)) {
        *out_edge_block_id = predecessor_block_id;
        return 1;
    }

    predecessor = &function->blocks[predecessor_block_id];
    if (predecessor->has_terminator && predecessor->terminator.kind == MACHINE_IR_TERM_BRANCH) {
        MachineIrBasicBlock *new_block = NULL;

        if (!machine_ir_function_append_block(function, out_edge_block_id, &new_block, error) || !new_block) {
            return 0;
        }
        predecessor = &function->blocks[predecessor_block_id];
        if (predecessor->terminator.as.branch.then_target == successor_block_id) {
            predecessor->terminator.as.branch.then_target = *out_edge_block_id;
        }
        if (predecessor->terminator.as.branch.else_target == successor_block_id) {
            predecessor->terminator.as.branch.else_target = *out_edge_block_id;
        }
        if (!machine_ir_block_set_jump(new_block, successor_block_id, error)) {
            return 0;
        }
        return 1;
    }

    *out_edge_block_id = predecessor_block_id;
    return 1;
}

static int machine_ir_compact_empty_jump_blocks(MachineIrFunction *function,
    int *out_changed,
    MachineIrError *error) {
    size_t block_index = 0;

    if (out_changed) {
        *out_changed = 0;
    }
    if (!function) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-520: invalid jump-block cleanup contract");
        return 0;
    }

    while (block_index < function->block_count) {
        MachineIrBasicBlock *block = &function->blocks[block_index];
        size_t redirect_target;
        size_t predecessor_index;
        size_t remap_index;
        size_t write_index;

        if (block->id == 0 ||
            block->phi_count != 0 ||
            block->instruction_count != 0 ||
            !block->has_terminator ||
            block->terminator.kind != MACHINE_IR_TERM_JUMP ||
            block->terminator.as.jump_target == block->id) {
            ++block_index;
            continue;
        }

        redirect_target = block->terminator.as.jump_target;
        for (predecessor_index = 0; predecessor_index < function->block_count; ++predecessor_index) {
            if (predecessor_index == block_index) {
                continue;
            }
            machine_ir_rewrite_terminator_target(&function->blocks[predecessor_index], block->id, redirect_target);
        }

        machine_ir_basic_block_free(block);
        for (write_index = block_index; write_index + 1 < function->block_count; ++write_index) {
            function->blocks[write_index] = function->blocks[write_index + 1];
            function->blocks[write_index].id = write_index;
        }
        --function->block_count;
        memset(&function->blocks[function->block_count], 0, sizeof(function->blocks[function->block_count]));
        if (out_changed) {
            *out_changed = 1;
        }

        for (remap_index = 0; remap_index < function->block_count; ++remap_index) {
            MachineIrBasicBlock *candidate = &function->blocks[remap_index];
            size_t phi_index;

            if (candidate->has_terminator) {
                switch (candidate->terminator.kind) {
                    case MACHINE_IR_TERM_JUMP:
                        if (candidate->terminator.as.jump_target > block_index) {
                            --candidate->terminator.as.jump_target;
                        }
                        break;
                    case MACHINE_IR_TERM_BRANCH:
                        if (candidate->terminator.as.branch.then_target > block_index) {
                            --candidate->terminator.as.branch.then_target;
                        }
                        if (candidate->terminator.as.branch.else_target > block_index) {
                            --candidate->terminator.as.branch.else_target;
                        }
                        break;
                    default:
                        break;
                }
            }

            for (phi_index = 0; phi_index < candidate->phi_count; ++phi_index) {
                size_t input_index;

                for (input_index = 0; input_index < candidate->phis[phi_index].input_count; ++input_index) {
                    if (candidate->phis[phi_index].inputs[input_index].predecessor_block_id > block_index) {
                        --candidate->phis[phi_index].inputs[input_index].predecessor_block_id;
                    }
                }
            }
        }
    }

    return 1;
}

static int machine_ir_block_append_instruction_before_terminator(MachineIrBasicBlock *block,
    const MachineIrInstruction *instruction,
    MachineIrError *error) {
    int saved_has_terminator;
    MachineIrTerminator saved_terminator;
    int ok;

    if (!block || !instruction) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-504: invalid before-terminator append contract");
        return 0;
    }
    saved_has_terminator = block->has_terminator;
    saved_terminator = block->terminator;
    block->has_terminator = 0;
    ok = machine_ir_block_append_instruction(block, instruction, error);
    block->terminator = saved_terminator;
    block->has_terminator = saved_has_terminator;
    return ok;
}

static int machine_ir_append_move_instruction(MachineIrBasicBlock *block,
    MachineIrOperand dst,
    MachineIrOperand src,
    MachineIrError *error) {
    MachineIrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = dst;
    instruction.as.mov_value = src;
    return machine_ir_block_append_instruction_before_terminator(block, &instruction, error);
}

static size_t machine_ir_count_cycle_breaks(const MachineIrPhiCopy *copies,
    size_t copy_count) {
    unsigned char *completed;
    size_t completed_count = 0;
    size_t cycle_break_count = 0;
    int progress;
    size_t copy_index;

    if (!copies || copy_count == 0) {
        return 0;
    }

    completed = (unsigned char *)calloc(copy_count, sizeof(unsigned char));
    if (!completed) {
        return copy_count;
    }

    while (completed_count < copy_count) {
        progress = 0;
        for (copy_index = 0; copy_index < copy_count; ++copy_index) {
            size_t other_index;
            int dst_is_live_source = 0;

            if (completed[copy_index]) {
                continue;
            }
            for (other_index = 0; other_index < copy_count; ++other_index) {
                if (completed[other_index] || other_index == copy_index) {
                    continue;
                }
                if (machine_ir_operands_equal(copies[copy_index].dst, copies[other_index].src)) {
                    dst_is_live_source = 1;
                    break;
                }
            }
            if (dst_is_live_source) {
                continue;
            }
            completed[copy_index] = 1u;
            ++completed_count;
            progress = 1;
        }
        if (!progress) {
            for (copy_index = 0; copy_index < copy_count; ++copy_index) {
                if (!completed[copy_index]) {
                    completed[copy_index] = 1u;
                    ++completed_count;
                    ++cycle_break_count;
                    break;
                }
            }
        }
    }

    free(completed);
    return cycle_break_count;
}

static int machine_ir_emit_parallel_copies_with_scratch(MachineIrBasicBlock *block,
    MachineIrPhiCopy *copies,
    size_t copy_count,
    size_t scratch_spill_slot,
    MachineIrError *error) {
    unsigned char *completed = NULL;
    size_t completed_count = 0;
    int progress = 1;

    if (!block) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-505: invalid parallel-copy emission contract");
        return 0;
    }
    if (copy_count == 0) {
        return 1;
    }

    completed = (unsigned char *)calloc(copy_count, sizeof(unsigned char));
    if (!completed) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-506: out of memory preparing parallel copies");
        return 0;
    }

    while (completed_count < copy_count) {
        size_t copy_index;

        progress = 0;
        for (copy_index = 0; copy_index < copy_count; ++copy_index) {
            size_t other_index;
            int dst_is_live_source = 0;

            if (completed[copy_index]) {
                continue;
            }
            for (other_index = 0; other_index < copy_count; ++other_index) {
                if (completed[other_index] || other_index == copy_index) {
                    continue;
                }
                if (machine_ir_operands_equal(copies[copy_index].dst, copies[other_index].src)) {
                    dst_is_live_source = 1;
                    break;
                }
            }
            if (dst_is_live_source) {
                continue;
            }
            if (!machine_ir_operands_equal(copies[copy_index].dst, copies[copy_index].src) &&
                !machine_ir_append_move_instruction(block, copies[copy_index].dst, copies[copy_index].src, error)) {
                free(completed);
                return 0;
            }
            completed[copy_index] = 1u;
            ++completed_count;
            progress = 1;
        }

        if (!progress) {
            size_t cycle_index;
            MachineIrOperand scratch_operand = machine_ir_operand_spill_slot(scratch_spill_slot);

            for (cycle_index = 0; cycle_index < copy_count; ++cycle_index) {
                if (!completed[cycle_index]) {
                    break;
                }
            }
            if (cycle_index >= copy_count) {
                break;
            }
            if (!machine_ir_append_move_instruction(block, scratch_operand, copies[cycle_index].src, error)) {
                free(completed);
                return 0;
            }
            copies[cycle_index].src = scratch_operand;
        }
    }

    free(completed);
    return 1;
}

int machine_ir_eliminate_phi_nodes(MachineIrProgram *program,
    MachineIrError *error) {
    size_t function_index;

    if (!program) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-507: invalid phi-elimination contract");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        MachineIrFunction *function = &program->functions[function_index];
        size_t block_index = 0;

        while (block_index < function->block_count) {
            MachineIrBasicBlock *block = &function->blocks[block_index];
            MachineIrPhiCopy *copies = NULL;
            size_t copy_count = 0;
            size_t phi_index;
            size_t current_block_id = block->id;

            if (block->phi_count == 0) {
                ++block_index;
                continue;
            }

            for (phi_index = 0; phi_index < block->phi_count; ++phi_index) {
                copy_count += block->phis[phi_index].input_count;
            }
            copies = copy_count > 0 ? (MachineIrPhiCopy *)calloc(copy_count, sizeof(MachineIrPhiCopy)) : NULL;
            if (copy_count > 0 && !copies) {
                machine_ir_set_error(error, 0, 0, "MACHINE-IR-508: out of memory collecting phi copies");
                return 0;
            }

            copy_count = 0;
            {
                size_t *edge_block_ids = (size_t *)malloc(function->block_count * sizeof(size_t));
                if (!edge_block_ids) {
                    free(copies);
                    machine_ir_set_error(error, 0, 0, "MACHINE-IR-508: out of memory preparing edge map");
                    return 0;
                }
                for (phi_index = 0; phi_index < function->block_count; ++phi_index) {
                    edge_block_ids[phi_index] = (size_t)-1;
                }

                for (phi_index = 0; phi_index < block->phi_count; ++phi_index) {
                    MachineIrPhi *phi = &block->phis[phi_index];
                    size_t input_index;

                    for (input_index = 0; input_index < phi->input_count; ++input_index) {
                        MachineIrPhiCopy candidate;
                        size_t predecessor_block_id = phi->inputs[input_index].predecessor_block_id;
                        size_t existing_index;

                        candidate.predecessor_block_id = predecessor_block_id;
                        candidate.dst = phi->result;
                        candidate.src = phi->inputs[input_index].value;
                        if (machine_ir_operands_equal(candidate.dst, candidate.src)) {
                            continue;
                        }
                        for (existing_index = 0; existing_index < copy_count; ++existing_index) {
                            if (machine_ir_phi_copy_equals(&copies[existing_index], &candidate)) {
                                break;
                            }
                        }
                        if (existing_index < copy_count) {
                            continue;
                        }
                        copies[copy_count++] = candidate;
                    }
                }

                for (phi_index = 0; phi_index < copy_count; ++phi_index) {
                    size_t predecessor_block_id = copies[phi_index].predecessor_block_id;
                    size_t edge_block_id;

                    if (edge_block_ids[predecessor_block_id] == (size_t)-1) {
                        if (!machine_ir_ensure_edge_block(
                                function,
                                predecessor_block_id,
                                current_block_id,
                                &edge_block_id,
                                error)) {
                            free(edge_block_ids);
                            free(copies);
                            return 0;
                        }
                        edge_block_ids[predecessor_block_id] = edge_block_id;
                    }
                    copies[phi_index].predecessor_block_id = edge_block_ids[predecessor_block_id];
                }
                free(edge_block_ids);
            }

            /*
             * Critical-edge splitting may grow/realloc function->blocks, so the
             * old `block` pointer is no longer trustworthy after the edge pass.
             */
            block = &function->blocks[current_block_id];

            if (copy_count > 0) {
                size_t predecessor_index = 0;
                size_t cycle_break_count = machine_ir_count_cycle_breaks(copies, copy_count);
                size_t scratch_spill_slot = function->spill_slot_count;

                if (cycle_break_count > 0) {
                    function->spill_slot_count += cycle_break_count;
                }

                while (predecessor_index < function->block_count) {
                    MachineIrBasicBlock *predecessor = &function->blocks[predecessor_index];
                    size_t edge_copy_count = 0;
                    size_t copy_index;
                    MachineIrPhiCopy *edge_copies = NULL;

                    for (copy_index = 0; copy_index < copy_count; ++copy_index) {
                        if (copies[copy_index].predecessor_block_id == predecessor_index) {
                            ++edge_copy_count;
                        }
                    }
                    if (edge_copy_count == 0) {
                        ++predecessor_index;
                        continue;
                    }
                    edge_copies = (MachineIrPhiCopy *)malloc(edge_copy_count * sizeof(MachineIrPhiCopy));
                    if (!edge_copies) {
                        free(copies);
                        machine_ir_set_error(error, 0, 0, "MACHINE-IR-509: out of memory grouping edge phi copies");
                        return 0;
                    }
                    edge_copy_count = 0;
                    for (copy_index = 0; copy_index < copy_count; ++copy_index) {
                        if (copies[copy_index].predecessor_block_id == predecessor_index) {
                            edge_copies[edge_copy_count++] = copies[copy_index];
                        }
                    }

                    if (!machine_ir_emit_parallel_copies_with_scratch(
                            predecessor, edge_copies, edge_copy_count, scratch_spill_slot, error)) {
                        free(edge_copies);
                        free(copies);
                        return 0;
                    }

                    free(edge_copies);
                    ++predecessor_index;
                }
            }

            for (phi_index = 0; phi_index < block->phi_count; ++phi_index) {
                machine_ir_phi_free(&block->phis[phi_index]);
            }
            free(block->phis);
            block->phis = NULL;
            block->phi_count = 0;
            block->phi_capacity = 0;
            free(copies);
            ++block_index;
        }
    }

    return machine_ir_verify_program(program, error);
}

int machine_ir_eliminate_phi_nodes_in_report(MachineIrAllocateRewriteReport *report,
    MachineIrError *error) {
    if (!report) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-512: invalid phi-elimination report contract");
        return 0;
    }
    if (!machine_ir_eliminate_phi_nodes(&report->program, error)) {
        return 0;
    }
    return machine_ir_allocate_rewrite_report_refresh_shape(report, error);
}

int machine_ir_eliminate_phi_nodes_dump(const MachineIrProgram *program,
    char **out_text,
    MachineIrError *error) {
    MachineIrProgram copy;
    int ok = 0;

    if (out_text) {
        *out_text = NULL;
    }
    if (!program || !out_text) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-510: invalid phi-elimination dump contract");
        return 0;
    }

    machine_ir_program_init(&copy);
    if (!machine_ir_clone_program(program, &copy, error)) {
        return 0;
    }
    ok = machine_ir_eliminate_phi_nodes(&copy, error) &&
        machine_ir_dump_program(&copy, out_text, error);
    machine_ir_program_free(&copy);
    return ok;
}

#endif
