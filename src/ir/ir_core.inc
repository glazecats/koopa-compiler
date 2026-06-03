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
        } else if (block->instructions[i].kind == IR_INSTR_FN_MAKE) {
            free(block->instructions[i].as.fn_make.callee_name);
        } else if (block->instructions[i].kind == IR_INSTR_CALL_INDIRECT) {
            free(block->instructions[i].as.call_indirect.args);
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
    free(global->array_extents);
    global->array_extents = NULL;
    global->byte_size = 4u;
    global->has_initializer = 0;
    global->initializer_value = 0;
    global->has_runtime_initializer = 0;
}

static void ir_function_shape_free(IrFunctionShape *shape) {
    if (!shape) {
        return;
    }

    free(shape->parameter_types);
    shape->parameter_types = NULL;
    shape->parameter_count = 0;
}

void ir_program_init(IrProgram *program) {
    if (!program) {
        return;
    }

    program->function_shapes = NULL;
    program->function_shape_count = 0;
    program->function_shape_capacity = 0;
    program->function_external_refs = NULL;
    program->function_external_ref_owned = NULL;
    program->function_external_ref_count = 0;
    program->function_external_ref_capacity = 0;
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

    for (i = 0; i < program->function_shape_count; ++i) {
        ir_function_shape_free(&program->function_shapes[i]);
    }
    free(program->function_shapes);
    program->function_shapes = NULL;
    program->function_shape_count = 0;
    program->function_shape_capacity = 0;

    if (program->function_external_refs && program->function_external_ref_owned) {
        for (i = 0; i < program->function_external_ref_count; ++i) {
            if (program->function_external_ref_owned[i] &&
                program->function_external_refs[i]) {
                AstExternal *owned_external =
                    (AstExternal *)program->function_external_refs[i];
                free(owned_external->parameter_names);
                free(owned_external->parameter_value_kinds);
                free(owned_external->parameter_function_return_types);
                free(owned_external->parameter_function_parameter_counts);
                free(owned_external->parameter_function_parameter_value_kinds);
                free(owned_external->parameter_function_parameter_return_types);
                free(owned_external->parameter_function_parameter_parameter_counts);
                free(owned_external->parameter_name_lines);
                free(owned_external->parameter_name_columns);
                free(owned_external);
            }
        }
    }
    free(program->function_external_refs);
    free(program->function_external_ref_owned);
    program->function_external_refs = NULL;
    program->function_external_ref_owned = NULL;
    program->function_external_ref_count = 0;
    program->function_external_ref_capacity = 0;

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
    global->value_type = AST_FUNCTION_RETURN_INT;
    global->array_rank = 0;
    global->array_extents = NULL;
    global->byte_size = 4u;
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

static int ir_program_append_function_shape(IrProgram *program,
    AstFunctionReturnType return_type,
    const AstFunctionReturnType *parameter_types,
    size_t parameter_count,
    size_t *out_shape_id,
    IrError *error) {
    IrFunctionShape *new_shapes;
    size_t next_capacity;
    IrFunctionShape *shape;

    if (!program || (parameter_count > 0 && !parameter_types)) {
        ir_set_error(error, 0, 0, "IR-INT-042: invalid function shape append contract");
        return 0;
    }

    if (program->function_shape_count == program->function_shape_capacity) {
        if (!ir_next_growth_capacity(program->function_shape_capacity,
                4,
                sizeof(IrFunctionShape),
                &next_capacity)) {
            ir_set_error(error, 0, 0, "IR-INT-043: function shape storage capacity overflow");
            return 0;
        }
        new_shapes = (IrFunctionShape *)realloc(program->function_shapes,
            next_capacity * sizeof(IrFunctionShape));
        if (!new_shapes) {
            ir_set_error(error, 0, 0, "IR-INT-044: out of memory while growing IR function shapes");
            return 0;
        }
        program->function_shapes = new_shapes;
        program->function_shape_capacity = next_capacity;
    }

    shape = &program->function_shapes[program->function_shape_count];
    shape->id = program->function_shape_count;
    shape->return_type = return_type;
    shape->parameter_types = NULL;
    shape->parameter_count = parameter_count;
    if (parameter_count > 0) {
        shape->parameter_types = (AstFunctionReturnType *)malloc(
            parameter_count * sizeof(AstFunctionReturnType));
        if (!shape->parameter_types) {
            ir_set_error(error, 0, 0, "IR-INT-045: out of memory while copying function shape parameters");
            return 0;
        }
        memcpy(shape->parameter_types,
            parameter_types,
            parameter_count * sizeof(AstFunctionReturnType));
    }

    program->function_shape_count += 1;
    if (out_shape_id) {
        *out_shape_id = shape->id;
    }
    return 1;
}

static int ir_program_bind_function_external_ref(IrProgram *program,
    const char *function_name,
    const AstExternal *external,
    int external_is_owned,
    IrError *error) {
    size_t function_index;

    if (!program || !function_name || function_name[0] == '\0') {
        ir_set_error(error, 0, 0, "IR-INT-141: invalid function external-ref bind contract");
        return 0;
    }

    for (function_index = 0u; function_index < program->function_count; ++function_index) {
        if (program->functions &&
            program->functions[function_index].name &&
            strcmp(program->functions[function_index].name, function_name) == 0) {
            if (program->function_external_ref_capacity < program->function_count) {
                const AstExternal **new_refs =
                    (const AstExternal **)realloc(
                        program->function_external_refs,
                        program->function_count * sizeof(const AstExternal *));
                int *new_owned =
                    (int *)realloc(
                        program->function_external_ref_owned,
                        program->function_count * sizeof(int));
                size_t fill_index;

                if (!new_refs || !new_owned) {
                    if (new_refs && !program->function_external_refs) {
                        free((void *)new_refs);
                    }
                    if (new_owned && !program->function_external_ref_owned) {
                        free(new_owned);
                    }
                    ir_set_error(error, 0, 0, "IR-INT-142: out of memory while growing function external-ref table");
                    return 0;
                }
                for (fill_index = program->function_external_ref_capacity;
                        fill_index < program->function_count;
                        ++fill_index) {
                    new_refs[fill_index] = NULL;
                    new_owned[fill_index] = 0;
                }
                program->function_external_refs = new_refs;
                program->function_external_ref_owned = new_owned;
                program->function_external_ref_capacity = program->function_count;
            }
            program->function_external_refs[function_index] = external;
            program->function_external_ref_owned[function_index] = external_is_owned ? 1 : 0;
            if (program->function_external_ref_count < function_index + 1u) {
                program->function_external_ref_count = function_index + 1u;
            }
            return 1;
        }
    }

    ir_set_error(error, 0, 0, "IR-INT-143: failed to bind function external ref for unknown function '%s'", function_name);
    return 0;
}

static const AstExternal *ir_program_find_function_external_ref(const IrProgram *program,
    const char *function_name) {
    size_t function_index;

    if (!program || !function_name || function_name[0] == '\0' ||
        !program->functions || !program->function_external_refs) {
        return NULL;
    }

    for (function_index = 0u;
            function_index < program->function_count &&
            function_index < program->function_external_ref_count;
            ++function_index) {
        if (program->functions[function_index].name &&
            strcmp(program->functions[function_index].name, function_name) == 0) {
            return program->function_external_refs[function_index];
        }
    }

    return NULL;
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
    function->return_type = AST_FUNCTION_RETURN_INT;
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
    local->value_type = AST_FUNCTION_RETURN_INT;
    local->array_rank = 0;

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
    block->terminator.has_return_value = 1;
    block->terminator.as.return_value = value;
    block->has_terminator = 1;
    return 1;
}

static int ir_block_set_void_return(IrBasicBlock *block, IrError *error) {
    if (!block) {
        ir_set_error(error, 0, 0, "IR-INT-015: invalid return terminator block");
        return 0;
    }
    if (block->has_terminator) {
        ir_set_error(error, 0, 0, "IR-INT-016: block already has a terminator");
        return 0;
    }

    block->terminator.kind = IR_TERM_RETURN;
    block->terminator.has_return_value = 0;
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

static int ir_emit_fn_make(IrLowerContext *ctx,
    IrValueRef destination,
    const char *callee_name,
    IrValueRef env,
    size_t shape_id) {
    IrInstruction instruction;
    IrBasicBlock *block = ir_lower_current_block(ctx);
    size_t name_length;

    if (!ctx || !block || !callee_name) {
        return 0;
    }

    instruction.kind = IR_INSTR_FN_MAKE;
    instruction.result = destination;
    instruction.as.fn_make.callee_name = NULL;
    instruction.as.fn_make.env = env;
    instruction.as.fn_make.shape_id = shape_id;

    name_length = strlen(callee_name);
    instruction.as.fn_make.callee_name = (char *)malloc(name_length + 1);
    if (!instruction.as.fn_make.callee_name) {
        ir_set_error(ctx->error, 0, 0, "IR-INT-046: out of memory while copying IR fn_make callee");
        return 0;
    }
    memcpy(instruction.as.fn_make.callee_name, callee_name, name_length + 1);

    if (!ir_block_append_instruction(block, &instruction, ctx->error)) {
        free(instruction.as.fn_make.callee_name);
        return 0;
    }

    return 1;
}

static int ir_emit_call_indirect(IrLowerContext *ctx,
    IrValueRef destination,
    IrValueRef callee,
    const IrValueRef *args,
    size_t arg_count) {
    IrInstruction instruction;
    IrBasicBlock *block = ir_lower_current_block(ctx);

    if (!ctx || !block) {
        return 0;
    }

    instruction.kind = IR_INSTR_CALL_INDIRECT;
    instruction.result = destination;
    instruction.as.call_indirect.callee = callee;
    instruction.as.call_indirect.args = NULL;
    instruction.as.call_indirect.arg_count = arg_count;

    if (arg_count > 0) {
        instruction.as.call_indirect.args =
            (IrValueRef *)malloc(arg_count * sizeof(IrValueRef));
        if (!instruction.as.call_indirect.args) {
            ir_set_error(ctx->error,
                0,
                0,
                "IR-INT-047: out of memory while copying IR indirect call arguments");
            return 0;
        }
        memcpy(instruction.as.call_indirect.args, args, arg_count * sizeof(IrValueRef));
    }

    if (!ir_block_append_instruction(block, &instruction, ctx->error)) {
        free(instruction.as.call_indirect.args);
        return 0;
    }

    return 1;
}


static int ir_emit_addr_local(IrLowerContext *ctx,
    IrValueRef destination,
    size_t local_id) {
    IrInstruction instruction;
    IrBasicBlock *block = ir_lower_current_block(ctx);

    if (!ctx || !block) {
        return 0;
    }

    instruction.kind = IR_INSTR_ADDR_LOCAL;
    instruction.result = destination;
    instruction.as.addr_local_id = local_id;
    return ir_block_append_instruction(block, &instruction, ctx->error);
}

static int ir_emit_addr_global(IrLowerContext *ctx,
    IrValueRef destination,
    size_t global_id) {
    IrInstruction instruction;
    IrBasicBlock *block = ir_lower_current_block(ctx);

    if (!ctx || !block) {
        return 0;
    }

    instruction.kind = IR_INSTR_ADDR_GLOBAL;
    instruction.result = destination;
    instruction.as.addr_global_id = global_id;
    return ir_block_append_instruction(block, &instruction, ctx->error);
}

static int ir_emit_load_indirect(IrLowerContext *ctx,
    IrValueRef destination,
    IrValueRef address) {
    IrInstruction instruction;
    IrBasicBlock *block = ir_lower_current_block(ctx);

    if (!ctx || !block) {
        return 0;
    }

    instruction.kind = IR_INSTR_LOAD_INDIRECT;
    instruction.result = destination;
    instruction.as.load_indirect_addr = address;
    return ir_block_append_instruction(block, &instruction, ctx->error);
}

static int ir_emit_store_indirect(IrLowerContext *ctx,
    IrValueRef address,
    IrValueRef value) {
    IrInstruction instruction;
    IrBasicBlock *block = ir_lower_current_block(ctx);

    if (!ctx || !block) {
        return 0;
    }

    instruction.kind = IR_INSTR_STORE_INDIRECT;
    instruction.result = ir_value_immediate(0);
    instruction.as.store_indirect.addr = address;
    instruction.as.store_indirect.value = value;
    return ir_block_append_instruction(block, &instruction, ctx->error);
}

static size_t ir_allocate_temp(IrFunction *function) {
    size_t temp_id = function->next_temp_id;
    function->next_temp_id++;
    return temp_id;
}

#endif
