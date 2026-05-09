#include "value_ssa.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} ValueSsaStringBuilder;

static void value_ssa_set_error(ValueSsaError *error, int line, int column, const char *fmt, ...);
static int value_ssa_next_growth_capacity(size_t current,
    size_t initial,
    size_t elem_size,
    size_t *out_next_capacity);
static char *value_ssa_strdup(const char *text);
static void value_ssa_instruction_free(ValueSsaInstruction *instruction);
static void value_ssa_phi_free(ValueSsaPhi *phi);
static void value_ssa_basic_block_free(ValueSsaBasicBlock *block);
static void value_ssa_function_free(ValueSsaFunction *function);
static void value_ssa_global_free(ValueSsaGlobal *global);
static const ValueSsaLocal *value_ssa_function_get_local(const ValueSsaFunction *function, size_t local_id);
static const ValueSsaGlobal *value_ssa_program_get_global(const ValueSsaProgram *program, size_t global_id);
static int value_ssa_instruction_copy(ValueSsaInstruction *destination,
    const ValueSsaInstruction *source,
    ValueSsaError *error);

static int value_ssa_append_string(ValueSsaStringBuilder *builder, const char *text);
static int value_ssa_append_format(ValueSsaStringBuilder *builder, const char *fmt, ...);
static int value_ssa_append_value_name(ValueSsaStringBuilder *builder, ValueSsaValueRef value);
static int value_ssa_append_slot_name(ValueSsaStringBuilder *builder,
    const ValueSsaProgram *program,
    const ValueSsaFunction *function,
    ValueSsaSlotRef slot);

static void value_ssa_set_error(ValueSsaError *error, int line, int column, const char *fmt, ...) {
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

static int value_ssa_next_growth_capacity(size_t current,
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

static char *value_ssa_strdup(const char *text) {
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

static void value_ssa_instruction_free(ValueSsaInstruction *instruction) {
    if (!instruction) {
        return;
    }

    if (instruction->kind == VALUE_SSA_INSTR_CALL) {
        free(instruction->as.call.callee_name);
        free(instruction->as.call.args);
        instruction->as.call.callee_name = NULL;
        instruction->as.call.args = NULL;
        instruction->as.call.arg_count = 0;
    }
}

static void value_ssa_phi_free(ValueSsaPhi *phi) {
    if (!phi) {
        return;
    }

    free(phi->inputs);
    phi->inputs = NULL;
    phi->input_count = 0;
    phi->input_capacity = 0;
}

static void value_ssa_basic_block_free(ValueSsaBasicBlock *block) {
    size_t i;

    if (!block) {
        return;
    }

    for (i = 0; i < block->phi_count; ++i) {
        value_ssa_phi_free(&block->phis[i]);
    }
    free(block->phis);
    block->phis = NULL;
    block->phi_count = 0;
    block->phi_capacity = 0;

    for (i = 0; i < block->instruction_count; ++i) {
        value_ssa_instruction_free(&block->instructions[i]);
    }
    free(block->instructions);
    block->instructions = NULL;
    block->instruction_count = 0;
    block->instruction_capacity = 0;
    block->has_terminator = 0;
    memset(&block->terminator, 0, sizeof(block->terminator));
}

static void value_ssa_function_free(ValueSsaFunction *function) {
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
        value_ssa_basic_block_free(&function->blocks[i]);
    }
    free(function->blocks);
    function->blocks = NULL;
    function->block_count = 0;
    function->block_capacity = 0;
    function->parameter_count = 0;
    function->next_value_id = 0;
    function->has_body = 0;
}

static void value_ssa_global_free(ValueSsaGlobal *global) {
    if (!global) {
        return;
    }

    free(global->name);
    global->name = NULL;
}

static const ValueSsaLocal *value_ssa_function_get_local(const ValueSsaFunction *function, size_t local_id) {
    if (!function || local_id >= function->local_count) {
        return NULL;
    }

    return &function->locals[local_id];
}

static const ValueSsaGlobal *value_ssa_program_get_global(const ValueSsaProgram *program, size_t global_id) {
    if (!program || global_id >= program->global_count) {
        return NULL;
    }

    return &program->globals[global_id];
}

static int value_ssa_instruction_copy(ValueSsaInstruction *destination,
    const ValueSsaInstruction *source,
    ValueSsaError *error) {
    if (!destination || !source) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-001: invalid instruction copy contract");
        return 0;
    }

    *destination = *source;
    if (source->kind != VALUE_SSA_INSTR_CALL) {
        return 1;
    }

    destination->as.call.callee_name = NULL;
    destination->as.call.args = NULL;

    if (source->as.call.callee_name) {
        destination->as.call.callee_name = value_ssa_strdup(source->as.call.callee_name);
        if (!destination->as.call.callee_name) {
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-002: out of memory copying callee name");
            return 0;
        }
    }

    if (source->as.call.arg_count > 0) {
        destination->as.call.args = (ValueSsaValueRef *)malloc(
            source->as.call.arg_count * sizeof(ValueSsaValueRef));
        if (!destination->as.call.args) {
            free(destination->as.call.callee_name);
            destination->as.call.callee_name = NULL;
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-003: out of memory copying call args");
            return 0;
        }
        memcpy(destination->as.call.args,
            source->as.call.args,
            source->as.call.arg_count * sizeof(ValueSsaValueRef));
    }

    return 1;
}

static int value_ssa_append_string(ValueSsaStringBuilder *builder, const char *text) {
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
            if (!value_ssa_next_growth_capacity(next_capacity, 64, sizeof(char), &next_capacity)) {
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

static int value_ssa_append_format(ValueSsaStringBuilder *builder, const char *fmt, ...) {
    va_list args;
    va_list copy;
    int needed;
    char *new_data;
    size_t required;
    size_t next_capacity;

    if (!builder || !fmt) {
        return 0;
    }

    va_start(args, fmt);
    va_copy(copy, args);
    needed = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (needed < 0) {
        va_end(args);
        return 0;
    }

    required = builder->length + (size_t)needed + 1;
    if (required > builder->capacity) {
        next_capacity = builder->capacity;
        while (next_capacity < required) {
            if (!value_ssa_next_growth_capacity(next_capacity, 64, sizeof(char), &next_capacity)) {
                va_end(args);
                return 0;
            }
        }
        new_data = (char *)realloc(builder->data, next_capacity);
        if (!new_data) {
            va_end(args);
            return 0;
        }
        builder->data = new_data;
        builder->capacity = next_capacity;
    }

    vsnprintf(builder->data + builder->length, builder->capacity - builder->length, fmt, args);
    builder->length += (size_t)needed;
    va_end(args);
    return 1;
}

static int value_ssa_append_value_name(ValueSsaStringBuilder *builder, ValueSsaValueRef value) {
    switch (value.kind) {
    case VALUE_SSA_VALUE_IMMEDIATE:
        return value_ssa_append_format(builder, "%lld", value.immediate);
    case VALUE_SSA_VALUE_ID:
        return value_ssa_append_format(builder, "ssa.%zu", value.value_id);
    default:
        return 0;
    }
}

static int value_ssa_append_slot_name(ValueSsaStringBuilder *builder,
    const ValueSsaProgram *program,
    const ValueSsaFunction *function,
    ValueSsaSlotRef slot) {
    switch (slot.kind) {
    case VALUE_SSA_SLOT_LOCAL:
        {
            const ValueSsaLocal *local = value_ssa_function_get_local(function, slot.id);

            if (!local || !local->source_name) {
                return value_ssa_append_format(builder, "local.%zu", slot.id);
            }
            return value_ssa_append_format(builder, "%s.%zu", local->source_name, local->id);
        }
    case VALUE_SSA_SLOT_GLOBAL:
        {
            const ValueSsaGlobal *global = value_ssa_program_get_global(program, slot.id);

            if (!global || !global->name) {
                return value_ssa_append_format(builder, "global.%zu", slot.id);
            }
            return value_ssa_append_format(builder, "%s.%zu", global->name, global->id);
        }
    default:
        return 0;
    }
}

void value_ssa_program_init(ValueSsaProgram *program) {
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

void value_ssa_program_free(ValueSsaProgram *program) {
    size_t i;

    if (!program) {
        return;
    }

    for (i = 0; i < program->global_count; ++i) {
        value_ssa_global_free(&program->globals[i]);
    }
    free(program->globals);
    program->globals = NULL;
    program->global_count = 0;
    program->global_capacity = 0;

    for (i = 0; i < program->function_count; ++i) {
        value_ssa_function_free(&program->functions[i]);
    }
    free(program->functions);
    program->functions = NULL;
    program->function_count = 0;
    program->function_capacity = 0;
}

ValueSsaValueRef value_ssa_value_immediate(long long value) {
    ValueSsaValueRef ref;

    ref.kind = VALUE_SSA_VALUE_IMMEDIATE;
    ref.immediate = value;
    ref.value_id = 0;
    return ref;
}

ValueSsaValueRef value_ssa_value_id(size_t value_id) {
    ValueSsaValueRef ref;

    ref.kind = VALUE_SSA_VALUE_ID;
    ref.immediate = 0;
    ref.value_id = value_id;
    return ref;
}

ValueSsaSlotRef value_ssa_slot_local(size_t local_id) {
    ValueSsaSlotRef ref;

    ref.kind = VALUE_SSA_SLOT_LOCAL;
    ref.id = local_id;
    return ref;
}

ValueSsaSlotRef value_ssa_slot_global(size_t global_id) {
    ValueSsaSlotRef ref;

    ref.kind = VALUE_SSA_SLOT_GLOBAL;
    ref.id = global_id;
    return ref;
}

int value_ssa_program_append_global(ValueSsaProgram *program,
    const char *name,
    ValueSsaGlobal **out_global,
    ValueSsaError *error) {
    size_t next_capacity;
    ValueSsaGlobal *globals;
    ValueSsaGlobal *global;

    if (!program || !name || name[0] == '\0') {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-004: invalid global append contract");
        return 0;
    }

    if (program->global_count == program->global_capacity) {
        if (!value_ssa_next_growth_capacity(program->global_capacity, 4, sizeof(ValueSsaGlobal), &next_capacity)) {
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-005: global table capacity overflow");
            return 0;
        }
        globals = (ValueSsaGlobal *)realloc(program->globals, next_capacity * sizeof(ValueSsaGlobal));
        if (!globals) {
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-006: out of memory growing global table");
            return 0;
        }
        program->globals = globals;
        program->global_capacity = next_capacity;
    }

    global = &program->globals[program->global_count];
    global->id = program->global_count;
    global->name = value_ssa_strdup(name);
    global->byte_size = 4u;
    global->has_initializer = 0;
    global->initializer_value = 0;
    global->has_runtime_initializer = 0;
    if (!global->name) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-007: out of memory copying global name");
        return 0;
    }

    ++program->global_count;
    if (out_global) {
        *out_global = global;
    }
    return 1;
}

int value_ssa_program_append_function(ValueSsaProgram *program,
    const char *name,
    int has_body,
    ValueSsaFunction **out_function,
    ValueSsaError *error) {
    size_t next_capacity;
    ValueSsaFunction *functions;
    ValueSsaFunction *function;

    if (!program || !name || name[0] == '\0') {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-008: invalid function append contract");
        return 0;
    }

    if (program->function_count == program->function_capacity) {
        if (!value_ssa_next_growth_capacity(program->function_capacity, 4, sizeof(ValueSsaFunction), &next_capacity)) {
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-009: function table capacity overflow");
            return 0;
        }
        functions = (ValueSsaFunction *)realloc(program->functions, next_capacity * sizeof(ValueSsaFunction));
        if (!functions) {
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-010: out of memory growing function table");
            return 0;
        }
        program->functions = functions;
        program->function_capacity = next_capacity;
    }

    function = &program->functions[program->function_count];
    function->name = value_ssa_strdup(name);
    if (!function->name) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-011: out of memory copying function name");
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
    function->next_value_id = 0;

    ++program->function_count;
    if (out_function) {
        *out_function = function;
    }
    return 1;
}

int value_ssa_function_append_local(ValueSsaFunction *function,
    const char *source_name,
    int is_parameter,
    size_t *out_local_id,
    ValueSsaError *error) {
    size_t next_capacity;
    ValueSsaLocal *locals;
    ValueSsaLocal *local;

    if (!function || !source_name || source_name[0] == '\0') {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-012: invalid local append contract");
        return 0;
    }
    if (is_parameter && function->local_count != function->parameter_count) {
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-013: function '%s' cannot append parameter local '%s' after non-parameter locals",
            function->name ? function->name : "<anon>",
            source_name);
        return 0;
    }

    if (function->local_count == function->local_capacity) {
        if (!value_ssa_next_growth_capacity(function->local_capacity, 4, sizeof(ValueSsaLocal), &next_capacity)) {
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-014: local table capacity overflow");
            return 0;
        }
        locals = (ValueSsaLocal *)realloc(function->locals, next_capacity * sizeof(ValueSsaLocal));
        if (!locals) {
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-015: out of memory growing local table");
            return 0;
        }
        function->locals = locals;
        function->local_capacity = next_capacity;
    }

    local = &function->locals[function->local_count];
    local->id = function->local_count;
    local->source_name = value_ssa_strdup(source_name);
    if (!local->source_name) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-016: out of memory copying local name");
        return 0;
    }
    local->is_parameter = is_parameter;
    local->array_rank = 0u;

    if (is_parameter) {
        ++function->parameter_count;
    }
    if (out_local_id) {
        *out_local_id = local->id;
    }

    ++function->local_count;
    return 1;
}

int value_ssa_function_append_block(ValueSsaFunction *function,
    size_t *out_block_id,
    ValueSsaBasicBlock **out_block,
    ValueSsaError *error) {
    size_t next_capacity;
    ValueSsaBasicBlock *blocks;
    ValueSsaBasicBlock *block;

    if (!function) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-017: invalid block append contract");
        return 0;
    }
    if (!function->has_body) {
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-018: declaration '%s' cannot append body blocks",
            function->name ? function->name : "<anon>");
        return 0;
    }

    if (function->block_count == function->block_capacity) {
        if (!value_ssa_next_growth_capacity(function->block_capacity, 4, sizeof(ValueSsaBasicBlock), &next_capacity)) {
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-019: block table capacity overflow");
            return 0;
        }
        blocks = (ValueSsaBasicBlock *)realloc(function->blocks, next_capacity * sizeof(ValueSsaBasicBlock));
        if (!blocks) {
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-020: out of memory growing block table");
            return 0;
        }
        function->blocks = blocks;
        function->block_capacity = next_capacity;
    }

    block = &function->blocks[function->block_count];
    block->id = function->block_count;
    block->phis = NULL;
    block->phi_count = 0;
    block->phi_capacity = 0;
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

size_t value_ssa_function_allocate_value(ValueSsaFunction *function) {
    size_t value_id;

    if (!function || !function->has_body || function->next_value_id == (size_t)-1) {
        return (size_t)-1;
    }

    value_id = function->next_value_id;
    ++function->next_value_id;
    return value_id;
}

int value_ssa_block_append_phi(ValueSsaBasicBlock *block,
    size_t result_id,
    const ValueSsaPhiInput *inputs,
    size_t input_count,
    ValueSsaError *error) {
    size_t next_capacity;
    ValueSsaPhi *phis;
    ValueSsaPhi *phi;

    if (!block || (input_count > 0 && !inputs)) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-021: invalid phi append contract");
        return 0;
    }
    if (block->instruction_count > 0) {
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-022: bb.%zu cannot append phi nodes after normal instructions",
            block->id);
        return 0;
    }
    if (block->has_terminator) {
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-023: bb.%zu cannot append phi nodes after terminator",
            block->id);
        return 0;
    }

    if (block->phi_count == block->phi_capacity) {
        if (!value_ssa_next_growth_capacity(block->phi_capacity, 4, sizeof(ValueSsaPhi), &next_capacity)) {
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-024: phi table capacity overflow");
            return 0;
        }
        phis = (ValueSsaPhi *)realloc(block->phis, next_capacity * sizeof(ValueSsaPhi));
        if (!phis) {
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-025: out of memory growing phi table");
            return 0;
        }
        block->phis = phis;
        block->phi_capacity = next_capacity;
    }

    phi = &block->phis[block->phi_count];
    phi->result_id = result_id;
    phi->inputs = NULL;
    phi->input_count = input_count;
    phi->input_capacity = input_count;
    if (input_count > 0) {
        phi->inputs = (ValueSsaPhiInput *)malloc(input_count * sizeof(ValueSsaPhiInput));
        if (!phi->inputs) {
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-026: out of memory copying phi inputs");
            return 0;
        }
        memcpy(phi->inputs, inputs, input_count * sizeof(ValueSsaPhiInput));
    }

    ++block->phi_count;
    return 1;
}

int value_ssa_block_append_instruction(ValueSsaBasicBlock *block,
    const ValueSsaInstruction *instruction,
    ValueSsaError *error) {
    size_t next_capacity;
    ValueSsaInstruction *instructions;
    ValueSsaInstruction *copy;

    if (!block || !instruction) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-027: invalid instruction append contract");
        return 0;
    }
    if (block->has_terminator) {
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-028: bb.%zu cannot append instruction after terminator",
            block->id);
        return 0;
    }

    if (block->instruction_count == block->instruction_capacity) {
        if (!value_ssa_next_growth_capacity(block->instruction_capacity, 4, sizeof(ValueSsaInstruction), &next_capacity)) {
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-029: instruction table capacity overflow");
            return 0;
        }
        instructions = (ValueSsaInstruction *)realloc(block->instructions,
            next_capacity * sizeof(ValueSsaInstruction));
        if (!instructions) {
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-030: out of memory growing instruction table");
            return 0;
        }
        block->instructions = instructions;
        block->instruction_capacity = next_capacity;
    }

    copy = &block->instructions[block->instruction_count];
    if (!value_ssa_instruction_copy(copy, instruction, error)) {
        return 0;
    }

    ++block->instruction_count;
    return 1;
}

int value_ssa_block_set_return(ValueSsaBasicBlock *block,
    ValueSsaValueRef value,
    ValueSsaError *error) {
    if (!block) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-031: invalid return terminator contract");
        return 0;
    }
    if (block->has_terminator) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-032: bb.%zu already has a terminator", block->id);
        return 0;
    }

    block->has_terminator = 1;
    block->terminator.kind = VALUE_SSA_TERM_RETURN;
    block->terminator.has_return_value = 1;
    block->terminator.as.return_value = value;
    return 1;
}

int value_ssa_block_set_void_return(ValueSsaBasicBlock *block, ValueSsaError *error) {
    if (!block) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-031: invalid return terminator contract");
        return 0;
    }
    if (block->has_terminator) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-032: bb.%zu already has a terminator", block->id);
        return 0;
    }

    block->has_terminator = 1;
    block->terminator.kind = VALUE_SSA_TERM_RETURN;
    block->terminator.has_return_value = 0;
    block->terminator.as.return_value = value_ssa_value_immediate(0);
    return 1;
}

int value_ssa_block_set_jump(ValueSsaBasicBlock *block,
    size_t target_block_id,
    ValueSsaError *error) {
    if (!block) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-033: invalid jump terminator contract");
        return 0;
    }
    if (block->has_terminator) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-032: bb.%zu already has a terminator", block->id);
        return 0;
    }

    block->has_terminator = 1;
    block->terminator.kind = VALUE_SSA_TERM_JUMP;
    block->terminator.as.jump_target = target_block_id;
    return 1;
}

int value_ssa_block_set_branch(ValueSsaBasicBlock *block,
    ValueSsaValueRef condition,
    size_t then_target,
    size_t else_target,
    ValueSsaError *error) {
    if (!block) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-034: invalid branch terminator contract");
        return 0;
    }
    if (block->has_terminator) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-032: bb.%zu already has a terminator", block->id);
        return 0;
    }

    block->has_terminator = 1;
    block->terminator.kind = VALUE_SSA_TERM_BRANCH;
    block->terminator.as.branch.condition = condition;
    block->terminator.as.branch.then_target = then_target;
    block->terminator.as.branch.else_target = else_target;
    return 1;
}

#define VALUE_SSA_SPLIT_AGGREGATOR
#include "value_ssa_verify.inc"
#include "value_ssa_dump.inc"
#include "value_ssa_analysis.inc"
#include "value_ssa_alloc_prep.inc"
#include "value_ssa_alloc_worklist.inc"
#include "value_ssa_alloc_dump.inc"
#include "value_ssa_rename.inc"
#include "value_ssa_from_lower_ir.inc"
