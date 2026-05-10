#if defined(__INTELLISENSE__) && !defined(MACHINE_IR_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_IR_SPLIT_AGGREGATOR)
#define MACHINE_IR_SPLIT_INTELLISENSE_CONTEXT
#include "machine_ir.c"
#else

static MachineIrBinaryOp machine_ir_lower_binary_op(ValueSsaBinaryOp op) {
    return (MachineIrBinaryOp)op;
}

static MachineIrOperand machine_ir_lower_value_ref(const ValueSsaFunctionMachineAllocationView *machine_view,
    const size_t *protected_spill_slots,
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
    if (protected_spill_slots && protected_spill_slots[value.value_id] != (size_t)-1) {
        return machine_ir_operand_spill_slot(protected_spill_slots[value.value_id]);
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

static int machine_ir_get_value_colored_register_id(const ValueSsaFunctionMachineAllocationView *machine_view,
    const ValueSsaMachineRegisterBank *bank,
    ValueSsaValueRef value,
    size_t *out_register_id) {
    const ValueSsaMachineAllocatedValuePlacement *placement = NULL;

    if (out_register_id) {
        *out_register_id = (size_t)-1;
    }
    if (!machine_view || !bank || !out_register_id || value.kind != VALUE_SSA_VALUE_ID ||
        !value_ssa_function_machine_allocation_view_get_placement(machine_view, value.value_id, &placement) ||
        !placement || placement->kind != VALUE_SSA_ALLOCATED_VALUE_PLACEMENT_COLOR ||
        placement->machine_register_id >= bank->register_count) {
        return 0;
    }
    *out_register_id = placement->machine_register_id;
    return 1;
}

static int machine_ir_ensure_protected_spill_slot_for_value(size_t value_id,
    size_t value_count,
    size_t *slots,
    size_t *next_spill_slot,
    MachineIrError *error) {
    if (!slots || !next_spill_slot || value_id >= value_count) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-321: invalid protected spill-slot assignment contract");
        return 0;
    }
    if (slots[value_id] == (size_t)-1) {
        slots[value_id] = (*next_spill_slot)++;
    }
    return 1;
}

static int machine_ir_protect_same_register_value_pair(const ValueSsaFunctionMachineAllocationView *machine_view,
    const ValueSsaMachineRegisterBank *bank,
    ValueSsaValueRef lhs,
    ValueSsaValueRef rhs,
    size_t *slots,
    size_t *next_spill_slot,
    MachineIrError *error) {
    size_t lhs_reg = (size_t)-1;
    size_t rhs_reg = (size_t)-1;

    if (lhs.kind != VALUE_SSA_VALUE_ID || rhs.kind != VALUE_SSA_VALUE_ID || lhs.value_id == rhs.value_id) {
        return 1;
    }
    if (!machine_ir_get_value_colored_register_id(machine_view, bank, lhs, &lhs_reg) ||
        !machine_ir_get_value_colored_register_id(machine_view, bank, rhs, &rhs_reg) ||
        lhs_reg != rhs_reg) {
        return 1;
    }
    return machine_ir_ensure_protected_spill_slot_for_value(
               lhs.value_id, machine_view->value_count, slots, next_spill_slot, error) &&
        machine_ir_ensure_protected_spill_slot_for_value(
               rhs.value_id, machine_view->value_count, slots, next_spill_slot, error);
}

static int machine_ir_function_has_call_instruction(const ValueSsaFunction *function) {
    size_t block_index;

    if (!function || !function->has_body) {
        return 0;
    }

    for (block_index = 0u; block_index < function->block_count; ++block_index) {
        const ValueSsaBasicBlock *block = &function->blocks[block_index];
        size_t instruction_index;

        for (instruction_index = 0u; instruction_index < block->instruction_count; ++instruction_index) {
            if (block->instructions[instruction_index].kind == VALUE_SSA_INSTR_CALL) {
                return 1;
            }
        }
    }
    return 0;
}

static int machine_ir_build_protected_call_crossing_spill_slots(
    const ValueSsaFunction *function,
    const ValueSsaFunctionMachineAllocationView *machine_view,
    const ValueSsaMachineRegisterBank *bank,
    size_t **out_slots,
    size_t *out_protected_spill_count,
    MachineIrError *error) {
    ValueSsaAllocationPrep prep;
    ValueSsaCfgAnalysis cfg;
    ValueSsaLivenessAnalysis liveness;
    ValueSsaError value_error;
    size_t *slots = NULL;
    size_t block_id;
    size_t value_id;
    size_t next_spill_slot = 0;
    int function_has_call = 0;

    if (out_slots) {
        *out_slots = NULL;
    }
    if (out_protected_spill_count) {
        *out_protected_spill_count = 0;
    }
    if (!function || !machine_view || !bank || !out_slots || !out_protected_spill_count) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-315: invalid protected spill-slot build contract");
        return 0;
    }
    if (!function->has_body || machine_view->value_count == 0) {
        return 1;
    }
    function_has_call = machine_ir_function_has_call_instruction(function);

    value_ssa_allocation_prep_init(&prep);
    value_ssa_cfg_analysis_init(&cfg);
    value_ssa_liveness_analysis_init(&liveness);
    memset(&value_error, 0, sizeof(value_error));
    slots = (size_t *)malloc(machine_view->value_count * sizeof(size_t));
    if (!slots) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-316: out of memory building protected spill-slot map");
        return 0;
    }
    for (value_id = 0; value_id < machine_view->value_count; ++value_id) {
        slots[value_id] = (size_t)-1;
    }
    if (!value_ssa_compute_allocation_prep(function, NULL, NULL, NULL, NULL, &prep, &value_error)) {
        free(slots);
        value_ssa_liveness_analysis_free(&liveness);
        value_ssa_cfg_analysis_free(&cfg);
        value_ssa_allocation_prep_free(&prep);
        machine_ir_set_error_from_value_ssa(error, &value_error);
        return 0;
    }
    if (!value_ssa_compute_cfg_analysis(function, &cfg, &value_error) ||
        !value_ssa_compute_liveness_analysis(function, &cfg, &liveness, &value_error)) {
        free(slots);
        value_ssa_liveness_analysis_free(&liveness);
        value_ssa_cfg_analysis_free(&cfg);
        value_ssa_allocation_prep_free(&prep);
        machine_ir_set_error_from_value_ssa(error, &value_error);
        return 0;
    }

    next_spill_slot = machine_view->spill_slot_count;
    if (function_has_call) {
        for (value_id = 0; value_id < machine_view->value_count; ++value_id) {
            const ValueSsaMachineAllocatedValuePlacement *placement = NULL;

            if (!prep.call_crossing_counts || prep.call_crossing_counts[value_id] == 0 ||
                !value_ssa_function_machine_allocation_view_get_placement(machine_view, value_id, &placement) ||
                !placement || placement->kind != VALUE_SSA_ALLOCATED_VALUE_PLACEMENT_COLOR ||
                placement->machine_register_id >= bank->register_count ||
                !bank->registers[placement->machine_register_id].caller_clobbered) {
                continue;
            }
            slots[value_id] = next_spill_slot++;
        }
    }

    for (block_id = 0u; block_id < function->block_count; ++block_id) {
        const ValueSsaBasicBlock *block = &function->blocks[block_id];
        const unsigned char *live_out_row = &liveness.live_out[block_id * machine_view->value_count];
        size_t instruction_index;
        size_t phi_index;
        size_t condition_value_id;
        const ValueSsaMachineAllocatedValuePlacement *condition_placement = NULL;

        for (instruction_index = 0u; instruction_index < block->instruction_count; ++instruction_index) {
            const ValueSsaInstruction *instruction = &block->instructions[instruction_index];
            size_t arg_index;
            size_t other_arg_index;

            switch (instruction->kind) {
                case VALUE_SSA_INSTR_BINARY:
                    if (!machine_ir_protect_same_register_value_pair(
                            machine_view,
                            bank,
                            instruction->as.binary.lhs,
                            instruction->as.binary.rhs,
                            slots,
                            &next_spill_slot,
                            error)) {
                        free(slots);
                        value_ssa_liveness_analysis_free(&liveness);
                        value_ssa_cfg_analysis_free(&cfg);
                        value_ssa_allocation_prep_free(&prep);
                        return 0;
                    }
                    break;
                case VALUE_SSA_INSTR_CALL:
                    for (arg_index = 0u; arg_index < instruction->as.call.arg_count; ++arg_index) {
                        for (other_arg_index = arg_index + 1u;
                             other_arg_index < instruction->as.call.arg_count;
                             ++other_arg_index) {
                            if (!machine_ir_protect_same_register_value_pair(
                                    machine_view,
                                    bank,
                                    instruction->as.call.args[arg_index],
                                    instruction->as.call.args[other_arg_index],
                                    slots,
                                    &next_spill_slot,
                                    error)) {
                                free(slots);
                                value_ssa_liveness_analysis_free(&liveness);
                                value_ssa_cfg_analysis_free(&cfg);
                                value_ssa_allocation_prep_free(&prep);
                                return 0;
                            }
                        }
                    }
                    break;
                case VALUE_SSA_INSTR_STORE_INDIRECT:
                    if (!machine_ir_protect_same_register_value_pair(
                            machine_view,
                            bank,
                            instruction->as.store_indirect.addr,
                            instruction->as.store_indirect.value,
                            slots,
                            &next_spill_slot,
                            error)) {
                        free(slots);
                        value_ssa_liveness_analysis_free(&liveness);
                        value_ssa_cfg_analysis_free(&cfg);
                        value_ssa_allocation_prep_free(&prep);
                        return 0;
                    }
                    break;
                default:
                    break;
            }
        }

        for (phi_index = 0u; phi_index < block->phi_count; ++phi_index) {
            size_t phi_value_id = block->phis[phi_index].result_id;
            const ValueSsaMachineAllocatedValuePlacement *phi_placement = NULL;

            if (phi_value_id >= machine_view->value_count || slots[phi_value_id] != (size_t)-1 ||
                !value_ssa_function_machine_allocation_view_get_placement(
                    machine_view, phi_value_id, &phi_placement) ||
                !phi_placement ||
                phi_placement->kind != VALUE_SSA_ALLOCATED_VALUE_PLACEMENT_COLOR ||
                phi_placement->machine_register_id >= bank->register_count) {
                continue;
            }
            for (value_id = 0; value_id < machine_view->value_count; ++value_id) {
                const ValueSsaMachineAllocatedValuePlacement *live_out_placement = NULL;

                if (value_id == phi_value_id || !live_out_row[value_id] ||
                    !value_ssa_function_machine_allocation_view_get_placement(
                        machine_view, value_id, &live_out_placement) ||
                    !live_out_placement ||
                    live_out_placement->kind != VALUE_SSA_ALLOCATED_VALUE_PLACEMENT_COLOR) {
                    continue;
                }
                if (live_out_placement->machine_register_id != phi_placement->machine_register_id) {
                    continue;
                }
                slots[phi_value_id] = next_spill_slot++;
                break;
            }
        }

        if (block->terminator.kind != VALUE_SSA_TERM_BRANCH) {
            continue;
        }
        for (value_id = 0; value_id < machine_view->value_count; ++value_id) {
            const ValueSsaMachineAllocatedValuePlacement *live_out_placement = NULL;

            if (!live_out_row[value_id] || slots[value_id] != (size_t)-1 ||
                !value_ssa_function_machine_allocation_view_get_placement(
                    machine_view, value_id, &live_out_placement) ||
                !live_out_placement ||
                live_out_placement->kind != VALUE_SSA_ALLOCATED_VALUE_PLACEMENT_COLOR ||
                live_out_placement->machine_register_id >= bank->register_count) {
                continue;
            }
            slots[value_id] = next_spill_slot++;
        }
        if (block->terminator.as.branch.condition.kind != VALUE_SSA_VALUE_ID) {
            continue;
        }
        condition_value_id = block->terminator.as.branch.condition.value_id;
        if (condition_value_id >= machine_view->value_count || slots[condition_value_id] != (size_t)-1 ||
            !value_ssa_function_machine_allocation_view_get_placement(
                machine_view, condition_value_id, &condition_placement) ||
            !condition_placement ||
            condition_placement->kind != VALUE_SSA_ALLOCATED_VALUE_PLACEMENT_COLOR ||
            condition_placement->machine_register_id >= bank->register_count) {
            continue;
        }
        for (value_id = 0; value_id < machine_view->value_count; ++value_id) {
            const ValueSsaMachineAllocatedValuePlacement *live_out_placement = NULL;

            if (value_id == condition_value_id || !live_out_row[value_id] ||
                !value_ssa_function_machine_allocation_view_get_placement(
                    machine_view, value_id, &live_out_placement) ||
                !live_out_placement ||
                live_out_placement->kind != VALUE_SSA_ALLOCATED_VALUE_PLACEMENT_COLOR) {
                continue;
            }
            if (live_out_placement->machine_register_id != condition_placement->machine_register_id) {
                continue;
            }
            slots[condition_value_id] = next_spill_slot++;
            break;
        }
    }

    *out_slots = slots;
    *out_protected_spill_count = next_spill_slot - machine_view->spill_slot_count;
    value_ssa_liveness_analysis_free(&liveness);
    value_ssa_cfg_analysis_free(&cfg);
    value_ssa_allocation_prep_free(&prep);
    return 1;
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
        global->byte_size = program->globals[global_index].byte_size;
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
    size_t *protected_spill_slots = NULL;
    size_t protected_spill_count = 0;

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
    if (!machine_ir_build_protected_call_crossing_spill_slots(
            function, machine_view, bank, &protected_spill_slots, &protected_spill_count, error)) {
        machine_ir_function_free(out_function);
        return 0;
    }
    out_function->spill_slot_count = machine_view->spill_slot_count + protected_spill_count;

    if (!function->has_body) {
        free(protected_spill_slots);
        return 1;
    }

    if (function->local_count > 0u) {
        out_function->locals = (MachineIrLocal *)calloc(function->local_count, sizeof(MachineIrLocal));
        if (!out_function->locals) {
            free(protected_spill_slots);
            machine_ir_function_free(out_function);
            machine_ir_set_error(error, 0, 0, "MACHINE-IR-317: out of memory preallocating locals");
            return 0;
        }
        out_function->local_capacity = function->local_count;
    }
    if (function->block_count > 0u) {
        out_function->blocks = (MachineIrBasicBlock *)calloc(function->block_count, sizeof(MachineIrBasicBlock));
        if (!out_function->blocks) {
            free(protected_spill_slots);
            machine_ir_function_free(out_function);
            machine_ir_set_error(error, 0, 0, "MACHINE-IR-318: out of memory preallocating blocks");
            return 0;
        }
        out_function->block_capacity = function->block_count;
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

        if (source_block->phi_count > 0u) {
            dest_block->phis = (MachineIrPhi *)calloc(source_block->phi_count, sizeof(MachineIrPhi));
            if (!dest_block->phis) {
                free(protected_spill_slots);
                machine_ir_function_free(out_function);
                machine_ir_set_error(error, 0, 0, "MACHINE-IR-319: out of memory preallocating phis");
                return 0;
            }
            dest_block->phi_capacity = source_block->phi_count;
        }
        if (source_block->instruction_count > 0u) {
            dest_block->instructions =
                (MachineIrInstruction *)calloc(source_block->instruction_count, sizeof(MachineIrInstruction));
            if (!dest_block->instructions) {
                free(protected_spill_slots);
                machine_ir_function_free(out_function);
                machine_ir_set_error(error, 0, 0, "MACHINE-IR-320: out of memory preallocating instructions");
                return 0;
            }
            dest_block->instruction_capacity = source_block->instruction_count;
        }

        for (phi_index = 0; phi_index < source_block->phi_count; ++phi_index) {
            const ValueSsaPhi *source_phi = &source_block->phis[phi_index];
            MachineIrPhi *dest_phi = NULL;
            size_t input_index;
            MachineIrOperand result = machine_ir_lower_value_ref(
                machine_view, protected_spill_slots, value_ssa_value_id(source_phi->result_id), error);

            if (result.kind == MACHINE_IR_OPERAND_NONE ||
                !machine_ir_block_append_phi(dest_block, result, NULL, &dest_phi, error) || !dest_phi) {
                free(protected_spill_slots);
                machine_ir_function_free(out_function);
                return 0;
            }
            for (input_index = 0; input_index < source_phi->input_count; ++input_index) {
                MachineIrOperand input_value = machine_ir_lower_value_ref(
                    machine_view, protected_spill_slots, source_phi->inputs[input_index].value, error);

                if (input_value.kind == MACHINE_IR_OPERAND_NONE ||
                    !machine_ir_phi_append_input(dest_phi,
                        source_phi->inputs[input_index].predecessor_block_id,
                        input_value,
                        error)) {
                    free(protected_spill_slots);
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
                instruction.result =
                    machine_ir_lower_value_ref(machine_view, protected_spill_slots, source_instruction->result, error);
                if (instruction.result.kind == MACHINE_IR_OPERAND_NONE) {
                    free(protected_spill_slots);
                    machine_ir_function_free(out_function);
                    return 0;
                }
            }

            switch (source_instruction->kind) {
                case VALUE_SSA_INSTR_MOV:
                    instruction.as.mov_value =
                        machine_ir_lower_value_ref(machine_view, protected_spill_slots, source_instruction->as.mov_value, error);
                    break;
                case VALUE_SSA_INSTR_BINARY:
                    instruction.as.binary.op = machine_ir_lower_binary_op(source_instruction->as.binary.op);
                    instruction.as.binary.lhs =
                        machine_ir_lower_value_ref(machine_view, protected_spill_slots, source_instruction->as.binary.lhs, error);
                    instruction.as.binary.rhs =
                        machine_ir_lower_value_ref(machine_view, protected_spill_slots, source_instruction->as.binary.rhs, error);
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
                            machine_ir_lower_value_ref(
                                machine_view, protected_spill_slots, source_instruction->as.call.args[arg_index], error);
                        if (instruction.as.call.args[arg_index].kind == MACHINE_IR_OPERAND_NONE) {
                            machine_ir_instruction_free(&instruction);
                            free(protected_spill_slots);
                            machine_ir_function_free(out_function);
                            return 0;
                        }
                    }
                    break;
                }
                case VALUE_SSA_INSTR_ADDR_LOCAL:
                    instruction.as.addr_slot = machine_ir_slot_local(source_instruction->as.addr_slot.id);
                    break;
                case VALUE_SSA_INSTR_ADDR_GLOBAL:
                    instruction.as.addr_slot = machine_ir_slot_global(source_instruction->as.addr_slot.id);
                    break;
                case VALUE_SSA_INSTR_LOAD_LOCAL:
                    instruction.as.load_slot = machine_ir_slot_local(source_instruction->as.load_slot.id);
                    break;
                case VALUE_SSA_INSTR_STORE_LOCAL:
                    instruction.as.store.slot = machine_ir_slot_local(source_instruction->as.store.slot.id);
                    instruction.as.store.value =
                        machine_ir_lower_value_ref(machine_view, protected_spill_slots, source_instruction->as.store.value, error);
                    break;
                case VALUE_SSA_INSTR_LOAD_GLOBAL:
                    instruction.as.load_slot = machine_ir_slot_global(source_instruction->as.load_slot.id);
                    break;
                case VALUE_SSA_INSTR_STORE_GLOBAL:
                    instruction.as.store.slot = machine_ir_slot_global(source_instruction->as.store.slot.id);
                    instruction.as.store.value =
                        machine_ir_lower_value_ref(machine_view, protected_spill_slots, source_instruction->as.store.value, error);
                    break;
                case VALUE_SSA_INSTR_LOAD_INDIRECT:
                    instruction.as.load_indirect_addr =
                        machine_ir_lower_value_ref(machine_view, protected_spill_slots, source_instruction->as.load_indirect_addr, error);
                    break;
                case VALUE_SSA_INSTR_STORE_INDIRECT:
                    instruction.as.store_indirect.addr =
                        machine_ir_lower_value_ref(machine_view, protected_spill_slots, source_instruction->as.store_indirect.addr, error);
                    instruction.as.store_indirect.value =
                        machine_ir_lower_value_ref(machine_view, protected_spill_slots, source_instruction->as.store_indirect.value, error);
                    break;
                default:
                    machine_ir_instruction_free(&instruction);
                    free(protected_spill_slots);
                    machine_ir_function_free(out_function);
                    machine_ir_set_error(error, 0, 0, "MACHINE-IR-311: unsupported Value-SSA instruction kind");
                    return 0;
            }

            if (!machine_ir_block_append_instruction(dest_block, &instruction, error)) {
                machine_ir_instruction_free(&instruction);
                free(protected_spill_slots);
                machine_ir_function_free(out_function);
                return 0;
            }
            machine_ir_instruction_free(&instruction);
        }

        switch (source_block->terminator.kind) {
            case VALUE_SSA_TERM_RETURN: {
                if (!source_block->terminator.has_return_value) {
                    if (!machine_ir_block_set_void_return(dest_block, error)) {
                        machine_ir_function_free(out_function);
                        return 0;
                    }
                } else {
                    MachineIrOperand value = machine_ir_lower_value_ref(
                        machine_view, protected_spill_slots, source_block->terminator.as.return_value, error);

                    if (value.kind == MACHINE_IR_OPERAND_NONE ||
                        !machine_ir_block_set_return(dest_block, value, error)) {
                        free(protected_spill_slots);
                        machine_ir_function_free(out_function);
                        return 0;
                    }
                }
                break;
            }
            case VALUE_SSA_TERM_JUMP:
                if (!machine_ir_block_set_jump(dest_block, source_block->terminator.as.jump_target, error)) {
                    free(protected_spill_slots);
                    machine_ir_function_free(out_function);
                    return 0;
                }
                break;
            case VALUE_SSA_TERM_BRANCH: {
                MachineIrOperand condition = machine_ir_lower_value_ref(
                    machine_view, protected_spill_slots, source_block->terminator.as.branch.condition, error);

                if (condition.kind == MACHINE_IR_OPERAND_NONE ||
                    !machine_ir_block_set_branch(dest_block,
                        condition,
                        source_block->terminator.as.branch.then_target,
                        source_block->terminator.as.branch.else_target,
                        error)) {
                    free(protected_spill_slots);
                    machine_ir_function_free(out_function);
                    return 0;
                }
                break;
            }
            default:
                free(protected_spill_slots);
                machine_ir_function_free(out_function);
                machine_ir_set_error(error, 0, 0, "MACHINE-IR-312: unsupported Value-SSA terminator kind");
                return 0;
        }
    }

    free(protected_spill_slots);
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
