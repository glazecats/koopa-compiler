#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_SPLIT_AGGREGATOR)
#define VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT
#include "../value_ssa_pass/value_ssa_pass.c"
#else

static int value_ssa_perf_value_is_loop_invariant(const ValueSsaProgram *program,
    const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    const unsigned char *readonly_globals,
    const ValueSsaPerfSimpleLoop *loop,
    ValueSsaValueRef value,
    unsigned depth);
static int value_ssa_perf_value_is_loop_invariant_for_indirect_addr(const ValueSsaProgram *program,
    const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    const unsigned char *readonly_globals,
    const unsigned char *address_taken_locals,
    const ValueSsaPerfSimpleLoop *loop,
    ValueSsaValueRef value,
    unsigned depth);
static int value_ssa_perf_extract_address_root(const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    ValueSsaValueRef value,
    ValueSsaPerfAddressRoot *out_root,
    unsigned depth);
static int value_ssa_perf_store_cannot_alias_loaded_address(const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    const unsigned char *parameter_pointer_non_alias_pairs,
    const ValueSsaPerfAddressRoot *load_root,
    const ValueSsaInstruction *store_instruction);
static int value_ssa_perf_clone_loop_invariant_value_into_preheader(const ValueSsaProgram *program,
    ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    const unsigned char *readonly_globals,
    const ValueSsaPerfSimpleLoop *loop,
    ValueSsaValueRef value,
    unsigned char *has_mapping,
    ValueSsaValueRef *mappings,
    ValueSsaBasicBlock *rebuilt_preheader,
    ValueSsaValueRef *out_value,
    ValueSsaError *error);
static int value_ssa_perf_collect_readonly_scalar_globals(const ValueSsaProgram *program,
    unsigned char *readonly_flags);
static int value_ssa_perf_collect_address_taken_locals(const ValueSsaFunction *function,
    unsigned char *address_taken_locals);

static int value_ssa_perf_loop_instructions_preserve_indirect_load(const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    const ValueSsaBasicBlock *block,
    const unsigned char *parameter_pointer_non_alias_pairs,
    const ValueSsaPerfAddressRoot *load_root) {
    size_t instruction_index;

    if (!function || !analysis || !block || !load_root) {
        return 0;
    }

    for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
        if (!value_ssa_perf_store_cannot_alias_loaded_address(
                function,
                analysis,
                parameter_pointer_non_alias_pairs,
                load_root,
                &block->instructions[instruction_index])) {
            return 0;
        }
    }

    return 1;
}

static int value_ssa_perf_instruction_preserves_indirect_load(const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    const unsigned char *parameter_pointer_non_alias_pairs,
    const ValueSsaPerfAddressRoot *load_root,
    const ValueSsaInstruction *instruction) {
    if (!instruction || instruction->kind == VALUE_SSA_INSTR_CALL) {
        return 0;
    }

    return value_ssa_perf_store_cannot_alias_loaded_address(
        function, analysis, parameter_pointer_non_alias_pairs, load_root, instruction);
}

static int value_ssa_perf_instruction_range_preserves_indirect_load(const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    const ValueSsaBasicBlock *block,
    size_t start_index,
    size_t end_index,
    const unsigned char *parameter_pointer_non_alias_pairs,
    const ValueSsaPerfAddressRoot *load_root) {
    size_t instruction_index;

    if (!function || !analysis || !block || !load_root ||
        start_index > end_index || end_index > block->instruction_count) {
        return 0;
    }

    for (instruction_index = start_index; instruction_index < end_index; ++instruction_index) {
        if (!value_ssa_perf_instruction_preserves_indirect_load(
                function,
                analysis,
                parameter_pointer_non_alias_pairs,
                load_root,
                &block->instructions[instruction_index])) {
            return 0;
        }
    }

    return 1;
}

static int value_ssa_perf_clone_loop_invariant_value_into_preheader(const ValueSsaProgram *program,
    ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    const unsigned char *readonly_globals,
    const ValueSsaPerfSimpleLoop *loop,
    ValueSsaValueRef value,
    unsigned char *has_mapping,
    ValueSsaValueRef *mappings,
    ValueSsaBasicBlock *rebuilt_preheader,
    ValueSsaValueRef *out_value,
    ValueSsaError *error);

static int value_ssa_perf_try_hoist_loop_invariant_indirect_load(ValueSsaFunction *function,
    const ValueSsaProgram *program,
    const ValueSsaDefUseAnalysis *analysis,
    const unsigned char *readonly_globals,
    const unsigned char *address_taken_locals,
    const unsigned char *parameter_pointer_non_alias_pairs,
    const ValueSsaPerfSimpleLoop *loop,
    unsigned char *value_has_mapping,
    ValueSsaValueRef *value_mappings,
    ValueSsaBasicBlock *rebuilt_preheader,
    ValueSsaBasicBlock *rebuilt_target,
    const ValueSsaInstruction *instruction,
    int *out_changed,
    ValueSsaError *error) {
    ValueSsaPerfAddressRoot load_root;
    ValueSsaInstruction cloned_load;
    ValueSsaInstruction replacement;
    ValueSsaValueRef hoisted_addr;

    if (out_changed) {
        *out_changed = 0;
    }
    if (!function || !program || !analysis || !readonly_globals || !loop || !rebuilt_preheader || !rebuilt_target || !instruction ||
        (!address_taken_locals && function->local_count > 0u) ||
        instruction->kind != VALUE_SSA_INSTR_LOAD_INDIRECT ||
        !instruction->has_result ||
        instruction->result.kind != VALUE_SSA_VALUE_ID) {
        return 1;
    }

    if (!value_ssa_perf_value_is_loop_invariant_for_indirect_addr(
            program,
            function,
            analysis,
            readonly_globals,
            address_taken_locals,
            loop,
            instruction->as.load_indirect_addr,
            0u)) {
        return 1;
    }
    if (!value_ssa_perf_extract_address_root(function, analysis, instruction->as.load_indirect_addr, &load_root, 0u) ||
        !value_ssa_perf_loop_instructions_preserve_indirect_load(
            function,
            analysis,
            &function->blocks[loop->header_id],
            parameter_pointer_non_alias_pairs,
            &load_root) ||
        !value_ssa_perf_loop_instructions_preserve_indirect_load(
            function,
            analysis,
            &function->blocks[loop->body_id],
            parameter_pointer_non_alias_pairs,
            &load_root)) {
        return 1;
    }

    memset(&cloned_load, 0, sizeof(cloned_load));
    cloned_load.kind = VALUE_SSA_INSTR_LOAD_INDIRECT;
    cloned_load.has_result = 1;
    cloned_load.result = value_ssa_value_id(value_ssa_function_allocate_value(function));
    if (!value_ssa_perf_clone_loop_invariant_value_into_preheader(
            program,
            function,
            analysis,
            readonly_globals,
            loop,
            instruction->as.load_indirect_addr,
            value_has_mapping,
            value_mappings,
            rebuilt_preheader,
            &hoisted_addr,
            error)) {
        return 0;
    }
    cloned_load.as.load_indirect_addr = hoisted_addr;
    if (cloned_load.result.value_id == (size_t)-1 ||
        !value_ssa_block_append_instruction(rebuilt_preheader, &cloned_load, error)) {
        return 0;
    }

    memset(&replacement, 0, sizeof(replacement));
    replacement.kind = VALUE_SSA_INSTR_MOV;
    replacement.has_result = 1;
    replacement.result = instruction->result;
    replacement.as.mov_value = cloned_load.result;
    if (!value_ssa_block_append_instruction(rebuilt_target, &replacement, error)) {
        return 0;
    }

    if (out_changed) {
        *out_changed = 1;
    }
    return 1;
}

static int value_ssa_perf_try_hoist_loop_invariant_branch_bound_load(ValueSsaFunction *function,
    const ValueSsaProgram *program,
    const ValueSsaDefUseAnalysis *analysis,
    const unsigned char *readonly_globals,
    const unsigned char *address_taken_locals,
    const unsigned char *parameter_pointer_non_alias_pairs,
    const ValueSsaPerfSimpleLoop *loop,
    unsigned char *value_has_mapping,
    ValueSsaValueRef *value_mappings,
    ValueSsaBasicBlock *rebuilt_preheader,
    ValueSsaBasicBlock *rebuilt_target,
    const ValueSsaInstruction *instruction,
    int *out_changed,
    ValueSsaError *error) {
    ValueSsaInstruction *condition_def = NULL;
    ValueSsaValueRef candidate_value;
    ValueSsaInstruction *candidate_def = NULL;
    ValueSsaUseSite load_use_site;
    ValueSsaInstruction cloned_load;
    ValueSsaInstruction replacement;
    ValueSsaPerfAddressRoot load_root;
    ValueSsaValueRef hoisted_addr;
    size_t use_offset;

    if (out_changed) {
        *out_changed = 0;
    }
    if (!function || !program || !analysis || !readonly_globals || !loop || !value_has_mapping || !value_mappings ||
        !rebuilt_preheader || !rebuilt_target || !instruction) {
        return 1;
    }
    if ((!address_taken_locals && function->local_count > 0u) ||
        (!parameter_pointer_non_alias_pairs && function->parameter_count > 0u)) {
        return 1;
    }
    if (loop->header_id >= function->block_count) {
        return 1;
    }
    if (!function->blocks[loop->header_id].has_terminator ||
        function->blocks[loop->header_id].terminator.kind != VALUE_SSA_TERM_BRANCH) {
        return 1;
    }
    if (function->blocks[loop->header_id].terminator.as.branch.condition.kind != VALUE_SSA_VALUE_ID) {
        return 1;
    }

    condition_def = value_ssa_pass_lookup_instruction_definition(
        function, analysis, function->blocks[loop->header_id].terminator.as.branch.condition.value_id);
    if (!condition_def || condition_def->kind != VALUE_SSA_INSTR_BINARY) {
        return 1;
    }

    candidate_value.kind = VALUE_SSA_VALUE_IMMEDIATE;
    candidate_value.immediate = 0;
    candidate_value.value_id = 0u;
    if (condition_def->as.binary.lhs.kind == VALUE_SSA_VALUE_ID) {
        ValueSsaInstruction *lhs_def = value_ssa_pass_lookup_instruction_definition(
            function, analysis, condition_def->as.binary.lhs.value_id);
        if (lhs_def && lhs_def->kind == VALUE_SSA_INSTR_LOAD_INDIRECT) {
            candidate_value = condition_def->as.binary.lhs;
        }
    }
    if (candidate_value.kind != VALUE_SSA_VALUE_ID && condition_def->as.binary.rhs.kind == VALUE_SSA_VALUE_ID) {
        ValueSsaInstruction *rhs_def = value_ssa_pass_lookup_instruction_definition(
            function, analysis, condition_def->as.binary.rhs.value_id);
        if (rhs_def && rhs_def->kind == VALUE_SSA_INSTR_LOAD_INDIRECT) {
            candidate_value = condition_def->as.binary.rhs;
        }
    }
    if (candidate_value.kind != VALUE_SSA_VALUE_ID ||
        candidate_value.value_id >= analysis->value_count ||
        analysis->use_counts[candidate_value.value_id] != 1u) {
        return 1;
    }

    use_offset = analysis->use_offsets[candidate_value.value_id];
    if (!analysis->use_sites ||
        use_offset >= analysis->use_offsets[candidate_value.value_id] + analysis->use_counts[candidate_value.value_id]) {
        return 1;
    }
    load_use_site = analysis->use_sites[use_offset];
    if (load_use_site.kind != VALUE_SSA_USE_INSTR ||
        load_use_site.block_id != loop->header_id ||
        load_use_site.instruction_index >= function->blocks[loop->header_id].instruction_count) {
        return 1;
    }
    if (&function->blocks[loop->header_id].instructions[load_use_site.instruction_index] != condition_def) {
        return 1;
    }
    if (condition_def->has_result &&
        condition_def->result.kind == VALUE_SSA_VALUE_ID &&
        condition_def->result.value_id < analysis->value_count &&
        analysis->use_counts[condition_def->result.value_id] == 1u) {
        size_t condition_use_offset = analysis->use_offsets[condition_def->result.value_id];

        if (!analysis->use_sites ||
            analysis->use_sites[condition_use_offset].kind != VALUE_SSA_USE_TERM ||
            analysis->use_sites[condition_use_offset].role != VALUE_SSA_USE_ROLE_BRANCH_CONDITION ||
            analysis->use_sites[condition_use_offset].block_id != loop->header_id) {
            return 1;
        }
    } else {
        return 1;
    }

    candidate_def = value_ssa_pass_lookup_instruction_definition(function, analysis, candidate_value.value_id);
    if (!candidate_def || candidate_def->kind != VALUE_SSA_INSTR_LOAD_INDIRECT) {
        return 1;
    }
    if (candidate_def != instruction) {
        return 1;
    }
    if (!value_ssa_perf_value_is_loop_invariant_for_indirect_addr(
            program,
            function,
            analysis,
            readonly_globals,
            address_taken_locals,
            loop,
            candidate_def->as.load_indirect_addr,
            0u)) {
        return 1;
    }
    if (!value_ssa_perf_extract_address_root(function, analysis, candidate_def->as.load_indirect_addr, &load_root, 0u) ||
        !value_ssa_perf_loop_instructions_preserve_indirect_load(
            function,
            analysis,
            &function->blocks[loop->header_id],
            parameter_pointer_non_alias_pairs,
            &load_root) ||
        !value_ssa_perf_loop_instructions_preserve_indirect_load(
            function,
            analysis,
            &function->blocks[loop->body_id],
            parameter_pointer_non_alias_pairs,
            &load_root)) {
        return 1;
    }

    memset(&cloned_load, 0, sizeof(cloned_load));
    cloned_load.kind = VALUE_SSA_INSTR_LOAD_INDIRECT;
    cloned_load.has_result = 1;
    cloned_load.result = value_ssa_value_id(value_ssa_function_allocate_value(function));
    if (!value_ssa_perf_clone_loop_invariant_value_into_preheader(
            program,
            function,
            analysis,
            readonly_globals,
            loop,
            candidate_def->as.load_indirect_addr,
            value_has_mapping,
            value_mappings,
            rebuilt_preheader,
            &hoisted_addr,
            error)) {
        return 0;
    }
    cloned_load.as.load_indirect_addr = hoisted_addr;
    if (cloned_load.result.value_id == (size_t)-1 ||
        !value_ssa_block_append_instruction(rebuilt_preheader, &cloned_load, error)) {
        return 0;
    }

    memset(&replacement, 0, sizeof(replacement));
    replacement.kind = VALUE_SSA_INSTR_MOV;
    replacement.has_result = 1;
    replacement.result = candidate_def->result;
    replacement.as.mov_value = cloned_load.result;
    if (!value_ssa_block_append_instruction(rebuilt_target, &replacement, error)) {
        return 0;
    }

    if (out_changed) {
        *out_changed = 1;
    }
    return 1;
}

static int value_ssa_perf_clone_loop_invariant_value_into_preheader(const ValueSsaProgram *program,
    ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    const unsigned char *readonly_globals,
    const ValueSsaPerfSimpleLoop *loop,
    ValueSsaValueRef value,
    unsigned char *has_mapping,
    ValueSsaValueRef *mappings,
    ValueSsaBasicBlock *rebuilt_preheader,
    ValueSsaValueRef *out_value,
    ValueSsaError *error) {
    ValueSsaInstruction *definition;
    ValueSsaInstruction cloned_instruction;

    if (!out_value) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-296: invalid perf invariant clone output");
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
    if (value.value_id >= analysis->value_count || !analysis->has_def[value.value_id]) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-297: missing perf invariant definition for ssa.%zu", value.value_id);
        return 0;
    }
    if (analysis->def_block_ids[value.value_id] != loop->header_id &&
        analysis->def_block_ids[value.value_id] != loop->body_id) {
        *out_value = value;
        return 1;
    }

    definition = value_ssa_pass_lookup_instruction_definition(function, analysis, value.value_id);
    if (!definition) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-297: missing perf invariant definition for ssa.%zu", value.value_id);
        return 0;
    }
    if (!value_ssa_perf_value_is_loop_invariant(
            program,
            function,
            analysis,
            readonly_globals,
            loop,
            value,
            0u)) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-298: attempted to clone non-invariant perf value ssa.%zu", value.value_id);
        return 0;
    }

    memset(&cloned_instruction, 0, sizeof(cloned_instruction));
    cloned_instruction.kind = definition->kind;
    cloned_instruction.has_result = 1;
    cloned_instruction.result = value_ssa_value_id(value_ssa_function_allocate_value(function));
    if (cloned_instruction.result.value_id == (size_t)-1) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-299: failed to allocate perf invariant value");
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
    case VALUE_SSA_INSTR_MOV:
        if (!value_ssa_perf_clone_loop_invariant_value_into_preheader(
                program,
                function,
                analysis,
                readonly_globals,
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
        if (!value_ssa_perf_clone_loop_invariant_value_into_preheader(
                program,
                function,
                analysis,
                readonly_globals,
                loop,
                definition->as.binary.lhs,
                has_mapping,
                mappings,
                rebuilt_preheader,
                &cloned_instruction.as.binary.lhs,
                error) ||
            !value_ssa_perf_clone_loop_invariant_value_into_preheader(
                program,
                function,
                analysis,
                readonly_globals,
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
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-300: unsupported perf invariant clone instruction kind %d",
            (int)definition->kind);
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

static int value_ssa_perf_hoist_simple_loop_invariant_loads(ValueSsaProgram *program,
    ValueSsaError *error) {
    unsigned char *readonly_globals = NULL;
    size_t function_index;
    int changed_any = 0;

    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-292: invalid perf loop-load hoist contract");
        return 0;
    }

    readonly_globals = (unsigned char *)calloc(program->global_count ? program->global_count : 1u, sizeof(unsigned char));
    if (!readonly_globals) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-293: out of memory preparing perf loop-load hoist globals");
        return 0;
    }
    if (!value_ssa_perf_collect_readonly_scalar_globals(program, readonly_globals)) {
        free(readonly_globals);
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-294: failed collecting perf loop-load hoist globals");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunction *function = &program->functions[function_index];
        ValueSsaCfgAnalysis cfg;
        ValueSsaDefUseAnalysis def_use;
        size_t header_id;
        int changed_function = 0;

        if (!function->has_body) {
            continue;
        }

        value_ssa_cfg_analysis_init(&cfg);
        value_ssa_def_use_analysis_init(&def_use);
        if (!value_ssa_compute_cfg_analysis(function, &cfg, error) ||
            !value_ssa_compute_def_use_analysis(function, &def_use, error)) {
            value_ssa_def_use_analysis_free(&def_use);
            value_ssa_cfg_analysis_free(&cfg);
            free(readonly_globals);
            return 0;
        }

        for (header_id = 0; header_id < function->block_count; ++header_id) {
            ValueSsaPerfSimpleLoop loop;
            ValueSsaBasicBlock *preheader;
            ValueSsaBasicBlock *header;
            ValueSsaBasicBlock *body;
            ValueSsaBasicBlock rebuilt_preheader;
            ValueSsaBasicBlock rebuilt_header;
            ValueSsaBasicBlock rebuilt_body;
            unsigned char *address_taken_locals = NULL;
            unsigned char *parameter_pointer_non_alias_pairs = NULL;
            unsigned char *value_has_mapping = NULL;
            ValueSsaValueRef *value_mappings = NULL;
            unsigned char *local_has_mapping = NULL;
            ValueSsaValueRef *local_mappings = NULL;
            unsigned char *global_has_mapping = NULL;
            ValueSsaValueRef *global_mappings = NULL;
            size_t instruction_index;
            int changed_loop = 0;
            int phi_header_only = 0;
            int ok = 1;

            if (!value_ssa_perf_find_simple_single_body_loop(function, &cfg, header_id, &loop) &&
                !value_ssa_perf_find_simple_branch_body_loop(function, &cfg, header_id, &loop)) {
                if (!value_ssa_perf_find_phi_header_single_body_loop(function, &cfg, header_id, &loop)) {
                    continue;
                }
                phi_header_only = 1;
            }

            preheader = &function->blocks[loop.preheader_id];
            header = &function->blocks[loop.header_id];
            body = &function->blocks[loop.body_id];
            if (preheader->instruction_count == 0u ||
                (header->instruction_count == 0u && body->instruction_count == 0u)) {
                continue;
            }

            memset(&rebuilt_preheader, 0, sizeof(rebuilt_preheader));
            memset(&rebuilt_header, 0, sizeof(rebuilt_header));
            memset(&rebuilt_body, 0, sizeof(rebuilt_body));
            rebuilt_preheader.id = preheader->id;
            rebuilt_header.id = header->id;
            rebuilt_body.id = body->id;

            if (function->local_count > 0u) {
                address_taken_locals = (unsigned char *)calloc(function->local_count, sizeof(unsigned char));
            }
            if (function->parameter_count > 0u) {
                parameter_pointer_non_alias_pairs = (unsigned char *)calloc(
                    function->parameter_count * function->parameter_count,
                    sizeof(unsigned char));
            }
            if (function->next_value_id > 0u) {
                value_has_mapping = (unsigned char *)calloc(function->next_value_id, sizeof(unsigned char));
                value_mappings = (ValueSsaValueRef *)calloc(function->next_value_id, sizeof(ValueSsaValueRef));
            }
            if (function->local_count > 0u) {
                local_has_mapping = (unsigned char *)calloc(function->local_count, sizeof(unsigned char));
                local_mappings = (ValueSsaValueRef *)calloc(function->local_count, sizeof(ValueSsaValueRef));
            }
            if (program->global_count > 0u) {
                global_has_mapping = (unsigned char *)calloc(program->global_count, sizeof(unsigned char));
                global_mappings = (ValueSsaValueRef *)calloc(program->global_count, sizeof(ValueSsaValueRef));
            }
            if ((function->local_count > 0u && !address_taken_locals) ||
                (function->parameter_count > 0u && !parameter_pointer_non_alias_pairs) ||
                (function->next_value_id > 0u && (!value_has_mapping || !value_mappings)) ||
                (function->local_count > 0u && (!local_has_mapping || !local_mappings)) ||
                (program->global_count > 0u && (!global_has_mapping || !global_mappings))) {
                free(parameter_pointer_non_alias_pairs);
                free(global_mappings);
                free(global_has_mapping);
                free(local_mappings);
                free(local_has_mapping);
                free(value_mappings);
                free(value_has_mapping);
                free(address_taken_locals);
                value_ssa_basic_block_free(&rebuilt_preheader);
                value_ssa_basic_block_free(&rebuilt_header);
                value_ssa_basic_block_free(&rebuilt_body);
                value_ssa_cfg_analysis_free(&cfg);
                free(readonly_globals);
                value_ssa_set_error(error, 0, 0, "VALUE-SSA-295: out of memory preparing perf loop-load hoist mappings");
                return 0;
            }
            if (address_taken_locals &&
                !value_ssa_perf_collect_address_taken_locals(function, address_taken_locals)) {
                free(parameter_pointer_non_alias_pairs);
                free(global_mappings);
                free(global_has_mapping);
                free(local_mappings);
                free(local_has_mapping);
                free(value_mappings);
                free(value_has_mapping);
                free(address_taken_locals);
                value_ssa_basic_block_free(&rebuilt_preheader);
                value_ssa_basic_block_free(&rebuilt_header);
                value_ssa_basic_block_free(&rebuilt_body);
                value_ssa_cfg_analysis_free(&cfg);
                free(readonly_globals);
                value_ssa_set_error(error, 0, 0, "VALUE-SSA-295: failed collecting perf local address-taken flags");
                return 0;
            }
            if (parameter_pointer_non_alias_pairs &&
                !value_ssa_perf_collect_non_alias_parameter_pointer_pairs(
                    program, function, parameter_pointer_non_alias_pairs)) {
                free(parameter_pointer_non_alias_pairs);
                free(global_mappings);
                free(global_has_mapping);
                free(local_mappings);
                free(local_has_mapping);
                free(value_mappings);
                free(value_has_mapping);
                free(address_taken_locals);
                value_ssa_basic_block_free(&rebuilt_preheader);
                value_ssa_basic_block_free(&rebuilt_header);
                value_ssa_basic_block_free(&rebuilt_body);
                value_ssa_cfg_analysis_free(&cfg);
                free(readonly_globals);
                value_ssa_set_error(
                    error, 0, 0, "VALUE-SSA-295: failed collecting perf parameter non-alias pairs");
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

                if ((instruction->kind == VALUE_SSA_INSTR_ADDR_LOCAL ||
                        instruction->kind == VALUE_SSA_INSTR_ADDR_GLOBAL) &&
                    instruction->has_result &&
                    instruction->result.kind == VALUE_SSA_VALUE_ID &&
                    value_ssa_perf_value_is_loop_invariant(
                        program,
                        function,
                        &def_use,
                        readonly_globals,
                        &loop,
                        instruction->result,
                        0u)) {
                    ValueSsaInstruction replacement;
                    ValueSsaValueRef hoisted_value;

                    if (!value_ssa_perf_clone_loop_invariant_value_into_preheader(
                            program,
                            function,
                            &def_use,
                            readonly_globals,
                            &loop,
                            instruction->result,
                            value_has_mapping,
                            value_mappings,
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
                    changed_loop = 1;
                    continue;
                }

                if (instruction->kind == VALUE_SSA_INSTR_LOAD_LOCAL &&
                    instruction->has_result &&
                    instruction->result.kind == VALUE_SSA_VALUE_ID &&
                    instruction->as.load_slot.kind == VALUE_SSA_SLOT_LOCAL &&
                    instruction->as.load_slot.id < function->local_count &&
                    function->locals[instruction->as.load_slot.id].is_parameter &&
                    value_ssa_perf_loop_preserves_local_slot(function, &loop, instruction->as.load_slot.id)) {
                    ValueSsaInstruction cloned_load;
                    ValueSsaInstruction replacement;
                    ValueSsaValueRef hoisted_value;
                    size_t slot_id = instruction->as.load_slot.id;

                    if (!local_has_mapping[slot_id]) {
                        memset(&cloned_load, 0, sizeof(cloned_load));
                        cloned_load.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
                        cloned_load.has_result = 1;
                        cloned_load.result = value_ssa_value_id(value_ssa_function_allocate_value(function));
                        cloned_load.as.load_slot = instruction->as.load_slot;
                        if (cloned_load.result.value_id == (size_t)-1 ||
                            !value_ssa_block_append_instruction(&rebuilt_preheader, &cloned_load, error)) {
                            ok = 0;
                            break;
                        }
                        local_has_mapping[slot_id] = 1u;
                        local_mappings[slot_id] = cloned_load.result;
                    }

                    hoisted_value = local_mappings[slot_id];
                    memset(&replacement, 0, sizeof(replacement));
                    replacement.kind = VALUE_SSA_INSTR_MOV;
                    replacement.has_result = 1;
                    replacement.result = instruction->result;
                    replacement.as.mov_value = hoisted_value;
                    if (!value_ssa_block_append_instruction(&rebuilt_header, &replacement, error)) {
                        ok = 0;
                        break;
                    }
                    changed_loop = 1;
                    continue;
                }

                if (instruction->kind == VALUE_SSA_INSTR_LOAD_GLOBAL &&
                    instruction->has_result &&
                    instruction->result.kind == VALUE_SSA_VALUE_ID &&
                    instruction->as.load_slot.kind == VALUE_SSA_SLOT_GLOBAL &&
                    instruction->as.load_slot.id < program->global_count &&
                    value_ssa_perf_loop_preserves_global_slot(
                        program, function, &loop, readonly_globals, instruction->as.load_slot.id)) {
                    ValueSsaInstruction cloned_load;
                    ValueSsaInstruction replacement;
                    ValueSsaValueRef hoisted_value;
                    size_t slot_id = instruction->as.load_slot.id;

                    if (!global_has_mapping[slot_id]) {
                        memset(&cloned_load, 0, sizeof(cloned_load));
                        cloned_load.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
                        cloned_load.has_result = 1;
                        cloned_load.result = value_ssa_value_id(value_ssa_function_allocate_value(function));
                        cloned_load.as.load_slot = instruction->as.load_slot;
                        if (cloned_load.result.value_id == (size_t)-1 ||
                            !value_ssa_block_append_instruction(&rebuilt_preheader, &cloned_load, error)) {
                            ok = 0;
                            break;
                        }
                        global_has_mapping[slot_id] = 1u;
                        global_mappings[slot_id] = cloned_load.result;
                    }

                    hoisted_value = global_mappings[slot_id];
                    memset(&replacement, 0, sizeof(replacement));
                    replacement.kind = VALUE_SSA_INSTR_MOV;
                    replacement.has_result = 1;
                    replacement.result = instruction->result;
                    replacement.as.mov_value = hoisted_value;
                    if (!value_ssa_block_append_instruction(&rebuilt_header, &replacement, error)) {
                        ok = 0;
                        break;
                    }
                    changed_loop = 1;
                    continue;
                }

                if ((instruction->kind == VALUE_SSA_INSTR_MOV ||
                        instruction->kind == VALUE_SSA_INSTR_BINARY) &&
                    instruction->has_result &&
                    instruction->result.kind == VALUE_SSA_VALUE_ID &&
                    value_ssa_perf_value_is_loop_invariant(
                        program,
                        function,
                        &def_use,
                        readonly_globals,
                        &loop,
                        instruction->result,
                        0u) &&
                    (instruction->kind != VALUE_SSA_INSTR_MOV ||
                        instruction->as.mov_value.kind == VALUE_SSA_VALUE_IMMEDIATE ||
                        value_ssa_perf_value_is_defined_inside_loop(
                            function,
                            &def_use,
                            &loop,
                            instruction->as.mov_value))) {
                    ValueSsaInstruction replacement;
                    ValueSsaValueRef hoisted_value;

                    if (!value_ssa_perf_clone_loop_invariant_value_into_preheader(
                            program,
                            function,
                            &def_use,
                            readonly_globals,
                            &loop,
                            instruction->result,
                            value_has_mapping,
                            value_mappings,
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
                    changed_loop = 1;
                    continue;
                }

                {
                    int hoisted_branch_bound = 0;
                    int hoisted_indirect = 0;

                    if (!value_ssa_perf_try_hoist_loop_invariant_branch_bound_load(
                            function,
                            program,
                            &def_use,
                            readonly_globals,
                            address_taken_locals,
                            parameter_pointer_non_alias_pairs,
                            &loop,
                            value_has_mapping,
                            value_mappings,
                            &rebuilt_preheader,
                            &rebuilt_header,
                            instruction,
                            &hoisted_branch_bound,
                            error)) {
                        ok = 0;
                        break;
                    }
                    if (hoisted_branch_bound) {
                        changed_loop = 1;
                        continue;
                    }
                    if (phi_header_only) {
                        if (!value_ssa_block_append_instruction(&rebuilt_header, instruction, error)) {
                            ok = 0;
                            break;
                        }
                        continue;
                    }

                    if (!value_ssa_perf_try_hoist_loop_invariant_indirect_load(
                            function,
                            program,
                            &def_use,
                            readonly_globals,
                            address_taken_locals,
                            parameter_pointer_non_alias_pairs,
                            &loop,
                            value_has_mapping,
                            value_mappings,
                            &rebuilt_preheader,
                            &rebuilt_header,
                            instruction,
                            &hoisted_indirect,
                            error)) {
                        ok = 0;
                        break;
                    }
                    if (hoisted_indirect) {
                        changed_loop = 1;
                        continue;
                    }
                }

                if (!value_ssa_block_append_instruction(&rebuilt_header, instruction, error)) {
                    ok = 0;
                    break;
                }
            }

            for (instruction_index = 0; ok && instruction_index < body->instruction_count; ++instruction_index) {
                const ValueSsaInstruction *instruction = &body->instructions[instruction_index];

                if ((instruction->kind == VALUE_SSA_INSTR_ADDR_LOCAL ||
                        instruction->kind == VALUE_SSA_INSTR_ADDR_GLOBAL) &&
                    instruction->has_result &&
                    instruction->result.kind == VALUE_SSA_VALUE_ID &&
                    value_ssa_perf_value_is_loop_invariant(
                        program,
                        function,
                        &def_use,
                        readonly_globals,
                        &loop,
                        instruction->result,
                        0u)) {
                    ValueSsaInstruction replacement;
                    ValueSsaValueRef hoisted_value;

                    if (!value_ssa_perf_clone_loop_invariant_value_into_preheader(
                            program,
                            function,
                            &def_use,
                            readonly_globals,
                            &loop,
                            instruction->result,
                            value_has_mapping,
                            value_mappings,
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
                    if (!value_ssa_block_append_instruction(&rebuilt_body, &replacement, error)) {
                        ok = 0;
                        break;
                    }
                    changed_loop = 1;
                    continue;
                }

                if (instruction->kind == VALUE_SSA_INSTR_LOAD_LOCAL &&
                    instruction->has_result &&
                    instruction->result.kind == VALUE_SSA_VALUE_ID &&
                    instruction->as.load_slot.kind == VALUE_SSA_SLOT_LOCAL &&
                    instruction->as.load_slot.id < function->local_count &&
                    function->locals[instruction->as.load_slot.id].is_parameter &&
                    value_ssa_perf_loop_preserves_local_slot(function, &loop, instruction->as.load_slot.id)) {
                    ValueSsaInstruction cloned_load;
                    ValueSsaInstruction replacement;
                    ValueSsaValueRef hoisted_value;
                    size_t slot_id = instruction->as.load_slot.id;

                    if (!local_has_mapping[slot_id]) {
                        memset(&cloned_load, 0, sizeof(cloned_load));
                        cloned_load.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
                        cloned_load.has_result = 1;
                        cloned_load.result = value_ssa_value_id(value_ssa_function_allocate_value(function));
                        cloned_load.as.load_slot = instruction->as.load_slot;
                        if (cloned_load.result.value_id == (size_t)-1 ||
                            !value_ssa_block_append_instruction(&rebuilt_preheader, &cloned_load, error)) {
                            ok = 0;
                            break;
                        }
                        local_has_mapping[slot_id] = 1u;
                        local_mappings[slot_id] = cloned_load.result;
                    }

                    hoisted_value = local_mappings[slot_id];
                    memset(&replacement, 0, sizeof(replacement));
                    replacement.kind = VALUE_SSA_INSTR_MOV;
                    replacement.has_result = 1;
                    replacement.result = instruction->result;
                    replacement.as.mov_value = hoisted_value;
                    if (!value_ssa_block_append_instruction(&rebuilt_body, &replacement, error)) {
                        ok = 0;
                        break;
                    }
                    changed_loop = 1;
                    continue;
                }

                if (instruction->kind == VALUE_SSA_INSTR_LOAD_GLOBAL &&
                    instruction->has_result &&
                    instruction->result.kind == VALUE_SSA_VALUE_ID &&
                    instruction->as.load_slot.kind == VALUE_SSA_SLOT_GLOBAL &&
                    instruction->as.load_slot.id < program->global_count &&
                    value_ssa_perf_loop_preserves_global_slot(
                        program, function, &loop, readonly_globals, instruction->as.load_slot.id)) {
                    ValueSsaInstruction cloned_load;
                    ValueSsaInstruction replacement;
                    ValueSsaValueRef hoisted_value;
                    size_t slot_id = instruction->as.load_slot.id;

                    if (!global_has_mapping[slot_id]) {
                        memset(&cloned_load, 0, sizeof(cloned_load));
                        cloned_load.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
                        cloned_load.has_result = 1;
                        cloned_load.result = value_ssa_value_id(value_ssa_function_allocate_value(function));
                        cloned_load.as.load_slot = instruction->as.load_slot;
                        if (cloned_load.result.value_id == (size_t)-1 ||
                            !value_ssa_block_append_instruction(&rebuilt_preheader, &cloned_load, error)) {
                            ok = 0;
                            break;
                        }
                        global_has_mapping[slot_id] = 1u;
                        global_mappings[slot_id] = cloned_load.result;
                    }

                    hoisted_value = global_mappings[slot_id];
                    memset(&replacement, 0, sizeof(replacement));
                    replacement.kind = VALUE_SSA_INSTR_MOV;
                    replacement.has_result = 1;
                    replacement.result = instruction->result;
                    replacement.as.mov_value = hoisted_value;
                    if (!value_ssa_block_append_instruction(&rebuilt_body, &replacement, error)) {
                        ok = 0;
                        break;
                    }
                    changed_loop = 1;
                    continue;
                }

                if ((instruction->kind == VALUE_SSA_INSTR_MOV ||
                        instruction->kind == VALUE_SSA_INSTR_BINARY) &&
                    instruction->has_result &&
                    instruction->result.kind == VALUE_SSA_VALUE_ID &&
                    value_ssa_perf_value_is_loop_invariant(
                        program,
                        function,
                        &def_use,
                        readonly_globals,
                        &loop,
                        instruction->result,
                        0u) &&
                    (instruction->kind != VALUE_SSA_INSTR_MOV ||
                        instruction->as.mov_value.kind == VALUE_SSA_VALUE_IMMEDIATE ||
                        value_ssa_perf_value_is_defined_inside_loop(
                            function,
                            &def_use,
                            &loop,
                            instruction->as.mov_value))) {
                    ValueSsaInstruction replacement;
                    ValueSsaValueRef hoisted_value;

                    if (!value_ssa_perf_clone_loop_invariant_value_into_preheader(
                            program,
                            function,
                            &def_use,
                            readonly_globals,
                            &loop,
                            instruction->result,
                            value_has_mapping,
                            value_mappings,
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
                    if (!value_ssa_block_append_instruction(&rebuilt_body, &replacement, error)) {
                        ok = 0;
                        break;
                    }
                    changed_loop = 1;
                    continue;
                }

                {
                    int hoisted_branch_bound = 0;
                    int hoisted_indirect = 0;

                    if (!value_ssa_perf_try_hoist_loop_invariant_branch_bound_load(
                            function,
                            program,
                            &def_use,
                            readonly_globals,
                            address_taken_locals,
                            parameter_pointer_non_alias_pairs,
                            &loop,
                            value_has_mapping,
                            value_mappings,
                            &rebuilt_preheader,
                            &rebuilt_body,
                            instruction,
                            &hoisted_branch_bound,
                            error)) {
                        ok = 0;
                        break;
                    }
                    if (hoisted_branch_bound) {
                        changed_loop = 1;
                        continue;
                    }
                    if (!value_ssa_perf_try_hoist_loop_invariant_indirect_load(
                            function,
                            program,
                            &def_use,
                            readonly_globals,
                            address_taken_locals,
                            parameter_pointer_non_alias_pairs,
                            &loop,
                            value_has_mapping,
                            value_mappings,
                            &rebuilt_preheader,
                            &rebuilt_body,
                            instruction,
                            &hoisted_indirect,
                            error)) {
                        ok = 0;
                        break;
                    }
                    if (hoisted_indirect) {
                        changed_loop = 1;
                        continue;
                    }
                    if (phi_header_only) {
                        if (!value_ssa_block_append_instruction(&rebuilt_body, instruction, error)) {
                            ok = 0;
                            break;
                        }
                        continue;
                    }
                }

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

                for (instruction_index = 0; instruction_index < preheader->instruction_count; ++instruction_index) {
                    value_ssa_instruction_free(&preheader->instructions[instruction_index]);
                }
                free(preheader->instructions);
                preheader->instructions = rebuilt_preheader.instructions;
                preheader->instruction_count = rebuilt_preheader.instruction_count;
                preheader->instruction_capacity = rebuilt_preheader.instruction_capacity;
                rebuilt_preheader.instructions = NULL;
                rebuilt_preheader.instruction_count = 0u;
                rebuilt_preheader.instruction_capacity = 0u;

                for (instruction_index = 0; instruction_index < header->instruction_count; ++instruction_index) {
                    value_ssa_instruction_free(&header->instructions[instruction_index]);
                }
                free(header->instructions);
                header->instructions = rebuilt_header.instructions;
                header->instruction_count = rebuilt_header.instruction_count;
                header->instruction_capacity = rebuilt_header.instruction_capacity;
                rebuilt_header.instructions = NULL;
                rebuilt_header.instruction_count = 0u;
                rebuilt_header.instruction_capacity = 0u;

                for (instruction_index = 0; instruction_index < body->instruction_count; ++instruction_index) {
                    value_ssa_instruction_free(&body->instructions[instruction_index]);
                }
                free(body->instructions);
                body->instructions = rebuilt_body.instructions;
                body->instruction_count = rebuilt_body.instruction_count;
                body->instruction_capacity = rebuilt_body.instruction_capacity;
                rebuilt_body.instructions = NULL;
                rebuilt_body.instruction_count = 0u;
                rebuilt_body.instruction_capacity = 0u;

                changed_any = 1;
                changed_function = 1;
            }

            value_ssa_basic_block_free(&rebuilt_preheader);
            value_ssa_basic_block_free(&rebuilt_header);
            value_ssa_basic_block_free(&rebuilt_body);
            free(global_mappings);
            free(global_has_mapping);
            free(local_mappings);
            free(local_has_mapping);
            free(value_mappings);
            free(value_has_mapping);
            free(parameter_pointer_non_alias_pairs);
            free(address_taken_locals);

            if (!ok) {
                value_ssa_def_use_analysis_free(&def_use);
                value_ssa_cfg_analysis_free(&cfg);
                free(readonly_globals);
                return 0;
            }
            if (changed_loop) {
                break;
            }
        }

        value_ssa_def_use_analysis_free(&def_use);
        value_ssa_cfg_analysis_free(&cfg);

        if (changed_function) {
            --function_index;
        }
    }

    free(readonly_globals);

    if (changed_any &&
        (!value_ssa_simplify_trivial_values(program, error) ||
            !value_ssa_eliminate_dead_value_defs(program, error))) {
        return 0;
    }

    return value_ssa_verify_program(program, error);
}

static int value_ssa_perf_reuse_spmv_loop_exit_indirect_loads(ValueSsaProgram *program,
    ValueSsaError *error) {
    unsigned char *readonly_globals = NULL;
    size_t function_index;
    int changed_any = 0;

    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-307: invalid perf spmv loop-exit reuse contract");
        return 0;
    }

    readonly_globals = (unsigned char *)calloc(program->global_count ? program->global_count : 1u, sizeof(unsigned char));
    if (!readonly_globals) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-308: out of memory preparing perf spmv loop-exit reuse globals");
        return 0;
    }
    if (!value_ssa_perf_collect_readonly_scalar_globals(program, readonly_globals)) {
        free(readonly_globals);
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-309: failed collecting perf spmv loop-exit reuse globals");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunction *function = &program->functions[function_index];
        ValueSsaCfgAnalysis cfg;
        ValueSsaDefUseAnalysis def_use;
        unsigned char *address_taken_locals = NULL;
        unsigned char *parameter_pointer_non_alias_pairs = NULL;
        size_t header_id;
        int changed_function = 0;

        if (!function->has_body || !function->name || strcmp(function->name, "spmv") != 0 ||
            value_ssa_perf_function_has_calls(function)) {
            continue;
        }

        value_ssa_cfg_analysis_init(&cfg);
        value_ssa_def_use_analysis_init(&def_use);
        if (!value_ssa_compute_cfg_analysis(function, &cfg, error) ||
            !value_ssa_compute_def_use_analysis(function, &def_use, error)) {
            value_ssa_def_use_analysis_free(&def_use);
            value_ssa_cfg_analysis_free(&cfg);
            free(readonly_globals);
            return 0;
        }

        if (function->local_count > 0u) {
            address_taken_locals = (unsigned char *)calloc(function->local_count, sizeof(unsigned char));
        }
        if (function->parameter_count > 0u) {
            parameter_pointer_non_alias_pairs = (unsigned char *)calloc(
                function->parameter_count * function->parameter_count,
                sizeof(unsigned char));
        }
        if ((function->local_count > 0u && !address_taken_locals) ||
            (function->parameter_count > 0u && !parameter_pointer_non_alias_pairs)) {
            free(parameter_pointer_non_alias_pairs);
            free(address_taken_locals);
            value_ssa_def_use_analysis_free(&def_use);
            value_ssa_cfg_analysis_free(&cfg);
            free(readonly_globals);
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-310: out of memory preparing perf spmv loop-exit reuse state");
            return 0;
        }
        if (address_taken_locals &&
            !value_ssa_perf_collect_address_taken_locals(function, address_taken_locals)) {
            free(parameter_pointer_non_alias_pairs);
            free(address_taken_locals);
            value_ssa_def_use_analysis_free(&def_use);
            value_ssa_cfg_analysis_free(&cfg);
            free(readonly_globals);
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-311: failed collecting perf spmv address-taken locals");
            return 0;
        }
        if (parameter_pointer_non_alias_pairs &&
            !value_ssa_perf_collect_non_alias_parameter_pointer_pairs(
                program, function, parameter_pointer_non_alias_pairs)) {
            free(parameter_pointer_non_alias_pairs);
            free(address_taken_locals);
            value_ssa_def_use_analysis_free(&def_use);
            value_ssa_cfg_analysis_free(&cfg);
            free(readonly_globals);
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-312: failed collecting perf spmv parameter non-alias pairs");
            return 0;
        }

        for (header_id = 0; header_id < function->block_count; ++header_id) {
            ValueSsaPerfSimpleLoop loop;
            ValueSsaBasicBlock *preheader;
            ValueSsaBasicBlock *exit_block;
            size_t exit_instruction_index;
            int loop_matches = 0;

            if (value_ssa_perf_find_phi_header_single_body_loop(function, &cfg, header_id, &loop) ||
                value_ssa_perf_find_simple_single_body_loop(function, &cfg, header_id, &loop) ||
                value_ssa_perf_find_simple_branch_body_loop(function, &cfg, header_id, &loop)) {
                loop_matches = 1;
            }
            if (!loop_matches ||
                loop.preheader_id >= function->block_count ||
                loop.header_id >= function->block_count ||
                loop.body_id >= function->block_count ||
                loop.exit_id >= function->block_count ||
                !cfg.dominates[loop.preheader_id * function->block_count + loop.exit_id]) {
                continue;
            }

            preheader = &function->blocks[loop.preheader_id];
            exit_block = &function->blocks[loop.exit_id];
            if (preheader->instruction_count == 0u || exit_block->instruction_count == 0u) {
                continue;
            }

            for (exit_instruction_index = 0; exit_instruction_index < exit_block->instruction_count; ++exit_instruction_index) {
                ValueSsaInstruction *exit_instruction = &exit_block->instructions[exit_instruction_index];
                ValueSsaPerfAddressRoot exit_load_root;
                size_t preheader_instruction_index;

                if (exit_instruction->kind != VALUE_SSA_INSTR_LOAD_INDIRECT ||
                    !exit_instruction->has_result ||
                    exit_instruction->result.kind != VALUE_SSA_VALUE_ID ||
                    !value_ssa_perf_value_is_loop_invariant_for_indirect_addr(
                        program,
                        function,
                        &def_use,
                        readonly_globals,
                        address_taken_locals,
                        &loop,
                        exit_instruction->as.load_indirect_addr,
                        0u) ||
                    !value_ssa_perf_extract_address_root(
                        function, &def_use, exit_instruction->as.load_indirect_addr, &exit_load_root, 0u) ||
                    !value_ssa_perf_loop_instructions_preserve_indirect_load(
                        function,
                        &def_use,
                        &function->blocks[loop.header_id],
                        parameter_pointer_non_alias_pairs,
                        &exit_load_root) ||
                    !value_ssa_perf_loop_instructions_preserve_indirect_load(
                        function,
                        &def_use,
                        &function->blocks[loop.body_id],
                        parameter_pointer_non_alias_pairs,
                        &exit_load_root) ||
                    !value_ssa_perf_instruction_range_preserves_indirect_load(
                        function,
                        &def_use,
                        exit_block,
                        0u,
                        exit_instruction_index,
                        parameter_pointer_non_alias_pairs,
                        &exit_load_root)) {
                    continue;
                }

                for (preheader_instruction_index = preheader->instruction_count; preheader_instruction_index > 0u; --preheader_instruction_index) {
                    ValueSsaInstruction *preheader_instruction = &preheader->instructions[preheader_instruction_index - 1u];
                    ValueSsaPerfAddressRoot preheader_load_root;

                    if (preheader_instruction->kind != VALUE_SSA_INSTR_LOAD_INDIRECT ||
                        !preheader_instruction->has_result ||
                        preheader_instruction->result.kind != VALUE_SSA_VALUE_ID ||
                        !value_ssa_perf_same_pure_address_value(
                            function,
                            &def_use,
                            preheader_instruction->as.load_indirect_addr,
                            exit_instruction->as.load_indirect_addr,
                            0u) ||
                        !value_ssa_perf_extract_address_root(
                            function,
                            &def_use,
                            preheader_instruction->as.load_indirect_addr,
                            &preheader_load_root,
                            0u) ||
                        !value_ssa_perf_instruction_range_preserves_indirect_load(
                            function,
                            &def_use,
                            preheader,
                            preheader_instruction_index,
                            preheader->instruction_count,
                            parameter_pointer_non_alias_pairs,
                            &preheader_load_root)) {
                        continue;
                    }

                    exit_instruction->kind = VALUE_SSA_INSTR_MOV;
                    exit_instruction->as.mov_value = preheader_instruction->result;
                    changed_function = 1;
                    changed_any = 1;
                    break;
                }
            }
        }

        free(parameter_pointer_non_alias_pairs);
        free(address_taken_locals);
        value_ssa_def_use_analysis_free(&def_use);
        value_ssa_cfg_analysis_free(&cfg);

        if (changed_function) {
            --function_index;
        }
    }

    free(readonly_globals);

    if (changed_any &&
        (!value_ssa_simplify_trivial_values(program, error) ||
            !value_ssa_eliminate_dead_value_defs(program, error))) {
        return 0;
    }

    return value_ssa_verify_program(program, error);
}


#endif
