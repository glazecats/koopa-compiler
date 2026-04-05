#include "lower_ir.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} LowerIrStringBuilder;

static void lower_ir_set_error(LowerIrError *error, int line, int column, const char *fmt, ...);
static int lower_ir_next_growth_capacity(size_t current,
    size_t initial,
    size_t elem_size,
    size_t *out_next_capacity);
static char *lower_ir_strdup(const char *text);
static void lower_ir_instruction_free(LowerIrInstruction *instruction);
static void lower_ir_basic_block_free(LowerIrBasicBlock *block);
static void lower_ir_function_free(LowerIrFunction *function);
static void lower_ir_global_free(LowerIrGlobal *global);
static const LowerIrLocal *lower_ir_function_get_local(const LowerIrFunction *function, size_t local_id);
static const LowerIrGlobal *lower_ir_program_get_global(const LowerIrProgram *program, size_t global_id);
static int lower_ir_instruction_copy(LowerIrInstruction *destination,
    const LowerIrInstruction *source,
    LowerIrError *error);

static int lower_ir_append_string(LowerIrStringBuilder *builder, const char *text);
static int lower_ir_append_format(LowerIrStringBuilder *builder, const char *fmt, ...);
static int lower_ir_append_value_name(LowerIrStringBuilder *builder, LowerIrValueRef value);
static int lower_ir_append_slot_name(LowerIrStringBuilder *builder,
    const LowerIrProgram *program,
    const LowerIrFunction *function,
    LowerIrSlotRef slot);

static void lower_ir_set_error(LowerIrError *error, int line, int column, const char *fmt, ...) {
    va_list args;

    if (!error || !fmt) {
        return;
    }

    error->line = line;
    error->column = column;

    va_start(args, fmt);
    vsnprintf(error->message, sizeof(error->message), fmt, args);
    va_end(args);
}

static int lower_ir_next_growth_capacity(size_t current,
    size_t initial,
    size_t elem_size,
    size_t *out_next_capacity) {
    size_t next_capacity;

    if (!out_next_capacity || initial == 0 || elem_size == 0) {
        return 0;
    }

    if (current == 0) {
        next_capacity = initial;
    } else {
        if (current > ((size_t)-1) / 2) {
            return 0;
        }
        next_capacity = current * 2;
    }

    if (next_capacity > ((size_t)-1) / elem_size) {
        return 0;
    }

    *out_next_capacity = next_capacity;
    return 1;
}

static char *lower_ir_strdup(const char *text) {
    char *copy;
    size_t length;

    if (!text) {
        return NULL;
    }

    length = strlen(text);
    copy = (char *)malloc(length + 1);
    if (!copy) {
        return NULL;
    }

    memcpy(copy, text, length + 1);
    return copy;
}

static void lower_ir_instruction_free(LowerIrInstruction *instruction) {
    if (!instruction) {
        return;
    }

    if (instruction->kind == LOWER_IR_INSTR_CALL) {
        free(instruction->as.call.callee_name);
        free(instruction->as.call.args);
        instruction->as.call.callee_name = NULL;
        instruction->as.call.args = NULL;
        instruction->as.call.arg_count = 0;
    }
}

static void lower_ir_basic_block_free(LowerIrBasicBlock *block) {
    size_t i;

    if (!block) {
        return;
    }

    for (i = 0; i < block->instruction_count; ++i) {
        lower_ir_instruction_free(&block->instructions[i]);
    }

    free(block->instructions);
    block->instructions = NULL;
    block->instruction_count = 0;
    block->instruction_capacity = 0;
    block->has_terminator = 0;
}

static void lower_ir_function_free(LowerIrFunction *function) {
    size_t i;

    if (!function) {
        return;
    }

    free(function->name);
    function->name = NULL;

    for (i = 0; i < function->local_count; ++i) {
        free(function->locals[i].source_name);
        function->locals[i].source_name = NULL;
    }
    free(function->locals);
    function->locals = NULL;
    function->local_count = 0;
    function->local_capacity = 0;

    for (i = 0; i < function->block_count; ++i) {
        lower_ir_basic_block_free(&function->blocks[i]);
    }
    free(function->blocks);
    function->blocks = NULL;
    function->block_count = 0;
    function->block_capacity = 0;
    function->parameter_count = 0;
    function->next_temp_id = 0;
    function->has_body = 0;
}

static void lower_ir_global_free(LowerIrGlobal *global) {
    if (!global) {
        return;
    }

    free(global->name);
    global->name = NULL;
}

static const LowerIrLocal *lower_ir_function_get_local(const LowerIrFunction *function, size_t local_id) {
    if (!function || local_id >= function->local_count) {
        return NULL;
    }

    return &function->locals[local_id];
}

static const LowerIrGlobal *lower_ir_program_get_global(const LowerIrProgram *program, size_t global_id) {
    if (!program || global_id >= program->global_count) {
        return NULL;
    }

    return &program->globals[global_id];
}

static int lower_ir_instruction_copy(LowerIrInstruction *destination,
    const LowerIrInstruction *source,
    LowerIrError *error) {
    if (!destination || !source) {
        lower_ir_set_error(error, 0, 0, "LOWER-IR-001: invalid instruction copy contract");
        return 0;
    }

    *destination = *source;
    if (source->kind != LOWER_IR_INSTR_CALL) {
        return 1;
    }

    destination->as.call.callee_name = NULL;
    destination->as.call.args = NULL;

    if (source->as.call.callee_name) {
        destination->as.call.callee_name = lower_ir_strdup(source->as.call.callee_name);
        if (!destination->as.call.callee_name) {
            lower_ir_set_error(error, 0, 0, "LOWER-IR-002: out of memory copying callee name");
            return 0;
        }
    }

    if (source->as.call.arg_count > 0) {
        destination->as.call.args = (LowerIrValueRef *)malloc(
            source->as.call.arg_count * sizeof(LowerIrValueRef));
        if (!destination->as.call.args) {
            free(destination->as.call.callee_name);
            destination->as.call.callee_name = NULL;
            lower_ir_set_error(error, 0, 0, "LOWER-IR-003: out of memory copying call args");
            return 0;
        }
        memcpy(destination->as.call.args,
            source->as.call.args,
            source->as.call.arg_count * sizeof(LowerIrValueRef));
    }

    return 1;
}

static int lower_ir_append_string(LowerIrStringBuilder *builder, const char *text) {
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
            if (!lower_ir_next_growth_capacity(next_capacity, 64, sizeof(char), &next_capacity)) {
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

static int lower_ir_append_format(LowerIrStringBuilder *builder, const char *fmt, ...) {
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
        ok = lower_ir_append_string(builder, stack_buffer);
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
    ok = lower_ir_append_string(builder, heap_buffer);
    free(heap_buffer);
    return ok;
}

static int lower_ir_append_value_name(LowerIrStringBuilder *builder, LowerIrValueRef value) {
    switch (value.kind) {
    case LOWER_IR_VALUE_IMMEDIATE:
        return lower_ir_append_format(builder, "%lld", value.immediate);
    case LOWER_IR_VALUE_TEMP:
        return lower_ir_append_format(builder, "tmp.%zu", value.temp_id);
    default:
        return 0;
    }
}

static int lower_ir_append_slot_name(LowerIrStringBuilder *builder,
    const LowerIrProgram *program,
    const LowerIrFunction *function,
    LowerIrSlotRef slot) {
    const LowerIrLocal *local;
    const LowerIrGlobal *global;

    switch (slot.kind) {
    case LOWER_IR_SLOT_LOCAL:
        local = lower_ir_function_get_local(function, slot.id);
        if (!local || !local->source_name || local->source_name[0] == '\0') {
            return lower_ir_append_format(builder, "local.%zu", slot.id);
        }
        return lower_ir_append_format(builder, "%s.%zu", local->source_name, local->id);
    case LOWER_IR_SLOT_GLOBAL:
        global = lower_ir_program_get_global(program, slot.id);
        if (!global || !global->name || global->name[0] == '\0') {
            return lower_ir_append_format(builder, "global.%zu", slot.id);
        }
        return lower_ir_append_format(builder, "%s.%zu", global->name, global->id);
    default:
        return 0;
    }
}

void lower_ir_program_init(LowerIrProgram *program) {
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

void lower_ir_program_free(LowerIrProgram *program) {
    size_t i;

    if (!program) {
        return;
    }

    for (i = 0; i < program->global_count; ++i) {
        lower_ir_global_free(&program->globals[i]);
    }
    free(program->globals);

    for (i = 0; i < program->function_count; ++i) {
        lower_ir_function_free(&program->functions[i]);
    }
    free(program->functions);

    lower_ir_program_init(program);
}

LowerIrValueRef lower_ir_value_immediate(long long value) {
    LowerIrValueRef ref;

    ref.kind = LOWER_IR_VALUE_IMMEDIATE;
    ref.immediate = value;
    ref.temp_id = 0;
    return ref;
}

LowerIrValueRef lower_ir_value_temp(size_t temp_id) {
    LowerIrValueRef ref;

    ref.kind = LOWER_IR_VALUE_TEMP;
    ref.immediate = 0;
    ref.temp_id = temp_id;
    return ref;
}

LowerIrSlotRef lower_ir_slot_local(size_t local_id) {
    LowerIrSlotRef ref;

    ref.kind = LOWER_IR_SLOT_LOCAL;
    ref.id = local_id;
    return ref;
}

LowerIrSlotRef lower_ir_slot_global(size_t global_id) {
    LowerIrSlotRef ref;

    ref.kind = LOWER_IR_SLOT_GLOBAL;
    ref.id = global_id;
    return ref;
}

int lower_ir_program_append_global(LowerIrProgram *program,
    const char *name,
    LowerIrGlobal **out_global,
    LowerIrError *error) {
    size_t next_capacity;
    LowerIrGlobal *globals;
    LowerIrGlobal *global;

    if (!program || !name || name[0] == '\0') {
        lower_ir_set_error(error, 0, 0, "LOWER-IR-004: invalid global append contract");
        return 0;
    }

    if (program->global_count == program->global_capacity) {
        if (!lower_ir_next_growth_capacity(program->global_capacity,
                4,
                sizeof(LowerIrGlobal),
                &next_capacity)) {
            lower_ir_set_error(error, 0, 0, "LOWER-IR-005: global table capacity overflow");
            return 0;
        }
        globals = (LowerIrGlobal *)realloc(program->globals, next_capacity * sizeof(LowerIrGlobal));
        if (!globals) {
            lower_ir_set_error(error, 0, 0, "LOWER-IR-006: out of memory growing global table");
            return 0;
        }
        program->globals = globals;
        program->global_capacity = next_capacity;
    }

    global = &program->globals[program->global_count];
    global->id = program->global_count;
    global->name = lower_ir_strdup(name);
    global->has_initializer = 0;
    global->initializer_value = 0;
    global->has_runtime_initializer = 0;
    if (!global->name) {
        lower_ir_set_error(error, 0, 0, "LOWER-IR-007: out of memory copying global name");
        return 0;
    }

    ++program->global_count;
    if (out_global) {
        *out_global = global;
    }
    return 1;
}

int lower_ir_program_append_function(LowerIrProgram *program,
    const char *name,
    int has_body,
    LowerIrFunction **out_function,
    LowerIrError *error) {
    size_t next_capacity;
    LowerIrFunction *functions;
    LowerIrFunction *function;

    if (!program || !name || name[0] == '\0') {
        lower_ir_set_error(error, 0, 0, "LOWER-IR-008: invalid function append contract");
        return 0;
    }

    if (program->function_count == program->function_capacity) {
        if (!lower_ir_next_growth_capacity(program->function_capacity,
                4,
                sizeof(LowerIrFunction),
                &next_capacity)) {
            lower_ir_set_error(error, 0, 0, "LOWER-IR-009: function table capacity overflow");
            return 0;
        }
        functions = (LowerIrFunction *)realloc(program->functions,
            next_capacity * sizeof(LowerIrFunction));
        if (!functions) {
            lower_ir_set_error(error, 0, 0, "LOWER-IR-010: out of memory growing function table");
            return 0;
        }
        program->functions = functions;
        program->function_capacity = next_capacity;
    }

    function = &program->functions[program->function_count];
    function->name = lower_ir_strdup(name);
    if (!function->name) {
        lower_ir_set_error(error, 0, 0, "LOWER-IR-011: out of memory copying function name");
        return 0;
    }
    function->has_body = has_body;
    function->parameter_count = 0;
    function->locals = NULL;
    function->local_count = 0;
    function->local_capacity = 0;
    function->blocks = NULL;
    function->block_count = 0;
    function->block_capacity = 0;
    function->next_temp_id = 0;

    ++program->function_count;
    if (out_function) {
        *out_function = function;
    }
    return 1;
}

int lower_ir_function_append_local(LowerIrFunction *function,
    const char *source_name,
    int is_parameter,
    size_t *out_local_id,
    LowerIrError *error) {
    size_t next_capacity;
    LowerIrLocal *locals;
    LowerIrLocal *local;

    if (!function || !source_name || source_name[0] == '\0') {
        lower_ir_set_error(error, 0, 0, "LOWER-IR-012: invalid local append contract");
        return 0;
    }
    if (is_parameter && function->local_count != function->parameter_count) {
        lower_ir_set_error(error,
            0,
            0,
            "LOWER-IR-037: function '%s' cannot append parameter local '%s' after non-parameter locals",
            function->name ? function->name : "<anon>",
            source_name);
        return 0;
    }

    if (function->local_count == function->local_capacity) {
        if (!lower_ir_next_growth_capacity(function->local_capacity,
                4,
                sizeof(LowerIrLocal),
                &next_capacity)) {
            lower_ir_set_error(error, 0, 0, "LOWER-IR-013: local table capacity overflow");
            return 0;
        }
        locals = (LowerIrLocal *)realloc(function->locals, next_capacity * sizeof(LowerIrLocal));
        if (!locals) {
            lower_ir_set_error(error, 0, 0, "LOWER-IR-014: out of memory growing local table");
            return 0;
        }
        function->locals = locals;
        function->local_capacity = next_capacity;
    }

    local = &function->locals[function->local_count];
    local->id = function->local_count;
    local->source_name = lower_ir_strdup(source_name);
    if (!local->source_name) {
        lower_ir_set_error(error, 0, 0, "LOWER-IR-015: out of memory copying local name");
        return 0;
    }
    local->is_parameter = is_parameter;

    if (is_parameter) {
        ++function->parameter_count;
    }

    if (out_local_id) {
        *out_local_id = local->id;
    }

    ++function->local_count;
    return 1;
}

int lower_ir_function_append_block(LowerIrFunction *function,
    size_t *out_block_id,
    LowerIrBasicBlock **out_block,
    LowerIrError *error) {
    size_t next_capacity;
    LowerIrBasicBlock *blocks;
    LowerIrBasicBlock *block;

    if (!function) {
        lower_ir_set_error(error, 0, 0, "LOWER-IR-016: invalid block append contract");
        return 0;
    }
    if (!function->has_body) {
        lower_ir_set_error(error,
            0,
            0,
            "LOWER-IR-038: declaration '%s' cannot append body blocks",
            function->name ? function->name : "<anon>");
        return 0;
    }

    if (function->block_count == function->block_capacity) {
        if (!lower_ir_next_growth_capacity(function->block_capacity,
                4,
                sizeof(LowerIrBasicBlock),
                &next_capacity)) {
            lower_ir_set_error(error, 0, 0, "LOWER-IR-017: block table capacity overflow");
            return 0;
        }
        blocks = (LowerIrBasicBlock *)realloc(function->blocks,
            next_capacity * sizeof(LowerIrBasicBlock));
        if (!blocks) {
            lower_ir_set_error(error, 0, 0, "LOWER-IR-018: out of memory growing block table");
            return 0;
        }
        function->blocks = blocks;
        function->block_capacity = next_capacity;
    }

    block = &function->blocks[function->block_count];
    block->id = function->block_count;
    block->instructions = NULL;
    block->instruction_count = 0;
    block->instruction_capacity = 0;
    block->has_terminator = 0;

    if (out_block_id) {
        *out_block_id = block->id;
    }
    if (out_block) {
        *out_block = block;
    }

    ++function->block_count;
    return 1;
}

size_t lower_ir_function_allocate_temp(LowerIrFunction *function) {
    size_t temp_id;

    if (!function || !function->has_body || function->next_temp_id == (size_t)-1) {
        return (size_t)-1;
    }

    temp_id = function->next_temp_id;
    ++function->next_temp_id;
    return temp_id;
}

int lower_ir_block_append_instruction(LowerIrBasicBlock *block,
    const LowerIrInstruction *instruction,
    LowerIrError *error) {
    size_t next_capacity;
    LowerIrInstruction *instructions;
    LowerIrInstruction *copy;

    if (!block || !instruction) {
        lower_ir_set_error(error, 0, 0, "LOWER-IR-019: invalid instruction append contract");
        return 0;
    }
    if (block->has_terminator) {
        lower_ir_set_error(error,
            0,
            0,
            "LOWER-IR-039: bb.%zu cannot append instruction after terminator",
            block->id);
        return 0;
    }

    if (block->instruction_count == block->instruction_capacity) {
        if (!lower_ir_next_growth_capacity(block->instruction_capacity,
                4,
                sizeof(LowerIrInstruction),
                &next_capacity)) {
            lower_ir_set_error(error, 0, 0, "LOWER-IR-020: instruction table capacity overflow");
            return 0;
        }
        instructions = (LowerIrInstruction *)realloc(block->instructions,
            next_capacity * sizeof(LowerIrInstruction));
        if (!instructions) {
            lower_ir_set_error(error, 0, 0, "LOWER-IR-021: out of memory growing instruction table");
            return 0;
        }
        block->instructions = instructions;
        block->instruction_capacity = next_capacity;
    }

    copy = &block->instructions[block->instruction_count];
    if (!lower_ir_instruction_copy(copy, instruction, error)) {
        return 0;
    }

    ++block->instruction_count;
    return 1;
}

int lower_ir_block_set_return(LowerIrBasicBlock *block,
    LowerIrValueRef value,
    LowerIrError *error) {
    if (!block) {
        lower_ir_set_error(error, 0, 0, "LOWER-IR-022: invalid return terminator contract");
        return 0;
    }
    if (block->has_terminator) {
        lower_ir_set_error(error,
            0,
            0,
            "LOWER-IR-040: bb.%zu already has a terminator",
            block->id);
        return 0;
    }

    block->has_terminator = 1;
    block->terminator.kind = LOWER_IR_TERM_RETURN;
    block->terminator.as.return_value = value;
    return 1;
}

int lower_ir_block_set_jump(LowerIrBasicBlock *block,
    size_t target_block_id,
    LowerIrError *error) {
    if (!block) {
        lower_ir_set_error(error, 0, 0, "LOWER-IR-023: invalid jump terminator contract");
        return 0;
    }
    if (block->has_terminator) {
        lower_ir_set_error(error,
            0,
            0,
            "LOWER-IR-040: bb.%zu already has a terminator",
            block->id);
        return 0;
    }

    block->has_terminator = 1;
    block->terminator.kind = LOWER_IR_TERM_JUMP;
    block->terminator.as.jump_target = target_block_id;
    return 1;
}

int lower_ir_block_set_branch(LowerIrBasicBlock *block,
    LowerIrValueRef condition,
    size_t then_target,
    size_t else_target,
    LowerIrError *error) {
    if (!block) {
        lower_ir_set_error(error, 0, 0, "LOWER-IR-024: invalid branch terminator contract");
        return 0;
    }
    if (block->has_terminator) {
        lower_ir_set_error(error,
            0,
            0,
            "LOWER-IR-040: bb.%zu already has a terminator",
            block->id);
        return 0;
    }

    block->has_terminator = 1;
    block->terminator.kind = LOWER_IR_TERM_BRANCH;
    block->terminator.as.branch.condition = condition;
    block->terminator.as.branch.then_target = then_target;
    block->terminator.as.branch.else_target = else_target;
    return 1;
}

#define LOWER_IR_SPLIT_AGGREGATOR
#include "lower_from_ir.inc"
#include "lower_ir_verify.inc"
#include "lower_ir_dump.inc"
