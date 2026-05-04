#if defined(__INTELLISENSE__) && !defined(MACHINE_IR_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_IR_SPLIT_AGGREGATOR)
#define MACHINE_IR_SPLIT_INTELLISENSE_CONTEXT
#include "machine_ir.c"
#else

static const char *machine_ir_binary_op_name(MachineIrBinaryOp op) {
    switch (op) {
        case MACHINE_IR_BINARY_ADD:
            return "add";
        case MACHINE_IR_BINARY_SUB:
            return "sub";
        case MACHINE_IR_BINARY_MUL:
            return "mul";
        case MACHINE_IR_BINARY_DIV:
            return "div";
        case MACHINE_IR_BINARY_MOD:
            return "mod";
        case MACHINE_IR_BINARY_BIT_AND:
            return "and";
        case MACHINE_IR_BINARY_BIT_XOR:
            return "xor";
        case MACHINE_IR_BINARY_BIT_OR:
            return "or";
        case MACHINE_IR_BINARY_SHIFT_LEFT:
            return "shl";
        case MACHINE_IR_BINARY_SHIFT_RIGHT:
            return "shr";
        case MACHINE_IR_BINARY_EQ:
            return "eq";
        case MACHINE_IR_BINARY_NE:
            return "ne";
        case MACHINE_IR_BINARY_LT:
            return "lt";
        case MACHINE_IR_BINARY_LE:
            return "le";
        case MACHINE_IR_BINARY_GT:
            return "gt";
        case MACHINE_IR_BINARY_GE:
            return "ge";
        default:
            return "unknown";
    }
}

static int machine_ir_append_operand(MachineIrStringBuilder *builder,
    const MachineIrProgram *program,
    MachineIrOperand operand) {
    if (!builder || !program) {
        return 0;
    }

    switch (operand.kind) {
        case MACHINE_IR_OPERAND_NONE:
            return machine_ir_append_format(builder, "<none>");
        case MACHINE_IR_OPERAND_IMMEDIATE:
            return machine_ir_append_format(builder, "%lld", operand.immediate);
        case MACHINE_IR_OPERAND_REGISTER:
            if (operand.machine_register_id < program->register_bank.register_count &&
                program->register_bank.registers[operand.machine_register_id].name) {
                return machine_ir_append_format(builder,
                    "reg.%zu(%s)",
                    operand.machine_register_id,
                    program->register_bank.registers[operand.machine_register_id].name);
            }
            return machine_ir_append_format(builder, "reg.%zu", operand.machine_register_id);
        case MACHINE_IR_OPERAND_SPILL_SLOT:
            return machine_ir_append_format(builder, "spill.%zu", operand.spill_slot);
        default:
            return 0;
    }
}

static int machine_ir_append_slot(MachineIrStringBuilder *builder,
    MachineIrSlotRef slot) {
    if (!builder) {
        return 0;
    }

    if (slot.kind == MACHINE_IR_SLOT_LOCAL) {
        return machine_ir_append_format(builder, "local.%zu", slot.id);
    }
    return machine_ir_append_format(builder, "global.%zu", slot.id);
}

int machine_ir_dump_program(const MachineIrProgram *program,
    char **out_text,
    MachineIrError *error) {
    MachineIrStringBuilder builder;
    size_t register_index;
    size_t global_index;
    size_t function_index;

    if (out_text) {
        *out_text = NULL;
    }
    if (!program || !out_text) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-200: invalid dump contract");
        return 0;
    }

    builder.data = NULL;
    builder.length = 0;
    builder.capacity = 0;

    if (!machine_ir_append_format(&builder, "machine_ir\nregister-bank:\n")) {
        goto oom;
    }
    for (register_index = 0; register_index < program->register_bank.register_count; ++register_index) {
        const MachineIrRegisterDesc *reg = &program->register_bank.registers[register_index];

        if (!machine_ir_append_format(&builder,
                "  reg.%zu name=%s alloc=%u caller-clobbered=%u callee-preserved=%u\n",
                reg->register_id,
                reg->name ? reg->name : "<unnamed>",
                reg->allocatable,
                reg->caller_clobbered,
                reg->callee_preserved)) {
            goto oom;
        }
    }

    if (!machine_ir_append_format(&builder, "globals:\n")) {
        goto oom;
    }
    for (global_index = 0; global_index < program->global_count; ++global_index) {
        const MachineIrGlobal *global = &program->globals[global_index];

        if (!machine_ir_append_format(&builder,
                "  global.%zu %s init=%d runtime-init=%d",
                global->id,
                global->name ? global->name : "<unnamed>",
                global->has_initializer,
                global->has_runtime_initializer)) {
            goto oom;
        }
        if (global->has_initializer && !machine_ir_append_format(&builder, " value=%lld", global->initializer_value)) {
            goto oom;
        }
        if (!machine_ir_append_format(&builder, "\n")) {
            goto oom;
        }
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const MachineIrFunction *function = &program->functions[function_index];
        size_t block_index;

        if (!machine_ir_append_format(&builder,
                "function %s params=%zu locals=%zu spills=%zu\n",
                function->name ? function->name : "<unnamed>",
                function->parameter_count,
                function->local_count,
                function->spill_slot_count)) {
            goto oom;
        }
        for (block_index = 0; block_index < function->block_count; ++block_index) {
            const MachineIrBasicBlock *block = &function->blocks[block_index];
            size_t phi_index;
            size_t instruction_index;

            if (!machine_ir_append_format(&builder, "  bb.%zu:\n", block->id)) {
                goto oom;
            }

            for (phi_index = 0; phi_index < block->phi_count; ++phi_index) {
                const MachineIrPhi *phi = &block->phis[phi_index];
                size_t input_index;

                if (!machine_ir_append_format(&builder, "    phi ")) {
                    goto oom;
                }
                if (!machine_ir_append_operand(&builder, program, phi->result) ||
                    !machine_ir_append_format(&builder, " = ")) {
                    goto oom;
                }
                for (input_index = 0; input_index < phi->input_count; ++input_index) {
                    if (input_index > 0 && !machine_ir_append_format(&builder, ", ")) {
                        goto oom;
                    }
                    if (!machine_ir_append_format(&builder, "[bb.%zu: ", phi->inputs[input_index].predecessor_block_id) ||
                        !machine_ir_append_operand(&builder, program, phi->inputs[input_index].value) ||
                        !machine_ir_append_format(&builder, "]")) {
                        goto oom;
                    }
                }
                if (!machine_ir_append_format(&builder, "\n")) {
                    goto oom;
                }
            }

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                const MachineIrInstruction *instruction = &block->instructions[instruction_index];
                size_t arg_index;

                if (!machine_ir_append_format(&builder, "    ")) {
                    goto oom;
                }
                if (instruction->has_result) {
                    if (!machine_ir_append_operand(&builder, program, instruction->result) ||
                        !machine_ir_append_format(&builder, " = ")) {
                        goto oom;
                    }
                }

                switch (instruction->kind) {
                    case MACHINE_IR_INSTR_MOV:
                        if (!machine_ir_append_format(&builder, "mov ") ||
                            !machine_ir_append_operand(&builder, program, instruction->as.mov_value)) {
                            goto oom;
                        }
                        break;
                    case MACHINE_IR_INSTR_BINARY:
                        if (!machine_ir_append_format(&builder, "%s ", machine_ir_binary_op_name(instruction->as.binary.op)) ||
                            !machine_ir_append_operand(&builder, program, instruction->as.binary.lhs) ||
                            !machine_ir_append_format(&builder, ", ") ||
                            !machine_ir_append_operand(&builder, program, instruction->as.binary.rhs)) {
                            goto oom;
                        }
                        break;
                    case MACHINE_IR_INSTR_CALL:
                        if (!machine_ir_append_format(&builder, "call %s(",
                                instruction->as.call.callee_name ? instruction->as.call.callee_name : "<null>")) {
                            goto oom;
                        }
                        for (arg_index = 0; arg_index < instruction->as.call.arg_count; ++arg_index) {
                            if (arg_index > 0 && !machine_ir_append_format(&builder, ", ")) {
                                goto oom;
                            }
                            if (!machine_ir_append_operand(&builder, program, instruction->as.call.args[arg_index])) {
                                goto oom;
                            }
                        }
                        if (!machine_ir_append_format(&builder, ")")) {
                            goto oom;
                        }
                        break;
                    case MACHINE_IR_INSTR_LOAD_LOCAL:
                    case MACHINE_IR_INSTR_LOAD_GLOBAL:
                        if (!machine_ir_append_format(&builder, "load ") ||
                            !machine_ir_append_slot(&builder, instruction->as.load_slot)) {
                            goto oom;
                        }
                        break;
                    case MACHINE_IR_INSTR_STORE_LOCAL:
                    case MACHINE_IR_INSTR_STORE_GLOBAL:
                        if (!machine_ir_append_format(&builder, "store ") ||
                            !machine_ir_append_slot(&builder, instruction->as.store.slot) ||
                            !machine_ir_append_format(&builder, ", ") ||
                            !machine_ir_append_operand(&builder, program, instruction->as.store.value)) {
                            goto oom;
                        }
                        break;
                    default:
                        if (!machine_ir_append_format(&builder, "<unknown>")) {
                            goto oom;
                        }
                        break;
                }

                if (!machine_ir_append_format(&builder, "\n")) {
                    goto oom;
                }
            }

            if (!machine_ir_append_format(&builder, "    ")) {
                goto oom;
            }
            switch (block->terminator.kind) {
                case MACHINE_IR_TERM_RETURN:
                    if (!machine_ir_append_format(&builder, "ret")) {
                        goto oom;
                    }
                    if (block->terminator.as.return_value.kind != MACHINE_IR_OPERAND_NONE &&
                        (!machine_ir_append_format(&builder, " ") ||
                            !machine_ir_append_operand(&builder, program, block->terminator.as.return_value))) {
                        goto oom;
                    }
                    break;
                case MACHINE_IR_TERM_JUMP:
                    if (!machine_ir_append_format(&builder, "jmp bb.%zu", block->terminator.as.jump_target)) {
                        goto oom;
                    }
                    break;
                case MACHINE_IR_TERM_BRANCH:
                    if (!machine_ir_append_format(&builder, "br ") ||
                        !machine_ir_append_operand(&builder, program, block->terminator.as.branch.condition) ||
                        !machine_ir_append_format(&builder,
                            ", bb.%zu, bb.%zu",
                            block->terminator.as.branch.then_target,
                            block->terminator.as.branch.else_target)) {
                        goto oom;
                    }
                    break;
                default:
                    if (!machine_ir_append_format(&builder, "<bad-term>")) {
                        goto oom;
                    }
                    break;
            }
            if (!machine_ir_append_format(&builder, "\n")) {
                goto oom;
            }
        }
    }

    *out_text = builder.data;
    return 1;

oom:
    free(builder.data);
    machine_ir_set_error(error, 0, 0, "MACHINE-IR-201: out of memory while dumping machine IR");
    return 0;
}

int machine_ir_dump_allocate_rewrite_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineIrError *error) {
    MachineIrStringBuilder builder;
    char *program_text = NULL;
    size_t allocation_rounds = 0;
    size_t rewrite_rounds = 0;
    size_t index;
    int ok = 0;

    if (out_text) {
        *out_text = NULL;
    }
    if (!report || !out_text) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-202: invalid allocate+rewrite machine-ir report dump contract");
        return 0;
    }

    builder.data = NULL;
    builder.length = 0;
    builder.capacity = 0;
    allocation_rounds = report->allocation_rounds;
    rewrite_rounds = report->rewrite_rounds;

    if (!machine_ir_append_format(&builder,
            "allocate+rewrite machine-ir-report allocation_rounds=%zu rewrite_rounds=%zu phi_funcs=%zu call_funcs=%zu spill_funcs=%zu memory_funcs=%zu register_only_funcs=%zu\n",
            allocation_rounds,
            rewrite_rounds,
            report->functions_with_phi_count,
            report->functions_with_call_count,
            report->functions_with_spills_count,
            report->functions_with_memory_ops_count,
            report->functions_register_only_count)) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-203: out of memory while dumping machine-ir report header");
        goto cleanup;
    }

    if (!machine_ir_append_format(&builder, "functions-with-phi:")) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-203: out of memory while dumping machine-ir report");
        goto cleanup;
    }
    for (index = 0; index < report->functions_with_phi_count; ++index) {
        if (!machine_ir_append_format(&builder, " %zu", report->function_indices_with_phi[index])) {
            machine_ir_set_error(error, 0, 0, "MACHINE-IR-203: out of memory while dumping machine-ir report");
            goto cleanup;
        }
    }
    if (!machine_ir_append_format(&builder, "\nfunctions-with-call:")) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-203: out of memory while dumping machine-ir report");
        goto cleanup;
    }
    for (index = 0; index < report->functions_with_call_count; ++index) {
        if (!machine_ir_append_format(&builder, " %zu", report->function_indices_with_call[index])) {
            machine_ir_set_error(error, 0, 0, "MACHINE-IR-203: out of memory while dumping machine-ir report");
            goto cleanup;
        }
    }
    if (!machine_ir_append_format(&builder, "\nfunctions-with-spills:")) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-203: out of memory while dumping machine-ir report");
        goto cleanup;
    }
    for (index = 0; index < report->functions_with_spills_count; ++index) {
        if (!machine_ir_append_format(&builder, " %zu", report->function_indices_with_spills[index])) {
            machine_ir_set_error(error, 0, 0, "MACHINE-IR-203: out of memory while dumping machine-ir report");
            goto cleanup;
        }
    }
    if (!machine_ir_append_format(&builder, "\nfunctions-with-memory-ops:")) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-203: out of memory while dumping machine-ir report");
        goto cleanup;
    }
    for (index = 0; index < report->functions_with_memory_ops_count; ++index) {
        if (!machine_ir_append_format(&builder, " %zu", report->function_indices_with_memory_ops[index])) {
            machine_ir_set_error(error, 0, 0, "MACHINE-IR-203: out of memory while dumping machine-ir report");
            goto cleanup;
        }
    }
    if (!machine_ir_append_format(&builder, "\nfunctions-register-only:")) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-203: out of memory while dumping machine-ir report");
        goto cleanup;
    }
    for (index = 0; index < report->functions_register_only_count; ++index) {
        if (!machine_ir_append_format(&builder, " %zu", report->function_indices_register_only[index])) {
            machine_ir_set_error(error, 0, 0, "MACHINE-IR-203: out of memory while dumping machine-ir report");
            goto cleanup;
        }
    }
    if (!machine_ir_append_format(&builder, "\nfunction-shapes:\n")) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-203: out of memory while dumping machine-ir report");
        goto cleanup;
    }
    for (index = 0; index < report->program.function_count; ++index) {
        const MachineIrFunction *function = &report->program.functions[index];
        const MachineIrFunctionShapeSummary *summary =
            report->function_shape_summaries ? &report->function_shape_summaries[index] : NULL;
        size_t block_summary_index;
        size_t block_offset =
            report->function_block_summary_offsets ? report->function_block_summary_offsets[index] : 0;
        size_t next_block_offset = index + 1 < report->program.function_count && report->function_block_summary_offsets
            ? report->function_block_summary_offsets[index + 1]
            : report->total_block_summary_count;

        if (!machine_ir_append_format(&builder,
                "  fn.%zu %s blocks=%zu phi=%zu call=%zu spills=%zu load_local=%zu store_local=%zu load_global=%zu store_global=%zu memory_ops=%zu blocks_with_phi=%zu blocks_with_call=%zu blocks_with_memory_ops=%zu branch_blocks=%zu jump_blocks=%zu return_blocks=%zu\n",
                index,
                function->name ? function->name : "<unnamed>",
                summary ? summary->block_count : 0,
                summary ? summary->phi_count : 0,
                summary ? summary->call_count : 0,
                summary ? summary->spill_slot_count : 0,
                summary ? summary->load_local_count : 0,
                summary ? summary->store_local_count : 0,
                summary ? summary->load_global_count : 0,
                summary ? summary->store_global_count : 0,
                summary ? summary->memory_op_count : 0,
                summary ? summary->blocks_with_phi_count : 0,
                summary ? summary->blocks_with_call_count : 0,
                summary ? summary->blocks_with_memory_ops_count : 0,
                summary ? summary->branch_block_count : 0,
                summary ? summary->jump_block_count : 0,
                summary ? summary->return_block_count : 0)) {
            machine_ir_set_error(error, 0, 0, "MACHINE-IR-203: out of memory while dumping machine-ir report");
            goto cleanup;
        }
        for (block_summary_index = block_offset; block_summary_index < next_block_offset; ++block_summary_index) {
            const MachineIrBlockShapeSummary *block_summary = &report->block_shape_summaries[block_summary_index];

            if (!machine_ir_append_format(&builder,
                    "    bb.%zu phi=%zu instr=%zu call=%zu load_local=%zu store_local=%zu load_global=%zu store_global=%zu memory_ops=%zu term_ret=%d term_jmp=%d term_br=%d\n",
                    block_summary->block_id,
                    block_summary->phi_count,
                    block_summary->instruction_count,
                    block_summary->call_count,
                    block_summary->load_local_count,
                    block_summary->store_local_count,
                    block_summary->load_global_count,
                    block_summary->store_global_count,
                    block_summary->memory_op_count,
                    block_summary->has_return_terminator,
                    block_summary->has_jump_terminator,
                    block_summary->has_branch_terminator)) {
                machine_ir_set_error(error, 0, 0, "MACHINE-IR-203: out of memory while dumping machine-ir report");
                goto cleanup;
            }
        }
    }
    if (!machine_ir_append_format(&builder, "\n") ||
        !machine_ir_dump_program(&report->program, &program_text, error) ||
        !machine_ir_append_format(&builder, "%s", program_text ? program_text : "")) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-204: out of memory while dumping machine-ir report");
        goto cleanup;
    }

    *out_text = builder.data;
    builder.data = NULL;
    ok = 1;

cleanup:
    free(program_text);
    free(builder.data);
    return ok;
}

#endif
