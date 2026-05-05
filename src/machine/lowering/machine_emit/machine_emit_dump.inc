#if defined(__INTELLISENSE__) && !defined(MACHINE_EMIT_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_EMIT_SPLIT_AGGREGATOR)
#define MACHINE_EMIT_SPLIT_INTELLISENSE_CONTEXT
#include "machine_emit.c"
#else

static int machine_emit_dump_operand(MachineEmitStringBuilder *builder, MachineEmitOperand operand) {
    switch (operand.kind) {
        case MACHINE_SELECT_OPERAND_IMMEDIATE:
            return machine_emit_append_format(builder, "%lld", operand.immediate);
        case MACHINE_SELECT_OPERAND_REGISTER:
            return machine_emit_append_format(builder, "reg.%zu", operand.machine_register_id);
        case MACHINE_SELECT_OPERAND_SPILL_SLOT:
            return machine_emit_append_format(builder, "spill.%zu", operand.spill_slot);
        default:
            return machine_emit_append_format(builder, "<none>");
    }
}

static const char *machine_emit_label_for_target(const MachineEmitFunction *function, size_t target_index) {
    if (!function || target_index >= function->block_count || !function->blocks) {
        return NULL;
    }
    return function->blocks[target_index].label_name;
}

int machine_emit_dump_program(const MachineEmitProgram *program,
    char **out_text,
    MachineEmitError *error) {
    MachineEmitStringBuilder builder;
    size_t function_index;

    if (out_text) {
        *out_text = NULL;
    }
    if (!program || !out_text) {
        machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-200: invalid dump contract");
        return 0;
    }

    memset(&builder, 0, sizeof(builder));
    if (!machine_emit_append_format(&builder, "machine_emit\n")) {
        free(builder.data);
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const MachineEmitFunction *function = &program->functions[function_index];
        size_t block_index;

        if (!machine_emit_append_format(
                &builder,
                "function %s params=%zu locals=%zu spills=%zu\n",
                function->name ? function->name : "<unnamed>",
                function->parameter_count,
                function->local_count,
                function->spill_slot_count)) {
            free(builder.data);
            return 0;
        }

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            const MachineEmitBlock *block = &function->blocks[block_index];
            size_t op_index;

            if (!machine_emit_append_format(
                    &builder,
                    "  %s: ; emit.%zu -> layout.%zu -> bb.%zu\n",
                    block->label_name ? block->label_name : "<null>",
                    block->emit_index,
                    block->original_layout_index,
                    block->original_block_id)) {
                free(builder.data);
                return 0;
            }

            for (op_index = 0; op_index < block->op_count; ++op_index) {
                const MachineEmitOp *op = &block->ops[op_index];

                if (!machine_emit_append_format(&builder, "    ")) {
                    free(builder.data);
                    return 0;
                }
                if (op->has_result) {
                    if (!machine_emit_dump_operand(&builder, op->result) ||
                        !machine_emit_append_format(&builder, " = ")) {
                        free(builder.data);
                        return 0;
                    }
                }

                switch (op->kind) {
                    case MACHINE_SELECT_OP_COPY:
                        if (!machine_emit_append_format(&builder, "copy ") ||
                            !machine_emit_dump_operand(&builder, op->as.copy_value)) {
                            free(builder.data);
                            return 0;
                        }
                        break;
                    case MACHINE_SELECT_OP_MATERIALIZE_IMM:
                        if (!machine_emit_append_format(&builder, "imm ") ||
                            !machine_emit_dump_operand(&builder, op->as.copy_value)) {
                            free(builder.data);
                            return 0;
                        }
                        break;
                    case MACHINE_SELECT_OP_ALU:
                        if (!machine_emit_append_format(&builder, "alu.%d ", (int)op->as.binary.op) ||
                            !machine_emit_dump_operand(&builder, op->as.binary.lhs) ||
                            !machine_emit_append_format(&builder, ", ") ||
                            !machine_emit_dump_operand(&builder, op->as.binary.rhs)) {
                            free(builder.data);
                            return 0;
                        }
                        break;
                    case MACHINE_SELECT_OP_ALU_IMM:
                        if (!machine_emit_append_format(&builder, "alui.%d ", (int)op->as.binary.op) ||
                            !machine_emit_dump_operand(&builder, op->as.binary.lhs) ||
                            !machine_emit_append_format(&builder, ", ") ||
                            !machine_emit_dump_operand(&builder, op->as.binary.rhs)) {
                            free(builder.data);
                            return 0;
                        }
                        break;
                    case MACHINE_SELECT_OP_CMP:
                        if (!machine_emit_append_format(&builder, "cmp.%d ", (int)op->as.binary.op) ||
                            !machine_emit_dump_operand(&builder, op->as.binary.lhs) ||
                            !machine_emit_append_format(&builder, ", ") ||
                            !machine_emit_dump_operand(&builder, op->as.binary.rhs)) {
                            free(builder.data);
                            return 0;
                        }
                        break;
                    case MACHINE_SELECT_OP_CMP_IMM:
                        if (!machine_emit_append_format(&builder, "cmpi.%d ", (int)op->as.binary.op) ||
                            !machine_emit_dump_operand(&builder, op->as.binary.lhs) ||
                            !machine_emit_append_format(&builder, ", ") ||
                            !machine_emit_dump_operand(&builder, op->as.binary.rhs)) {
                            free(builder.data);
                            return 0;
                        }
                        break;
                    case MACHINE_SELECT_OP_CALL:
                    case MACHINE_SELECT_OP_CALL_IMM:
                    case MACHINE_SELECT_OP_CALL_SPILL:
                    case MACHINE_SELECT_OP_CALL_IMM_SPILL:
                    case MACHINE_SELECT_OP_CALL_VOID:
                    case MACHINE_SELECT_OP_CALL_VOID_IMM: {
                        const char *opcode_name = "<bad-call>";
                        size_t arg_index;

                        switch (op->kind) {
                            case MACHINE_SELECT_OP_CALL:
                                opcode_name = "call";
                                break;
                            case MACHINE_SELECT_OP_CALL_IMM:
                                opcode_name = "calli";
                                break;
                            case MACHINE_SELECT_OP_CALL_SPILL:
                                opcode_name = "call_spill";
                                break;
                            case MACHINE_SELECT_OP_CALL_IMM_SPILL:
                                opcode_name = "calli_spill";
                                break;
                            case MACHINE_SELECT_OP_CALL_VOID:
                                opcode_name = "call_void";
                                break;
                            case MACHINE_SELECT_OP_CALL_VOID_IMM:
                                opcode_name = "call_voidi";
                                break;
                            default:
                                break;
                        }

                        if (!machine_emit_append_format(
                                &builder,
                                "%s %s(",
                                opcode_name,
                                op->as.call.callee_name ? op->as.call.callee_name : "<null>")) {
                            free(builder.data);
                            return 0;
                        }
                        for (arg_index = 0; arg_index < op->as.call.arg_count; ++arg_index) {
                            if (arg_index > 0 && !machine_emit_append_format(&builder, ", ")) {
                                free(builder.data);
                                return 0;
                            }
                            if (!machine_emit_dump_operand(&builder, op->as.call.args[arg_index])) {
                                free(builder.data);
                                return 0;
                            }
                        }
                        if (!machine_emit_append_format(&builder, ")")) {
                            free(builder.data);
                            return 0;
                        }
                        break;
                    }
                    case MACHINE_SELECT_OP_ADDR_LOCAL:
                        if (!machine_emit_append_format(&builder, "addr_local local.%zu", op->as.addr_slot.id)) {
                            free(builder.data);
                            return 0;
                        }
                        break;
                    case MACHINE_SELECT_OP_ADDR_GLOBAL:
                        if (!machine_emit_append_format(&builder, "addr_global global.%zu", op->as.addr_slot.id)) {
                            free(builder.data);
                            return 0;
                        }
                        break;
                    case MACHINE_SELECT_OP_LOAD_LOCAL:
                        if (!machine_emit_append_format(&builder, "load_local local.%zu", op->as.load_slot.id)) {
                            free(builder.data);
                            return 0;
                        }
                        break;
                    case MACHINE_SELECT_OP_STORE_LOCAL:
                        if (!machine_emit_append_format(&builder, "store_local local.%zu, ", op->as.store.slot.id) ||
                            !machine_emit_dump_operand(&builder, op->as.store.value)) {
                            free(builder.data);
                            return 0;
                        }
                        break;
                    case MACHINE_SELECT_OP_STORE_LOCAL_IMM:
                        if (!machine_emit_append_format(&builder, "store_locali local.%zu, ", op->as.store.slot.id) ||
                            !machine_emit_dump_operand(&builder, op->as.store.value)) {
                            free(builder.data);
                            return 0;
                        }
                        break;
                    case MACHINE_SELECT_OP_LOAD_GLOBAL:
                        if (!machine_emit_append_format(&builder, "load_global global.%zu", op->as.load_slot.id)) {
                            free(builder.data);
                            return 0;
                        }
                        break;
                    case MACHINE_SELECT_OP_STORE_GLOBAL:
                        if (!machine_emit_append_format(&builder, "store_global global.%zu, ", op->as.store.slot.id) ||
                            !machine_emit_dump_operand(&builder, op->as.store.value)) {
                            free(builder.data);
                            return 0;
                        }
                        break;
                    case MACHINE_SELECT_OP_STORE_GLOBAL_IMM:
                        if (!machine_emit_append_format(&builder, "store_globali global.%zu, ", op->as.store.slot.id) ||
                            !machine_emit_dump_operand(&builder, op->as.store.value)) {
                            free(builder.data);
                            return 0;
                        }
                        break;
                    case MACHINE_SELECT_OP_LOAD_INDIRECT:
                        if (!machine_emit_append_format(&builder, "load_indirect ") ||
                            !machine_emit_dump_operand(&builder, op->as.load_indirect_addr)) {
                            free(builder.data);
                            return 0;
                        }
                        break;
                    case MACHINE_SELECT_OP_STORE_INDIRECT:
                        if (!machine_emit_append_format(&builder, "store_indirect ") ||
                            !machine_emit_dump_operand(&builder, op->as.store_indirect.addr) ||
                            !machine_emit_append_format(&builder, ", ") ||
                            !machine_emit_dump_operand(&builder, op->as.store_indirect.value)) {
                            free(builder.data);
                            return 0;
                        }
                        break;
                    default:
                        free(builder.data);
                        machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-201: unknown op kind during dump");
                        return 0;
                }

                if (!machine_emit_append_format(&builder, "\n")) {
                    free(builder.data);
                    return 0;
                }
            }

            if (!machine_emit_append_format(&builder, "    ")) {
                free(builder.data);
                return 0;
            }
            switch (block->terminator.kind) {
                case MACHINE_LAYOUT_TERM_RETURN:
                    if (!machine_emit_append_format(&builder, "ret")) {
                        free(builder.data);
                        return 0;
                    }
                    if (block->terminator.as.return_value.kind != MACHINE_SELECT_OPERAND_NONE &&
                        (!machine_emit_append_format(&builder, " ") ||
                            !machine_emit_dump_operand(&builder, block->terminator.as.return_value))) {
                        free(builder.data);
                        return 0;
                    }
                    break;
                case MACHINE_LAYOUT_TERM_RETURN_IMM:
                    if (!machine_emit_append_format(&builder, "reti ") ||
                        !machine_emit_dump_operand(&builder, block->terminator.as.return_value)) {
                        free(builder.data);
                        return 0;
                    }
                    break;
                case MACHINE_LAYOUT_TERM_RETURN_SPILL:
                    if (!machine_emit_append_format(&builder, "retspill ") ||
                        !machine_emit_dump_operand(&builder, block->terminator.as.return_value)) {
                        free(builder.data);
                        return 0;
                    }
                    break;
                case MACHINE_LAYOUT_TERM_FALLTHROUGH: {
                    const char *label = machine_emit_label_for_target(function, block->terminator.as.fallthrough_target);

                    if (!label ||
                        !machine_emit_append_format(&builder, "fallthrough %s", label)) {
                        free(builder.data);
                        machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-202: invalid fallthrough target during dump");
                        return 0;
                    }
                    break;
                }
                case MACHINE_LAYOUT_TERM_JUMP: {
                    const char *label = machine_emit_label_for_target(function, block->terminator.as.jump_target);

                    if (!label ||
                        !machine_emit_append_format(&builder, "jmp %s", label)) {
                        free(builder.data);
                        machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-203: invalid jump target during dump");
                        return 0;
                    }
                    break;
                }
                case MACHINE_LAYOUT_TERM_BRANCH: {
                    const char *then_label = machine_emit_label_for_target(function, block->terminator.as.branch.then_target);
                    const char *else_label = machine_emit_label_for_target(function, block->terminator.as.branch.else_target);

                    if (!then_label || !else_label ||
                        !machine_emit_append_format(&builder, "br ") ||
                        !machine_emit_dump_operand(&builder, block->terminator.as.branch.condition) ||
                        !machine_emit_append_format(&builder, ", %s, %s", then_label, else_label)) {
                        free(builder.data);
                        machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-204: invalid branch target during dump");
                        return 0;
                    }
                    break;
                }
                case MACHINE_LAYOUT_TERM_BRANCH_FALLTHROUGH: {
                    const char *taken_label =
                        machine_emit_label_for_target(function, block->terminator.as.branch_fallthrough.taken_target);
                    const char *fallthrough_label = machine_emit_label_for_target(
                        function, block->terminator.as.branch_fallthrough.fallthrough_target);

                    if (!taken_label || !fallthrough_label ||
                        !machine_emit_append_format(
                            &builder,
                            "brft.%s ",
                            block->terminator.as.branch_fallthrough.branch_on_true ? "t" : "f") ||
                        !machine_emit_dump_operand(&builder, block->terminator.as.branch_fallthrough.condition) ||
                        !machine_emit_append_format(
                            &builder,
                            ", taken=%s, fallthrough=%s",
                            taken_label,
                            fallthrough_label)) {
                        free(builder.data);
                        machine_emit_set_error(
                            error, 0, 0, "MACHINE-EMIT-205: invalid branch-fallthrough target during dump");
                        return 0;
                    }
                    break;
                }
                case MACHINE_LAYOUT_TERM_COMPARE_BRANCH:
                case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM: {
                    const char *then_label =
                        machine_emit_label_for_target(function, block->terminator.as.compare_branch.then_target);
                    const char *else_label =
                        machine_emit_label_for_target(function, block->terminator.as.compare_branch.else_target);

                    if (!then_label || !else_label ||
                        !machine_emit_append_format(
                            &builder,
                            "%s.%d ",
                            block->terminator.kind == MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM ? "cmpbri" : "cmpbr",
                            (int)block->terminator.as.compare_branch.op) ||
                        !machine_emit_dump_operand(&builder, block->terminator.as.compare_branch.lhs) ||
                        !machine_emit_append_format(&builder, ", ") ||
                        !machine_emit_dump_operand(&builder, block->terminator.as.compare_branch.rhs) ||
                        !machine_emit_append_format(&builder, ", %s, %s", then_label, else_label)) {
                        free(builder.data);
                        machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-206: invalid compare-branch target during dump");
                        return 0;
                    }
                    break;
                }
                case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_FALLTHROUGH:
                case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM_FALLTHROUGH: {
                    const char *taken_label = machine_emit_label_for_target(
                        function, block->terminator.as.compare_branch_fallthrough.taken_target);
                    const char *fallthrough_label = machine_emit_label_for_target(
                        function, block->terminator.as.compare_branch_fallthrough.fallthrough_target);

                    if (!taken_label || !fallthrough_label ||
                        !machine_emit_append_format(
                            &builder,
                            "%sft.%s.%d ",
                            block->terminator.kind == MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM_FALLTHROUGH
                                ? "cmpbri"
                                : "cmpbr",
                            block->terminator.as.compare_branch_fallthrough.branch_on_true ? "t" : "f",
                            (int)block->terminator.as.compare_branch_fallthrough.op) ||
                        !machine_emit_dump_operand(&builder, block->terminator.as.compare_branch_fallthrough.lhs) ||
                        !machine_emit_append_format(&builder, ", ") ||
                        !machine_emit_dump_operand(&builder, block->terminator.as.compare_branch_fallthrough.rhs) ||
                        !machine_emit_append_format(
                            &builder,
                            ", taken=%s, fallthrough=%s",
                            taken_label,
                            fallthrough_label)) {
                        free(builder.data);
                        machine_emit_set_error(
                            error, 0, 0, "MACHINE-EMIT-207: invalid compare-branch-fallthrough target during dump");
                        return 0;
                    }
                    break;
                }
                default:
                    free(builder.data);
                    machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-208: unknown dump terminator");
                    return 0;
            }
            if (!machine_emit_append_format(&builder, "\n")) {
                free(builder.data);
                return 0;
            }
        }
    }

    *out_text = builder.data;
    return 1;
}

int machine_emit_dump_lower_report(const MachineEmitLowerReport *report,
    char **out_text,
    MachineEmitError *error) {
    MachineEmitStringBuilder builder;
    char *program_text = NULL;
    size_t index;
    int ok = 0;

    if (out_text) {
        *out_text = NULL;
    }
    if (!report || !out_text) {
        machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-209: invalid report dump contract");
        return 0;
    }

    memset(&builder, 0, sizeof(builder));
    if (!machine_emit_append_format(
            &builder,
            "machine_emit-report call_funcs=%zu fallthrough_funcs=%zu branch_funcs=%zu total_block_summaries=%zu\n",
            report->functions_with_calls_count,
            report->functions_with_fallthrough_count,
            report->functions_with_branches_count,
            report->total_block_summary_count)) {
        machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-210: out of memory while dumping emit report header");
        goto cleanup;
    }
    if (!machine_emit_append_format(
            &builder,
            "target_policy preview_reg_cap=%zu preserves_spills=%d preserves_global_slots=%d preserves_fallthrough=%d\n",
            report->target_policy_summary.select_policy.current_riscv32_preview_logical_register_cap,
            report->target_policy_summary.preserves_spill_operands_for_later_materialization,
            report->target_policy_summary.preserves_global_slot_ops_for_later_address_formation,
            report->target_policy_summary.preserves_fallthrough_terminator_shapes)) {
        machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-210: out of memory while dumping emit report header");
        goto cleanup;
    }

    if (!machine_emit_append_format(&builder, "functions-with-calls:")) {
        machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-211: out of memory while dumping emit report");
        goto cleanup;
    }
    for (index = 0; index < report->functions_with_calls_count; ++index) {
        if (!machine_emit_append_format(&builder, " %zu", report->function_indices_with_calls[index])) {
            machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-211: out of memory while dumping emit report");
            goto cleanup;
        }
    }
    if (!machine_emit_append_format(&builder, "\nfunctions-with-fallthrough:")) {
        machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-211: out of memory while dumping emit report");
        goto cleanup;
    }
    for (index = 0; index < report->functions_with_fallthrough_count; ++index) {
        if (!machine_emit_append_format(&builder, " %zu", report->function_indices_with_fallthrough[index])) {
            machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-211: out of memory while dumping emit report");
            goto cleanup;
        }
    }
    if (!machine_emit_append_format(&builder, "\nfunctions-with-branches:")) {
        machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-211: out of memory while dumping emit report");
        goto cleanup;
    }
    for (index = 0; index < report->functions_with_branches_count; ++index) {
        if (!machine_emit_append_format(&builder, " %zu", report->function_indices_with_branches[index])) {
            machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-211: out of memory while dumping emit report");
            goto cleanup;
        }
    }
    if (!machine_emit_append_format(&builder, "\nfunction-shapes:\n")) {
        machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-211: out of memory while dumping emit report");
        goto cleanup;
    }

    for (index = 0; index < report->program.function_count; ++index) {
        const MachineEmitFunction *function = &report->program.functions[index];
        const MachineEmitFunctionShapeSummary *summary =
            report->function_shape_summaries ? &report->function_shape_summaries[index] : NULL;
        size_t block_summary_index;
        size_t block_offset =
            report->function_block_summary_offsets ? report->function_block_summary_offsets[index] : 0;
        size_t next_block_offset = index + 1 < report->program.function_count && report->function_block_summary_offsets
            ? report->function_block_summary_offsets[index + 1]
            : report->total_block_summary_count;

        if (!machine_emit_append_format(
                &builder,
                "  fn.%zu %s blocks=%zu ops=%zu calls=%zu blocks_with_calls=%zu jumps=%zu fallthrough=%zu branch=%zu branch_ft=%zu cmpbr=%zu cmpbri=%zu cmpbrft=%zu cmpbrift=%zu ret=%zu reti=%zu retspill=%zu\n",
                index,
                function->name ? function->name : "<unnamed>",
                summary ? summary->block_count : 0,
                summary ? summary->op_count : 0,
                summary ? summary->call_count : 0,
                summary ? summary->blocks_with_calls_count : 0,
                summary ? summary->jump_count : 0,
                summary ? summary->fallthrough_count : 0,
                summary ? summary->branch_count : 0,
                summary ? summary->branch_fallthrough_count : 0,
                summary ? summary->compare_branch_count : 0,
                summary ? summary->compare_branch_imm_count : 0,
                summary ? summary->compare_branch_fallthrough_count : 0,
                summary ? summary->compare_branch_imm_fallthrough_count : 0,
                summary ? summary->return_count : 0,
                summary ? summary->return_imm_count : 0,
                summary ? summary->return_spill_count : 0)) {
            machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-211: out of memory while dumping emit report");
            goto cleanup;
        }

        for (block_summary_index = block_offset; block_summary_index < next_block_offset; ++block_summary_index) {
            const MachineEmitBlockShapeSummary *block_summary = &report->block_shape_summaries[block_summary_index];

            if (!machine_emit_append_format(
                    &builder,
                    "    emit.%zu label=%s layout.%zu bb.%zu ops=%zu calls=%zu has_term=%d term=%d\n",
                    block_summary->emit_index,
                    block_summary->label_name ? block_summary->label_name : "<null>",
                    block_summary->original_layout_index,
                    block_summary->original_block_id,
                    block_summary->op_count,
                    block_summary->call_count,
                    block_summary->has_terminator,
                    (int)block_summary->terminator_kind)) {
                machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-211: out of memory while dumping emit report");
                goto cleanup;
            }
        }
    }

    if (!machine_emit_append_format(&builder, "\n") ||
        !machine_emit_dump_program(&report->program, &program_text, error) ||
        !machine_emit_append_format(&builder, "%s", program_text ? program_text : "")) {
        machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-212: out of memory while dumping emit report");
        goto cleanup;
    }

    *out_text = builder.data;
    builder.data = NULL;
    ok = 1;

cleanup:
    free(program_text);
    free(builder.data);
    return ok;
}

#endif
