#include "memory_ssa.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} MemorySsaStringBuilder;

static void memory_ssa_set_error(MemorySsaError *error, int line, int column, const char *fmt, ...);
static int memory_ssa_next_growth_capacity(size_t current,
    size_t initial,
    size_t elem_size,
    size_t *out_next_capacity);
static char *memory_ssa_strdup(const char *text);
static void memory_ssa_instruction_clear(MemorySsaInstruction *instruction);
static void memory_ssa_instruction_free(MemorySsaInstruction *instruction);
static int memory_ssa_copy_value_instruction(ValueSsaInstruction *destination,
    const ValueSsaInstruction *source,
    MemorySsaError *error);
static int memory_ssa_instruction_deep_copy(MemorySsaInstruction *destination,
    const MemorySsaInstruction *source,
    MemorySsaError *error);
static void memory_ssa_phi_free(MemorySsaPhi *phi);
static void memory_ssa_basic_block_free(MemorySsaBasicBlock *block);
static void memory_ssa_function_free(MemorySsaFunction *function);
static int memory_ssa_append_string(MemorySsaStringBuilder *builder, const char *text);
static int memory_ssa_append_format(MemorySsaStringBuilder *builder, const char *fmt, ...);

static void memory_ssa_set_error(MemorySsaError *error, int line, int column, const char *fmt, ...) {
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

static int memory_ssa_next_growth_capacity(size_t current,
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

static char *memory_ssa_strdup(const char *text) {
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

static void memory_ssa_instruction_clear(MemorySsaInstruction *instruction) {
    if (!instruction) {
        return;
    }

    memset(instruction, 0, sizeof(*instruction));
}

static void memory_ssa_instruction_free(MemorySsaInstruction *instruction) {
    if (!instruction) {
        return;
    }

    if (instruction->instruction.kind == VALUE_SSA_INSTR_CALL) {
        free(instruction->instruction.as.call.callee_name);
        free(instruction->instruction.as.call.args);
    }
    free(instruction->accesses);
    memory_ssa_instruction_clear(instruction);
}

static int memory_ssa_copy_value_instruction(ValueSsaInstruction *destination,
    const ValueSsaInstruction *source,
    MemorySsaError *error) {
    if (!destination || !source) {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-001: invalid value instruction copy contract");
        return 0;
    }

    *destination = *source;
    if (source->kind == VALUE_SSA_INSTR_CALL) {
        size_t arg_bytes = 0;

        destination->as.call.callee_name = NULL;
        destination->as.call.args = NULL;
        if (source->as.call.callee_name) {
            destination->as.call.callee_name = memory_ssa_strdup(source->as.call.callee_name);
            if (!destination->as.call.callee_name) {
                memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-002: out of memory while copying call callee");
                return 0;
            }
        }
        if (source->as.call.arg_count > 0) {
            arg_bytes = source->as.call.arg_count * sizeof(ValueSsaValueRef);
            destination->as.call.args = (ValueSsaValueRef *)malloc(arg_bytes);
            if (!destination->as.call.args) {
                free(destination->as.call.callee_name);
                destination->as.call.callee_name = NULL;
                memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-003: out of memory while copying call args");
                return 0;
            }
            memcpy(destination->as.call.args, source->as.call.args, arg_bytes);
        }
    }

    return 1;
}

static int memory_ssa_instruction_deep_copy(MemorySsaInstruction *destination,
    const MemorySsaInstruction *source,
    MemorySsaError *error) {
    memory_ssa_instruction_clear(destination);
    if (!memory_ssa_copy_value_instruction(&destination->instruction, &source->instruction, error)) {
        return 0;
    }

    if (source->access_count > 0) {
        size_t bytes = source->access_count * sizeof(MemorySsaAccess);

        destination->accesses = (MemorySsaAccess *)malloc(bytes);
        if (!destination->accesses) {
            memory_ssa_instruction_free(destination);
            memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-004: out of memory while copying memory accesses");
            return 0;
        }

        memcpy(destination->accesses, source->accesses, bytes);
        destination->access_count = source->access_count;
        destination->access_capacity = source->access_count;
    }

    return 1;
}

static void memory_ssa_phi_free(MemorySsaPhi *phi) {
    if (!phi) {
        return;
    }

    free(phi->inputs);
    memset(phi, 0, sizeof(*phi));
}

static void memory_ssa_basic_block_free(MemorySsaBasicBlock *block) {
    size_t phi_index;
    size_t instruction_index;

    if (!block) {
        return;
    }

    for (phi_index = 0; phi_index < block->phi_count; ++phi_index) {
        memory_ssa_phi_free(&block->phis[phi_index]);
    }
    for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
        memory_ssa_instruction_free(&block->instructions[instruction_index]);
    }

    free(block->phis);
    free(block->instructions);
    memset(block, 0, sizeof(*block));
}

static void memory_ssa_function_free(MemorySsaFunction *function) {
    size_t slot_index;
    size_t block_index;

    if (!function) {
        return;
    }

    free(function->name);
    for (slot_index = 0; slot_index < function->tracked_slot_count; ++slot_index) {
        free(function->tracked_slots[slot_index].debug_name);
    }
    for (block_index = 0; block_index < function->block_count; ++block_index) {
        memory_ssa_basic_block_free(&function->blocks[block_index]);
    }

    free(function->tracked_slots);
    free(function->blocks);
    memset(function, 0, sizeof(*function));
}

static int memory_ssa_append_string(MemorySsaStringBuilder *builder, const char *text) {
    size_t needed;
    size_t text_length;
    size_t next_capacity;
    char *next_data;

    if (!builder || !text) {
        return 0;
    }

    text_length = strlen(text);
    needed = builder->length + text_length + 1;
    if (needed > builder->capacity) {
        next_capacity = builder->capacity;
        while (next_capacity < needed) {
            if (!memory_ssa_next_growth_capacity(next_capacity, 64, sizeof(char), &next_capacity)) {
                return 0;
            }
        }
        next_data = (char *)realloc(builder->data, next_capacity);
        if (!next_data) {
            return 0;
        }
        builder->data = next_data;
        builder->capacity = next_capacity;
    }

    memcpy(builder->data + builder->length, text, text_length + 1);
    builder->length += text_length;
    return 1;
}

static int memory_ssa_append_format(MemorySsaStringBuilder *builder, const char *fmt, ...) {
    char buffer[256];
    va_list args;
    int written;

    if (!builder || !fmt) {
        return 0;
    }

    va_start(args, fmt);
    written = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    if (written < 0 || (size_t)written >= sizeof(buffer)) {
        return 0;
    }

    return memory_ssa_append_string(builder, buffer);
}

void memory_ssa_program_init(MemorySsaProgram *program) {
    if (!program) {
        return;
    }

    program->functions = NULL;
    program->function_count = 0;
    program->function_capacity = 0;
}

void memory_ssa_program_free(MemorySsaProgram *program) {
    size_t function_index;

    if (!program) {
        return;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        memory_ssa_function_free(&program->functions[function_index]);
    }
    free(program->functions);
    memory_ssa_program_init(program);
}

int memory_ssa_program_append_function(MemorySsaProgram *program,
    const char *name,
    int has_body,
    MemorySsaFunction **out_function,
    MemorySsaError *error) {
    MemorySsaFunction *function;
    size_t next_capacity;
    MemorySsaFunction *next_functions;

    if (!program || !name || name[0] == '\0') {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-005: invalid function append contract");
        return 0;
    }

    if (program->function_count == program->function_capacity) {
        if (!memory_ssa_next_growth_capacity(
                program->function_capacity, 4, sizeof(MemorySsaFunction), &next_capacity)) {
            memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-006: function table overflow");
            return 0;
        }
        next_functions = (MemorySsaFunction *)realloc(program->functions,
            next_capacity * sizeof(MemorySsaFunction));
        if (!next_functions) {
            memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-007: out of memory while growing function table");
            return 0;
        }
        program->functions = next_functions;
        program->function_capacity = next_capacity;
    }

    function = &program->functions[program->function_count];
    memset(function, 0, sizeof(*function));
    function->name = memory_ssa_strdup(name);
    if (!function->name) {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-008: out of memory while copying function name");
        return 0;
    }
    function->has_body = has_body;
    ++program->function_count;
    if (out_function) {
        *out_function = function;
    }
    return 1;
}

int memory_ssa_function_append_tracked_slot(MemorySsaFunction *function,
    ValueSsaSlotRef slot,
    const char *debug_name,
    size_t *out_slot_id,
    MemorySsaError *error) {
    MemorySsaTrackedSlot *tracked_slot;
    size_t next_capacity;
    MemorySsaTrackedSlot *next_slots;

    if (!function || !function->has_body || !debug_name || debug_name[0] == '\0') {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-009: invalid tracked-slot append contract");
        return 0;
    }

    if (function->tracked_slot_count == function->tracked_slot_capacity) {
        if (!memory_ssa_next_growth_capacity(
                function->tracked_slot_capacity, 4, sizeof(MemorySsaTrackedSlot), &next_capacity)) {
            memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-010: tracked-slot table overflow");
            return 0;
        }
        next_slots = (MemorySsaTrackedSlot *)realloc(
            function->tracked_slots, next_capacity * sizeof(MemorySsaTrackedSlot));
        if (!next_slots) {
            memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-011: out of memory while growing tracked-slot table");
            return 0;
        }
        function->tracked_slots = next_slots;
        function->tracked_slot_capacity = next_capacity;
    }

    tracked_slot = &function->tracked_slots[function->tracked_slot_count];
    memset(tracked_slot, 0, sizeof(*tracked_slot));
    tracked_slot->id = function->tracked_slot_count;
    tracked_slot->slot = slot;
    tracked_slot->debug_name = memory_ssa_strdup(debug_name);
    tracked_slot->has_entry_version = 0;
    tracked_slot->entry_version_id = 0;
    if (!tracked_slot->debug_name) {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-012: out of memory while copying tracked-slot name");
        return 0;
    }

    if (out_slot_id) {
        *out_slot_id = tracked_slot->id;
    }
    ++function->tracked_slot_count;
    return 1;
}

int memory_ssa_function_set_entry_version(MemorySsaFunction *function,
    size_t slot_id,
    size_t version_id,
    MemorySsaError *error) {
    MemorySsaTrackedSlot *slot;

    if (!function || slot_id >= function->tracked_slot_count) {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-081: invalid entry-version contract");
        return 0;
    }
    if (version_id >= function->next_version_id) {
        memory_ssa_set_error(error,
            0,
            0,
            "MEMORY-SSA-082: slot.%zu entry version mem.%zu is out of range",
            slot_id,
            version_id);
        return 0;
    }

    slot = &function->tracked_slots[slot_id];
    slot->has_entry_version = 1;
    slot->entry_version_id = version_id;
    return 1;
}

int memory_ssa_function_append_block(MemorySsaFunction *function,
    size_t *out_block_id,
    MemorySsaBasicBlock **out_block,
    MemorySsaError *error) {
    MemorySsaBasicBlock *block;
    size_t next_capacity;
    MemorySsaBasicBlock *next_blocks;

    if (!function || !function->has_body) {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-013: invalid block append contract");
        return 0;
    }

    if (function->block_count == function->block_capacity) {
        if (!memory_ssa_next_growth_capacity(
                function->block_capacity, 4, sizeof(MemorySsaBasicBlock), &next_capacity)) {
            memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-014: block table overflow");
            return 0;
        }
        next_blocks = (MemorySsaBasicBlock *)realloc(
            function->blocks, next_capacity * sizeof(MemorySsaBasicBlock));
        if (!next_blocks) {
            memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-015: out of memory while growing block table");
            return 0;
        }
        function->blocks = next_blocks;
        function->block_capacity = next_capacity;
    }

    block = &function->blocks[function->block_count];
    memset(block, 0, sizeof(*block));
    block->id = function->block_count;
    ++function->block_count;
    if (out_block_id) {
        *out_block_id = block->id;
    }
    if (out_block) {
        *out_block = block;
    }
    return 1;
}

size_t memory_ssa_function_allocate_version(MemorySsaFunction *function) {
    if (!function || !function->has_body || function->next_version_id == (size_t)-1) {
        return (size_t)-1;
    }
    return function->next_version_id++;
}

int memory_ssa_block_append_phi(MemorySsaBasicBlock *block,
    size_t slot_id,
    size_t result_version_id,
    const MemorySsaPhiInput *inputs,
    size_t input_count,
    MemorySsaError *error) {
    MemorySsaPhi *phi;
    size_t next_capacity;
    MemorySsaPhi *next_phis;

    if (!block || !inputs || input_count == 0) {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-016: invalid phi append contract");
        return 0;
    }

    if (block->phi_count == block->phi_capacity) {
        if (!memory_ssa_next_growth_capacity(block->phi_capacity, 2, sizeof(MemorySsaPhi), &next_capacity)) {
            memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-017: phi table overflow");
            return 0;
        }
        next_phis = (MemorySsaPhi *)realloc(block->phis, next_capacity * sizeof(MemorySsaPhi));
        if (!next_phis) {
            memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-018: out of memory while growing phi table");
            return 0;
        }
        block->phis = next_phis;
        block->phi_capacity = next_capacity;
    }

    phi = &block->phis[block->phi_count];
    memset(phi, 0, sizeof(*phi));
    phi->slot_id = slot_id;
    phi->result_version_id = result_version_id;
    phi->inputs = (MemorySsaPhiInput *)malloc(input_count * sizeof(MemorySsaPhiInput));
    if (!phi->inputs) {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-019: out of memory while copying phi inputs");
        return 0;
    }
    memcpy(phi->inputs, inputs, input_count * sizeof(MemorySsaPhiInput));
    phi->input_count = input_count;
    phi->input_capacity = input_count;
    ++block->phi_count;
    return 1;
}

int memory_ssa_instruction_append_access(MemorySsaInstruction *instruction,
    const MemorySsaAccess *access,
    MemorySsaError *error) {
    MemorySsaAccess *next_accesses;
    size_t next_capacity;

    if (!instruction || !access) {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-020: invalid access append contract");
        return 0;
    }

    if (instruction->access_count == instruction->access_capacity) {
        if (!memory_ssa_next_growth_capacity(
                instruction->access_capacity, 1, sizeof(MemorySsaAccess), &next_capacity)) {
            memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-021: access table overflow");
            return 0;
        }
        next_accesses = (MemorySsaAccess *)realloc(
            instruction->accesses, next_capacity * sizeof(MemorySsaAccess));
        if (!next_accesses) {
            memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-022: out of memory while growing access table");
            return 0;
        }
        instruction->accesses = next_accesses;
        instruction->access_capacity = next_capacity;
    }

    instruction->accesses[instruction->access_count++] = *access;
    return 1;
}

int memory_ssa_block_append_instruction(MemorySsaBasicBlock *block,
    const MemorySsaInstruction *instruction,
    MemorySsaError *error) {
    MemorySsaInstruction *next_instruction;
    size_t next_capacity;
    MemorySsaInstruction *next_instructions;

    if (!block || !instruction) {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-023: invalid instruction append contract");
        return 0;
    }
    if (block->has_terminator) {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-024: bb.%zu already has a terminator", block->id);
        return 0;
    }

    if (block->instruction_count == block->instruction_capacity) {
        if (!memory_ssa_next_growth_capacity(
                block->instruction_capacity, 4, sizeof(MemorySsaInstruction), &next_capacity)) {
            memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-025: instruction table overflow");
            return 0;
        }
        next_instructions = (MemorySsaInstruction *)realloc(
            block->instructions, next_capacity * sizeof(MemorySsaInstruction));
        if (!next_instructions) {
            memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-026: out of memory while growing instruction table");
            return 0;
        }
        block->instructions = next_instructions;
        block->instruction_capacity = next_capacity;
    }

    next_instruction = &block->instructions[block->instruction_count];
    if (!memory_ssa_instruction_deep_copy(next_instruction, instruction, error)) {
        return 0;
    }

    ++block->instruction_count;
    return 1;
}

int memory_ssa_block_set_return(MemorySsaBasicBlock *block,
    ValueSsaValueRef value,
    MemorySsaError *error) {
    if (!block) {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-027: invalid return terminator contract");
        return 0;
    }
    if (block->has_terminator) {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-028: bb.%zu already has a terminator", block->id);
        return 0;
    }

    block->has_terminator = 1;
    block->terminator.kind = VALUE_SSA_TERM_RETURN;
    block->terminator.has_return_value = 1;
    block->terminator.as.return_value = value;
    return 1;
}

int memory_ssa_block_set_void_return(MemorySsaBasicBlock *block, MemorySsaError *error) {
    if (!block) {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-027: invalid return terminator contract");
        return 0;
    }
    if (block->has_terminator) {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-028: bb.%zu already has a terminator", block->id);
        return 0;
    }

    block->has_terminator = 1;
    block->terminator.kind = VALUE_SSA_TERM_RETURN;
    block->terminator.has_return_value = 0;
    return 1;
}

int memory_ssa_block_set_jump(MemorySsaBasicBlock *block,
    size_t target_block_id,
    MemorySsaError *error) {
    if (!block) {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-029: invalid jump terminator contract");
        return 0;
    }
    if (block->has_terminator) {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-028: bb.%zu already has a terminator", block->id);
        return 0;
    }

    block->has_terminator = 1;
    block->terminator.kind = VALUE_SSA_TERM_JUMP;
    block->terminator.as.jump_target = target_block_id;
    return 1;
}

int memory_ssa_block_set_branch(MemorySsaBasicBlock *block,
    ValueSsaValueRef condition,
    size_t then_target,
    size_t else_target,
    MemorySsaError *error) {
    if (!block) {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-030: invalid branch terminator contract");
        return 0;
    }
    if (block->has_terminator) {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-028: bb.%zu already has a terminator", block->id);
        return 0;
    }

    block->has_terminator = 1;
    block->terminator.kind = VALUE_SSA_TERM_BRANCH;
    block->terminator.as.branch.condition = condition;
    block->terminator.as.branch.then_target = then_target;
    block->terminator.as.branch.else_target = else_target;
    return 1;
}

#define MEMORY_SSA_SPLIT_AGGREGATOR
#include "memory_ssa_verify.inc"
#include "memory_ssa_dump.inc"
#include "memory_ssa_analysis.inc"
#include "memory_ssa_from_value_ssa.inc"
