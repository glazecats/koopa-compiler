#if defined(__INTELLISENSE__) && !defined(IR_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(IR_SPLIT_AGGREGATOR)
#define IR_SPLIT_INTELLISENSE_CONTEXT
#include "ir.c"
#else

static void ir_basic_block_free(IrBasicBlock *block) {
    size_t i;

    if (!block) {
        return;
    }

    for (i = 0; i < block->instruction_count; ++i) {
        if (block->instructions[i].kind == IR_INSTR_CALL) {
            free(block->instructions[i].as.call.callee_name);
            free(block->instructions[i].as.call.args);
        }
    }

    free(block->instructions);
    block->instructions = NULL;
    block->instruction_count = 0;
    block->instruction_capacity = 0;
    block->has_terminator = 0;
}

static void ir_function_free(IrFunction *function) {
    size_t i;

    if (!function) {
        return;
    }

    free(function->name);
    function->name = NULL;
    function->has_body = 0;

    for (i = 0; i < function->local_count; ++i) {
        free(function->locals[i].source_name);
    }
    free(function->locals);
    function->locals = NULL;
    function->local_count = 0;
    function->local_capacity = 0;

    for (i = 0; i < function->block_count; ++i) {
        ir_basic_block_free(&function->blocks[i]);
    }
    free(function->blocks);
    function->blocks = NULL;
    function->block_count = 0;
    function->block_capacity = 0;
    function->parameter_count = 0;
    function->next_temp_id = 0;
}

static void ir_global_free(IrGlobal *global) {
    if (!global) {
        return;
    }

    free(global->name);
    global->name = NULL;
    global->has_initializer = 0;
    global->initializer_value = 0;
    global->has_runtime_initializer = 0;
}

void ir_program_init(IrProgram *program) {
    if (!program) {
        return;
    }

    program->globals = NULL;
    program->global_count = 0;
    program->global_capacity = 0;
    program->functions = NULL;
    program->function_count = 0;
    program->function_capacity = 0;
}

void ir_program_free(IrProgram *program) {
    size_t i;

    if (!program) {
        return;
    }

    for (i = 0; i < program->global_count; ++i) {
        ir_global_free(&program->globals[i]);
    }
    free(program->globals);
    program->globals = NULL;
    program->global_count = 0;
    program->global_capacity = 0;

    for (i = 0; i < program->function_count; ++i) {
        ir_function_free(&program->functions[i]);
    }
    free(program->functions);
    program->functions = NULL;
    program->function_count = 0;
    program->function_capacity = 0;
}

static IrGlobal *ir_program_find_global_mut(IrProgram *program, const char *name) {
    size_t i;

    if (!program || !name || name[0] == '\0') {
        return NULL;
    }

    for (i = 0; i < program->global_count; ++i) {
        IrGlobal *global = &program->globals[i];

        if (global->name && strcmp(global->name, name) == 0) {
            return global;
        }
    }

    return NULL;
}

static const IrGlobal *ir_program_find_global(const IrProgram *program, const char *name) {
    size_t i;

    if (!program || !name || name[0] == '\0') {
        return NULL;
    }

    for (i = 0; i < program->global_count; ++i) {
        const IrGlobal *global = &program->globals[i];

        if (global->name && strcmp(global->name, name) == 0) {
            return global;
        }
    }

    return NULL;
}

static const IrGlobal *ir_program_get_global(const IrProgram *program, size_t global_id) {
    if (!program || global_id >= program->global_count) {
        return NULL;
    }

    return &program->globals[global_id];
}

static int ir_program_append_global(IrProgram *program,
    const char *name,
    IrGlobal **out_global,
    IrError *error) {
    IrGlobal *new_globals;
    size_t next_capacity;
    IrGlobal *global;
    size_t name_length;

    if (!program || !name || name[0] == '\0') {
        ir_set_error(error, 0, 0, "IR-INT-032: invalid global append contract");
        return 0;
    }

    if (program->global_count == program->global_capacity) {
        if (!ir_next_growth_capacity(program->global_capacity,
                4,
                sizeof(IrGlobal),
                &next_capacity)) {
            ir_set_error(error, 0, 0, "IR-INT-033: global storage capacity overflow");
            return 0;
        }
        new_globals = (IrGlobal *)realloc(program->globals,
            next_capacity * sizeof(IrGlobal));
        if (!new_globals) {
            ir_set_error(error, 0, 0, "IR-INT-034: out of memory while growing IR globals");
            return 0;
        }
        program->globals = new_globals;
        program->global_capacity = next_capacity;
    }

    global = &program->globals[program->global_count];
    global->id = program->global_count;
    global->name = NULL;
    global->has_initializer = 0;
    global->initializer_value = 0;
    global->has_runtime_initializer = 0;

    name_length = strlen(name);
    global->name = (char *)malloc(name_length + 1);
    if (!global->name) {
        ir_set_error(error, 0, 0, "IR-INT-035: out of memory while copying global name");
        return 0;
    }
    memcpy(global->name, name, name_length + 1);

    program->global_count += 1;
    if (out_global) {
        *out_global = global;
    }
    return 1;
}

static IrFunction *ir_program_find_function_mut(IrProgram *program, const char *name) {
    size_t i;

    if (!program || !name || name[0] == '\0') {
        return NULL;
    }

    for (i = 0; i < program->function_count; ++i) {
        IrFunction *function = &program->functions[i];

        if (function->name && strcmp(function->name, name) == 0) {
            return function;
        }
    }

    return NULL;
}

static int ir_program_append_function(IrProgram *program,
    const char *name,
    IrFunction **out_function,
    IrError *error) {
    IrFunction *new_functions;
    size_t next_capacity;
    IrFunction *function;
    size_t name_length = 0;

    if (!program || !name) {
        ir_set_error(error, 0, 0, "IR-INT-001: invalid program/function append contract");
        return 0;
    }

    if (program->function_count == program->function_capacity) {
        if (!ir_next_growth_capacity(program->function_capacity,
                4,
                sizeof(IrFunction),
                &next_capacity)) {
            ir_set_error(error, 0, 0, "IR-INT-002: function storage capacity overflow");
            return 0;
        }
        new_functions = (IrFunction *)realloc(program->functions,
            next_capacity * sizeof(IrFunction));
        if (!new_functions) {
            ir_set_error(error, 0, 0, "IR-INT-003: out of memory while growing IR functions");
            return 0;
        }
        program->functions = new_functions;
        program->function_capacity = next_capacity;
    }

    function = &program->functions[program->function_count];
    function->name = NULL;
    function->has_body = 0;
    function->parameter_count = 0;
    function->locals = NULL;
    function->local_count = 0;
    function->local_capacity = 0;
    function->blocks = NULL;
    function->block_count = 0;
    function->block_capacity = 0;
    function->next_temp_id = 0;

    name_length = strlen(name);
    function->name = (char *)malloc(name_length + 1);
    if (!function->name) {
        ir_set_error(error, 0, 0, "IR-INT-004: out of memory while copying function name");
        return 0;
    }
    memcpy(function->name, name, name_length + 1);

    program->function_count++;
    if (out_function) {
        *out_function = function;
    }
    return 1;
}

static int ir_function_append_local(IrFunction *function,
    const char *source_name,
    int is_parameter,
    size_t *out_local_id,
    IrError *error) {
    IrLocal *new_locals;
    size_t next_capacity;
    IrLocal *local;
    size_t local_id;
    size_t name_length;

    if (!function || !source_name) {
        ir_set_error(error, 0, 0, "IR-INT-005: invalid local append contract");
        return 0;
    }

    if (function->local_count == function->local_capacity) {
        if (!ir_next_growth_capacity(function->local_capacity,
                4,
                sizeof(IrLocal),
                &next_capacity)) {
            ir_set_error(error, 0, 0, "IR-INT-006: local storage capacity overflow");
            return 0;
        }
        new_locals = (IrLocal *)realloc(function->locals, next_capacity * sizeof(IrLocal));
        if (!new_locals) {
            ir_set_error(error, 0, 0, "IR-INT-007: out of memory while growing IR locals");
            return 0;
        }
        function->locals = new_locals;
        function->local_capacity = next_capacity;
    }

    local_id = function->local_count;
    local = &function->locals[local_id];
    local->id = local_id;
    local->source_name = NULL;
    local->is_parameter = is_parameter;

    name_length = strlen(source_name);
    local->source_name = (char *)malloc(name_length + 1);
    if (!local->source_name) {
        ir_set_error(error, 0, 0, "IR-INT-008: out of memory while copying local name");
        return 0;
    }
    memcpy(local->source_name, source_name, name_length + 1);

    function->local_count++;
    if (is_parameter) {
        function->parameter_count++;
    }
    if (out_local_id) {
        *out_local_id = local_id;
    }
    return 1;
}

static int ir_function_append_block(IrFunction *function,
    size_t *out_block_id,
    IrBasicBlock **out_block,
    IrError *error) {
    IrBasicBlock *new_blocks;
    size_t next_capacity;
    IrBasicBlock *block;
    size_t block_id;

    if (!function) {
        ir_set_error(error, 0, 0, "IR-INT-009: invalid block append contract");
        return 0;
    }

    if (function->block_count == function->block_capacity) {
        if (!ir_next_growth_capacity(function->block_capacity,
                4,
                sizeof(IrBasicBlock),
                &next_capacity)) {
            ir_set_error(error, 0, 0, "IR-INT-010: block storage capacity overflow");
            return 0;
        }
        new_blocks = (IrBasicBlock *)realloc(function->blocks,
            next_capacity * sizeof(IrBasicBlock));
        if (!new_blocks) {
            ir_set_error(error, 0, 0, "IR-INT-011: out of memory while growing IR blocks");
            return 0;
        }
        function->blocks = new_blocks;
        function->block_capacity = next_capacity;
    }

    block_id = function->block_count;
    block = &function->blocks[block_id];
    block->id = block_id;
    block->instructions = NULL;
    block->instruction_count = 0;
    block->instruction_capacity = 0;
    block->has_terminator = 0;

    function->block_count++;
    if (out_block_id) {
        *out_block_id = block_id;
    }
    if (out_block) {
        *out_block = block;
    }
    return 1;
}

static int ir_block_append_instruction(IrBasicBlock *block,
    const IrInstruction *instruction,
    IrError *error) {
    IrInstruction *new_instructions;
    size_t next_capacity;

    if (!block || !instruction) {
        ir_set_error(error, 0, 0, "IR-INT-012: invalid instruction append contract");
        return 0;
    }

    if (block->instruction_count == block->instruction_capacity) {
        if (!ir_next_growth_capacity(block->instruction_capacity,
                8,
                sizeof(IrInstruction),
                &next_capacity)) {
            ir_set_error(error, 0, 0, "IR-INT-013: instruction storage capacity overflow");
            return 0;
        }
        new_instructions = (IrInstruction *)realloc(block->instructions,
            next_capacity * sizeof(IrInstruction));
        if (!new_instructions) {
            ir_set_error(error, 0, 0, "IR-INT-014: out of memory while growing IR instructions");
            return 0;
        }
        block->instructions = new_instructions;
        block->instruction_capacity = next_capacity;
    }

    block->instructions[block->instruction_count++] = *instruction;
    return 1;
}

static int ir_block_prepend_instruction(IrBasicBlock *block,
    const IrInstruction *instruction,
    IrError *error) {
    IrInstruction *new_instructions;
    size_t next_capacity;

    if (!block || !instruction) {
        ir_set_error(error, 0, 0, "IR-INT-039: invalid instruction prepend contract");
        return 0;
    }

    if (block->instruction_count == block->instruction_capacity) {
        if (!ir_next_growth_capacity(block->instruction_capacity,
                8,
                sizeof(IrInstruction),
                &next_capacity)) {
            ir_set_error(error, 0, 0, "IR-INT-040: instruction storage capacity overflow on prepend");
            return 0;
        }
        new_instructions = (IrInstruction *)realloc(block->instructions,
            next_capacity * sizeof(IrInstruction));
        if (!new_instructions) {
            ir_set_error(error,
                0,
                0,
                "IR-INT-041: out of memory while growing IR instructions for prepend");
            return 0;
        }
        block->instructions = new_instructions;
        block->instruction_capacity = next_capacity;
    }

    if (block->instruction_count > 0) {
        memmove(block->instructions + 1,
            block->instructions,
            block->instruction_count * sizeof(IrInstruction));
    }
    block->instructions[0] = *instruction;
    block->instruction_count += 1;
    return 1;
}

static int ir_block_set_return(IrBasicBlock *block, IrValueRef value, IrError *error) {
    if (!block) {
        ir_set_error(error, 0, 0, "IR-INT-015: invalid return terminator block");
        return 0;
    }
    if (block->has_terminator) {
        ir_set_error(error, 0, 0, "IR-INT-016: block already has a terminator");
        return 0;
    }

    block->terminator.kind = IR_TERM_RETURN;
    block->terminator.as.return_value = value;
    block->has_terminator = 1;
    return 1;
}

static int ir_block_set_jump(IrBasicBlock *block, size_t target_block_id, IrError *error) {
    if (!block) {
        ir_set_error(error, 0, 0, "IR-INT-017: invalid jump terminator block");
        return 0;
    }
    if (block->has_terminator) {
        ir_set_error(error, 0, 0, "IR-INT-018: block already has a terminator");
        return 0;
    }

    block->terminator.kind = IR_TERM_JUMP;
    block->terminator.as.jump_target = target_block_id;
    block->has_terminator = 1;
    return 1;
}

static int ir_block_set_branch(IrBasicBlock *block,
    IrValueRef condition,
    size_t then_target,
    size_t else_target,
    IrError *error) {
    if (!block) {
        ir_set_error(error, 0, 0, "IR-INT-019: invalid branch terminator block");
        return 0;
    }
    if (block->has_terminator) {
        ir_set_error(error, 0, 0, "IR-INT-020: block already has a terminator");
        return 0;
    }

    block->terminator.kind = IR_TERM_BRANCH;
    block->terminator.as.branch.condition = condition;
    block->terminator.as.branch.then_target = then_target;
    block->terminator.as.branch.else_target = else_target;
    block->has_terminator = 1;
    return 1;
}

static IrBasicBlock *ir_function_get_block_mut(IrFunction *function, size_t block_id) {
    if (!function || block_id >= function->block_count) {
        return NULL;
    }
    return &function->blocks[block_id];
}

static const IrLocal *ir_function_get_local(const IrFunction *function, size_t local_id) {
    if (!function || local_id >= function->local_count) {
        return NULL;
    }
    return &function->locals[local_id];
}

static IrBasicBlock *ir_lower_current_block(IrLowerContext *ctx) {
    if (!ctx || !ctx->function || !ctx->has_block) {
        return NULL;
    }
    return ir_function_get_block_mut(ctx->function, ctx->block_id);
}

static int ir_emit_mov(IrLowerContext *ctx,
    IrValueRef destination,
    IrValueRef value) {
    IrInstruction instruction;
    IrBasicBlock *block = ir_lower_current_block(ctx);

    if (!ctx || !block) {
        return 0;
    }

    instruction.kind = IR_INSTR_MOV;
    instruction.result = destination;
    instruction.as.mov_value = value;
    return ir_block_append_instruction(block, &instruction, ctx->error);
}

static int ir_emit_binary(IrLowerContext *ctx,
    IrValueRef destination,
    IrBinaryOp op,
    IrValueRef lhs,
    IrValueRef rhs) {
    IrInstruction instruction;
    IrBasicBlock *block = ir_lower_current_block(ctx);

    if (!ctx || !block) {
        return 0;
    }

    instruction.kind = IR_INSTR_BINARY;
    instruction.result = destination;
    instruction.as.binary.op = op;
    instruction.as.binary.lhs = lhs;
    instruction.as.binary.rhs = rhs;
    return ir_block_append_instruction(block, &instruction, ctx->error);
}

static int ir_emit_call(IrLowerContext *ctx,
    IrValueRef destination,
    const char *callee_name,
    const IrValueRef *args,
    size_t arg_count) {
    IrInstruction instruction;
    IrBasicBlock *block = ir_lower_current_block(ctx);
    size_t name_length;

    if (!ctx || !block || !callee_name) {
        return 0;
    }

    instruction.kind = IR_INSTR_CALL;
    instruction.result = destination;
    instruction.as.call.callee_name = NULL;
    instruction.as.call.args = NULL;
    instruction.as.call.arg_count = arg_count;

    name_length = strlen(callee_name);
    instruction.as.call.callee_name = (char *)malloc(name_length + 1);
    if (!instruction.as.call.callee_name) {
        ir_set_error(ctx->error, 0, 0, "IR-INT-025: out of memory while copying IR call callee");
        return 0;
    }
    memcpy(instruction.as.call.callee_name, callee_name, name_length + 1);

    if (arg_count > 0) {
        instruction.as.call.args = (IrValueRef *)malloc(arg_count * sizeof(IrValueRef));
        if (!instruction.as.call.args) {
            free(instruction.as.call.callee_name);
            ir_set_error(ctx->error, 0, 0, "IR-INT-026: out of memory while copying IR call arguments");
            return 0;
        }
        memcpy(instruction.as.call.args, args, arg_count * sizeof(IrValueRef));
    }

    if (!ir_block_append_instruction(block, &instruction, ctx->error)) {
        free(instruction.as.call.callee_name);
        free(instruction.as.call.args);
        return 0;
    }

    return 1;
}

static size_t ir_allocate_temp(IrFunction *function) {
    size_t temp_id = function->next_temp_id;
    function->next_temp_id++;
    return temp_id;
}

#endif