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
                    if (!machine_layout_append_format(&builder, "ret ") ||
                        !machine_layout_dump_operand(&builder, block->terminator.as.return_value)) {
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

#endif
