#if defined(__INTELLISENSE__) && !defined(MACHINE_IR_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_IR_SPLIT_AGGREGATOR)
#define MACHINE_IR_SPLIT_INTELLISENSE_CONTEXT
#include "machine_ir.c"
#else

static int machine_ir_verify_operand(const MachineIrProgram *program,
    const MachineIrFunction *function,
    MachineIrOperand operand,
    const char *context,
    MachineIrError *error) {
    if (!program || !function || !context) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-100: invalid operand verifier contract");
        return 0;
    }

    switch (operand.kind) {
        case MACHINE_IR_OPERAND_IMMEDIATE:
            return 1;
        case MACHINE_IR_OPERAND_REGISTER:
            if (operand.machine_register_id >= program->register_bank.register_count ||
                !program->register_bank.registers) {
                machine_ir_set_error(error, 0, 0, "MACHINE-IR-101: %s uses out-of-range machine register", context);
                return 0;
            }
            return 1;
        case MACHINE_IR_OPERAND_SPILL_SLOT:
            if (operand.spill_slot >= function->spill_slot_count && function->spill_slot_count > 0) {
                machine_ir_set_error(error, 0, 0, "MACHINE-IR-102: %s uses out-of-range spill slot", context);
                return 0;
            }
            if (function->spill_slot_count == 0) {
                machine_ir_set_error(error, 0, 0, "MACHINE-IR-103: %s uses spill slot in spill-free function", context);
                return 0;
            }
            return 1;
        default:
            machine_ir_set_error(error, 0, 0, "MACHINE-IR-104: %s uses invalid operand kind", context);
            return 0;
    }
}

static int machine_ir_verify_result_operand(const MachineIrProgram *program,
    const MachineIrFunction *function,
    MachineIrOperand operand,
    const char *context,
    MachineIrError *error) {
    if (operand.kind != MACHINE_IR_OPERAND_REGISTER && operand.kind != MACHINE_IR_OPERAND_SPILL_SLOT) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-105: %s result must be register or spill slot", context);
        return 0;
    }
    return machine_ir_verify_operand(program, function, operand, context, error);
}

static int machine_ir_verify_slot_ref(const MachineIrProgram *program,
    const MachineIrFunction *function,
    MachineIrSlotRef slot,
    const char *context,
    MachineIrError *error) {
    if (!program || !function || !context) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-106: invalid slot verifier contract");
        return 0;
    }

    if (slot.kind == MACHINE_IR_SLOT_LOCAL) {
        if (slot.id >= function->local_count || !function->locals) {
            machine_ir_set_error(error, 0, 0, "MACHINE-IR-107: %s references invalid local slot", context);
            return 0;
        }
        return 1;
    }
    if (slot.kind == MACHINE_IR_SLOT_GLOBAL) {
        if (slot.id >= program->global_count || !program->globals) {
            machine_ir_set_error(error, 0, 0, "MACHINE-IR-108: %s references invalid global slot", context);
            return 0;
        }
        return 1;
    }

    machine_ir_set_error(error, 0, 0, "MACHINE-IR-109: %s uses invalid slot kind", context);
    return 0;
}

static int machine_ir_verify_function_cfg(const MachineIrFunction *function, MachineIrError *error) {
    unsigned char *reachable;
    size_t *stack;
    size_t stack_size = 0;
    size_t block_id;
    int ok = 0;

    if (!function) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-110: invalid CFG verifier contract");
        return 0;
    }
    if (!function->has_body) {
        return 1;
    }
    if (function->block_count == 0 || !function->blocks) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-111: function body must contain blocks");
        return 0;
    }

    reachable = (unsigned char *)calloc(function->block_count, sizeof(unsigned char));
    stack = (size_t *)malloc(function->block_count * sizeof(size_t));
    if (!reachable || !stack) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-112: out of memory computing CFG reachability");
        goto cleanup;
    }

    stack[stack_size++] = 0;
    reachable[0] = 1u;
    while (stack_size > 0) {
        size_t current = stack[--stack_size];
        const MachineIrBasicBlock *block = &function->blocks[current];

        if (!block->has_terminator) {
            machine_ir_set_error(error, 0, 0, "MACHINE-IR-113: reachable block is missing terminator");
            goto cleanup;
        }

        if (block->terminator.kind == MACHINE_IR_TERM_JUMP) {
            size_t target = block->terminator.as.jump_target;

            if (target >= function->block_count) {
                machine_ir_set_error(error, 0, 0, "MACHINE-IR-114: jump target out of range");
                goto cleanup;
            }
            if (!reachable[target]) {
                reachable[target] = 1u;
                stack[stack_size++] = target;
            }
        } else if (block->terminator.kind == MACHINE_IR_TERM_BRANCH) {
            size_t then_target = block->terminator.as.branch.then_target;
            size_t else_target = block->terminator.as.branch.else_target;

            if (then_target >= function->block_count || else_target >= function->block_count) {
                machine_ir_set_error(error, 0, 0, "MACHINE-IR-115: branch target out of range");
                goto cleanup;
            }
            if (!reachable[then_target]) {
                reachable[then_target] = 1u;
                stack[stack_size++] = then_target;
            }
            if (!reachable[else_target]) {
                reachable[else_target] = 1u;
                stack[stack_size++] = else_target;
            }
        }
    }

    for (block_id = 0; block_id < function->block_count; ++block_id) {
        if (!reachable[block_id]) {
            machine_ir_set_error(error, 0, 0, "MACHINE-IR-116: function contains unreachable block");
            goto cleanup;
        }
    }

    ok = 1;

cleanup:
    free(stack);
    free(reachable);
    return ok;
}

int machine_ir_verify_program(const MachineIrProgram *program, MachineIrError *error) {
    size_t register_index;
    size_t global_index;
    size_t function_index;

    if (!program) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-117: invalid program verifier contract");
        return 0;
    }
    if (program->register_bank.register_count > 0 && !program->register_bank.registers) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-118: nonzero register count requires register table");
        return 0;
    }
    if (program->global_count > 0 && !program->globals) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-119: nonzero global count requires global table");
        return 0;
    }
    if (program->function_count > 0 && !program->functions) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-120: nonzero function count requires function table");
        return 0;
    }

    for (register_index = 0; register_index < program->register_bank.register_count; ++register_index) {
        const MachineIrRegisterDesc *reg = &program->register_bank.registers[register_index];

        if (reg->register_id != register_index) {
            machine_ir_set_error(error, 0, 0, "MACHINE-IR-121: machine register ids must stay dense");
            return 0;
        }
        if (!reg->name || reg->name[0] == '\0') {
            machine_ir_set_error(error, 0, 0, "MACHINE-IR-122: machine registers must have names");
            return 0;
        }
    }

    for (global_index = 0; global_index < program->global_count; ++global_index) {
        const MachineIrGlobal *global = &program->globals[global_index];

        if (global->id != global_index) {
            machine_ir_set_error(error, 0, 0, "MACHINE-IR-123: global ids must stay dense");
            return 0;
        }
        if (!global->name || global->name[0] == '\0') {
            machine_ir_set_error(error, 0, 0, "MACHINE-IR-124: globals must have names");
            return 0;
        }
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const MachineIrFunction *function = &program->functions[function_index];
        size_t local_index;
        size_t block_index;

        if (!function->name || function->name[0] == '\0') {
            machine_ir_set_error(error, 0, 0, "MACHINE-IR-125: functions must have names");
            return 0;
        }
        if (function->has_body && ((function->local_count > 0 && !function->locals) ||
                                      (function->block_count > 0 && !function->blocks))) {
            machine_ir_set_error(error, 0, 0, "MACHINE-IR-126: function body metadata is structurally inconsistent");
            return 0;
        }
        for (local_index = 0; local_index < function->local_count; ++local_index) {
            if (function->locals[local_index].id != local_index) {
                machine_ir_set_error(error, 0, 0, "MACHINE-IR-127: local ids must stay dense");
                return 0;
            }
            if (local_index < function->parameter_count && !function->locals[local_index].is_parameter) {
                machine_ir_set_error(error, 0, 0, "MACHINE-IR-128: parameter locals must form a contiguous prefix");
                return 0;
            }
        }

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            const MachineIrBasicBlock *block = &function->blocks[block_index];
            size_t phi_index;
            size_t instruction_index;

            if (block->id != block_index) {
                machine_ir_set_error(error, 0, 0, "MACHINE-IR-129: block ids must stay dense");
                return 0;
            }
            if (!block->has_terminator) {
                machine_ir_set_error(
                    error, 0, 0, "MACHINE-IR-130: bb.%zu in function '%s' is missing a terminator",
                    block->id,
                    function->name ? function->name : "<unnamed>");
                return 0;
            }

            for (phi_index = 0; phi_index < block->phi_count; ++phi_index) {
                const MachineIrPhi *phi = &block->phis[phi_index];
                size_t input_index;
                size_t seen_index;

                if (!machine_ir_verify_result_operand(program, function, phi->result, "phi", error)) {
                    return 0;
                }
                if (phi->input_count == 0 || !phi->inputs) {
                    machine_ir_set_error(error, 0, 0, "MACHINE-IR-131: phi nodes must have inputs");
                    return 0;
                }
                for (input_index = 0; input_index < phi->input_count; ++input_index) {
                    if (phi->inputs[input_index].predecessor_block_id >= function->block_count) {
                        machine_ir_set_error(error, 0, 0, "MACHINE-IR-132: phi predecessor is out of range");
                        return 0;
                    }
                    for (seen_index = 0; seen_index < input_index; ++seen_index) {
                        if (phi->inputs[seen_index].predecessor_block_id ==
                            phi->inputs[input_index].predecessor_block_id) {
                            machine_ir_set_error(error, 0, 0, "MACHINE-IR-133: phi predecessor inputs must be unique");
                            return 0;
                        }
                    }
                    if (!machine_ir_verify_operand(program, function, phi->inputs[input_index].value, "phi input", error)) {
                        return 0;
                    }
                }
            }

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                const MachineIrInstruction *instruction = &block->instructions[instruction_index];

                switch (instruction->kind) {
                    case MACHINE_IR_INSTR_MOV:
                        if (!instruction->has_result ||
                            !machine_ir_verify_result_operand(program, function, instruction->result, "mov", error) ||
                            !machine_ir_verify_operand(program, function, instruction->as.mov_value, "mov", error)) {
                            return 0;
                        }
                        break;
                    case MACHINE_IR_INSTR_BINARY:
                        if (!instruction->has_result ||
                            !machine_ir_verify_result_operand(program, function, instruction->result, "binary", error) ||
                            !machine_ir_verify_operand(program, function, instruction->as.binary.lhs, "binary lhs", error) ||
                            !machine_ir_verify_operand(program, function, instruction->as.binary.rhs, "binary rhs", error)) {
                            return 0;
                        }
                        break;
                    case MACHINE_IR_INSTR_CALL: {
                        size_t arg_index;

                        if (!instruction->has_result ||
                            !machine_ir_verify_result_operand(program, function, instruction->result, "call", error) ||
                            !instruction->as.call.callee_name || instruction->as.call.callee_name[0] == '\0') {
                            machine_ir_set_error(error, 0, 0, "MACHINE-IR-134: calls require callee name and result");
                            return 0;
                        }
                        if (instruction->as.call.arg_count > 0 && !instruction->as.call.args) {
                            machine_ir_set_error(error, 0, 0, "MACHINE-IR-135: nonzero call arg count requires arg table");
                            return 0;
                        }
                        for (arg_index = 0; arg_index < instruction->as.call.arg_count; ++arg_index) {
                            if (!machine_ir_verify_operand(
                                    program, function, instruction->as.call.args[arg_index], "call arg", error)) {
                                return 0;
                            }
                        }
                        break;
                    }
                    case MACHINE_IR_INSTR_LOAD_LOCAL:
                    case MACHINE_IR_INSTR_LOAD_GLOBAL:
                        if (!instruction->has_result ||
                            !machine_ir_verify_result_operand(program, function, instruction->result, "load", error) ||
                            !machine_ir_verify_slot_ref(program, function, instruction->as.load_slot, "load slot", error)) {
                            return 0;
                        }
                        break;
                    case MACHINE_IR_INSTR_STORE_LOCAL:
                    case MACHINE_IR_INSTR_STORE_GLOBAL:
                        if (instruction->has_result ||
                            !machine_ir_verify_slot_ref(program, function, instruction->as.store.slot, "store slot", error) ||
                            !machine_ir_verify_operand(program, function, instruction->as.store.value, "store value", error)) {
                            return 0;
                        }
                        break;
                    default:
                        machine_ir_set_error(error, 0, 0, "MACHINE-IR-136: unknown instruction kind");
                        return 0;
                }
            }

            switch (block->terminator.kind) {
                case MACHINE_IR_TERM_RETURN:
                    if (block->terminator.as.return_value.kind != MACHINE_IR_OPERAND_NONE &&
                        !machine_ir_verify_operand(program, function, block->terminator.as.return_value, "return", error)) {
                        return 0;
                    }
                    break;
                case MACHINE_IR_TERM_JUMP:
                    if (block->terminator.as.jump_target >= function->block_count) {
                        machine_ir_set_error(error, 0, 0, "MACHINE-IR-137: jump target out of range");
                        return 0;
                    }
                    break;
                case MACHINE_IR_TERM_BRANCH:
                    if (!machine_ir_verify_operand(program, function, block->terminator.as.branch.condition, "branch condition", error) ||
                        block->terminator.as.branch.then_target >= function->block_count ||
                        block->terminator.as.branch.else_target >= function->block_count) {
                        machine_ir_set_error(error, 0, 0, "MACHINE-IR-138: branch contract is invalid");
                        return 0;
                    }
                    break;
                default:
                    machine_ir_set_error(error, 0, 0, "MACHINE-IR-139: unknown terminator kind");
                    return 0;
            }
        }

        if (!machine_ir_verify_function_cfg(function, error)) {
            return 0;
        }
    }

    return 1;
}

#endif
