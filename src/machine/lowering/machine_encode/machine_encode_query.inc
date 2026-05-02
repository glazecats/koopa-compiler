#if defined(__INTELLISENSE__) && !defined(MACHINE_ENCODE_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_ENCODE_SPLIT_AGGREGATOR)
#define MACHINE_ENCODE_SPLIT_INTELLISENSE_CONTEXT
#include "machine_encode.c"
#else

static const MachineEncodeBlock *machine_encode_target_block(const MachineEncodeFunction *function, size_t target_index) {
    if (!function || target_index >= function->block_count || !function->blocks) {
        return NULL;
    }
    return &function->blocks[target_index];
}

int machine_encode_program_get_summary(const MachineEncodeProgram *program,
    size_t *out_register_count,
    size_t *out_global_count,
    size_t *out_function_count) {
    if (!program) {
        return 0;
    }
    if (out_register_count) {
        *out_register_count = program->register_bank.register_count;
    }
    if (out_global_count) {
        *out_global_count = program->global_count;
    }
    if (out_function_count) {
        *out_function_count = program->function_count;
    }
    return 1;
}

int machine_encode_program_get_function(const MachineEncodeProgram *program,
    size_t function_index,
    const MachineEncodeFunction **out_function) {
    if (!program || !out_function || function_index >= program->function_count || !program->functions) {
        return 0;
    }
    *out_function = &program->functions[function_index];
    return 1;
}

int machine_encode_program_get_function_by_name(const MachineEncodeProgram *program,
    const char *function_name,
    size_t *out_function_index,
    const MachineEncodeFunction **out_function) {
    size_t function_index;

    if (!program || !function_name || !program->functions) {
        return 0;
    }
    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const MachineEncodeFunction *function = &program->functions[function_index];

        if (function->name && strcmp(function->name, function_name) == 0) {
            if (out_function_index) {
                *out_function_index = function_index;
            }
            if (out_function) {
                *out_function = function;
            }
            return 1;
        }
    }
    return 0;
}

int machine_encode_program_find_block_by_label(const MachineEncodeProgram *program,
    const char *label_name,
    size_t *out_function_index,
    const MachineEncodeFunction **out_function,
    const MachineEncodeBlock **out_block) {
    size_t function_index;

    if (!program || !label_name || !program->functions) {
        return 0;
    }
    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const MachineEncodeFunction *function = &program->functions[function_index];

        if (machine_encode_function_find_block_by_label(function, label_name, NULL, out_block)) {
            if (out_function_index) {
                *out_function_index = function_index;
            }
            if (out_function) {
                *out_function = function;
            }
            return 1;
        }
    }
    return 0;
}

int machine_encode_program_find_block_by_function_name_and_offset(const MachineEncodeProgram *program,
    const char *function_name,
    size_t offset,
    size_t *out_function_index,
    size_t *out_emit_index,
    const MachineEncodeFunction **out_function,
    const MachineEncodeBlock **out_block) {
    const MachineEncodeFunction *function;
    size_t function_index = 0;

    if (!program || !function_name) {
        return 0;
    }
    if (!machine_encode_program_get_function_by_name(program, function_name, &function_index, &function)) {
        return 0;
    }
    if (!machine_encode_function_find_block_covering_offset(function, offset, out_emit_index, out_block)) {
        return 0;
    }
    if (out_function_index) {
        *out_function_index = function_index;
    }
    if (out_function) {
        *out_function = function;
    }
    return 1;
}

int machine_encode_function_get_summary(const MachineEncodeFunction *function,
    const char **out_name,
    int *out_has_body,
    size_t *out_parameter_count,
    size_t *out_local_count,
    size_t *out_block_count,
    size_t *out_spill_slot_count) {
    if (!function) {
        return 0;
    }
    if (out_name) {
        *out_name = function->name;
    }
    if (out_has_body) {
        *out_has_body = function->has_body;
    }
    if (out_parameter_count) {
        *out_parameter_count = function->parameter_count;
    }
    if (out_local_count) {
        *out_local_count = function->local_count;
    }
    if (out_block_count) {
        *out_block_count = function->block_count;
    }
    if (out_spill_slot_count) {
        *out_spill_slot_count = function->spill_slot_count;
    }
    return 1;
}

int machine_encode_function_get_block(const MachineEncodeFunction *function,
    size_t emit_index,
    const MachineEncodeBlock **out_block) {
    if (!function || !out_block || emit_index >= function->block_count || !function->blocks) {
        return 0;
    }
    *out_block = &function->blocks[emit_index];
    return 1;
}

int machine_encode_function_find_block_by_label(const MachineEncodeFunction *function,
    const char *label_name,
    size_t *out_emit_index,
    const MachineEncodeBlock **out_block) {
    size_t block_index;

    if (!function || !label_name || !function->blocks) {
        return 0;
    }
    for (block_index = 0; block_index < function->block_count; ++block_index) {
        if (function->blocks[block_index].label_name &&
            strcmp(function->blocks[block_index].label_name, label_name) == 0) {
            if (out_emit_index) {
                *out_emit_index = block_index;
            }
            if (out_block) {
                *out_block = &function->blocks[block_index];
            }
            return 1;
        }
    }
    return 0;
}

int machine_encode_function_find_block_covering_offset(const MachineEncodeFunction *function,
    size_t offset,
    size_t *out_emit_index,
    const MachineEncodeBlock **out_block) {
    size_t block_index;

    if (!function || !function->blocks) {
        return 0;
    }
    for (block_index = 0; block_index < function->block_count; ++block_index) {
        const MachineEncodeBlock *block = &function->blocks[block_index];

        if (offset >= block->start_offset && offset < block->end_offset) {
            if (out_emit_index) {
                *out_emit_index = block_index;
            }
            if (out_block) {
                *out_block = block;
            }
            return 1;
        }
    }
    return 0;
}

int machine_encode_block_get_summary(const MachineEncodeBlock *block,
    MachineEncodeBlockSummary *out_summary) {
    if (!block || !out_summary) {
        return 0;
    }
    out_summary->emit_index = block->emit_index;
    out_summary->original_layout_index = block->original_layout_index;
    out_summary->original_block_id = block->original_block_id;
    out_summary->label_name = block->label_name;
    out_summary->start_offset = block->start_offset;
    out_summary->terminator_offset = block->terminator_offset;
    out_summary->end_offset = block->end_offset;
    return 1;
}

int machine_encode_function_get_block_terminator_summary(const MachineEncodeFunction *function,
    size_t emit_index,
    MachineEncodeTerminatorSummary *out_summary) {
    const MachineEncodeBlock *block;

    if (!function || !out_summary || !machine_encode_function_get_block(function, emit_index, &block)) {
        return 0;
    }

    memset(out_summary, 0, sizeof(*out_summary));
    out_summary->kind = block->block.terminator.kind;
    switch (block->block.terminator.kind) {
        case MACHINE_LAYOUT_TERM_FALLTHROUGH: {
            const MachineEncodeBlock *target =
                machine_encode_target_block(function, block->block.terminator.as.fallthrough_target);

            if (!target) {
                return 0;
            }
            out_summary->has_primary_target = 1;
            out_summary->primary_target_label_name = target->label_name;
            out_summary->primary_target_offset = target->start_offset;
            break;
        }
        case MACHINE_LAYOUT_TERM_JUMP: {
            const MachineEncodeBlock *target =
                machine_encode_target_block(function, block->block.terminator.as.jump_target);

            if (!target) {
                return 0;
            }
            out_summary->has_primary_target = 1;
            out_summary->primary_target_label_name = target->label_name;
            out_summary->primary_target_offset = target->start_offset;
            break;
        }
        case MACHINE_LAYOUT_TERM_BRANCH: {
            const MachineEncodeBlock *then_target =
                machine_encode_target_block(function, block->block.terminator.as.branch.then_target);
            const MachineEncodeBlock *else_target =
                machine_encode_target_block(function, block->block.terminator.as.branch.else_target);

            if (!then_target || !else_target) {
                return 0;
            }
            out_summary->has_primary_target = 1;
            out_summary->primary_target_label_name = then_target->label_name;
            out_summary->primary_target_offset = then_target->start_offset;
            out_summary->has_secondary_target = 1;
            out_summary->secondary_target_label_name = else_target->label_name;
            out_summary->secondary_target_offset = else_target->start_offset;
            break;
        }
        case MACHINE_LAYOUT_TERM_BRANCH_FALLTHROUGH: {
            const MachineEncodeBlock *taken_target =
                machine_encode_target_block(function, block->block.terminator.as.branch_fallthrough.taken_target);
            const MachineEncodeBlock *fallthrough_target = machine_encode_target_block(
                function, block->block.terminator.as.branch_fallthrough.fallthrough_target);

            if (!taken_target || !fallthrough_target) {
                return 0;
            }
            out_summary->has_primary_target = 1;
            out_summary->primary_target_label_name = taken_target->label_name;
            out_summary->primary_target_offset = taken_target->start_offset;
            out_summary->has_secondary_target = 1;
            out_summary->secondary_target_label_name = fallthrough_target->label_name;
            out_summary->secondary_target_offset = fallthrough_target->start_offset;
            out_summary->branch_on_true = block->block.terminator.as.branch_fallthrough.branch_on_true;
            break;
        }
        case MACHINE_LAYOUT_TERM_COMPARE_BRANCH:
        case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM: {
            const MachineEncodeBlock *then_target =
                machine_encode_target_block(function, block->block.terminator.as.compare_branch.then_target);
            const MachineEncodeBlock *else_target =
                machine_encode_target_block(function, block->block.terminator.as.compare_branch.else_target);

            if (!then_target || !else_target) {
                return 0;
            }
            out_summary->has_primary_target = 1;
            out_summary->primary_target_label_name = then_target->label_name;
            out_summary->primary_target_offset = then_target->start_offset;
            out_summary->has_secondary_target = 1;
            out_summary->secondary_target_label_name = else_target->label_name;
            out_summary->secondary_target_offset = else_target->start_offset;
            out_summary->compare_op = block->block.terminator.as.compare_branch.op;
            break;
        }
        case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_FALLTHROUGH:
        case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM_FALLTHROUGH: {
            const MachineEncodeBlock *taken_target = machine_encode_target_block(
                function, block->block.terminator.as.compare_branch_fallthrough.taken_target);
            const MachineEncodeBlock *fallthrough_target = machine_encode_target_block(
                function, block->block.terminator.as.compare_branch_fallthrough.fallthrough_target);

            if (!taken_target || !fallthrough_target) {
                return 0;
            }
            out_summary->has_primary_target = 1;
            out_summary->primary_target_label_name = taken_target->label_name;
            out_summary->primary_target_offset = taken_target->start_offset;
            out_summary->has_secondary_target = 1;
            out_summary->secondary_target_label_name = fallthrough_target->label_name;
            out_summary->secondary_target_offset = fallthrough_target->start_offset;
            out_summary->branch_on_true = block->block.terminator.as.compare_branch_fallthrough.branch_on_true;
            out_summary->compare_op = block->block.terminator.as.compare_branch_fallthrough.op;
            break;
        }
        default:
            break;
    }
    return 1;
}

int machine_encode_function_compute_summary(const MachineEncodeFunction *function,
    MachineEncodeFunctionSummary *out_summary) {
    size_t block_index;

    if (!function || !out_summary) {
        return 0;
    }
    memset(out_summary, 0, sizeof(*out_summary));
    out_summary->block_count = function->block_count;
    for (block_index = 0; block_index < function->block_count; ++block_index) {
        const MachineEncodeBlock *block = &function->blocks[block_index];
        size_t op_index;
        size_t block_call_count = 0;

        out_summary->op_unit_count += block->block.op_count;
        out_summary->terminator_unit_count += 1;
        out_summary->unit_count += block->end_offset - block->start_offset;
        for (op_index = 0; op_index < block->block.op_count; ++op_index) {
            if (machine_encode_op_has_call_payload(block->block.ops[op_index].kind)) {
                ++block_call_count;
            }
        }
        out_summary->call_count += block_call_count;
        if (block_call_count > 0) {
            ++out_summary->blocks_with_calls_count;
        }
        switch (block->block.terminator.kind) {
            case MACHINE_LAYOUT_TERM_RETURN:
            case MACHINE_LAYOUT_TERM_RETURN_IMM:
            case MACHINE_LAYOUT_TERM_RETURN_SPILL:
                ++out_summary->return_count;
                break;
            case MACHINE_LAYOUT_TERM_JUMP:
                ++out_summary->jump_count;
                break;
            case MACHINE_LAYOUT_TERM_FALLTHROUGH:
                ++out_summary->jump_count;
                ++out_summary->fallthrough_count;
                break;
            case MACHINE_LAYOUT_TERM_BRANCH:
                ++out_summary->branch_count;
                break;
            case MACHINE_LAYOUT_TERM_BRANCH_FALLTHROUGH:
                ++out_summary->branch_count;
                ++out_summary->fallthrough_count;
                break;
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH:
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM:
                ++out_summary->compare_branch_count;
                break;
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_FALLTHROUGH:
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM_FALLTHROUGH:
                ++out_summary->compare_branch_count;
                ++out_summary->fallthrough_count;
                break;
            default:
                break;
        }
    }
    return 1;
}

#endif
