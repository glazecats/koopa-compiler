#if defined(__INTELLISENSE__) && !defined(MACHINE_IR_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_IR_SPLIT_AGGREGATOR)
#define MACHINE_IR_SPLIT_INTELLISENSE_CONTEXT
#include "machine_ir.c"
#else

void machine_ir_register_bank_init(MachineIrRegisterBank *bank) {
    if (!bank) {
        return;
    }

    bank->register_count = 0;
    bank->registers = NULL;
}

void machine_ir_register_bank_free(MachineIrRegisterBank *bank) {
    size_t register_index;

    if (!bank) {
        return;
    }

    for (register_index = 0; register_index < bank->register_count; ++register_index) {
        machine_ir_register_desc_free(&bank->registers[register_index]);
    }
    free(bank->registers);
    machine_ir_register_bank_init(bank);
}

void machine_ir_program_init(MachineIrProgram *program) {
    if (!program) {
        return;
    }

    machine_ir_register_bank_init(&program->register_bank);
    program->globals = NULL;
    program->global_count = 0;
    program->global_capacity = 0;
    program->functions = NULL;
    program->function_count = 0;
    program->function_capacity = 0;
}

void machine_ir_program_free(MachineIrProgram *program) {
    size_t global_index;
    size_t function_index;

    if (!program) {
        return;
    }

    machine_ir_register_bank_free(&program->register_bank);
    for (global_index = 0; global_index < program->global_count; ++global_index) {
        machine_ir_global_free(&program->globals[global_index]);
    }
    for (function_index = 0; function_index < program->function_count; ++function_index) {
        machine_ir_function_free(&program->functions[function_index]);
    }
    free(program->globals);
    free(program->functions);
    machine_ir_program_init(program);
}

void machine_ir_allocate_rewrite_report_init(MachineIrAllocateRewriteReport *report) {
    if (!report) {
        return;
    }

    report->allocation_rounds = 0;
    report->rewrite_rounds = 0;
    machine_ir_program_init(&report->program);
    report->function_shape_summaries = NULL;
    report->function_block_summary_offsets = NULL;
    report->block_shape_summaries = NULL;
    report->total_block_summary_count = 0;
    report->function_phi_counts = NULL;
    report->function_call_counts = NULL;
    report->function_spill_slot_counts = NULL;
    report->function_load_local_counts = NULL;
    report->function_store_local_counts = NULL;
    report->function_load_global_counts = NULL;
    report->function_store_global_counts = NULL;
    report->function_memory_op_counts = NULL;
    report->functions_with_phi_count = 0;
    report->functions_with_call_count = 0;
    report->functions_with_spills_count = 0;
    report->functions_with_memory_ops_count = 0;
    report->functions_register_only_count = 0;
    report->function_indices_with_phi = NULL;
    report->function_indices_with_call = NULL;
    report->function_indices_with_spills = NULL;
    report->function_indices_with_memory_ops = NULL;
    report->function_indices_register_only = NULL;
}

void machine_ir_allocate_rewrite_report_free(MachineIrAllocateRewriteReport *report) {
    if (!report) {
        return;
    }

    free(report->function_shape_summaries);
    free(report->function_block_summary_offsets);
    free(report->block_shape_summaries);
    free(report->function_phi_counts);
    free(report->function_call_counts);
    free(report->function_spill_slot_counts);
    free(report->function_load_local_counts);
    free(report->function_store_local_counts);
    free(report->function_load_global_counts);
    free(report->function_store_global_counts);
    free(report->function_memory_op_counts);
    free(report->function_indices_with_phi);
    free(report->function_indices_with_call);
    free(report->function_indices_with_spills);
    free(report->function_indices_with_memory_ops);
    free(report->function_indices_register_only);
    machine_ir_program_free(&report->program);
    machine_ir_allocate_rewrite_report_init(report);
}

MachineIrOperand machine_ir_operand_none(void) {
    MachineIrOperand operand;

    operand.kind = MACHINE_IR_OPERAND_NONE;
    operand.immediate = 0;
    operand.machine_register_id = 0;
    operand.spill_slot = 0;
    return operand;
}

MachineIrOperand machine_ir_operand_immediate(long long value) {
    MachineIrOperand operand = machine_ir_operand_none();

    operand.kind = MACHINE_IR_OPERAND_IMMEDIATE;
    operand.immediate = value;
    return operand;
}

MachineIrOperand machine_ir_operand_register(size_t machine_register_id) {
    MachineIrOperand operand = machine_ir_operand_none();

    operand.kind = MACHINE_IR_OPERAND_REGISTER;
    operand.machine_register_id = machine_register_id;
    return operand;
}

MachineIrOperand machine_ir_operand_spill_slot(size_t spill_slot) {
    MachineIrOperand operand = machine_ir_operand_none();

    operand.kind = MACHINE_IR_OPERAND_SPILL_SLOT;
    operand.spill_slot = spill_slot;
    return operand;
}

MachineIrSlotRef machine_ir_slot_local(size_t local_id) {
    MachineIrSlotRef slot;

    slot.kind = MACHINE_IR_SLOT_LOCAL;
    slot.id = local_id;
    return slot;
}

MachineIrSlotRef machine_ir_slot_global(size_t global_id) {
    MachineIrSlotRef slot;

    slot.kind = MACHINE_IR_SLOT_GLOBAL;
    slot.id = global_id;
    return slot;
}

int machine_ir_program_append_global(MachineIrProgram *program,
    const char *name,
    MachineIrGlobal **out_global,
    MachineIrError *error) {
    size_t next_capacity;
    MachineIrGlobal *new_globals;
    MachineIrGlobal *global;

    if (out_global) {
        *out_global = NULL;
    }
    if (!program || !name || name[0] == '\0') {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-010: invalid global append contract");
        return 0;
    }

    if (program->global_count == program->global_capacity) {
        if (!machine_ir_next_growth_capacity(
                program->global_capacity, 4, sizeof(MachineIrGlobal), &next_capacity)) {
            machine_ir_set_error(error, 0, 0, "MACHINE-IR-011: global table capacity overflow");
            return 0;
        }
        new_globals = (MachineIrGlobal *)realloc(program->globals, next_capacity * sizeof(MachineIrGlobal));
        if (!new_globals) {
            machine_ir_set_error(error, 0, 0, "MACHINE-IR-012: out of memory growing globals");
            return 0;
        }
        program->globals = new_globals;
        program->global_capacity = next_capacity;
    }

    global = &program->globals[program->global_count];
    memset(global, 0, sizeof(*global));
    global->id = program->global_count;
    global->name = machine_ir_strdup(name);
    if (!global->name) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-013: out of memory copying global name");
        return 0;
    }

    ++program->global_count;
    if (out_global) {
        *out_global = global;
    }
    return 1;
}

int machine_ir_program_append_function(MachineIrProgram *program,
    const char *name,
    int has_body,
    MachineIrFunction **out_function,
    MachineIrError *error) {
    size_t next_capacity;
    MachineIrFunction *new_functions;
    MachineIrFunction *function;

    if (out_function) {
        *out_function = NULL;
    }
    if (!program || !name || name[0] == '\0') {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-014: invalid function append contract");
        return 0;
    }

    if (program->function_count == program->function_capacity) {
        if (!machine_ir_next_growth_capacity(
                program->function_capacity, 4, sizeof(MachineIrFunction), &next_capacity)) {
            machine_ir_set_error(error, 0, 0, "MACHINE-IR-015: function table capacity overflow");
            return 0;
        }
        new_functions =
            (MachineIrFunction *)realloc(program->functions, next_capacity * sizeof(MachineIrFunction));
        if (!new_functions) {
            machine_ir_set_error(error, 0, 0, "MACHINE-IR-016: out of memory growing functions");
            return 0;
        }
        program->functions = new_functions;
        program->function_capacity = next_capacity;
    }

    function = &program->functions[program->function_count];
    memset(function, 0, sizeof(*function));
    function->name = machine_ir_strdup(name);
    if (!function->name) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-017: out of memory copying function name");
        return 0;
    }
    function->has_body = has_body ? 1 : 0;
    ++program->function_count;
    if (out_function) {
        *out_function = function;
    }
    return 1;
}

int machine_ir_function_append_local(MachineIrFunction *function,
    const char *source_name,
    int is_parameter,
    size_t *out_local_id,
    MachineIrError *error) {
    size_t next_capacity;
    MachineIrLocal *new_locals;
    MachineIrLocal *local;

    if (out_local_id) {
        *out_local_id = (size_t)-1;
    }
    if (!function || !function->has_body) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-018: cannot append local to declaration-only function");
        return 0;
    }

    if (function->local_count == function->local_capacity) {
        if (!machine_ir_next_growth_capacity(
                function->local_capacity, 4, sizeof(MachineIrLocal), &next_capacity)) {
            machine_ir_set_error(error, 0, 0, "MACHINE-IR-019: local table capacity overflow");
            return 0;
        }
        new_locals = (MachineIrLocal *)realloc(function->locals, next_capacity * sizeof(MachineIrLocal));
        if (!new_locals) {
            machine_ir_set_error(error, 0, 0, "MACHINE-IR-020: out of memory growing locals");
            return 0;
        }
        function->locals = new_locals;
        function->local_capacity = next_capacity;
    }

    local = &function->locals[function->local_count];
    memset(local, 0, sizeof(*local));
    local->id = function->local_count;
    local->source_name = source_name ? machine_ir_strdup(source_name) : NULL;
    if (source_name && !local->source_name) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-021: out of memory copying local name");
        return 0;
    }
    local->is_parameter = is_parameter ? 1 : 0;
    if (is_parameter) {
        if (function->local_count != function->parameter_count) {
            machine_ir_set_error(error, 0, 0, "MACHINE-IR-022: parameter locals must form a contiguous prefix");
            free(local->source_name);
            local->source_name = NULL;
            return 0;
        }
        ++function->parameter_count;
    }
    ++function->local_count;
    if (out_local_id) {
        *out_local_id = local->id;
    }
    return 1;
}

int machine_ir_function_append_block(MachineIrFunction *function,
    size_t *out_block_id,
    MachineIrBasicBlock **out_block,
    MachineIrError *error) {
    size_t next_capacity;
    MachineIrBasicBlock *new_blocks;
    MachineIrBasicBlock *block;

    if (out_block_id) {
        *out_block_id = (size_t)-1;
    }
    if (out_block) {
        *out_block = NULL;
    }
    if (!function || !function->has_body) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-023: cannot append block to declaration-only function");
        return 0;
    }

    if (function->block_count == function->block_capacity) {
        if (!machine_ir_next_growth_capacity(
                function->block_capacity, 4, sizeof(MachineIrBasicBlock), &next_capacity)) {
            machine_ir_set_error(error, 0, 0, "MACHINE-IR-024: block table capacity overflow");
            return 0;
        }
        new_blocks = (MachineIrBasicBlock *)realloc(function->blocks, next_capacity * sizeof(MachineIrBasicBlock));
        if (!new_blocks) {
            machine_ir_set_error(error, 0, 0, "MACHINE-IR-025: out of memory growing blocks");
            return 0;
        }
        function->blocks = new_blocks;
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

int machine_ir_block_append_phi(MachineIrBasicBlock *block,
    MachineIrOperand result,
    size_t *out_phi_index,
    MachineIrPhi **out_phi,
    MachineIrError *error) {
    size_t next_capacity;
    MachineIrPhi *new_phis;
    MachineIrPhi *phi;

    if (out_phi_index) {
        *out_phi_index = (size_t)-1;
    }
    if (out_phi) {
        *out_phi = NULL;
    }
    if (!block) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-026: invalid phi append contract");
        return 0;
    }
    if (result.kind != MACHINE_IR_OPERAND_REGISTER && result.kind != MACHINE_IR_OPERAND_SPILL_SLOT) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-027: phi result must be register or spill slot");
        return 0;
    }

    if (block->phi_count == block->phi_capacity) {
        if (!machine_ir_next_growth_capacity(block->phi_capacity, 4, sizeof(MachineIrPhi), &next_capacity)) {
            machine_ir_set_error(error, 0, 0, "MACHINE-IR-028: phi table capacity overflow");
            return 0;
        }
        new_phis = (MachineIrPhi *)realloc(block->phis, next_capacity * sizeof(MachineIrPhi));
        if (!new_phis) {
            machine_ir_set_error(error, 0, 0, "MACHINE-IR-029: out of memory growing phis");
            return 0;
        }
        block->phis = new_phis;
        block->phi_capacity = next_capacity;
    }

    phi = &block->phis[block->phi_count];
    memset(phi, 0, sizeof(*phi));
    phi->result = result;
    ++block->phi_count;
    if (out_phi_index) {
        *out_phi_index = block->phi_count - 1;
    }
    if (out_phi) {
        *out_phi = phi;
    }
    return 1;
}

int machine_ir_phi_append_input(MachineIrPhi *phi,
    size_t predecessor_block_id,
    MachineIrOperand value,
    MachineIrError *error) {
    size_t next_capacity;
    MachineIrPhiInput *new_inputs;

    if (!phi) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-030: invalid phi input append contract");
        return 0;
    }
    if (value.kind == MACHINE_IR_OPERAND_NONE) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-031: phi input cannot be empty");
        return 0;
    }

    if (phi->input_count == phi->input_capacity) {
        if (!machine_ir_next_growth_capacity(phi->input_capacity, 2, sizeof(MachineIrPhiInput), &next_capacity)) {
            machine_ir_set_error(error, 0, 0, "MACHINE-IR-032: phi-input table capacity overflow");
            return 0;
        }
        new_inputs = (MachineIrPhiInput *)realloc(phi->inputs, next_capacity * sizeof(MachineIrPhiInput));
        if (!new_inputs) {
            machine_ir_set_error(error, 0, 0, "MACHINE-IR-033: out of memory growing phi inputs");
            return 0;
        }
        phi->inputs = new_inputs;
        phi->input_capacity = next_capacity;
    }

    phi->inputs[phi->input_count].predecessor_block_id = predecessor_block_id;
    phi->inputs[phi->input_count].value = value;
    ++phi->input_count;
    return 1;
}

int machine_ir_block_append_instruction(MachineIrBasicBlock *block,
    const MachineIrInstruction *instruction,
    MachineIrError *error) {
    size_t next_capacity;
    MachineIrInstruction *new_instructions;

    if (!block || !instruction) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-034: invalid instruction append contract");
        return 0;
    }
    if (block->has_terminator) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-035: cannot append instruction after terminator");
        return 0;
    }

    if (block->instruction_count == block->instruction_capacity) {
        if (!machine_ir_next_growth_capacity(
                block->instruction_capacity, 8, sizeof(MachineIrInstruction), &next_capacity)) {
            machine_ir_set_error(error, 0, 0, "MACHINE-IR-036: instruction table capacity overflow");
            return 0;
        }
        new_instructions = (MachineIrInstruction *)realloc(
            block->instructions, next_capacity * sizeof(MachineIrInstruction));
        if (!new_instructions) {
            machine_ir_set_error(error, 0, 0, "MACHINE-IR-037: out of memory growing instructions");
            return 0;
        }
        block->instructions = new_instructions;
        block->instruction_capacity = next_capacity;
    }

    memset(&block->instructions[block->instruction_count], 0, sizeof(MachineIrInstruction));
    if (!machine_ir_instruction_copy(
            &block->instructions[block->instruction_count], instruction, error)) {
        return 0;
    }
    ++block->instruction_count;
    return 1;
}

int machine_ir_block_set_return(MachineIrBasicBlock *block,
    MachineIrOperand value,
    MachineIrError *error) {
    if (!block || value.kind == MACHINE_IR_OPERAND_NONE) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-038: invalid return terminator contract");
        return 0;
    }
    if (block->has_terminator) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-039: block terminator already set");
        return 0;
    }

    block->has_terminator = 1;
    block->terminator.kind = MACHINE_IR_TERM_RETURN;
    block->terminator.as.return_value = value;
    return 1;
}

int machine_ir_block_set_jump(MachineIrBasicBlock *block,
    size_t target_block_id,
    MachineIrError *error) {
    if (!block) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-040: invalid jump terminator contract");
        return 0;
    }
    if (block->has_terminator) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-041: block terminator already set");
        return 0;
    }

    block->has_terminator = 1;
    block->terminator.kind = MACHINE_IR_TERM_JUMP;
    block->terminator.as.jump_target = target_block_id;
    return 1;
}

int machine_ir_block_set_branch(MachineIrBasicBlock *block,
    MachineIrOperand condition,
    size_t then_target,
    size_t else_target,
    MachineIrError *error) {
    if (!block || condition.kind == MACHINE_IR_OPERAND_NONE) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-042: invalid branch terminator contract");
        return 0;
    }
    if (block->has_terminator) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-043: block terminator already set");
        return 0;
    }

    block->has_terminator = 1;
    block->terminator.kind = MACHINE_IR_TERM_BRANCH;
    block->terminator.as.branch.condition = condition;
    block->terminator.as.branch.then_target = then_target;
    block->terminator.as.branch.else_target = else_target;
    return 1;
}

static int machine_ir_replace_block_with_single_instruction_and_terminator(
    MachineIrBasicBlock *block,
    const MachineIrInstruction *instruction,
    const MachineIrTerminator *terminator,
    MachineIrError *error) {
    size_t instruction_index;
    MachineIrInstruction replacement_instruction;

    if (!block || !instruction || !terminator) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-044: invalid single-instruction block rewrite contract");
        return 0;
    }

    memset(&replacement_instruction, 0, sizeof(replacement_instruction));
    if (!machine_ir_instruction_copy(&replacement_instruction, instruction, error)) {
        return 0;
    }

    for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
        machine_ir_instruction_free(&block->instructions[instruction_index]);
        memset(&block->instructions[instruction_index], 0, sizeof(block->instructions[instruction_index]));
    }
    block->instruction_count = 0;
    block->has_terminator = 0;

    if (block->instruction_capacity == 0) {
        block->instructions = (MachineIrInstruction *)calloc(1, sizeof(MachineIrInstruction));
        if (!block->instructions) {
            machine_ir_instruction_free(&replacement_instruction);
            machine_ir_set_error(error, 0, 0, "MACHINE-IR-045: out of memory growing rewritten block instruction table");
            return 0;
        }
        block->instruction_capacity = 1;
    }

    block->instructions[0] = replacement_instruction;
    block->instruction_count = 1;
    block->terminator = *terminator;
    block->has_terminator = 1;
    return 1;
}

#endif
