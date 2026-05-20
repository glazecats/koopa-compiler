#if defined(__INTELLISENSE__) && !defined(MEMORY_SSA_PASS_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MEMORY_SSA_PASS_SPLIT_AGGREGATOR)
#define MEMORY_SSA_PASS_SPLIT_INTELLISENSE_CONTEXT
#include "memory_ssa_pass.c"
#else

static void memory_ssa_pass_set_error(ValueSsaError *error, int line, int column, const char *fmt, ...) {
    va_list args;

    if (!error || !fmt) {
        return;
    }

    error->line = line;
    error->column = column;
    va_start(args, fmt);
    vsnprintf(error->message, sizeof(error->message), fmt, args);
    va_end(args);
}

static void memory_ssa_pass_copy_memory_error(ValueSsaError *error, const MemorySsaError *memory_error) {
    if (!error || !memory_error) {
        return;
    }

    error->line = memory_error->line;
    error->column = memory_error->column;
    snprintf(error->message, sizeof(error->message), "%s", memory_error->message);
}

static int memory_ssa_pass_compact_removed_instructions(ValueSsaBasicBlock *block,
    const unsigned char *remove_flags) {
    size_t read_index;
    size_t write_index = 0;

    if (!block || !remove_flags) {
        return 0;
    }

    for (read_index = 0; read_index < block->instruction_count; ++read_index) {
        if (remove_flags[read_index]) {
            continue;
        }

        if (write_index != read_index) {
            block->instructions[write_index] = block->instructions[read_index];
        }
        ++write_index;
    }

    block->instruction_count = write_index;
    return 1;
}

static int memory_ssa_pass_append_instruction_ignoring_terminator(ValueSsaBasicBlock *block,
    const ValueSsaInstruction *instruction,
    ValueSsaError *error) {
    ValueSsaInstruction *instructions;
    ValueSsaInstruction *copy;
    size_t next_capacity;

    if (!block || !instruction) {
        memory_ssa_pass_set_error(error, 0, 0, "MEMORY-SSA-PASS-046: invalid trailing instruction append contract");
        return 0;
    }
    if (block->instruction_count == block->instruction_capacity) {
        next_capacity = block->instruction_capacity ? (block->instruction_capacity * 2) : 4;
        if (next_capacity < block->instruction_count ||
            next_capacity > ((size_t)-1) / sizeof(ValueSsaInstruction)) {
            memory_ssa_pass_set_error(error, 0, 0, "MEMORY-SSA-PASS-047: instruction table capacity overflow");
            return 0;
        }
        instructions = (ValueSsaInstruction *)realloc(
            block->instructions, next_capacity * sizeof(ValueSsaInstruction));
        if (!instructions) {
            memory_ssa_pass_set_error(error, 0, 0, "MEMORY-SSA-PASS-048: out of memory growing instruction table");
            return 0;
        }
        block->instructions = instructions;
        block->instruction_capacity = next_capacity;
    }

    copy = &block->instructions[block->instruction_count];
    memset(copy, 0, sizeof(*copy));
    memcpy(copy, instruction, sizeof(*copy));
    if (instruction->kind == VALUE_SSA_INSTR_CALL) {
        size_t arg_bytes = instruction->as.call.arg_count * sizeof(ValueSsaValueRef);

        copy->as.call.callee_name = NULL;
        copy->as.call.args = NULL;
        if (instruction->as.call.callee_name) {
            size_t name_len = strlen(instruction->as.call.callee_name) + 1;
            copy->as.call.callee_name = (char *)malloc(name_len);
            if (!copy->as.call.callee_name) {
                memory_ssa_pass_set_error(error, 0, 0, "MEMORY-SSA-PASS-049: out of memory copying call name");
                return 0;
            }
            memcpy(copy->as.call.callee_name, instruction->as.call.callee_name, name_len);
        }
        if (instruction->as.call.arg_count > 0) {
            copy->as.call.args = (ValueSsaValueRef *)malloc(arg_bytes);
            if (!copy->as.call.args) {
                free(copy->as.call.callee_name);
                copy->as.call.callee_name = NULL;
                memory_ssa_pass_set_error(error, 0, 0, "MEMORY-SSA-PASS-050: out of memory copying call args");
                return 0;
            }
            memcpy(copy->as.call.args, instruction->as.call.args, arg_bytes);
        }
    }

    ++block->instruction_count;
    return 1;
}

static int memory_ssa_pass_validate_alignment(const ValueSsaFunction *value_function,
    const MemorySsaFunction *memory_function,
    ValueSsaError *error) {
    size_t block_index;

    if (!value_function || !memory_function) {
        memory_ssa_pass_set_error(error, 0, 0, "MEMORY-SSA-PASS-001: invalid pass alignment contract");
        return 0;
    }
    if (value_function->block_count != memory_function->block_count) {
        memory_ssa_pass_set_error(error,
            0,
            0,
            "MEMORY-SSA-PASS-002: function '%s' block-count mismatch between value_ssa and memory_ssa",
            value_function->name ? value_function->name : "<anon>");
        return 0;
    }

    for (block_index = 0; block_index < value_function->block_count; ++block_index) {
        if (value_function->blocks[block_index].instruction_count !=
            memory_function->blocks[block_index].instruction_count) {
            memory_ssa_pass_set_error(error,
                0,
                0,
                "MEMORY-SSA-PASS-003: function '%s' bb.%zu instruction-count mismatch between value_ssa and memory_ssa",
                value_function->name ? value_function->name : "<anon>",
                block_index);
            return 0;
        }
    }

    return 1;
}

static int memory_ssa_pass_append_phi(ValueSsaBasicBlock *block,
    size_t result_id,
    const ValueSsaPhiInput *inputs,
    size_t input_count,
    ValueSsaError *error) {
    ValueSsaPhi *phis;
    ValueSsaPhi *phi;
    size_t next_capacity;

    if (!block || (input_count > 0 && !inputs)) {
        memory_ssa_pass_set_error(error, 0, 0, "MEMORY-SSA-PASS-033: invalid phi append contract");
        return 0;
    }

    if (block->phi_count == block->phi_capacity) {
        next_capacity = block->phi_capacity ? (block->phi_capacity * 2) : 4;
        if (next_capacity < block->phi_count || next_capacity > ((size_t)-1) / sizeof(ValueSsaPhi)) {
            memory_ssa_pass_set_error(error, 0, 0, "MEMORY-SSA-PASS-034: phi table capacity overflow");
            return 0;
        }
        phis = (ValueSsaPhi *)realloc(block->phis, next_capacity * sizeof(ValueSsaPhi));
        if (!phis) {
            memory_ssa_pass_set_error(error, 0, 0, "MEMORY-SSA-PASS-035: out of memory growing phi table");
            return 0;
        }
        block->phis = phis;
        block->phi_capacity = next_capacity;
    }

    phi = &block->phis[block->phi_count];
    phi->result_id = result_id;
    phi->inputs = NULL;
    phi->input_count = input_count;
    phi->input_capacity = input_count;
    if (input_count > 0) {
        phi->inputs = (ValueSsaPhiInput *)malloc(input_count * sizeof(ValueSsaPhiInput));
        if (!phi->inputs) {
            memory_ssa_pass_set_error(error, 0, 0, "MEMORY-SSA-PASS-036: out of memory copying phi inputs");
            return 0;
        }
        memcpy(phi->inputs, inputs, input_count * sizeof(ValueSsaPhiInput));
    }

    ++block->phi_count;
    return 1;
}

static int memory_ssa_pass_value_refs_equal(ValueSsaValueRef lhs, ValueSsaValueRef rhs) {
    if (lhs.kind != rhs.kind) {
        return 0;
    }

    switch (lhs.kind) {
    case VALUE_SSA_VALUE_IMMEDIATE:
        return lhs.immediate == rhs.immediate;
    case VALUE_SSA_VALUE_ID:
        return lhs.value_id == rhs.value_id;
    default:
        return 0;
    }
}

static int memory_ssa_pass_mark_natural_loop_blocks(const ValueSsaCfgAnalysis *cfg,
    size_t header_block_id,
    size_t backedge_pred_block_id,
    unsigned char *loop_marks,
    size_t *stack) {
    size_t stack_size = 0;
    size_t block_count;

    if (!cfg || !loop_marks || !stack || header_block_id >= cfg->block_count || backedge_pred_block_id >= cfg->block_count) {
        return 0;
    }

    block_count = cfg->block_count;
    loop_marks[header_block_id] = 1u;
    loop_marks[backedge_pred_block_id] = 1u;
    if (backedge_pred_block_id != header_block_id) {
        stack[stack_size++] = backedge_pred_block_id;
    }

    while (stack_size > 0) {
        size_t block_id = stack[--stack_size];
        size_t pred_block_id;

        for (pred_block_id = 0; pred_block_id < block_count; ++pred_block_id) {
            if (!cfg->reachable[pred_block_id] || !cfg->predecessor_matrix[pred_block_id * block_count + block_id]) {
                continue;
            }
            if (pred_block_id != header_block_id && !cfg->dominates[header_block_id * block_count + pred_block_id]) {
                continue;
            }
            if (loop_marks[pred_block_id]) {
                continue;
            }
            loop_marks[pred_block_id] = 1u;
            stack[stack_size++] = pred_block_id;
        }
    }

    return 1;
}

static int memory_ssa_pass_collect_blocks_in_any_loop(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *cfg,
    unsigned char *loop_marks,
    size_t *stack) {
    size_t header_block_id;
    size_t predecessor_block_id;

    if (!function || !cfg || !loop_marks || !stack || cfg->block_count != function->block_count) {
        return 0;
    }

    memset(loop_marks, 0, function->block_count * sizeof(unsigned char));
    for (header_block_id = 0; header_block_id < function->block_count; ++header_block_id) {
        for (predecessor_block_id = 0; predecessor_block_id < function->block_count; ++predecessor_block_id) {
            if (!cfg->predecessor_matrix[predecessor_block_id * function->block_count + header_block_id]) {
                continue;
            }
            if (!cfg->dominates[header_block_id * function->block_count + predecessor_block_id]) {
                continue;
            }
            if (!memory_ssa_pass_mark_natural_loop_blocks(
                    cfg, header_block_id, predecessor_block_id, loop_marks, stack)) {
                return 0;
            }
        }
    }

    return 1;
}

static int memory_ssa_pass_local_slot_has_loop_carried_store(const ValueSsaFunction *function, ValueSsaSlotRef slot) {
    ValueSsaCfgAnalysis cfg;
    unsigned char *loop_marks = NULL;
    size_t *stack = NULL;
    size_t block_id;
    int has_loop_carried_store = 0;

    if (!function || slot.kind != VALUE_SSA_SLOT_LOCAL || slot.id >= function->local_count || !function->has_body) {
        return 0;
    }

    value_ssa_cfg_analysis_init(&cfg);
    if (!value_ssa_compute_cfg_analysis(function, &cfg, NULL)) {
        value_ssa_cfg_analysis_free(&cfg);
        return 1;
    }
    loop_marks = (unsigned char *)calloc(function->block_count ? function->block_count : 1u, sizeof(unsigned char));
    stack = (size_t *)calloc(function->block_count ? function->block_count : 1u, sizeof(size_t));
    if (!loop_marks || !stack ||
        !memory_ssa_pass_collect_blocks_in_any_loop(function, &cfg, loop_marks, stack)) {
        free(loop_marks);
        free(stack);
        value_ssa_cfg_analysis_free(&cfg);
        return 1;
    }

    for (block_id = 0; block_id < function->block_count && !has_loop_carried_store; ++block_id) {
        const ValueSsaBasicBlock *block = &function->blocks[block_id];
        size_t instruction_index;

        if (!loop_marks[block_id]) {
            continue;
        }

        for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
            const ValueSsaInstruction *instruction = &block->instructions[instruction_index];

            if (instruction->kind == VALUE_SSA_INSTR_STORE_LOCAL &&
                instruction->as.store.slot.kind == VALUE_SSA_SLOT_LOCAL &&
                instruction->as.store.slot.id == slot.id) {
                has_loop_carried_store = 1;
                break;
            }
        }
    }

    free(loop_marks);
    free(stack);
    value_ssa_cfg_analysis_free(&cfg);
    return has_loop_carried_store;
}

static int memory_ssa_pass_value_depends_on_local_slot_recursive(const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *def_use,
    ValueSsaValueRef value,
    size_t slot_id,
    unsigned char *visit_state) {
    const ValueSsaInstruction *instruction;
    const ValueSsaPhi *phi;
    size_t block_id;
    size_t instruction_index;
    size_t input_index;

    if (!function || !def_use || !visit_state || slot_id >= function->local_count) {
        return 0;
    }
    if (value.kind != VALUE_SSA_VALUE_ID || value.value_id >= function->next_value_id) {
        return 0;
    }
    if (visit_state[value.value_id]) {
        return 0;
    }
    if (!def_use->has_def || !def_use->has_def[value.value_id]) {
        return 0;
    }

    visit_state[value.value_id] = 1u;
    block_id = def_use->def_block_ids[value.value_id];
    if (block_id >= function->block_count) {
        return 0;
    }

    if (def_use->def_instruction_indices[value.value_id] == (size_t)-1) {
        size_t phi_index = def_use->def_phi_indices[value.value_id];

        if (phi_index >= function->blocks[block_id].phi_count) {
            return 0;
        }
        phi = &function->blocks[block_id].phis[phi_index];
        for (input_index = 0; input_index < phi->input_count; ++input_index) {
            if (memory_ssa_pass_value_depends_on_local_slot_recursive(
                    function, def_use, phi->inputs[input_index].value, slot_id, visit_state)) {
                return 1;
            }
        }
        return 0;
    }

    instruction_index = def_use->def_instruction_indices[value.value_id];
    if (instruction_index >= function->blocks[block_id].instruction_count) {
        return 0;
    }
    instruction = &function->blocks[block_id].instructions[instruction_index];

    switch (instruction->kind) {
    case VALUE_SSA_INSTR_LOAD_LOCAL:
        return instruction->as.load_slot.kind == VALUE_SSA_SLOT_LOCAL &&
            instruction->as.load_slot.id == slot_id;
    case VALUE_SSA_INSTR_MOV:
        return memory_ssa_pass_value_depends_on_local_slot_recursive(
            function, def_use, instruction->as.mov_value, slot_id, visit_state);
    case VALUE_SSA_INSTR_BINARY:
        return memory_ssa_pass_value_depends_on_local_slot_recursive(
                   function, def_use, instruction->as.binary.lhs, slot_id, visit_state) ||
            memory_ssa_pass_value_depends_on_local_slot_recursive(
                function, def_use, instruction->as.binary.rhs, slot_id, visit_state);
    default:
        return 0;
    }
}

static int memory_ssa_pass_value_depends_on_local_slot(const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *def_use,
    ValueSsaValueRef value,
    size_t slot_id) {
    unsigned char *visit_state;
    int depends;

    if (!function || !def_use || slot_id >= function->local_count) {
        return 0;
    }

    visit_state = (unsigned char *)calloc(function->next_value_id ? function->next_value_id : 1u, sizeof(unsigned char));
    if (!visit_state) {
        return 1;
    }
    depends = memory_ssa_pass_value_depends_on_local_slot_recursive(function, def_use, value, slot_id, visit_state);
    free(visit_state);
    return depends;
}

static int memory_ssa_pass_local_slot_has_loop_sensitive_nontrivial_use(const ValueSsaFunction *function,
    ValueSsaSlotRef slot) {
    ValueSsaCfgAnalysis cfg;
    ValueSsaDefUseAnalysis def_use;
    unsigned char *loop_marks = NULL;
    size_t *stack = NULL;
    size_t block_id;
    int has_sensitive_use = 0;

    if (!function || slot.kind != VALUE_SSA_SLOT_LOCAL || slot.id >= function->local_count || !function->has_body) {
        return 0;
    }

    value_ssa_cfg_analysis_init(&cfg);
    value_ssa_def_use_analysis_init(&def_use);
    if (!value_ssa_compute_cfg_analysis(function, &cfg, NULL)) {
        value_ssa_cfg_analysis_free(&cfg);
        return 1;
    }
    if (!value_ssa_compute_def_use_analysis(function, &def_use, NULL)) {
        value_ssa_cfg_analysis_free(&cfg);
        value_ssa_def_use_analysis_free(&def_use);
        return 1;
    }
    loop_marks = (unsigned char *)calloc(function->block_count ? function->block_count : 1u, sizeof(unsigned char));
    stack = (size_t *)calloc(function->block_count ? function->block_count : 1u, sizeof(size_t));
    if (!loop_marks || !stack ||
        !memory_ssa_pass_collect_blocks_in_any_loop(function, &cfg, loop_marks, stack)) {
        free(loop_marks);
        free(stack);
        value_ssa_cfg_analysis_free(&cfg);
        value_ssa_def_use_analysis_free(&def_use);
        return 1;
    }

    for (block_id = 0; block_id < function->block_count && !has_sensitive_use; ++block_id) {
        const ValueSsaBasicBlock *block = &function->blocks[block_id];
        size_t instruction_index;

        if (!loop_marks[block_id]) {
            continue;
        }

        for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
            const ValueSsaInstruction *instruction = &block->instructions[instruction_index];

            switch (instruction->kind) {
            case VALUE_SSA_INSTR_CALL: {
                size_t arg_index;
                for (arg_index = 0; arg_index < instruction->as.call.arg_count; ++arg_index) {
                    ValueSsaValueRef arg = instruction->as.call.args[arg_index];
                    if (memory_ssa_pass_value_depends_on_local_slot(function, &def_use, arg, slot.id)) {
                        has_sensitive_use = 1;
                        break;
                    }
                }
                break;
            }
            case VALUE_SSA_INSTR_STORE_INDIRECT:
                if (memory_ssa_pass_value_depends_on_local_slot(
                        function, &def_use, instruction->as.store_indirect.addr, slot.id) ||
                    memory_ssa_pass_value_depends_on_local_slot(
                        function, &def_use, instruction->as.store_indirect.value, slot.id)) {
                    has_sensitive_use = 1;
                }
                break;
            case VALUE_SSA_INSTR_LOAD_INDIRECT:
                if (memory_ssa_pass_value_depends_on_local_slot(
                        function, &def_use, instruction->as.load_indirect_addr, slot.id)) {
                    has_sensitive_use = 1;
                }
                break;
            case VALUE_SSA_INSTR_BINARY:
                if (memory_ssa_pass_value_depends_on_local_slot(
                        function, &def_use, instruction->as.binary.lhs, slot.id) ||
                    memory_ssa_pass_value_depends_on_local_slot(
                        function, &def_use, instruction->as.binary.rhs, slot.id)) {
                    has_sensitive_use = 1;
                }
                break;
            default:
                break;
            }

            if (has_sensitive_use) {
                break;
            }
        }
    }

    free(loop_marks);
    free(stack);
    value_ssa_def_use_analysis_free(&def_use);
    value_ssa_cfg_analysis_free(&cfg);
    return has_sensitive_use;
}

static int memory_ssa_pass_local_slot_is_scalar_replaceable(const ValueSsaFunction *function, ValueSsaSlotRef slot) {
    if (!function || slot.kind != VALUE_SSA_SLOT_LOCAL || slot.id >= function->local_count) {
        return 0;
    }

    /*
     * Loop-carried scalar locals are the main remaining unsound corner for the
     * current slot-promotion/value-phi bridge. If a scalar local is updated in
     * a block that participates in a backedge cycle, keep it memory-backed
     * until the loop-carried proof becomes stronger.
     */
    if (memory_ssa_pass_local_slot_has_loop_carried_store(function, slot)) {
        return 0;
    }

    if (memory_ssa_pass_local_slot_has_loop_sensitive_nontrivial_use(function, slot)) {
        return 0;
    }

    /*
     * Local arrays are represented as contiguous local-slot families, while
     * `addr_local` only directly names the base slot. Per-slot memory-SSA
     * scalar replacement is therefore too weak to soundly rewrite later
     * element stores/loads reached through indirect addressing.
     */
    return function->locals[slot.id].array_rank == 0u;
}

static int memory_ssa_pass_program_has_extreme_local_scalar_replace_hotspot(const ValueSsaProgram *program) {
    size_t function_index;

    if (!program || !program->functions) {
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const ValueSsaFunction *function = &program->functions[function_index];
        size_t instruction_count = 0u;
        size_t block_index;
        int function_is_single_block_return = 0;
        int function_has_indirect_memory = 0;

        if (!function->has_body) {
            continue;
        }
        if (function->parameter_count > 512u) {
            return 1;
        }
        if (function->block_count == 1u && function->blocks) {
            const ValueSsaBasicBlock *block = &function->blocks[0];

            function_is_single_block_return =
                block->has_terminator && block->terminator.kind == VALUE_SSA_TERM_RETURN;
        }

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            const ValueSsaBasicBlock *block = &function->blocks[block_index];
            size_t instruction_index;

            instruction_count += block->instruction_count;
            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                switch (block->instructions[instruction_index].kind) {
                case VALUE_SSA_INSTR_ADDR_LOCAL:
                case VALUE_SSA_INSTR_ADDR_GLOBAL:
                case VALUE_SSA_INSTR_LOAD_INDIRECT:
                case VALUE_SSA_INSTR_STORE_INDIRECT:
                    function_has_indirect_memory = 1;
                    break;
                default:
                    break;
                }
                if (function_has_indirect_memory) {
                    break;
                }
            }
            if (function_has_indirect_memory) {
                break;
            }
        }

        if (function_is_single_block_return && !function_has_indirect_memory &&
            (function->next_value_id >= 12000u || instruction_count >= 12000u)) {
            return 1;
        }
    }

    return 0;
}

static int memory_ssa_pass_dump_program_copy(const ValueSsaProgram *program,
    char **out_text,
    ValueSsaError *error) {
    if (!out_text) {
        memory_ssa_pass_set_error(error, 0, 0, "MEMORY-SSA-PASS-015: invalid dump-copy contract");
        return 0;
    }

    *out_text = NULL;
    if (!value_ssa_dump_program(program, out_text)) {
        memory_ssa_pass_set_error(error, 0, 0, "MEMORY-SSA-PASS-016: failed to dump Value-SSA program");
        return 0;
    }

    return 1;
}

static unsigned long long memory_ssa_pass_text_hash(const char *text) {
    unsigned long long hash = 1469598103934665603ull;

    if (!text) {
        return 0ull;
    }

    while (*text) {
        hash ^= (unsigned char)*text;
        hash *= 1099511628211ull;
        ++text;
    }

    return hash;
}

static int memory_ssa_pass_run_one_step(const char *step_name,
    int (*pass_fn)(ValueSsaProgram *, ValueSsaError *),
    ValueSsaProgram *program,
    ValueSsaError *error) {
    int trace_timing = memory_ssa_pass_trace_timing_enabled();
    double step_start = trace_timing ? memory_ssa_pass_now_s() : 0.0;

    if (!step_name || !pass_fn || !program) {
        memory_ssa_pass_set_error(error, 0, 0, "MEMORY-SSA-PASS-017: invalid pipeline-step contract");
        return 0;
    }
    if (!pass_fn(program, error)) {
        return 0;
    }
    if (!value_ssa_verify_program(program, error)) {
        memory_ssa_pass_set_error(error,
            0,
            0,
            "MEMORY-SSA-PASS-018: pipeline step '%s' produced verifier-invalid Value-SSA: %s",
            step_name,
            error ? error->message : "");
        return 0;
    }
    if (trace_timing) {
        memory_ssa_pass_trace_timing(step_name, memory_ssa_pass_now_s() - step_start);
    }

    return 1;
}

#endif
