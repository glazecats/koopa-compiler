#if defined(__INTELLISENSE__) && !defined(MACHINE_IR_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_IR_SPLIT_AGGREGATOR)
#define MACHINE_IR_SPLIT_INTELLISENSE_CONTEXT
#include "machine_ir.c"
#else

typedef struct {
    size_t function_count;
    size_t global_count;
    unsigned char *global_reads;
    unsigned char *global_writes;
} MachineIrGlobalCallEffectSummary;

static void machine_ir_global_call_effect_summary_init(MachineIrGlobalCallEffectSummary *summary) {
    if (!summary) {
        return;
    }
    memset(summary, 0, sizeof(*summary));
}

static void machine_ir_global_call_effect_summary_free(MachineIrGlobalCallEffectSummary *summary) {
    if (!summary) {
        return;
    }
    free(summary->global_reads);
    free(summary->global_writes);
    machine_ir_global_call_effect_summary_init(summary);
}

static size_t machine_ir_find_function_index_by_name(const MachineIrProgram *program,
    const char *function_name) {
    size_t function_index;

    if (!program || !function_name) {
        return (size_t)-1;
    }
    for (function_index = 0; function_index < program->function_count; ++function_index) {
        if (!program->functions[function_index].name) {
            continue;
        }
        if (strcmp(program->functions[function_index].name, function_name) == 0) {
            return function_index;
        }
    }
    return (size_t)-1;
}

static int machine_ir_compute_global_call_effect_summary(const MachineIrProgram *program,
    MachineIrGlobalCallEffectSummary *out_summary,
    MachineIrError *error) {
    MachineIrGlobalCallEffectSummary summary;
    int changed = 1;
    size_t function_index;

    if (out_summary) {
        machine_ir_global_call_effect_summary_init(out_summary);
    }
    if (!program || !out_summary) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-539: invalid call-effect summary contract");
        return 0;
    }

    machine_ir_global_call_effect_summary_init(&summary);
    summary.function_count = program->function_count;
    summary.global_count = program->global_count;

    if (summary.function_count == 0 || summary.global_count == 0) {
        *out_summary = summary;
        return 1;
    }

    summary.global_reads =
        (unsigned char *)calloc(summary.function_count * summary.global_count, sizeof(unsigned char));
    summary.global_writes =
        (unsigned char *)calloc(summary.function_count * summary.global_count, sizeof(unsigned char));
    if (!summary.global_reads || !summary.global_writes) {
        machine_ir_global_call_effect_summary_free(&summary);
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-540: out of memory building call-effect summary");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const MachineIrFunction *function = &program->functions[function_index];
        size_t block_index;

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            const MachineIrBasicBlock *block = &function->blocks[block_index];
            size_t instruction_index;

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                const MachineIrInstruction *instruction = &block->instructions[instruction_index];

                switch (instruction->kind) {
                    case MACHINE_IR_INSTR_LOAD_GLOBAL:
                        if (instruction->as.load_slot.id < summary.global_count) {
                            summary.global_reads[(function_index * summary.global_count) +
                                instruction->as.load_slot.id] = 1u;
                        }
                        break;
                    case MACHINE_IR_INSTR_STORE_GLOBAL:
                        if (instruction->as.store.slot.id < summary.global_count) {
                            summary.global_writes[(function_index * summary.global_count) +
                                instruction->as.store.slot.id] = 1u;
                        }
                        break;
                    case MACHINE_IR_INSTR_CALL: {
                        size_t callee_index =
                            machine_ir_find_function_index_by_name(program, instruction->as.call.callee_name);
                        size_t global_index;

                        if (callee_index == (size_t)-1 || !program->functions[callee_index].has_body) {
                            for (global_index = 0; global_index < summary.global_count; ++global_index) {
                                summary.global_reads[(function_index * summary.global_count) + global_index] = 1u;
                                summary.global_writes[(function_index * summary.global_count) + global_index] = 1u;
                            }
                        }
                        break;
                    }
                    default:
                        break;
                }
            }
        }
    }

    while (changed) {
        changed = 0;
        for (function_index = 0; function_index < program->function_count; ++function_index) {
            const MachineIrFunction *function = &program->functions[function_index];
            size_t block_index;

            for (block_index = 0; block_index < function->block_count; ++block_index) {
                const MachineIrBasicBlock *block = &function->blocks[block_index];
                size_t instruction_index;

                for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                    const MachineIrInstruction *instruction = &block->instructions[instruction_index];

                    if (instruction->kind == MACHINE_IR_INSTR_CALL) {
                        size_t callee_index =
                            machine_ir_find_function_index_by_name(program, instruction->as.call.callee_name);
                        size_t global_index;

                        if (callee_index == (size_t)-1 || !program->functions[callee_index].has_body) {
                            continue;
                        }

                        for (global_index = 0; global_index < summary.global_count; ++global_index) {
                            unsigned char *read_slot =
                                &summary.global_reads[(function_index * summary.global_count) + global_index];
                            unsigned char *write_slot =
                                &summary.global_writes[(function_index * summary.global_count) + global_index];
                            unsigned char callee_reads =
                                summary.global_reads[(callee_index * summary.global_count) + global_index];
                            unsigned char callee_writes =
                                summary.global_writes[(callee_index * summary.global_count) + global_index];

                            if (!*read_slot && callee_reads) {
                                *read_slot = 1u;
                                changed = 1;
                            }
                            if (!*write_slot && callee_writes) {
                                *write_slot = 1u;
                                changed = 1;
                            }
                        }
                    }
                }
            }
        }
    }

    *out_summary = summary;
    return 1;
}

static int machine_ir_call_may_read_global(const MachineIrProgram *program,
    const MachineIrGlobalCallEffectSummary *summary,
    const MachineIrInstruction *instruction,
    size_t global_id) {
    size_t callee_index;

    if (!program || !instruction || instruction->kind != MACHINE_IR_INSTR_CALL) {
        return 0;
    }
    callee_index = machine_ir_find_function_index_by_name(program, instruction->as.call.callee_name);
    if (callee_index == (size_t)-1 || !summary || !program->functions[callee_index].has_body) {
        return 1;
    }
    if (global_id >= summary->global_count || callee_index >= summary->function_count) {
        return 1;
    }
    return summary->global_reads[(callee_index * summary->global_count) + global_id] != 0;
}

static int machine_ir_call_may_write_global(const MachineIrProgram *program,
    const MachineIrGlobalCallEffectSummary *summary,
    const MachineIrInstruction *instruction,
    size_t global_id) {
    size_t callee_index;

    if (!program || !instruction || instruction->kind != MACHINE_IR_INSTR_CALL) {
        return 0;
    }
    callee_index = machine_ir_find_function_index_by_name(program, instruction->as.call.callee_name);
    if (callee_index == (size_t)-1 || !summary || !program->functions[callee_index].has_body) {
        return 1;
    }
    if (global_id >= summary->global_count || callee_index >= summary->function_count) {
        return 1;
    }
    return summary->global_writes[(callee_index * summary->global_count) + global_id] != 0;
}

#endif
