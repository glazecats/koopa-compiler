#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_SPLIT_AGGREGATOR)
#define VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa_pass.c"
#else

typedef struct {
    size_t header_id;
    size_t body_id;
    size_t preheader_id;
    size_t block_count;
    unsigned char *loop_marks;
} ValueSsaLicmSimpleLoop;

static void value_ssa_licm_simple_loop_init(ValueSsaLicmSimpleLoop *loop) {
    if (!loop) {
        return;
    }

    loop->header_id = (size_t)-1;
    loop->body_id = (size_t)-1;
    loop->preheader_id = (size_t)-1;
    loop->block_count = 0u;
    loop->loop_marks = NULL;
}

static void value_ssa_licm_simple_loop_free(ValueSsaLicmSimpleLoop *loop) {
    if (!loop) {
        return;
    }

    free(loop->loop_marks);
    loop->loop_marks = NULL;
    loop->block_count = 0u;
}

static int value_ssa_licm_mark_natural_loop_blocks(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *analysis,
    size_t header_id,
    size_t backedge_pred_id,
    unsigned char *loop_marks,
    size_t *stack) {
    size_t stack_size = 0u;
    size_t block_count;

    if (!function || !analysis || !loop_marks || !stack ||
        analysis->block_count != function->block_count ||
        header_id >= function->block_count ||
        backedge_pred_id >= function->block_count) {
        return 0;
    }

    block_count = function->block_count;
    memset(loop_marks, 0, block_count * sizeof(unsigned char));
    loop_marks[header_id] = 1u;
    loop_marks[backedge_pred_id] = 1u;
    if (backedge_pred_id != header_id) {
        stack[stack_size++] = backedge_pred_id;
    }

    while (stack_size > 0u) {
        size_t block_id = stack[--stack_size];
        size_t predecessor_id;

        for (predecessor_id = 0; predecessor_id < block_count; ++predecessor_id) {
            if (!analysis->reachable[predecessor_id] ||
                !analysis->predecessor_matrix[predecessor_id * block_count + block_id]) {
                continue;
            }
            if (predecessor_id != header_id &&
                !analysis->dominates[header_id * block_count + predecessor_id]) {
                continue;
            }
            if (loop_marks[predecessor_id]) {
                continue;
            }

            loop_marks[predecessor_id] = 1u;
            stack[stack_size++] = predecessor_id;
        }
    }

    return 1;
}

static int value_ssa_licm_loop_contains_block(const ValueSsaLicmSimpleLoop *loop, size_t block_id) {
    return loop &&
        loop->loop_marks &&
        block_id < loop->block_count &&
        loop->loop_marks[block_id];
}

static int value_ssa_licm_collect_readonly_scalar_globals(const ValueSsaProgram *program,
    unsigned char *readonly_flags) {
    size_t global_index;
    size_t function_index;

    if (!program || !readonly_flags) {
        return 0;
    }

    for (global_index = 0; global_index < program->global_count; ++global_index) {
        const ValueSsaGlobal *global = &program->globals[global_index];

        readonly_flags[global_index] =
            (unsigned char)(global->byte_size == 4u && !global->has_runtime_initializer);
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const ValueSsaFunction *function = &program->functions[function_index];
        size_t block_index;

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            const ValueSsaBasicBlock *block = &function->blocks[block_index];
            size_t instruction_index;

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                const ValueSsaInstruction *instruction = &block->instructions[instruction_index];

                if (instruction->kind == VALUE_SSA_INSTR_STORE_GLOBAL &&
                    instruction->as.store.slot.id < program->global_count) {
                    readonly_flags[instruction->as.store.slot.id] = 0u;
                } else if (instruction->kind == VALUE_SSA_INSTR_ADDR_GLOBAL &&
                           instruction->as.addr_slot.id < program->global_count) {
                    readonly_flags[instruction->as.addr_slot.id] = 0u;
                }
            }
        }
    }

    return 1;
}

static int value_ssa_licm_collect_address_taken_locals(const ValueSsaFunction *function,
    unsigned char *address_taken_locals) {
    size_t block_index;

    if (!function || !address_taken_locals) {
        return 0;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        const ValueSsaBasicBlock *block = &function->blocks[block_index];
        size_t instruction_index;

        for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
            const ValueSsaInstruction *instruction = &block->instructions[instruction_index];

            if (instruction->kind == VALUE_SSA_INSTR_ADDR_LOCAL &&
                instruction->as.addr_slot.id < function->local_count) {
                address_taken_locals[instruction->as.addr_slot.id] = 1u;
            }
        }
    }

    return 1;
}

static int value_ssa_licm_find_simple_loop(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *analysis,
    size_t header_id,
    ValueSsaLicmSimpleLoop *out_loop) {
    const ValueSsaBasicBlock *header;
    size_t predecessor_index;
    size_t predecessor_a = (size_t)-1;
    size_t predecessor_b = (size_t)-1;
    size_t body_id = (size_t)-1;
    size_t preheader_id = (size_t)-1;
    size_t *stack = NULL;
    unsigned char *loop_marks = NULL;

    if (!function || !analysis || !out_loop || header_id >= function->block_count) {
        return 0;
    }

    header = &function->blocks[header_id];
    if (header->phi_count != 0u ||
        !header->has_terminator ||
        header->terminator.kind != VALUE_SSA_TERM_BRANCH ||
        analysis->predecessor_counts[header_id] != 2u) {
        return 0;
    }

    for (predecessor_index = 0; predecessor_index < function->block_count; ++predecessor_index) {
        if (!analysis->predecessor_matrix[predecessor_index * function->block_count + header_id]) {
            continue;
        }
        if (predecessor_a == (size_t)-1) {
            predecessor_a = predecessor_index;
        } else if (predecessor_b == (size_t)-1) {
            predecessor_b = predecessor_index;
        } else {
            return 0;
        }
    }

    if (predecessor_a == (size_t)-1 || predecessor_b == (size_t)-1) {
        return 0;
    }

    if (function->blocks[predecessor_a].has_terminator &&
        function->blocks[predecessor_a].terminator.kind == VALUE_SSA_TERM_JUMP &&
        function->blocks[predecessor_a].terminator.as.jump_target == header_id &&
        analysis->dominates[header_id * function->block_count + predecessor_a]) {
        body_id = predecessor_a;
        preheader_id = predecessor_b;
    } else if (function->blocks[predecessor_b].has_terminator &&
               function->blocks[predecessor_b].terminator.kind == VALUE_SSA_TERM_JUMP &&
               function->blocks[predecessor_b].terminator.as.jump_target == header_id &&
               analysis->dominates[header_id * function->block_count + predecessor_b]) {
        body_id = predecessor_b;
        preheader_id = predecessor_a;
    } else {
        return 0;
    }

    if (body_id >= function->block_count || preheader_id >= function->block_count) {
        return 0;
    }

    if (function->blocks[body_id].phi_count != 0u ||
        function->blocks[preheader_id].phi_count != 0u ||
        !function->blocks[body_id].has_terminator ||
        function->blocks[body_id].terminator.kind != VALUE_SSA_TERM_JUMP ||
        function->blocks[body_id].terminator.as.jump_target != header_id ||
        !function->blocks[preheader_id].has_terminator ||
        function->blocks[preheader_id].terminator.kind != VALUE_SSA_TERM_JUMP ||
        function->blocks[preheader_id].terminator.as.jump_target != header_id) {
        return 0;
    }

    if ((header->terminator.as.branch.then_target != body_id &&
            header->terminator.as.branch.else_target != body_id) ||
        header->terminator.as.branch.then_target == header->terminator.as.branch.else_target) {
        return 0;
    }

    loop_marks = (unsigned char *)calloc(function->block_count ? function->block_count : 1u, sizeof(unsigned char));
    stack = (size_t *)malloc((function->block_count ? function->block_count : 1u) * sizeof(size_t));
    if (!loop_marks || !stack ||
        !value_ssa_licm_mark_natural_loop_blocks(function, analysis, header_id, body_id, loop_marks, stack)) {
        free(stack);
        free(loop_marks);
        return 0;
    }
    free(stack);

    out_loop->header_id = header_id;
    out_loop->body_id = body_id;
    out_loop->preheader_id = preheader_id;
    out_loop->block_count = function->block_count;
    out_loop->loop_marks = loop_marks;
    return 1;
}

static int value_ssa_licm_loop_preserves_local_slot(const ValueSsaFunction *function,
    const ValueSsaLicmSimpleLoop *loop,
    size_t slot_id) {
    size_t block_id;

    if (!function || !loop || slot_id >= function->local_count) {
        return 0;
    }

    for (block_id = 0; block_id < function->block_count; ++block_id) {
        const ValueSsaBasicBlock *block;
        size_t instruction_index;

        if (!value_ssa_licm_loop_contains_block(loop, block_id)) {
            continue;
        }
        block = &function->blocks[block_id];
        for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
            const ValueSsaInstruction *instruction = &block->instructions[instruction_index];

            if (instruction->kind == VALUE_SSA_INSTR_STORE_LOCAL &&
                instruction->as.store.slot.kind == VALUE_SSA_SLOT_LOCAL &&
                instruction->as.store.slot.id == slot_id) {
                return 0;
            }
        }
    }

    return 1;
}

static int value_ssa_licm_loop_preserves_global_slot(const unsigned char *readonly_globals,
    size_t slot_id) {
    return readonly_globals && readonly_globals[slot_id];
}

static int value_ssa_licm_address_roots_proven_non_alias(const ValueSsaPassAddressRoot *lhs,
    const ValueSsaPassAddressRoot *rhs) {
    if (!lhs || !rhs ||
        lhs->kind == VALUE_SSA_PASS_ADDRESS_ROOT_UNKNOWN ||
        rhs->kind == VALUE_SSA_PASS_ADDRESS_ROOT_UNKNOWN) {
        return 0;
    }

    if (lhs->kind == VALUE_SSA_PASS_ADDRESS_ROOT_LOCAL_SLOT &&
        rhs->kind == VALUE_SSA_PASS_ADDRESS_ROOT_LOCAL_SLOT) {
        return lhs->id != rhs->id;
    }
    if (lhs->kind == VALUE_SSA_PASS_ADDRESS_ROOT_GLOBAL_SLOT &&
        rhs->kind == VALUE_SSA_PASS_ADDRESS_ROOT_GLOBAL_SLOT) {
        return lhs->id != rhs->id;
    }
    if ((lhs->kind == VALUE_SSA_PASS_ADDRESS_ROOT_LOCAL_SLOT &&
            rhs->kind == VALUE_SSA_PASS_ADDRESS_ROOT_GLOBAL_SLOT) ||
        (lhs->kind == VALUE_SSA_PASS_ADDRESS_ROOT_GLOBAL_SLOT &&
            rhs->kind == VALUE_SSA_PASS_ADDRESS_ROOT_LOCAL_SLOT)) {
        return 1;
    }
    if ((lhs->kind == VALUE_SSA_PASS_ADDRESS_ROOT_LOCAL_SLOT &&
            rhs->kind == VALUE_SSA_PASS_ADDRESS_ROOT_PARAMETER_POINTER) ||
        (lhs->kind == VALUE_SSA_PASS_ADDRESS_ROOT_PARAMETER_POINTER &&
            rhs->kind == VALUE_SSA_PASS_ADDRESS_ROOT_LOCAL_SLOT)) {
        return 1;
    }

    return 0;
}

static int value_ssa_licm_instruction_may_alias_loop_load(const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    const ValueSsaPassAddressRoot *load_root,
    ValueSsaValueRef load_addr,
    const ValueSsaInstruction *instruction) {
    ValueSsaPassAddressRoot store_root;

    (void)load_addr;

    if (!function || !analysis || !instruction || !load_root) {
        return 1;
    }

    switch (instruction->kind) {
    case VALUE_SSA_INSTR_CALL:
        return 1;
    case VALUE_SSA_INSTR_STORE_LOCAL:
        if (load_root->kind == VALUE_SSA_PASS_ADDRESS_ROOT_UNKNOWN) {
            return 1;
        }
        if (load_root->kind == VALUE_SSA_PASS_ADDRESS_ROOT_LOCAL_SLOT) {
            if (instruction->as.store.slot.kind != VALUE_SSA_SLOT_LOCAL ||
                instruction->as.store.slot.id >= function->local_count) {
                return 1;
            }
            if (instruction->as.store.slot.id == load_root->id) {
                return 1;
            }
            return function->locals[instruction->as.store.slot.id].array_rank != 0u;
        }
        if (load_root->kind == VALUE_SSA_PASS_ADDRESS_ROOT_PARAMETER_POINTER) {
            return 1;
        }
        return 0;
    case VALUE_SSA_INSTR_STORE_GLOBAL:
        if (instruction->as.store.slot.kind != VALUE_SSA_SLOT_GLOBAL) {
            return 1;
        }
        if (load_root->kind == VALUE_SSA_PASS_ADDRESS_ROOT_GLOBAL_SLOT) {
            return instruction->as.store.slot.id == load_root->id;
        }
        if (load_root->kind == VALUE_SSA_PASS_ADDRESS_ROOT_LOCAL_SLOT) {
            return 0;
        }
        return 1;
    case VALUE_SSA_INSTR_STORE_INDIRECT:
        if (!value_ssa_pass_extract_address_root(function, analysis, instruction->as.store_indirect.addr, &store_root, 0u)) {
            return 1;
        }
        return !value_ssa_licm_address_roots_proven_non_alias(load_root, &store_root);
    default:
        return 0;
    }
}

static int value_ssa_licm_loop_has_aliasing_store_for_load(const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    const ValueSsaLicmSimpleLoop *loop,
    ValueSsaValueRef load_addr) {
    ValueSsaPassAddressRoot load_root;
    size_t block_id;

    if (!function || !analysis || !loop) {
        return 1;
    }

    if (!value_ssa_pass_extract_address_root(function, analysis, load_addr, &load_root, 0u)) {
        load_root.kind = VALUE_SSA_PASS_ADDRESS_ROOT_UNKNOWN;
        load_root.id = 0u;
    }

    for (block_id = 0; block_id < function->block_count; ++block_id) {
        const ValueSsaBasicBlock *block;
        size_t instruction_index;

        if (!value_ssa_licm_loop_contains_block(loop, block_id)) {
            continue;
        }
        block = &function->blocks[block_id];
        for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
            const ValueSsaInstruction *instruction = &block->instructions[instruction_index];

            if (value_ssa_licm_instruction_may_alias_loop_load(function, analysis, &load_root, load_addr, instruction)) {
                return 1;
            }
        }
    }

    return 0;
}

static int value_ssa_licm_value_is_loop_invariant(const ValueSsaProgram *program,
    ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    const unsigned char *readonly_globals,
    const unsigned char *address_taken_locals,
    const ValueSsaLicmSimpleLoop *loop,
    ValueSsaValueRef value,
    unsigned depth) {
    ValueSsaInstruction *definition;

    if (!program || !function || !analysis || !readonly_globals || !loop) {
        return 0;
    }
    if (value.kind != VALUE_SSA_VALUE_ID) {
        return 1;
    }
    if (depth > 12u) {
        return 0;
    }
    if (value.value_id >= analysis->value_count || !analysis->has_def[value.value_id]) {
        return 0;
    }
    if (!value_ssa_licm_loop_contains_block(loop, analysis->def_block_ids[value.value_id])) {
        return 1;
    }

    definition = value_ssa_pass_lookup_instruction_definition(function, analysis, value.value_id);
    if (!definition || definition->kind == VALUE_SSA_INSTR_CALL) {
        return 0;
    }

    switch (definition->kind) {
    case VALUE_SSA_INSTR_ADDR_LOCAL:
    case VALUE_SSA_INSTR_ADDR_GLOBAL:
        return 1;
    case VALUE_SSA_INSTR_LOAD_LOCAL:
        return definition->as.load_slot.kind == VALUE_SSA_SLOT_LOCAL &&
            definition->as.load_slot.id < function->local_count &&
            value_ssa_licm_loop_preserves_local_slot(function, loop, definition->as.load_slot.id) &&
            (!address_taken_locals || !address_taken_locals[definition->as.load_slot.id]);
    case VALUE_SSA_INSTR_LOAD_GLOBAL:
        return definition->as.load_slot.kind == VALUE_SSA_SLOT_GLOBAL &&
            definition->as.load_slot.id < program->global_count &&
            value_ssa_licm_loop_preserves_global_slot(
                readonly_globals,
                definition->as.load_slot.id);
    case VALUE_SSA_INSTR_LOAD_INDIRECT:
        return value_ssa_licm_value_is_loop_invariant(
                   program,
                   function,
                   analysis,
                   readonly_globals,
                   address_taken_locals,
                   loop,
                   definition->as.load_indirect_addr,
                   depth + 1u) &&
            !value_ssa_licm_loop_has_aliasing_store_for_load(
                function,
                analysis,
                loop,
                definition->as.load_indirect_addr);
    case VALUE_SSA_INSTR_MOV:
        return value_ssa_licm_value_is_loop_invariant(
            program, function, analysis, readonly_globals, address_taken_locals, loop, definition->as.mov_value, depth + 1u);
    case VALUE_SSA_INSTR_BINARY:
        return value_ssa_licm_value_is_loop_invariant(
                   program,
                   function,
                   analysis,
                   readonly_globals,
                   address_taken_locals,
                   loop,
                   definition->as.binary.lhs,
                   depth + 1u) &&
            value_ssa_licm_value_is_loop_invariant(
                   program,
                   function,
                   analysis,
                   readonly_globals,
                   address_taken_locals,
                   loop,
                   definition->as.binary.rhs,
                   depth + 1u);
    default:
        return 0;
    }
}

static int value_ssa_licm_clone_loop_invariant_value_into_preheader(const ValueSsaProgram *program,
    ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    const unsigned char *readonly_globals,
    const unsigned char *address_taken_locals,
    const ValueSsaLicmSimpleLoop *loop,
    ValueSsaValueRef value,
    unsigned char *has_mapping,
    ValueSsaValueRef *mappings,
    ValueSsaBasicBlock *rebuilt_preheader,
    ValueSsaValueRef *out_value,
    ValueSsaError *error) {
    ValueSsaInstruction *definition;
    ValueSsaInstruction cloned_instruction;

    if (!out_value) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-400: invalid LICM clone output");
        return 0;
    }

    if (value.kind != VALUE_SSA_VALUE_ID) {
        *out_value = value;
        return 1;
    }
    if (value.value_id < function->next_value_id && has_mapping[value.value_id]) {
        *out_value = mappings[value.value_id];
        return 1;
    }

    definition = value_ssa_pass_lookup_instruction_definition(function, analysis, value.value_id);
    if (!definition) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-401: missing LICM definition for ssa.%zu", value.value_id);
        return 0;
    }
    if (!value_ssa_licm_loop_contains_block(loop, analysis->def_block_ids[value.value_id])) {
        *out_value = value;
        return 1;
    }
    if (!value_ssa_licm_value_is_loop_invariant(
            program, function, analysis, readonly_globals, address_taken_locals, loop, value, 0u)) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-402: attempted to clone non-invariant loop value ssa.%zu", value.value_id);
        return 0;
    }

    memset(&cloned_instruction, 0, sizeof(cloned_instruction));
    cloned_instruction.kind = definition->kind;
    cloned_instruction.has_result = 1;
    cloned_instruction.result = value_ssa_value_id(value_ssa_function_allocate_value(function));
    if (cloned_instruction.result.value_id == (size_t)-1) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-403: failed to allocate LICM hoisted value");
        return 0;
    }

    switch (definition->kind) {
    case VALUE_SSA_INSTR_ADDR_LOCAL:
    case VALUE_SSA_INSTR_ADDR_GLOBAL:
        cloned_instruction.as.addr_slot = definition->as.addr_slot;
        break;
    case VALUE_SSA_INSTR_LOAD_LOCAL:
        cloned_instruction.as.load_slot = definition->as.load_slot;
        break;
    case VALUE_SSA_INSTR_LOAD_GLOBAL:
        cloned_instruction.as.load_slot = definition->as.load_slot;
        break;
    case VALUE_SSA_INSTR_LOAD_INDIRECT:
        if (!value_ssa_licm_clone_loop_invariant_value_into_preheader(
                program,
                function,
                analysis,
                readonly_globals,
                address_taken_locals,
                loop,
                definition->as.load_indirect_addr,
                has_mapping,
                mappings,
                rebuilt_preheader,
                &cloned_instruction.as.load_indirect_addr,
                error)) {
            return 0;
        }
        break;
    case VALUE_SSA_INSTR_MOV:
        if (!value_ssa_licm_clone_loop_invariant_value_into_preheader(
                program,
                function,
                analysis,
                readonly_globals,
                address_taken_locals,
                loop,
                definition->as.mov_value,
                has_mapping,
                mappings,
                rebuilt_preheader,
                &cloned_instruction.as.mov_value,
                error)) {
            return 0;
        }
        break;
    case VALUE_SSA_INSTR_BINARY:
        cloned_instruction.as.binary.op = definition->as.binary.op;
        if (!value_ssa_licm_clone_loop_invariant_value_into_preheader(
                program,
                function,
                analysis,
                readonly_globals,
                address_taken_locals,
                loop,
                definition->as.binary.lhs,
                has_mapping,
                mappings,
                rebuilt_preheader,
                &cloned_instruction.as.binary.lhs,
                error) ||
            !value_ssa_licm_clone_loop_invariant_value_into_preheader(
                program,
                function,
                analysis,
                readonly_globals,
                address_taken_locals,
                loop,
                definition->as.binary.rhs,
                has_mapping,
                mappings,
                rebuilt_preheader,
                &cloned_instruction.as.binary.rhs,
                error)) {
            return 0;
        }
        break;
    default:
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-404: unsupported LICM instruction kind %d", (int)definition->kind);
        return 0;
    }

    if (!value_ssa_block_append_instruction(rebuilt_preheader, &cloned_instruction, error)) {
        return 0;
    }

    has_mapping[value.value_id] = 1u;
    mappings[value.value_id] = cloned_instruction.result;
    *out_value = cloned_instruction.result;
    return 1;
}

int value_ssa_licm_hoist_simple_loop_invariant_values(ValueSsaProgram *program, ValueSsaError *error) {
    unsigned char *readonly_globals = NULL;
    size_t function_index;
    int changed_any = 0;

    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-405: invalid LICM contract");
        return 0;
    }

    readonly_globals = (unsigned char *)calloc(program->global_count ? program->global_count : 1u, sizeof(unsigned char));
    if (!readonly_globals) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-406: out of memory preparing LICM globals");
        return 0;
    }
    if (!value_ssa_licm_collect_readonly_scalar_globals(program, readonly_globals)) {
        free(readonly_globals);
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-407: failed collecting LICM globals");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunction *function = &program->functions[function_index];
        ValueSsaCfgAnalysis cfg;
        ValueSsaDefUseAnalysis def_use;
        unsigned char *address_taken_locals = NULL;
        unsigned char *already_hoisted_values = NULL;
        size_t header_id;
        int changed_this_function = 0;

        if (!function->has_body) {
            continue;
        }

        if (function->local_count > 0u) {
            address_taken_locals = (unsigned char *)calloc(function->local_count, sizeof(unsigned char));
            if (!address_taken_locals) {
                free(readonly_globals);
                value_ssa_set_error(error, 0, 0, "VALUE-SSA-409: out of memory preparing LICM locals");
                return 0;
            }
        }
        if (function->next_value_id > 0u) {
            already_hoisted_values =
                (unsigned char *)calloc(function->next_value_id, sizeof(unsigned char));
            if (!already_hoisted_values) {
                free(address_taken_locals);
                free(readonly_globals);
                value_ssa_set_error(error, 0, 0, "VALUE-SSA-411: out of memory preparing LICM hoist markers");
                return 0;
            }
        }
        if (address_taken_locals &&
            !value_ssa_licm_collect_address_taken_locals(function, address_taken_locals)) {
            free(address_taken_locals);
            free(already_hoisted_values);
            free(readonly_globals);
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-410: failed collecting LICM address-taken locals");
            return 0;
        }

        do {
            value_ssa_cfg_analysis_init(&cfg);
            value_ssa_def_use_analysis_init(&def_use);
            if (!value_ssa_compute_cfg_analysis(function, &cfg, error) ||
                !value_ssa_compute_def_use_analysis(function, &def_use, error)) {
                value_ssa_def_use_analysis_free(&def_use);
                value_ssa_cfg_analysis_free(&cfg);
                free(address_taken_locals);
                free(already_hoisted_values);
                free(readonly_globals);
                return 0;
            }

            changed_this_function = 0;

            for (header_id = 0; header_id < function->block_count; ++header_id) {
                ValueSsaLicmSimpleLoop loop;
                ValueSsaBasicBlock *preheader;
                ValueSsaBasicBlock *header;
                ValueSsaBasicBlock *body;
                ValueSsaBasicBlock rebuilt_preheader;
                ValueSsaBasicBlock rebuilt_header;
                ValueSsaBasicBlock rebuilt_body;
                unsigned char *has_mapping = NULL;
                ValueSsaValueRef *mappings = NULL;
                size_t instruction_index;
                int changed_loop = 0;
                int ok = 1;

                value_ssa_licm_simple_loop_init(&loop);
                if (!value_ssa_licm_find_simple_loop(function, &cfg, header_id, &loop)) {
                    continue;
                }

                preheader = &function->blocks[loop.preheader_id];
                header = &function->blocks[loop.header_id];
                body = &function->blocks[loop.body_id];
                if (preheader->instruction_count == 0u || body->instruction_count == 0u) {
                    value_ssa_licm_simple_loop_free(&loop);
                    continue;
                }

                memset(&rebuilt_preheader, 0, sizeof(rebuilt_preheader));
                memset(&rebuilt_header, 0, sizeof(rebuilt_header));
                memset(&rebuilt_body, 0, sizeof(rebuilt_body));
                rebuilt_preheader.id = preheader->id;
                rebuilt_header.id = header->id;
                rebuilt_body.id = body->id;

                has_mapping = (unsigned char *)calloc(function->next_value_id ? function->next_value_id : 1u,
                    sizeof(unsigned char));
                mappings = (ValueSsaValueRef *)calloc(function->next_value_id ? function->next_value_id : 1u,
                    sizeof(ValueSsaValueRef));
                if (!has_mapping || !mappings) {
                    free(mappings);
                    free(has_mapping);
                    value_ssa_basic_block_free(&rebuilt_preheader);
                    value_ssa_basic_block_free(&rebuilt_header);
                    value_ssa_basic_block_free(&rebuilt_body);
                    value_ssa_def_use_analysis_free(&def_use);
                    value_ssa_cfg_analysis_free(&cfg);
                    free(address_taken_locals);
                    free(already_hoisted_values);
                    free(readonly_globals);
                    value_ssa_set_error(error, 0, 0, "VALUE-SSA-408: out of memory preparing LICM mappings");
                    return 0;
                }

                for (instruction_index = 0; instruction_index < preheader->instruction_count; ++instruction_index) {
                    if (!value_ssa_block_append_instruction(&rebuilt_preheader, &preheader->instructions[instruction_index], error)) {
                        ok = 0;
                        break;
                    }
                }

                for (instruction_index = 0; ok && instruction_index < header->instruction_count; ++instruction_index) {
                    const ValueSsaInstruction *instruction = &header->instructions[instruction_index];
                    ValueSsaInstruction replacement;
                    ValueSsaValueRef hoisted_value;

                    if ((instruction->kind == VALUE_SSA_INSTR_ADDR_LOCAL ||
                            instruction->kind == VALUE_SSA_INSTR_ADDR_GLOBAL ||
                            instruction->kind == VALUE_SSA_INSTR_LOAD_LOCAL ||
                            instruction->kind == VALUE_SSA_INSTR_LOAD_GLOBAL ||
                            instruction->kind == VALUE_SSA_INSTR_LOAD_INDIRECT ||
                            instruction->kind == VALUE_SSA_INSTR_MOV ||
                            instruction->kind == VALUE_SSA_INSTR_BINARY) &&
                        instruction->has_result &&
                        instruction->result.kind == VALUE_SSA_VALUE_ID &&
                        !already_hoisted_values[instruction->result.value_id] &&
                        value_ssa_licm_value_is_loop_invariant(
                            program,
                            function,
                            &def_use,
                            readonly_globals,
                            address_taken_locals,
                            &loop,
                            instruction->result,
                            0u)) {
                        if (!value_ssa_licm_clone_loop_invariant_value_into_preheader(
                                program,
                                function,
                                &def_use,
                                readonly_globals,
                                address_taken_locals,
                                &loop,
                                instruction->result,
                                has_mapping,
                                mappings,
                                &rebuilt_preheader,
                                &hoisted_value,
                                error)) {
                            ok = 0;
                            break;
                        }

                        memset(&replacement, 0, sizeof(replacement));
                        replacement.kind = VALUE_SSA_INSTR_MOV;
                        replacement.has_result = 1;
                        replacement.result = instruction->result;
                        replacement.as.mov_value = hoisted_value;
                        if (!value_ssa_block_append_instruction(&rebuilt_header, &replacement, error)) {
                            ok = 0;
                            break;
                        }
                        already_hoisted_values[instruction->result.value_id] = 1u;
                        changed_loop = 1;
                        continue;
                    }

                    if (!value_ssa_block_append_instruction(&rebuilt_header, instruction, error)) {
                        ok = 0;
                        break;
                    }
                }

                for (instruction_index = 0; ok && instruction_index < body->instruction_count; ++instruction_index) {
                    const ValueSsaInstruction *instruction = &body->instructions[instruction_index];

                    if (!value_ssa_block_append_instruction(&rebuilt_body, instruction, error)) {
                        ok = 0;
                        break;
                    }
                }

                if (ok && changed_loop) {
                    rebuilt_preheader.has_terminator = preheader->has_terminator;
                    rebuilt_preheader.terminator = preheader->terminator;
                    rebuilt_header.has_terminator = header->has_terminator;
                    rebuilt_header.terminator = header->terminator;
                    rebuilt_body.has_terminator = body->has_terminator;
                    rebuilt_body.terminator = body->terminator;

                    value_ssa_basic_block_free(preheader);
                    *preheader = rebuilt_preheader;
                    memset(&rebuilt_preheader, 0, sizeof(rebuilt_preheader));

                    value_ssa_basic_block_free(header);
                    *header = rebuilt_header;
                    memset(&rebuilt_header, 0, sizeof(rebuilt_header));

                    value_ssa_basic_block_free(body);
                    *body = rebuilt_body;
                    memset(&rebuilt_body, 0, sizeof(rebuilt_body));

                    changed_any = 1;
                    changed_this_function = 1;
                }

                value_ssa_basic_block_free(&rebuilt_preheader);
                value_ssa_basic_block_free(&rebuilt_header);
                value_ssa_basic_block_free(&rebuilt_body);
                free(mappings);
                free(has_mapping);
                value_ssa_licm_simple_loop_free(&loop);

                if (!ok) {
                    value_ssa_def_use_analysis_free(&def_use);
                    value_ssa_cfg_analysis_free(&cfg);
                    free(address_taken_locals);
                    free(already_hoisted_values);
                    free(readonly_globals);
                    return 0;
                }
                if (changed_loop) {
                    break;
                }
            }

            value_ssa_def_use_analysis_free(&def_use);
            value_ssa_cfg_analysis_free(&cfg);
        } while (changed_this_function);

        free(address_taken_locals);
        free(already_hoisted_values);
    }

    free(readonly_globals);

    if (changed_any &&
        (!value_ssa_simplify_trivial_values(program, error) ||
            !value_ssa_eliminate_dead_value_defs(program, error) ||
            !value_ssa_simplify_cfg(program, error))) {
        return 0;
    }

    return value_ssa_verify_program(program, error);
}

#endif
