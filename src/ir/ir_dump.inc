#if defined(__INTELLISENSE__) && !defined(IR_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(IR_SPLIT_AGGREGATOR)
#define IR_SPLIT_INTELLISENSE_CONTEXT
#include "ir.c"
#else

static int ir_append_string(IrStringBuilder *builder, const char *text) {
    char *new_data;
    size_t text_length;
    size_t needed_capacity;
    size_t next_capacity;

    if (!builder || !text) {
        return 0;
    }

    text_length = strlen(text);
    if (text_length > ((size_t)-1) - builder->length - 1) {
        return 0;
    }
    needed_capacity = builder->length + text_length + 1;

    if (needed_capacity > builder->capacity) {
        next_capacity = builder->capacity;
        while (next_capacity < needed_capacity) {
            if (!ir_next_growth_capacity(next_capacity, 64, sizeof(char), &next_capacity)) {
                return 0;
            }
        }
        new_data = (char *)realloc(builder->data, next_capacity);
        if (!new_data) {
            return 0;
        }
        builder->data = new_data;
        builder->capacity = next_capacity;
    }

    memcpy(builder->data + builder->length, text, text_length + 1);
    builder->length += text_length;
    return 1;
}

static int ir_append_format(IrStringBuilder *builder, const char *fmt, ...) {
    va_list args;
    va_list args_copy;
    char stack_buffer[128];
    int needed;
    char *heap_buffer = NULL;
    int ok;

    if (!builder || !fmt) {
        return 0;
    }

    va_start(args, fmt);
    va_copy(args_copy, args);
    needed = vsnprintf(stack_buffer, sizeof(stack_buffer), fmt, args);
    va_end(args);

    if (needed < 0) {
        va_end(args_copy);
        return 0;
    }

    if ((size_t)needed < sizeof(stack_buffer)) {
        ok = ir_append_string(builder, stack_buffer);
        va_end(args_copy);
        return ok;
    }

    heap_buffer = (char *)malloc((size_t)needed + 1);
    if (!heap_buffer) {
        va_end(args_copy);
        return 0;
    }

    vsnprintf(heap_buffer, (size_t)needed + 1, fmt, args_copy);
    va_end(args_copy);
    ok = ir_append_string(builder, heap_buffer);
    free(heap_buffer);
    return ok;
}

static int ir_append_value_name(IrStringBuilder *builder,
    const IrProgram *program,
    const IrFunction *function,
    IrValueRef value) {
    const IrLocal *local;
    const IrGlobal *global;

    switch (value.kind) {
    case IR_VALUE_IMMEDIATE:
        return ir_append_format(builder, "%lld", value.immediate);
    case IR_VALUE_TEMP:
        return ir_append_format(builder, "tmp.%zu", value.id);
    case IR_VALUE_LOCAL:
        local = ir_function_get_local(function, value.id);
        if (!local || !local->source_name || local->source_name[0] == '\0') {
            return ir_append_format(builder, "local.%zu", value.id);
        }
        return ir_append_format(builder, "%s.%zu", local->source_name, local->id);
    case IR_VALUE_GLOBAL:
        global = ir_program_get_global(program, value.id);
        if (!global || !global->name || global->name[0] == '\0') {
            return ir_append_format(builder, "global.%zu", value.id);
        }
        return ir_append_format(builder, "%s.%zu", global->name, global->id);
    default:
        return 0;
    }
}

int ir_dump_program(const IrProgram *program, char **out_text) {
    IrStringBuilder builder;
    size_t i;

    if (!program || !out_text) {
        return 0;
    }

    *out_text = NULL;
    builder.data = NULL;
    builder.length = 0;
    builder.capacity = 0;

    for (i = 0; i < program->global_count; ++i) {
        const IrGlobal *global = &program->globals[i];

        if (!ir_append_format(&builder,
                "global %s.%zu",
                (global->name && global->name[0] != '\0') ? global->name : "global",
                global->id)) {
            free(builder.data);
            return 0;
        }
        if (global->has_initializer &&
            !ir_append_format(&builder, " = %lld", global->initializer_value)) {
            free(builder.data);
            return 0;
        }
        if (!ir_append_string(&builder, "\n")) {
            free(builder.data);
            return 0;
        }
    }

    for (i = 0; i < program->function_count; ++i) {
        const IrFunction *function = &program->functions[i];
        size_t j;

        if ((i > 0 || program->global_count > 0) && !ir_append_string(&builder, "\n")) {
            free(builder.data);
            return 0;
        }

        if (!ir_append_format(&builder,
                function->has_body ? "func %s(" : "declare %s(",
                function->name ? function->name : "<anon>")) {
            free(builder.data);
            return 0;
        }
        for (j = 0; j < function->parameter_count; ++j) {
            if (j > 0 && !ir_append_string(&builder, ", ")) {
                free(builder.data);
                return 0;
            }
            if (!ir_append_value_name(&builder, program, function, ir_value_local(j))) {
                free(builder.data);
                return 0;
            }
        }
        if (!function->has_body) {
            if (!ir_append_string(&builder, ")\n")) {
                free(builder.data);
                return 0;
            }
            continue;
        }

        if (!ir_append_string(&builder, ") {\n")) {
            free(builder.data);
            return 0;
        }

        for (j = 0; j < function->block_count; ++j) {
            const IrBasicBlock *block = &function->blocks[j];
            size_t k;

            if (!ir_append_format(&builder, "  bb.%zu:\n", block->id)) {
                free(builder.data);
                return 0;
            }
            for (k = 0; k < block->instruction_count; ++k) {
                const IrInstruction *instruction = &block->instructions[k];

                if (!ir_append_string(&builder, "    ") ||
                    !ir_append_value_name(&builder, program, function, instruction->result) ||
                    !ir_append_string(&builder, " = ")) {
                    free(builder.data);
                    return 0;
                }

                if (instruction->kind == IR_INSTR_MOV) {
                    if (!ir_append_string(&builder, "mov ") ||
                        !ir_append_value_name(&builder,
                            program,
                            function,
                            instruction->as.mov_value) ||
                        !ir_append_string(&builder, "\n")) {
                        free(builder.data);
                        return 0;
                    }
                } else if (instruction->kind == IR_INSTR_BINARY) {
                    if (!ir_append_string(&builder, ir_binary_op_name(instruction->as.binary.op)) ||
                        !ir_append_string(&builder, " ") ||
                        !ir_append_value_name(&builder,
                            program,
                            function,
                            instruction->as.binary.lhs) ||
                        !ir_append_string(&builder, ", ") ||
                        !ir_append_value_name(&builder,
                            program,
                            function,
                            instruction->as.binary.rhs) ||
                        !ir_append_string(&builder, "\n")) {
                        free(builder.data);
                        return 0;
                    }
                } else if (instruction->kind == IR_INSTR_CALL) {
                    size_t arg_index;

                    if (!ir_append_format(&builder,
                            "call %s(",
                            instruction->as.call.callee_name ? instruction->as.call.callee_name : "<anon>")) {
                        free(builder.data);
                        return 0;
                    }
                    for (arg_index = 0; arg_index < instruction->as.call.arg_count; ++arg_index) {
                        if ((arg_index > 0 && !ir_append_string(&builder, ", ")) ||
                            !ir_append_value_name(&builder,
                                program,
                                function,
                                instruction->as.call.args[arg_index])) {
                            free(builder.data);
                            return 0;
                        }
                    }
                    if (!ir_append_string(&builder, ")\n")) {
                        free(builder.data);
                        return 0;
                    }
                } else {
                    free(builder.data);
                    return 0;
                }
            }

            if (block->has_terminator) {
                if (block->terminator.kind == IR_TERM_RETURN) {
                    if (!ir_append_string(&builder, "    ret ") ||
                        !ir_append_value_name(&builder,
                            program,
                            function,
                            block->terminator.as.return_value) ||
                        !ir_append_string(&builder, "\n")) {
                        free(builder.data);
                        return 0;
                    }
                } else if (block->terminator.kind == IR_TERM_JUMP) {
                    if (!ir_append_format(&builder,
                            "    jmp bb.%zu\n",
                            block->terminator.as.jump_target)) {
                        free(builder.data);
                        return 0;
                    }
                } else if (block->terminator.kind == IR_TERM_BRANCH) {
                    if (!ir_append_string(&builder, "    br ") ||
                        !ir_append_value_name(&builder,
                            program,
                            function,
                            block->terminator.as.branch.condition) ||
                        !ir_append_format(&builder,
                            ", bb.%zu, bb.%zu\n",
                            block->terminator.as.branch.then_target,
                            block->terminator.as.branch.else_target)) {
                        free(builder.data);
                        return 0;
                    }
                } else {
                    free(builder.data);
                    return 0;
                }
            }
        }

        if (!ir_append_string(&builder, "}\n")) {
            free(builder.data);
            return 0;
        }
    }

    if (!builder.data) {
        builder.data = (char *)malloc(1);
        if (!builder.data) {
            return 0;
        }
        builder.data[0] = '\0';
    }

    *out_text = builder.data;
    return 1;
}

#endif