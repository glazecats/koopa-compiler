#include "machine/ir.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} MachineIrStringBuilder;

static void machine_ir_set_error(MachineIrError *error, int line, int column, const char *fmt, ...);
static void machine_ir_set_error_from_value_ssa(MachineIrError *error, const ValueSsaError *value_error);
static int machine_ir_next_growth_capacity(size_t current,
    size_t initial,
    size_t elem_size,
    size_t *out_next_capacity);
static char *machine_ir_strdup(const char *text);
static int machine_ir_clone_value_ssa_program(const ValueSsaProgram *source,
    ValueSsaProgram *out_program,
    MachineIrError *error);
static int machine_ir_clone_program(const MachineIrProgram *source,
    MachineIrProgram *out_program,
    MachineIrError *error);
static int machine_ir_append_format(MachineIrStringBuilder *builder, const char *fmt, ...);
static void machine_ir_phi_free(MachineIrPhi *phi);
static void machine_ir_instruction_free(MachineIrInstruction *instruction);
static void machine_ir_basic_block_free(MachineIrBasicBlock *block);
static void machine_ir_function_free(MachineIrFunction *function);
static void machine_ir_global_free(MachineIrGlobal *global);
static void machine_ir_register_desc_free(MachineIrRegisterDesc *desc);
static int machine_ir_instruction_copy(MachineIrInstruction *destination,
    const MachineIrInstruction *source,
    MachineIrError *error);
static int machine_ir_replace_block_with_single_instruction_and_terminator(
    MachineIrBasicBlock *block,
    const MachineIrInstruction *instruction,
    const MachineIrTerminator *terminator,
    MachineIrError *error);
static int machine_ir_operands_equal(MachineIrOperand lhs, MachineIrOperand rhs);
static int machine_ir_compact_empty_jump_blocks(MachineIrFunction *function,
    int *out_changed,
    MachineIrError *error);
static int machine_ir_remove_unreachable_blocks(MachineIrFunction *function,
    int *out_changed,
    MachineIrError *error);
static int machine_ir_cleanup_local_copies_in_function(MachineIrProgram *program,
    MachineIrFunction *function,
    int *out_changed,
    MachineIrError *error);
static int machine_ir_cleanup_known_slot_values(MachineIrProgram *program,
    int *out_changed,
    MachineIrError *error);
static int machine_ir_cleanup_machine_values(MachineIrProgram *program,
    int *out_changed,
    MachineIrError *error);

static void machine_ir_set_error(MachineIrError *error, int line, int column, const char *fmt, ...) {
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

static void machine_ir_set_error_from_value_ssa(MachineIrError *error, const ValueSsaError *value_error) {
    if (!error || !value_error) {
        return;
    }

    error->line = value_error->line;
    error->column = value_error->column;
    memcpy(error->message, value_error->message, sizeof(error->message));
    error->message[sizeof(error->message) - 1] = '\0';
}

static int machine_ir_next_growth_capacity(size_t current,
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

static char *machine_ir_strdup(const char *text) {
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

static int machine_ir_clone_value_ssa_program(const ValueSsaProgram *source,
    ValueSsaProgram *out_program,
    MachineIrError *error) {
    ValueSsaError value_error;
    size_t global_index;
    size_t function_index;

    if (!source || !out_program) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-004: invalid Value-SSA clone contract");
        return 0;
    }

    memset(&value_error, 0, sizeof(value_error));
    value_ssa_program_free(out_program);
    value_ssa_program_init(out_program);

    for (global_index = 0; global_index < source->global_count; ++global_index) {
        ValueSsaGlobal *dest_global = NULL;

        if (!value_ssa_program_append_global(out_program, source->globals[global_index].name, &dest_global, &value_error) ||
            !dest_global) {
            machine_ir_set_error_from_value_ssa(error, &value_error);
            value_ssa_program_free(out_program);
            return 0;
        }
        dest_global->byte_size = source->globals[global_index].byte_size;
        dest_global->has_initializer = source->globals[global_index].has_initializer;
        dest_global->initializer_value = source->globals[global_index].initializer_value;
        dest_global->has_runtime_initializer = source->globals[global_index].has_runtime_initializer;
    }

    for (function_index = 0; function_index < source->function_count; ++function_index) {
        const ValueSsaFunction *source_function = &source->functions[function_index];
        ValueSsaFunction *dest_function = NULL;
        size_t local_index;
        size_t block_index;

        if (!value_ssa_program_append_function(
                out_program, source_function->name, source_function->has_body, &dest_function, &value_error) ||
            !dest_function) {
            machine_ir_set_error_from_value_ssa(error, &value_error);
            value_ssa_program_free(out_program);
            return 0;
        }

        for (local_index = 0; local_index < source_function->local_count; ++local_index) {
            if (!value_ssa_function_append_local(dest_function,
                    source_function->locals[local_index].source_name,
                    source_function->locals[local_index].is_parameter,
                    NULL,
                    &value_error)) {
                machine_ir_set_error_from_value_ssa(error, &value_error);
                value_ssa_program_free(out_program);
                return 0;
            }
        }

        for (block_index = 0; block_index < source_function->block_count; ++block_index) {
            const ValueSsaBasicBlock *source_block = &source_function->blocks[block_index];
            ValueSsaBasicBlock *dest_block = NULL;
            size_t phi_index;
            size_t instruction_index;

            if (!value_ssa_function_append_block(dest_function, NULL, &dest_block, &value_error) || !dest_block) {
                machine_ir_set_error_from_value_ssa(error, &value_error);
                value_ssa_program_free(out_program);
                return 0;
            }

            for (phi_index = 0; phi_index < source_block->phi_count; ++phi_index) {
                if (!value_ssa_block_append_phi(dest_block,
                        source_block->phis[phi_index].result_id,
                        source_block->phis[phi_index].inputs,
                        source_block->phis[phi_index].input_count,
                        &value_error)) {
                    machine_ir_set_error_from_value_ssa(error, &value_error);
                    value_ssa_program_free(out_program);
                    return 0;
                }
            }

            for (instruction_index = 0; instruction_index < source_block->instruction_count; ++instruction_index) {
                if (!value_ssa_block_append_instruction(
                        dest_block, &source_block->instructions[instruction_index], &value_error)) {
                    machine_ir_set_error_from_value_ssa(error, &value_error);
                    value_ssa_program_free(out_program);
                    return 0;
                }
            }

            if (source_block->has_terminator) {
                switch (source_block->terminator.kind) {
                    case VALUE_SSA_TERM_RETURN:
                        if ((source_block->terminator.has_return_value &&
                                !value_ssa_block_set_return(
                                    dest_block, source_block->terminator.as.return_value, &value_error)) ||
                            (!source_block->terminator.has_return_value &&
                                !value_ssa_block_set_void_return(dest_block, &value_error))) {
                            machine_ir_set_error_from_value_ssa(error, &value_error);
                            value_ssa_program_free(out_program);
                            return 0;
                        }
                        break;
                    case VALUE_SSA_TERM_JUMP:
                        if (!value_ssa_block_set_jump(
                                dest_block, source_block->terminator.as.jump_target, &value_error)) {
                            machine_ir_set_error_from_value_ssa(error, &value_error);
                            value_ssa_program_free(out_program);
                            return 0;
                        }
                        break;
                    case VALUE_SSA_TERM_BRANCH:
                        if (!value_ssa_block_set_branch(dest_block,
                                source_block->terminator.as.branch.condition,
                                source_block->terminator.as.branch.then_target,
                                source_block->terminator.as.branch.else_target,
                                &value_error)) {
                            machine_ir_set_error_from_value_ssa(error, &value_error);
                            value_ssa_program_free(out_program);
                            return 0;
                        }
                        break;
                    default:
                        machine_ir_set_error(error, 0, 0, "MACHINE-IR-005: unsupported Value-SSA terminator in clone");
                        value_ssa_program_free(out_program);
                        return 0;
                }
            }
        }

        dest_function->next_value_id = source_function->next_value_id;
    }

    return 1;
}

static int machine_ir_clone_program(const MachineIrProgram *source,
    MachineIrProgram *out_program,
    MachineIrError *error) {
    size_t register_index;
    size_t global_index;
    size_t function_index;

    if (!source || !out_program) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-006: invalid machine-ir clone contract");
        return 0;
    }

    machine_ir_program_free(out_program);
    machine_ir_program_init(out_program);

    out_program->register_bank.register_count = source->register_bank.register_count;
    if (source->register_bank.register_count > 0) {
        out_program->register_bank.registers = (MachineIrRegisterDesc *)calloc(
            source->register_bank.register_count, sizeof(MachineIrRegisterDesc));
        if (!out_program->register_bank.registers) {
            machine_ir_set_error(error, 0, 0, "MACHINE-IR-007: out of memory cloning register bank");
            machine_ir_program_free(out_program);
            return 0;
        }
    }
    for (register_index = 0; register_index < source->register_bank.register_count; ++register_index) {
        out_program->register_bank.registers[register_index].register_id =
            source->register_bank.registers[register_index].register_id;
        out_program->register_bank.registers[register_index].name =
            machine_ir_strdup(source->register_bank.registers[register_index].name);
        if (source->register_bank.registers[register_index].name &&
            !out_program->register_bank.registers[register_index].name) {
            machine_ir_set_error(error, 0, 0, "MACHINE-IR-008: out of memory cloning register name");
            machine_ir_program_free(out_program);
            return 0;
        }
        out_program->register_bank.registers[register_index].register_class =
            source->register_bank.registers[register_index].register_class;
        out_program->register_bank.registers[register_index].allocatable =
            source->register_bank.registers[register_index].allocatable;
        out_program->register_bank.registers[register_index].caller_clobbered =
            source->register_bank.registers[register_index].caller_clobbered;
        out_program->register_bank.registers[register_index].callee_preserved =
            source->register_bank.registers[register_index].callee_preserved;
    }

    for (global_index = 0; global_index < source->global_count; ++global_index) {
        MachineIrGlobal *global = NULL;

        if (!machine_ir_program_append_global(out_program, source->globals[global_index].name, &global, error) || !global) {
            machine_ir_program_free(out_program);
            return 0;
        }
        global->byte_size = source->globals[global_index].byte_size;
        global->has_initializer = source->globals[global_index].has_initializer;
        global->initializer_value = source->globals[global_index].initializer_value;
        global->has_runtime_initializer = source->globals[global_index].has_runtime_initializer;
    }

    for (function_index = 0; function_index < source->function_count; ++function_index) {
        const MachineIrFunction *source_function = &source->functions[function_index];
        MachineIrFunction *dest_function = NULL;
        size_t local_index;
        size_t block_index;

        if (!machine_ir_program_append_function(
                out_program, source_function->name, source_function->has_body, &dest_function, error) ||
            !dest_function) {
            machine_ir_program_free(out_program);
            return 0;
        }
        dest_function->spill_slot_count = source_function->spill_slot_count;

        for (local_index = 0; local_index < source_function->local_count; ++local_index) {
            if (!machine_ir_function_append_local(dest_function,
                    source_function->locals[local_index].source_name,
                    source_function->locals[local_index].is_parameter,
                    NULL,
                    error)) {
                machine_ir_program_free(out_program);
                return 0;
            }
        }

        for (block_index = 0; block_index < source_function->block_count; ++block_index) {
            const MachineIrBasicBlock *source_block = &source_function->blocks[block_index];
            MachineIrBasicBlock *dest_block = NULL;
            size_t phi_index;
            size_t instruction_index;

            if (!machine_ir_function_append_block(dest_function, NULL, &dest_block, error) || !dest_block) {
                machine_ir_program_free(out_program);
                return 0;
            }
            for (phi_index = 0; phi_index < source_block->phi_count; ++phi_index) {
                MachineIrPhi *dest_phi = NULL;
                size_t input_index;

                if (!machine_ir_block_append_phi(dest_block, source_block->phis[phi_index].result, NULL, &dest_phi, error) ||
                    !dest_phi) {
                    machine_ir_program_free(out_program);
                    return 0;
                }
                for (input_index = 0; input_index < source_block->phis[phi_index].input_count; ++input_index) {
                    if (!machine_ir_phi_append_input(dest_phi,
                            source_block->phis[phi_index].inputs[input_index].predecessor_block_id,
                            source_block->phis[phi_index].inputs[input_index].value,
                            error)) {
                        machine_ir_program_free(out_program);
                        return 0;
                    }
                }
            }
            for (instruction_index = 0; instruction_index < source_block->instruction_count; ++instruction_index) {
                if (!machine_ir_block_append_instruction(dest_block,
                        &source_block->instructions[instruction_index],
                        error)) {
                    machine_ir_program_free(out_program);
                    return 0;
                }
            }
            if (source_block->has_terminator) {
                switch (source_block->terminator.kind) {
                    case MACHINE_IR_TERM_RETURN:
                        if ((source_block->terminator.as.return_value.kind == MACHINE_IR_OPERAND_NONE &&
                                !machine_ir_block_set_void_return(dest_block, error)) ||
                            (source_block->terminator.as.return_value.kind != MACHINE_IR_OPERAND_NONE &&
                                !machine_ir_block_set_return(dest_block, source_block->terminator.as.return_value, error))) {
                            machine_ir_program_free(out_program);
                            return 0;
                        }
                        break;
                    case MACHINE_IR_TERM_JUMP:
                        if (!machine_ir_block_set_jump(dest_block, source_block->terminator.as.jump_target, error)) {
                            machine_ir_program_free(out_program);
                            return 0;
                        }
                        break;
                    case MACHINE_IR_TERM_BRANCH:
                        if (!machine_ir_block_set_branch(dest_block,
                                source_block->terminator.as.branch.condition,
                                source_block->terminator.as.branch.then_target,
                                source_block->terminator.as.branch.else_target,
                                error)) {
                            machine_ir_program_free(out_program);
                            return 0;
                        }
                        break;
                    default:
                        machine_ir_set_error(error, 0, 0, "MACHINE-IR-009: unsupported machine-ir terminator in clone");
                        machine_ir_program_free(out_program);
                        return 0;
                }
            }
        }
    }

    return 1;
}

static int machine_ir_append_format(MachineIrStringBuilder *builder, const char *fmt, ...) {
    va_list args;
    va_list args_copy;
    int needed;
    size_t required;
    size_t new_capacity;
    char *new_data;

    if (!builder || !fmt) {
        return 0;
    }

    va_start(args, fmt);
    va_copy(args_copy, args);
    needed = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);
    if (needed < 0) {
        va_end(args);
        return 0;
    }

    required = builder->length + (size_t)needed + 1;
    if (required > builder->capacity) {
        new_capacity = builder->capacity == 0 ? 128 : builder->capacity;
        while (new_capacity < required) {
            size_t doubled = new_capacity * 2;

            if (doubled <= new_capacity) {
                new_capacity = required;
                break;
            }
            new_capacity = doubled;
        }
        new_data = (char *)realloc(builder->data, new_capacity);
        if (!new_data) {
            va_end(args);
            return 0;
        }
        builder->data = new_data;
        builder->capacity = new_capacity;
    }

    vsnprintf(builder->data + builder->length, builder->capacity - builder->length, fmt, args);
    va_end(args);
    builder->length += (size_t)needed;
    return 1;
}

static void machine_ir_phi_free(MachineIrPhi *phi) {
    if (!phi) {
        return;
    }

    free(phi->inputs);
    phi->inputs = NULL;
    phi->input_count = 0;
    phi->input_capacity = 0;
    phi->result = machine_ir_operand_none();
}

static void machine_ir_instruction_free(MachineIrInstruction *instruction) {
    if (!instruction) {
        return;
    }

    if (instruction->kind == MACHINE_IR_INSTR_CALL) {
        free(instruction->as.call.callee_name);
        free(instruction->as.call.args);
        instruction->as.call.callee_name = NULL;
        instruction->as.call.args = NULL;
        instruction->as.call.arg_count = 0;
    }
}

static void machine_ir_basic_block_free(MachineIrBasicBlock *block) {
    size_t phi_index;
    size_t instruction_index;

    if (!block) {
        return;
    }

    for (phi_index = 0; phi_index < block->phi_count; ++phi_index) {
        machine_ir_phi_free(&block->phis[phi_index]);
    }
    for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
        machine_ir_instruction_free(&block->instructions[instruction_index]);
    }

    free(block->phis);
    free(block->instructions);
    block->phis = NULL;
    block->phi_count = 0;
    block->phi_capacity = 0;
    block->instructions = NULL;
    block->instruction_count = 0;
    block->instruction_capacity = 0;
    block->has_terminator = 0;
}

static void machine_ir_function_free(MachineIrFunction *function) {
    size_t local_index;
    size_t block_index;

    if (!function) {
        return;
    }

    free(function->name);
    function->name = NULL;
    for (local_index = 0; local_index < function->local_count; ++local_index) {
        free(function->locals[local_index].source_name);
    }
    for (block_index = 0; block_index < function->block_count; ++block_index) {
        machine_ir_basic_block_free(&function->blocks[block_index]);
    }
    free(function->locals);
    free(function->blocks);
    function->locals = NULL;
    function->blocks = NULL;
    function->local_count = 0;
    function->local_capacity = 0;
    function->block_count = 0;
    function->block_capacity = 0;
    function->parameter_count = 0;
    function->spill_slot_count = 0;
    function->has_body = 0;
}

static void machine_ir_global_free(MachineIrGlobal *global) {
    if (!global) {
        return;
    }

    free(global->name);
    global->name = NULL;
}

static void machine_ir_register_desc_free(MachineIrRegisterDesc *desc) {
    if (!desc) {
        return;
    }

    free(desc->name);
    desc->name = NULL;
}

static int machine_ir_instruction_copy(MachineIrInstruction *destination,
    const MachineIrInstruction *source,
    MachineIrError *error) {
    if (!destination || !source) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-001: invalid instruction copy contract");
        return 0;
    }

    *destination = *source;
    if (source->kind != MACHINE_IR_INSTR_CALL) {
        return 1;
    }

    destination->as.call.callee_name = NULL;
    destination->as.call.args = NULL;
    if (source->as.call.callee_name) {
        destination->as.call.callee_name = machine_ir_strdup(source->as.call.callee_name);
        if (!destination->as.call.callee_name) {
            machine_ir_set_error(error, 0, 0, "MACHINE-IR-002: out of memory copying callee name");
            return 0;
        }
    }
    if (source->as.call.arg_count > 0) {
        destination->as.call.args =
            (MachineIrOperand *)malloc(source->as.call.arg_count * sizeof(MachineIrOperand));
        if (!destination->as.call.args) {
            free(destination->as.call.callee_name);
            destination->as.call.callee_name = NULL;
            machine_ir_set_error(error, 0, 0, "MACHINE-IR-003: out of memory copying call args");
            return 0;
        }
        memcpy(destination->as.call.args,
            source->as.call.args,
            source->as.call.arg_count * sizeof(MachineIrOperand));
    }
    return 1;
}

#define MACHINE_IR_SPLIT_AGGREGATOR
#include "machine_ir_core.inc"
#include "machine_ir_query.inc"
#include "machine_ir_verify.inc"
#include "machine_ir_dump.inc"
#include "machine_ir_lower.inc"
#include "machine_ir_report.inc"
#include "machine_ir_phi_elim.inc"
#include "machine_ir_constraints.inc"
#include "machine_ir_cleanup.inc"
#include "machine_ir_call_effects.inc"
#include "machine_ir_copy_cleanup.inc"
#include "machine_ir_slot_cleanup.inc"
#include "machine_ir_value_cleanup.inc"
