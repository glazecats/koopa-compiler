#if defined(__INTELLISENSE__) && !defined(MACHINE_LAYOUT_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_LAYOUT_SPLIT_AGGREGATOR)
#define MACHINE_LAYOUT_SPLIT_INTELLISENSE_CONTEXT
#include "machine_layout.c"
#else

static int machine_layout_dump_operand(MachineLayoutStringBuilder *builder, MachineLayoutOperand operand) {
    switch (operand.kind) {
        case MACHINE_SELECT_OPERAND_IMMEDIATE:
            return machine_layout_append_format(builder, "%lld", operand.immediate);
        case MACHINE_SELECT_OPERAND_REGISTER:
            return machine_layout_append_format(builder, "reg.%zu", operand.machine_register_id);
        case MACHINE_SELECT_OPERAND_SPILL_SLOT:
            return machine_layout_append_format(builder, "spill.%zu", operand.spill_slot);
        default:
            return machine_layout_append_format(builder, "<none>");
    }
}

int machine_layout_dump_program(const MachineLayoutProgram *program,
    char **out_text,
    MachineLayoutError *error) {
    MachineLayoutStringBuilder builder;
    size_t function_index;

    if (out_text) {
        *out_text = NULL;
    }
    if (!program || !out_text) {
        machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-200: invalid dump contract");
        return 0;
    }

    memset(&builder, 0, sizeof(builder));
    if (!machine_layout_append_format(&builder, "machine_layout\n")) {
        free(builder.data);
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const MachineLayoutFunction *function = &program->functions[function_index];
        size_t block_index;

        if (!machine_layout_append_format(
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
            const MachineLayoutBlock *block = &function->blocks[block_index];
            size_t op_index;

            if (!machine_layout_append_format(
                    &builder,
                    "  layout.%zu -> bb.%zu:\n",
                    block->layout_index,
                    block->original_block_id)) {
                free(builder.data);
                return 0;
            }

            for (op_index = 0; op_index < block->op_count; ++op_index) {
                const MachineLayoutOp *op = &block->ops[op_index];

                if (!machine_layout_append_format(&builder, "    ")) {
                    free(builder.data);
                    return 0;
                }
                if (op->has_result) {
                    if (!machine_layout_dump_operand(&builder, op->result) ||
                        !machine_layout_append_format(&builder, " = ")) {
                        free(builder.data);
                        return 0;
                    }
                }

                switch (op->kind) {
                    case MACHINE_SELECT_OP_COPY:
                        if (!machine_layout_append_format(&builder, "copy ") ||
                            !machine_layout_dump_operand(&builder, op->as.copy_value)) {
                            free(builder.data);
                            return 0;
                        }
                        break;
                    case MACHINE_SELECT_OP_MATERIALIZE_IMM:
                        if (!machine_layout_append_format(&builder, "imm ") ||
                            !machine_layout_dump_operand(&builder, op->as.copy_value)) {
                            free(builder.data);
                            return 0;
                        }
                        break;
                    case MACHINE_SELECT_OP_ALU:
                        if (!machine_layout_append_format(&builder, "alu.%d ", (int)op->as.binary.op) ||
                            !machine_layout_dump_operand(&builder, op->as.binary.lhs) ||
                            !machine_layout_append_format(&builder, ", ") ||
                            !machine_layout_dump_operand(&builder, op->as.binary.rhs)) {
                            free(builder.data);
                            return 0;
                        }
                        break;
                    case MACHINE_SELECT_OP_ALU_IMM:
                        if (!machine_layout_append_format(&builder, "alui.%d ", (int)op->as.binary.op) ||
                            !machine_layout_dump_operand(&builder, op->as.binary.lhs) ||
                            !machine_layout_append_format(&builder, ", ") ||
                            !machine_layout_dump_operand(&builder, op->as.binary.rhs)) {
                            free(builder.data);
                            return 0;
                        }
                        break;
                    case MACHINE_SELECT_OP_CMP:
                        if (!machine_layout_append_format(&builder, "cmp.%d ", (int)op->as.binary.op) ||
                            !machine_layout_dump_operand(&builder, op->as.binary.lhs) ||
                            !machine_layout_append_format(&builder, ", ") ||
                            !machine_layout_dump_operand(&builder, op->as.binary.rhs)) {
                            free(builder.data);
                            return 0;
                        }
                        break;
                    case MACHINE_SELECT_OP_CMP_IMM:
                        if (!machine_layout_append_format(&builder, "cmpi.%d ", (int)op->as.binary.op) ||
                            !machine_layout_dump_operand(&builder, op->as.binary.lhs) ||
                            !machine_layout_append_format(&builder, ", ") ||
                            !machine_layout_dump_operand(&builder, op->as.binary.rhs)) {
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

                        if (!machine_layout_append_format(
                                &builder,
                                "%s %s(",
                                opcode_name,
                                op->as.call.callee_name ? op->as.call.callee_name : "<null>")) {
                            free(builder.data);
                            return 0;
                        }
                        for (arg_index = 0; arg_index < op->as.call.arg_count; ++arg_index) {
                            if (arg_index > 0 && !machine_layout_append_format(&builder, ", ")) {
                                free(builder.data);
                                return 0;
                            }
                            if (!machine_layout_dump_operand(&builder, op->as.call.args[arg_index])) {
                                free(builder.data);
                                return 0;
                            }
                        }
                        if (!machine_layout_append_format(&builder, ")")) {
                            free(builder.data);
                            return 0;
                        }
                        break;
                    }
                    case MACHINE_SELECT_OP_ADDR_LOCAL:
                        if (!machine_layout_append_format(&builder, "addr_local local.%zu", op->as.addr_slot.id)) {
                            free(builder.data);
                            return 0;
                        }
                        break;
                    case MACHINE_SELECT_OP_ADDR_FUNCTION:
                        if (!machine_layout_append_format(&builder,
                                "addr_function %s",
                                op->as.addr_function_name ? op->as.addr_function_name : "<null>")) {
                            free(builder.data);
                            return 0;
                        }
                        break;
                    case MACHINE_SELECT_OP_ADDR_GLOBAL:
                        if (!machine_layout_append_format(&builder, "addr_global global.%zu", op->as.addr_slot.id)) {
                            free(builder.data);
                            return 0;
                        }
                        break;
                    case MACHINE_SELECT_OP_LOAD_LOCAL:
                        if (!machine_layout_append_format(&builder, "load_local local.%zu", op->as.load_slot.id)) {
                            free(builder.data);
                            return 0;
                        }
                        break;
                    case MACHINE_SELECT_OP_STORE_LOCAL:
                        if (!machine_layout_append_format(&builder, "store_local local.%zu, ", op->as.store.slot.id) ||
                            !machine_layout_dump_operand(&builder, op->as.store.value)) {
                            free(builder.data);
                            return 0;
                        }
                        break;
                    case MACHINE_SELECT_OP_STORE_LOCAL_IMM:
                        if (!machine_layout_append_format(&builder, "store_locali local.%zu, ", op->as.store.slot.id) ||
                            !machine_layout_dump_operand(&builder, op->as.store.value)) {
                            free(builder.data);
                            return 0;
                        }
                        break;
                    case MACHINE_SELECT_OP_LOAD_GLOBAL:
                        if (!machine_layout_append_format(&builder, "load_global global.%zu", op->as.load_slot.id)) {
                            free(builder.data);
                            return 0;
                        }
                        break;
                    case MACHINE_SELECT_OP_STORE_GLOBAL:
                        if (!machine_layout_append_format(&builder, "store_global global.%zu, ", op->as.store.slot.id) ||
                            !machine_layout_dump_operand(&builder, op->as.store.value)) {
                            free(builder.data);
                            return 0;
                        }
                        break;
                    case MACHINE_SELECT_OP_STORE_GLOBAL_IMM:
                        if (!machine_layout_append_format(&builder, "store_globali global.%zu, ", op->as.store.slot.id) ||
                            !machine_layout_dump_operand(&builder, op->as.store.value)) {
                            free(builder.data);
                            return 0;
                        }
                        break;
                    case MACHINE_SELECT_OP_LOAD_INDIRECT:
                        if (!machine_layout_append_format(&builder, "load_indirect ") ||
                            !machine_layout_dump_operand(&builder, op->as.load_indirect_addr)) {
                            free(builder.data);
                            return 0;
                        }
                        break;
                    case MACHINE_SELECT_OP_STORE_INDIRECT:
                        if (!machine_layout_append_format(&builder, "store_indirect ") ||
                            !machine_layout_dump_operand(&builder, op->as.store_indirect.addr) ||
                            !machine_layout_append_format(&builder, ", ") ||
                            !machine_layout_dump_operand(&builder, op->as.store_indirect.value)) {
                            free(builder.data);
                            return 0;
                        }
                        break;
                    default:
                        free(builder.data);
                        machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-201: unknown op kind during dump");
                        return 0;
                }

                if (!machine_layout_append_format(&builder, "\n")) {
                    free(builder.data);
                    return 0;
                }
            }

            if (!machine_layout_append_format(&builder, "    ")) {
                free(builder.data);
                return 0;
            }
            switch (block->terminator.kind) {
                case MACHINE_LAYOUT_TERM_RETURN:
                    if (!machine_layout_append_format(&builder, "ret")) {
                        free(builder.data);
                        return 0;
                    }
                    if (block->terminator.as.return_value.kind != MACHINE_SELECT_OPERAND_NONE &&
                        (!machine_layout_append_format(&builder, " ") ||
                            !machine_layout_dump_operand(&builder, block->terminator.as.return_value))) {
                        free(builder.data);
                        return 0;
                    }
                    break;
                case MACHINE_LAYOUT_TERM_RETURN_IMM:
                    if (!machine_layout_append_format(&builder, "reti ") ||
                        !machine_layout_dump_operand(&builder, block->terminator.as.return_value)) {
                        free(builder.data);
                        return 0;
                    }
                    break;
                case MACHINE_LAYOUT_TERM_RETURN_SPILL:
                    if (!machine_layout_append_format(&builder, "retspill ") ||
                        !machine_layout_dump_operand(&builder, block->terminator.as.return_value)) {
                        free(builder.data);
                        return 0;
                    }
                    break;
                case MACHINE_LAYOUT_TERM_FALLTHROUGH:
                    if (!machine_layout_append_format(
                            &builder, "fallthrough layout.%zu", block->terminator.as.fallthrough_target)) {
                        free(builder.data);
                        return 0;
                    }
                    break;
                case MACHINE_LAYOUT_TERM_JUMP:
                    if (!machine_layout_append_format(&builder, "jmp layout.%zu", block->terminator.as.jump_target)) {
                        free(builder.data);
                        return 0;
                    }
                    break;
                case MACHINE_LAYOUT_TERM_BRANCH:
                    if (!machine_layout_append_format(&builder, "br ") ||
                        !machine_layout_dump_operand(&builder, block->terminator.as.branch.condition) ||
                        !machine_layout_append_format(
                            &builder,
                            ", layout.%zu, layout.%zu",
                            block->terminator.as.branch.then_target,
                            block->terminator.as.branch.else_target)) {
                        free(builder.data);
                        return 0;
                    }
                    break;
                case MACHINE_LAYOUT_TERM_BRANCH_FALLTHROUGH:
                    if (!machine_layout_append_format(
                            &builder,
                            "brft.%s ",
                            block->terminator.as.branch_fallthrough.branch_on_true ? "t" : "f") ||
                        !machine_layout_dump_operand(&builder, block->terminator.as.branch_fallthrough.condition) ||
                        !machine_layout_append_format(
                            &builder,
                            ", taken=layout.%zu, fallthrough=layout.%zu",
                            block->terminator.as.branch_fallthrough.taken_target,
                            block->terminator.as.branch_fallthrough.fallthrough_target)) {
                        free(builder.data);
                        return 0;
                    }
                    break;
                case MACHINE_LAYOUT_TERM_COMPARE_BRANCH:
                case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM:
                    if (!machine_layout_append_format(
                            &builder,
                            "%s.%d ",
                            block->terminator.kind == MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM ? "cmpbri" : "cmpbr",
                            (int)block->terminator.as.compare_branch.op) ||
                        !machine_layout_dump_operand(&builder, block->terminator.as.compare_branch.lhs) ||
                        !machine_layout_append_format(&builder, ", ") ||
                        !machine_layout_dump_operand(&builder, block->terminator.as.compare_branch.rhs) ||
                        !machine_layout_append_format(
                            &builder,
                            ", layout.%zu, layout.%zu",
                            block->terminator.as.compare_branch.then_target,
                            block->terminator.as.compare_branch.else_target)) {
                        free(builder.data);
                        return 0;
                    }
                    break;
                case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_FALLTHROUGH:
                case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM_FALLTHROUGH:
                    if (!machine_layout_append_format(
                            &builder,
                            "%sft.%s.%d ",
                            block->terminator.kind == MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM_FALLTHROUGH
                                ? "cmpbri"
                                : "cmpbr",
                            block->terminator.as.compare_branch_fallthrough.branch_on_true ? "t" : "f",
                            (int)block->terminator.as.compare_branch_fallthrough.op) ||
                        !machine_layout_dump_operand(&builder, block->terminator.as.compare_branch_fallthrough.lhs) ||
                        !machine_layout_append_format(&builder, ", ") ||
                        !machine_layout_dump_operand(&builder, block->terminator.as.compare_branch_fallthrough.rhs) ||
                        !machine_layout_append_format(
                            &builder,
                            ", taken=layout.%zu, fallthrough=layout.%zu",
                            block->terminator.as.compare_branch_fallthrough.taken_target,
                            block->terminator.as.compare_branch_fallthrough.fallthrough_target)) {
                        free(builder.data);
                        return 0;
                    }
                    break;
                default:
                    free(builder.data);
                    machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-202: unknown dump terminator");
                    return 0;
            }
            if (!machine_layout_append_format(&builder, "\n")) {
                free(builder.data);
                return 0;
            }
        }
    }

    *out_text = builder.data;
    return 1;
}

int machine_layout_dump_report(const MachineLayoutReport *report,
    char **out_text,
    MachineLayoutError *error) {
    MachineLayoutStringBuilder builder;
    char *program_text = NULL;
    size_t function_index;
    size_t index;

    if (out_text) {
        *out_text = NULL;
    }
    if (!report || !out_text) {
        machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-201: invalid report dump contract");
        return 0;
    }
    if (report->function_summary_count != report->program.function_count) {
        machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-203: report function-summary count drift");
        return 0;
    }
    if (report->program.function_count > 0 &&
        (!report->program.functions || !report->function_summaries || !report->function_block_summary_offsets)) {
        machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-204: report function tables missing");
        return 0;
    }
    if (report->total_block_summary_count > 0 && !report->block_summaries) {
        machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-205: report block summary table missing");
        return 0;
    }
    if ((report->functions_with_fallthrough_count > 0 && !report->function_indices_with_fallthrough) ||
        (report->functions_with_branches_count > 0 && !report->function_indices_with_branches)) {
        machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-206: report filter tables missing");
        return 0;
    }

    memset(&builder, 0, sizeof(builder));
    if (!machine_layout_append_format(
            &builder,
            "machine_layout-report registers=%zu globals=%zu functions=%zu total_block_summaries=%zu fallthrough_funcs=%zu branch_funcs=%zu\n",
            report->program.register_bank.register_count,
            report->program.global_count,
            report->program.function_count,
            report->total_block_summary_count,
            report->functions_with_fallthrough_count,
            report->functions_with_branches_count) ||
        !machine_layout_append_format(
            &builder,
            "target_policy preview_reg_cap=%zu preserves_spills=%d preserves_global_slots=%d preserves_fallthrough=%d\n",
            report->target_policy_summary.select_policy.current_riscv32_preview_logical_register_cap,
            report->target_policy_summary.preserves_spill_operands_for_later_materialization,
            report->target_policy_summary.preserves_global_slot_ops_for_later_address_formation,
            report->target_policy_summary.preserves_fallthrough_terminator_shapes)) {
        free(builder.data);
        machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-202: out of memory while dumping report");
        return 0;
    }
    if (!machine_layout_append_format(&builder, "functions-with-fallthrough:")) {
        free(builder.data);
        return 0;
    }
    for (index = 0; index < report->functions_with_fallthrough_count; ++index) {
        if (!machine_layout_append_format(&builder, " %zu", report->function_indices_with_fallthrough[index])) {
            free(builder.data);
            return 0;
        }
    }
    if (!machine_layout_append_format(&builder, "\nfunctions-with-branches:")) {
        free(builder.data);
        return 0;
    }
    for (index = 0; index < report->functions_with_branches_count; ++index) {
        if (!machine_layout_append_format(&builder, " %zu", report->function_indices_with_branches[index])) {
            free(builder.data);
            return 0;
        }
    }
    if (!machine_layout_append_format(&builder, "\nfunction-summaries:\n")) {
        free(builder.data);
        return 0;
    }
    for (function_index = 0; function_index < report->function_summary_count; ++function_index) {
        const MachineLayoutFunction *function = &report->program.functions[function_index];
        const MachineLayoutFunctionSummary *summary = &report->function_summaries[function_index];
        size_t block_offset = report->function_block_summary_offsets ? report->function_block_summary_offsets[function_index] : 0u;
        size_t next_block_offset = function_index + 1u < report->program.function_count && report->function_block_summary_offsets
            ? report->function_block_summary_offsets[function_index + 1u]
            : report->total_block_summary_count;
        size_t block_summary_index;

        if (next_block_offset < block_offset || next_block_offset > report->total_block_summary_count) {
            machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-207: report block summary offsets drift");
            free(builder.data);
            return 0;
        }

        if (!machine_layout_append_format(
                &builder,
                "  fn.%zu %s blocks=%zu ops=%zu jump=%zu fallthrough=%zu br=%zu brft=%zu cmpbr=%zu cmpbri=%zu cmpbrft=%zu cmpbrift=%zu ret=%zu reti=%zu retspill=%zu\n",
                function_index,
                function->name ? function->name : "<unnamed>",
                summary->block_count,
                summary->op_count,
                summary->jump_count,
                summary->fallthrough_count,
                summary->branch_count,
                summary->branch_fallthrough_count,
                summary->compare_branch_count,
                summary->compare_branch_imm_count,
                summary->compare_branch_fallthrough_count,
                summary->compare_branch_imm_fallthrough_count,
                summary->return_count,
                summary->return_imm_count,
                summary->return_spill_count)) {
            free(builder.data);
            return 0;
        }
        for (block_summary_index = block_offset; block_summary_index < next_block_offset; ++block_summary_index) {
            const MachineLayoutBlockSummary *block_summary = &report->block_summaries[block_summary_index];

            if (!machine_layout_append_format(
                    &builder,
                    "    layout.%zu bb.%zu ops=%zu has_term=%d term=%d\n",
                    block_summary->layout_index,
                    block_summary->original_block_id,
                    block_summary->op_count,
                    block_summary->has_terminator,
                    (int)block_summary->terminator_kind)) {
                free(builder.data);
                return 0;
            }
        }
    }
    if (!machine_layout_dump_program(&report->program, &program_text, error) ||
        !machine_layout_append_format(&builder, "\n%s", program_text ? program_text : "")) {
        free(program_text);
        free(builder.data);
        return 0;
    }
    free(program_text);
    *out_text = builder.data;
    return 1;
}

#endif
