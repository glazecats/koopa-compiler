#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_SPLIT_AGGREGATOR)
#define VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa_pass.c"
#else

static int value_ssa_lower_ir_is_extreme_straight_line_hotspot(const LowerIrProgram *program) {
    size_t function_index;
    int found_hotspot = 0;

    if (!program || !program->functions) {
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const LowerIrFunction *function = &program->functions[function_index];
        const LowerIrBasicBlock *block = NULL;
        size_t instruction_count = 0;
        size_t block_index;
        int function_is_single_block_return = 0;
        int function_has_indirect_memory = 0;

        if (!function->has_body) {
            continue;
        }
        if (function->block_count == 1u && function->blocks) {
            block = &function->blocks[0];
            function_is_single_block_return =
                block->has_terminator && block->terminator.kind == LOWER_IR_TERM_RETURN;
        }

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            const LowerIrBasicBlock *scan_block = &function->blocks[block_index];
            size_t instruction_index;

            instruction_count += scan_block->instruction_count;
            for (instruction_index = 0; instruction_index < scan_block->instruction_count; ++instruction_index) {
                switch (scan_block->instructions[instruction_index].kind) {
                case LOWER_IR_INSTR_ADDR_LOCAL:
                case LOWER_IR_INSTR_ADDR_GLOBAL:
                case LOWER_IR_INSTR_LOAD_INDIRECT:
                case LOWER_IR_INSTR_STORE_INDIRECT:
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

        if (!function_is_single_block_return || function_has_indirect_memory) {
            continue;
        }

        if (function->parameter_count > 512u ||
            function->next_temp_id >= 12000u ||
            instruction_count >= 12000u) {
            found_hotspot = 1;
        }
    }

    return found_hotspot;
}

static int value_ssa_lower_ir_uses_indirect_memory(const LowerIrProgram *program) {
    size_t function_index;

    if (!program) {
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const LowerIrFunction *function = &program->functions[function_index];
        size_t block_index;

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            const LowerIrBasicBlock *block = &function->blocks[block_index];
            size_t instruction_index;

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                switch (block->instructions[instruction_index].kind) {
                case LOWER_IR_INSTR_ADDR_LOCAL:
                case LOWER_IR_INSTR_ADDR_GLOBAL:
                case LOWER_IR_INSTR_LOAD_INDIRECT:
                case LOWER_IR_INSTR_STORE_INDIRECT:
                    return 1;
                default:
                    break;
                }
            }
        }
    }

    return 0;
}

static int value_ssa_build_from_lower_ir_with_bridge(const LowerIrProgram *program,
    ValueSsaProgram *out_program,
    int (*bridge_fn)(const LowerIrProgram *, ValueSsaProgram *, ValueSsaError *),
    const char *invalid_error_code,
    ValueSsaError *error) {
    if (!program || !out_program) {
        value_ssa_set_error(error,
            0,
            0,
            "%s",
            invalid_error_code ? invalid_error_code : "VALUE-SSA-173: invalid lower-ir bridge contract");
        return 0;
    }
    if (!bridge_fn) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-175: invalid lower-ir bridge callback");
        return 0;
    }

    return bridge_fn(program, out_program, error);
}

static ValueSsaInstruction *value_ssa_bridge_lookup_instruction_definition(ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    size_t value_id) {
    size_t block_id;
    size_t instruction_index;

    if (!function || !analysis || value_id >= analysis->value_count || !analysis->has_def[value_id]) {
        return NULL;
    }
    instruction_index = analysis->def_instruction_indices[value_id];
    if (instruction_index == (size_t)-1) {
        return NULL;
    }
    block_id = analysis->def_block_ids[value_id];
    if (block_id >= function->block_count || instruction_index >= function->blocks[block_id].instruction_count) {
        return NULL;
    }
    return &function->blocks[block_id].instructions[instruction_index];
}

typedef enum {
    VALUE_SSA_ADDRESS_ROOT_UNKNOWN = 0,
    VALUE_SSA_ADDRESS_ROOT_LOCAL_SLOT,
    VALUE_SSA_ADDRESS_ROOT_GLOBAL_SLOT,
    VALUE_SSA_ADDRESS_ROOT_PARAMETER_POINTER,
} ValueSsaAddressRootKind;

typedef struct {
    ValueSsaAddressRootKind kind;
    size_t id;
} ValueSsaAddressRoot;

static const ValueSsaFunction *value_ssa_bridge_find_function_by_name(const ValueSsaProgram *program,
    const char *name) {
    size_t function_index;

    if (!program || !name || name[0] == '\0') {
        return NULL;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const ValueSsaFunction *function = &program->functions[function_index];

        if (function->name && strcmp(function->name, name) == 0) {
            return function;
        }
    }

    return NULL;
}

static int value_ssa_bridge_collect_readonly_scalar_globals(const ValueSsaProgram *program,
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

static int value_ssa_bridge_function_is_reusable_pure_internal_call_target(const ValueSsaProgram *program,
    const ValueSsaFunction *function,
    const unsigned char *readonly_globals) {
    size_t block_index;

    if (!program || !function || !function->has_body || !readonly_globals) {
        return 0;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        const ValueSsaBasicBlock *block = &function->blocks[block_index];
        size_t instruction_index;

        for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
            const ValueSsaInstruction *instruction = &block->instructions[instruction_index];

            switch (instruction->kind) {
            case VALUE_SSA_INSTR_MOV:
            case VALUE_SSA_INSTR_BINARY:
            case VALUE_SSA_INSTR_LOAD_LOCAL:
            case VALUE_SSA_INSTR_STORE_LOCAL:
                break;
            case VALUE_SSA_INSTR_LOAD_GLOBAL:
                if (instruction->as.load_slot.id >= program->global_count ||
                    !readonly_globals[instruction->as.load_slot.id]) {
                    return 0;
                }
                break;
            default:
                return 0;
            }
        }
    }

    return 1;
}

static int value_ssa_bridge_same_call_args(const ValueSsaInstruction *lhs, const ValueSsaInstruction *rhs) {
    size_t arg_index;

    if (!lhs || !rhs || lhs->kind != VALUE_SSA_INSTR_CALL || rhs->kind != VALUE_SSA_INSTR_CALL ||
        lhs->as.call.arg_count != rhs->as.call.arg_count) {
        return 0;
    }

    for (arg_index = 0; arg_index < lhs->as.call.arg_count; ++arg_index) {
        if (!value_ssa_value_refs_equal(lhs->as.call.args[arg_index], rhs->as.call.args[arg_index])) {
            return 0;
        }
    }

    return 1;
}

static int value_ssa_bridge_instruction_blocks_pure_internal_call_reuse(const ValueSsaInstruction *instruction) {
    if (!instruction) {
        return 1;
    }

    switch (instruction->kind) {
    case VALUE_SSA_INSTR_CALL:
        return 1;
    default:
        return 0;
    }
}

static int value_ssa_bridge_reuse_repeated_pure_internal_calls_in_program(ValueSsaProgram *program,
    ValueSsaError *error) {
    unsigned char *readonly_globals = NULL;
    size_t function_index;
    int changed = 0;

    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-271: invalid pure-call reuse contract");
        return 0;
    }

    readonly_globals = (unsigned char *)calloc(program->global_count ? program->global_count : 1u, sizeof(unsigned char));
    if (!readonly_globals) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-272: out of memory preparing pure-call reuse globals");
        return 0;
    }
    if (!value_ssa_bridge_collect_readonly_scalar_globals(program, readonly_globals)) {
        free(readonly_globals);
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-273: failed collecting pure-call reuse globals");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunction *function = &program->functions[function_index];
        size_t block_index;

        if (!function->has_body) {
            continue;
        }

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            ValueSsaBasicBlock *block = &function->blocks[block_index];
            size_t instruction_index;

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                ValueSsaInstruction *instruction = &block->instructions[instruction_index];
                const ValueSsaFunction *callee = NULL;
                size_t candidate_index;

                if (instruction->kind != VALUE_SSA_INSTR_CALL ||
                    !instruction->has_result ||
                    instruction->result.kind != VALUE_SSA_VALUE_ID ||
                    !instruction->as.call.callee_name) {
                    continue;
                }

                callee = value_ssa_bridge_find_function_by_name(program, instruction->as.call.callee_name);
                if (!callee ||
                    !value_ssa_bridge_function_is_reusable_pure_internal_call_target(
                        program, callee, readonly_globals)) {
                    continue;
                }

                for (candidate_index = 0; candidate_index < instruction_index; ++candidate_index) {
                    ValueSsaInstruction *candidate = &block->instructions[candidate_index];
                    size_t middle_index;
                    int blocked = 0;

                    if (candidate->kind != VALUE_SSA_INSTR_CALL ||
                        !candidate->has_result ||
                        candidate->result.kind != VALUE_SSA_VALUE_ID ||
                        !candidate->as.call.callee_name ||
                        strcmp(candidate->as.call.callee_name, instruction->as.call.callee_name) != 0 ||
                        !value_ssa_bridge_same_call_args(candidate, instruction)) {
                        continue;
                    }

                    for (middle_index = candidate_index + 1u; middle_index < instruction_index; ++middle_index) {
                        if (value_ssa_bridge_instruction_blocks_pure_internal_call_reuse(
                                &block->instructions[middle_index])) {
                            blocked = 1;
                            break;
                        }
                    }
                    if (blocked) {
                        continue;
                    }

                    instruction->kind = VALUE_SSA_INSTR_MOV;
                    instruction->as.mov_value = candidate->result;
                    changed = 1;
                    break;
                }
            }
        }
    }

    free(readonly_globals);

    if (changed &&
        (!value_ssa_simplify_trivial_values(program, error) ||
            !value_ssa_eliminate_dead_value_defs(program, error))) {
        return 0;
    }

    return value_ssa_verify_program(program, error);
}

static int value_ssa_bridge_reuse_addr_roots_in_program(ValueSsaProgram *program, ValueSsaError *error) {
    size_t function_index;

    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-268: invalid addr-root reuse contract");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunction *function = &program->functions[function_index];
        size_t block_index;

        if (!function->has_body) {
            continue;
        }

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            ValueSsaBasicBlock *block = &function->blocks[block_index];
            unsigned char *seen_locals = NULL;
            unsigned char *seen_globals = NULL;
            ValueSsaValueRef *local_values = NULL;
            ValueSsaValueRef *global_values = NULL;
            size_t instruction_index;

            if (block->instruction_count == 0u) {
                continue;
            }

            if (function->local_count > 0u) {
                seen_locals = (unsigned char *)calloc(function->local_count, sizeof(unsigned char));
                local_values = (ValueSsaValueRef *)calloc(function->local_count, sizeof(ValueSsaValueRef));
                if (!seen_locals || !local_values) {
                    free(local_values);
                    free(seen_locals);
                    free(global_values);
                    free(seen_globals);
                    value_ssa_set_error(error, 0, 0, "VALUE-SSA-269: out of memory preparing local addr-root reuse");
                    return 0;
                }
            }
            if (program->global_count > 0u) {
                seen_globals = (unsigned char *)calloc(program->global_count, sizeof(unsigned char));
                global_values = (ValueSsaValueRef *)calloc(program->global_count, sizeof(ValueSsaValueRef));
                if (!seen_globals || !global_values) {
                    free(local_values);
                    free(seen_locals);
                    free(global_values);
                    free(seen_globals);
                    value_ssa_set_error(error, 0, 0, "VALUE-SSA-270: out of memory preparing global addr-root reuse");
                    return 0;
                }
            }

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                ValueSsaInstruction *instruction = &block->instructions[instruction_index];

                if (!instruction->has_result || instruction->result.kind != VALUE_SSA_VALUE_ID) {
                    continue;
                }

                if (instruction->kind == VALUE_SSA_INSTR_ADDR_LOCAL &&
                    instruction->as.addr_slot.id < function->local_count) {
                    if (seen_locals[instruction->as.addr_slot.id]) {
                        instruction->kind = VALUE_SSA_INSTR_MOV;
                        instruction->as.mov_value = local_values[instruction->as.addr_slot.id];
                    } else {
                        seen_locals[instruction->as.addr_slot.id] = 1u;
                        local_values[instruction->as.addr_slot.id] = instruction->result;
                    }
                } else if (instruction->kind == VALUE_SSA_INSTR_ADDR_GLOBAL &&
                           instruction->as.addr_slot.id < program->global_count) {
                    if (seen_globals[instruction->as.addr_slot.id]) {
                        instruction->kind = VALUE_SSA_INSTR_MOV;
                        instruction->as.mov_value = global_values[instruction->as.addr_slot.id];
                    } else {
                        seen_globals[instruction->as.addr_slot.id] = 1u;
                        global_values[instruction->as.addr_slot.id] = instruction->result;
                    }
                }
            }

            free(local_values);
            free(seen_locals);
            free(global_values);
            free(seen_globals);
        }
    }

    return value_ssa_verify_program(program, error);
}

static int value_ssa_bridge_same_pure_address_value(ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    ValueSsaValueRef lhs,
    ValueSsaValueRef rhs,
    unsigned depth) {
    ValueSsaInstruction *lhs_def;
    ValueSsaInstruction *rhs_def;

    if (depth > 8u) {
        return 0;
    }
    if (value_ssa_value_refs_equal(lhs, rhs)) {
        return 1;
    }
    if (lhs.kind != VALUE_SSA_VALUE_ID || rhs.kind != VALUE_SSA_VALUE_ID) {
        return 0;
    }

    lhs_def = value_ssa_bridge_lookup_instruction_definition(function, analysis, lhs.value_id);
    rhs_def = value_ssa_bridge_lookup_instruction_definition(function, analysis, rhs.value_id);
    if (!lhs_def || !rhs_def || lhs_def->kind != rhs_def->kind) {
        return 0;
    }

    switch (lhs_def->kind) {
    case VALUE_SSA_INSTR_ADDR_LOCAL:
    case VALUE_SSA_INSTR_ADDR_GLOBAL:
        return lhs_def->as.addr_slot.kind == rhs_def->as.addr_slot.kind &&
            lhs_def->as.addr_slot.id == rhs_def->as.addr_slot.id;
    case VALUE_SSA_INSTR_MOV:
        return value_ssa_bridge_same_pure_address_value(
            function, analysis, lhs_def->as.mov_value, rhs_def->as.mov_value, depth + 1u);
    case VALUE_SSA_INSTR_BINARY:
        return lhs_def->as.binary.op == rhs_def->as.binary.op &&
            value_ssa_bridge_same_pure_address_value(
                function, analysis, lhs_def->as.binary.lhs, rhs_def->as.binary.lhs, depth + 1u) &&
            value_ssa_bridge_same_pure_address_value(
                function, analysis, lhs_def->as.binary.rhs, rhs_def->as.binary.rhs, depth + 1u);
    default:
        return 0;
    }
}

static int value_ssa_bridge_extract_address_root(ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    ValueSsaValueRef value,
    ValueSsaAddressRoot *out_root,
    unsigned depth) {
    ValueSsaInstruction *definition;

    if (!out_root) {
        return 0;
    }

    out_root->kind = VALUE_SSA_ADDRESS_ROOT_UNKNOWN;
    out_root->id = 0u;
    if (depth > 8u || value.kind != VALUE_SSA_VALUE_ID) {
        return 0;
    }

    definition = value_ssa_bridge_lookup_instruction_definition(function, analysis, value.value_id);
    if (!definition) {
        return 0;
    }

    switch (definition->kind) {
    case VALUE_SSA_INSTR_ADDR_LOCAL:
        out_root->kind = VALUE_SSA_ADDRESS_ROOT_LOCAL_SLOT;
        out_root->id = definition->as.addr_slot.id;
        return 1;
    case VALUE_SSA_INSTR_ADDR_GLOBAL:
        out_root->kind = VALUE_SSA_ADDRESS_ROOT_GLOBAL_SLOT;
        out_root->id = definition->as.addr_slot.id;
        return 1;
    case VALUE_SSA_INSTR_LOAD_LOCAL:
        if (definition->as.load_slot.kind == VALUE_SSA_SLOT_LOCAL &&
            definition->as.load_slot.id < function->local_count &&
            function->locals[definition->as.load_slot.id].is_parameter &&
            function->locals[definition->as.load_slot.id].array_rank > 0u) {
            out_root->kind = VALUE_SSA_ADDRESS_ROOT_PARAMETER_POINTER;
            out_root->id = definition->as.load_slot.id;
            return 1;
        }
        return 0;
    case VALUE_SSA_INSTR_MOV:
        return value_ssa_bridge_extract_address_root(
            function, analysis, definition->as.mov_value, out_root, depth + 1u);
    case VALUE_SSA_INSTR_BINARY: {
        ValueSsaAddressRoot lhs_root;
        ValueSsaAddressRoot rhs_root;
        int has_lhs_root;
        int has_rhs_root;

        if (definition->as.binary.op != VALUE_SSA_BINARY_ADD) {
            return 0;
        }

        has_lhs_root = value_ssa_bridge_extract_address_root(
            function, analysis, definition->as.binary.lhs, &lhs_root, depth + 1u);
        has_rhs_root = value_ssa_bridge_extract_address_root(
            function, analysis, definition->as.binary.rhs, &rhs_root, depth + 1u);
        if (has_lhs_root == has_rhs_root) {
            return 0;
        }
        *out_root = has_lhs_root ? lhs_root : rhs_root;
        return 1;
    }
    default:
        return 0;
    }
}

static int value_ssa_bridge_address_roots_proven_non_alias(const ValueSsaAddressRoot *lhs,
    const ValueSsaAddressRoot *rhs) {
    if (!lhs || !rhs ||
        lhs->kind == VALUE_SSA_ADDRESS_ROOT_UNKNOWN ||
        rhs->kind == VALUE_SSA_ADDRESS_ROOT_UNKNOWN) {
        return 0;
    }

    if (lhs->kind == VALUE_SSA_ADDRESS_ROOT_LOCAL_SLOT &&
        rhs->kind == VALUE_SSA_ADDRESS_ROOT_LOCAL_SLOT) {
        return lhs->id != rhs->id;
    }
    if (lhs->kind == VALUE_SSA_ADDRESS_ROOT_GLOBAL_SLOT &&
        rhs->kind == VALUE_SSA_ADDRESS_ROOT_GLOBAL_SLOT) {
        return lhs->id != rhs->id;
    }
    if ((lhs->kind == VALUE_SSA_ADDRESS_ROOT_LOCAL_SLOT && rhs->kind == VALUE_SSA_ADDRESS_ROOT_GLOBAL_SLOT) ||
        (lhs->kind == VALUE_SSA_ADDRESS_ROOT_GLOBAL_SLOT && rhs->kind == VALUE_SSA_ADDRESS_ROOT_LOCAL_SLOT)) {
        return 1;
    }
    if ((lhs->kind == VALUE_SSA_ADDRESS_ROOT_LOCAL_SLOT && rhs->kind == VALUE_SSA_ADDRESS_ROOT_PARAMETER_POINTER) ||
        (lhs->kind == VALUE_SSA_ADDRESS_ROOT_PARAMETER_POINTER && rhs->kind == VALUE_SSA_ADDRESS_ROOT_LOCAL_SLOT)) {
        return 1;
    }

    return 0;
}

static int value_ssa_bridge_store_cannot_alias_loaded_address(ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    const ValueSsaAddressRoot *load_root,
    const ValueSsaInstruction *store_instruction) {
    ValueSsaAddressRoot store_root;

    if (!function || !analysis || !load_root || !store_instruction) {
        return 0;
    }

    switch (store_instruction->kind) {
    case VALUE_SSA_INSTR_STORE_LOCAL:
        if (load_root->kind != VALUE_SSA_ADDRESS_ROOT_LOCAL_SLOT ||
            store_instruction->as.store.slot.kind != VALUE_SSA_SLOT_LOCAL ||
            store_instruction->as.store.slot.id >= function->local_count) {
            return load_root->kind != VALUE_SSA_ADDRESS_ROOT_PARAMETER_POINTER;
        }
        if (store_instruction->as.store.slot.id == load_root->id) {
            return 0;
        }
        return function->locals[store_instruction->as.store.slot.id].array_rank == 0u;
    case VALUE_SSA_INSTR_STORE_GLOBAL:
        if (store_instruction->as.store.slot.kind != VALUE_SSA_SLOT_GLOBAL) {
            return 0;
        }
        if (load_root->kind == VALUE_SSA_ADDRESS_ROOT_GLOBAL_SLOT) {
            return store_instruction->as.store.slot.id != load_root->id;
        }
        return load_root->kind == VALUE_SSA_ADDRESS_ROOT_LOCAL_SLOT;
    case VALUE_SSA_INSTR_STORE_INDIRECT:
        if (!value_ssa_bridge_extract_address_root(
                function, analysis, store_instruction->as.store_indirect.addr, &store_root, 0u)) {
            return 0;
        }
        return value_ssa_bridge_address_roots_proven_non_alias(load_root, &store_root);
    default:
        return 1;
    }
}

static int value_ssa_bridge_forward_same_block_repeated_indirect_loads(ValueSsaProgram *program,
    ValueSsaError *error) {
    size_t function_index;

    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-274: invalid same-block indirect-load forward contract");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunction *function = &program->functions[function_index];
        ValueSsaDefUseAnalysis def_use;
        int changed = 0;
        size_t block_index;

        if (!function->has_body) {
            continue;
        }

        value_ssa_def_use_analysis_init(&def_use);
        if (!value_ssa_compute_def_use_analysis(function, &def_use, error)) {
            value_ssa_def_use_analysis_free(&def_use);
            return 0;
        }

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            ValueSsaBasicBlock *block = &function->blocks[block_index];
            size_t instruction_index;

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                ValueSsaInstruction *load = &block->instructions[instruction_index];
                ValueSsaAddressRoot load_root;
                size_t candidate_index;

                if (load->kind != VALUE_SSA_INSTR_LOAD_INDIRECT ||
                    !load->has_result ||
                    load->result.kind != VALUE_SSA_VALUE_ID ||
                    !value_ssa_bridge_extract_address_root(
                        function, &def_use, load->as.load_indirect_addr, &load_root, 0u)) {
                    continue;
                }

                for (candidate_index = instruction_index; candidate_index > 0; --candidate_index) {
                    ValueSsaInstruction *candidate = &block->instructions[candidate_index - 1u];
                    size_t middle_index;
                    int blocked = 0;

                    if (candidate->kind != VALUE_SSA_INSTR_LOAD_INDIRECT ||
                        !candidate->has_result ||
                        candidate->result.kind != VALUE_SSA_VALUE_ID) {
                        continue;
                    }
                    if (!value_ssa_value_refs_equal(candidate->as.load_indirect_addr, load->as.load_indirect_addr) &&
                        !value_ssa_bridge_same_pure_address_value(
                            function, &def_use, candidate->as.load_indirect_addr, load->as.load_indirect_addr, 0u)) {
                        continue;
                    }

                    for (middle_index = candidate_index; middle_index < instruction_index; ++middle_index) {
                        ValueSsaInstruction *middle = &block->instructions[middle_index];

                        if (middle->kind == VALUE_SSA_INSTR_CALL ||
                            !value_ssa_bridge_store_cannot_alias_loaded_address(
                                function, &def_use, &load_root, middle)) {
                            blocked = 1;
                            break;
                        }
                    }
                    if (blocked) {
                        continue;
                    }

                    load->kind = VALUE_SSA_INSTR_MOV;
                    load->as.mov_value = candidate->result;
                    changed = 1;
                    break;
                }
            }
        }

        value_ssa_def_use_analysis_free(&def_use);
        if (changed &&
            (!value_ssa_simplify_trivial_values(program, error) ||
                !value_ssa_eliminate_dead_value_defs(program, error))) {
            return 0;
        }
    }

    return value_ssa_verify_program(program, error);
}

static int value_ssa_run_indirect_memory_direct_fast_cleanup(ValueSsaProgram *program, ValueSsaError *error) {
    int trace_timing = value_ssa_trace_enabled();
    double stage_start = trace_timing ? value_ssa_now_s() : 0.0;

    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-257: invalid indirect-memory direct cleanup contract");
        return 0;
    }

    if (!value_ssa_forward_local_loads(program, error)) {
        return 0;
    }
    if (trace_timing) {
        value_ssa_trace_timing("indirect-direct-forward-local-loads", value_ssa_now_s() - stage_start);
        stage_start = value_ssa_now_s();
    }

    if (!value_ssa_eliminate_unread_scalar_local_stores(program, error)) {
        return 0;
    }
    if (trace_timing) {
        value_ssa_trace_timing("indirect-direct-eliminate-unread-scalar-local-stores", value_ssa_now_s() - stage_start);
        stage_start = value_ssa_now_s();
    }

    {
        size_t function_index;
        for (function_index = 0; function_index < program->function_count; ++function_index) {
            ValueSsaFunction *function = &program->functions[function_index];
            ValueSsaCfgAnalysis cfg;
            ValueSsaDefUseAnalysis def_use;
            int changed = 0;
            size_t block_index;

            if (!function->has_body || function->block_count == 0u) {
                continue;
            }

            value_ssa_cfg_analysis_init(&cfg);
            value_ssa_def_use_analysis_init(&def_use);
            if (!value_ssa_compute_cfg_analysis(function, &cfg, error) ||
                !value_ssa_compute_def_use_analysis(function, &def_use, error)) {
                value_ssa_def_use_analysis_free(&def_use);
                value_ssa_cfg_analysis_free(&cfg);
                return 0;
            }

            for (block_index = 0; block_index < function->block_count; ++block_index) {
                ValueSsaBasicBlock *block = &function->blocks[block_index];
                size_t predecessor_id = value_ssa_pass_get_single_idom_predecessor(function, &cfg, block_index);
                ValueSsaBasicBlock *predecessor;
                size_t predecessor_inst_index;
                size_t instruction_index;

                if (predecessor_id == (size_t)-1 || block->instruction_count == 0u) {
                    continue;
                }
                predecessor = &function->blocks[predecessor_id];

                for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                    ValueSsaInstruction *load = &block->instructions[instruction_index];
                    size_t candidate_index;
                    ValueSsaInstruction *candidate = NULL;
                    size_t scan_index;
                    int blocked = 0;

                    if (load->kind != VALUE_SSA_INSTR_LOAD_INDIRECT ||
                        !load->has_result ||
                        load->result.kind != VALUE_SSA_VALUE_ID) {
                        continue;
                    }

                    for (scan_index = 0; scan_index < instruction_index; ++scan_index) {
                        switch (block->instructions[scan_index].kind) {
                        case VALUE_SSA_INSTR_STORE_LOCAL:
                        case VALUE_SSA_INSTR_STORE_GLOBAL:
                        case VALUE_SSA_INSTR_STORE_INDIRECT:
                        case VALUE_SSA_INSTR_CALL:
                            blocked = 1;
                            break;
                        default:
                            break;
                        }
                        if (blocked) {
                            break;
                        }
                    }
                    if (blocked) {
                        continue;
                    }

                    for (candidate_index = predecessor->instruction_count; candidate_index > 0; --candidate_index) {
                        candidate = &predecessor->instructions[candidate_index - 1u];
                        if (candidate->kind != VALUE_SSA_INSTR_LOAD_INDIRECT ||
                            !candidate->has_result ||
                            candidate->result.kind != VALUE_SSA_VALUE_ID) {
                            continue;
                        }

                        blocked = 0;
                        for (predecessor_inst_index = candidate_index; predecessor_inst_index < predecessor->instruction_count;
                             ++predecessor_inst_index) {
                            ValueSsaInstruction *after = &predecessor->instructions[predecessor_inst_index];
                            if (after->result.kind == VALUE_SSA_VALUE_ID &&
                                candidate->result.kind == VALUE_SSA_VALUE_ID &&
                                after->result.value_id == candidate->result.value_id) {
                                blocked = 1;
                                break;
                            }
                            switch (after->kind) {
                            case VALUE_SSA_INSTR_STORE_LOCAL:
                            case VALUE_SSA_INSTR_STORE_GLOBAL:
                            case VALUE_SSA_INSTR_STORE_INDIRECT:
                            case VALUE_SSA_INSTR_CALL:
                                blocked = 1;
                                break;
                            default:
                                break;
                            }
                            if (blocked) {
                                break;
                            }
                        }
                        if (blocked) {
                            continue;
                        }

                        if (value_ssa_value_refs_equal(candidate->as.load_indirect_addr, load->as.load_indirect_addr)) {
                            load->kind = VALUE_SSA_INSTR_MOV;
                            load->as.mov_value = candidate->result;
                            changed = 1;
                            break;
                        }

                        if (value_ssa_bridge_same_pure_address_value(
                                function,
                                &def_use,
                                candidate->as.load_indirect_addr,
                                load->as.load_indirect_addr,
                                0u)) {
                            load->kind = VALUE_SSA_INSTR_MOV;
                            load->as.mov_value = candidate->result;
                            changed = 1;
                            break;
                        }
                    }
                }
            }

            value_ssa_def_use_analysis_free(&def_use);
            value_ssa_cfg_analysis_free(&cfg);
            if (changed && !value_ssa_simplify_trivial_values(program, error)) {
                return 0;
            }
        }
    }
    if (trace_timing) {
        value_ssa_trace_timing("indirect-direct-edge-load-forward", value_ssa_now_s() - stage_start);
        stage_start = value_ssa_now_s();
    }

    if (!value_ssa_forward_non_address_taken_global_loads(program, error)) {
        return 0;
    }
    if (trace_timing) {
        value_ssa_trace_timing("indirect-direct-forward-safe-global-loads", value_ssa_now_s() - stage_start);
        stage_start = value_ssa_now_s();
    }

    if (!value_ssa_bridge_forward_same_block_repeated_indirect_loads(program, error)) {
        return 0;
    }
    if (trace_timing) {
        value_ssa_trace_timing("indirect-direct-same-block-indirect-load-forward", value_ssa_now_s() - stage_start);
        stage_start = value_ssa_now_s();
    }

    if (!value_ssa_simplify_trivial_values(program, error)) {
        return 0;
    }
    if (trace_timing) {
        value_ssa_trace_timing("indirect-direct-simplify-trivial-values", value_ssa_now_s() - stage_start);
        stage_start = value_ssa_now_s();
    }

    if (!value_ssa_bridge_reuse_repeated_pure_internal_calls_in_program(program, error)) {
        return 0;
    }
    if (trace_timing) {
        value_ssa_trace_timing("indirect-direct-reuse-pure-internal-calls", value_ssa_now_s() - stage_start);
        stage_start = value_ssa_now_s();
    }

    if (!value_ssa_inline_tiny_internal_helpers(program, error)) {
        return 0;
    }
    if (trace_timing) {
        value_ssa_trace_timing("indirect-direct-inline-tiny-helpers", value_ssa_now_s() - stage_start);
        stage_start = value_ssa_now_s();
    }

    if (!value_ssa_bridge_reuse_addr_roots_in_program(program, error)) {
        return 0;
    }
    if (trace_timing) {
        value_ssa_trace_timing("indirect-direct-reuse-addr-roots", value_ssa_now_s() - stage_start);
        stage_start = value_ssa_now_s();
    }

    if (!value_ssa_simplify_trivial_values(program, error)) {
        return 0;
    }
    if (trace_timing) {
        value_ssa_trace_timing("indirect-direct-post-root-simplify", value_ssa_now_s() - stage_start);
        stage_start = value_ssa_now_s();
    }

    if (!value_ssa_normalize_binary_operands(program, error) ||
        !value_ssa_eliminate_redundant_binaries(program, error) ||
        !value_ssa_simplify_trivial_values(program, error) ||
        !value_ssa_eliminate_dead_value_defs(program, error) ||
        !value_ssa_normalize_binary_operands(program, error) ||
        !value_ssa_eliminate_redundant_binaries(program, error) ||
        !value_ssa_simplify_trivial_values(program, error) ||
        !value_ssa_eliminate_dead_value_defs(program, error)) {
        return 0;
    }
    if (trace_timing) {
        value_ssa_trace_timing("indirect-direct-redundant-binary-cleanup", value_ssa_now_s() - stage_start);
    }

    return value_ssa_verify_program(program, error);
}

int value_ssa_build_from_lower_ir_with_canonicalization(const LowerIrProgram *program,
    ValueSsaLowerIrCanonicalizeMode mode,
    ValueSsaProgram *out_program,
    ValueSsaError *error) {
    if (!program || !out_program) {
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-176: invalid lower-ir canonicalization-mode entry contract");
        return 0;
    }

    switch (mode) {
    case VALUE_SSA_LOWER_IR_CANONICALIZE_CLASSIC:
        if (!value_ssa_build_from_lower_ir(program, out_program, error)) {
            return 0;
        }
        if (!value_ssa_canonicalize_program(out_program, error)) {
            value_ssa_program_free(out_program);
            return 0;
        }
        return 1;
    case VALUE_SSA_LOWER_IR_CANONICALIZE_MEMORY_VALUE:
        return value_ssa_build_from_lower_ir_with_bridge(program,
            out_program,
            memory_ssa_pass_build_memory_canonicalized_from_lower_ir,
            "VALUE-SSA-173: invalid lower-ir memory-value canonicalized conversion entry contract",
            error);
    case VALUE_SSA_LOWER_IR_CANONICALIZE_MEMORY_FULL:
        return value_ssa_build_from_lower_ir_with_bridge(program,
            out_program,
            memory_ssa_pass_build_canonicalized_from_lower_ir,
            "VALUE-SSA-174: invalid lower-ir memory canonicalized conversion entry contract",
            error);
    default:
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-177: unknown lower-ir canonicalization mode %d",
            (int)mode);
        return 0;
    }
}

int value_ssa_build_default_from_lower_ir(const LowerIrProgram *program,
    ValueSsaProgram *out_program,
    ValueSsaError *error) {
    int trace_timing = value_ssa_trace_enabled();
    double total_start = trace_timing ? value_ssa_now_s() : 0.0;
    double stage_start = 0.0;
    int uses_indirect_memory = value_ssa_lower_ir_uses_indirect_memory(program);
    int use_extreme_straight_line_fast_path = value_ssa_lower_ir_is_extreme_straight_line_hotspot(program);

    if (trace_timing) {
        fprintf(stderr,
            "[value-ssa-timing] flags indirect=%d extreme-straight-line=%d\n",
            uses_indirect_memory,
            use_extreme_straight_line_fast_path);
    }
    if (uses_indirect_memory) {
        stage_start = trace_timing ? value_ssa_now_s() : 0.0;
        if (!value_ssa_build_from_lower_ir(program, out_program, error)) {
            return 0;
        }
        if (trace_timing) {
            value_ssa_trace_timing("direct-build-from-lower-ir", value_ssa_now_s() - stage_start);
            stage_start = value_ssa_now_s();
        }
        if (!value_ssa_run_indirect_memory_direct_fast_cleanup(out_program, error)) {
            value_ssa_program_free(out_program);
            return 0;
        }
        if (trace_timing) {
            value_ssa_trace_timing("indirect-direct-fast-cleanup", value_ssa_now_s() - stage_start);
            value_ssa_trace_timing("total", value_ssa_now_s() - total_start);
        }
        return 1;
    }
    if (use_extreme_straight_line_fast_path) {
        stage_start = trace_timing ? value_ssa_now_s() : 0.0;
        if (!value_ssa_build_from_lower_ir(program, out_program, error)) {
            return 0;
        }
        if (trace_timing) {
            value_ssa_trace_timing("extreme-direct-build-from-lower-ir", value_ssa_now_s() - stage_start);
            value_ssa_trace_timing("total", value_ssa_now_s() - total_start);
        }
        return 1;
    }
    stage_start = trace_timing ? value_ssa_now_s() : 0.0;
    if (!value_ssa_build_from_lower_ir_with_canonicalization(
            program, VALUE_SSA_LOWER_IR_CANONICALIZE_DEFAULT, out_program, error)) {
        return 0;
    }
    if (trace_timing) {
        value_ssa_trace_timing("default-canonicalized-build", value_ssa_now_s() - stage_start);
        value_ssa_trace_timing("total", value_ssa_now_s() - total_start);
    }
    return 1;
}

int value_ssa_build_memory_value_canonicalized_from_lower_ir(const LowerIrProgram *program,
    ValueSsaProgram *out_program,
    ValueSsaError *error) {
    return value_ssa_build_from_lower_ir_with_canonicalization(program,
        VALUE_SSA_LOWER_IR_CANONICALIZE_MEMORY_VALUE,
        out_program,
        error);
}

int value_ssa_build_memory_canonicalized_from_lower_ir(const LowerIrProgram *program,
    ValueSsaProgram *out_program,
    ValueSsaError *error) {
    return value_ssa_build_from_lower_ir_with_canonicalization(program,
        VALUE_SSA_LOWER_IR_CANONICALIZE_MEMORY_FULL,
        out_program,
        error);
}

#endif
