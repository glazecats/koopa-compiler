#if defined(__INTELLISENSE__) && !defined(MACHINE_LAYOUT_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_LAYOUT_SPLIT_AGGREGATOR)
#define MACHINE_LAYOUT_SPLIT_INTELLISENSE_CONTEXT
#include "machine_layout.c"
#else

typedef enum {
    MACHINE_LAYOUT_SEED_DETACHED = 0,
    MACHINE_LAYOUT_SEED_ATTACHED_UNREADY,
    MACHINE_LAYOUT_SEED_READY_CONTINUATION,
    MACHINE_LAYOUT_SEED_READY_SHARED_MERGE,
} MachineLayoutSeedClass;

static size_t machine_layout_find_layout_index_for_original_block(const MachineLayoutFunction *function,
    size_t original_block_id) {
    size_t block_index;

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        if (function->blocks[block_index].original_block_id == original_block_id) {
            return block_index;
        }
    }
    return (size_t)-1;
}

static size_t machine_layout_get_successor_count(const MachineSelectBasicBlock *block) {
    if (!block || !block->has_terminator) {
        return 0;
    }
    switch (block->terminator.kind) {
        case MACHINE_SELECT_TERM_JUMP:
            return 1;
        case MACHINE_SELECT_TERM_BRANCH:
        case MACHINE_SELECT_TERM_COMPARE_BRANCH:
        case MACHINE_SELECT_TERM_COMPARE_BRANCH_IMM:
            return 2;
        default:
            return 0;
    }
}

static size_t machine_layout_get_successor_at(const MachineSelectBasicBlock *block, size_t successor_index) {
    if (!block || !block->has_terminator) {
        return (size_t)-1;
    }
    switch (block->terminator.kind) {
        case MACHINE_SELECT_TERM_JUMP:
            return successor_index == 0 ? block->terminator.as.jump_target : (size_t)-1;
        case MACHINE_SELECT_TERM_BRANCH:
            if (successor_index == 0) {
                return block->terminator.as.branch.else_target;
            }
            if (successor_index == 1) {
                return block->terminator.as.branch.then_target;
            }
            return (size_t)-1;
        case MACHINE_SELECT_TERM_COMPARE_BRANCH:
        case MACHINE_SELECT_TERM_COMPARE_BRANCH_IMM:
            if (successor_index == 0) {
                return block->terminator.as.compare_branch.else_target;
            }
            if (successor_index == 1) {
                return block->terminator.as.compare_branch.then_target;
            }
            return (size_t)-1;
        default:
            return (size_t)-1;
    }
}

static int machine_layout_compute_predecessor_counts(const MachineSelectFunction *src_function,
    size_t *predecessor_counts) {
    size_t block_index;

    if (!src_function || !predecessor_counts) {
        return 0;
    }
    memset(predecessor_counts, 0, src_function->block_count * sizeof(size_t));
    for (block_index = 0; block_index < src_function->block_count; ++block_index) {
        const MachineSelectBasicBlock *block = &src_function->blocks[block_index];
        size_t successor_count = machine_layout_get_successor_count(block);
        size_t successor_index;

        for (successor_index = 0; successor_index < successor_count; ++successor_index) {
            size_t successor = machine_layout_get_successor_at(block, successor_index);

            if (successor == (size_t)-1 || successor >= src_function->block_count) {
                return 0;
            }
            predecessor_counts[successor] += 1;
        }
    }
    return 1;
}

static size_t machine_layout_count_visited_predecessors(const MachineSelectFunction *src_function,
    size_t block_id,
    const unsigned char *visited) {
    size_t predecessor_count = 0;
    size_t source_block_index;

    if (!src_function || !visited || block_id >= src_function->block_count) {
        return 0;
    }
    for (source_block_index = 0; source_block_index < src_function->block_count; ++source_block_index) {
        const MachineSelectBasicBlock *block = &src_function->blocks[source_block_index];
        size_t successor_count = machine_layout_get_successor_count(block);
        size_t successor_index;

        if (!visited[source_block_index]) {
            continue;
        }
        for (successor_index = 0; successor_index < successor_count; ++successor_index) {
            if (machine_layout_get_successor_at(block, successor_index) == block_id) {
                predecessor_count += 1;
            }
        }
    }

    return predecessor_count;
}

static int machine_layout_block_has_successor(const MachineSelectFunction *src_function,
    size_t block_id,
    size_t successor_id) {
    const MachineSelectBasicBlock *block = NULL;
    size_t successor_count;
    size_t successor_index;

    if (!src_function ||
        block_id >= src_function->block_count ||
        successor_id >= src_function->block_count) {
        return 0;
    }

    block = &src_function->blocks[block_id];
    successor_count = machine_layout_get_successor_count(block);
    for (successor_index = 0; successor_index < successor_count; ++successor_index) {
        if (machine_layout_get_successor_at(block, successor_index) == successor_id) {
            return 1;
        }
    }
    return 0;
}

static size_t machine_layout_latest_visited_predecessor_index(const MachineSelectFunction *src_function,
    size_t block_id,
    const size_t *layout_positions) {
    size_t latest_index = (size_t)-1;
    size_t source_block_index;

    if (!src_function || !layout_positions || block_id >= src_function->block_count) {
        return (size_t)-1;
    }

    for (source_block_index = 0; source_block_index < src_function->block_count; ++source_block_index) {
        if (layout_positions[source_block_index] == (size_t)-1) {
            continue;
        }
        if (machine_layout_block_has_successor(src_function, source_block_index, block_id) &&
            (latest_index == (size_t)-1 || layout_positions[source_block_index] > latest_index)) {
            latest_index = layout_positions[source_block_index];
        }
    }

    return latest_index;
}

static size_t machine_layout_estimate_trace_span_recursive(const MachineSelectFunction *src_function,
    size_t block_id,
    const size_t *predecessor_counts,
    size_t *trace_spans,
    unsigned char *trace_states) {
    const MachineSelectBasicBlock *block = NULL;
    size_t successor_count;
    size_t successor_index;
    size_t best_tail = 0;

    if (!src_function || !predecessor_counts || !trace_spans || !trace_states ||
        block_id >= src_function->block_count) {
        return 0;
    }
    if (trace_states[block_id] == 2u) {
        return trace_spans[block_id];
    }
    if (trace_states[block_id] == 1u) {
        return 0;
    }

    trace_states[block_id] = 1u;
    block = &src_function->blocks[block_id];
    successor_count = machine_layout_get_successor_count(block);
    if (predecessor_counts[block_id] == 1 && successor_count > 0) {
        for (successor_index = 0; successor_index < successor_count; ++successor_index) {
            size_t successor = machine_layout_get_successor_at(block, successor_index);
            size_t tail = machine_layout_estimate_trace_span_recursive(
                src_function, successor, predecessor_counts, trace_spans, trace_states);

            if (tail > best_tail) {
                best_tail = tail;
            }
        }
    }
    trace_states[block_id] = 2u;
    trace_spans[block_id] = 1 + best_tail;
    return trace_spans[block_id];
}

static int machine_layout_compute_trace_spans(const MachineSelectFunction *src_function,
    const size_t *predecessor_counts,
    size_t *trace_spans) {
    unsigned char *trace_states = NULL;
    size_t block_id;

    if (!src_function || !predecessor_counts || !trace_spans) {
        return 0;
    }
    trace_states = (unsigned char *)calloc(src_function->block_count, sizeof(unsigned char));
    if (!trace_states) {
        return 0;
    }
    for (block_id = 0; block_id < src_function->block_count; ++block_id) {
        (void)machine_layout_estimate_trace_span_recursive(
            src_function, block_id, predecessor_counts, trace_spans, trace_states);
    }
    free(trace_states);
    return 1;
}

static int machine_layout_should_defer_successor(const MachineSelectFunction *src_function,
    size_t current_block_id,
    size_t successor_id,
    const size_t *predecessor_counts,
    const unsigned char *visited) {
    if (!src_function || !predecessor_counts ||
        !visited ||
        current_block_id >= src_function->block_count ||
        successor_id >= src_function->block_count) {
        return 0;
    }

    if (machine_layout_get_successor_count(&src_function->blocks[current_block_id]) != 1) {
        return 0;
    }
    if (predecessor_counts[current_block_id] != 1) {
        return 0;
    }
    if (predecessor_counts[successor_id] <= 1) {
        return 0;
    }
    return 1;
}

static int machine_layout_block_immediately_defers_shared_tail(const MachineSelectFunction *src_function,
    size_t block_id,
    const size_t *predecessor_counts) {
    size_t successor_id;

    if (!src_function || !predecessor_counts || block_id >= src_function->block_count) {
        return 0;
    }
    if (machine_layout_get_successor_count(&src_function->blocks[block_id]) != 1) {
        return 0;
    }
    if (predecessor_counts[block_id] != 1) {
        return 0;
    }

    successor_id = machine_layout_get_successor_at(&src_function->blocks[block_id], 0);
    if (successor_id == (size_t)-1 || successor_id >= src_function->block_count) {
        return 0;
    }
    return predecessor_counts[successor_id] > 1;
}

static size_t machine_layout_get_local_successor_trace_span(const MachineSelectFunction *src_function,
    size_t successor_id,
    const size_t *predecessor_counts,
    const size_t *trace_spans) {
    if (!src_function || !predecessor_counts || !trace_spans || successor_id >= src_function->block_count) {
        return 0;
    }
    if (machine_layout_block_immediately_defers_shared_tail(src_function, successor_id, predecessor_counts)) {
        return 1;
    }
    return trace_spans[successor_id];
}

static void machine_layout_choose_successor_order(const MachineSelectFunction *src_function,
    const MachineSelectBasicBlock *block,
    const size_t *predecessor_counts,
    const size_t *trace_spans,
    size_t ordered_successors[2]) {
    size_t first = machine_layout_get_successor_at(block, 0);
    size_t second = machine_layout_get_successor_at(block, 1);
    size_t first_span = 0;
    size_t second_span = 0;
    int first_score = 0;
    int second_score = 0;

    ordered_successors[0] = first;
    ordered_successors[1] = second;
    if (!src_function || !block || !predecessor_counts || !trace_spans || second == (size_t)-1) {
        return;
    }

    if (first < src_function->block_count) {
        first_span = machine_layout_get_local_successor_trace_span(
            src_function, first, predecessor_counts, trace_spans);
    }
    if (second < src_function->block_count) {
        second_span = machine_layout_get_local_successor_trace_span(
            src_function, second, predecessor_counts, trace_spans);
    }

    if (first < src_function->block_count && predecessor_counts[first] == 1) {
        first_score += 4;
    }
    if (second < src_function->block_count && predecessor_counts[second] == 1) {
        second_score += 4;
    }
    if (first < src_function->block_count &&
        machine_layout_get_successor_count(&src_function->blocks[first]) > 0 &&
        !machine_layout_block_immediately_defers_shared_tail(src_function, first, predecessor_counts)) {
        first_score += 2;
    }
    if (second < src_function->block_count &&
        machine_layout_get_successor_count(&src_function->blocks[second]) > 0 &&
        !machine_layout_block_immediately_defers_shared_tail(src_function, second, predecessor_counts)) {
        second_score += 2;
    }

    if (second_score > first_score) {
        ordered_successors[0] = second;
        ordered_successors[1] = first;
        return;
    }
    if (first_score > second_score) {
        return;
    }

    if (second_span > first_span) {
        ordered_successors[0] = second;
        ordered_successors[1] = first;
    }
}

static size_t machine_layout_pick_next_seed(const MachineSelectFunction *src_function,
    const size_t *predecessor_counts,
    const size_t *trace_spans,
    const size_t *layout_positions,
    const unsigned char *visited) {
    size_t best_block_id = (size_t)-1;
    MachineLayoutSeedClass best_seed_class = MACHINE_LAYOUT_SEED_DETACHED;
    size_t best_latest_predecessor_index = (size_t)-1;
    size_t best_visited_predecessor_count = 0;
    size_t best_trace_span = 0;
    size_t block_id;

    if (!src_function || !predecessor_counts || !trace_spans || !layout_positions || !visited) {
        return (size_t)-1;
    }

    for (block_id = 0; block_id < src_function->block_count; ++block_id) {
        MachineLayoutSeedClass seed_class;
        size_t latest_predecessor_index = (size_t)-1;
        size_t visited_predecessor_count;
        size_t trace_span;

        if (visited[block_id]) {
            continue;
        }

        visited_predecessor_count =
            machine_layout_count_visited_predecessors(src_function, block_id, visited);
        if (predecessor_counts[block_id] > 1 &&
            visited_predecessor_count == predecessor_counts[block_id]) {
            seed_class = MACHINE_LAYOUT_SEED_READY_SHARED_MERGE;
        } else if (predecessor_counts[block_id] > 0 &&
            visited_predecessor_count == predecessor_counts[block_id]) {
            seed_class = MACHINE_LAYOUT_SEED_READY_CONTINUATION;
        } else if (visited_predecessor_count > 0) {
            seed_class = MACHINE_LAYOUT_SEED_ATTACHED_UNREADY;
        } else {
            seed_class = MACHINE_LAYOUT_SEED_DETACHED;
        }
        if (seed_class != MACHINE_LAYOUT_SEED_DETACHED &&
            visited_predecessor_count > 0) {
            latest_predecessor_index =
                machine_layout_latest_visited_predecessor_index(src_function, block_id, layout_positions);
        }
        trace_span = trace_spans[block_id];
        if (best_block_id == (size_t)-1 ||
            seed_class > best_seed_class ||
            (seed_class == best_seed_class &&
                latest_predecessor_index > best_latest_predecessor_index) ||
            (seed_class == best_seed_class &&
                latest_predecessor_index == best_latest_predecessor_index &&
                visited_predecessor_count > best_visited_predecessor_count) ||
            (seed_class == best_seed_class &&
                latest_predecessor_index == best_latest_predecessor_index &&
                visited_predecessor_count == best_visited_predecessor_count &&
                trace_span > best_trace_span) ||
            (seed_class == best_seed_class &&
                latest_predecessor_index == best_latest_predecessor_index &&
                visited_predecessor_count == best_visited_predecessor_count &&
                trace_span == best_trace_span &&
                block_id < best_block_id)) {
            best_block_id = block_id;
            best_seed_class = seed_class;
            best_latest_predecessor_index = latest_predecessor_index;
            best_visited_predecessor_count = visited_predecessor_count;
            best_trace_span = trace_span;
        }
    }

    return best_block_id;
}

static int machine_layout_build_block_order_from(const MachineSelectFunction *src_function,
    size_t block_id,
    const size_t *predecessor_counts,
    const size_t *trace_spans,
    unsigned char *visited,
    size_t *layout_positions,
    size_t *layout_order,
    size_t *layout_count) {
    const MachineSelectBasicBlock *block = NULL;
    size_t successor_count;
    size_t successor_index;
    size_t ordered_successors[2] = {(size_t)-1, (size_t)-1};

    if (!src_function || !predecessor_counts || !trace_spans || !visited || !layout_positions || !layout_order ||
        !layout_count ||
        block_id >= src_function->block_count) {
        return 0;
    }
    if (visited[block_id]) {
        return 1;
    }

    visited[block_id] = 1u;
    layout_positions[block_id] = *layout_count;
    layout_order[*layout_count] = block_id;
    *layout_count += 1;
    block = &src_function->blocks[block_id];
    successor_count = machine_layout_get_successor_count(block);
    if (successor_count == 2) {
        machine_layout_choose_successor_order(
            src_function, block, predecessor_counts, trace_spans, ordered_successors);
    }
    for (successor_index = 0; successor_index < successor_count; ++successor_index) {
        size_t successor = successor_count == 2
            ? ordered_successors[successor_index]
            : machine_layout_get_successor_at(block, successor_index);

        if (successor == (size_t)-1 || successor >= src_function->block_count) {
            return 0;
        }
        if (machine_layout_should_defer_successor(
                src_function, block_id, successor, predecessor_counts, visited)) {
            continue;
        }
        if (!machine_layout_build_block_order_from(
                src_function,
                successor,
                predecessor_counts,
                trace_spans,
                visited,
                layout_positions,
                layout_order,
                layout_count)) {
            return 0;
        }
    }

    return 1;
}

int machine_layout_lower_program_from_machine_select(const MachineSelectProgram *source,
    MachineLayoutProgram *out_program,
    MachineLayoutError *error) {
    size_t register_index;
    size_t global_index;
    size_t function_index;

    if (!source || !out_program) {
        machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-300: invalid lowering contract");
        return 0;
    }

    machine_layout_program_free(out_program);
    machine_layout_program_init(out_program);
    out_program->register_bank.register_count = source->register_bank.register_count;
    if (source->register_bank.register_count > 0) {
        out_program->register_bank.registers =
            (MachineLayoutRegisterDesc *)calloc(source->register_bank.register_count, sizeof(MachineLayoutRegisterDesc));
        if (!out_program->register_bank.registers) {
            machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-309: out of memory copying register bank");
            machine_layout_program_free(out_program);
            return 0;
        }
    }
    for (register_index = 0; register_index < source->register_bank.register_count; ++register_index) {
        out_program->register_bank.registers[register_index] = source->register_bank.registers[register_index];
        out_program->register_bank.registers[register_index].name =
            machine_layout_strdup(source->register_bank.registers[register_index].name);
        if (source->register_bank.registers[register_index].name &&
            !out_program->register_bank.registers[register_index].name) {
            machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-310: out of memory copying register name");
            machine_layout_program_free(out_program);
            return 0;
        }
    }

    out_program->global_count = source->global_count;
    out_program->global_capacity = source->global_count;
    if (source->global_count > 0) {
        out_program->globals = (MachineLayoutGlobal *)calloc(source->global_count, sizeof(MachineLayoutGlobal));
        if (!out_program->globals) {
            machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-311: out of memory copying globals");
            machine_layout_program_free(out_program);
            return 0;
        }
    }
    for (global_index = 0; global_index < source->global_count; ++global_index) {
        out_program->globals[global_index] = source->globals[global_index];
        out_program->globals[global_index].name = machine_layout_strdup(source->globals[global_index].name);
        if (source->globals[global_index].name && !out_program->globals[global_index].name) {
            machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-312: out of memory copying global name");
            machine_layout_program_free(out_program);
            return 0;
        }
    }

    out_program->function_count = source->function_count;
    out_program->function_capacity = source->function_count;
    if (source->function_count > 0) {
        out_program->functions =
            (MachineLayoutFunction *)calloc(source->function_count, sizeof(MachineLayoutFunction));
        if (!out_program->functions) {
            machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-301: out of memory copying functions");
            machine_layout_program_free(out_program);
            return 0;
        }
    }

    for (function_index = 0; function_index < source->function_count; ++function_index) {
        const MachineSelectFunction *src_function = &source->functions[function_index];
        MachineLayoutFunction *dest_function = &out_program->functions[function_index];
        size_t *layout_order = NULL;
        size_t *layout_positions = NULL;
        size_t *predecessor_counts = NULL;
        size_t *trace_spans = NULL;
        unsigned char *visited = NULL;
        size_t layout_count = 0;
        size_t block_index;

        dest_function->name = machine_layout_strdup(src_function->name);
        if (src_function->name && !dest_function->name) {
            machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-302: out of memory copying function name");
            machine_layout_program_free(out_program);
            return 0;
        }
        dest_function->has_body = src_function->has_body;
        dest_function->parameter_count = src_function->parameter_count;
        dest_function->local_count = src_function->local_count;
        dest_function->spill_slot_count = src_function->spill_slot_count;
        dest_function->block_count = src_function->block_count;
        dest_function->block_capacity = src_function->block_count;
        if (src_function->block_count > 0) {
            dest_function->blocks =
                (MachineLayoutBlock *)calloc(src_function->block_count, sizeof(MachineLayoutBlock));
            if (!dest_function->blocks) {
                machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-303: out of memory copying blocks");
                machine_layout_program_free(out_program);
                return 0;
            }
            layout_order = (size_t *)calloc(src_function->block_count, sizeof(size_t));
            layout_positions = (size_t *)malloc(src_function->block_count * sizeof(size_t));
            predecessor_counts = (size_t *)calloc(src_function->block_count, sizeof(size_t));
            trace_spans = (size_t *)calloc(src_function->block_count, sizeof(size_t));
            visited = (unsigned char *)calloc(src_function->block_count, sizeof(unsigned char));
            if (!layout_order || !layout_positions || !predecessor_counts || !trace_spans || !visited) {
                free(layout_order);
                free(layout_positions);
                free(predecessor_counts);
                free(trace_spans);
                free(visited);
                machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-315: out of memory building layout order");
                machine_layout_program_free(out_program);
                return 0;
            }
            memset(layout_positions, 0xff, src_function->block_count * sizeof(size_t));
            if (!machine_layout_compute_predecessor_counts(src_function, predecessor_counts) ||
                !machine_layout_compute_trace_spans(src_function, predecessor_counts, trace_spans) ||
                !machine_layout_build_block_order_from(
                    src_function,
                    0,
                    predecessor_counts,
                    trace_spans,
                    visited,
                    layout_positions,
                    layout_order,
                    &layout_count)) {
                free(layout_order);
                free(layout_positions);
                free(predecessor_counts);
                free(trace_spans);
                free(visited);
                machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-316: malformed CFG during layout ordering");
                machine_layout_program_free(out_program);
                return 0;
            }
            while (layout_count < src_function->block_count) {
                size_t seed_block_id =
                    machine_layout_pick_next_seed(
                        src_function, predecessor_counts, trace_spans, layout_positions, visited);

                if (seed_block_id == (size_t)-1 ||
                    !machine_layout_build_block_order_from(
                        src_function,
                        seed_block_id,
                        predecessor_counts,
                        trace_spans,
                        visited,
                        layout_positions,
                        layout_order,
                        &layout_count)) {
                    free(layout_order);
                    free(layout_positions);
                    free(predecessor_counts);
                    free(trace_spans);
                    free(visited);
                    machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-317: failed to extend deferred layout trace");
                    machine_layout_program_free(out_program);
                    return 0;
                }
            }
        }

        for (block_index = 0; block_index < src_function->block_count; ++block_index) {
            const MachineSelectBasicBlock *src_block = &src_function->blocks[layout_order ? layout_order[block_index] : block_index];

            dest_function->blocks[block_index].layout_index = block_index;
            dest_function->blocks[block_index].original_block_id = src_block->id;
            dest_function->blocks[block_index].op_count = src_block->op_count;
            dest_function->blocks[block_index].op_capacity = src_block->op_count;
            dest_function->blocks[block_index].has_terminator = src_block->has_terminator;
            if (src_block->op_count > 0) {
                size_t op_index;

                dest_function->blocks[block_index].ops =
                    (MachineLayoutOp *)calloc(src_block->op_count, sizeof(MachineLayoutOp));
                if (!dest_function->blocks[block_index].ops) {
                    free(layout_order);
                    free(layout_positions);
                    free(predecessor_counts);
                    free(trace_spans);
                    free(visited);
                    machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-313: out of memory copying block ops");
                    machine_layout_program_free(out_program);
                    return 0;
                }
                for (op_index = 0; op_index < src_block->op_count; ++op_index) {
                    if (!machine_layout_op_clone(
                            &dest_function->blocks[block_index].ops[op_index],
                            &src_block->ops[op_index])) {
                        free(layout_order);
                        free(layout_positions);
                        free(predecessor_counts);
                        free(trace_spans);
                        free(visited);
                        machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-314: out of memory cloning selected op");
                        machine_layout_program_free(out_program);
                        return 0;
                    }
                }
            }
        }

        for (block_index = 0; block_index < src_function->block_count; ++block_index) {
            const MachineSelectBasicBlock *src_block = &src_function->blocks[layout_order ? layout_order[block_index] : block_index];
            MachineLayoutBlock *dest_block = &dest_function->blocks[block_index];
            size_t next_layout_index = block_index + 1;

            switch (src_block->terminator.kind) {
                case MACHINE_SELECT_TERM_RETURN:
                    dest_block->terminator.kind = MACHINE_LAYOUT_TERM_RETURN;
                    dest_block->terminator.as.return_value = src_block->terminator.as.return_value;
                    break;
                case MACHINE_SELECT_TERM_RETURN_IMM:
                    dest_block->terminator.kind = MACHINE_LAYOUT_TERM_RETURN_IMM;
                    dest_block->terminator.as.return_value = src_block->terminator.as.return_value;
                    break;
                case MACHINE_SELECT_TERM_RETURN_SPILL:
                    dest_block->terminator.kind = MACHINE_LAYOUT_TERM_RETURN_SPILL;
                    dest_block->terminator.as.return_value = src_block->terminator.as.return_value;
                    break;
                case MACHINE_SELECT_TERM_JUMP: {
                    size_t target_index = machine_layout_find_layout_index_for_original_block(
                        dest_function,
                        src_block->terminator.as.jump_target);

                    if (target_index == (size_t)-1) {
                        machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-304: jump target not found");
                        machine_layout_program_free(out_program);
                        return 0;
                    }
                    if (next_layout_index < dest_function->block_count && target_index == next_layout_index) {
                        dest_block->terminator.kind = MACHINE_LAYOUT_TERM_FALLTHROUGH;
                        dest_block->terminator.as.fallthrough_target = target_index;
                    } else {
                        dest_block->terminator.kind = MACHINE_LAYOUT_TERM_JUMP;
                        dest_block->terminator.as.jump_target = target_index;
                    }
                    break;
                }
                case MACHINE_SELECT_TERM_BRANCH: {
                    size_t then_index = machine_layout_find_layout_index_for_original_block(
                        dest_function,
                        src_block->terminator.as.branch.then_target);
                    size_t else_index = machine_layout_find_layout_index_for_original_block(
                        dest_function,
                        src_block->terminator.as.branch.else_target);

                    if (then_index == (size_t)-1 || else_index == (size_t)-1) {
                        machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-305: branch target not found");
                        machine_layout_program_free(out_program);
                        return 0;
                    }
                    if (next_layout_index < dest_function->block_count && else_index == next_layout_index) {
                        dest_block->terminator.kind = MACHINE_LAYOUT_TERM_BRANCH_FALLTHROUGH;
                        dest_block->terminator.as.branch_fallthrough.condition =
                            src_block->terminator.as.branch.condition;
                        dest_block->terminator.as.branch_fallthrough.taken_target = then_index;
                        dest_block->terminator.as.branch_fallthrough.fallthrough_target = else_index;
                        dest_block->terminator.as.branch_fallthrough.branch_on_true = 1u;
                    } else if (next_layout_index < dest_function->block_count && then_index == next_layout_index) {
                        dest_block->terminator.kind = MACHINE_LAYOUT_TERM_BRANCH_FALLTHROUGH;
                        dest_block->terminator.as.branch_fallthrough.condition =
                            src_block->terminator.as.branch.condition;
                        dest_block->terminator.as.branch_fallthrough.taken_target = else_index;
                        dest_block->terminator.as.branch_fallthrough.fallthrough_target = then_index;
                        dest_block->terminator.as.branch_fallthrough.branch_on_true = 0u;
                    } else {
                        dest_block->terminator.kind = MACHINE_LAYOUT_TERM_BRANCH;
                        dest_block->terminator.as.branch.condition = src_block->terminator.as.branch.condition;
                        dest_block->terminator.as.branch.then_target = then_index;
                        dest_block->terminator.as.branch.else_target = else_index;
                    }
                    break;
                }
                case MACHINE_SELECT_TERM_COMPARE_BRANCH:
                case MACHINE_SELECT_TERM_COMPARE_BRANCH_IMM: {
                    size_t then_index = machine_layout_find_layout_index_for_original_block(
                        dest_function,
                        src_block->terminator.as.compare_branch.then_target);
                    size_t else_index = machine_layout_find_layout_index_for_original_block(
                        dest_function,
                        src_block->terminator.as.compare_branch.else_target);

                    if (then_index == (size_t)-1 || else_index == (size_t)-1) {
                        machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-306: compare-branch target not found");
                        machine_layout_program_free(out_program);
                        return 0;
                    }
                    if (next_layout_index < dest_function->block_count && else_index == next_layout_index) {
                        dest_block->terminator.kind =
                            src_block->terminator.kind == MACHINE_SELECT_TERM_COMPARE_BRANCH_IMM
                            ? MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM_FALLTHROUGH
                            : MACHINE_LAYOUT_TERM_COMPARE_BRANCH_FALLTHROUGH;
                        dest_block->terminator.as.compare_branch_fallthrough.op =
                            src_block->terminator.as.compare_branch.op;
                        dest_block->terminator.as.compare_branch_fallthrough.lhs =
                            src_block->terminator.as.compare_branch.lhs;
                        dest_block->terminator.as.compare_branch_fallthrough.rhs =
                            src_block->terminator.as.compare_branch.rhs;
                        dest_block->terminator.as.compare_branch_fallthrough.taken_target = then_index;
                        dest_block->terminator.as.compare_branch_fallthrough.fallthrough_target = else_index;
                        dest_block->terminator.as.compare_branch_fallthrough.branch_on_true = 1u;
                    } else if (next_layout_index < dest_function->block_count && then_index == next_layout_index) {
                        dest_block->terminator.kind =
                            src_block->terminator.kind == MACHINE_SELECT_TERM_COMPARE_BRANCH_IMM
                            ? MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM_FALLTHROUGH
                            : MACHINE_LAYOUT_TERM_COMPARE_BRANCH_FALLTHROUGH;
                        dest_block->terminator.as.compare_branch_fallthrough.op =
                            src_block->terminator.as.compare_branch.op;
                        dest_block->terminator.as.compare_branch_fallthrough.lhs =
                            src_block->terminator.as.compare_branch.lhs;
                        dest_block->terminator.as.compare_branch_fallthrough.rhs =
                            src_block->terminator.as.compare_branch.rhs;
                        dest_block->terminator.as.compare_branch_fallthrough.taken_target = else_index;
                        dest_block->terminator.as.compare_branch_fallthrough.fallthrough_target = then_index;
                        dest_block->terminator.as.compare_branch_fallthrough.branch_on_true = 0u;
                    } else {
                        dest_block->terminator.kind =
                            src_block->terminator.kind == MACHINE_SELECT_TERM_COMPARE_BRANCH_IMM
                            ? MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM
                            : MACHINE_LAYOUT_TERM_COMPARE_BRANCH;
                        dest_block->terminator.as.compare_branch.op = src_block->terminator.as.compare_branch.op;
                        dest_block->terminator.as.compare_branch.lhs = src_block->terminator.as.compare_branch.lhs;
                        dest_block->terminator.as.compare_branch.rhs = src_block->terminator.as.compare_branch.rhs;
                        dest_block->terminator.as.compare_branch.then_target = then_index;
                        dest_block->terminator.as.compare_branch.else_target = else_index;
                    }
                    break;
                }
                default:
                    machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-307: unknown source terminator kind");
                    machine_layout_program_free(out_program);
                    return 0;
            }
        }

        free(layout_order);
        free(layout_positions);
        free(predecessor_counts);
        free(trace_spans);
        free(visited);
    }

    return machine_layout_verify_program(out_program, error);
}

int machine_layout_lower_program_from_machine_ir(const MachineIrProgram *source,
    MachineLayoutProgram *out_program,
    MachineLayoutError *error) {
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok;

    if (!source || !out_program) {
        machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-308: invalid machine-ir bridge contract");
        return 0;
    }

    memset(&select_error, 0, sizeof(select_error));
    machine_select_program_init(&select_program);
    ok = machine_select_lower_canonicalized_program_from_machine_ir(source, &select_program, &select_error);
    if (!ok) {
        machine_layout_set_error(error, select_error.line, select_error.column, "%s", select_error.message);
        machine_select_program_free(&select_program);
        return 0;
    }
    ok = machine_layout_lower_program_from_machine_select(&select_program, out_program, error);
    machine_select_program_free(&select_program);
    return ok;
}

int machine_layout_function_compute_summary(const MachineLayoutFunction *function,
    MachineLayoutFunctionSummary *out_summary) {
    size_t block_index;

    if (!function || !out_summary) {
        return 0;
    }
    memset(out_summary, 0, sizeof(*out_summary));
    out_summary->block_count = function->block_count;

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        const MachineLayoutBlock *block = &function->blocks[block_index];

        out_summary->op_count += block->op_count;
        switch (block->terminator.kind) {
            case MACHINE_LAYOUT_TERM_RETURN:
                ++out_summary->return_count;
                break;
            case MACHINE_LAYOUT_TERM_RETURN_IMM:
                ++out_summary->return_imm_count;
                break;
            case MACHINE_LAYOUT_TERM_RETURN_SPILL:
                ++out_summary->return_spill_count;
                break;
            case MACHINE_LAYOUT_TERM_FALLTHROUGH:
                ++out_summary->fallthrough_count;
                break;
            case MACHINE_LAYOUT_TERM_JUMP:
                ++out_summary->jump_count;
                break;
            case MACHINE_LAYOUT_TERM_BRANCH:
                ++out_summary->branch_count;
                break;
            case MACHINE_LAYOUT_TERM_BRANCH_FALLTHROUGH:
                ++out_summary->branch_fallthrough_count;
                break;
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH:
                ++out_summary->compare_branch_count;
                break;
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM:
                ++out_summary->compare_branch_imm_count;
                break;
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_FALLTHROUGH:
                ++out_summary->compare_branch_fallthrough_count;
                break;
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM_FALLTHROUGH:
                ++out_summary->compare_branch_imm_fallthrough_count;
                break;
            default:
                break;
        }
    }
    return 1;
}

#endif
