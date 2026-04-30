#if defined(__INTELLISENSE__) && !defined(MACHINE_IR_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_IR_SPLIT_AGGREGATOR)
#define MACHINE_IR_SPLIT_INTELLISENSE_CONTEXT
#include "machine_ir.c"
#else

static MachineIrBinaryOp machine_ir_lower_binary_op(ValueSsaBinaryOp op) {
    return (MachineIrBinaryOp)op;
}

static MachineIrOperand machine_ir_lower_value_ref(const ValueSsaFunctionMachineAllocationView *machine_view,
    ValueSsaValueRef value,
    MachineIrError *error) {
    const ValueSsaMachineAllocatedValuePlacement *placement = NULL;

    if (!machine_view) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-300: invalid machine-view operand lowering contract");
        return machine_ir_operand_none();
    }

    if (value.kind == VALUE_SSA_VALUE_IMMEDIATE) {
        return machine_ir_operand_immediate(value.immediate);
    }
    if (value.kind != VALUE_SSA_VALUE_ID ||
        !value_ssa_function_machine_allocation_view_get_placement(machine_view, value.value_id, &placement) ||
        !placement) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-301: missing placement for SSA value");
        return machine_ir_operand_none();
    }

    if (placement->kind == VALUE_SSA_ALLOCATED_VALUE_PLACEMENT_COLOR) {
        return machine_ir_operand_register(placement->machine_register_id);
    }
    if (placement->kind == VALUE_SSA_ALLOCATED_VALUE_PLACEMENT_SPILL) {
        return machine_ir_operand_spill_slot(placement->spill_slot);
    }

    machine_ir_set_error(error, 0, 0, "MACHINE-IR-302: SSA value has no lowered machine placement");
    return machine_ir_operand_none();
}

static int machine_ir_copy_globals_from_value_ssa(const ValueSsaProgram *program,
    MachineIrProgram *out_program,
    MachineIrError *error) {
    size_t global_index;

    if (!program || !out_program) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-303: invalid global lowering contract");
        return 0;
    }

    for (global_index = 0; global_index < program->global_count; ++global_index) {
        MachineIrGlobal *global = NULL;

        if (!machine_ir_program_append_global(out_program, program->globals[global_index].name, &global, error) || !global) {
            return 0;
        }
        global->has_initializer = program->globals[global_index].has_initializer;
        global->initializer_value = program->globals[global_index].initializer_value;
        global->has_runtime_initializer = program->globals[global_index].has_runtime_initializer;
    }
    return 1;
}

int machine_ir_copy_register_bank_from_value_ssa(const ValueSsaMachineRegisterBank *source_bank,
    MachineIrRegisterBank *out_bank,
    MachineIrError *error) {
    size_t register_index;

    if (!source_bank || !out_bank) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-304: invalid register-bank copy contract");
        return 0;
    }

    machine_ir_register_bank_free(out_bank);
    out_bank->register_count = source_bank->register_count;
    if (source_bank->register_count == 0) {
        return 1;
    }

    out_bank->registers =
        (MachineIrRegisterDesc *)calloc(source_bank->register_count, sizeof(MachineIrRegisterDesc));
    if (!out_bank->registers) {
        machine_ir_register_bank_free(out_bank);
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-305: out of memory copying register bank");
        return 0;
    }

    for (register_index = 0; register_index < source_bank->register_count; ++register_index) {
        out_bank->registers[register_index].register_id = source_bank->registers[register_index].register_id;
        out_bank->registers[register_index].name =
            machine_ir_strdup(source_bank->registers[register_index].name);
        if (source_bank->registers[register_index].name && !out_bank->registers[register_index].name) {
            machine_ir_register_bank_free(out_bank);
            machine_ir_set_error(error, 0, 0, "MACHINE-IR-306: out of memory copying register name");
            return 0;
        }
        out_bank->registers[register_index].register_class =
            source_bank->registers[register_index].register_class;
        out_bank->registers[register_index].allocatable =
            source_bank->registers[register_index].allocatable;
        out_bank->registers[register_index].caller_clobbered =
            source_bank->registers[register_index].caller_clobbered;
        out_bank->registers[register_index].callee_preserved =
            source_bank->registers[register_index].callee_preserved;
    }
    return 1;
}

int machine_ir_lower_function_from_value_ssa(const ValueSsaFunction *function,
    const ValueSsaFunctionMachineAllocationView *machine_view,
    const ValueSsaMachineRegisterBank *bank,
    MachineIrFunction *out_function,
    MachineIrError *error) {
    size_t local_index;
    size_t block_index;

    if (!function || !machine_view || !bank || !out_function) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-307: invalid function lowering contract");
        return 0;
    }

    machine_ir_function_free(out_function);
    memset(out_function, 0, sizeof(*out_function));
    out_function->name = machine_ir_strdup(function->name);
    if (function->name && !out_function->name) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-308: out of memory copying function name");
        return 0;
    }
    out_function->has_body = function->has_body;
    out_function->spill_slot_count = machine_view->spill_slot_count;

    if (!function->has_body) {
        return 1;
    }

    for (local_index = 0; local_index < function->local_count; ++local_index) {
        if (!machine_ir_function_append_local(
                out_function,
                function->locals[local_index].source_name,
                function->locals[local_index].is_parameter,
                NULL,
                error)) {
            machine_ir_function_free(out_function);
            return 0;
        }
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        const ValueSsaBasicBlock *source_block = &function->blocks[block_index];
        MachineIrBasicBlock *dest_block = NULL;
        size_t phi_index;
        size_t instruction_index;

        if (!machine_ir_function_append_block(out_function, NULL, &dest_block, error) || !dest_block) {
            machine_ir_function_free(out_function);
            return 0;
        }

        for (phi_index = 0; phi_index < source_block->phi_count; ++phi_index) {
            const ValueSsaPhi *source_phi = &source_block->phis[phi_index];
            MachineIrPhi *dest_phi = NULL;
            size_t input_index;
            MachineIrOperand result = machine_ir_lower_value_ref(
                machine_view, value_ssa_value_id(source_phi->result_id), error);

            if (result.kind == MACHINE_IR_OPERAND_NONE ||
                !machine_ir_block_append_phi(dest_block, result, NULL, &dest_phi, error) || !dest_phi) {
                machine_ir_function_free(out_function);
                return 0;
            }
            for (input_index = 0; input_index < source_phi->input_count; ++input_index) {
                MachineIrOperand input_value = machine_ir_lower_value_ref(
                    machine_view, source_phi->inputs[input_index].value, error);

                if (input_value.kind == MACHINE_IR_OPERAND_NONE ||
                    !machine_ir_phi_append_input(dest_phi,
                        source_phi->inputs[input_index].predecessor_block_id,
                        input_value,
                        error)) {
                    machine_ir_function_free(out_function);
                    return 0;
                }
            }
        }

        for (instruction_index = 0; instruction_index < source_block->instruction_count; ++instruction_index) {
            const ValueSsaInstruction *source_instruction = &source_block->instructions[instruction_index];
            MachineIrInstruction instruction;

            memset(&instruction, 0, sizeof(instruction));
            instruction.kind = (MachineIrInstructionKind)source_instruction->kind;
            instruction.has_result = source_instruction->has_result;
            if (source_instruction->has_result) {
                instruction.result = machine_ir_lower_value_ref(machine_view, source_instruction->result, error);
                if (instruction.result.kind == MACHINE_IR_OPERAND_NONE) {
                    machine_ir_function_free(out_function);
                    return 0;
                }
            }

            switch (source_instruction->kind) {
                case VALUE_SSA_INSTR_MOV:
                    instruction.as.mov_value =
                        machine_ir_lower_value_ref(machine_view, source_instruction->as.mov_value, error);
                    break;
                case VALUE_SSA_INSTR_BINARY:
                    instruction.as.binary.op = machine_ir_lower_binary_op(source_instruction->as.binary.op);
                    instruction.as.binary.lhs =
                        machine_ir_lower_value_ref(machine_view, source_instruction->as.binary.lhs, error);
                    instruction.as.binary.rhs =
                        machine_ir_lower_value_ref(machine_view, source_instruction->as.binary.rhs, error);
                    break;
                case VALUE_SSA_INSTR_CALL: {
                    size_t arg_index;

                    instruction.as.call.callee_name =
                        machine_ir_strdup(source_instruction->as.call.callee_name);
                    instruction.as.call.arg_count = source_instruction->as.call.arg_count;
                    if (source_instruction->as.call.callee_name && !instruction.as.call.callee_name) {
                        machine_ir_instruction_free(&instruction);
                        machine_ir_function_free(out_function);
                        machine_ir_set_error(error, 0, 0, "MACHINE-IR-309: out of memory copying callee name");
                        return 0;
                    }
                    if (instruction.as.call.arg_count > 0) {
                        instruction.as.call.args = (MachineIrOperand *)calloc(
                            instruction.as.call.arg_count, sizeof(MachineIrOperand));
                        if (!instruction.as.call.args) {
                            machine_ir_instruction_free(&instruction);
                            machine_ir_function_free(out_function);
                            machine_ir_set_error(error, 0, 0, "MACHINE-IR-310: out of memory copying call args");
                            return 0;
                        }
                    }
                    for (arg_index = 0; arg_index < instruction.as.call.arg_count; ++arg_index) {
                        instruction.as.call.args[arg_index] =
                            machine_ir_lower_value_ref(machine_view, source_instruction->as.call.args[arg_index], error);
                        if (instruction.as.call.args[arg_index].kind == MACHINE_IR_OPERAND_NONE) {
                            machine_ir_instruction_free(&instruction);
                            machine_ir_function_free(out_function);
                            return 0;
                        }
                    }
                    break;
                }
                case VALUE_SSA_INSTR_LOAD_LOCAL:
                    instruction.as.load_slot = machine_ir_slot_local(source_instruction->as.load_slot.id);
                    break;
                case VALUE_SSA_INSTR_STORE_LOCAL:
                    instruction.as.store.slot = machine_ir_slot_local(source_instruction->as.store.slot.id);
                    instruction.as.store.value =
                        machine_ir_lower_value_ref(machine_view, source_instruction->as.store.value, error);
                    break;
                case VALUE_SSA_INSTR_LOAD_GLOBAL:
                    instruction.as.load_slot = machine_ir_slot_global(source_instruction->as.load_slot.id);
                    break;
                case VALUE_SSA_INSTR_STORE_GLOBAL:
                    instruction.as.store.slot = machine_ir_slot_global(source_instruction->as.store.slot.id);
                    instruction.as.store.value =
                        machine_ir_lower_value_ref(machine_view, source_instruction->as.store.value, error);
                    break;
                default:
                    machine_ir_instruction_free(&instruction);
                    machine_ir_function_free(out_function);
                    machine_ir_set_error(error, 0, 0, "MACHINE-IR-311: unsupported Value-SSA instruction kind");
                    return 0;
            }

            if (!machine_ir_block_append_instruction(dest_block, &instruction, error)) {
                machine_ir_instruction_free(&instruction);
                machine_ir_function_free(out_function);
                return 0;
            }
            machine_ir_instruction_free(&instruction);
        }

        switch (source_block->terminator.kind) {
            case VALUE_SSA_TERM_RETURN: {
                MachineIrOperand value = machine_ir_lower_value_ref(
                    machine_view, source_block->terminator.as.return_value, error);

                if (value.kind == MACHINE_IR_OPERAND_NONE ||
                    !machine_ir_block_set_return(dest_block, value, error)) {
                    machine_ir_function_free(out_function);
                    return 0;
                }
                break;
            }
            case VALUE_SSA_TERM_JUMP:
                if (!machine_ir_block_set_jump(dest_block, source_block->terminator.as.jump_target, error)) {
                    machine_ir_function_free(out_function);
                    return 0;
                }
                break;
            case VALUE_SSA_TERM_BRANCH: {
                MachineIrOperand condition = machine_ir_lower_value_ref(
                    machine_view, source_block->terminator.as.branch.condition, error);

                if (condition.kind == MACHINE_IR_OPERAND_NONE ||
                    !machine_ir_block_set_branch(dest_block,
                        condition,
                        source_block->terminator.as.branch.then_target,
                        source_block->terminator.as.branch.else_target,
                        error)) {
                    machine_ir_function_free(out_function);
                    return 0;
                }
                break;
            }
            default:
                machine_ir_function_free(out_function);
                machine_ir_set_error(error, 0, 0, "MACHINE-IR-312: unsupported Value-SSA terminator kind");
                return 0;
        }
    }

    (void)bank;
    return 1;
}

int machine_ir_lower_program_from_value_ssa(const ValueSsaProgram *program,
    const ValueSsaProgramMachineAllocationView *machine_view,
    const ValueSsaMachineRegisterBank *bank,
    MachineIrProgram *out_program,
    MachineIrError *error) {
    size_t function_index;

    if (!program || !machine_view || !bank || !out_program) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-313: invalid program lowering contract");
        return 0;
    }
    if (program->function_count != machine_view->function_count) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-314: program/machine-view function counts must match");
        return 0;
    }

    machine_ir_program_free(out_program);
    if (!machine_ir_copy_register_bank_from_value_ssa(bank, &out_program->register_bank, error) ||
        !machine_ir_copy_globals_from_value_ssa(program, out_program, error)) {
        machine_ir_program_free(out_program);
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        MachineIrFunction *dest_function = NULL;
        const ValueSsaFunctionMachineAllocationView *source_view = NULL;

        if (!machine_ir_program_append_function(
                out_program, program->functions[function_index].name, program->functions[function_index].has_body, &dest_function, error) ||
            !dest_function ||
            !value_ssa_program_machine_allocation_view_get_function_view(machine_view, function_index, &source_view) ||
            !source_view ||
            !machine_ir_lower_function_from_value_ssa(&program->functions[function_index], source_view, bank, dest_function, error)) {
            machine_ir_program_free(out_program);
            return 0;
        }
    }

    if (!machine_ir_verify_program(out_program, error)) {
        machine_ir_program_free(out_program);
        return 0;
    }
    return 1;
}

int machine_ir_allocate_lower_program_from_value_ssa(const ValueSsaProgram *program,
    size_t color_budget,
    const ValueSsaMachineRegisterBank *bank,
    MachineIrProgram *out_program,
    MachineIrError *error) {
    ValueSsaProgramMachineAllocationView machine_view;
    ValueSsaError value_error;
    int ok;

    value_ssa_program_machine_allocation_view_init(&machine_view);
    memset(&value_error, 0, sizeof(value_error));
    if (!value_ssa_allocate_program_machine_view(program, color_budget, bank, &machine_view, &value_error)) {
        machine_ir_set_error_from_value_ssa(error, &value_error);
        return 0;
    }

    ok = machine_ir_lower_program_from_value_ssa(program, &machine_view, bank, out_program, error);
    value_ssa_program_machine_allocation_view_free(&machine_view);
    return ok;
}

int machine_ir_allocate_lower_program_from_value_ssa_flat(const ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    MachineIrProgram *out_program,
    MachineIrError *error) {
    ValueSsaMachineRegisterBank bank;
    ValueSsaError value_error;
    int ok;

    value_ssa_machine_register_bank_init(&bank);
    memset(&value_error, 0, sizeof(value_error));
    if (!value_ssa_build_flat_machine_register_bank(machine_register_count, &bank, &value_error)) {
        machine_ir_set_error_from_value_ssa(error, &value_error);
        return 0;
    }

    ok = machine_ir_allocate_lower_program_from_value_ssa(program, color_budget, &bank, out_program, error);
    value_ssa_machine_register_bank_free(&bank);
    return ok;
}

#endif
