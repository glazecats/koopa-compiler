#if defined(__INTELLISENSE__) && !defined(LOWER_IR_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(LOWER_IR_SPLIT_AGGREGATOR)
#define LOWER_IR_SPLIT_INTELLISENSE_CONTEXT
#include "lower_ir.c"
#else

static int lower_ir_trace_enabled(void) {
    const char *flag = getenv("LOWER_IR_TRACE");

    return flag && flag[0] != '\0' && strcmp(flag, "0") != 0;
}

static LowerIrBinaryOp lower_ir_binary_op_from_ir(IrBinaryOp op) {
    return (LowerIrBinaryOp)op;
}

static int lower_ir_builtin_call_returns_void(const char *callee_name) {
    if (!callee_name || callee_name[0] == '\0') {
        return 0;
    }
    return strcmp(callee_name, "putint") == 0 ||
        strcmp(callee_name, "putch") == 0 ||
        strcmp(callee_name, "putarray") == 0 ||
        strcmp(callee_name, "starttime") == 0 ||
        strcmp(callee_name, "stoptime") == 0;
}

static int lower_ir_call_returns_void(const IrProgram *program, const IrInstruction *instruction) {
    size_t function_index;

    if (!instruction || instruction->kind != IR_INSTR_CALL ||
        !instruction->as.call.callee_name || instruction->as.call.callee_name[0] == '\0') {
        return 0;
    }
    if (lower_ir_builtin_call_returns_void(instruction->as.call.callee_name)) {
        return 1;
    }
    if (!program || !program->functions) {
        return 0;
    }
    for (function_index = 0u; function_index < program->function_count; ++function_index) {
        const IrFunction *callee = &program->functions[function_index];

        if (!callee->name || strcmp(callee->name, instruction->as.call.callee_name) != 0) {
            continue;
        }
        return callee->return_type == AST_FUNCTION_RETURN_VOID;
    }
    return 0;
}

static int lower_ir_fn_make_target_returns_void(const IrProgram *program, const IrInstruction *instruction) {
    size_t function_index;

    if (!instruction || instruction->kind != IR_INSTR_FN_MAKE ||
        !instruction->as.fn_make.callee_name || instruction->as.fn_make.callee_name[0] == '\0') {
        return 0;
    }
    if (!program || !program->functions) {
        return 0;
    }
    for (function_index = 0u; function_index < program->function_count; ++function_index) {
        const IrFunction *callee = &program->functions[function_index];

        if (!callee->name || strcmp(callee->name, instruction->as.fn_make.callee_name) != 0) {
            continue;
        }
        return callee->return_type == AST_FUNCTION_RETURN_VOID;
    }
    return 0;
}

static const IrInstruction *lower_ir_find_temp_definition_instruction(const IrFunction *function,
    size_t temp_id) {
    size_t block_index;

    if (!function || !function->blocks) {
        return NULL;
    }

    for (block_index = 0u; block_index < function->block_count; ++block_index) {
        const IrBasicBlock *block = &function->blocks[block_index];
        size_t instruction_index;

        if (!block->instructions) {
            continue;
        }
        for (instruction_index = 0u;
             instruction_index < block->instruction_count;
             ++instruction_index) {
            const IrInstruction *instruction = &block->instructions[instruction_index];

            if (instruction->result.kind == IR_VALUE_TEMP &&
                instruction->result.id == temp_id) {
                return instruction;
            }
        }
    }

    return NULL;
}

static const IrInstruction *lower_ir_resolve_callable_object_producer_instruction(const IrFunction *function,
    size_t temp_id) {
    const IrInstruction *producer = lower_ir_find_temp_definition_instruction(function, temp_id);

    while (producer && producer->kind == IR_INSTR_MOV &&
           producer->as.mov_value.kind == IR_VALUE_TEMP) {
        producer = lower_ir_find_temp_definition_instruction(function, producer->as.mov_value.id);
    }

    return producer;
}

static int lower_ir_lower_value_ref(const IrValueRef *value,
    LowerIrFunction *out_function,
    LowerIrBasicBlock *out_block,
    LowerIrValueRef *out_value,
    LowerIrError *error) {
    LowerIrInstruction instruction;
    size_t temp_id;

    if (!value || !out_function || !out_block || !out_value) {
        lower_ir_set_error(error, 0, 0, "LOWER-IR-025: invalid value-lowering contract");
        return 0;
    }

    switch (value->kind) {
    case IR_VALUE_IMMEDIATE:
        *out_value = lower_ir_value_immediate(value->immediate);
        return 1;
    case IR_VALUE_TEMP:
        *out_value = lower_ir_value_temp(value->id);
        return 1;
    case IR_VALUE_LOCAL:
    case IR_VALUE_GLOBAL:
        temp_id = lower_ir_function_allocate_temp(out_function);
        if (temp_id == (size_t)-1) {
            lower_ir_set_error(error, 0, 0, "LOWER-IR-026: temp allocation overflow while lowering value ref");
            return 0;
        }

        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = (value->kind == IR_VALUE_LOCAL)
            ? LOWER_IR_INSTR_LOAD_LOCAL
            : LOWER_IR_INSTR_LOAD_GLOBAL;
        instruction.has_result = 1;
        instruction.result = lower_ir_value_temp(temp_id);
        instruction.as.load_slot = (value->kind == IR_VALUE_LOCAL)
            ? lower_ir_slot_local(value->id)
            : lower_ir_slot_global(value->id);

        if (!lower_ir_block_append_instruction(out_block, &instruction, error)) {
            return 0;
        }

        *out_value = lower_ir_value_temp(temp_id);
        return 1;
    default:
        lower_ir_set_error(error,
            0,
            0,
            "LOWER-IR-027: unsupported canonical value kind %d during lowering",
            (int)value->kind);
        return 0;
    }
}

static int lower_ir_lower_store_like_mov(const IrInstruction *instruction,
    LowerIrFunction *out_function,
    LowerIrBasicBlock *out_block,
    LowerIrError *error) {
    LowerIrValueRef lowered_value;
    LowerIrInstruction lowered_instruction;

    if (!instruction || !out_function || !out_block) {
        lower_ir_set_error(error, 0, 0, "LOWER-IR-028: invalid mov-lowering contract");
        return 0;
    }

    if (!lower_ir_lower_value_ref(&instruction->as.mov_value,
            out_function,
            out_block,
            &lowered_value,
            error)) {
        return 0;
    }

    memset(&lowered_instruction, 0, sizeof(lowered_instruction));
    switch (instruction->result.kind) {
    case IR_VALUE_TEMP:
        lowered_instruction.kind = LOWER_IR_INSTR_MOV;
        lowered_instruction.has_result = 1;
        lowered_instruction.result = lower_ir_value_temp(instruction->result.id);
        lowered_instruction.as.mov_value = lowered_value;
        return lower_ir_block_append_instruction(out_block, &lowered_instruction, error);
    case IR_VALUE_LOCAL:
        lowered_instruction.kind = LOWER_IR_INSTR_STORE_LOCAL;
        lowered_instruction.has_result = 0;
        lowered_instruction.as.store.slot = lower_ir_slot_local(instruction->result.id);
        lowered_instruction.as.store.value = lowered_value;
        return lower_ir_block_append_instruction(out_block, &lowered_instruction, error);
    case IR_VALUE_GLOBAL:
        lowered_instruction.kind = LOWER_IR_INSTR_STORE_GLOBAL;
        lowered_instruction.has_result = 0;
        lowered_instruction.as.store.slot = lower_ir_slot_global(instruction->result.id);
        lowered_instruction.as.store.value = lowered_value;
        return lower_ir_block_append_instruction(out_block, &lowered_instruction, error);
    default:
        lower_ir_set_error(error,
            0,
            0,
            "LOWER-IR-029: mov result kind %d is not lowerable",
            (int)instruction->result.kind);
        return 0;
    }
}

static int lower_ir_emit_temp_placeholder(LowerIrBasicBlock *out_block,
    size_t temp_id,
    LowerIrError *error) {
    LowerIrInstruction lowered_instruction;

    if (!out_block) {
        lower_ir_set_error(error, 0, 0, "LOWER-IR-046: invalid placeholder-lowering contract");
        return 0;
    }

    memset(&lowered_instruction, 0, sizeof(lowered_instruction));
    lowered_instruction.kind = LOWER_IR_INSTR_MOV;
    lowered_instruction.has_result = 1;
    lowered_instruction.result = lower_ir_value_temp(temp_id);
    lowered_instruction.as.mov_value = lower_ir_value_immediate(0);
    return lower_ir_block_append_instruction(out_block, &lowered_instruction, error);
}

static int lower_ir_lower_instruction(const IrInstruction *instruction,
    const IrProgram *program,
    const IrFunction *source_function,
    LowerIrFunction *out_function,
    LowerIrBasicBlock *out_block,
    LowerIrError *error) {
    LowerIrInstruction lowered_instruction;
    LowerIrValueRef lhs;
    LowerIrValueRef rhs;
    size_t arg_index;

    if (!instruction || !out_function || !out_block) {
        lower_ir_set_error(error, 0, 0, "LOWER-IR-030: invalid instruction-lowering contract");
        return 0;
    }

    switch (instruction->kind) {
    case IR_INSTR_MOV:
        return lower_ir_lower_store_like_mov(instruction, out_function, out_block, error);
    case IR_INSTR_BINARY:
        if (!lower_ir_lower_value_ref(&instruction->as.binary.lhs,
                out_function,
                out_block,
                &lhs,
                error) ||
            !lower_ir_lower_value_ref(&instruction->as.binary.rhs,
                out_function,
                out_block,
                &rhs,
                error)) {
            return 0;
        }

        memset(&lowered_instruction, 0, sizeof(lowered_instruction));
        lowered_instruction.kind = LOWER_IR_INSTR_BINARY;
        lowered_instruction.has_result = 1;
        lowered_instruction.result = lower_ir_value_temp(instruction->result.id);
        lowered_instruction.as.binary.op = lower_ir_binary_op_from_ir(instruction->as.binary.op);
        lowered_instruction.as.binary.lhs = lhs;
        lowered_instruction.as.binary.rhs = rhs;
        return lower_ir_block_append_instruction(out_block, &lowered_instruction, error);
    case IR_INSTR_CALL:
        memset(&lowered_instruction, 0, sizeof(lowered_instruction));
        lowered_instruction.kind = LOWER_IR_INSTR_CALL;
        lowered_instruction.has_result = lower_ir_call_returns_void(program, instruction) ? 0 : 1;
        if (lowered_instruction.has_result) {
            lowered_instruction.result = lower_ir_value_temp(instruction->result.id);
        }
        lowered_instruction.as.call.callee_name = instruction->as.call.callee_name;
        lowered_instruction.as.call.arg_count = instruction->as.call.arg_count;

        if (instruction->as.call.arg_count > 0) {
            lowered_instruction.as.call.args = (LowerIrValueRef *)malloc(
                instruction->as.call.arg_count * sizeof(LowerIrValueRef));
            if (!lowered_instruction.as.call.args) {
                lower_ir_set_error(error, 0, 0, "LOWER-IR-031: out of memory lowering call args");
                return 0;
            }
        }

        for (arg_index = 0; arg_index < instruction->as.call.arg_count; ++arg_index) {
            if (!lower_ir_lower_value_ref(&instruction->as.call.args[arg_index],
                    out_function,
                    out_block,
                    &lowered_instruction.as.call.args[arg_index],
                    error)) {
                free(lowered_instruction.as.call.args);
                lowered_instruction.as.call.args = NULL;
                return 0;
            }
        }

        if (!lower_ir_block_append_instruction(out_block, &lowered_instruction, error)) {
            free(lowered_instruction.as.call.args);
            lowered_instruction.as.call.args = NULL;
            return 0;
        }

        free(lowered_instruction.as.call.args);
        lowered_instruction.as.call.args = NULL;
        return 1;
    case IR_INSTR_FN_MAKE:
        if (instruction->result.kind == IR_VALUE_TEMP) {
            return lower_ir_emit_temp_placeholder(out_block, instruction->result.id, error);
        }
        return 1;
    case IR_INSTR_FN_CODE:
        {
            const IrInstruction *producer;

            if (instruction->as.fn_project_operand.kind != IR_VALUE_TEMP) {
                lower_ir_set_error(error,
                    0,
                    0,
                    "LOWER-IR-048: fn_code operand must be a temp before lower-IR bridge");
                return 0;
            }
            producer = lower_ir_resolve_callable_object_producer_instruction(
                source_function,
                instruction->as.fn_project_operand.id);
            if (!producer || producer->kind != IR_INSTR_FN_MAKE ||
                !producer->as.fn_make.callee_name || producer->as.fn_make.callee_name[0] == '\0') {
                lower_ir_set_error(error,
                    0,
                    0,
                    "LOWER-IR-049: fn_code bridge requires operand resolving to named fn_make");
                return 0;
            }

            memset(&lowered_instruction, 0, sizeof(lowered_instruction));
            lowered_instruction.kind = LOWER_IR_INSTR_ADDR_FUNCTION;
            lowered_instruction.has_result = 1;
            lowered_instruction.result = lower_ir_value_temp(instruction->result.id);
            lowered_instruction.as.addr_function_name =
                lower_ir_strdup(producer->as.fn_make.callee_name);
            if (!lowered_instruction.as.addr_function_name) {
                lower_ir_set_error(error,
                    0,
                    0,
                    "LOWER-IR-050: out of memory lowering fn_code function name");
                return 0;
            }
            if (!lower_ir_block_append_instruction(out_block, &lowered_instruction, error)) {
                free(lowered_instruction.as.addr_function_name);
                lowered_instruction.as.addr_function_name = NULL;
                return 0;
            }
            free(lowered_instruction.as.addr_function_name);
            lowered_instruction.as.addr_function_name = NULL;
            return 1;
        }
    case IR_INSTR_FN_ENV:
    case IR_INSTR_FN_SHAPE:
        {
            const IrInstruction *producer;

            if (instruction->as.fn_project_operand.kind != IR_VALUE_TEMP) {
                lower_ir_set_error(error,
                    0,
                    0,
                    "LOWER-IR-046: callable projection operand must be a temp before lower-IR bridge");
                return 0;
            }

            producer = lower_ir_resolve_callable_object_producer_instruction(
                source_function,
                instruction->as.fn_project_operand.id);
            if (!producer || producer->kind != IR_INSTR_FN_MAKE) {
                lower_ir_set_error(error,
                    0,
                    0,
                    "LOWER-IR-047: callable projection bridge requires operand resolving to fn_make");
                return 0;
            }

            memset(&lowered_instruction, 0, sizeof(lowered_instruction));
            lowered_instruction.kind = LOWER_IR_INSTR_MOV;
            lowered_instruction.has_result = 1;
            lowered_instruction.result = lower_ir_value_temp(instruction->result.id);

            if (instruction->kind == IR_INSTR_FN_ENV) {
                if (!lower_ir_lower_value_ref(&producer->as.fn_make.env,
                        out_function,
                        out_block,
                        &lowered_instruction.as.mov_value,
                        error)) {
                    return 0;
                }
            } else {
                lowered_instruction.as.mov_value =
                    lower_ir_value_immediate((long long)producer->as.fn_make.shape_id);
            }

            return lower_ir_block_append_instruction(out_block, &lowered_instruction, error);
        }
    case IR_INSTR_CALL_INDIRECT:
        {
            const IrInstruction *producer;
            size_t lowered_arg_count;

            if (instruction->as.call_indirect.callee.kind != IR_VALUE_TEMP) {
                lower_ir_set_error(error,
                    0,
                    0,
                    "LOWER-IR-043: call_indirect callee must be a temp before lower-IR bridge");
                return 0;
            }

            producer = lower_ir_resolve_callable_object_producer_instruction(
                source_function,
                instruction->as.call_indirect.callee.id);
            if (!producer || producer->kind != IR_INSTR_FN_MAKE) {
                lower_ir_set_error(error,
                    0,
                    0,
                    "LOWER-IR-044: call_indirect bridge requires callee resolving to fn_make");
                return 0;
            }

            memset(&lowered_instruction, 0, sizeof(lowered_instruction));
            lowered_instruction.kind = LOWER_IR_INSTR_CALL;
            lowered_instruction.has_result =
                lower_ir_fn_make_target_returns_void(program, producer) ? 0 : 1;
            if (lowered_instruction.has_result) {
                lowered_instruction.result = lower_ir_value_temp(instruction->result.id);
            }
            lowered_instruction.as.call.callee_name = producer->as.fn_make.callee_name;
            lowered_instruction.as.call.arg_count = instruction->as.call_indirect.arg_count + 1u;

            lowered_instruction.as.call.args = (LowerIrValueRef *)malloc(
                lowered_instruction.as.call.arg_count * sizeof(LowerIrValueRef));
            if (!lowered_instruction.as.call.args) {
                lower_ir_set_error(error,
                    0,
                    0,
                    "LOWER-IR-045: out of memory lowering bridged call_indirect args");
                return 0;
            }

            if (!lower_ir_lower_value_ref(&producer->as.fn_make.env,
                    out_function,
                    out_block,
                    &lowered_instruction.as.call.args[0],
                    error)) {
                free(lowered_instruction.as.call.args);
                lowered_instruction.as.call.args = NULL;
                return 0;
            }

            for (lowered_arg_count = 0u;
                 lowered_arg_count < instruction->as.call_indirect.arg_count;
                 ++lowered_arg_count) {
                if (!lower_ir_lower_value_ref(&instruction->as.call_indirect.args[lowered_arg_count],
                        out_function,
                        out_block,
                        &lowered_instruction.as.call.args[lowered_arg_count + 1u],
                        error)) {
                    free(lowered_instruction.as.call.args);
                    lowered_instruction.as.call.args = NULL;
                    return 0;
                }
            }

            if (!lower_ir_block_append_instruction(out_block, &lowered_instruction, error)) {
                free(lowered_instruction.as.call.args);
                lowered_instruction.as.call.args = NULL;
                return 0;
            }

            free(lowered_instruction.as.call.args);
            lowered_instruction.as.call.args = NULL;
            return 1;
        }
    case IR_INSTR_ADDR_LOCAL:
        memset(&lowered_instruction, 0, sizeof(lowered_instruction));
        lowered_instruction.kind = LOWER_IR_INSTR_ADDR_LOCAL;
        lowered_instruction.has_result = 1;
        lowered_instruction.result = lower_ir_value_temp(instruction->result.id);
        lowered_instruction.as.addr_slot = lower_ir_slot_local(instruction->as.addr_local_id);
        return lower_ir_block_append_instruction(out_block, &lowered_instruction, error);
    case IR_INSTR_ADDR_GLOBAL:
        memset(&lowered_instruction, 0, sizeof(lowered_instruction));
        lowered_instruction.kind = LOWER_IR_INSTR_ADDR_GLOBAL;
        lowered_instruction.has_result = 1;
        lowered_instruction.result = lower_ir_value_temp(instruction->result.id);
        lowered_instruction.as.addr_slot = lower_ir_slot_global(instruction->as.addr_global_id);
        return lower_ir_block_append_instruction(out_block, &lowered_instruction, error);
    case IR_INSTR_LOAD_INDIRECT:
        memset(&lowered_instruction, 0, sizeof(lowered_instruction));
        lowered_instruction.kind = LOWER_IR_INSTR_LOAD_INDIRECT;
        lowered_instruction.has_result = 1;
        lowered_instruction.result = lower_ir_value_temp(instruction->result.id);
        if (!lower_ir_lower_value_ref(&instruction->as.load_indirect_addr,
                out_function,
                out_block,
                &lowered_instruction.as.load_indirect_addr,
                error)) {
            return 0;
        }
        return lower_ir_block_append_instruction(out_block, &lowered_instruction, error);
    case IR_INSTR_STORE_INDIRECT:
        memset(&lowered_instruction, 0, sizeof(lowered_instruction));
        lowered_instruction.kind = LOWER_IR_INSTR_STORE_INDIRECT;
        lowered_instruction.has_result = 0;
        if (!lower_ir_lower_value_ref(&instruction->as.store_indirect.addr,
                out_function,
                out_block,
                &lowered_instruction.as.store_indirect.addr,
                error) ||
            !lower_ir_lower_value_ref(&instruction->as.store_indirect.value,
                out_function,
                out_block,
                &lowered_instruction.as.store_indirect.value,
                error)) {
            return 0;
        }
        return lower_ir_block_append_instruction(out_block, &lowered_instruction, error);
    default:
        lower_ir_set_error(error,
            0,
            0,
            "LOWER-IR-032: unsupported canonical instruction kind %d",
            (int)instruction->kind);
        return 0;
    }
}

static int lower_ir_lower_terminator(const IrTerminator *terminator,
    LowerIrFunction *out_function,
    LowerIrBasicBlock *out_block,
    LowerIrError *error) {
    LowerIrValueRef lowered_value;

    if (!terminator || !out_function || !out_block) {
        lower_ir_set_error(error, 0, 0, "LOWER-IR-033: invalid terminator-lowering contract");
        return 0;
    }

    switch (terminator->kind) {
    case IR_TERM_RETURN:
        if (!terminator->has_return_value) {
            return lower_ir_block_set_void_return(out_block, error);
        }
        if (!lower_ir_lower_value_ref(&terminator->as.return_value,
                out_function,
                out_block,
                &lowered_value,
                error)) {
            return 0;
        }
        return lower_ir_block_set_return(out_block, lowered_value, error);
    case IR_TERM_JUMP:
        return lower_ir_block_set_jump(out_block, terminator->as.jump_target, error);
    case IR_TERM_BRANCH:
        if (!lower_ir_lower_value_ref(&terminator->as.branch.condition,
                out_function,
                out_block,
                &lowered_value,
                error)) {
            return 0;
        }
        return lower_ir_block_set_branch(out_block,
            lowered_value,
            terminator->as.branch.then_target,
            terminator->as.branch.else_target,
            error);
    default:
        lower_ir_set_error(error,
            0,
            0,
            "LOWER-IR-034: unsupported canonical terminator kind %d",
            (int)terminator->kind);
        return 0;
    }
}

int lower_ir_lower_from_ir(const IrProgram *program,
    const LowerIrOptions *options,
    LowerIrProgram *out_program,
    LowerIrError *error) {
    IrError ir_error;
    LowerIrError verify_error;
    size_t function_index;
    size_t global_index;

    if (!program || !out_program) {
        lower_ir_set_error(error, 0, 0, "LOWER-IR-035: invalid lower-from-ir contract");
        return 0;
    }

    lower_ir_program_init(out_program);

    if (!ir_verify_program(program, &ir_error)) {
        lower_ir_set_error(error,
            ir_error.line,
            ir_error.column,
            "LOWER-IR-036: canonical input IR failed verifier: %s",
            ir_error.message);
        return 0;
    }

    for (global_index = 0; global_index < program->global_count; ++global_index) {
        const IrGlobal *source_global = &program->globals[global_index];
        LowerIrGlobal *target_global = NULL;

        if (!lower_ir_program_append_global(out_program, source_global->name, &target_global, error)) {
            lower_ir_program_free(out_program);
            return 0;
        }

        target_global->value_type = source_global->value_type;
        target_global->byte_size = source_global->byte_size;
        target_global->has_initializer = source_global->has_initializer;
        target_global->initializer_value = source_global->initializer_value;
        target_global->has_runtime_initializer = source_global->has_runtime_initializer;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const IrFunction *source_function = &program->functions[function_index];
        LowerIrFunction *target_function = NULL;
        size_t local_index;
        size_t block_index;

        if (lower_ir_trace_enabled()) {
            fprintf(stderr,
                "[lower-ir-trace] function[%zu] name=%s has_body=%d locals=%zu blocks=%zu next_temp=%zu\n",
                function_index,
                source_function->name ? source_function->name : "<anon>",
                source_function->has_body,
                source_function->local_count,
                source_function->block_count,
                source_function->next_temp_id);
        }

        if (!lower_ir_program_append_function(out_program,
                source_function->name,
                source_function->has_body,
                &target_function,
                error)) {
            lower_ir_program_free(out_program);
            return 0;
        }

        for (local_index = 0; local_index < source_function->local_count; ++local_index) {
            const IrLocal *source_local = &source_function->locals[local_index];

            if (lower_ir_trace_enabled() && ((local_index % 64u) == 0u || local_index + 1u == source_function->local_count)) {
                fprintf(stderr,
                    "[lower-ir-trace]   local[%zu/%zu] name=%s is_param=%d array_rank=%zu\n",
                    local_index,
                    source_function->local_count,
                    source_local->source_name ? source_local->source_name : "<null>",
                    source_local->is_parameter,
                    source_local->array_rank);
            }

            if (!lower_ir_function_append_local(target_function,
                    source_local->source_name,
                    source_local->is_parameter,
                    NULL,
                    error)) {
                lower_ir_program_free(out_program);
                return 0;
            }
            target_function->locals[local_index].value_type = source_local->value_type;
            target_function->locals[local_index].array_rank = source_local->array_rank;
        }

        if (!source_function->has_body) {
            continue;
        }

        for (block_index = 0; block_index < source_function->block_count; ++block_index) {
            if (lower_ir_trace_enabled()) {
                fprintf(stderr,
                    "[lower-ir-trace]   append-block[%zu/%zu]\n",
                    block_index,
                    source_function->block_count);
            }
            if (!lower_ir_function_append_block(target_function, NULL, NULL, error)) {
                lower_ir_program_free(out_program);
                return 0;
            }
        }

        target_function->next_temp_id = source_function->next_temp_id;

        for (block_index = 0; block_index < source_function->block_count; ++block_index) {
            const IrBasicBlock *source_block = &source_function->blocks[block_index];
            LowerIrBasicBlock *target_block = &target_function->blocks[block_index];
            size_t instruction_index;

            if (lower_ir_trace_enabled()) {
                fprintf(stderr,
                    "[lower-ir-trace]   lower-block[%zu] instrs=%zu has_term=%d\n",
                    block_index,
                    source_block->instruction_count,
                    source_block->has_terminator);
            }

            for (instruction_index = 0;
                 instruction_index < source_block->instruction_count;
                 ++instruction_index) {
                if (lower_ir_trace_enabled() &&
                    ((instruction_index % 128u) == 0u || instruction_index + 1u == source_block->instruction_count)) {
                    fprintf(stderr,
                        "[lower-ir-trace]     instr[%zu/%zu] kind=%d\n",
                        instruction_index,
                        source_block->instruction_count,
                        (int)source_block->instructions[instruction_index].kind);
                }
                if (!lower_ir_lower_instruction(&source_block->instructions[instruction_index],
                        program,
                        source_function,
                        target_function,
                        target_block,
                        error)) {
                    lower_ir_program_free(out_program);
                    return 0;
                }
            }

            if (source_block->has_terminator) {
                if (!lower_ir_lower_terminator(&source_block->terminator,
                        target_function,
                        target_block,
                        error)) {
                    lower_ir_program_free(out_program);
                    return 0;
                }
            } else if (options && options->allow_implicit_fallthrough_return) {
                if (!lower_ir_block_set_return(target_block, lower_ir_value_immediate(0), error)) {
                    lower_ir_program_free(out_program);
                    return 0;
                }
            } else {
                lower_ir_set_error(error,
                    0,
                    0,
                    "LOWER-IR-042: function '%s' body does not terminate on all lowered paths",
                    source_function->name ? source_function->name : "<anon>");
                lower_ir_program_free(out_program);
                return 0;
            }
        }
    }

    if (!lower_ir_verify_program(out_program, &verify_error)) {
        lower_ir_set_error(error,
            verify_error.line,
            verify_error.column,
            "LOWER-IR-041: lowered output IR failed verifier: %s",
            verify_error.message);
        lower_ir_program_free(out_program);
        return 0;
    }

    return 1;
}

#endif
