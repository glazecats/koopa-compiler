#if defined(__INTELLISENSE__) && !defined(LOWER_IR_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(LOWER_IR_SPLIT_AGGREGATOR)
#define LOWER_IR_SPLIT_INTELLISENSE_CONTEXT
#include "lower_ir.c"
#else

static LowerIrBinaryOp lower_ir_binary_op_from_ir(IrBinaryOp op) {
    return (LowerIrBinaryOp)op;
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

static int lower_ir_lower_instruction(const IrInstruction *instruction,
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
        lowered_instruction.has_result = 1;
        lowered_instruction.result = lower_ir_value_temp(instruction->result.id);
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

        target_global->has_initializer = source_global->has_initializer;
        target_global->initializer_value = source_global->initializer_value;
        target_global->has_runtime_initializer = source_global->has_runtime_initializer;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const IrFunction *source_function = &program->functions[function_index];
        LowerIrFunction *target_function = NULL;
        size_t local_index;
        size_t block_index;

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

            if (!lower_ir_function_append_local(target_function,
                    source_local->source_name,
                    source_local->is_parameter,
                    NULL,
                    error)) {
                lower_ir_program_free(out_program);
                return 0;
            }
        }

        if (!source_function->has_body) {
            continue;
        }

        for (block_index = 0; block_index < source_function->block_count; ++block_index) {
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

            for (instruction_index = 0;
                 instruction_index < source_block->instruction_count;
                 ++instruction_index) {
                if (!lower_ir_lower_instruction(&source_block->instructions[instruction_index],
                        target_function,
                        target_block,
                        error)) {
                    lower_ir_program_free(out_program);
                    return 0;
                }
            }

            if (!lower_ir_lower_terminator(&source_block->terminator,
                    target_function,
                    target_block,
                    error)) {
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
