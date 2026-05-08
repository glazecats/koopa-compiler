#if defined(__INTELLISENSE__) && !defined(MACHINE_SELECT_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_SELECT_SPLIT_AGGREGATOR)
#define MACHINE_SELECT_SPLIT_INTELLISENSE_CONTEXT
#include "machine_select.c"
#else

static double machine_select_cleanup_now_s(void) {
    return machine_select_now_s();
}

static int machine_select_cleanup_trace_enabled(void) {
    return machine_select_trace_enabled();
}

static void machine_select_cleanup_trace_timing(const char *stage, double elapsed_s) {
    if (!stage || !machine_select_cleanup_trace_enabled()) {
        return;
    }
    fprintf(stderr, "[machine-select-cleanup] %s %.3f\n", stage, elapsed_s);
}

static int machine_select_operand_get_resource_index(const MachineSelectProgram *program,
    const MachineSelectFunction *function,
    MachineSelectOperand operand,
    size_t *out_index);

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

static int machine_select_operands_equal(MachineSelectOperand lhs, MachineSelectOperand rhs) {
    if (lhs.kind != rhs.kind) {
        return 0;
    }
    switch (lhs.kind) {
        case MACHINE_SELECT_OPERAND_NONE:
            return 1;
        case MACHINE_SELECT_OPERAND_IMMEDIATE:
            return lhs.immediate == rhs.immediate;
        case MACHINE_SELECT_OPERAND_REGISTER:
            return lhs.machine_register_id == rhs.machine_register_id;
        case MACHINE_SELECT_OPERAND_SPILL_SLOT:
            return lhs.spill_slot == rhs.spill_slot;
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

static int machine_select_op_is_call(const MachineSelectOp *op) {
    if (!op) {
        return 0;
    }
    switch (op->kind) {
        case MACHINE_SELECT_OP_CALL:
        case MACHINE_SELECT_OP_CALL_IMM:
        case MACHINE_SELECT_OP_CALL_SPILL:
        case MACHINE_SELECT_OP_CALL_IMM_SPILL:
        case MACHINE_SELECT_OP_CALL_VOID:
        case MACHINE_SELECT_OP_CALL_VOID_IMM:
            return 1;
        default:
            return 0;
    }
}

static int machine_select_op_clobbers_operand_via_call(const MachineSelectProgram *program,
    const MachineSelectFunction *function,
    const MachineSelectOp *op,
    MachineSelectOperand operand) {
    size_t resource_index;

    if (!program || !function || !op || operand.kind != MACHINE_SELECT_OPERAND_REGISTER ||
        !machine_select_op_is_call(op) ||
        !machine_select_operand_get_resource_index(program, function, operand, &resource_index) ||
        resource_index >= program->register_bank.register_count) {
        return 0;
    }

    return program->register_bank.registers[resource_index].caller_clobbered != 0;
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
        case MACHINE_SELECT_OP_LOAD_INDIRECT:
            if (machine_select_operand_matches_result(op->as.load_indirect_addr, result)) {
                count += 1;
            }
            break;
        case MACHINE_SELECT_OP_STORE_INDIRECT:
            if (machine_select_operand_matches_result(op->as.store_indirect.addr, result)) {
                count += 1;
            }
            if (machine_select_operand_matches_result(op->as.store_indirect.value, result)) {
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
static int machine_select_cleanup_terminator_shape(MachineSelectBasicBlock *block);
static int machine_select_cleanup_op_is_preserved_edge_copy(const MachineSelectFunction *function,
    const MachineSelectBasicBlock *block,
    size_t op_index,
    size_t **predecessor_counts_ptr);
static int machine_select_cleanup_forward_trivial_defs_in_block(const MachineSelectProgram *program,
    MachineSelectFunction *function,
    size_t block_index,
    size_t resource_count,
    const unsigned char *live_out_sets);
static int machine_select_cleanup_remove_dead_defs_in_block(MachineSelectFunction *function,
    const MachineSelectProgram *program,
    size_t block_index,
    size_t resource_count,
    const unsigned char *live_out_sets);
static int machine_select_cleanup_remove_redundant_addr_defs_in_block(const MachineSelectProgram *program,
    const MachineSelectFunction *function,
    MachineSelectBasicBlock *block);
static int machine_select_cleanup_remove_copy_self_in_block(MachineSelectBasicBlock *block);
static int machine_select_cleanup_fold_addr_copy_pairs_in_block(const MachineSelectProgram *program,
    const MachineSelectFunction *function,
    MachineSelectBasicBlock *block);
static int machine_select_cleanup_reuse_internal_pure_calls_program(MachineSelectProgram *program,
    MachineSelectError *error);
static int machine_select_cleanup_forward_same_block_indirect_loads_program(MachineSelectProgram *program);
static int machine_select_cleanup_reuse_spill_pure_expr_program(MachineSelectProgram *program);
static int machine_select_op_is_small_reusable_pure_expr(const MachineSelectOp *op);
static int machine_select_find_defining_op_in_block(const MachineSelectBasicBlock *block,
    size_t use_index,
    MachineSelectOperand operand,
    size_t *out_def_index);
static int machine_select_ops_compute_same_small_reusable_pure_expr_in_block(
    const MachineSelectBasicBlock *lhs_block,
    size_t lhs_index,
    const MachineSelectBasicBlock *rhs_block,
    size_t rhs_index,
    unsigned depth);
static void machine_select_remove_op_at(MachineSelectBasicBlock *block, size_t op_index);

typedef enum {
    MACHINE_SELECT_ADDR_ROOT_UNKNOWN = 0,
    MACHINE_SELECT_ADDR_ROOT_LOCAL_SLOT,
    MACHINE_SELECT_ADDR_ROOT_GLOBAL_SLOT,
    MACHINE_SELECT_ADDR_ROOT_PARAMETER_POINTER,
} MachineSelectAddressRootKind;

typedef struct {
    MachineSelectAddressRootKind kind;
    size_t id;
} MachineSelectAddressRoot;

static int machine_select_defs_compute_same_address_root(const MachineSelectBasicBlock *lhs_block,
    size_t lhs_def_index,
    const MachineSelectBasicBlock *rhs_block,
    size_t rhs_def_index) {
    const MachineSelectOp *lhs;
    const MachineSelectOp *rhs;

    if (!lhs_block || !rhs_block ||
        lhs_def_index >= lhs_block->op_count ||
        rhs_def_index >= rhs_block->op_count) {
        return 0;
    }

    lhs = &lhs_block->ops[lhs_def_index];
    rhs = &rhs_block->ops[rhs_def_index];
    if (!lhs->has_result || !rhs->has_result ||
        lhs->kind != rhs->kind) {
        return 0;
    }

    switch (lhs->kind) {
        case MACHINE_SELECT_OP_ADDR_LOCAL:
        case MACHINE_SELECT_OP_ADDR_GLOBAL:
            return lhs->as.addr_slot.kind == rhs->as.addr_slot.kind &&
                lhs->as.addr_slot.id == rhs->as.addr_slot.id;
        default:
            return 0;
    }
}

static int machine_select_cleanup_extract_address_root_operand(const MachineSelectFunction *function,
    const MachineSelectBasicBlock *block,
    size_t use_index,
    MachineSelectOperand operand,
    MachineSelectAddressRoot *out_root,
    unsigned depth);

static int machine_select_cleanup_extract_address_root_from_def(const MachineSelectFunction *function,
    const MachineSelectBasicBlock *block,
    size_t def_index,
    MachineSelectAddressRoot *out_root,
    unsigned depth) {
    const MachineSelectOp *op;

    if (!function || !block || !out_root || def_index >= block->op_count || depth > 8u) {
        return 0;
    }

    out_root->kind = MACHINE_SELECT_ADDR_ROOT_UNKNOWN;
    out_root->id = 0u;
    op = &block->ops[def_index];

    switch (op->kind) {
        case MACHINE_SELECT_OP_ADDR_LOCAL:
            out_root->kind = MACHINE_SELECT_ADDR_ROOT_LOCAL_SLOT;
            out_root->id = op->as.addr_slot.id;
            return 1;
        case MACHINE_SELECT_OP_ADDR_GLOBAL:
            out_root->kind = MACHINE_SELECT_ADDR_ROOT_GLOBAL_SLOT;
            out_root->id = op->as.addr_slot.id;
            return 1;
        case MACHINE_SELECT_OP_LOAD_LOCAL:
            if (op->as.load_slot.id < function->local_count &&
                function->locals[op->as.load_slot.id].is_parameter) {
                out_root->kind = MACHINE_SELECT_ADDR_ROOT_PARAMETER_POINTER;
                out_root->id = op->as.load_slot.id;
                return 1;
            }
            return 0;
        case MACHINE_SELECT_OP_COPY:
        case MACHINE_SELECT_OP_MATERIALIZE_IMM:
            return machine_select_cleanup_extract_address_root_operand(
                function, block, def_index, op->as.copy_value, out_root, depth + 1u);
        case MACHINE_SELECT_OP_ALU:
        case MACHINE_SELECT_OP_ALU_IMM: {
            MachineSelectAddressRoot lhs_root;
            MachineSelectAddressRoot rhs_root;
            int has_lhs_root;
            int has_rhs_root;

            if (op->as.binary.op != MACHINE_IR_BINARY_ADD) {
                return 0;
            }

            has_lhs_root = machine_select_cleanup_extract_address_root_operand(
                function, block, def_index, op->as.binary.lhs, &lhs_root, depth + 1u);
            has_rhs_root = machine_select_cleanup_extract_address_root_operand(
                function, block, def_index, op->as.binary.rhs, &rhs_root, depth + 1u);
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

static int machine_select_cleanup_extract_address_root_operand(const MachineSelectFunction *function,
    const MachineSelectBasicBlock *block,
    size_t use_index,
    MachineSelectOperand operand,
    MachineSelectAddressRoot *out_root,
    unsigned depth) {
    size_t def_index = 0u;

    if (!out_root) {
        return 0;
    }

    out_root->kind = MACHINE_SELECT_ADDR_ROOT_UNKNOWN;
    out_root->id = 0u;
    if (!function || !block || depth > 8u ||
        (operand.kind != MACHINE_SELECT_OPERAND_REGISTER &&
            operand.kind != MACHINE_SELECT_OPERAND_SPILL_SLOT)) {
        return 0;
    }

    if (!machine_select_find_defining_op_in_block(block, use_index, operand, &def_index)) {
        return 0;
    }
    return machine_select_cleanup_extract_address_root_from_def(function, block, def_index, out_root, depth + 1u);
}

static int machine_select_cleanup_address_roots_proven_non_alias(const MachineSelectAddressRoot *lhs,
    const MachineSelectAddressRoot *rhs) {
    if (!lhs || !rhs ||
        lhs->kind == MACHINE_SELECT_ADDR_ROOT_UNKNOWN ||
        rhs->kind == MACHINE_SELECT_ADDR_ROOT_UNKNOWN) {
        return 0;
    }

    if (lhs->kind == MACHINE_SELECT_ADDR_ROOT_LOCAL_SLOT &&
        rhs->kind == MACHINE_SELECT_ADDR_ROOT_LOCAL_SLOT) {
        return lhs->id != rhs->id;
    }
    if (lhs->kind == MACHINE_SELECT_ADDR_ROOT_GLOBAL_SLOT &&
        rhs->kind == MACHINE_SELECT_ADDR_ROOT_GLOBAL_SLOT) {
        return lhs->id != rhs->id;
    }
    if ((lhs->kind == MACHINE_SELECT_ADDR_ROOT_LOCAL_SLOT && rhs->kind == MACHINE_SELECT_ADDR_ROOT_GLOBAL_SLOT) ||
        (lhs->kind == MACHINE_SELECT_ADDR_ROOT_GLOBAL_SLOT && rhs->kind == MACHINE_SELECT_ADDR_ROOT_LOCAL_SLOT) ||
        (lhs->kind == MACHINE_SELECT_ADDR_ROOT_LOCAL_SLOT && rhs->kind == MACHINE_SELECT_ADDR_ROOT_PARAMETER_POINTER) ||
        (lhs->kind == MACHINE_SELECT_ADDR_ROOT_PARAMETER_POINTER && rhs->kind == MACHINE_SELECT_ADDR_ROOT_LOCAL_SLOT)) {
        return 1;
    }

    return 0;
}

static int machine_select_cleanup_store_cannot_alias_loaded_address(const MachineSelectFunction *function,
    const MachineSelectBasicBlock *block,
    const MachineSelectAddressRoot *load_root,
    size_t op_index) {
    const MachineSelectOp *op;
    MachineSelectAddressRoot store_root;

    if (!function || !block || !load_root || op_index >= block->op_count) {
        return 0;
    }
    op = &block->ops[op_index];

    switch (op->kind) {
        case MACHINE_SELECT_OP_STORE_LOCAL:
        case MACHINE_SELECT_OP_STORE_LOCAL_IMM:
            if (op->as.store.slot.id >= function->local_count) {
                return 0;
            }
            if (load_root->kind == MACHINE_SELECT_ADDR_ROOT_LOCAL_SLOT) {
                return op->as.store.slot.id != load_root->id;
            }
            return load_root->kind != MACHINE_SELECT_ADDR_ROOT_PARAMETER_POINTER;
        case MACHINE_SELECT_OP_STORE_GLOBAL:
        case MACHINE_SELECT_OP_STORE_GLOBAL_IMM:
            if (load_root->kind == MACHINE_SELECT_ADDR_ROOT_GLOBAL_SLOT) {
                return op->as.store.slot.id != load_root->id;
            }
            return load_root->kind == MACHINE_SELECT_ADDR_ROOT_LOCAL_SLOT;
        case MACHINE_SELECT_OP_STORE_INDIRECT:
            if (!machine_select_cleanup_extract_address_root_operand(
                    function, block, op_index, op->as.store_indirect.addr, &store_root, 0u)) {
                return 0;
            }
            return machine_select_cleanup_address_roots_proven_non_alias(load_root, &store_root);
        default:
            return 1;
    }
}

typedef struct {
    size_t block_count;
    unsigned char *predecessor_matrix;
} MachineSelectCleanupCfg;

typedef enum {
    MACHINE_SELECT_CALL_ARG_DESC_INVALID = 0,
    MACHINE_SELECT_CALL_ARG_DESC_IMMEDIATE,
    MACHINE_SELECT_CALL_ARG_DESC_LOCAL_VALUE,
    MACHINE_SELECT_CALL_ARG_DESC_GLOBAL_VALUE,
    MACHINE_SELECT_CALL_ARG_DESC_LOCAL_ADDRESS,
    MACHINE_SELECT_CALL_ARG_DESC_GLOBAL_ADDRESS,
    MACHINE_SELECT_CALL_ARG_DESC_LIVE_IN_OPERAND,
} MachineSelectCallArgDescKind;

typedef struct {
    MachineSelectCallArgDescKind kind;
    long long immediate;
    size_t slot_id;
    MachineSelectOperand operand;
} MachineSelectCallArgDesc;

typedef struct {
    size_t function_count;
    size_t global_count;
    unsigned char *global_reads;
    unsigned char *global_writes;
    unsigned char *cacheable_pure;
} MachineSelectCallEffectSummary;

static void machine_select_call_effect_summary_init(MachineSelectCallEffectSummary *summary) {
    if (!summary) {
        return;
    }
    memset(summary, 0, sizeof(*summary));
}

static void machine_select_call_effect_summary_free(MachineSelectCallEffectSummary *summary) {
    if (!summary) {
        return;
    }
    free(summary->global_reads);
    free(summary->global_writes);
    free(summary->cacheable_pure);
    machine_select_call_effect_summary_init(summary);
}

static size_t machine_select_find_function_index_by_name(const MachineSelectProgram *program,
    const char *function_name) {
    size_t function_index;

    if (!program || !function_name) {
        return (size_t)-1;
    }
    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const MachineSelectFunction *function = &program->functions[function_index];

        if (!function->name) {
            continue;
        }
        if (strcmp(function->name, function_name) == 0) {
            return function_index;
        }
    }
    return (size_t)-1;
}

static int machine_select_call_effect_summary_compute(const MachineSelectProgram *program,
    MachineSelectCallEffectSummary *out_summary,
    MachineSelectError *error) {
    MachineSelectCallEffectSummary summary;
    size_t function_index;
    int changed = 1;

    if (out_summary) {
        machine_select_call_effect_summary_init(out_summary);
    }
    if (!program || !out_summary) {
        machine_select_set_error(error, 0, 0, "MACHINE-SELECT-424: invalid call-effect summary contract");
        return 0;
    }

    machine_select_call_effect_summary_init(&summary);
    summary.function_count = program->function_count;
    summary.global_count = program->global_count;

    if (summary.function_count == 0u) {
        *out_summary = summary;
        return 1;
    }

    if (summary.global_count > 0u) {
        summary.global_reads =
            (unsigned char *)calloc(summary.function_count * summary.global_count, sizeof(unsigned char));
        summary.global_writes =
            (unsigned char *)calloc(summary.function_count * summary.global_count, sizeof(unsigned char));
        if (!summary.global_reads || !summary.global_writes) {
            machine_select_call_effect_summary_free(&summary);
            machine_select_set_error(error, 0, 0, "MACHINE-SELECT-425: out of memory building call-effect summary");
            return 0;
        }
    }
    summary.cacheable_pure = (unsigned char *)calloc(summary.function_count, sizeof(unsigned char));
    if (!summary.cacheable_pure) {
        machine_select_call_effect_summary_free(&summary);
        machine_select_set_error(error, 0, 0, "MACHINE-SELECT-426: out of memory building purity summary");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const MachineSelectFunction *function = &program->functions[function_index];
        size_t block_index;
        int cacheable_pure = function->has_body ? 1 : 0;

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            const MachineSelectBasicBlock *block = &function->blocks[block_index];
            size_t op_index;

            for (op_index = 0; op_index < block->op_count; ++op_index) {
                const MachineSelectOp *op = &block->ops[op_index];

                switch (op->kind) {
                    case MACHINE_SELECT_OP_LOAD_GLOBAL:
                        if (op->as.load_slot.id < summary.global_count) {
                            summary.global_reads[(function_index * summary.global_count) + op->as.load_slot.id] = 1u;
                        }
                        break;
                    case MACHINE_SELECT_OP_STORE_GLOBAL:
                    case MACHINE_SELECT_OP_STORE_GLOBAL_IMM:
                        if (op->as.store.slot.id < summary.global_count) {
                            summary.global_writes[(function_index * summary.global_count) + op->as.store.slot.id] = 1u;
                        }
                        cacheable_pure = 0;
                        break;
                    case MACHINE_SELECT_OP_CALL:
                    case MACHINE_SELECT_OP_CALL_IMM:
                    case MACHINE_SELECT_OP_CALL_SPILL:
                    case MACHINE_SELECT_OP_CALL_IMM_SPILL:
                    case MACHINE_SELECT_OP_CALL_VOID:
                    case MACHINE_SELECT_OP_CALL_VOID_IMM:
                    case MACHINE_SELECT_OP_ADDR_LOCAL:
                    case MACHINE_SELECT_OP_ADDR_GLOBAL:
                    case MACHINE_SELECT_OP_LOAD_INDIRECT:
                    case MACHINE_SELECT_OP_STORE_INDIRECT:
                        cacheable_pure = 0;
                        break;
                    default:
                        break;
                }
            }
        }

        summary.cacheable_pure[function_index] = cacheable_pure ? 1u : 0u;
    }

    while (changed) {
        changed = 0;
        for (function_index = 0; function_index < program->function_count; ++function_index) {
            const MachineSelectFunction *function = &program->functions[function_index];
            size_t block_index;

            if (!summary.cacheable_pure[function_index]) {
                continue;
            }

            for (block_index = 0; block_index < function->block_count; ++block_index) {
                const MachineSelectBasicBlock *block = &function->blocks[block_index];
                size_t op_index;

                for (op_index = 0; op_index < block->op_count; ++op_index) {
                    const MachineSelectOp *op = &block->ops[op_index];
                    size_t callee_index;
                    size_t global_index;

                    if (!machine_select_op_is_call_kind(op->kind)) {
                        continue;
                    }
                    callee_index = machine_select_find_function_index_by_name(program, op->as.call.callee_name);
                    if (callee_index == (size_t)-1 || !summary.cacheable_pure[callee_index]) {
                        summary.cacheable_pure[function_index] = 0u;
                        changed = 1;
                        break;
                    }
                    for (global_index = 0; global_index < summary.global_count; ++global_index) {
                        unsigned char *read_slot =
                            &summary.global_reads[(function_index * summary.global_count) + global_index];
                        unsigned char *write_slot =
                            &summary.global_writes[(function_index * summary.global_count) + global_index];
                        unsigned char callee_reads =
                            summary.global_reads[(callee_index * summary.global_count) + global_index];
                        unsigned char callee_writes =
                            summary.global_writes[(callee_index * summary.global_count) + global_index];

                        if (!*read_slot && callee_reads) {
                            *read_slot = 1u;
                            changed = 1;
                        }
                        if (!*write_slot && callee_writes) {
                            *write_slot = 1u;
                            changed = 1;
                        }
                        if (callee_writes) {
                            summary.cacheable_pure[function_index] = 0u;
                            changed = 1;
                        }
                    }
                    if (!summary.cacheable_pure[function_index]) {
                        break;
                    }
                }

                if (!summary.cacheable_pure[function_index]) {
                    break;
                }
            }
        }
    }

    *out_summary = summary;
    return 1;
}

static size_t machine_select_operand_resource_slot(const MachineSelectProgram *program,
    const MachineSelectFunction *function,
    MachineSelectOperand operand) {
    if (!program || !function) {
        return (size_t)-1;
    }
    switch (operand.kind) {
        case MACHINE_SELECT_OPERAND_REGISTER:
            if (operand.machine_register_id >= program->register_bank.register_count) {
                return (size_t)-1;
            }
            return operand.machine_register_id;
        case MACHINE_SELECT_OPERAND_SPILL_SLOT:
            if (operand.spill_slot >= function->spill_slot_count) {
                return (size_t)-1;
            }
            return program->register_bank.register_count + operand.spill_slot;
        default:
            return (size_t)-1;
    }
}

static int machine_select_operand_get_resource_index(const MachineSelectProgram *program,
    const MachineSelectFunction *function,
    MachineSelectOperand operand,
    size_t *out_index) {
    size_t index = machine_select_operand_resource_slot(program, function, operand);

    if (out_index) {
        *out_index = 0u;
    }
    if (index == (size_t)-1) {
        return 0;
    }
    if (out_index) {
        *out_index = index;
    }
    return 1;
}

static int machine_select_cleanup_block_reaches_target(const MachineSelectBasicBlock *block,
    size_t target_block_id) {
    if (!block) {
        return 0;
    }
    switch (block->terminator.kind) {
        case MACHINE_SELECT_TERM_JUMP:
            return block->terminator.as.jump_target == target_block_id;
        case MACHINE_SELECT_TERM_BRANCH:
            return block->terminator.as.branch.then_target == target_block_id ||
                block->terminator.as.branch.else_target == target_block_id;
        case MACHINE_SELECT_TERM_COMPARE_BRANCH:
        case MACHINE_SELECT_TERM_COMPARE_BRANCH_IMM:
            return block->terminator.as.compare_branch.then_target == target_block_id ||
                block->terminator.as.compare_branch.else_target == target_block_id;
        default:
            return 0;
    }
}

static int machine_select_cleanup_compute_predecessor_counts(const MachineSelectFunction *function,
    size_t **out_predecessor_counts,
    MachineSelectError *error) {
    size_t *predecessor_counts = NULL;
    size_t block_index;

    if (out_predecessor_counts) {
        *out_predecessor_counts = NULL;
    }
    if (!function || !out_predecessor_counts) {
        machine_select_set_error(error, 0, 0, "MACHINE-SELECT-420: invalid predecessor-count cleanup contract");
        return 0;
    }
    if (function->block_count == 0) {
        return 1;
    }

    predecessor_counts = (size_t *)calloc(function->block_count, sizeof(size_t));
    if (!predecessor_counts) {
        machine_select_set_error(error, 0, 0, "MACHINE-SELECT-421: out of memory building predecessor counts");
        return 0;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        const MachineSelectBasicBlock *block = &function->blocks[block_index];

        switch (block->terminator.kind) {
            case MACHINE_SELECT_TERM_JUMP:
                if (block->terminator.as.jump_target < function->block_count) {
                    ++predecessor_counts[block->terminator.as.jump_target];
                }
                break;
            case MACHINE_SELECT_TERM_BRANCH:
                if (block->terminator.as.branch.then_target < function->block_count) {
                    ++predecessor_counts[block->terminator.as.branch.then_target];
                }
                if (block->terminator.as.branch.else_target < function->block_count &&
                    block->terminator.as.branch.else_target != block->terminator.as.branch.then_target) {
                    ++predecessor_counts[block->terminator.as.branch.else_target];
                }
                break;
            case MACHINE_SELECT_TERM_COMPARE_BRANCH:
            case MACHINE_SELECT_TERM_COMPARE_BRANCH_IMM:
                if (block->terminator.as.compare_branch.then_target < function->block_count) {
                    ++predecessor_counts[block->terminator.as.compare_branch.then_target];
                }
                if (block->terminator.as.compare_branch.else_target < function->block_count &&
                    block->terminator.as.compare_branch.else_target !=
                        block->terminator.as.compare_branch.then_target) {
                    ++predecessor_counts[block->terminator.as.compare_branch.else_target];
                }
                break;
            default:
                break;
        }
    }

    *out_predecessor_counts = predecessor_counts;
    return 1;
}

static int machine_select_cleanup_alias_states_equal(size_t resource_count,
    const unsigned char *lhs_valid,
    const MachineSelectOperand *lhs_values,
    const unsigned char *rhs_valid,
    const MachineSelectOperand *rhs_values) {
    size_t resource_index;

    for (resource_index = 0; resource_index < resource_count; ++resource_index) {
        if (lhs_valid[resource_index] != rhs_valid[resource_index]) {
            return 0;
        }
        if (lhs_valid[resource_index] &&
            !machine_select_operands_equal(lhs_values[resource_index], rhs_values[resource_index])) {
            return 0;
        }
    }
    return 1;
}

static MachineSelectOperand machine_select_cleanup_resolve_alias(size_t resource_count,
    const MachineSelectProgram *program,
    const MachineSelectFunction *function,
    const unsigned char *alias_valid,
    const MachineSelectOperand *alias_values,
    MachineSelectOperand operand) {
    size_t hop_count = 0;

    while (hop_count < resource_count) {
        size_t resource_index = machine_select_operand_resource_slot(program, function, operand);

        if (resource_index == (size_t)-1 ||
            !alias_valid ||
            !alias_values ||
            resource_index >= resource_count ||
            !alias_valid[resource_index]) {
            return operand;
        }
        operand = alias_values[resource_index];
        ++hop_count;
    }

    return operand;
}

static void machine_select_cleanup_invalidate_aliases(const MachineSelectProgram *program,
    const MachineSelectFunction *function,
    size_t resource_count,
    unsigned char *alias_valid,
    const MachineSelectOperand *alias_values,
    MachineSelectOperand killed_operand) {
    size_t killed_index;
    size_t resource_index;

    if (!alias_valid || !alias_values) {
        return;
    }
    if (!machine_select_operand_get_resource_index(program, function, killed_operand, &killed_index)) {
        return;
    }

    if (killed_index < resource_count) {
        alias_valid[killed_index] = 0u;
    }
    for (resource_index = 0; resource_index < resource_count; ++resource_index) {
        if (!alias_valid[resource_index]) {
            continue;
        }
        if (machine_select_operand_matches_result(alias_values[resource_index], killed_operand)) {
            alias_valid[resource_index] = 0u;
        }
    }
}

static void machine_select_cleanup_invalidate_caller_clobbered_aliases(const MachineSelectProgram *program,
    const MachineSelectFunction *function,
    size_t resource_count,
    unsigned char *alias_valid,
    const MachineSelectOperand *alias_values) {
    size_t register_index;

    if (!program || !function || !alias_valid || !alias_values) {
        return;
    }

    for (register_index = 0; register_index < program->register_bank.register_count; ++register_index) {
        if (!program->register_bank.registers[register_index].caller_clobbered) {
            continue;
        }
        machine_select_cleanup_invalidate_aliases(
            program,
            function,
            resource_count,
            alias_valid,
            alias_values,
            machine_select_operand_register(register_index));
    }
}

static void machine_select_cleanup_rewrite_op_operands_with_aliases(size_t resource_count,
    const MachineSelectProgram *program,
    const MachineSelectFunction *function,
    const unsigned char *alias_valid,
    const MachineSelectOperand *alias_values,
    MachineSelectOp *op,
    int *out_changed) {
    MachineSelectOperand before;
    MachineSelectOperand after;
    size_t arg_index;

    if (!op) {
        return;
    }

    switch (op->kind) {
        case MACHINE_SELECT_OP_COPY:
        case MACHINE_SELECT_OP_MATERIALIZE_IMM:
            before = op->as.copy_value;
            after = machine_select_cleanup_resolve_alias(
                resource_count, program, function, alias_valid, alias_values, before);
                if (!machine_select_operands_equal(before, after)) {
                    op->as.copy_value = after;
                    if (out_changed) {
                        *out_changed = 1;
                }
            }
            break;
        case MACHINE_SELECT_OP_ALU:
        case MACHINE_SELECT_OP_ALU_IMM:
        case MACHINE_SELECT_OP_CMP:
        case MACHINE_SELECT_OP_CMP_IMM:
            before = op->as.binary.lhs;
            after = machine_select_cleanup_resolve_alias(
                resource_count, program, function, alias_valid, alias_values, before);
            if (!machine_select_operands_equal(before, after)) {
                op->as.binary.lhs = after;
                if (out_changed) {
                    *out_changed = 1;
                }
            }
            before = op->as.binary.rhs;
            after = machine_select_cleanup_resolve_alias(
                resource_count, program, function, alias_valid, alias_values, before);
            if (!machine_select_operands_equal(before, after)) {
                op->as.binary.rhs = after;
                if (out_changed) {
                    *out_changed = 1;
                }
            }
            break;
        case MACHINE_SELECT_OP_CALL:
        case MACHINE_SELECT_OP_CALL_IMM:
        case MACHINE_SELECT_OP_CALL_SPILL:
        case MACHINE_SELECT_OP_CALL_IMM_SPILL:
        case MACHINE_SELECT_OP_CALL_VOID:
        case MACHINE_SELECT_OP_CALL_VOID_IMM:
            for (arg_index = 0; arg_index < op->as.call.arg_count; ++arg_index) {
                before = op->as.call.args[arg_index];
                after = machine_select_cleanup_resolve_alias(
                    resource_count, program, function, alias_valid, alias_values, before);
                if (!machine_select_operands_equal(before, after)) {
                    op->as.call.args[arg_index] = after;
                    if (out_changed) {
                        *out_changed = 1;
                    }
                }
            }
            break;
        case MACHINE_SELECT_OP_STORE_LOCAL:
        case MACHINE_SELECT_OP_STORE_LOCAL_IMM:
        case MACHINE_SELECT_OP_STORE_GLOBAL:
        case MACHINE_SELECT_OP_STORE_GLOBAL_IMM:
            before = op->as.store.value;
            after = machine_select_cleanup_resolve_alias(
                resource_count, program, function, alias_valid, alias_values, before);
            if (!machine_select_operands_equal(before, after)) {
                op->as.store.value = after;
                if (out_changed) {
                    *out_changed = 1;
                }
            }
            break;
        default:
            break;
    }
}

static void machine_select_cleanup_rewrite_term_operands_with_aliases(size_t resource_count,
    const MachineSelectProgram *program,
    const MachineSelectFunction *function,
    const unsigned char *alias_valid,
    const MachineSelectOperand *alias_values,
    MachineSelectTerminator *terminator,
    int *out_changed) {
    MachineSelectOperand before;
    MachineSelectOperand after;

    if (!terminator) {
        return;
    }

    switch (terminator->kind) {
        case MACHINE_SELECT_TERM_RETURN:
        case MACHINE_SELECT_TERM_RETURN_IMM:
        case MACHINE_SELECT_TERM_RETURN_SPILL:
            before = terminator->as.return_value;
            after = machine_select_cleanup_resolve_alias(
                resource_count, program, function, alias_valid, alias_values, before);
            if (!machine_select_operands_equal(before, after)) {
                terminator->as.return_value = after;
                if (out_changed) {
                    *out_changed = 1;
                }
            }
            break;
        case MACHINE_SELECT_TERM_BRANCH:
            before = terminator->as.branch.condition;
            after = machine_select_cleanup_resolve_alias(
                resource_count, program, function, alias_valid, alias_values, before);
            if (!machine_select_operands_equal(before, after)) {
                terminator->as.branch.condition = after;
                if (out_changed) {
                    *out_changed = 1;
                }
            }
            break;
        case MACHINE_SELECT_TERM_COMPARE_BRANCH:
        case MACHINE_SELECT_TERM_COMPARE_BRANCH_IMM:
            before = terminator->as.compare_branch.lhs;
            after = machine_select_cleanup_resolve_alias(
                resource_count, program, function, alias_valid, alias_values, before);
            if (!machine_select_operands_equal(before, after)) {
                terminator->as.compare_branch.lhs = after;
                if (out_changed) {
                    *out_changed = 1;
                }
            }
            before = terminator->as.compare_branch.rhs;
            after = machine_select_cleanup_resolve_alias(
                resource_count, program, function, alias_valid, alias_values, before);
            if (!machine_select_operands_equal(before, after)) {
                terminator->as.compare_branch.rhs = after;
                if (out_changed) {
                    *out_changed = 1;
                }
            }
            break;
        default:
            break;
    }
}

static void machine_select_cleanup_collect_block_use_def(const MachineSelectProgram *program,
    const MachineSelectFunction *function,
    const MachineSelectBasicBlock *block,
    size_t resource_count,
    unsigned char *use_set,
    unsigned char *def_set) {
    unsigned char *seen_defs = NULL;
    size_t op_index;
    size_t resource_index;

    if (!program || !function || !block || resource_count == 0 || !use_set || !def_set) {
        return;
    }

    seen_defs = (unsigned char *)calloc(resource_count, sizeof(unsigned char));
    if (!seen_defs) {
        return;
    }

    for (op_index = 0; op_index < block->op_count; ++op_index) {
        const MachineSelectOp *op = &block->ops[op_index];
        size_t resource_index;
        size_t arg_index;

        switch (op->kind) {
            case MACHINE_SELECT_OP_COPY:
            case MACHINE_SELECT_OP_MATERIALIZE_IMM:
                if (machine_select_operand_get_resource_index(
                        program, function, op->as.copy_value, &resource_index) &&
                    !seen_defs[resource_index]) {
                    use_set[resource_index] = 1u;
                }
                break;
            case MACHINE_SELECT_OP_ALU:
            case MACHINE_SELECT_OP_ALU_IMM:
            case MACHINE_SELECT_OP_CMP:
            case MACHINE_SELECT_OP_CMP_IMM:
                if (machine_select_operand_get_resource_index(
                        program, function, op->as.binary.lhs, &resource_index) &&
                    !seen_defs[resource_index]) {
                    use_set[resource_index] = 1u;
                }
                if (machine_select_operand_get_resource_index(
                        program, function, op->as.binary.rhs, &resource_index) &&
                    !seen_defs[resource_index]) {
                    use_set[resource_index] = 1u;
                }
                break;
            case MACHINE_SELECT_OP_CALL:
            case MACHINE_SELECT_OP_CALL_IMM:
            case MACHINE_SELECT_OP_CALL_SPILL:
            case MACHINE_SELECT_OP_CALL_IMM_SPILL:
            case MACHINE_SELECT_OP_CALL_VOID:
            case MACHINE_SELECT_OP_CALL_VOID_IMM:
                for (arg_index = 0; arg_index < op->as.call.arg_count; ++arg_index) {
                    if (machine_select_operand_get_resource_index(
                            program, function, op->as.call.args[arg_index], &resource_index) &&
                        !seen_defs[resource_index]) {
                        use_set[resource_index] = 1u;
                    }
                }
                for (resource_index = 0; resource_index < program->register_bank.register_count; ++resource_index) {
                    if (!program->register_bank.registers[resource_index].caller_clobbered) {
                        continue;
                    }
                    seen_defs[resource_index] = 1u;
                    def_set[resource_index] = 1u;
                }
                break;
            case MACHINE_SELECT_OP_STORE_LOCAL:
            case MACHINE_SELECT_OP_STORE_LOCAL_IMM:
            case MACHINE_SELECT_OP_STORE_GLOBAL:
            case MACHINE_SELECT_OP_STORE_GLOBAL_IMM:
                if (machine_select_operand_get_resource_index(
                        program, function, op->as.store.value, &resource_index) &&
                    !seen_defs[resource_index]) {
                    use_set[resource_index] = 1u;
                }
                break;
            case MACHINE_SELECT_OP_LOAD_INDIRECT:
                if (machine_select_operand_get_resource_index(
                        program, function, op->as.load_indirect_addr, &resource_index) &&
                    !seen_defs[resource_index]) {
                    use_set[resource_index] = 1u;
                }
                break;
            case MACHINE_SELECT_OP_STORE_INDIRECT:
                if (machine_select_operand_get_resource_index(
                        program, function, op->as.store_indirect.addr, &resource_index) &&
                    !seen_defs[resource_index]) {
                    use_set[resource_index] = 1u;
                }
                if (machine_select_operand_get_resource_index(
                        program, function, op->as.store_indirect.value, &resource_index) &&
                    !seen_defs[resource_index]) {
                    use_set[resource_index] = 1u;
                }
                break;
            default:
                break;
        }

        if (op->has_result &&
            machine_select_operand_get_resource_index(program, function, op->result, &resource_index)) {
            seen_defs[resource_index] = 1u;
            def_set[resource_index] = 1u;
        }
    }

    switch (block->terminator.kind) {
        case MACHINE_SELECT_TERM_RETURN:
        case MACHINE_SELECT_TERM_RETURN_IMM:
        case MACHINE_SELECT_TERM_RETURN_SPILL:
            if (machine_select_operand_get_resource_index(
                    program, function, block->terminator.as.return_value, &resource_index) &&
                !seen_defs[resource_index]) {
                use_set[resource_index] = 1u;
            }
            break;
        case MACHINE_SELECT_TERM_BRANCH:
            if (machine_select_operand_get_resource_index(
                    program, function, block->terminator.as.branch.condition, &resource_index) &&
                !seen_defs[resource_index]) {
                use_set[resource_index] = 1u;
            }
            break;
        case MACHINE_SELECT_TERM_COMPARE_BRANCH:
        case MACHINE_SELECT_TERM_COMPARE_BRANCH_IMM:
            if (machine_select_operand_get_resource_index(
                    program, function, block->terminator.as.compare_branch.lhs, &resource_index) &&
                !seen_defs[resource_index]) {
                use_set[resource_index] = 1u;
            }
            if (machine_select_operand_get_resource_index(
                    program, function, block->terminator.as.compare_branch.rhs, &resource_index) &&
                !seen_defs[resource_index]) {
                use_set[resource_index] = 1u;
            }
            break;
        default:
            break;
    }

    free(seen_defs);
}

static int machine_select_cleanup_operand_is_live_out(const MachineSelectProgram *program,
    const MachineSelectFunction *function,
    size_t block_index,
    size_t resource_count,
    const unsigned char *live_out_sets,
    MachineSelectOperand operand) {
    size_t resource_index;

    if (!program || !function || !live_out_sets) {
        return 0;
    }
    if (!machine_select_operand_get_resource_index(program, function, operand, &resource_index) ||
        resource_index >= resource_count) {
        return 0;
    }
    return live_out_sets[(block_index * resource_count) + resource_index] != 0;
}

static int machine_select_cleanup_compute_block_live_out(const MachineSelectProgram *program,
    const MachineSelectFunction *function,
    size_t resource_count,
    unsigned char **out_live_out_sets,
    MachineSelectError *error) {
    unsigned char *use_sets = NULL;
    unsigned char *def_sets = NULL;
    unsigned char *live_in_sets = NULL;
    unsigned char *live_out_sets = NULL;
    int changed = 1;
    size_t block_index;

    if (out_live_out_sets) {
        *out_live_out_sets = NULL;
    }
    if (!program || !function || !out_live_out_sets) {
        machine_select_set_error(error, 0, 0, "MACHINE-SELECT-422: invalid block-liveness cleanup contract");
        return 0;
    }
    if (function->block_count == 0 || resource_count == 0) {
        return 1;
    }

    use_sets = (unsigned char *)calloc(function->block_count * resource_count, sizeof(unsigned char));
    def_sets = (unsigned char *)calloc(function->block_count * resource_count, sizeof(unsigned char));
    live_in_sets = (unsigned char *)calloc(function->block_count * resource_count, sizeof(unsigned char));
    live_out_sets = (unsigned char *)calloc(function->block_count * resource_count, sizeof(unsigned char));
    if (!use_sets || !def_sets || !live_in_sets || !live_out_sets) {
        machine_select_set_error(error, 0, 0, "MACHINE-SELECT-423: out of memory computing block liveness");
        free(live_out_sets);
        free(live_in_sets);
        free(def_sets);
        free(use_sets);
        return 0;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        machine_select_cleanup_collect_block_use_def(program,
            function,
            &function->blocks[block_index],
            resource_count,
            use_sets + (block_index * resource_count),
            def_sets + (block_index * resource_count));
    }

    while (changed) {
        changed = 0;
        for (block_index = function->block_count; block_index-- > 0;) {
            const MachineSelectBasicBlock *block = &function->blocks[block_index];
            unsigned char *live_in = live_in_sets + (block_index * resource_count);
            unsigned char *live_out = live_out_sets + (block_index * resource_count);
            size_t successor_a = (size_t)-1;
            size_t successor_b = (size_t)-1;
            size_t resource_index;

            switch (block->terminator.kind) {
                case MACHINE_SELECT_TERM_JUMP:
                    successor_a = block->terminator.as.jump_target;
                    break;
                case MACHINE_SELECT_TERM_BRANCH:
                    successor_a = block->terminator.as.branch.then_target;
                    successor_b = block->terminator.as.branch.else_target;
                    break;
                case MACHINE_SELECT_TERM_COMPARE_BRANCH:
                case MACHINE_SELECT_TERM_COMPARE_BRANCH_IMM:
                    successor_a = block->terminator.as.compare_branch.then_target;
                    successor_b = block->terminator.as.compare_branch.else_target;
                    break;
                default:
                    break;
            }

            for (resource_index = 0; resource_index < resource_count; ++resource_index) {
                unsigned char new_live_out = 0;
                unsigned char new_live_in;

                if (successor_a != (size_t)-1) {
                    new_live_out |= live_in_sets[(successor_a * resource_count) + resource_index];
                }
                if (successor_b != (size_t)-1) {
                    new_live_out |= live_in_sets[(successor_b * resource_count) + resource_index];
                }

                new_live_in = use_sets[(block_index * resource_count) + resource_index];
                if (!def_sets[(block_index * resource_count) + resource_index] && new_live_out) {
                    new_live_in = 1u;
                }

                if (live_out[resource_index] != new_live_out) {
                    live_out[resource_index] = new_live_out;
                    changed = 1;
                }
                if (live_in[resource_index] != new_live_in) {
                    live_in[resource_index] = new_live_in;
                    changed = 1;
                }
            }
        }
    }

    free(live_in_sets);
    free(def_sets);
    free(use_sets);
    *out_live_out_sets = live_out_sets;
    return 1;
}

static void machine_select_cleanup_compute_block_entry_aliases(const MachineSelectFunction *function,
    size_t block_id,
    size_t resource_count,
    const size_t *predecessor_counts,
    const unsigned char *block_exit_alias_valid,
    const MachineSelectOperand *block_exit_alias_values,
    unsigned char *out_alias_valid,
    MachineSelectOperand *out_alias_values) {
    size_t resource_index;
    size_t predecessor_block_id;

    if (!function || !predecessor_counts || !block_exit_alias_valid || !block_exit_alias_values ||
        !out_alias_valid || !out_alias_values || block_id >= function->block_count) {
        return;
    }

    memset(out_alias_valid, 0, resource_count * sizeof(unsigned char));
    memset(out_alias_values, 0, resource_count * sizeof(MachineSelectOperand));
    if (block_id == 0 || predecessor_counts[block_id] == 0) {
        return;
    }

    for (resource_index = 0; resource_index < resource_count; ++resource_index) {
        int have_candidate = 0;
        MachineSelectOperand candidate = machine_select_operand_none();
        int all_match = 1;

        for (predecessor_block_id = 0; predecessor_block_id < function->block_count; ++predecessor_block_id) {
            const MachineSelectBasicBlock *predecessor = &function->blocks[predecessor_block_id];
            const unsigned char *pred_valid = block_exit_alias_valid + (predecessor_block_id * resource_count);
            const MachineSelectOperand *pred_values = block_exit_alias_values + (predecessor_block_id * resource_count);

            if (!machine_select_cleanup_block_reaches_target(predecessor, block_id)) {
                continue;
            }
            if (!pred_valid[resource_index]) {
                all_match = 0;
                break;
            }
            if (!have_candidate) {
                candidate = pred_values[resource_index];
                have_candidate = 1;
                continue;
            }
            if (!machine_select_operands_equal(candidate, pred_values[resource_index])) {
                all_match = 0;
                break;
            }
        }

        if (have_candidate && all_match) {
            out_alias_valid[resource_index] = 1u;
            out_alias_values[resource_index] = candidate;
        }
    }
}

static int machine_select_cleanup_compute_block_exit_aliases(const MachineSelectProgram *program,
    const MachineSelectFunction *function,
    size_t resource_count,
    const size_t *predecessor_counts,
    unsigned char *block_exit_alias_valid,
    MachineSelectOperand *block_exit_alias_values,
    MachineSelectError *error) {
    unsigned char *entry_alias_valid = NULL;
    MachineSelectOperand *entry_alias_values = NULL;
    unsigned char *local_alias_valid = NULL;
    MachineSelectOperand *local_alias_values = NULL;
    int changed = 1;
    size_t block_index;

    if (!program || !function || !predecessor_counts || !block_exit_alias_valid || !block_exit_alias_values) {
        machine_select_set_error(error, 0, 0, "MACHINE-SELECT-424: invalid block-exit alias computation contract");
        return 0;
    }

    entry_alias_valid = (unsigned char *)calloc(resource_count, sizeof(unsigned char));
    entry_alias_values = (MachineSelectOperand *)calloc(resource_count, sizeof(MachineSelectOperand));
    local_alias_valid = (unsigned char *)calloc(resource_count, sizeof(unsigned char));
    local_alias_values = (MachineSelectOperand *)calloc(resource_count, sizeof(MachineSelectOperand));
    if (!entry_alias_valid || !entry_alias_values || !local_alias_valid || !local_alias_values) {
        machine_select_set_error(error, 0, 0, "MACHINE-SELECT-425: out of memory computing block-exit aliases");
        free(local_alias_values);
        free(local_alias_valid);
        free(entry_alias_values);
        free(entry_alias_valid);
        return 0;
    }

    while (changed) {
        changed = 0;
        for (block_index = 0; block_index < function->block_count; ++block_index) {
            const MachineSelectBasicBlock *block = &function->blocks[block_index];
            unsigned char *exit_valid = block_exit_alias_valid + (block_index * resource_count);
            MachineSelectOperand *exit_values = block_exit_alias_values + (block_index * resource_count);
            size_t op_index;
            size_t result_index;

            machine_select_cleanup_compute_block_entry_aliases(
                function,
                block_index,
                resource_count,
                predecessor_counts,
                block_exit_alias_valid,
                block_exit_alias_values,
                entry_alias_valid,
                entry_alias_values);

            memcpy(local_alias_valid, entry_alias_valid, resource_count * sizeof(unsigned char));
            memcpy(local_alias_values, entry_alias_values, resource_count * sizeof(MachineSelectOperand));

            for (op_index = 0; op_index < block->op_count; ++op_index) {
                const MachineSelectOp *op = &block->ops[op_index];

                if (op->has_result &&
                    machine_select_operand_get_resource_index(program, function, op->result, &result_index)) {
                    machine_select_cleanup_invalidate_aliases(
                        program, function, resource_count, local_alias_valid, local_alias_values, op->result);
                }

                if ((op->kind == MACHINE_SELECT_OP_COPY || op->kind == MACHINE_SELECT_OP_MATERIALIZE_IMM) &&
                    op->has_result &&
                    machine_select_operand_get_resource_index(program, function, op->result, &result_index) &&
                    !machine_select_operand_matches_result(op->result, op->as.copy_value)) {
                    local_alias_valid[result_index] = 1u;
                    local_alias_values[result_index] =
                        machine_select_cleanup_resolve_alias(resource_count,
                            program,
                            function,
                            local_alias_valid,
                            local_alias_values,
                            op->as.copy_value);
                }
                if (op->kind == MACHINE_SELECT_OP_CALL ||
                    op->kind == MACHINE_SELECT_OP_CALL_IMM ||
                    op->kind == MACHINE_SELECT_OP_CALL_SPILL ||
                    op->kind == MACHINE_SELECT_OP_CALL_IMM_SPILL ||
                    op->kind == MACHINE_SELECT_OP_CALL_VOID ||
                    op->kind == MACHINE_SELECT_OP_CALL_VOID_IMM) {
                    machine_select_cleanup_invalidate_caller_clobbered_aliases(
                        program,
                        function,
                        resource_count,
                        local_alias_valid,
                        local_alias_values);
                }
            }

            if (!machine_select_cleanup_alias_states_equal(
                    resource_count,
                    exit_valid,
                    exit_values,
                    local_alias_valid,
                    local_alias_values)) {
                memcpy(exit_valid, local_alias_valid, resource_count * sizeof(unsigned char));
                memcpy(exit_values, local_alias_values, resource_count * sizeof(MachineSelectOperand));
                changed = 1;
            }
        }
    }

    free(local_alias_values);
    free(local_alias_valid);
    free(entry_alias_values);
    free(entry_alias_valid);
    return 1;
}

static int machine_select_cleanup_propagate_aliases_in_function(const MachineSelectProgram *program,
    MachineSelectFunction *function,
    int *out_changed,
    MachineSelectError *error) {
    size_t resource_count;
    size_t *predecessor_counts = NULL;
    unsigned char *block_exit_alias_valid = NULL;
    MachineSelectOperand *block_exit_alias_values = NULL;
    unsigned char *alias_valid = NULL;
    MachineSelectOperand *alias_values = NULL;
    size_t block_index;

    if (out_changed) {
        *out_changed = 0;
    }
    if (!program || !function) {
        machine_select_set_error(error, 0, 0, "MACHINE-SELECT-426: invalid alias-cleanup contract");
        return 0;
    }

    resource_count = program->register_bank.register_count + function->spill_slot_count;
    if (function->block_count == 0 || resource_count == 0) {
        return 1;
    }

    if (!machine_select_cleanup_compute_predecessor_counts(function, &predecessor_counts, error)) {
        return 0;
    }

    block_exit_alias_valid =
        (unsigned char *)calloc(function->block_count * resource_count, sizeof(unsigned char));
    block_exit_alias_values =
        (MachineSelectOperand *)calloc(function->block_count * resource_count, sizeof(MachineSelectOperand));
    alias_valid = (unsigned char *)calloc(resource_count, sizeof(unsigned char));
    alias_values = (MachineSelectOperand *)calloc(resource_count, sizeof(MachineSelectOperand));
    if (!block_exit_alias_valid || !block_exit_alias_values || !alias_valid || !alias_values) {
        machine_select_set_error(error, 0, 0, "MACHINE-SELECT-427: out of memory preparing alias cleanup");
        free(alias_values);
        free(alias_valid);
        free(block_exit_alias_values);
        free(block_exit_alias_valid);
        free(predecessor_counts);
        return 0;
    }

    if (!machine_select_cleanup_compute_block_exit_aliases(program,
            function,
            resource_count,
            predecessor_counts,
            block_exit_alias_valid,
            block_exit_alias_values,
            error)) {
        free(alias_values);
        free(alias_valid);
        free(block_exit_alias_values);
        free(block_exit_alias_valid);
        free(predecessor_counts);
        return 0;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        MachineSelectBasicBlock *block = &function->blocks[block_index];
        size_t op_index;
        size_t result_index;

        machine_select_cleanup_compute_block_entry_aliases(
            function,
            block_index,
            resource_count,
            predecessor_counts,
            block_exit_alias_valid,
            block_exit_alias_values,
            alias_valid,
            alias_values);

        for (op_index = 0; op_index < block->op_count; ++op_index) {
            MachineSelectOp *op = &block->ops[op_index];

            machine_select_cleanup_rewrite_op_operands_with_aliases(
                resource_count, program, function, alias_valid, alias_values, op, out_changed);
            if (machine_select_cleanup_op_shape(op)) {
                if (out_changed) {
                    *out_changed = 1;
                }
            }

            if (op->has_result &&
                machine_select_operand_get_resource_index(program, function, op->result, &result_index)) {
                machine_select_cleanup_invalidate_aliases(
                    program, function, resource_count, alias_valid, alias_values, op->result);
            }

            if ((op->kind == MACHINE_SELECT_OP_COPY || op->kind == MACHINE_SELECT_OP_MATERIALIZE_IMM) &&
                op->has_result &&
                machine_select_operand_get_resource_index(program, function, op->result, &result_index) &&
                !machine_select_operand_matches_result(op->result, op->as.copy_value)) {
                alias_valid[result_index] = 1u;
                alias_values[result_index] =
                    machine_select_cleanup_resolve_alias(resource_count,
                        program,
                        function,
                        alias_valid,
                        alias_values,
                        op->as.copy_value);
            }
            if (op->kind == MACHINE_SELECT_OP_CALL ||
                op->kind == MACHINE_SELECT_OP_CALL_IMM ||
                op->kind == MACHINE_SELECT_OP_CALL_SPILL ||
                op->kind == MACHINE_SELECT_OP_CALL_IMM_SPILL ||
                op->kind == MACHINE_SELECT_OP_CALL_VOID ||
                op->kind == MACHINE_SELECT_OP_CALL_VOID_IMM) {
                machine_select_cleanup_invalidate_caller_clobbered_aliases(
                    program,
                    function,
                    resource_count,
                    alias_valid,
                    alias_values);
            }
        }

        machine_select_cleanup_rewrite_term_operands_with_aliases(
            resource_count, program, function, alias_valid, alias_values, &block->terminator, out_changed);
        if (machine_select_cleanup_terminator_shape(block)) {
            if (out_changed) {
                *out_changed = 1;
            }
        }
    }

    free(alias_values);
    free(alias_valid);
    free(block_exit_alias_values);
    free(block_exit_alias_valid);
    free(predecessor_counts);
    return 1;
}

static void machine_select_cleanup_cfg_free(MachineSelectCleanupCfg *cfg) {
    if (!cfg) {
        return;
    }
    free(cfg->predecessor_matrix);
    cfg->predecessor_matrix = NULL;
    cfg->block_count = 0;
}

static int machine_select_cleanup_cfg_init(const MachineSelectFunction *function,
    MachineSelectCleanupCfg *out_cfg) {
    size_t block_index;

    if (!function || !out_cfg) {
        return 0;
    }

    memset(out_cfg, 0, sizeof(*out_cfg));
    out_cfg->block_count = function->block_count;
    if (function->block_count == 0) {
        return 1;
    }

    out_cfg->predecessor_matrix =
        (unsigned char *)calloc(function->block_count * function->block_count, sizeof(unsigned char));
    if (!out_cfg->predecessor_matrix) {
        machine_select_cleanup_cfg_free(out_cfg);
        return 0;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        const MachineSelectBasicBlock *block = &function->blocks[block_index];

        switch (block->terminator.kind) {
            case MACHINE_SELECT_TERM_JUMP:
                if (block->terminator.as.jump_target < function->block_count) {
                    out_cfg->predecessor_matrix[block_index * function->block_count +
                        block->terminator.as.jump_target] = 1u;
                }
                break;
            case MACHINE_SELECT_TERM_BRANCH:
                if (block->terminator.as.branch.then_target < function->block_count) {
                    out_cfg->predecessor_matrix[block_index * function->block_count +
                        block->terminator.as.branch.then_target] = 1u;
                }
                if (block->terminator.as.branch.else_target < function->block_count) {
                    out_cfg->predecessor_matrix[block_index * function->block_count +
                        block->terminator.as.branch.else_target] = 1u;
                }
                break;
            case MACHINE_SELECT_TERM_COMPARE_BRANCH:
            case MACHINE_SELECT_TERM_COMPARE_BRANCH_IMM:
                if (block->terminator.as.compare_branch.then_target < function->block_count) {
                    out_cfg->predecessor_matrix[block_index * function->block_count +
                        block->terminator.as.compare_branch.then_target] = 1u;
                }
                if (block->terminator.as.compare_branch.else_target < function->block_count) {
                    out_cfg->predecessor_matrix[block_index * function->block_count +
                        block->terminator.as.compare_branch.else_target] = 1u;
                }
                break;
            default:
                break;
        }
    }

    return 1;
}

static int machine_select_terminator_has_successor(const MachineSelectTerminator *terminator) {
    if (!terminator) {
        return 0;
    }
    switch (terminator->kind) {
        case MACHINE_SELECT_TERM_JUMP:
        case MACHINE_SELECT_TERM_BRANCH:
        case MACHINE_SELECT_TERM_COMPARE_BRANCH:
        case MACHINE_SELECT_TERM_COMPARE_BRANCH_IMM:
            return 1;
        default:
            return 0;
    }
}

static size_t machine_select_count_visible_result_uses_in_block(const MachineSelectProgram *program,
    const MachineSelectFunction *function,
    const MachineSelectBasicBlock *block,
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
        if (machine_select_op_redefines_result(&block->ops[op_index], result) ||
            machine_select_op_clobbers_operand_via_call(program, function, &block->ops[op_index], result)) {
            if (out_redefined_before_terminator) {
                *out_redefined_before_terminator = 1;
            }
            return use_count;
        }
    }

    use_count += machine_select_count_term_result_uses(&block->terminator, result);
    return use_count;
}

static int machine_select_cleanup_op_writes_memory(const MachineSelectOp *op) {
    if (!op) {
        return 0;
    }
    switch (op->kind) {
        case MACHINE_SELECT_OP_STORE_LOCAL:
        case MACHINE_SELECT_OP_STORE_LOCAL_IMM:
        case MACHINE_SELECT_OP_STORE_GLOBAL:
        case MACHINE_SELECT_OP_STORE_GLOBAL_IMM:
        case MACHINE_SELECT_OP_STORE_INDIRECT:
            return 1;
        default:
            return 0;
    }
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
        case MACHINE_SELECT_OP_LOAD_INDIRECT:
            if (machine_select_operand_matches_result(op->as.load_indirect_addr, result)) {
                op->as.load_indirect_addr = replacement;
            }
            break;
        case MACHINE_SELECT_OP_STORE_INDIRECT:
            if (machine_select_operand_matches_result(op->as.store_indirect.addr, result)) {
                op->as.store_indirect.addr = replacement;
            }
            if (machine_select_operand_matches_result(op->as.store_indirect.value, result)) {
                op->as.store_indirect.value = replacement;
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

static int machine_select_rewrite_visible_uses_in_block(const MachineSelectProgram *program,
    const MachineSelectFunction *function,
    MachineSelectBasicBlock *block,
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
        if (machine_select_op_redefines_result(&block->ops[op_index], result) ||
            machine_select_op_clobbers_operand_via_call(program, function, &block->ops[op_index], result)) {
            return changed;
        }
    }

    if (machine_select_count_term_result_uses(&block->terminator, result) != 0) {
        machine_select_rewrite_term_operand(&block->terminator, result, replacement);
        changed = 1;
    }
    return changed;
}

static int machine_select_operand_redefined_before_result_uses_end_in_block(const MachineSelectProgram *program,
    const MachineSelectFunction *function,
    const MachineSelectBasicBlock *block,
    size_t producer_index,
    MachineSelectOperand result,
    MachineSelectOperand operand) {
    size_t op_index;
    int redefined_before_terminator = 0;
    size_t total_uses;

    if (!block || producer_index >= block->op_count) {
        return 0;
    }
    if (operand.kind != MACHINE_SELECT_OPERAND_REGISTER &&
        operand.kind != MACHINE_SELECT_OPERAND_SPILL_SLOT) {
        return 0;
    }

    total_uses = machine_select_count_visible_result_uses_in_block(
        program, function, block, producer_index, result, &redefined_before_terminator);
    if (total_uses == 0) {
        return 0;
    }

    for (op_index = producer_index + 1; op_index < block->op_count; ++op_index) {
        if (machine_select_op_redefines_result(&block->ops[op_index], result) ||
            machine_select_op_clobbers_operand_via_call(program, function, &block->ops[op_index], result)) {
            return 0;
        }
        if (machine_select_op_redefines_result(&block->ops[op_index], operand) ||
            machine_select_op_clobbers_operand_via_call(program, function, &block->ops[op_index], operand)) {
            return 1;
        }
    }

    return 0;
}

static int machine_select_insert_op_at(MachineSelectBasicBlock *block,
    size_t op_index,
    const MachineSelectOp *op,
    MachineSelectError *error) {
    MachineSelectOp *new_ops;
    size_t new_capacity;
    size_t move_count;

    if (!block || !op || op_index > block->op_count) {
        return 0;
    }
    if (block->op_count == block->op_capacity) {
        new_capacity = block->op_capacity == 0u ? 4u : block->op_capacity * 2u;
        new_ops = (MachineSelectOp *)realloc(block->ops, new_capacity * sizeof(MachineSelectOp));
        if (!new_ops) {
            machine_select_set_error(error, 0, 0, "MACHINE-SELECT-427: out of memory growing selected op table");
            return 0;
        }
        block->ops = new_ops;
        block->op_capacity = new_capacity;
    }

    move_count = block->op_count - op_index;
    if (move_count > 0u) {
        memmove(
            &block->ops[op_index + 1u],
            &block->ops[op_index],
            move_count * sizeof(MachineSelectOp));
    }
    block->ops[op_index] = *op;
    ++block->op_count;
    return 1;
}

static int machine_select_describe_call_arg_source_in_block(const MachineSelectBasicBlock *block,
    size_t use_index,
    MachineSelectOperand operand,
    MachineSelectCallArgDesc *out_desc) {
    size_t scan_index;

    if (out_desc) {
        memset(out_desc, 0, sizeof(*out_desc));
    }
    if (!block || !out_desc || use_index > block->op_count) {
        return 0;
    }

    if (operand.kind == MACHINE_SELECT_OPERAND_IMMEDIATE) {
        out_desc->kind = MACHINE_SELECT_CALL_ARG_DESC_IMMEDIATE;
        out_desc->immediate = operand.immediate;
        return 1;
    }
    if (operand.kind != MACHINE_SELECT_OPERAND_REGISTER &&
        operand.kind != MACHINE_SELECT_OPERAND_SPILL_SLOT) {
        return 0;
    }

    for (scan_index = use_index; scan_index > 0u; --scan_index) {
        const MachineSelectOp *op = &block->ops[scan_index - 1u];

        if (!machine_select_op_redefines_result(op, operand)) {
            continue;
        }

        switch (op->kind) {
            case MACHINE_SELECT_OP_COPY:
            case MACHINE_SELECT_OP_MATERIALIZE_IMM:
                return machine_select_describe_call_arg_source_in_block(
                    block, scan_index - 1u, op->as.copy_value, out_desc);
            case MACHINE_SELECT_OP_LOAD_LOCAL:
                out_desc->kind = MACHINE_SELECT_CALL_ARG_DESC_LOCAL_VALUE;
                out_desc->slot_id = op->as.load_slot.id;
                return 1;
            case MACHINE_SELECT_OP_LOAD_GLOBAL:
                out_desc->kind = MACHINE_SELECT_CALL_ARG_DESC_GLOBAL_VALUE;
                out_desc->slot_id = op->as.load_slot.id;
                return 1;
            case MACHINE_SELECT_OP_ADDR_LOCAL:
                out_desc->kind = MACHINE_SELECT_CALL_ARG_DESC_LOCAL_ADDRESS;
                out_desc->slot_id = op->as.addr_slot.id;
                return 1;
            case MACHINE_SELECT_OP_ADDR_GLOBAL:
                out_desc->kind = MACHINE_SELECT_CALL_ARG_DESC_GLOBAL_ADDRESS;
                out_desc->slot_id = op->as.addr_slot.id;
                return 1;
            default:
                return 0;
        }
    }

    if (operand.kind == MACHINE_SELECT_OPERAND_REGISTER || operand.kind == MACHINE_SELECT_OPERAND_SPILL_SLOT) {
        out_desc->kind = MACHINE_SELECT_CALL_ARG_DESC_LIVE_IN_OPERAND;
        out_desc->operand = operand;
        return 1;
    }

    return 0;
}

static int machine_select_call_arg_descs_equal(const MachineSelectCallArgDesc *lhs,
    const MachineSelectCallArgDesc *rhs) {
    if (!lhs || !rhs || lhs->kind != rhs->kind) {
        return 0;
    }
    switch (lhs->kind) {
        case MACHINE_SELECT_CALL_ARG_DESC_IMMEDIATE:
            return lhs->immediate == rhs->immediate;
        case MACHINE_SELECT_CALL_ARG_DESC_LOCAL_VALUE:
        case MACHINE_SELECT_CALL_ARG_DESC_GLOBAL_VALUE:
        case MACHINE_SELECT_CALL_ARG_DESC_LOCAL_ADDRESS:
        case MACHINE_SELECT_CALL_ARG_DESC_GLOBAL_ADDRESS:
            return lhs->slot_id == rhs->slot_id;
        case MACHINE_SELECT_CALL_ARG_DESC_LIVE_IN_OPERAND:
            return machine_select_operands_equal(lhs->operand, rhs->operand);
        default:
            return 0;
    }
}

static int machine_select_call_arg_desc_matches_store(const MachineSelectCallArgDesc *desc,
    const MachineSelectOp *op) {
    if (!desc || !op) {
        return 0;
    }
    switch (op->kind) {
        case MACHINE_SELECT_OP_STORE_LOCAL:
        case MACHINE_SELECT_OP_STORE_LOCAL_IMM:
            return desc->kind == MACHINE_SELECT_CALL_ARG_DESC_LOCAL_VALUE &&
                op->as.store.slot.id == desc->slot_id;
        case MACHINE_SELECT_OP_STORE_GLOBAL:
        case MACHINE_SELECT_OP_STORE_GLOBAL_IMM:
            return desc->kind == MACHINE_SELECT_CALL_ARG_DESC_GLOBAL_VALUE &&
                op->as.store.slot.id == desc->slot_id;
        default:
            return 0;
    }
}

static int machine_select_call_reads_globals(const MachineSelectCallEffectSummary *summary,
    size_t function_index) {
    size_t global_index;

    if (!summary || function_index >= summary->function_count) {
        return 0;
    }
    for (global_index = 0u; global_index < summary->global_count; ++global_index) {
        if (summary->global_reads[(function_index * summary->global_count) + global_index] != 0u) {
            return 1;
        }
    }
    return 0;
}

static int machine_select_call_write_conflicts_with_summary(const MachineSelectCallEffectSummary *summary,
    size_t function_index,
    const MachineSelectOp *op) {
    size_t global_index;

    if (!summary || !op || function_index >= summary->function_count) {
        return 1;
    }

    switch (op->kind) {
        case MACHINE_SELECT_OP_STORE_GLOBAL:
        case MACHINE_SELECT_OP_STORE_GLOBAL_IMM:
            if (machine_select_call_reads_globals(summary, function_index)) {
                return 1;
            }
            for (global_index = 0u; global_index < summary->global_count; ++global_index) {
                if (summary->global_reads[(function_index * summary->global_count) + global_index] != 0u &&
                    op->as.store.slot.id == global_index) {
                    return 1;
                }
            }
            return 0;
        default:
            return 0;
    }
}

static int machine_select_cleanup_reuse_internal_pure_calls_in_block(const MachineSelectProgram *program,
    MachineSelectFunction *function,
    MachineSelectBasicBlock *block,
    const MachineSelectCallEffectSummary *summary,
    MachineSelectError *error) {
    size_t op_index;
    int changed = 0;

    if (!program || !function || !block || !summary) {
        return 0;
    }

    for (op_index = 0u; op_index < block->op_count; ++op_index) {
        MachineSelectOp *op = &block->ops[op_index];
        size_t callee_index;
        size_t arg_index;
        MachineSelectCallArgDesc arg_descs[8];
        size_t prev_index;

        if (!machine_select_op_is_call(op) ||
            !op->has_result ||
            op->as.call.arg_count > sizeof(arg_descs) / sizeof(arg_descs[0])) {
            continue;
        }

        callee_index = machine_select_find_function_index_by_name(program, op->as.call.callee_name);
        if (callee_index == (size_t)-1 ||
            !summary->cacheable_pure[callee_index]) {
            continue;
        }

        for (arg_index = 0u; arg_index < op->as.call.arg_count; ++arg_index) {
            if (!machine_select_describe_call_arg_source_in_block(
                    block, op_index, op->as.call.args[arg_index], &arg_descs[arg_index])) {
                break;
            }
        }
        if (arg_index != op->as.call.arg_count) {
            continue;
        }

        for (prev_index = op_index; prev_index > 0u; --prev_index) {
            MachineSelectOp *prev = &block->ops[prev_index - 1u];
            size_t scan_index;
            int invalidated = 0;

            if (!machine_select_op_is_call(prev) ||
                !prev->has_result ||
                prev->as.call.arg_count != op->as.call.arg_count ||
                !prev->as.call.callee_name ||
                strcmp(prev->as.call.callee_name, op->as.call.callee_name) != 0) {
                continue;
            }

            for (arg_index = 0u; arg_index < op->as.call.arg_count; ++arg_index) {
                MachineSelectCallArgDesc prev_desc;

                if (!machine_select_describe_call_arg_source_in_block(
                        block, prev_index - 1u, prev->as.call.args[arg_index], &prev_desc) ||
                    !machine_select_call_arg_descs_equal(&prev_desc, &arg_descs[arg_index])) {
                    invalidated = 1;
                    break;
                }
            }
            if (invalidated) {
                continue;
            }

            for (scan_index = prev_index; scan_index < op_index; ++scan_index) {
                MachineSelectOp *scan = &block->ops[scan_index];

                if (machine_select_op_is_call(scan)) {
                    invalidated = 1;
                    break;
                }
                if (machine_select_call_write_conflicts_with_summary(summary, callee_index, scan)) {
                    invalidated = 1;
                    break;
                }
                for (arg_index = 0u; arg_index < op->as.call.arg_count; ++arg_index) {
                    if (machine_select_call_arg_desc_matches_store(&arg_descs[arg_index], scan)) {
                        invalidated = 1;
                        break;
                    }
                    if (arg_descs[arg_index].kind == MACHINE_SELECT_CALL_ARG_DESC_LIVE_IN_OPERAND &&
                        (machine_select_op_redefines_result(scan, arg_descs[arg_index].operand) ||
                            machine_select_op_clobbers_operand_via_call(
                                program, function, scan, arg_descs[arg_index].operand))) {
                        invalidated = 1;
                        break;
                    }
                }
                if (invalidated) {
                    break;
                }
            }
            if (invalidated) {
                continue;
            }

            if (prev->result.kind != MACHINE_SELECT_OPERAND_SPILL_SLOT) {
                MachineSelectOp cache_copy;
                MachineSelectOperand original_result = prev->result;

                memset(&cache_copy, 0, sizeof(cache_copy));
                prev->result = machine_select_operand_spill_slot(function->spill_slot_count);
                function->spill_slot_count += 1u;
                machine_select_cleanup_op_shape(prev);

                cache_copy.kind = MACHINE_SELECT_OP_COPY;
                cache_copy.has_result = 1;
                cache_copy.result = original_result;
                cache_copy.as.copy_value = prev->result;
                if (!machine_select_insert_op_at(block, prev_index, &cache_copy, error)) {
                    return -1;
                }
                changed = 1;
                if (prev_index <= op_index) {
                    op_index += 1u;
                }
            }

            {
                op = &block->ops[op_index];
                MachineSelectOperand current_result = op->result;

                machine_select_op_free(op);
                memset(op, 0, sizeof(*op));
                op->kind = MACHINE_SELECT_OP_COPY;
                op->has_result = 1;
                op->result = current_result;
                op->as.copy_value = prev->result;
                machine_select_cleanup_op_shape(op);
                changed = 1;
                break;
            }
        }
    }

    return changed;
}

static int machine_select_cleanup_reuse_internal_pure_calls_program(MachineSelectProgram *program,
    MachineSelectError *error) {
    MachineSelectCallEffectSummary summary;
    size_t function_index;
    int changed = 0;

    if (!program) {
        return 0;
    }

    machine_select_call_effect_summary_init(&summary);
    if (!machine_select_call_effect_summary_compute(program, &summary, error)) {
        return 0;
    }

    for (function_index = 0u; function_index < program->function_count; ++function_index) {
        MachineSelectFunction *function = &program->functions[function_index];
        size_t block_index;

        for (block_index = 0u; block_index < function->block_count; ++block_index) {
            int block_changed = machine_select_cleanup_reuse_internal_pure_calls_in_block(
                program, function, &function->blocks[block_index], &summary, error);

            if (block_changed < 0) {
                machine_select_call_effect_summary_free(&summary);
                return 0;
            }
            if (block_changed) {
                changed = 1;
            }
        }
    }

    machine_select_call_effect_summary_free(&summary);
    return changed;
}

static int machine_select_cleanup_reuse_spill_pure_expr_in_block(const MachineSelectProgram *program,
    const MachineSelectFunction *function,
    MachineSelectBasicBlock *block) {
    size_t op_index;
    int changed = 0;

    if (!program || !function || !block) {
        return 0;
    }

    for (op_index = 1u; op_index < block->op_count; ++op_index) {
        MachineSelectOp *op = &block->ops[op_index];
        size_t prev_index;

        if (!op->has_result ||
            op->result.kind != MACHINE_SELECT_OPERAND_REGISTER ||
            !machine_select_op_is_small_reusable_pure_expr(op)) {
            continue;
        }

        for (prev_index = 0u; prev_index < op_index; ++prev_index) {
            MachineSelectOp *prev = &block->ops[prev_index];
            size_t scan_index;
            MachineSelectOperand current_result;
            int killed = 0;

            if (!prev->has_result ||
                prev->result.kind != MACHINE_SELECT_OPERAND_SPILL_SLOT ||
                !machine_select_ops_compute_same_small_reusable_pure_expr_in_block(
                    block, prev_index, block, op_index, 0u)) {
                continue;
            }

            for (scan_index = prev_index + 1u; scan_index < op_index; ++scan_index) {
                if (machine_select_op_redefines_result(&block->ops[scan_index], prev->result) ||
                    machine_select_op_clobbers_operand_via_call(program, function, &block->ops[scan_index], prev->result)) {
                    killed = 1;
                    break;
                }
            }
            if (killed) {
                continue;
            }

            current_result = op->result;
            machine_select_op_free(op);
            memset(op, 0, sizeof(*op));
            op->kind = MACHINE_SELECT_OP_COPY;
            op->has_result = 1;
            op->result = current_result;
            op->as.copy_value = prev->result;
            machine_select_cleanup_op_shape(op);
            changed = 1;
            break;
        }
    }

    return changed;
}

static int machine_select_cleanup_reuse_spill_pure_expr_program(MachineSelectProgram *program) {
    size_t function_index;
    int changed = 0;

    if (!program) {
        return 0;
    }

    for (function_index = 0u; function_index < program->function_count; ++function_index) {
        MachineSelectFunction *function = &program->functions[function_index];
        size_t block_index;

        for (block_index = 0u; block_index < function->block_count; ++block_index) {
            if (machine_select_cleanup_reuse_spill_pure_expr_in_block(
                    program, function, &function->blocks[block_index])) {
                changed = 1;
            }
        }
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
            if (op->as.binary.lhs.kind == MACHINE_SELECT_OPERAND_IMMEDIATE &&
                op->as.binary.rhs.kind != MACHINE_SELECT_OPERAND_IMMEDIATE) {
                switch (op->as.binary.op) {
                    case MACHINE_IR_BINARY_ADD:
                    case MACHINE_IR_BINARY_MUL:
                    case MACHINE_IR_BINARY_BIT_AND:
                    case MACHINE_IR_BINARY_BIT_XOR:
                    case MACHINE_IR_BINARY_BIT_OR: {
                        MachineSelectOperand tmp = op->as.binary.lhs;

                        op->as.binary.lhs = op->as.binary.rhs;
                        op->as.binary.rhs = tmp;
                        break;
                    }
                    default:
                        break;
                }
            }
            if (op->as.binary.rhs.kind == MACHINE_SELECT_OPERAND_IMMEDIATE &&
                op->as.binary.lhs.kind != MACHINE_SELECT_OPERAND_IMMEDIATE) {
                if (op->as.binary.rhs.immediate == 0) {
                    switch (op->as.binary.op) {
                        case MACHINE_IR_BINARY_ADD:
                        case MACHINE_IR_BINARY_SUB:
                        case MACHINE_IR_BINARY_BIT_OR:
                        case MACHINE_IR_BINARY_BIT_XOR:
                        case MACHINE_IR_BINARY_SHIFT_LEFT:
                        case MACHINE_IR_BINARY_SHIFT_RIGHT:
                            op->kind = MACHINE_SELECT_OP_COPY;
                            op->as.copy_value = op->as.binary.lhs;
                            return 1;
                        case MACHINE_IR_BINARY_MUL:
                        case MACHINE_IR_BINARY_BIT_AND:
                            op->kind = MACHINE_SELECT_OP_MATERIALIZE_IMM;
                            op->as.copy_value = machine_select_operand_immediate(0);
                            return 1;
                        default:
                            break;
                    }
                }
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
            if (op->as.binary.lhs.kind == MACHINE_SELECT_OPERAND_IMMEDIATE &&
                op->as.binary.rhs.kind != MACHINE_SELECT_OPERAND_IMMEDIATE) {
                MachineSelectOperand tmp = op->as.binary.lhs;

                op->as.binary.lhs = op->as.binary.rhs;
                op->as.binary.rhs = tmp;
                switch (op->as.binary.op) {
                    case MACHINE_IR_BINARY_LT:
                        op->as.binary.op = MACHINE_IR_BINARY_GT;
                        break;
                    case MACHINE_IR_BINARY_LE:
                        op->as.binary.op = MACHINE_IR_BINARY_GE;
                        break;
                    case MACHINE_IR_BINARY_GT:
                        op->as.binary.op = MACHINE_IR_BINARY_LT;
                        break;
                    case MACHINE_IR_BINARY_GE:
                        op->as.binary.op = MACHINE_IR_BINARY_LE;
                        break;
                    default:
                        break;
                }
            }
            if (op->as.binary.rhs.kind == MACHINE_SELECT_OPERAND_IMMEDIATE &&
                op->as.binary.lhs.kind != MACHINE_SELECT_OPERAND_IMMEDIATE) {
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

static int machine_select_op_is_small_reusable_pure_expr(const MachineSelectOp *op) {
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
            return 1;
        default:
            return 0;
    }
}

static int machine_select_find_defining_op_in_block(const MachineSelectBasicBlock *block,
    size_t use_index,
    MachineSelectOperand operand,
    size_t *out_def_index) {
    size_t scan_index;

    if (out_def_index) {
        *out_def_index = 0u;
    }
    if (!block || use_index > block->op_count ||
        (operand.kind != MACHINE_SELECT_OPERAND_REGISTER &&
            operand.kind != MACHINE_SELECT_OPERAND_SPILL_SLOT)) {
        return 0;
    }

    for (scan_index = use_index; scan_index > 0u; --scan_index) {
        const MachineSelectOp *op = &block->ops[scan_index - 1u];

        if (!machine_select_op_redefines_result(op, operand)) {
            continue;
        }
        if (out_def_index) {
            *out_def_index = scan_index - 1u;
        }
        return 1;
    }
    return 0;
}

static int machine_select_operands_compute_same_small_pure_value_in_block(const MachineSelectBasicBlock *lhs_block,
    size_t lhs_use_index,
    MachineSelectOperand lhs,
    const MachineSelectBasicBlock *rhs_block,
    size_t rhs_use_index,
    MachineSelectOperand rhs,
    unsigned depth);

static int machine_select_ops_compute_same_small_reusable_pure_expr_in_block(
    const MachineSelectBasicBlock *lhs_block,
    size_t lhs_index,
    const MachineSelectBasicBlock *rhs_block,
    size_t rhs_index,
    unsigned depth) {
    const MachineSelectOp *lhs;
    const MachineSelectOp *rhs;

    if (!lhs_block || !rhs_block ||
        lhs_index >= lhs_block->op_count ||
        rhs_index >= rhs_block->op_count ||
        depth > 8u) {
        return 0;
    }

    lhs = &lhs_block->ops[lhs_index];
    rhs = &rhs_block->ops[rhs_index];
    if (!machine_select_op_is_small_reusable_pure_expr(lhs) ||
        !machine_select_op_is_small_reusable_pure_expr(rhs) ||
        lhs->kind != rhs->kind) {
        return 0;
    }

    switch (lhs->kind) {
        case MACHINE_SELECT_OP_COPY:
        case MACHINE_SELECT_OP_MATERIALIZE_IMM:
            return machine_select_operands_compute_same_small_pure_value_in_block(
                lhs_block, lhs_index, lhs->as.copy_value,
                rhs_block, rhs_index, rhs->as.copy_value,
                depth + 1u);
        case MACHINE_SELECT_OP_ALU:
        case MACHINE_SELECT_OP_ALU_IMM:
        case MACHINE_SELECT_OP_CMP:
        case MACHINE_SELECT_OP_CMP_IMM:
            return lhs->as.binary.op == rhs->as.binary.op &&
                machine_select_operands_compute_same_small_pure_value_in_block(
                    lhs_block, lhs_index, lhs->as.binary.lhs,
                    rhs_block, rhs_index, rhs->as.binary.lhs,
                    depth + 1u) &&
                machine_select_operands_compute_same_small_pure_value_in_block(
                    lhs_block, lhs_index, lhs->as.binary.rhs,
                    rhs_block, rhs_index, rhs->as.binary.rhs,
                    depth + 1u);
        default:
            return 0;
    }
}

static int machine_select_operands_compute_same_small_pure_value_in_block(const MachineSelectBasicBlock *lhs_block,
    size_t lhs_use_index,
    MachineSelectOperand lhs,
    const MachineSelectBasicBlock *rhs_block,
    size_t rhs_use_index,
    MachineSelectOperand rhs,
    unsigned depth) {
    size_t lhs_def_index;
    size_t rhs_def_index;

    if (depth > 8u) {
        return 0;
    }
    if (lhs.kind == MACHINE_SELECT_OPERAND_IMMEDIATE ||
        rhs.kind == MACHINE_SELECT_OPERAND_IMMEDIATE) {
        return machine_select_operands_equal(lhs, rhs);
    }

    if ((lhs.kind == MACHINE_SELECT_OPERAND_REGISTER || lhs.kind == MACHINE_SELECT_OPERAND_SPILL_SLOT) &&
        (rhs.kind == MACHINE_SELECT_OPERAND_REGISTER || rhs.kind == MACHINE_SELECT_OPERAND_SPILL_SLOT)) {
        int lhs_has_def = machine_select_find_defining_op_in_block(lhs_block, lhs_use_index, lhs, &lhs_def_index);
        int rhs_has_def = machine_select_find_defining_op_in_block(rhs_block, rhs_use_index, rhs, &rhs_def_index);

        if (lhs_has_def != rhs_has_def) {
            return 0;
        }
        if (lhs_has_def && rhs_has_def) {
            if (lhs_block == rhs_block && lhs_def_index == rhs_def_index) {
                return 1;
            }
            if (machine_select_defs_compute_same_address_root(
                    lhs_block, lhs_def_index, rhs_block, rhs_def_index)) {
                return 1;
            }
            if (machine_select_op_is_small_reusable_pure_expr(&lhs_block->ops[lhs_def_index]) &&
                machine_select_op_is_small_reusable_pure_expr(&rhs_block->ops[rhs_def_index])) {
                return machine_select_ops_compute_same_small_reusable_pure_expr_in_block(
                    lhs_block, lhs_def_index, rhs_block, rhs_def_index, depth + 1u);
            }
            return 0;
        }
    }

    return machine_select_operands_equal(lhs, rhs);
}

static int machine_select_append_op(MachineSelectBasicBlock *block,
    const MachineSelectOp *op,
    MachineSelectError *error) {
    MachineSelectOp *new_ops;
    size_t new_capacity;

    if (!block || !op) {
        return 0;
    }
    if (block->op_count == block->op_capacity) {
        new_capacity = block->op_capacity == 0 ? 4u : block->op_capacity * 2u;
        new_ops = (MachineSelectOp *)realloc(block->ops, new_capacity * sizeof(MachineSelectOp));
        if (!new_ops) {
            machine_select_set_error(error, 0, 0, "MACHINE-SELECT-419: out of memory growing selected op table");
            return 0;
        }
        block->ops = new_ops;
        block->op_capacity = new_capacity;
    }

    block->ops[block->op_count] = *op;
    ++block->op_count;
    return 1;
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
        if (block->terminator.as.compare_branch.lhs.kind == MACHINE_SELECT_OPERAND_IMMEDIATE &&
            block->terminator.as.compare_branch.rhs.kind != MACHINE_SELECT_OPERAND_IMMEDIATE) {
            MachineSelectOperand tmp = block->terminator.as.compare_branch.lhs;

            block->terminator.as.compare_branch.lhs = block->terminator.as.compare_branch.rhs;
            block->terminator.as.compare_branch.rhs = tmp;
            switch (block->terminator.as.compare_branch.op) {
                case MACHINE_IR_BINARY_LT:
                    block->terminator.as.compare_branch.op = MACHINE_IR_BINARY_GT;
                    break;
                case MACHINE_IR_BINARY_LE:
                    block->terminator.as.compare_branch.op = MACHINE_IR_BINARY_GE;
                    break;
                case MACHINE_IR_BINARY_GT:
                    block->terminator.as.compare_branch.op = MACHINE_IR_BINARY_LT;
                    break;
                case MACHINE_IR_BINARY_GE:
                    block->terminator.as.compare_branch.op = MACHINE_IR_BINARY_LE;
                    break;
                default:
                    break;
            }
        }
        if (block->terminator.as.compare_branch.rhs.kind == MACHINE_SELECT_OPERAND_IMMEDIATE &&
            block->terminator.as.compare_branch.lhs.kind != MACHINE_SELECT_OPERAND_IMMEDIATE) {
            block->terminator.kind = MACHINE_SELECT_TERM_COMPARE_BRANCH_IMM;
            return 1;
        }
    }

    return 0;
}

static int machine_select_cleanup_canonicalize_return_abi(MachineSelectBasicBlock *block, MachineSelectError *error) {
    MachineSelectOp move_op;

    if (!block) {
        return 0;
    }
    if (block->terminator.kind != MACHINE_SELECT_TERM_RETURN ||
        block->terminator.as.return_value.kind != MACHINE_SELECT_OPERAND_REGISTER ||
        block->terminator.as.return_value.machine_register_id == 0u) {
        return 0;
    }

    memset(&move_op, 0, sizeof(move_op));
    move_op.kind = MACHINE_SELECT_OP_COPY;
    move_op.has_result = 1;
    move_op.result = machine_select_operand_register(0u);
    move_op.as.copy_value = block->terminator.as.return_value;
    if (!machine_select_append_op(block, &move_op, error)) {
        return -1;
    }
    block->terminator.as.return_value = move_op.result;
    return 1;
}

static int machine_select_cleanup_canonicalize_return_abi_program(MachineSelectProgram *program,
    MachineSelectError *error) {
    size_t function_index;

    if (!program) {
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        MachineSelectFunction *function = &program->functions[function_index];
        size_t block_index;

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            int abi_changed =
                machine_select_cleanup_canonicalize_return_abi(&function->blocks[block_index], error);

            if (abi_changed < 0) {
                return 0;
            }
        }
    }

    return 1;
}

static int machine_select_cleanup_remove_copy_self_in_block(MachineSelectBasicBlock *block) {
    size_t op_index;
    int changed = 0;

    if (!block) {
        return 0;
    }

    for (op_index = 0; op_index < block->op_count; ++op_index) {
        MachineSelectOp *op = &block->ops[op_index];

        if (!op->has_result ||
            op->kind != MACHINE_SELECT_OP_COPY ||
            !machine_select_operand_matches_result(op->result, op->as.copy_value)) {
            continue;
        }

        machine_select_remove_op_at(block, op_index);
        changed = 1;
        op_index -= 1u;
    }

    return changed;
}

static int machine_select_cleanup_fold_addr_copy_pairs_in_block(const MachineSelectProgram *program,
    const MachineSelectFunction *function,
    MachineSelectBasicBlock *block) {
    size_t op_index;
    int changed = 0;

    if (!program || !function || !block) {
        return 0;
    }

    for (op_index = 1u; op_index < block->op_count; ++op_index) {
        MachineSelectOp *copy_op = &block->ops[op_index];
        MachineSelectOp *producer = &block->ops[op_index - 1u];
        int redefined_before_terminator = 0;
        size_t total_uses;

        if (!producer->has_result ||
            (producer->kind != MACHINE_SELECT_OP_ADDR_LOCAL && producer->kind != MACHINE_SELECT_OP_ADDR_GLOBAL) ||
            copy_op->kind != MACHINE_SELECT_OP_COPY ||
            !copy_op->has_result ||
            !machine_select_operand_matches_result(copy_op->as.copy_value, producer->result) ||
            machine_select_operand_matches_result(copy_op->result, producer->result)) {
            continue;
        }

        total_uses = machine_select_count_visible_result_uses_in_block(
            program, function, block, op_index - 1u, producer->result, &redefined_before_terminator);
        if (total_uses != 1u) {
            continue;
        }

        producer->result = copy_op->result;
        machine_select_remove_op_at(block, op_index);
        changed = 1;
        op_index -= 1u;
    }

    return changed;
}

static int machine_select_cleanup_remove_redundant_addr_defs_in_block(const MachineSelectProgram *program,
    const MachineSelectFunction *function,
    MachineSelectBasicBlock *block) {
    size_t op_index;
    int changed = 0;

    if (!program || !function || !block) {
        return 0;
    }

    for (op_index = 0; op_index < block->op_count; ++op_index) {
        MachineSelectOp *op = &block->ops[op_index];
        size_t scan_index;
        int removable = 0;

        if (!op->has_result ||
            (op->kind != MACHINE_SELECT_OP_ADDR_LOCAL && op->kind != MACHINE_SELECT_OP_ADDR_GLOBAL)) {
            continue;
        }

        for (scan_index = op_index; scan_index > 0; --scan_index) {
            MachineSelectOp *prev = &block->ops[scan_index - 1u];

            if (machine_select_op_redefines_result(prev, op->result) ||
                machine_select_op_clobbers_operand_via_call(program, function, prev, op->result)) {
                if (prev->has_result &&
                    prev->kind == op->kind &&
                    machine_select_operand_matches_result(prev->result, op->result) &&
                    prev->as.addr_slot.kind == op->as.addr_slot.kind &&
                    prev->as.addr_slot.id == op->as.addr_slot.id) {
                    removable = 1;
                }
                break;
            }
        }

        if (!removable) {
            continue;
        }

        machine_select_remove_op_at(block, op_index);
        changed = 1;
        op_index -= 1u;
    }

    return changed;
}

static int machine_select_op_is_addr_slot_kind(MachineSelectOpKind kind) {
    return kind == MACHINE_SELECT_OP_ADDR_LOCAL || kind == MACHINE_SELECT_OP_ADDR_GLOBAL;
}

static int machine_select_cleanup_reuse_addr_roots_in_block(const MachineSelectProgram *program,
    const MachineSelectFunction *function,
    MachineSelectBasicBlock *block) {
    size_t op_index;
    int changed = 0;

    if (!program || !function || !block) {
        return 0;
    }

    for (op_index = 0; op_index < block->op_count; ++op_index) {
        MachineSelectOp *op = &block->ops[op_index];
        size_t candidate_index;

        if (!op->has_result || !machine_select_op_is_addr_slot_kind(op->kind)) {
            continue;
        }

        for (candidate_index = 0; candidate_index < op_index; ++candidate_index) {
            MachineSelectOp *candidate = &block->ops[candidate_index];
            int killed = 0;
            size_t scan_index;

            if (!candidate->has_result ||
                candidate->kind != op->kind ||
                candidate->as.addr_slot.kind != op->as.addr_slot.kind ||
                candidate->as.addr_slot.id != op->as.addr_slot.id) {
                continue;
            }

            for (scan_index = candidate_index + 1u; scan_index < block->op_count; ++scan_index) {
                if (machine_select_op_redefines_result(&block->ops[scan_index], candidate->result) ||
                    machine_select_op_clobbers_operand_via_call(program, function, &block->ops[scan_index], candidate->result)) {
                    killed = 1;
                    break;
                }
            }
            if (killed) {
                continue;
            }

            for (scan_index = candidate_index + 1u; scan_index < op_index; ++scan_index) {
                if (machine_select_op_redefines_result(&block->ops[scan_index], candidate->result) ||
                    machine_select_op_clobbers_operand_via_call(program, function, &block->ops[scan_index], candidate->result)) {
                    killed = 1;
                    break;
                }
            }
            if (killed) {
                continue;
            }

            if (!machine_select_rewrite_visible_uses_in_block(
                    program,
                    function,
                    block,
                    op_index,
                    op->result,
                    candidate->result)) {
                break;
            }
            machine_select_remove_op_at(block, op_index);
            changed = 1;
            op_index -= 1u;
            break;
        }
    }

    return changed;
}

static int machine_select_cleanup_reuse_addr_roots_program(MachineSelectProgram *program) {
    size_t function_index;
    int changed = 0;

    if (!program) {
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        MachineSelectFunction *function = &program->functions[function_index];
        size_t block_index;

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            if (machine_select_cleanup_reuse_addr_roots_in_block(program, function, &function->blocks[block_index])) {
                changed = 1;
            }
        }
    }

    return changed;
}

static int machine_select_cleanup_pure_program(MachineSelectProgram *program, MachineSelectError *error) {
    size_t function_index;
    int changed = 0;
    int trace_timing = machine_select_cleanup_trace_enabled();

    if (!program) {
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        MachineSelectFunction *function = &program->functions[function_index];
        int function_changed = 1;
        size_t iteration = 0u;

        while (function_changed) {
            size_t resource_count = program->register_bank.register_count + function->spill_slot_count;
            unsigned char *live_out_sets = NULL;
            size_t block_index;
            double iter_start = trace_timing ? machine_select_cleanup_now_s() : 0.0;
            double stage_start = iter_start;
            double t_live_out = 0.0;
            double t_canonicalize = 0.0;
            double t_copy_self = 0.0;
            double t_addr_copy = 0.0;
            double t_redundant_addr = 0.0;
            double t_forward = 0.0;
            double t_dead = 0.0;
            double t_term = 0.0;

            function_changed = 0;
            ++iteration;

            if (!machine_select_cleanup_compute_block_live_out(
                    program, function, resource_count, &live_out_sets, error)) {
                return 0;
            }
            if (trace_timing) {
                t_live_out = machine_select_cleanup_now_s() - stage_start;
            }

            for (block_index = 0; block_index < function->block_count; ++block_index) {
                MachineSelectBasicBlock *block = &function->blocks[block_index];

                if (trace_timing) {
                    stage_start = machine_select_cleanup_now_s();
                }
                if (machine_select_cleanup_canonicalize_ops(block)) {
                    function_changed = 1;
                }
                if (trace_timing) {
                    t_canonicalize += machine_select_cleanup_now_s() - stage_start;
                    stage_start = machine_select_cleanup_now_s();
                }
                if (machine_select_cleanup_remove_copy_self_in_block(block)) {
                    function_changed = 1;
                }
                if (trace_timing) {
                    t_copy_self += machine_select_cleanup_now_s() - stage_start;
                    stage_start = machine_select_cleanup_now_s();
                }
                if (machine_select_cleanup_fold_addr_copy_pairs_in_block(program, function, block)) {
                    function_changed = 1;
                }
                if (trace_timing) {
                    t_addr_copy += machine_select_cleanup_now_s() - stage_start;
                    stage_start = machine_select_cleanup_now_s();
                }
                if (machine_select_cleanup_remove_redundant_addr_defs_in_block(program, function, block)) {
                    function_changed = 1;
                }
                if (trace_timing) {
                    t_redundant_addr += machine_select_cleanup_now_s() - stage_start;
                    stage_start = machine_select_cleanup_now_s();
                }
                if (machine_select_cleanup_forward_trivial_defs_in_block(
                        program, function, block_index, resource_count, live_out_sets)) {
                    function_changed = 1;
                }
                if (trace_timing) {
                    t_forward += machine_select_cleanup_now_s() - stage_start;
                    stage_start = machine_select_cleanup_now_s();
                }
                if (machine_select_cleanup_remove_dead_defs_in_block(
                        function, program, block_index, resource_count, live_out_sets)) {
                    function_changed = 1;
                }
                if (trace_timing) {
                    t_dead += machine_select_cleanup_now_s() - stage_start;
                    stage_start = machine_select_cleanup_now_s();
                }
                if (machine_select_cleanup_terminator_shape(block)) {
                    function_changed = 1;
                }
                if (trace_timing) {
                    t_term += machine_select_cleanup_now_s() - stage_start;
                }
            }

            free(live_out_sets);
            if (trace_timing) {
                char label[128];

                snprintf(label,
                    sizeof(label),
                    "pure fn=%zu iter=%zu live_out",
                    function_index,
                    iteration);
                machine_select_cleanup_trace_timing(label, t_live_out);
                snprintf(label,
                    sizeof(label),
                    "pure fn=%zu iter=%zu canonicalize",
                    function_index,
                    iteration);
                machine_select_cleanup_trace_timing(label, t_canonicalize);
                snprintf(label,
                    sizeof(label),
                    "pure fn=%zu iter=%zu copy_self",
                    function_index,
                    iteration);
                machine_select_cleanup_trace_timing(label, t_copy_self);
                snprintf(label,
                    sizeof(label),
                    "pure fn=%zu iter=%zu addr_copy",
                    function_index,
                    iteration);
                machine_select_cleanup_trace_timing(label, t_addr_copy);
                snprintf(label,
                    sizeof(label),
                    "pure fn=%zu iter=%zu redundant_addr",
                    function_index,
                    iteration);
                machine_select_cleanup_trace_timing(label, t_redundant_addr);
                snprintf(label,
                    sizeof(label),
                    "pure fn=%zu iter=%zu forward",
                    function_index,
                    iteration);
                machine_select_cleanup_trace_timing(label, t_forward);
                snprintf(label,
                    sizeof(label),
                    "pure fn=%zu iter=%zu dead",
                    function_index,
                    iteration);
                machine_select_cleanup_trace_timing(label, t_dead);
                snprintf(label,
                    sizeof(label),
                    "pure fn=%zu iter=%zu terminator",
                    function_index,
                    iteration);
                machine_select_cleanup_trace_timing(label, t_term);
                snprintf(label,
                    sizeof(label),
                    "pure fn=%zu iter=%zu total",
                    function_index,
                    iteration);
                machine_select_cleanup_trace_timing(label, machine_select_cleanup_now_s() - iter_start);
            }
            if (function_changed) {
                changed = 1;
            }
        }
    }
    return changed;
}

static int machine_select_cleanup_forward_trivial_defs_in_block(const MachineSelectProgram *program,
    MachineSelectFunction *function,
    size_t block_index,
    size_t resource_count,
    const unsigned char *live_out_sets) {
    MachineSelectBasicBlock *block;
    int changed = 0;
    int local_changed = 1;
    size_t *predecessor_counts = NULL;

    if (!function || block_index >= function->block_count) {
        return 0;
    }
    block = &function->blocks[block_index];

    while (local_changed) {
        size_t op_index;

        local_changed = 0;
        for (op_index = block->op_count; op_index > 0; --op_index) {
            MachineSelectOp *op = &block->ops[op_index - 1];
            MachineSelectOperand replacement;
            size_t total_uses;
            int redefined_before_terminator = 0;
            int preserve_edge_copy = machine_select_cleanup_op_is_preserved_edge_copy(
                function,
                block,
                op_index - 1u,
                &predecessor_counts);

            if (!op->has_result ||
                (op->kind != MACHINE_SELECT_OP_COPY && op->kind != MACHINE_SELECT_OP_MATERIALIZE_IMM)) {
                continue;
            }
            if (preserve_edge_copy) {
                continue;
            }

            total_uses = machine_select_count_visible_result_uses_in_block(
                program, function, block, op_index - 1, op->result, &redefined_before_terminator);
            if (total_uses == 0) {
                continue;
            }
            if (!redefined_before_terminator &&
                machine_select_terminator_has_successor(&block->terminator) &&
                machine_select_cleanup_operand_is_live_out(
                    program, function, block_index, resource_count, live_out_sets, op->result)) {
                continue;
            }

            replacement = op->as.copy_value;
            if (replacement.kind != MACHINE_SELECT_OPERAND_IMMEDIATE &&
                machine_select_operand_redefined_before_result_uses_end_in_block(
                    program, function, block, op_index - 1, op->result, replacement)) {
                continue;
            }
            machine_select_rewrite_visible_uses_in_block(
                program, function, block, op_index - 1, op->result, replacement);
            machine_select_remove_op_at(block, op_index - 1);
            changed = 1;
            local_changed = 1;
            machine_select_cleanup_terminator_shape(block);
            break;
        }
    }

    free(predecessor_counts);
    return changed;
}

static int machine_select_cleanup_op_is_preserved_edge_copy(const MachineSelectFunction *function,
    const MachineSelectBasicBlock *block,
    size_t op_index,
    size_t **predecessor_counts_ptr) {
    size_t *predecessor_counts = predecessor_counts_ptr ? *predecessor_counts_ptr : NULL;
    size_t successor_a = (size_t)-1;
    size_t successor_b = (size_t)-1;
    size_t trailing_index;

    if (!function || !block || op_index >= block->op_count) {
        return 0;
    }
    if (block->ops[op_index].kind != MACHINE_SELECT_OP_COPY) {
        return 0;
    }

    if (!predecessor_counts) {
        if (!machine_select_cleanup_compute_predecessor_counts(function, &predecessor_counts, NULL)) {
            return 0;
        }
        if (predecessor_counts_ptr) {
            *predecessor_counts_ptr = predecessor_counts;
        }
    }

    switch (block->terminator.kind) {
        case MACHINE_SELECT_TERM_JUMP:
            successor_a = block->terminator.as.jump_target;
            break;
        case MACHINE_SELECT_TERM_BRANCH:
            successor_a = block->terminator.as.branch.then_target;
            successor_b = block->terminator.as.branch.else_target;
            break;
        case MACHINE_SELECT_TERM_COMPARE_BRANCH:
        case MACHINE_SELECT_TERM_COMPARE_BRANCH_IMM:
            successor_a = block->terminator.as.compare_branch.then_target;
            successor_b = block->terminator.as.compare_branch.else_target;
            break;
        default:
            return 0;
    }

    if ((successor_a >= function->block_count || predecessor_counts[successor_a] <= 1u) &&
        (successor_b >= function->block_count || predecessor_counts[successor_b] <= 1u)) {
        return 0;
    }
    for (trailing_index = op_index + 1u; trailing_index < block->op_count; ++trailing_index) {
        if (block->ops[trailing_index].kind != MACHINE_SELECT_OP_COPY) {
            return 0;
        }
    }
    return 1;
}

static int machine_select_cleanup_remove_dead_defs_in_block(MachineSelectFunction *function,
    const MachineSelectProgram *program,
    size_t block_index,
    size_t resource_count,
    const unsigned char *live_out_sets) {
    MachineSelectBasicBlock *block;
    int changed = 0;
    int local_changed = 1;
    size_t *predecessor_counts = NULL;

    if (!function || block_index >= function->block_count) {
        return 0;
    }
    block = &function->blocks[block_index];

    while (local_changed) {
        size_t op_index;

        local_changed = 0;
        for (op_index = block->op_count; op_index > 0; --op_index) {
            MachineSelectOp *op = &block->ops[op_index - 1];
            size_t total_uses;
            int redefined_before_terminator = 0;
            int preserve_edge_copy = machine_select_cleanup_op_is_preserved_edge_copy(
                function,
                block,
                op_index - 1u,
                &predecessor_counts);

            if (!machine_select_op_is_pure_value_op(op)) {
                continue;
            }

            total_uses = machine_select_count_visible_result_uses_in_block(
                program, function, block, op_index - 1, op->result, &redefined_before_terminator);
            if (total_uses != 0) {
                continue;
            }
            if (preserve_edge_copy) {
                continue;
            }
            if (!redefined_before_terminator &&
                machine_select_terminator_has_successor(&block->terminator) &&
                machine_select_cleanup_operand_is_live_out(
                    program, function, block_index, resource_count, live_out_sets, op->result)) {
                continue;
            }

            machine_select_remove_op_at(block, op_index - 1);
            changed = 1;
            local_changed = 1;
            break;
        }
    }

    free(predecessor_counts);
    return changed;
}

static int machine_select_cleanup_program(MachineSelectProgram *program, MachineSelectError *error) {
    size_t function_index;

    if (!program) {
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        MachineSelectFunction *function = &program->functions[function_index];
        int function_changed = 1;

        while (function_changed) {
            MachineSelectCleanupCfg cfg;
            size_t resource_count = program->register_bank.register_count + function->spill_slot_count;
            unsigned char *live_out_sets = NULL;
            size_t block_index;

            function_changed = 0;

            if (!machine_select_cleanup_cfg_init(function, &cfg)) {
                machine_select_set_error(error, 0, 0, "MACHINE-SELECT-418: out of memory building cleanup CFG");
                return 0;
            }

            if (!machine_select_cleanup_propagate_aliases_in_function(program, function, &function_changed, error)) {
                machine_select_cleanup_cfg_free(&cfg);
                return 0;
            }

            if (!machine_select_cleanup_compute_block_live_out(
                    program, function, resource_count, &live_out_sets, error)) {
                machine_select_cleanup_cfg_free(&cfg);
                return 0;
            }

            for (block_index = 0; block_index < function->block_count; ++block_index) {
                MachineSelectBasicBlock *block = &function->blocks[block_index];

                if (machine_select_cleanup_canonicalize_ops(block)) {
                    function_changed = 1;
                }
                if (machine_select_cleanup_reuse_addr_roots_in_block(program, function, block)) {
                    function_changed = 1;
                }
                if (machine_select_cleanup_forward_trivial_defs_in_block(
                        program, function, block_index, resource_count, live_out_sets)) {
                    function_changed = 1;
                }
                if (machine_select_cleanup_remove_dead_defs_in_block(
                        function, program, block_index, resource_count, live_out_sets)) {
                    function_changed = 1;
                }
                if (machine_select_cleanup_terminator_shape(block)) {
                    function_changed = 1;
                }
            }

            free(live_out_sets);
            machine_select_cleanup_cfg_free(&cfg);
        }
    }
    return machine_select_cleanup_canonicalize_return_abi_program(program, error);
}

#endif
