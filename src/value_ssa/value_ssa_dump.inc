#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_SPLIT_AGGREGATOR)
#define VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa.c"
#else

static const char *value_ssa_binary_op_name(ValueSsaBinaryOp op) {
    switch (op) {
    case VALUE_SSA_BINARY_ADD:
        return "add";
    case VALUE_SSA_BINARY_SUB:
        return "sub";
    case VALUE_SSA_BINARY_MUL:
        return "mul";
    case VALUE_SSA_BINARY_DIV:
        return "div";
    case VALUE_SSA_BINARY_MOD:
        return "mod";
    case VALUE_SSA_BINARY_BIT_AND:
        return "and";
    case VALUE_SSA_BINARY_BIT_XOR:
        return "xor";
    case VALUE_SSA_BINARY_BIT_OR:
        return "or";
    case VALUE_SSA_BINARY_SHIFT_LEFT:
        return "shl";
    case VALUE_SSA_BINARY_SHIFT_RIGHT:
        return "shr";
    case VALUE_SSA_BINARY_EQ:
        return "eq";
    case VALUE_SSA_BINARY_NE:
        return "ne";
    case VALUE_SSA_BINARY_LT:
        return "lt";
    case VALUE_SSA_BINARY_LE:
        return "le";
    case VALUE_SSA_BINARY_GT:
        return "gt";
    case VALUE_SSA_BINARY_GE:
        return "ge";
    default:
        return "<unknown>";
    }
}

int value_ssa_dump_program(const ValueSsaProgram *program, char **out_text) {
    ValueSsaStringBuilder builder;
    size_t i;

    if (!program || !out_text) {
        return 0;
    }

    *out_text = NULL;
    builder.data = NULL;
    builder.length = 0;
    builder.capacity = 0;

    for (i = 0; i < program->global_count; ++i) {
        const ValueSsaGlobal *global = &program->globals[i];

        if (!value_ssa_append_format(&builder,
                "global %s.%zu",
                (global->name && global->name[0] != '\0') ? global->name : "global",
                global->id)) {
            free(builder.data);
            return 0;
        }
        if (global->has_initializer &&
            !value_ssa_append_format(&builder, " = %lld", global->initializer_value)) {
            free(builder.data);
            return 0;
        }
        if (!value_ssa_append_string(&builder, "\n")) {
            free(builder.data);
            return 0;
        }
    }

    for (i = 0; i < program->function_count; ++i) {
        const ValueSsaFunction *function = &program->functions[i];
        size_t j;

        if ((i > 0 || program->global_count > 0) && !value_ssa_append_string(&builder, "\n")) {
            free(builder.data);
            return 0;
        }

        if (!value_ssa_append_format(&builder,
                function->has_body ? "func %s(" : "declare %s(",
                function->name ? function->name : "<anon>")) {
            free(builder.data);
            return 0;
        }
        for (j = 0; j < function->parameter_count; ++j) {
            if (j > 0 && !value_ssa_append_string(&builder, ", ")) {
                free(builder.data);
                return 0;
            }
            if (!value_ssa_append_slot_name(&builder,
                    program,
                    function,
                    value_ssa_slot_local(j))) {
                free(builder.data);
                return 0;
            }
        }
        if (!function->has_body) {
            if (!value_ssa_append_string(&builder, ")\n")) {
                free(builder.data);
                return 0;
            }
            continue;
        }

        if (!value_ssa_append_string(&builder, ") {\n")) {
            free(builder.data);
            return 0;
        }

        for (j = 0; j < function->block_count; ++j) {
            const ValueSsaBasicBlock *block = &function->blocks[j];
            size_t k;

            if (!value_ssa_append_format(&builder, "  bb.%zu:\n", block->id)) {
                free(builder.data);
                return 0;
            }

            for (k = 0; k < block->phi_count; ++k) {
                const ValueSsaPhi *phi = &block->phis[k];
                size_t input_index;

                if (!value_ssa_append_string(&builder, "    ") ||
                    !value_ssa_append_format(&builder, "ssa.%zu = phi ", phi->result_id)) {
                    free(builder.data);
                    return 0;
                }
                for (input_index = 0; input_index < phi->input_count; ++input_index) {
                    const ValueSsaPhiInput *input = &phi->inputs[input_index];

                    if (input_index > 0 && !value_ssa_append_string(&builder, ", ")) {
                        free(builder.data);
                        return 0;
                    }
                    if (!value_ssa_append_format(&builder, "[bb.%zu: ", input->predecessor_block_id) ||
                        !value_ssa_append_value_name(&builder, input->value) ||
                        !value_ssa_append_string(&builder, "]")) {
                        free(builder.data);
                        return 0;
                    }
                }
                if (!value_ssa_append_string(&builder, "\n")) {
                    free(builder.data);
                    return 0;
                }
            }

            for (k = 0; k < block->instruction_count; ++k) {
                const ValueSsaInstruction *instruction = &block->instructions[k];

                if (!value_ssa_append_string(&builder, "    ")) {
                    free(builder.data);
                    return 0;
                }

                switch (instruction->kind) {
                case VALUE_SSA_INSTR_MOV:
                    if (!value_ssa_append_value_name(&builder, instruction->result) ||
                        !value_ssa_append_string(&builder, " = mov ") ||
                        !value_ssa_append_value_name(&builder, instruction->as.mov_value) ||
                        !value_ssa_append_string(&builder, "\n")) {
                        free(builder.data);
                        return 0;
                    }
                    break;
                case VALUE_SSA_INSTR_BINARY:
                    if (!value_ssa_append_value_name(&builder, instruction->result) ||
                        !value_ssa_append_string(&builder, " = ") ||
                        !value_ssa_append_string(&builder, value_ssa_binary_op_name(instruction->as.binary.op)) ||
                        !value_ssa_append_string(&builder, " ") ||
                        !value_ssa_append_value_name(&builder, instruction->as.binary.lhs) ||
                        !value_ssa_append_string(&builder, ", ") ||
                        !value_ssa_append_value_name(&builder, instruction->as.binary.rhs) ||
                        !value_ssa_append_string(&builder, "\n")) {
                        free(builder.data);
                        return 0;
                    }
                    break;
                case VALUE_SSA_INSTR_CALL:
                    if (instruction->has_result) {
                        if (!value_ssa_append_value_name(&builder, instruction->result) ||
                            !value_ssa_append_string(&builder, " = ")) {
                            free(builder.data);
                            return 0;
                        }
                    }
                    if (!value_ssa_append_format(&builder,
                            "call %s(",
                            instruction->as.call.callee_name ? instruction->as.call.callee_name : "<anon>")) {
                        free(builder.data);
                        return 0;
                    }
                    for (size_t arg_index = 0; arg_index < instruction->as.call.arg_count; ++arg_index) {
                        if (arg_index > 0 && !value_ssa_append_string(&builder, ", ")) {
                            free(builder.data);
                            return 0;
                        }
                        if (!value_ssa_append_value_name(&builder, instruction->as.call.args[arg_index])) {
                            free(builder.data);
                            return 0;
                        }
                    }
                    if (!value_ssa_append_string(&builder, ")\n")) {
                        free(builder.data);
                        return 0;
                    }
                    break;
                case VALUE_SSA_INSTR_LOAD_LOCAL:
                    if (!value_ssa_append_value_name(&builder, instruction->result) ||
                        !value_ssa_append_string(&builder, " = load_local ") ||
                        !value_ssa_append_slot_name(&builder,
                            program,
                            function,
                            instruction->as.load_slot) ||
                        !value_ssa_append_string(&builder, "\n")) {
                        free(builder.data);
                        return 0;
                    }
                    break;
                case VALUE_SSA_INSTR_STORE_LOCAL:
                    if (!value_ssa_append_string(&builder, "store_local ") ||
                        !value_ssa_append_slot_name(&builder,
                            program,
                            function,
                            instruction->as.store.slot) ||
                        !value_ssa_append_string(&builder, ", ") ||
                        !value_ssa_append_value_name(&builder, instruction->as.store.value) ||
                        !value_ssa_append_string(&builder, "\n")) {
                        free(builder.data);
                        return 0;
                    }
                    break;
                case VALUE_SSA_INSTR_LOAD_GLOBAL:
                    if (!value_ssa_append_value_name(&builder, instruction->result) ||
                        !value_ssa_append_string(&builder, " = load_global ") ||
                        !value_ssa_append_slot_name(&builder,
                            program,
                            function,
                            instruction->as.load_slot) ||
                        !value_ssa_append_string(&builder, "\n")) {
                        free(builder.data);
                        return 0;
                    }
                    break;
                case VALUE_SSA_INSTR_STORE_GLOBAL:
                    if (!value_ssa_append_string(&builder, "store_global ") ||
                        !value_ssa_append_slot_name(&builder,
                            program,
                            function,
                            instruction->as.store.slot) ||
                        !value_ssa_append_string(&builder, ", ") ||
                        !value_ssa_append_value_name(&builder, instruction->as.store.value) ||
                        !value_ssa_append_string(&builder, "\n")) {
                        free(builder.data);
                        return 0;
                    }
                    break;
                default:
                    free(builder.data);
                    return 0;
                }
            }

            if (!value_ssa_append_string(&builder, "    ")) {
                free(builder.data);
                return 0;
            }
            switch (block->terminator.kind) {
            case VALUE_SSA_TERM_RETURN:
                if (!value_ssa_append_string(&builder, "ret")) {
                    free(builder.data);
                    return 0;
                }
                if (block->terminator.has_return_value &&
                    (!value_ssa_append_string(&builder, " ") ||
                        !value_ssa_append_value_name(&builder, block->terminator.as.return_value))) {
                    free(builder.data);
                    return 0;
                }
                if (!value_ssa_append_string(&builder, "\n")) {
                    free(builder.data);
                    return 0;
                }
                break;
            case VALUE_SSA_TERM_JUMP:
                if (!value_ssa_append_format(&builder, "jmp bb.%zu\n", block->terminator.as.jump_target)) {
                    free(builder.data);
                    return 0;
                }
                break;
            case VALUE_SSA_TERM_BRANCH:
                if (!value_ssa_append_string(&builder, "br ") ||
                    !value_ssa_append_value_name(&builder, block->terminator.as.branch.condition) ||
                    !value_ssa_append_format(&builder,
                        ", bb.%zu, bb.%zu\n",
                        block->terminator.as.branch.then_target,
                        block->terminator.as.branch.else_target)) {
                    free(builder.data);
                    return 0;
                }
                break;
            default:
                free(builder.data);
                return 0;
            }
        }

        if (!value_ssa_append_string(&builder, "}\n")) {
            free(builder.data);
            return 0;
        }
    }

    *out_text = builder.data;
    return 1;
}

#endif
