#if defined(__INTELLISENSE__) && !defined(MEMORY_SSA_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MEMORY_SSA_SPLIT_AGGREGATOR)
#define MEMORY_SSA_SPLIT_INTELLISENSE_CONTEXT
#include "memory_ssa.c"
#else

void memory_ssa_def_use_analysis_init(MemorySsaDefUseAnalysis *analysis) {
    if (!analysis) {
        return;
    }

    analysis->version_count = 0;
    analysis->def_slot_ids = NULL;
    analysis->def_kinds = NULL;
    analysis->def_block_ids = NULL;
    analysis->def_phi_indices = NULL;
    analysis->def_instruction_indices = NULL;
    analysis->has_def = NULL;
    analysis->use_counts = NULL;
    analysis->use_offsets = NULL;
    analysis->use_sites = NULL;
}

void memory_ssa_def_use_analysis_free(MemorySsaDefUseAnalysis *analysis) {
    if (!analysis) {
        return;
    }

    free(analysis->def_slot_ids);
    free(analysis->def_kinds);
    free(analysis->def_block_ids);
    free(analysis->def_phi_indices);
    free(analysis->def_instruction_indices);
    free(analysis->has_def);
    free(analysis->use_counts);
    free(analysis->use_offsets);
    free(analysis->use_sites);
    memory_ssa_def_use_analysis_init(analysis);
}

static int memory_ssa_value_refs_equal(ValueSsaValueRef lhs, ValueSsaValueRef rhs) {
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

static int memory_ssa_def_use_set_definition(MemorySsaDefUseAnalysis *analysis,
    size_t version_id,
    size_t slot_id,
    MemorySsaVersionDefKind def_kind,
    size_t block_id,
    size_t phi_index,
    size_t instruction_index,
    MemorySsaError *error) {
    if (!analysis || version_id >= analysis->version_count) {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-087: invalid definition record while computing def-use");
        return 0;
    }
    if (analysis->has_def[version_id]) {
        memory_ssa_set_error(
            error, 0, 0, "MEMORY-SSA-087: mem.%zu is defined multiple times during def-use analysis", version_id);
        return 0;
    }

    analysis->def_slot_ids[version_id] = slot_id;
    analysis->def_kinds[version_id] = def_kind;
    analysis->def_block_ids[version_id] = block_id;
    analysis->def_phi_indices[version_id] = phi_index;
    analysis->def_instruction_indices[version_id] = instruction_index;
    analysis->has_def[version_id] = 1;
    return 1;
}

static void memory_ssa_record_version_use_count(size_t version_id, size_t version_count, size_t *use_counts) {
    if (!use_counts || version_id >= version_count) {
        return;
    }

    ++use_counts[version_id];
}

static int memory_ssa_write_use_site(size_t version_id,
    MemorySsaVersionUseKind use_kind,
    MemorySsaVersionUseRole use_role,
    size_t block_id,
    size_t phi_index,
    size_t instruction_index,
    size_t access_index,
    size_t predecessor_block_id,
    size_t *cursor,
    size_t *use_offsets,
    MemorySsaVersionUseSite *use_sites) {
    size_t slot;

    if (!cursor || !use_offsets || !use_sites) {
        return 0;
    }

    slot = use_offsets[version_id] + cursor[version_id];
    use_sites[slot].kind = use_kind;
    use_sites[slot].role = use_role;
    use_sites[slot].block_id = block_id;
    use_sites[slot].phi_index = phi_index;
    use_sites[slot].instruction_index = instruction_index;
    use_sites[slot].access_index = access_index;
    use_sites[slot].predecessor_block_id = predecessor_block_id;
    ++cursor[version_id];
    return 1;
}

static int memory_ssa_def_use_instruction_role(const MemorySsaInstruction *instruction,
    MemorySsaVersionUseRole *out_role,
    MemorySsaError *error) {
    if (!instruction || !out_role) {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-090: invalid instruction-use-role contract");
        return 0;
    }

    switch (instruction->instruction.kind) {
    case VALUE_SSA_INSTR_LOAD_LOCAL:
    case VALUE_SSA_INSTR_LOAD_GLOBAL:
        *out_role = MEMORY_SSA_VERSION_USE_ROLE_LOAD;
        return 1;
    case VALUE_SSA_INSTR_STORE_LOCAL:
    case VALUE_SSA_INSTR_STORE_GLOBAL:
        *out_role = MEMORY_SSA_VERSION_USE_ROLE_STORE_PREVIOUS;
        return 1;
    case VALUE_SSA_INSTR_CALL:
        *out_role = MEMORY_SSA_VERSION_USE_ROLE_CALL;
        return 1;
    default:
        memory_ssa_set_error(error,
            0,
            0,
            "MEMORY-SSA-090: instruction kind %d cannot read a memory version",
            (int)instruction->instruction.kind);
        return 0;
    }
}

int memory_ssa_compute_def_use_analysis(const MemorySsaFunction *function,
    MemorySsaDefUseAnalysis *analysis,
    MemorySsaError *error) {
    size_t total_use_count = 0;
    size_t block_index;
    size_t version_index;
    size_t *cursor = NULL;

    if (!function || !analysis) {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-084: invalid def-use analysis contract");
        return 0;
    }
    if (!function->has_body) {
        memory_ssa_set_error(error,
            0,
            0,
            "MEMORY-SSA-085: cannot compute def-use analysis for declaration '%s'",
            function->name ? function->name : "<anon>");
        return 0;
    }

    memory_ssa_def_use_analysis_free(analysis);
    analysis->version_count = function->next_version_id;
    if (analysis->version_count == 0) {
        return 1;
    }

    analysis->def_slot_ids = (size_t *)malloc(analysis->version_count * sizeof(size_t));
    analysis->def_kinds =
        (MemorySsaVersionDefKind *)malloc(analysis->version_count * sizeof(MemorySsaVersionDefKind));
    analysis->def_block_ids = (size_t *)malloc(analysis->version_count * sizeof(size_t));
    analysis->def_phi_indices = (size_t *)malloc(analysis->version_count * sizeof(size_t));
    analysis->def_instruction_indices = (size_t *)malloc(analysis->version_count * sizeof(size_t));
    analysis->has_def = (unsigned char *)calloc(analysis->version_count, sizeof(unsigned char));
    analysis->use_counts = (size_t *)calloc(analysis->version_count, sizeof(size_t));
    analysis->use_offsets = (size_t *)calloc(analysis->version_count, sizeof(size_t));
    if (!analysis->def_slot_ids || !analysis->def_kinds || !analysis->def_block_ids || !analysis->def_phi_indices ||
        !analysis->def_instruction_indices || !analysis->has_def || !analysis->use_counts ||
        !analysis->use_offsets) {
        memory_ssa_def_use_analysis_free(analysis);
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-086: out of memory while preparing def-use analysis");
        return 0;
    }

    for (version_index = 0; version_index < analysis->version_count; ++version_index) {
        analysis->def_slot_ids[version_index] = (size_t)-1;
        analysis->def_block_ids[version_index] = (size_t)-1;
        analysis->def_phi_indices[version_index] = (size_t)-1;
        analysis->def_instruction_indices[version_index] = (size_t)-1;
    }

    for (version_index = 0; version_index < function->tracked_slot_count; ++version_index) {
        const MemorySsaTrackedSlot *slot = &function->tracked_slots[version_index];

        if (slot->has_entry_version &&
            !memory_ssa_def_use_set_definition(analysis,
                slot->entry_version_id,
                slot->id,
                MEMORY_SSA_VERSION_DEF_ENTRY,
                (size_t)-1,
                (size_t)-1,
                (size_t)-1,
                error)) {
            memory_ssa_def_use_analysis_free(analysis);
            return 0;
        }
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        const MemorySsaBasicBlock *block = &function->blocks[block_index];
        size_t phi_index;
        size_t instruction_index;

        for (phi_index = 0; phi_index < block->phi_count; ++phi_index) {
            const MemorySsaPhi *phi = &block->phis[phi_index];
            size_t input_index;

            if (phi->slot_id >= function->tracked_slot_count || phi->result_version_id >= analysis->version_count) {
                memory_ssa_def_use_analysis_free(analysis);
                memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-088: phi in bb.%zu references an invalid slot/version", block->id);
                return 0;
            }
            if (!memory_ssa_def_use_set_definition(analysis,
                    phi->result_version_id,
                    phi->slot_id,
                    MEMORY_SSA_VERSION_DEF_PHI,
                    block_index,
                    phi_index,
                    (size_t)-1,
                    error)) {
                memory_ssa_def_use_analysis_free(analysis);
                return 0;
            }
            for (input_index = 0; input_index < phi->input_count; ++input_index) {
                size_t version_id = phi->inputs[input_index].version_id;

                if (version_id >= analysis->version_count) {
                    memory_ssa_def_use_analysis_free(analysis);
                    memory_ssa_set_error(error,
                        0,
                        0,
                        "MEMORY-SSA-088: phi in bb.%zu references invalid input mem.%zu",
                        block->id,
                        version_id);
                    return 0;
                }
                memory_ssa_record_version_use_count(version_id, analysis->version_count, analysis->use_counts);
            }
        }

        for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
            const MemorySsaInstruction *instruction = &block->instructions[instruction_index];
            size_t access_index;

            for (access_index = 0; access_index < instruction->access_count; ++access_index) {
                const MemorySsaAccess *access = &instruction->accesses[access_index];

                if (access->slot_id >= function->tracked_slot_count) {
                    memory_ssa_def_use_analysis_free(analysis);
                    memory_ssa_set_error(error,
                        0,
                        0,
                        "MEMORY-SSA-089: bb.%zu instr %zu references invalid slot.%zu during def-use analysis",
                        block->id,
                        instruction_index,
                        access->slot_id);
                    return 0;
                }
                if (access->has_use_version) {
                    if (access->use_version_id >= analysis->version_count) {
                        memory_ssa_def_use_analysis_free(analysis);
                        memory_ssa_set_error(error,
                            0,
                            0,
                            "MEMORY-SSA-090: bb.%zu instr %zu references invalid use mem.%zu",
                            block->id,
                            instruction_index,
                            access->use_version_id);
                        return 0;
                    }
                    memory_ssa_record_version_use_count(
                        access->use_version_id, analysis->version_count, analysis->use_counts);
                }
                if (access->has_def_version) {
                    if (access->def_version_id >= analysis->version_count) {
                        memory_ssa_def_use_analysis_free(analysis);
                        memory_ssa_set_error(error,
                            0,
                            0,
                            "MEMORY-SSA-091: bb.%zu instr %zu defines invalid mem.%zu",
                            block->id,
                            instruction_index,
                            access->def_version_id);
                        return 0;
                    }
                    if (!memory_ssa_def_use_set_definition(analysis,
                            access->def_version_id,
                            access->slot_id,
                            MEMORY_SSA_VERSION_DEF_INSTR,
                            block_index,
                            (size_t)-1,
                            instruction_index,
                            error)) {
                        memory_ssa_def_use_analysis_free(analysis);
                        return 0;
                    }
                }
            }
        }
    }

    for (version_index = 0; version_index < analysis->version_count; ++version_index) {
        analysis->use_offsets[version_index] = total_use_count;
        total_use_count += analysis->use_counts[version_index];
    }

    if (total_use_count == 0) {
        return 1;
    }

    analysis->use_sites = (MemorySsaVersionUseSite *)calloc(total_use_count, sizeof(MemorySsaVersionUseSite));
    cursor = (size_t *)calloc(analysis->version_count, sizeof(size_t));
    if (!analysis->use_sites || !cursor) {
        free(cursor);
        memory_ssa_def_use_analysis_free(analysis);
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-086: out of memory while preparing def-use analysis");
        return 0;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        const MemorySsaBasicBlock *block = &function->blocks[block_index];
        size_t phi_index;
        size_t instruction_index;

        for (phi_index = 0; phi_index < block->phi_count; ++phi_index) {
            const MemorySsaPhi *phi = &block->phis[phi_index];
            size_t input_index;

            for (input_index = 0; input_index < phi->input_count; ++input_index) {
                size_t version_id = phi->inputs[input_index].version_id;

                if (!memory_ssa_write_use_site(version_id,
                        MEMORY_SSA_VERSION_USE_PHI,
                        MEMORY_SSA_VERSION_USE_ROLE_PHI_INPUT,
                        block_index,
                        phi_index,
                        (size_t)-1,
                        (size_t)-1,
                        phi->inputs[input_index].predecessor_block_id,
                        cursor,
                        analysis->use_offsets,
                        analysis->use_sites)) {
                    free(cursor);
                    memory_ssa_def_use_analysis_free(analysis);
                    memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-092: failed to write phi use site");
                    return 0;
                }
            }
        }

        for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
            const MemorySsaInstruction *instruction = &block->instructions[instruction_index];
            size_t access_index;

            for (access_index = 0; access_index < instruction->access_count; ++access_index) {
                const MemorySsaAccess *access = &instruction->accesses[access_index];
                MemorySsaVersionUseRole role;

                if (!access->has_use_version) {
                    continue;
                }
                if (!memory_ssa_def_use_instruction_role(instruction, &role, error) ||
                    !memory_ssa_write_use_site(access->use_version_id,
                        MEMORY_SSA_VERSION_USE_INSTR,
                        role,
                        block_index,
                        (size_t)-1,
                        instruction_index,
                        access_index,
                        (size_t)-1,
                        cursor,
                        analysis->use_offsets,
                        analysis->use_sites)) {
                    free(cursor);
                    memory_ssa_def_use_analysis_free(analysis);
                    if (error && error->message[0] == '\0') {
                        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-092: failed to write instruction use site");
                    }
                    return 0;
                }
            }
        }
    }

    free(cursor);
    return 1;
}

int memory_ssa_find_version_use_sites(const MemorySsaDefUseAnalysis *analysis,
    size_t version_id,
    const MemorySsaVersionUseSite **out_sites,
    size_t *out_count) {
    if (!analysis || !out_sites || !out_count || version_id >= analysis->version_count) {
        return 0;
    }

    *out_sites = analysis->use_sites ? &analysis->use_sites[analysis->use_offsets[version_id]] : NULL;
    *out_count = analysis->use_counts ? analysis->use_counts[version_id] : 0;
    return 1;
}

static int memory_ssa_resolve_version_value_recursive(const MemorySsaFunction *function,
    const MemorySsaDefUseAnalysis *analysis,
    size_t version_id,
    unsigned char *visit_state,
    int *resolved_has_value,
    ValueSsaValueRef *resolved_values,
    int *out_has_value,
    ValueSsaValueRef *out_value,
    MemorySsaError *error) {
    size_t def_block_id;
    size_t def_instruction_index;
    size_t def_phi_index;

    if (!function || !analysis || !visit_state || !resolved_has_value || !resolved_values || !out_has_value ||
        !out_value || version_id >= analysis->version_count || !analysis->has_def || !analysis->has_def[version_id]) {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-096: invalid memory-version resolution contract");
        return 0;
    }

    if (visit_state[version_id] == 2) {
        *out_has_value = resolved_has_value[version_id];
        *out_value = resolved_values[version_id];
        return 1;
    }
    if (visit_state[version_id] == 1) {
        *out_has_value = 0;
        *out_value = value_ssa_value_immediate(0);
        return 1;
    }

    visit_state[version_id] = 1;
    *out_has_value = 0;
    *out_value = value_ssa_value_immediate(0);

    switch (analysis->def_kinds[version_id]) {
    case MEMORY_SSA_VERSION_DEF_INSTR: {
        const MemorySsaInstruction *def_instruction;

        def_block_id = analysis->def_block_ids[version_id];
        def_instruction_index = analysis->def_instruction_indices[version_id];
        if (def_block_id >= function->block_count || def_instruction_index >= function->blocks[def_block_id].instruction_count) {
            memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-096: mem.%zu points at an invalid defining instruction", version_id);
            return 0;
        }

        def_instruction = &function->blocks[def_block_id].instructions[def_instruction_index];
        if (def_instruction->instruction.kind == VALUE_SSA_INSTR_STORE_LOCAL ||
            def_instruction->instruction.kind == VALUE_SSA_INSTR_STORE_GLOBAL) {
            *out_has_value = 1;
            *out_value = def_instruction->instruction.as.store.value;
        }
        break;
    }
    case MEMORY_SSA_VERSION_DEF_PHI: {
        const MemorySsaPhi *phi;
        size_t input_index;
        int has_candidate = 0;
        int has_non_cycle_input = 0;
        ValueSsaValueRef candidate = value_ssa_value_immediate(0);

        def_block_id = analysis->def_block_ids[version_id];
        def_phi_index = analysis->def_phi_indices[version_id];
        if (def_block_id >= function->block_count || def_phi_index >= function->blocks[def_block_id].phi_count) {
            memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-096: mem.%zu points at an invalid defining phi", version_id);
            return 0;
        }

        phi = &function->blocks[def_block_id].phis[def_phi_index];
        for (input_index = 0; input_index < phi->input_count; ++input_index) {
            int input_has_value = 0;
            ValueSsaValueRef input_value;
            size_t input_version_id = phi->inputs[input_index].version_id;

            if (!memory_ssa_resolve_version_value_recursive(function,
                    analysis,
                    input_version_id,
                    visit_state,
                    resolved_has_value,
                    resolved_values,
                    &input_has_value,
                    &input_value,
                    error)) {
                return 0;
            }
            if (!input_has_value) {
                if (visit_state[input_version_id] == 1) {
                    continue;
                }
                has_candidate = 0;
                *out_has_value = 0;
                *out_value = value_ssa_value_immediate(0);
                goto finish;
            }
            has_non_cycle_input = 1;
            if (!has_candidate) {
                candidate = input_value;
                has_candidate = 1;
                continue;
            }
            if (!memory_ssa_value_refs_equal(candidate, input_value)) {
                has_candidate = 0;
                *out_has_value = 0;
                *out_value = value_ssa_value_immediate(0);
                goto finish;
            }
        }

        if (has_candidate && has_non_cycle_input) {
            *out_has_value = 1;
            *out_value = candidate;
        }
        break;
    }
    case MEMORY_SSA_VERSION_DEF_ENTRY:
    default:
        break;
    }

finish:
    visit_state[version_id] = 2;
    resolved_has_value[version_id] = *out_has_value;
    resolved_values[version_id] = *out_value;
    return 1;
}

/*
 * Iterative wrapper around memory_ssa_resolve_version_value_recursive.
 *
 * The recursive resolver handles direct self-cycles (phiA's input traces back
 * to phiA) by skipping cycle inputs.  However, indirect cycles (phiA -> phiB
 * -> phiA) can still fail: phiB resolves first, fails because its only non-
 * cycle input is phiA (still in-progress), gets marked visit_state=2 with
 * has_value=0.  When phiA later sees phiB's result, it treats it as a genuine
 * failure rather than a cycle artifact.
 *
 * Fix: after the first pass, reset any PHI-defined versions that resolved to
 * has_value=0 back to unvisited, then re-run.  On the second pass, versions
 * that did resolve on pass 1 are cached (visit_state=2), so the cycle members
 * can now see their sibling phis' resolved values.  At most 2 iterations
 * suffice because the resolver is monotonic: each pass can only add new
 * resolved values, never revoke them.
 */
static int memory_ssa_resolve_version_value_iterative(const MemorySsaFunction *function,
    const MemorySsaDefUseAnalysis *analysis,
    size_t version_id,
    int *out_has_value,
    ValueSsaValueRef *out_value,
    MemorySsaError *error) {
    unsigned char *visit_state = NULL;
    int *resolved_has_value = NULL;
    ValueSsaValueRef *resolved_values = NULL;
    int ok;
    int pass;

    if (!analysis || analysis->version_count == 0) {
        *out_has_value = 0;
        *out_value = value_ssa_value_immediate(0);
        return 1;
    }

    visit_state = (unsigned char *)calloc(analysis->version_count, sizeof(unsigned char));
    resolved_has_value = (int *)calloc(analysis->version_count, sizeof(int));
    resolved_values = (ValueSsaValueRef *)calloc(analysis->version_count, sizeof(ValueSsaValueRef));
    if (!visit_state || !resolved_has_value || !resolved_values) {
        free(visit_state);
        free(resolved_has_value);
        free(resolved_values);
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-086: out of memory while resolving memory version value");
        return 0;
    }

    for (pass = 0; pass < 2; ++pass) {
        ok = memory_ssa_resolve_version_value_recursive(function,
            analysis,
            version_id,
            visit_state,
            resolved_has_value,
            resolved_values,
            out_has_value,
            out_value,
            error);
        if (!ok) {
            break;
        }
        if (*out_has_value || pass == 1) {
            break;
        }
        {
            size_t v;

            for (v = 0; v < analysis->version_count; ++v) {
                if (visit_state[v] == 2 && !resolved_has_value[v] &&
                    analysis->def_kinds[v] == MEMORY_SSA_VERSION_DEF_PHI) {
                    visit_state[v] = 0;
                }
            }
        }
    }

    free(visit_state);
    free(resolved_has_value);
    free(resolved_values);
    return ok;
}

int memory_ssa_resolve_version_value(const MemorySsaFunction *function,
    const MemorySsaDefUseAnalysis *analysis,
    size_t version_id,
    int *out_has_value,
    ValueSsaValueRef *out_value,
    MemorySsaError *error) {
    if (!function || !analysis || !out_has_value || !out_value) {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-105: invalid version-value resolution contract");
        return 0;
    }

    *out_has_value = 0;
    *out_value = value_ssa_value_immediate(0);
    if (version_id >= analysis->version_count || !analysis->has_def || !analysis->has_def[version_id]) {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-106: mem.%zu is not a defined version", version_id);
        return 0;
    }

    return memory_ssa_resolve_version_value_iterative(function, analysis, version_id, out_has_value, out_value, error);
}

static int memory_ssa_version_is_observed_recursive(const MemorySsaFunction *function,
    const MemorySsaDefUseAnalysis *analysis,
    size_t version_id,
    unsigned char *visit_state,
    int *memo_observed,
    int *out_observed,
    MemorySsaError *error) {
    const MemorySsaVersionUseSite *use_sites = NULL;
    size_t use_count = 0;
    size_t use_index;

    if (!function || !analysis || !visit_state || !memo_observed || !out_observed ||
        version_id >= analysis->version_count || !analysis->has_def || !analysis->has_def[version_id]) {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-100: invalid memory-version observation contract");
        return 0;
    }

    if (visit_state[version_id] == 2) {
        *out_observed = memo_observed[version_id];
        return 1;
    }
    if (visit_state[version_id] == 1) {
        *out_observed = 0;
        return 1;
    }
    if (!memory_ssa_find_version_use_sites(analysis, version_id, &use_sites, &use_count)) {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-100: failed to locate mem.%zu use sites", version_id);
        return 0;
    }

    visit_state[version_id] = 1;
    *out_observed = 0;
    for (use_index = 0; use_index < use_count; ++use_index) {
        const MemorySsaVersionUseSite *use_site = &use_sites[use_index];

        switch (use_site->role) {
        case MEMORY_SSA_VERSION_USE_ROLE_LOAD:
        case MEMORY_SSA_VERSION_USE_ROLE_CALL:
            *out_observed = 1;
            goto finish;
        case MEMORY_SSA_VERSION_USE_ROLE_STORE_PREVIOUS:
            break;
        case MEMORY_SSA_VERSION_USE_ROLE_PHI_INPUT:
            if (use_site->kind == MEMORY_SSA_VERSION_USE_PHI &&
                use_site->block_id < function->block_count &&
                use_site->phi_index < function->blocks[use_site->block_id].phi_count) {
                const MemorySsaPhi *phi = &function->blocks[use_site->block_id].phis[use_site->phi_index];
                int phi_observed = 0;

                if (!memory_ssa_version_is_observed_recursive(function,
                        analysis,
                        phi->result_version_id,
                        visit_state,
                        memo_observed,
                        &phi_observed,
                        error)) {
                    return 0;
                }
                if (phi_observed) {
                    *out_observed = 1;
                    goto finish;
                }
                break;
            }
            *out_observed = 1;
            goto finish;
        default:
            *out_observed = 1;
            goto finish;
        }
    }

finish:
    visit_state[version_id] = 2;
    memo_observed[version_id] = *out_observed;
    return 1;
}

int memory_ssa_resolve_load_store_value(const MemorySsaFunction *function,
    const MemorySsaDefUseAnalysis *analysis,
    size_t block_id,
    size_t instruction_index,
    int *out_has_value,
    ValueSsaValueRef *out_value,
    MemorySsaError *error) {
    const MemorySsaInstruction *instruction;
    size_t version_id;

    if (!function || !analysis || !out_has_value || !out_value) {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-093: invalid load-resolution contract");
        return 0;
    }

    *out_has_value = 0;
    *out_value = value_ssa_value_immediate(0);
    if (block_id >= function->block_count || instruction_index >= function->blocks[block_id].instruction_count) {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-093: invalid load-resolution site");
        return 0;
    }

    instruction = &function->blocks[block_id].instructions[instruction_index];
    if (instruction->instruction.kind != VALUE_SSA_INSTR_LOAD_LOCAL &&
        instruction->instruction.kind != VALUE_SSA_INSTR_LOAD_GLOBAL) {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-094: bb.%zu instr %zu is not a load", block_id, instruction_index);
        return 0;
    }
    if (instruction->access_count == 0 || !instruction->accesses[0].has_use_version) {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-095: bb.%zu instr %zu is missing a memory use", block_id, instruction_index);
        return 0;
    }

    version_id = instruction->accesses[0].use_version_id;
    if (version_id >= analysis->version_count || !analysis->has_def || !analysis->has_def[version_id]) {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-096: bb.%zu instr %zu reads unknown mem.%zu", block_id, instruction_index, version_id);
        return 0;
    }

    return memory_ssa_resolve_version_value_iterative(function, analysis, version_id, out_has_value, out_value, error);
}

int memory_ssa_resolve_store_previous_value(const MemorySsaFunction *function,
    const MemorySsaDefUseAnalysis *analysis,
    size_t block_id,
    size_t instruction_index,
    int *out_has_value,
    ValueSsaValueRef *out_value,
    MemorySsaError *error) {
    const MemorySsaInstruction *instruction;
    size_t version_id;

    if (!function || !analysis || !out_has_value || !out_value) {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-101: invalid store-previous-value contract");
        return 0;
    }

    *out_has_value = 0;
    *out_value = value_ssa_value_immediate(0);
    if (block_id >= function->block_count || instruction_index >= function->blocks[block_id].instruction_count) {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-101: invalid store-previous-value site");
        return 0;
    }

    instruction = &function->blocks[block_id].instructions[instruction_index];
    if (instruction->instruction.kind != VALUE_SSA_INSTR_STORE_LOCAL &&
        instruction->instruction.kind != VALUE_SSA_INSTR_STORE_GLOBAL) {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-102: bb.%zu instr %zu is not a store", block_id, instruction_index);
        return 0;
    }
    if (instruction->access_count == 0 || !instruction->accesses[0].has_def_version) {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-103: bb.%zu instr %zu is missing a memory def", block_id, instruction_index);
        return 0;
    }
    if (!instruction->accesses[0].has_use_version) {
        return 1;
    }

    version_id = instruction->accesses[0].use_version_id;
    if (version_id >= analysis->version_count || !analysis->has_def || !analysis->has_def[version_id]) {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-104: bb.%zu instr %zu reads unknown prior mem.%zu", block_id, instruction_index, version_id);
        return 0;
    }

    return memory_ssa_resolve_version_value_iterative(function, analysis, version_id, out_has_value, out_value, error);
}

int memory_ssa_is_trivially_dead_store(const MemorySsaFunction *function,
    const MemorySsaDefUseAnalysis *analysis,
    size_t block_id,
    size_t instruction_index,
    int *out_is_dead,
    MemorySsaError *error) {
    const MemorySsaInstruction *instruction;
    unsigned char *visit_state = NULL;
    int *memo_observed = NULL;
    int observed = 0;
    size_t version_id;

    if (!function || !analysis || !out_is_dead) {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-097: invalid dead-store query contract");
        return 0;
    }

    *out_is_dead = 0;
    if (block_id >= function->block_count || instruction_index >= function->blocks[block_id].instruction_count) {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-097: invalid dead-store query site");
        return 0;
    }

    instruction = &function->blocks[block_id].instructions[instruction_index];
    if (instruction->instruction.kind != VALUE_SSA_INSTR_STORE_LOCAL &&
        instruction->instruction.kind != VALUE_SSA_INSTR_STORE_GLOBAL) {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-098: bb.%zu instr %zu is not a store", block_id, instruction_index);
        return 0;
    }
    if (instruction->access_count == 0 || !instruction->accesses[0].has_def_version) {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-099: bb.%zu instr %zu is missing a memory def", block_id, instruction_index);
        return 0;
    }
    version_id = instruction->accesses[0].def_version_id;
    if (version_id >= analysis->version_count || !analysis->use_counts) {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-100: bb.%zu instr %zu references invalid mem.%zu", block_id, instruction_index, version_id);
        return 0;
    }

    visit_state = (unsigned char *)calloc(analysis->version_count, sizeof(unsigned char));
    memo_observed = (int *)calloc(analysis->version_count, sizeof(int));
    if (!visit_state || !memo_observed) {
        free(visit_state);
        free(memo_observed);
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-086: out of memory while checking store deadness");
        return 0;
    }

    if (!memory_ssa_version_is_observed_recursive(
            function, analysis, version_id, visit_state, memo_observed, &observed, error)) {
        free(visit_state);
        free(memo_observed);
        return 0;
    }

    free(visit_state);
    free(memo_observed);
    *out_is_dead = !observed;
    return 1;
}

static int memory_ssa_slot_refs_equal(ValueSsaSlotRef lhs, ValueSsaSlotRef rhs) {
    return lhs.kind == rhs.kind && lhs.id == rhs.id;
}

static int memory_ssa_value_matches_slot_version_recursive(const ValueSsaFunction *value_function,
    const MemorySsaFunction *memory_function,
    const MemorySsaDefUseAnalysis *memory_def_use,
    const ValueSsaDefUseAnalysis *value_def_use,
    ValueSsaValueRef value,
    ValueSsaSlotRef expected_slot,
    size_t expected_version_id,
    unsigned char *visit_state,
    int *out_matches,
    MemorySsaError *error) {
    size_t def_block_id;
    size_t def_phi_index;
    size_t def_instruction_index;
    const ValueSsaInstruction *def_instruction;
    const MemorySsaInstruction *memory_instruction;

    if (!value_function || !memory_function || !memory_def_use || !value_def_use || !visit_state || !out_matches) {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-107: invalid slot-version value match contract");
        return 0;
    }

    *out_matches = 0;
    if (value.kind != VALUE_SSA_VALUE_ID || value.value_id >= value_def_use->value_count ||
        !value_def_use->has_def || !value_def_use->has_def[value.value_id]) {
        return 1;
    }
    if (visit_state[value.value_id] == 1) {
        *out_matches = 1;
        return 1;
    }

    visit_state[value.value_id] = 1;
    def_block_id = value_def_use->def_block_ids[value.value_id];
    def_phi_index = value_def_use->def_phi_indices[value.value_id];
    def_instruction_index = value_def_use->def_instruction_indices[value.value_id];

    if (def_phi_index != (size_t)-1) {
        const ValueSsaPhi *phi;
        size_t input_index;
        const MemorySsaPhi *expected_phi = NULL;
        size_t expected_phi_block_id = 0;
        size_t expected_phi_index = 0;
        int expected_is_phi = 0;

        if (def_block_id >= value_function->block_count || def_phi_index >= value_function->blocks[def_block_id].phi_count) {
            visit_state[value.value_id] = 2;
            return 1;
        }
        if (expected_version_id < memory_def_use->version_count &&
            memory_def_use->has_def &&
            memory_def_use->has_def[expected_version_id] &&
            memory_def_use->def_kinds &&
            memory_def_use->def_kinds[expected_version_id] == MEMORY_SSA_VERSION_DEF_PHI) {
            expected_phi_block_id = memory_def_use->def_block_ids[expected_version_id];
            expected_phi_index = memory_def_use->def_phi_indices[expected_version_id];
            if (expected_phi_block_id < memory_function->block_count &&
                expected_phi_index < memory_function->blocks[expected_phi_block_id].phi_count) {
                expected_phi = &memory_function->blocks[expected_phi_block_id].phis[expected_phi_index];
                expected_is_phi = 1;
            }
        }

        phi = &value_function->blocks[def_block_id].phis[def_phi_index];
        for (input_index = 0; input_index < phi->input_count; ++input_index) {
            int input_matches = 0;
            size_t input_expected_version_id = expected_version_id;

            if (expected_is_phi) {
                size_t expected_input_index;
                int found_expected_input = 0;

                for (expected_input_index = 0; expected_input_index < expected_phi->input_count; ++expected_input_index) {
                    if (expected_phi->inputs[expected_input_index].predecessor_block_id ==
                        phi->inputs[input_index].predecessor_block_id) {
                        input_expected_version_id = expected_phi->inputs[expected_input_index].version_id;
                        found_expected_input = 1;
                        break;
                    }
                }
                if (!found_expected_input) {
                    visit_state[value.value_id] = 2;
                    return 1;
                }
            }

            if (!memory_ssa_value_matches_slot_version_recursive(value_function,
                    memory_function,
                    memory_def_use,
                    value_def_use,
                    phi->inputs[input_index].value,
                    expected_slot,
                    input_expected_version_id,
                    visit_state,
                    &input_matches,
                    error)) {
                visit_state[value.value_id] = 2;
                return 0;
            }
            if (!input_matches) {
                visit_state[value.value_id] = 2;
                return 1;
            }
        }

        *out_matches = 1;
        visit_state[value.value_id] = 2;
        return 1;
    }

    if (def_instruction_index == (size_t)-1 || def_block_id >= value_function->block_count ||
        def_instruction_index >= value_function->blocks[def_block_id].instruction_count ||
        def_block_id >= memory_function->block_count ||
        def_instruction_index >= memory_function->blocks[def_block_id].instruction_count) {
        visit_state[value.value_id] = 2;
        return 1;
    }

    def_instruction = &value_function->blocks[def_block_id].instructions[def_instruction_index];
    memory_instruction = &memory_function->blocks[def_block_id].instructions[def_instruction_index];
    switch (def_instruction->kind) {
    case VALUE_SSA_INSTR_MOV:
        if (!memory_ssa_value_matches_slot_version_recursive(value_function,
                memory_function,
                memory_def_use,
                value_def_use,
                def_instruction->as.mov_value,
                expected_slot,
                expected_version_id,
                visit_state,
                out_matches,
                error)) {
            visit_state[value.value_id] = 2;
            return 0;
        }
        break;
    case VALUE_SSA_INSTR_LOAD_LOCAL:
    case VALUE_SSA_INSTR_LOAD_GLOBAL:
        if (memory_instruction->access_count > 0 && memory_instruction->accesses[0].has_use_version &&
            memory_instruction->accesses[0].use_version_id == expected_version_id &&
            memory_ssa_slot_refs_equal(def_instruction->as.load_slot, expected_slot)) {
            *out_matches = 1;
        }
        break;
    default:
        break;
    }

    visit_state[value.value_id] = 2;
    return 1;
}

int memory_ssa_value_matches_slot_version(const ValueSsaFunction *value_function,
    const MemorySsaFunction *memory_function,
    const MemorySsaDefUseAnalysis *memory_def_use,
    const ValueSsaDefUseAnalysis *value_def_use,
    ValueSsaValueRef value,
    ValueSsaSlotRef expected_slot,
    size_t expected_version_id,
    int *out_matches,
    MemorySsaError *error) {
    unsigned char *visit_state;

    if (!value_function || !value_def_use || !out_matches) {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-107: invalid slot-version value match contract");
        return 0;
    }

    *out_matches = 0;
    visit_state = (unsigned char *)calloc(value_def_use->value_count ? value_def_use->value_count : 1, sizeof(unsigned char));
    if (!visit_state) {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-108: out of memory while matching slot-version value");
        return 0;
    }

    if (!memory_ssa_value_matches_slot_version_recursive(value_function,
            memory_function,
            memory_def_use,
            value_def_use,
            value,
            expected_slot,
            expected_version_id,
            visit_state,
            out_matches,
            error)) {
        free(visit_state);
        return 0;
    }

    free(visit_state);
    return 1;
}

int memory_ssa_find_block_equivalent_value(const ValueSsaFunction *value_function,
    const MemorySsaFunction *memory_function,
    const MemorySsaDefUseAnalysis *memory_def_use,
    const ValueSsaDefUseAnalysis *value_def_use,
    size_t block_id,
    ValueSsaSlotRef expected_slot,
    size_t expected_version_id,
    int *out_has_value,
    ValueSsaValueRef *out_value,
    MemorySsaError *error) {
    unsigned char *visit_state = NULL;
    const ValueSsaBasicBlock *block;
    size_t instruction_index;
    size_t alloc_size;

    if (!value_function || !memory_function || !memory_def_use || !value_def_use || !out_has_value || !out_value ||
        block_id >= value_function->block_count || block_id >= memory_function->block_count) {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-109: invalid block-equivalent value lookup contract");
        return 0;
    }

    *out_has_value = 0;
    *out_value = value_ssa_value_immediate(0);
    alloc_size = value_def_use->value_count ? value_def_use->value_count : 1;
    visit_state = (unsigned char *)calloc(alloc_size, sizeof(unsigned char));
    if (!visit_state) {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-110: out of memory while searching block equivalent value");
        return 0;
    }

    block = &value_function->blocks[block_id];
    for (instruction_index = block->instruction_count; instruction_index > 0; --instruction_index) {
        const ValueSsaInstruction *instruction = &block->instructions[instruction_index - 1];
        int matches = 0;

        if (!instruction->has_result) {
            continue;
        }
        memset(visit_state, 0, alloc_size * sizeof(unsigned char));
        if (!memory_ssa_value_matches_slot_version_recursive(value_function,
                memory_function,
                memory_def_use,
                value_def_use,
                instruction->result,
                expected_slot,
                expected_version_id,
                visit_state,
                &matches,
                error)) {
            free(visit_state);
            return 0;
        }
        if (matches) {
            *out_has_value = 1;
            *out_value = instruction->result;
            free(visit_state);
            return 1;
        }
    }

    for (instruction_index = block->phi_count; instruction_index > 0; --instruction_index) {
        ValueSsaValueRef phi_value = value_ssa_value_id(block->phis[instruction_index - 1].result_id);
        int matches = 0;

        memset(visit_state, 0, alloc_size * sizeof(unsigned char));
        if (!memory_ssa_value_matches_slot_version_recursive(value_function,
                memory_function,
                memory_def_use,
                value_def_use,
                phi_value,
                expected_slot,
                expected_version_id,
                visit_state,
                &matches,
                error)) {
            free(visit_state);
            return 0;
        }
        if (matches) {
            *out_has_value = 1;
            *out_value = phi_value;
            free(visit_state);
            return 1;
        }
    }

    free(visit_state);
    return 1;
}

#endif
