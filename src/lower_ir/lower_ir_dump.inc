#if defined(__INTELLISENSE__) && !defined(LOWER_IR_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(LOWER_IR_SPLIT_AGGREGATOR)
#define LOWER_IR_SPLIT_INTELLISENSE_CONTEXT
#include "lower_ir.c"
#else

static const char *lower_ir_binary_op_name(LowerIrBinaryOp op) {
    switch (op) {
    case LOWER_IR_BINARY_ADD:
        return "add";
    case LOWER_IR_BINARY_SUB:
        return "sub";
    case LOWER_IR_BINARY_MUL:
        return "mul";
    case LOWER_IR_BINARY_DIV:
        return "div";
    case LOWER_IR_BINARY_MOD:
        return "mod";
    case LOWER_IR_BINARY_BIT_AND:
        return "and";
    case LOWER_IR_BINARY_BIT_XOR:
        return "xor";
    case LOWER_IR_BINARY_BIT_OR:
        return "or";
    case LOWER_IR_BINARY_SHIFT_LEFT:
        return "shl";
    case LOWER_IR_BINARY_SHIFT_RIGHT:
        return "shr";
    case LOWER_IR_BINARY_EQ:
        return "eq";
    case LOWER_IR_BINARY_NE:
        return "ne";
    case LOWER_IR_BINARY_LT:
        return "lt";
    case LOWER_IR_BINARY_LE:
        return "le";
    case LOWER_IR_BINARY_GT:
        return "gt";
    case LOWER_IR_BINARY_GE:
        return "ge";
    default:
        return "<unknown>";
    }
}

int lower_ir_dump_program(const LowerIrProgram *program, char **out_text) {
    LowerIrStringBuilder builder;
    size_t i;

    if (!program || !out_text) {
        return 0;
    }

    *out_text = NULL;
    builder.data = NULL;
    builder.length = 0;
    builder.capacity = 0;

    for (i = 0; i < program->global_count; ++i) {
        const LowerIrGlobal *global = &program->globals[i];

        if (!lower_ir_append_format(&builder,
                "global %s.%zu",
                (global->name && global->name[0] != '\0') ? global->name : "global",
                global->id)) {
            free(builder.data);
            return 0;
        }
        if (global->has_initializer &&
            !lower_ir_append_format(&builder, " = %lld", global->initializer_value)) {
            free(builder.data);
            return 0;
        }
        if (!lower_ir_append_string(&builder, "\n")) {
            free(builder.data);
            return 0;
        }
    }

    for (i = 0; i < program->function_count; ++i) {
        const LowerIrFunction *function = &program->functions[i];
        size_t j;

        if ((i > 0 || program->global_count > 0) && !lower_ir_append_string(&builder, "\n")) {
            free(builder.data);
            return 0;
        }

        if (!lower_ir_append_format(&builder,
                function->has_body ? "func %s(" : "declare %s(",
                function->name ? function->name : "<anon>")) {
            free(builder.data);
            return 0;
        }
        for (j = 0; j < function->parameter_count; ++j) {
            if (j > 0 && !lower_ir_append_string(&builder, ", ")) {
                free(builder.data);
                return 0;
            }
            if (!lower_ir_append_slot_name(&builder,
                    program,
                    function,
                    lower_ir_slot_local(j))) {
                free(builder.data);
                return 0;
            }
        }
        if (!function->has_body) {
            if (!lower_ir_append_string(&builder, ")\n")) {
                free(builder.data);
                return 0;
            }
            continue;
        }

        if (!lower_ir_append_string(&builder, ") {\n")) {
            free(builder.data);
            return 0;
        }

        for (j = 0; j < function->block_count; ++j) {
            const LowerIrBasicBlock *block = &function->blocks[j];
            size_t k;

            if (!lower_ir_append_format(&builder, "  bb.%zu:\n", block->id)) {
                free(builder.data);
                return 0;
            }

            for (k = 0; k < block->instruction_count; ++k) {
                const LowerIrInstruction *instruction = &block->instructions[k];

                if (!lower_ir_append_string(&builder, "    ")) {
                    free(builder.data);
                    return 0;
                }

                switch (instruction->kind) {
                case LOWER_IR_INSTR_MOV:
                    if (!lower_ir_append_value_name(&builder, instruction->result) ||
                        !lower_ir_append_string(&builder, " = mov ") ||
                        !lower_ir_append_value_name(&builder, instruction->as.mov_value) ||
                        !lower_ir_append_string(&builder, "\n")) {
                        free(builder.data);
                        return 0;
                    }
                    break;
                case LOWER_IR_INSTR_BINARY:
                    if (!lower_ir_append_value_name(&builder, instruction->result) ||
                        !lower_ir_append_string(&builder, " = ") ||
                        !lower_ir_append_string(&builder,
                            lower_ir_binary_op_name(instruction->as.binary.op)) ||
                        !lower_ir_append_string(&builder, " ") ||
                        !lower_ir_append_value_name(&builder, instruction->as.binary.lhs) ||
                        !lower_ir_append_string(&builder, ", ") ||
                        !lower_ir_append_value_name(&builder, instruction->as.binary.rhs) ||
                        !lower_ir_append_string(&builder, "\n")) {
                        free(builder.data);
                        return 0;
                    }
                    break;
                case LOWER_IR_INSTR_CALL:
                    if (!lower_ir_append_value_name(&builder, instruction->result) ||
                        !lower_ir_append_format(&builder,
                            " = call %s(",
                            instruction->as.call.callee_name ? instruction->as.call.callee_name : "<anon>")) {
                        free(builder.data);
                        return 0;
                    }
                    for (size_t arg_index = 0; arg_index < instruction->as.call.arg_count; ++arg_index) {
                        if (arg_index > 0 && !lower_ir_append_string(&builder, ", ")) {
                            free(builder.data);
                            return 0;
                        }
                        if (!lower_ir_append_value_name(&builder, instruction->as.call.args[arg_index])) {
                            free(builder.data);
                            return 0;
                        }
                    }
                    if (!lower_ir_append_string(&builder, ")\n")) {
                        free(builder.data);
                        return 0;
                    }
                    break;
                case LOWER_IR_INSTR_LOAD_LOCAL:
                    if (!lower_ir_append_value_name(&builder, instruction->result) ||
                        !lower_ir_append_string(&builder, " = load_local ") ||
                        !lower_ir_append_slot_name(&builder,
                            program,
                            function,
                            instruction->as.load_slot) ||
                        !lower_ir_append_string(&builder, "\n")) {
                        free(builder.data);
                        return 0;
                    }
                    break;
                case LOWER_IR_INSTR_STORE_LOCAL:
                    if (!lower_ir_append_string(&builder, "store_local ") ||
                        !lower_ir_append_slot_name(&builder,
                            program,
                            function,
                            instruction->as.store.slot) ||
                        !lower_ir_append_string(&builder, ", ") ||
                        !lower_ir_append_value_name(&builder, instruction->as.store.value) ||
                        !lower_ir_append_string(&builder, "\n")) {
                        free(builder.data);
                        return 0;
                    }
                    break;
                case LOWER_IR_INSTR_LOAD_GLOBAL:
                    if (!lower_ir_append_value_name(&builder, instruction->result) ||
                        !lower_ir_append_string(&builder, " = load_global ") ||
                        !lower_ir_append_slot_name(&builder,
                            program,
                            function,
                            instruction->as.load_slot) ||
                        !lower_ir_append_string(&builder, "\n")) {
                        free(builder.data);
                        return 0;
                    }
                    break;
                case LOWER_IR_INSTR_STORE_GLOBAL:
                    if (!lower_ir_append_string(&builder, "store_global ") ||
                        !lower_ir_append_slot_name(&builder,
                            program,
                            function,
                            instruction->as.store.slot) ||
                        !lower_ir_append_string(&builder, ", ") ||
                        !lower_ir_append_value_name(&builder, instruction->as.store.value) ||
                        !lower_ir_append_string(&builder, "\n")) {
                        free(builder.data);
                        return 0;
                    }
                    break;
                default:
                    free(builder.data);
                    return 0;
                }
            }

            if (!lower_ir_append_string(&builder, "    ")) {
                free(builder.data);
                return 0;
            }

            switch (block->terminator.kind) {
            case LOWER_IR_TERM_RETURN:
                if (!lower_ir_append_string(&builder, "ret ") ||
                    !lower_ir_append_value_name(&builder, block->terminator.as.return_value) ||
                    !lower_ir_append_string(&builder, "\n")) {
                    free(builder.data);
                    return 0;
                }
                break;
            case LOWER_IR_TERM_JUMP:
                if (!lower_ir_append_format(&builder,
                        "jmp bb.%zu\n",
                        block->terminator.as.jump_target)) {
                    free(builder.data);
                    return 0;
                }
                break;
            case LOWER_IR_TERM_BRANCH:
                if (!lower_ir_append_string(&builder, "br ") ||
                    !lower_ir_append_value_name(&builder, block->terminator.as.branch.condition) ||
                    !lower_ir_append_format(&builder,
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

        if (!lower_ir_append_string(&builder, "}\n")) {
            free(builder.data);
            return 0;
        }
    }

    *out_text = builder.data;
    return 1;
}

#endif
