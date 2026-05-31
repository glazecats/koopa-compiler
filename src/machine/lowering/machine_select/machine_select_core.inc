#if defined(__INTELLISENSE__) && !defined(MACHINE_SELECT_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_SELECT_SPLIT_AGGREGATOR)
#define MACHINE_SELECT_SPLIT_INTELLISENSE_CONTEXT
#include "machine_select.c"
#else

void machine_select_program_init(MachineSelectProgram *program) {
    if (!program) {
        return;
    }
    program->register_bank.register_count = 0;
    program->register_bank.registers = NULL;
    program->globals = NULL;
    program->global_count = 0;
    program->global_capacity = 0;
    program->functions = NULL;
    program->function_count = 0;
    program->function_capacity = 0;
}

void machine_select_program_free(MachineSelectProgram *program) {
    size_t register_index;
    size_t global_index;
    size_t function_index;

    if (!program) {
        return;
    }
    if (program->register_bank.registers) {
        for (register_index = 0; register_index < program->register_bank.register_count; ++register_index) {
            machine_select_register_desc_free(&program->register_bank.registers[register_index]);
        }
    }
    if (program->globals) {
        for (global_index = 0; global_index < program->global_count; ++global_index) {
            machine_select_global_free(&program->globals[global_index]);
        }
    }
    if (program->functions) {
        for (function_index = 0; function_index < program->function_count; ++function_index) {
            machine_select_function_free(&program->functions[function_index]);
        }
    }
    free(program->register_bank.registers);
    free(program->globals);
    free(program->functions);
    machine_select_program_init(program);
}

int machine_select_clone_program(const MachineSelectProgram *source,
    MachineSelectProgram *out_program,
    MachineSelectError *error) {
    size_t register_index;
    size_t global_index;
    size_t function_index;

    if (!source || !out_program) {
        machine_select_set_error(error, 0, 0, "MACHINE-SELECT-050: invalid clone contract");
        return 0;
    }

    machine_select_program_free(out_program);

    out_program->register_bank.register_count = source->register_bank.register_count;
    if (source->register_bank.register_count > 0) {
        out_program->register_bank.registers = (MachineSelectRegisterDesc *)calloc(
            source->register_bank.register_count, sizeof(MachineSelectRegisterDesc));
        if (!out_program->register_bank.registers) {
            machine_select_set_error(error, 0, 0, "MACHINE-SELECT-051: out of memory cloning registers");
            machine_select_program_free(out_program);
            return 0;
        }
    }
    for (register_index = 0; register_index < source->register_bank.register_count; ++register_index) {
        MachineSelectRegisterDesc *dest = &out_program->register_bank.registers[register_index];
        const MachineSelectRegisterDesc *src = &source->register_bank.registers[register_index];

        *dest = *src;
        dest->name = machine_select_strdup(src->name);
        if (src->name && !dest->name) {
            machine_select_set_error(error, 0, 0, "MACHINE-SELECT-052: out of memory cloning register name");
            machine_select_program_free(out_program);
            return 0;
        }
    }

    out_program->global_count = source->global_count;
    out_program->global_capacity = source->global_count;
    if (source->global_count > 0) {
        out_program->globals = (MachineSelectGlobal *)calloc(source->global_count, sizeof(MachineSelectGlobal));
        if (!out_program->globals) {
            machine_select_set_error(error, 0, 0, "MACHINE-SELECT-053: out of memory cloning globals");
            machine_select_program_free(out_program);
            return 0;
        }
    }
    for (global_index = 0; global_index < source->global_count; ++global_index) {
        MachineSelectGlobal *dest = &out_program->globals[global_index];
        const MachineSelectGlobal *src = &source->globals[global_index];

        *dest = *src;
        dest->name = machine_select_strdup(src->name);
        if (src->name && !dest->name) {
            machine_select_set_error(error, 0, 0, "MACHINE-SELECT-054: out of memory cloning global name");
            machine_select_program_free(out_program);
            return 0;
        }
        dest->value_type = src->value_type;
    }

    out_program->function_count = source->function_count;
    out_program->function_capacity = source->function_count;
    if (source->function_count > 0) {
        out_program->functions = (MachineSelectFunction *)calloc(source->function_count, sizeof(MachineSelectFunction));
        if (!out_program->functions) {
            machine_select_set_error(error, 0, 0, "MACHINE-SELECT-055: out of memory cloning functions");
            machine_select_program_free(out_program);
            return 0;
        }
    }

    for (function_index = 0; function_index < source->function_count; ++function_index) {
        const MachineSelectFunction *src_function = &source->functions[function_index];
        MachineSelectFunction *dest_function = &out_program->functions[function_index];
        size_t local_index;
        size_t block_index;

        dest_function->has_body = src_function->has_body;
        dest_function->parameter_count = src_function->parameter_count;
        dest_function->local_count = src_function->local_count;
        dest_function->local_capacity = src_function->local_count;
        dest_function->block_count = src_function->block_count;
        dest_function->block_capacity = src_function->block_count;
        dest_function->spill_slot_count = src_function->spill_slot_count;
        dest_function->name = machine_select_strdup(src_function->name);
        if (src_function->name && !dest_function->name) {
            machine_select_set_error(error, 0, 0, "MACHINE-SELECT-056: out of memory cloning function name");
            machine_select_program_free(out_program);
            return 0;
        }

        if (src_function->local_count > 0) {
            dest_function->locals = (MachineSelectLocal *)calloc(src_function->local_count, sizeof(MachineSelectLocal));
            if (!dest_function->locals) {
                machine_select_set_error(error, 0, 0, "MACHINE-SELECT-057: out of memory cloning locals");
                machine_select_program_free(out_program);
                return 0;
            }
        }
        if (src_function->block_count > 0) {
            dest_function->blocks =
                (MachineSelectBasicBlock *)calloc(src_function->block_count, sizeof(MachineSelectBasicBlock));
            if (!dest_function->blocks) {
                machine_select_set_error(error, 0, 0, "MACHINE-SELECT-058: out of memory cloning blocks");
                machine_select_program_free(out_program);
                return 0;
            }
        }

        for (local_index = 0; local_index < src_function->local_count; ++local_index) {
            MachineSelectLocal *dest_local = &dest_function->locals[local_index];
            const MachineSelectLocal *src_local = &src_function->locals[local_index];

            *dest_local = *src_local;
            dest_local->source_name = machine_select_strdup(src_local->source_name);
            if (src_local->source_name && !dest_local->source_name) {
                machine_select_set_error(error, 0, 0, "MACHINE-SELECT-059: out of memory cloning local name");
                machine_select_program_free(out_program);
                return 0;
            }
            dest_local->value_type = src_local->value_type;
        }

        for (block_index = 0; block_index < src_function->block_count; ++block_index) {
            MachineSelectBasicBlock *dest_block = &dest_function->blocks[block_index];
            const MachineSelectBasicBlock *src_block = &src_function->blocks[block_index];
            size_t op_index;

            dest_block->id = src_block->id;
            dest_block->op_count = src_block->op_count;
            dest_block->op_capacity = src_block->op_count;
            dest_block->has_terminator = src_block->has_terminator;
            dest_block->terminator = src_block->terminator;
            if (src_block->op_count > 0) {
                dest_block->ops = (MachineSelectOp *)calloc(src_block->op_count, sizeof(MachineSelectOp));
                if (!dest_block->ops) {
                    machine_select_set_error(error, 0, 0, "MACHINE-SELECT-060: out of memory cloning ops");
                    machine_select_program_free(out_program);
                    return 0;
                }
            }

            for (op_index = 0; op_index < src_block->op_count; ++op_index) {
                MachineSelectOp *dest_op = &dest_block->ops[op_index];
                const MachineSelectOp *src_op = &src_block->ops[op_index];

                *dest_op = *src_op;
                if (machine_select_op_is_call_kind(src_op->kind)) {
                    dest_op->as.call.callee_name = machine_select_strdup(src_op->as.call.callee_name);
                    if (src_op->as.call.callee_name && !dest_op->as.call.callee_name) {
                        machine_select_set_error(error, 0, 0, "MACHINE-SELECT-061: out of memory cloning callee");
                        machine_select_program_free(out_program);
                        return 0;
                    }
                    if (src_op->as.call.arg_count > 0) {
                        dest_op->as.call.args =
                            (MachineSelectOperand *)calloc(src_op->as.call.arg_count, sizeof(MachineSelectOperand));
                        if (!dest_op->as.call.args) {
                            machine_select_set_error(error, 0, 0, "MACHINE-SELECT-062: out of memory cloning call args");
                            machine_select_program_free(out_program);
                            return 0;
                        }
                        memcpy(dest_op->as.call.args,
                            src_op->as.call.args,
                            src_op->as.call.arg_count * sizeof(MachineSelectOperand));
                    }
                } else if (src_op->kind == MACHINE_SELECT_OP_ADDR_FUNCTION) {
                    dest_op->as.addr_function_name = machine_select_strdup(src_op->as.addr_function_name);
                    if (src_op->as.addr_function_name && !dest_op->as.addr_function_name) {
                        machine_select_set_error(error, 0, 0, "MACHINE-SELECT-062A: out of memory cloning function address");
                        machine_select_program_free(out_program);
                        return 0;
                    }
                }
            }
        }
    }

    return 1;
}

void machine_select_lower_report_init(MachineSelectLowerReport *report) {
    if (!report) {
        return;
    }
    machine_select_program_init(&report->program);
    memset(&report->target_policy_summary, 0, sizeof(report->target_policy_summary));
    report->function_summaries = NULL;
    report->function_summary_count = 0;
    report->function_block_summary_offsets = NULL;
    report->block_summaries = NULL;
    report->total_block_summary_count = 0;
    report->functions_with_calls_count = 0;
    report->functions_with_spills_count = 0;
    report->functions_with_memory_ops_count = 0;
    report->functions_with_branches_count = 0;
    report->function_indices_with_calls = NULL;
    report->function_indices_with_spills = NULL;
    report->function_indices_with_memory_ops = NULL;
    report->function_indices_with_branches = NULL;
}

void machine_select_lower_report_free(MachineSelectLowerReport *report) {
    if (!report) {
        return;
    }
    free(report->function_summaries);
    free(report->function_block_summary_offsets);
    free(report->block_summaries);
    free(report->function_indices_with_calls);
    free(report->function_indices_with_spills);
    free(report->function_indices_with_memory_ops);
    free(report->function_indices_with_branches);
    machine_select_program_free(&report->program);
    machine_select_lower_report_init(report);
}

int machine_select_program_get_target_policy_summary(const MachineSelectProgram *program,
    MachineSelectTargetPolicySummary *out_summary) {
    if (!program || !out_summary) {
        return 0;
    }
    return machine_select_get_target_policy_summary(out_summary);
}

MachineSelectOperand machine_select_operand_none(void) {
    MachineSelectOperand operand;

    operand.kind = MACHINE_SELECT_OPERAND_NONE;
    operand.immediate = 0;
    operand.machine_register_id = 0;
    operand.spill_slot = 0;
    return operand;
}

MachineSelectOperand machine_select_operand_immediate(long long value) {
    MachineSelectOperand operand = machine_select_operand_none();

    operand.kind = MACHINE_SELECT_OPERAND_IMMEDIATE;
    operand.immediate = value;
    return operand;
}

MachineSelectOperand machine_select_operand_register(size_t machine_register_id) {
    MachineSelectOperand operand = machine_select_operand_none();

    operand.kind = MACHINE_SELECT_OPERAND_REGISTER;
    operand.machine_register_id = machine_register_id;
    return operand;
}

MachineSelectOperand machine_select_operand_spill_slot(size_t spill_slot) {
    MachineSelectOperand operand = machine_select_operand_none();

    operand.kind = MACHINE_SELECT_OPERAND_SPILL_SLOT;
    operand.spill_slot = spill_slot;
    return operand;
}

MachineSelectSlotRef machine_select_slot_local(size_t local_id) {
    MachineSelectSlotRef slot;

    slot.kind = MACHINE_SELECT_SLOT_LOCAL;
    slot.id = local_id;
    return slot;
}

MachineSelectSlotRef machine_select_slot_global(size_t global_id) {
    MachineSelectSlotRef slot;

    slot.kind = MACHINE_SELECT_SLOT_GLOBAL;
    slot.id = global_id;
    return slot;
}

#endif
