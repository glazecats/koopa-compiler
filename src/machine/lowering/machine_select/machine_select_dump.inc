#if defined(__INTELLISENSE__) && !defined(MACHINE_SELECT_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_SELECT_SPLIT_AGGREGATOR)
#define MACHINE_SELECT_SPLIT_INTELLISENSE_CONTEXT
#include "machine_select.c"
#else

static int machine_select_dump_operand(MachineSelectStringBuilder *builder, MachineSelectOperand operand) {
    if (!builder) {
        return 0;
    }
    switch (operand.kind) {
        case MACHINE_SELECT_OPERAND_IMMEDIATE:
            return machine_select_append_format(builder, "%lld", operand.immediate);
        case MACHINE_SELECT_OPERAND_REGISTER:
            return machine_select_append_format(builder, "reg.%zu", operand.machine_register_id);
        case MACHINE_SELECT_OPERAND_SPILL_SLOT:
            return machine_select_append_format(builder, "spill.%zu", operand.spill_slot);
        default:
            return machine_select_append_format(builder, "<none>");
    }
}

int machine_select_dump_program(const MachineSelectProgram *program,
    char **out_text,
    MachineSelectError *error) {
    MachineSelectStringBuilder builder;
    size_t function_index;

    if (out_text) {
        *out_text = NULL;
    }
    if (!program || !out_text) {
        machine_select_set_error(error, 0, 0, "MACHINE-SELECT-200: invalid dump contract");
        return 0;
    }

    memset(&builder, 0, sizeof(builder));
    if (!machine_select_append_format(&builder, "machine_select\n")) {
        free(builder.data);
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const MachineSelectFunction *function = &program->functions[function_index];
        size_t block_index;

        if (!machine_select_append_format(
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
            const MachineSelectBasicBlock *block = &function->blocks[block_index];
            size_t op_index;

            if (!machine_select_append_format(&builder, "  bb.%zu:\n", block->id)) {
                free(builder.data);
                return 0;
            }

            for (op_index = 0; op_index < block->op_count; ++op_index) {
                const MachineSelectOp *op = &block->ops[op_index];

                if (!machine_select_append_format(&builder, "    ")) {
                    free(builder.data);
                    return 0;
                }
                if (op->has_result) {
                    if (!machine_select_dump_operand(&builder, op->result) ||
                        !machine_select_append_format(&builder, " = ")) {
                        free(builder.data);
                        return 0;
                    }
                }

                switch (op->kind) {
                    case MACHINE_SELECT_OP_COPY:
                        if (!machine_select_append_format(&builder, "copy ") ||
                            !machine_select_dump_operand(&builder, op->as.copy_value)) {
                            free(builder.data);
                            return 0;
                        }
                        break;
                    case MACHINE_SELECT_OP_MATERIALIZE_IMM:
                        if (!machine_select_append_format(&builder, "imm ") ||
                            !machine_select_dump_operand(&builder, op->as.copy_value)) {
                            free(builder.data);
                            return 0;
                        }
                        break;
                    case MACHINE_SELECT_OP_ALU:
                        if (!machine_select_append_format(&builder, "alu.%d ", (int)op->as.binary.op) ||
                            !machine_select_dump_operand(&builder, op->as.binary.lhs) ||
                            !machine_select_append_format(&builder, ", ") ||
                            !machine_select_dump_operand(&builder, op->as.binary.rhs)) {
                            free(builder.data);
                            return 0;
                        }
                        break;
                    case MACHINE_SELECT_OP_ALU_IMM:
                        if (!machine_select_append_format(&builder, "alui.%d ", (int)op->as.binary.op) ||
                            !machine_select_dump_operand(&builder, op->as.binary.lhs) ||
                            !machine_select_append_format(&builder, ", ") ||
                            !machine_select_dump_operand(&builder, op->as.binary.rhs)) {
                            free(builder.data);
                            return 0;
                        }
                        break;
                    case MACHINE_SELECT_OP_CMP:
                        if (!machine_select_append_format(&builder, "cmp.%d ", (int)op->as.binary.op) ||
                            !machine_select_dump_operand(&builder, op->as.binary.lhs) ||
                            !machine_select_append_format(&builder, ", ") ||
                            !machine_select_dump_operand(&builder, op->as.binary.rhs)) {
                            free(builder.data);
                            return 0;
                        }
                        break;
                    case MACHINE_SELECT_OP_CMP_IMM:
                        if (!machine_select_append_format(&builder, "cmpi.%d ", (int)op->as.binary.op) ||
                            !machine_select_dump_operand(&builder, op->as.binary.lhs) ||
                            !machine_select_append_format(&builder, ", ") ||
                            !machine_select_dump_operand(&builder, op->as.binary.rhs)) {
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
                        size_t arg_index;
                        const char *opcode_name;

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
                                opcode_name = "<bad-call>";
                                break;
                        }

                        if (!machine_select_append_format(
                                &builder,
                                "%s %s(",
                                opcode_name,
                                op->as.call.callee_name ? op->as.call.callee_name : "<null>")) {
                            free(builder.data);
                            return 0;
                        }
                        for (arg_index = 0; arg_index < op->as.call.arg_count; ++arg_index) {
                            if (arg_index > 0 && !machine_select_append_format(&builder, ", ")) {
                                free(builder.data);
                                return 0;
                            }
                            if (!machine_select_dump_operand(&builder, op->as.call.args[arg_index])) {
                                free(builder.data);
                                return 0;
                            }
                        }
                        if (!machine_select_append_format(&builder, ")")) {
                            free(builder.data);
                            return 0;
                        }
                        break;
                    }
                    case MACHINE_SELECT_OP_LOAD_LOCAL:
                        if (!machine_select_append_format(&builder, "load_local local.%zu", op->as.load_slot.id)) {
                            free(builder.data);
                            return 0;
                        }
                        break;
                    case MACHINE_SELECT_OP_STORE_LOCAL:
                        if (!machine_select_append_format(&builder, "store_local local.%zu, ", op->as.store.slot.id) ||
                            !machine_select_dump_operand(&builder, op->as.store.value)) {
                            free(builder.data);
                            return 0;
                        }
                        break;
                    case MACHINE_SELECT_OP_STORE_LOCAL_IMM:
                        if (!machine_select_append_format(&builder, "store_locali local.%zu, ", op->as.store.slot.id) ||
                            !machine_select_dump_operand(&builder, op->as.store.value)) {
                            free(builder.data);
                            return 0;
                        }
                        break;
                    case MACHINE_SELECT_OP_LOAD_GLOBAL:
                        if (!machine_select_append_format(&builder, "load_global global.%zu", op->as.load_slot.id)) {
                            free(builder.data);
                            return 0;
                        }
                        break;
                    case MACHINE_SELECT_OP_STORE_GLOBAL:
                        if (!machine_select_append_format(&builder, "store_global global.%zu, ", op->as.store.slot.id) ||
                            !machine_select_dump_operand(&builder, op->as.store.value)) {
                            free(builder.data);
                            return 0;
                        }
                        break;
                    case MACHINE_SELECT_OP_STORE_GLOBAL_IMM:
                        if (!machine_select_append_format(&builder, "store_globali global.%zu, ", op->as.store.slot.id) ||
                            !machine_select_dump_operand(&builder, op->as.store.value)) {
                            free(builder.data);
                            return 0;
                        }
                        break;
                    default:
                        free(builder.data);
                        machine_select_set_error(error, 0, 0, "MACHINE-SELECT-201: unknown op kind during dump");
                        return 0;
                }

                if (!machine_select_append_format(&builder, "\n")) {
                    free(builder.data);
                    return 0;
                }
            }

            if (!machine_select_append_format(&builder, "    ")) {
                free(builder.data);
                return 0;
            }
            switch (block->terminator.kind) {
                case MACHINE_SELECT_TERM_RETURN:
                case MACHINE_SELECT_TERM_RETURN_IMM:
                case MACHINE_SELECT_TERM_RETURN_SPILL:
                    if (!machine_select_append_format(
                            &builder,
                            "%s ",
                            block->terminator.kind == MACHINE_SELECT_TERM_RETURN_IMM
                                ? "reti"
                                : (block->terminator.kind == MACHINE_SELECT_TERM_RETURN_SPILL ? "retspill" : "ret")) ||
                        !machine_select_dump_operand(&builder, block->terminator.as.return_value)) {
                        free(builder.data);
                        return 0;
                    }
                    break;
                case MACHINE_SELECT_TERM_JUMP:
                    if (!machine_select_append_format(&builder, "jmp bb.%zu", block->terminator.as.jump_target)) {
                        free(builder.data);
                        return 0;
                    }
                    break;
                case MACHINE_SELECT_TERM_BRANCH:
                    if (!machine_select_append_format(&builder, "br ") ||
                        !machine_select_dump_operand(&builder, block->terminator.as.branch.condition) ||
                        !machine_select_append_format(
                            &builder,
                            ", bb.%zu, bb.%zu",
                            block->terminator.as.branch.then_target,
                            block->terminator.as.branch.else_target)) {
                        free(builder.data);
                        return 0;
                    }
                    break;
                case MACHINE_SELECT_TERM_COMPARE_BRANCH:
                    if (!machine_select_append_format(
                            &builder, "cmpbr.%d ", (int)block->terminator.as.compare_branch.op) ||
                        !machine_select_dump_operand(&builder, block->terminator.as.compare_branch.lhs) ||
                        !machine_select_append_format(&builder, ", ") ||
                        !machine_select_dump_operand(&builder, block->terminator.as.compare_branch.rhs) ||
                        !machine_select_append_format(
                            &builder,
                            ", bb.%zu, bb.%zu",
                            block->terminator.as.compare_branch.then_target,
                            block->terminator.as.compare_branch.else_target)) {
                        free(builder.data);
                        return 0;
                    }
                    break;
                case MACHINE_SELECT_TERM_COMPARE_BRANCH_IMM:
                    if (!machine_select_append_format(
                            &builder, "cmpbri.%d ", (int)block->terminator.as.compare_branch.op) ||
                        !machine_select_dump_operand(&builder, block->terminator.as.compare_branch.lhs) ||
                        !machine_select_append_format(&builder, ", ") ||
                        !machine_select_dump_operand(&builder, block->terminator.as.compare_branch.rhs) ||
                        !machine_select_append_format(
                            &builder,
                            ", bb.%zu, bb.%zu",
                            block->terminator.as.compare_branch.then_target,
                            block->terminator.as.compare_branch.else_target)) {
                        free(builder.data);
                        return 0;
                    }
                    break;
                default:
                    free(builder.data);
                    machine_select_set_error(error, 0, 0, "MACHINE-SELECT-202: unknown terminator during dump");
                    return 0;
            }
            if (!machine_select_append_format(&builder, "\n")) {
                free(builder.data);
                return 0;
            }
        }
    }

    *out_text = builder.data;
    return 1;
}

int machine_select_lower_dump_from_machine_ir(const MachineIrProgram *source,
    char **out_text,
    MachineSelectError *error) {
    MachineSelectProgram program;
    int ok;

    machine_select_program_init(&program);
    ok = machine_select_lower_program_from_machine_ir(source, &program, error) &&
        machine_select_dump_program(&program, out_text, error);
    machine_select_program_free(&program);
    return ok;
}

int machine_select_lower_canonicalized_dump_from_machine_ir(const MachineIrProgram *source,
    char **out_text,
    MachineSelectError *error) {
    MachineSelectProgram program;
    int ok;

    machine_select_program_init(&program);
    ok = machine_select_lower_canonicalized_program_from_machine_ir(source, &program, error) &&
        machine_select_dump_program(&program, out_text, error);
    machine_select_program_free(&program);
    return ok;
}

#endif
