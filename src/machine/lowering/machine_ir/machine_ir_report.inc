#if defined(__INTELLISENSE__) && !defined(MACHINE_IR_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_IR_SPLIT_AGGREGATOR)
#define MACHINE_IR_SPLIT_INTELLISENSE_CONTEXT
#include "machine_ir.c"
#else

static void machine_ir_allocate_rewrite_report_clear_shape(MachineIrAllocateRewriteReport *report) {
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

static int machine_ir_value_ssa_program_needs_conservative_lowering(const ValueSsaProgram *program) {
    size_t function_index;

    if (!program) {
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const ValueSsaFunction *function = &program->functions[function_index];
        size_t block_index;

        if (function->parameter_count > 16u) {
            return 1;
        }

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            const ValueSsaBasicBlock *block = &function->blocks[block_index];
            size_t instruction_index;

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                const ValueSsaInstruction *instruction = &block->instructions[instruction_index];

                if (instruction->kind == VALUE_SSA_INSTR_CALL &&
                    instruction->as.call.arg_count > 16u) {
                    return 1;
                }
                switch (instruction->kind) {
                case VALUE_SSA_INSTR_ADDR_LOCAL:
                case VALUE_SSA_INSTR_ADDR_GLOBAL:
                case VALUE_SSA_INSTR_LOAD_INDIRECT:
                case VALUE_SSA_INSTR_STORE_INDIRECT:
                    return 1;
                default:
                    break;
                }
            }
        }
    }

    return 0;
}

static int machine_ir_value_ssa_program_needs_all_spill_lowering(const ValueSsaProgram *program) {
    size_t function_index;
    size_t total_block_count = 0;
    size_t total_instruction_count = 0;
    size_t total_value_count = 0;

    if (!program) {
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const ValueSsaFunction *function = &program->functions[function_index];
        size_t function_instruction_count = 0;
        size_t block_index;

        if (function->parameter_count > 1024u) {
            return 1;
        }

        total_block_count += function->block_count;
        total_value_count += function->next_value_id;
        if (total_block_count > 5000u || total_value_count > 50000u) {
            return 1;
        }

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            const ValueSsaBasicBlock *block = &function->blocks[block_index];
            size_t instruction_index;

            function_instruction_count += block->phi_count + block->instruction_count;
            total_instruction_count += block->phi_count + block->instruction_count;
            if (function_instruction_count > 15000u || total_instruction_count > 30000u) {
                return 1;
            }

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                const ValueSsaInstruction *instruction = &block->instructions[instruction_index];

                if (instruction->kind == VALUE_SSA_INSTR_CALL &&
                    instruction->as.call.arg_count > 1024u) {
                    return 1;
                }
            }
        }
    }

    return 0;
}

static int machine_ir_build_all_spill_allocation_result(const ValueSsaProgram *program,
    ValueSsaProgramAllocationResult *out_result,
    MachineIrError *error) {
    size_t function_index;

    if (!program || !out_result) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-413: invalid all-spill allocation result contract");
        return 0;
    }

    value_ssa_program_allocation_result_free(out_result);
    out_result->function_count = program->function_count;
    if (program->function_count == 0) {
        return 1;
    }

    out_result->function_results =
        (ValueSsaAllocationResult *)calloc(program->function_count, sizeof(ValueSsaAllocationResult));
    if (!out_result->function_results) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-414: out of memory building all-spill allocation result");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const ValueSsaFunction *function = &program->functions[function_index];
        ValueSsaAllocationResult *function_result = &out_result->function_results[function_index];
        size_t value_id;

        value_ssa_allocation_result_init(function_result);
        function_result->value_count = function->next_value_id;
        function_result->spill_slot_count = function->next_value_id;
        if (function->name) {
            function_result->function_name = machine_ir_strdup(function->name);
            if (!function_result->function_name) {
                machine_ir_set_error(error, 0, 0, "MACHINE-IR-415: out of memory copying all-spill function name");
                value_ssa_program_allocation_result_free(out_result);
                return 0;
            }
        }
        if (function_result->value_count == 0) {
            continue;
        }

        function_result->spill_flags =
            (unsigned char *)calloc(function_result->value_count, sizeof(unsigned char));
        function_result->spill_confirmed_flags =
            (unsigned char *)calloc(function_result->value_count, sizeof(unsigned char));
        function_result->spill_slots =
            (size_t *)malloc(function_result->value_count * sizeof(size_t));
        if (!function_result->spill_flags || !function_result->spill_confirmed_flags || !function_result->spill_slots) {
            machine_ir_set_error(error, 0, 0, "MACHINE-IR-416: out of memory building all-spill placement tables");
            value_ssa_program_allocation_result_free(out_result);
            return 0;
        }

        for (value_id = 0; value_id < function_result->value_count; ++value_id) {
            function_result->spill_flags[value_id] = 1;
            function_result->spill_confirmed_flags[value_id] = 1;
            function_result->spill_slots[value_id] = value_id;
        }
    }

    return 1;
}

int machine_ir_allocate_rewrite_report_refresh_shape(MachineIrAllocateRewriteReport *report,
    MachineIrError *error) {
    size_t total_block_summary_count = 0;
    size_t function_index;

    if (!report) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-412: invalid machine-ir report refresh contract");
        return 0;
    }

    machine_ir_allocate_rewrite_report_clear_shape(report);
    for (function_index = 0; function_index < report->program.function_count; ++function_index) {
        total_block_summary_count += report->program.functions[function_index].block_count;
    }

    if (report->program.function_count > 0) {
        report->function_shape_summaries =
            (MachineIrFunctionShapeSummary *)calloc(report->program.function_count, sizeof(MachineIrFunctionShapeSummary));
        report->function_block_summary_offsets =
            (size_t *)calloc(report->program.function_count, sizeof(size_t));
        report->function_phi_counts =
            (size_t *)calloc(report->program.function_count, sizeof(size_t));
        report->function_call_counts =
            (size_t *)calloc(report->program.function_count, sizeof(size_t));
        report->function_spill_slot_counts =
            (size_t *)calloc(report->program.function_count, sizeof(size_t));
        report->function_load_local_counts =
            (size_t *)calloc(report->program.function_count, sizeof(size_t));
        report->function_store_local_counts =
            (size_t *)calloc(report->program.function_count, sizeof(size_t));
        report->function_load_global_counts =
            (size_t *)calloc(report->program.function_count, sizeof(size_t));
        report->function_store_global_counts =
            (size_t *)calloc(report->program.function_count, sizeof(size_t));
        report->function_memory_op_counts =
            (size_t *)calloc(report->program.function_count, sizeof(size_t));
        report->block_shape_summaries =
            total_block_summary_count > 0
                ? (MachineIrBlockShapeSummary *)calloc(total_block_summary_count, sizeof(MachineIrBlockShapeSummary))
                : NULL;
        report->total_block_summary_count = total_block_summary_count;
        report->function_indices_with_phi =
            (size_t *)malloc(report->program.function_count * sizeof(size_t));
        report->function_indices_with_call =
            (size_t *)malloc(report->program.function_count * sizeof(size_t));
        report->function_indices_with_spills =
            (size_t *)malloc(report->program.function_count * sizeof(size_t));
        report->function_indices_with_memory_ops =
            (size_t *)malloc(report->program.function_count * sizeof(size_t));
        report->function_indices_register_only =
            (size_t *)malloc(report->program.function_count * sizeof(size_t));
        if (!report->function_shape_summaries || !report->function_block_summary_offsets ||
            !report->function_phi_counts || !report->function_call_counts ||
            !report->function_spill_slot_counts || !report->function_load_local_counts ||
            !report->function_store_local_counts || !report->function_load_global_counts ||
            !report->function_store_global_counts || !report->function_memory_op_counts ||
            (total_block_summary_count > 0 && !report->block_shape_summaries) ||
            !report->function_indices_with_phi || !report->function_indices_with_call ||
            !report->function_indices_with_spills || !report->function_indices_with_memory_ops ||
            !report->function_indices_register_only) {
            machine_ir_set_error(error, 0, 0, "MACHINE-IR-411: out of memory building machine-ir report indices");
            machine_ir_allocate_rewrite_report_clear_shape(report);
            return 0;
        }
    }

    total_block_summary_count = 0;
    for (function_index = 0; function_index < report->program.function_count; ++function_index) {
        const MachineIrFunction *function = &report->program.functions[function_index];
        MachineIrFunctionShapeSummary *summary = &report->function_shape_summaries[function_index];
        size_t block_index;
        int has_phi = 0;
        int has_call = 0;

        report->function_block_summary_offsets[function_index] = total_block_summary_count;
        summary->block_count = function->block_count;
        report->function_spill_slot_counts[function_index] = function->spill_slot_count;
        summary->spill_slot_count = function->spill_slot_count;
        for (block_index = 0; block_index < function->block_count; ++block_index) {
            const MachineIrBasicBlock *block = &function->blocks[block_index];
            MachineIrBlockShapeSummary *block_summary = &report->block_shape_summaries[total_block_summary_count++];
            size_t instruction_index;

            block_summary->block_id = block->id;
            block_summary->phi_count = block->phi_count;
            block_summary->instruction_count = block->instruction_count;
            summary->instruction_count += block->instruction_count;
            if (block->phi_count > 0) {
                has_phi = 1;
                report->function_phi_counts[function_index] += block->phi_count;
                summary->phi_count += block->phi_count;
                ++summary->blocks_with_phi_count;
            }
            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                switch (block->instructions[instruction_index].kind) {
                    case MACHINE_IR_INSTR_CALL:
                        has_call = 1;
                        ++report->function_call_counts[function_index];
                        ++summary->call_count;
                        ++block_summary->call_count;
                        break;
                    case MACHINE_IR_INSTR_LOAD_LOCAL:
                        ++report->function_load_local_counts[function_index];
                        ++report->function_memory_op_counts[function_index];
                        ++summary->load_local_count;
                        ++summary->memory_op_count;
                        ++block_summary->load_local_count;
                        ++block_summary->memory_op_count;
                        break;
                    case MACHINE_IR_INSTR_STORE_LOCAL:
                        ++report->function_store_local_counts[function_index];
                        ++report->function_memory_op_counts[function_index];
                        ++summary->store_local_count;
                        ++summary->memory_op_count;
                        ++block_summary->store_local_count;
                        ++block_summary->memory_op_count;
                        break;
                    case MACHINE_IR_INSTR_LOAD_GLOBAL:
                        ++report->function_load_global_counts[function_index];
                        ++report->function_memory_op_counts[function_index];
                        ++summary->load_global_count;
                        ++summary->memory_op_count;
                        ++block_summary->load_global_count;
                        ++block_summary->memory_op_count;
                        break;
                    case MACHINE_IR_INSTR_STORE_GLOBAL:
                        ++report->function_store_global_counts[function_index];
                        ++report->function_memory_op_counts[function_index];
                        ++summary->store_global_count;
                        ++summary->memory_op_count;
                        ++block_summary->store_global_count;
                        ++block_summary->memory_op_count;
                        break;
                    default:
                        break;
                }
            }
            if (block_summary->call_count > 0) {
                ++summary->blocks_with_call_count;
            }
            if (block_summary->memory_op_count > 0) {
                ++summary->blocks_with_memory_ops_count;
            }
            if (block->has_terminator) {
                switch (block->terminator.kind) {
                    case MACHINE_IR_TERM_RETURN:
                        block_summary->has_return_terminator = 1;
                        ++summary->return_block_count;
                        break;
                    case MACHINE_IR_TERM_JUMP:
                        block_summary->has_jump_terminator = 1;
                        ++summary->jump_block_count;
                        break;
                    case MACHINE_IR_TERM_BRANCH:
                        block_summary->has_branch_terminator = 1;
                        ++summary->branch_block_count;
                        break;
                    default:
                        break;
                }
            }
        }

        if (has_phi) {
            report->function_indices_with_phi[report->functions_with_phi_count++] = function_index;
        }
        if (has_call) {
            report->function_indices_with_call[report->functions_with_call_count++] = function_index;
        }
        if (function->spill_slot_count > 0) {
            report->function_indices_with_spills[report->functions_with_spills_count++] = function_index;
        }
        if (summary->memory_op_count > 0) {
            report->function_indices_with_memory_ops[report->functions_with_memory_ops_count++] = function_index;
        }
        if (function->spill_slot_count == 0) {
            report->function_indices_register_only[report->functions_register_only_count++] = function_index;
        }
    }

    return 1;
}

int machine_ir_lower_program_from_value_ssa_dump(const ValueSsaProgram *program,
    const ValueSsaProgramMachineAllocationView *machine_view,
    const ValueSsaMachineRegisterBank *bank,
    char **out_text,
    MachineIrError *error) {
    MachineIrProgram machine_program;
    int ok;

    if (out_text) {
        *out_text = NULL;
    }
    if (!program || !machine_view || !bank || !out_text) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-400: invalid Value-SSA machine-IR dump contract");
        return 0;
    }

    machine_ir_program_init(&machine_program);
    ok = machine_ir_lower_program_from_value_ssa(program, machine_view, bank, &machine_program, error) &&
        machine_ir_dump_program(&machine_program, out_text, error);
    machine_ir_program_free(&machine_program);
    return ok;
}

int machine_ir_allocate_lower_program_from_value_ssa_dump(const ValueSsaProgram *program,
    size_t color_budget,
    const ValueSsaMachineRegisterBank *bank,
    char **out_text,
    MachineIrError *error) {
    MachineIrProgram machine_program;
    int ok;

    if (out_text) {
        *out_text = NULL;
    }
    if (!program || !bank || !out_text) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-401: invalid allocate+lower machine-IR dump contract");
        return 0;
    }

    machine_ir_program_init(&machine_program);
    ok = machine_ir_allocate_lower_program_from_value_ssa(program, color_budget, bank, &machine_program, error) &&
        machine_ir_dump_program(&machine_program, out_text, error);
    machine_ir_program_free(&machine_program);
    return ok;
}

int machine_ir_allocate_lower_program_from_value_ssa_flat_dump(const ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    char **out_text,
    MachineIrError *error) {
    MachineIrProgram machine_program;
    int ok;

    if (out_text) {
        *out_text = NULL;
    }
    if (!program || !out_text) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-402: invalid flat allocate+lower machine-IR dump contract");
        return 0;
    }

    machine_ir_program_init(&machine_program);
    ok = machine_ir_allocate_lower_program_from_value_ssa_flat(
             program, color_budget, machine_register_count, &machine_program, error) &&
        machine_ir_dump_program(&machine_program, out_text, error);
    machine_ir_program_free(&machine_program);
    return ok;
}

int machine_ir_allocate_and_rewrite_program_single_block_spills(const ValueSsaProgram *program,
    size_t color_budget,
    const ValueSsaMachineRegisterBank *bank,
    MachineIrProgram *out_program,
    MachineIrError *error) {
    ValueSsaProgram rewritten_program;
    ValueSsaProgramAllocationResult result;
    ValueSsaProgramAllocationLayout layout;
    ValueSsaProgramMachineAllocationView machine_view;
    ValueSsaError value_error;
    const ValueSsaProgram *lowering_source = NULL;
    const ValueSsaProgram *layout_context_source = NULL;
    int ok = 0;
    int trace_timing = machine_ir_trace_enabled();
    double total_start = trace_timing ? machine_ir_now_s() : 0.0;
    double stage_start = total_start;

    if (!program || !bank || !out_program) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-403: invalid allocate+rewrite machine-IR contract");
        return 0;
    }

    value_ssa_program_init(&rewritten_program);
    value_ssa_program_allocation_result_init(&result);
    value_ssa_program_allocation_layout_init(&layout);
    value_ssa_program_machine_allocation_view_init(&machine_view);
    memset(&value_error, 0, sizeof(value_error));

    if (machine_ir_value_ssa_program_needs_all_spill_lowering(program)) {
        if (!machine_ir_build_all_spill_allocation_result(program, &result, error)) {
            goto cleanup;
        }
        if (trace_timing) {
            machine_ir_trace_timing("all-spill-result", machine_ir_now_s() - stage_start);
            stage_start = machine_ir_now_s();
        }
        lowering_source = program;
        layout_context_source = program;
    } else if (machine_ir_value_ssa_program_needs_conservative_lowering(program)) {
        if (!value_ssa_allocate_program(program, color_budget, &result, &value_error)) {
            machine_ir_set_error_from_value_ssa(error, &value_error);
            goto cleanup;
        }
        if (trace_timing) {
            machine_ir_trace_timing("conservative-allocate", machine_ir_now_s() - stage_start);
            stage_start = machine_ir_now_s();
        }
        lowering_source = program;
        layout_context_source = program;
    } else {
        if (!machine_ir_clone_value_ssa_program(program, &rewritten_program, error)) {
            goto cleanup;
        }
        if (trace_timing) {
            machine_ir_trace_timing("clone-value-ssa", machine_ir_now_s() - stage_start);
            stage_start = machine_ir_now_s();
        }
        if (!value_ssa_allocate_and_rewrite_program_single_block_spills(
                &rewritten_program, color_budget, &result, &value_error)) {
            machine_ir_set_error_from_value_ssa(error, &value_error);
            goto cleanup;
        }
        if (trace_timing) {
            machine_ir_trace_timing("allocate+rewrite", machine_ir_now_s() - stage_start);
            stage_start = machine_ir_now_s();
        }
        lowering_source = &rewritten_program;
        layout_context_source = &rewritten_program;
    }

    if (!value_ssa_build_program_allocation_layout(&result, &layout, &value_error)) {
        machine_ir_set_error_from_value_ssa(error, &value_error);
        goto cleanup;
    }
    if (trace_timing) {
        machine_ir_trace_timing("build-layout", machine_ir_now_s() - stage_start);
        stage_start = machine_ir_now_s();
    }
    if (!value_ssa_program_allocation_layout_attach_program_context(
            &layout,
            layout_context_source ? layout_context_source : lowering_source,
            &value_error)) {
        machine_ir_set_error_from_value_ssa(error, &value_error);
        goto cleanup;
    }
    if (trace_timing) {
        machine_ir_trace_timing("attach-layout-context", machine_ir_now_s() - stage_start);
        stage_start = machine_ir_now_s();
    }
    if (!value_ssa_build_program_machine_allocation_view(&layout, bank, &machine_view, &value_error)) {
        machine_ir_set_error_from_value_ssa(error, &value_error);
        goto cleanup;
    }
    if (trace_timing) {
        machine_ir_trace_timing("build-machine-view", machine_ir_now_s() - stage_start);
        stage_start = machine_ir_now_s();
    }
    ok = machine_ir_lower_program_from_value_ssa(
        lowering_source ? lowering_source : &rewritten_program, &machine_view, bank, out_program, error);
    if (trace_timing) {
        machine_ir_trace_timing("lower-machine-ir", machine_ir_now_s() - stage_start);
        machine_ir_trace_timing("total", machine_ir_now_s() - total_start);
    }

cleanup:
    value_ssa_program_machine_allocation_view_free(&machine_view);
    value_ssa_program_allocation_layout_free(&layout);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&rewritten_program);
    return ok;
}

int machine_ir_allocate_and_rewrite_program_single_block_spills_flat(const ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    MachineIrProgram *out_program,
    MachineIrError *error) {
    ValueSsaMachineRegisterBank bank;
    ValueSsaError value_error;
    int ok = 0;

    if (!program || !out_program) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-404: invalid flat allocate+rewrite machine-IR contract");
        return 0;
    }

    value_ssa_machine_register_bank_init(&bank);
    memset(&value_error, 0, sizeof(value_error));
    if (!value_ssa_build_flat_machine_register_bank(machine_register_count, &bank, &value_error)) {
        machine_ir_set_error_from_value_ssa(error, &value_error);
        return 0;
    }
    ok = machine_ir_allocate_and_rewrite_program_single_block_spills(
        program, color_budget, &bank, out_program, error);
    value_ssa_machine_register_bank_free(&bank);
    return ok;
}

int machine_ir_allocate_and_rewrite_program_single_block_spills_dump(const ValueSsaProgram *program,
    size_t color_budget,
    const ValueSsaMachineRegisterBank *bank,
    char **out_text,
    MachineIrError *error) {
    MachineIrProgram machine_program;
    int ok;

    if (out_text) {
        *out_text = NULL;
    }
    if (!program || !bank || !out_text) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-405: invalid allocate+rewrite machine-IR dump contract");
        return 0;
    }

    machine_ir_program_init(&machine_program);
    ok = machine_ir_allocate_and_rewrite_program_single_block_spills(
             program, color_budget, bank, &machine_program, error) &&
        machine_ir_dump_program(&machine_program, out_text, error);
    machine_ir_program_free(&machine_program);
    return ok;
}

int machine_ir_allocate_and_rewrite_program_single_block_spills_flat_dump(const ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    char **out_text,
    MachineIrError *error) {
    MachineIrProgram machine_program;
    int ok;

    if (out_text) {
        *out_text = NULL;
    }
    if (!program || !out_text) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-406: invalid flat allocate+rewrite machine-IR dump contract");
        return 0;
    }

    machine_ir_program_init(&machine_program);
    ok = machine_ir_allocate_and_rewrite_program_single_block_spills_flat(
             program, color_budget, machine_register_count, &machine_program, error) &&
        machine_ir_dump_program(&machine_program, out_text, error);
    machine_ir_program_free(&machine_program);
    return ok;
}

int machine_ir_build_allocate_and_rewrite_program_single_block_spills_report(
    const ValueSsaProgram *program,
    size_t color_budget,
    const ValueSsaMachineRegisterBank *bank,
    MachineIrAllocateRewriteReport *out_report,
    MachineIrError *error) {
    ValueSsaProgram rewritten_program;
    ValueSsaProgramAllocationResult result;
    ValueSsaAllocateRewriteStats stats;
    ValueSsaProgramAllocationLayout layout;
    ValueSsaProgramMachineAllocationView machine_view;
    ValueSsaError value_error;
    const ValueSsaProgram *lowering_source = NULL;
    const ValueSsaProgram *layout_context_source = NULL;
    int ok = 0;
    int trace_timing = machine_ir_trace_enabled();
    double total_start = trace_timing ? machine_ir_now_s() : 0.0;
    double stage_start = total_start;

    if (!program || !bank || !out_report) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-407: invalid allocate+rewrite machine-ir report contract");
        return 0;
    }

    machine_ir_allocate_rewrite_report_free(out_report);
    value_ssa_program_init(&rewritten_program);
    value_ssa_program_allocation_result_init(&result);
    value_ssa_allocate_rewrite_stats_init(&stats);
    value_ssa_program_allocation_layout_init(&layout);
    value_ssa_program_machine_allocation_view_init(&machine_view);
    memset(&value_error, 0, sizeof(value_error));

    if (machine_ir_value_ssa_program_needs_all_spill_lowering(program)) {
        if (!machine_ir_build_all_spill_allocation_result(program, &result, error)) {
            goto cleanup;
        }
        if (trace_timing) {
            machine_ir_trace_timing("report all-spill-result", machine_ir_now_s() - stage_start);
            stage_start = machine_ir_now_s();
        }
        stats.allocation_rounds = 0;
        stats.rewrite_rounds = 0;
        lowering_source = program;
        layout_context_source = program;
    } else if (machine_ir_value_ssa_program_needs_conservative_lowering(program)) {
        if (!value_ssa_allocate_program(program, color_budget, &result, &value_error)) {
            machine_ir_set_error_from_value_ssa(error, &value_error);
            goto cleanup;
        }
        if (trace_timing) {
            machine_ir_trace_timing("report conservative-allocate", machine_ir_now_s() - stage_start);
            stage_start = machine_ir_now_s();
        }
        stats.allocation_rounds = 1;
        stats.rewrite_rounds = 0;
        lowering_source = program;
        layout_context_source = program;
    } else {
        if (!machine_ir_clone_value_ssa_program(program, &rewritten_program, error)) {
            goto cleanup;
        }
        if (trace_timing) {
            machine_ir_trace_timing("report clone-value-ssa", machine_ir_now_s() - stage_start);
            stage_start = machine_ir_now_s();
        }
        if (!value_ssa_allocate_and_rewrite_program_single_block_spills_with_stats(
                &rewritten_program, color_budget, &result, &stats, &value_error)) {
            machine_ir_set_error_from_value_ssa(error, &value_error);
            goto cleanup;
        }
        if (trace_timing) {
            machine_ir_trace_timing("report allocate+rewrite", machine_ir_now_s() - stage_start);
            stage_start = machine_ir_now_s();
        }
        lowering_source = &rewritten_program;
        layout_context_source = &rewritten_program;
    }

    if (!value_ssa_build_program_allocation_layout(&result, &layout, &value_error)) {
        machine_ir_set_error_from_value_ssa(error, &value_error);
        goto cleanup;
    }
    if (trace_timing) {
        machine_ir_trace_timing("report build-layout", machine_ir_now_s() - stage_start);
        stage_start = machine_ir_now_s();
    }
    if (!value_ssa_program_allocation_layout_attach_program_context(
            &layout,
            layout_context_source ? layout_context_source : lowering_source,
            &value_error)) {
        machine_ir_set_error_from_value_ssa(error, &value_error);
        goto cleanup;
    }
    if (trace_timing) {
        machine_ir_trace_timing("report attach-layout-context", machine_ir_now_s() - stage_start);
        stage_start = machine_ir_now_s();
    }
    if (!value_ssa_build_program_machine_allocation_view(&layout, bank, &machine_view, &value_error)) {
        machine_ir_set_error_from_value_ssa(error, &value_error);
        goto cleanup;
    }
    if (trace_timing) {
        machine_ir_trace_timing("report build-machine-view", machine_ir_now_s() - stage_start);
        stage_start = machine_ir_now_s();
    }

    out_report->allocation_rounds = stats.allocation_rounds;
    out_report->rewrite_rounds = stats.rewrite_rounds;
    ok = machine_ir_lower_program_from_value_ssa(
        lowering_source ? lowering_source : &rewritten_program, &machine_view, bank, &out_report->program, error);
    if (trace_timing) {
        machine_ir_trace_timing("report lower-machine-ir", machine_ir_now_s() - stage_start);
        stage_start = machine_ir_now_s();
    }
    if (!ok) {
        goto cleanup;
    }

    if (!machine_ir_allocate_rewrite_report_refresh_shape(out_report, error)) {
        ok = 0;
        goto cleanup;
    }
    if (trace_timing) {
        machine_ir_trace_timing("report refresh-shape", machine_ir_now_s() - stage_start);
        machine_ir_trace_timing("report total", machine_ir_now_s() - total_start);
    }

cleanup:
    if (!ok) {
        machine_ir_allocate_rewrite_report_free(out_report);
    }
    value_ssa_program_machine_allocation_view_free(&machine_view);
    value_ssa_program_allocation_layout_free(&layout);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&rewritten_program);
    return ok;
}

int machine_ir_build_allocate_and_rewrite_program_single_block_spills_flat_report(
    const ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    MachineIrAllocateRewriteReport *out_report,
    MachineIrError *error) {
    ValueSsaMachineRegisterBank bank;
    ValueSsaError value_error;
    int ok = 0;

    if (!program || !out_report) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-408: invalid flat allocate+rewrite machine-ir report contract");
        return 0;
    }

    value_ssa_machine_register_bank_init(&bank);
    memset(&value_error, 0, sizeof(value_error));
    if (!value_ssa_build_flat_machine_register_bank(machine_register_count, &bank, &value_error)) {
        machine_ir_set_error_from_value_ssa(error, &value_error);
        return 0;
    }
    ok = machine_ir_build_allocate_and_rewrite_program_single_block_spills_report(
        program, color_budget, &bank, out_report, error);
    value_ssa_machine_register_bank_free(&bank);
    return ok;
}

int machine_ir_build_allocate_and_rewrite_program_single_block_spills_flat_program_only_report(
    const ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    MachineIrAllocateRewriteReport *out_report,
    MachineIrError *error) {
    ValueSsaMachineRegisterBank bank;
    ValueSsaError value_error;
    int ok = 0;

    if (!program || !out_report) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-531: invalid flat program-only machine-ir report contract");
        return 0;
    }

    machine_ir_allocate_rewrite_report_free(out_report);
    machine_ir_allocate_rewrite_report_init(out_report);
    value_ssa_machine_register_bank_init(&bank);
    memset(&value_error, 0, sizeof(value_error));

    if (!value_ssa_build_flat_machine_register_bank(machine_register_count, &bank, &value_error)) {
        machine_ir_set_error_from_value_ssa(error, &value_error);
        goto cleanup;
    }

    ok = machine_ir_allocate_and_rewrite_program_single_block_spills(
        program, color_budget, &bank, &out_report->program, error);
    if (ok) {
        out_report->allocation_rounds = 0u;
        out_report->rewrite_rounds = 0u;
    } else {
        machine_ir_allocate_rewrite_report_free(out_report);
    }

cleanup:
    value_ssa_machine_register_bank_free(&bank);
    return ok;
}

int machine_ir_build_allocate_and_rewrite_program_single_block_spills_report_dump(
    const ValueSsaProgram *program,
    size_t color_budget,
    const ValueSsaMachineRegisterBank *bank,
    char **out_text,
    MachineIrError *error) {
    MachineIrAllocateRewriteReport report;
    int ok;

    if (out_text) {
        *out_text = NULL;
    }
    if (!program || !bank || !out_text) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-409: invalid allocate+rewrite machine-ir report dump contract");
        return 0;
    }

    machine_ir_allocate_rewrite_report_init(&report);
    ok = machine_ir_build_allocate_and_rewrite_program_single_block_spills_report(
             program, color_budget, bank, &report, error) &&
        machine_ir_dump_allocate_rewrite_report(&report, out_text, error);
    machine_ir_allocate_rewrite_report_free(&report);
    return ok;
}

int machine_ir_build_allocate_and_rewrite_program_single_block_spills_flat_report_dump(
    const ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    char **out_text,
    MachineIrError *error) {
    MachineIrAllocateRewriteReport report;
    int ok;

    if (out_text) {
        *out_text = NULL;
    }
    if (!program || !out_text) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-410: invalid flat allocate+rewrite machine-ir report dump contract");
        return 0;
    }

    machine_ir_allocate_rewrite_report_init(&report);
    ok = machine_ir_build_allocate_and_rewrite_program_single_block_spills_flat_report(
             program, color_budget, machine_register_count, &report, error) &&
        machine_ir_dump_allocate_rewrite_report(&report, out_text, error);
    machine_ir_allocate_rewrite_report_free(&report);
    return ok;
}

int machine_ir_build_allocate_and_rewrite_program_single_block_spills_phi_eliminated_flat_report(
    const ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    MachineIrAllocateRewriteReport *out_report,
    MachineIrError *error) {
    if (!machine_ir_build_allocate_and_rewrite_program_single_block_spills_flat_report(
            program, color_budget, machine_register_count, out_report, error)) {
        return 0;
    }
    if (!machine_ir_eliminate_phi_nodes_in_report(out_report, error)) {
        machine_ir_allocate_rewrite_report_free(out_report);
        return 0;
    }
    return 1;
}

int machine_ir_build_allocate_and_rewrite_program_single_block_spills_phi_eliminated_flat_report_dump(
    const ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    char **out_text,
    MachineIrError *error) {
    MachineIrAllocateRewriteReport report;
    int ok;

    if (out_text) {
        *out_text = NULL;
    }
    if (!program || !out_text) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-413: invalid phi-eliminated machine-ir report dump contract");
        return 0;
    }

    machine_ir_allocate_rewrite_report_init(&report);
    ok = machine_ir_build_allocate_and_rewrite_program_single_block_spills_phi_eliminated_flat_report(
             program, color_budget, machine_register_count, &report, error) &&
        machine_ir_dump_allocate_rewrite_report(&report, out_text, error);
    machine_ir_allocate_rewrite_report_free(&report);
    return ok;
}

int machine_ir_build_allocate_and_rewrite_program_single_block_spills_cleaned_flat_report(
    const ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    MachineIrAllocateRewriteReport *out_report,
    MachineIrError *error) {
    if (!machine_ir_build_allocate_and_rewrite_program_single_block_spills_flat_report(
            program, color_budget, machine_register_count, out_report, error)) {
        return 0;
    }
    if (!machine_ir_canonicalize_program_in_report(out_report, error)) {
        machine_ir_allocate_rewrite_report_free(out_report);
        return 0;
    }
    return 1;
}

int machine_ir_build_allocate_and_rewrite_program_single_block_spills_cleaned_flat_report_dump(
    const ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    char **out_text,
    MachineIrError *error) {
    MachineIrAllocateRewriteReport report;
    int ok;

    if (out_text) {
        *out_text = NULL;
    }
    if (!program || !out_text) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-519: invalid cleaned machine-ir report dump contract");
        return 0;
    }

    machine_ir_allocate_rewrite_report_init(&report);
    ok = machine_ir_build_allocate_and_rewrite_program_single_block_spills_cleaned_flat_report(
             program, color_budget, machine_register_count, &report, error) &&
        machine_ir_dump_allocate_rewrite_report(&report, out_text, error);
    machine_ir_allocate_rewrite_report_free(&report);
    return ok;
}

int machine_ir_build_allocate_and_rewrite_program_single_block_spills_canonicalized_flat_report(
    const ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    MachineIrAllocateRewriteReport *out_report,
    MachineIrError *error) {
    return machine_ir_build_allocate_and_rewrite_program_single_block_spills_cleaned_flat_report(
        program, color_budget, machine_register_count, out_report, error);
}

int machine_ir_build_allocate_and_rewrite_program_single_block_spills_canonicalized_flat_report_dump(
    const ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    char **out_text,
    MachineIrError *error) {
    MachineIrAllocateRewriteReport report;
    int ok;

    if (out_text) {
        *out_text = NULL;
    }
    if (!program || !out_text) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-530: invalid canonicalized machine-ir report dump contract");
        return 0;
    }

    machine_ir_allocate_rewrite_report_init(&report);
    ok = machine_ir_build_allocate_and_rewrite_program_single_block_spills_canonicalized_flat_report(
             program, color_budget, machine_register_count, &report, error) &&
        machine_ir_dump_allocate_rewrite_report(&report, out_text, error);
    machine_ir_allocate_rewrite_report_free(&report);
    return ok;
}

#endif
