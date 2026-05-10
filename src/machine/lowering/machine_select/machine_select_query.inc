#if defined(__INTELLISENSE__) && !defined(MACHINE_SELECT_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_SELECT_SPLIT_AGGREGATOR)
#define MACHINE_SELECT_SPLIT_INTELLISENSE_CONTEXT
#include "machine_select.c"
#else

int machine_select_program_get_summary(const MachineSelectProgram *program,
    size_t *out_register_count,
    size_t *out_global_count,
    size_t *out_function_count) {
    if (!program) {
        return 0;
    }
    if (out_register_count) {
        *out_register_count = program->register_bank.register_count;
    }
    if (out_global_count) {
        *out_global_count = program->global_count;
    }
    if (out_function_count) {
        *out_function_count = program->function_count;
    }
    return 1;
}

int machine_select_program_get_function_by_name(const MachineSelectProgram *program,
    const char *function_name,
    size_t *out_function_index,
    const MachineSelectFunction **out_function) {
    size_t function_index;

    if (out_function_index) {
        *out_function_index = 0;
    }
    if (out_function) {
        *out_function = NULL;
    }
    if (!program || !function_name) {
        return 0;
    }
    if (program->function_count > 0 && !program->functions) {
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const MachineSelectFunction *function = &program->functions[function_index];

        if (function->name && strcmp(function->name, function_name) == 0) {
            if (out_function_index) {
                *out_function_index = function_index;
            }
            if (out_function) {
                *out_function = function;
            }
            return 1;
        }
    }

    return 0;
}

int machine_select_program_get_function(const MachineSelectProgram *program,
    size_t function_index,
    const MachineSelectFunction **out_function) {
    if (!program || !out_function || function_index >= program->function_count || !program->functions) {
        return 0;
    }
    *out_function = &program->functions[function_index];
    return 1;
}

int machine_select_program_get_function_artifact(const MachineSelectProgram *program,
    size_t function_index,
    const MachineSelectFunction **out_function,
    MachineSelectFunctionSummary *out_summary) {
    const MachineSelectFunction *function = NULL;

    if (!program || !out_summary || !machine_select_program_get_function(program, function_index, &function) || !function) {
        return 0;
    }
    if (out_function) {
        *out_function = function;
    }
    return machine_select_function_compute_summary(function, out_summary);
}

int machine_select_program_get_function_summary(const MachineSelectProgram *program,
    size_t function_index,
    MachineSelectFunctionSummary *out_summary) {
    const MachineSelectFunction *function = NULL;

    if (!program || !out_summary || !machine_select_program_get_function(program, function_index, &function) || !function) {
        return 0;
    }
    return machine_select_function_compute_summary(function, out_summary);
}

int machine_select_program_get_function_artifact_by_name(const MachineSelectProgram *program,
    const char *function_name,
    size_t *out_function_index,
    const MachineSelectFunction **out_function,
    MachineSelectFunctionSummary *out_summary) {
    size_t function_index = 0;
    const MachineSelectFunction *function = NULL;

    if (out_function_index) {
        *out_function_index = 0;
    }
    if (out_function) {
        *out_function = NULL;
    }
    if (!program || !function_name || !out_summary) {
        return 0;
    }
    if (!machine_select_program_get_function_by_name(program, function_name, &function_index, &function) || !function) {
        return 0;
    }
    if (out_function_index) {
        *out_function_index = function_index;
    }
    if (out_function) {
        *out_function = function;
    }
    return machine_select_function_compute_summary(function, out_summary);
}

int machine_select_program_get_function_summary_by_name(const MachineSelectProgram *program,
    const char *function_name,
    size_t *out_function_index,
    MachineSelectFunctionSummary *out_summary) {
    size_t function_index = 0;
    const MachineSelectFunction *function = NULL;

    if (out_function_index) {
        *out_function_index = 0;
    }
    if (!program || !function_name || !out_summary) {
        return 0;
    }
    if (!machine_select_program_get_function_by_name(program, function_name, &function_index, &function) || !function) {
        return 0;
    }
    if (out_function_index) {
        *out_function_index = function_index;
    }
    return machine_select_function_compute_summary(function, out_summary);
}

int machine_select_program_get_block_by_function_name_and_block_id(const MachineSelectProgram *program,
    const char *function_name,
    size_t block_id,
    size_t *out_function_index,
    size_t *out_block_index,
    const MachineSelectFunction **out_function,
    const MachineSelectBasicBlock **out_block) {
    size_t function_index = 0;
    const MachineSelectFunction *function = NULL;
    size_t block_index;

    if (out_function_index) {
        *out_function_index = 0;
    }
    if (out_block_index) {
        *out_block_index = 0;
    }
    if (out_function) {
        *out_function = NULL;
    }
    if (out_block) {
        *out_block = NULL;
    }
    if (!program || !function_name || !out_block) {
        return 0;
    }
    if (!machine_select_program_get_function_by_name(program, function_name, &function_index, &function) || !function) {
        return 0;
    }
    if (function->block_count > 0 && !function->blocks) {
        return 0;
    }
    for (block_index = 0; block_index < function->block_count; ++block_index) {
        if (function->blocks[block_index].id == block_id) {
            if (out_function_index) {
                *out_function_index = function_index;
            }
            if (out_block_index) {
                *out_block_index = block_index;
            }
            if (out_function) {
                *out_function = function;
            }
            *out_block = &function->blocks[block_index];
            return 1;
        }
    }
    return 0;
}

int machine_select_program_get_block_artifact_by_function_name_and_block_id(const MachineSelectProgram *program,
    const char *function_name,
    size_t block_id,
    size_t *out_function_index,
    size_t *out_block_index,
    const MachineSelectFunction **out_function,
    const MachineSelectBasicBlock **out_block,
    MachineSelectBlockShapeSummary *out_block_summary,
    MachineSelectTerminatorSummary *out_terminator_summary) {
    if (out_function_index) {
        *out_function_index = 0;
    }
    if (out_block_index) {
        *out_block_index = 0;
    }
    if (out_function) {
        *out_function = NULL;
    }
    if (out_block) {
        *out_block = NULL;
    }
    if (!program || !function_name || !out_block || !out_block_summary || !out_terminator_summary) {
        return 0;
    }
    if (!machine_select_program_get_block_by_function_name_and_block_id(
            program, function_name, block_id, out_function_index, out_block_index, out_function, out_block) ||
        !*out_block) {
        return 0;
    }
    memset(out_block_summary, 0, sizeof(*out_block_summary));
    if (!machine_select_program_get_block_summary_by_function_name_and_block_id(
            program, function_name, block_id, NULL, NULL, out_block_summary) ||
        !machine_select_basic_block_get_terminator_summary(*out_block, out_terminator_summary)) {
        return 0;
    }
    return 1;
}

int machine_select_program_get_block_summary_by_function_name_and_block_id(const MachineSelectProgram *program,
    const char *function_name,
    size_t block_id,
    size_t *out_function_index,
    size_t *out_block_index,
    MachineSelectBlockShapeSummary *out_summary) {
    const MachineSelectBasicBlock *block = NULL;

    if (!program || !function_name || !out_summary) {
        return 0;
    }
    if (!machine_select_program_get_block_by_function_name_and_block_id(
            program, function_name, block_id, out_function_index, out_block_index, NULL, &block) ||
        !block) {
        return 0;
    }

    memset(out_summary, 0, sizeof(*out_summary));
    if (!machine_select_basic_block_get_summary(
            block,
            &out_summary->block_id,
            &out_summary->op_count,
            NULL,
            NULL)) {
        return 0;
    }
    if (block->has_terminator) {
        switch (block->terminator.kind) {
            case MACHINE_SELECT_TERM_RETURN:
            case MACHINE_SELECT_TERM_RETURN_IMM:
            case MACHINE_SELECT_TERM_RETURN_SPILL:
                out_summary->has_return_terminator = 1;
                break;
            case MACHINE_SELECT_TERM_JUMP:
                out_summary->has_jump_terminator = 1;
                break;
            case MACHINE_SELECT_TERM_BRANCH:
                out_summary->has_branch_terminator = 1;
                break;
            case MACHINE_SELECT_TERM_COMPARE_BRANCH:
            case MACHINE_SELECT_TERM_COMPARE_BRANCH_IMM:
                out_summary->has_compare_branch_terminator = 1;
                break;
            default:
                break;
        }
    }
    return 1;
}

int machine_select_basic_block_get_summary(const MachineSelectBasicBlock *block,
    size_t *out_block_id,
    size_t *out_op_count,
    int *out_has_terminator,
    MachineSelectTerminatorKind *out_terminator_kind) {
    if (!block) {
        return 0;
    }

    if (out_block_id) {
        *out_block_id = block->id;
    }
    if (out_op_count) {
        *out_op_count = block->op_count;
    }
    if (out_has_terminator) {
        *out_has_terminator = block->has_terminator;
    }
    if (out_terminator_kind) {
        *out_terminator_kind = block->terminator.kind;
    }
    return 1;
}

int machine_select_program_get_block_terminator_summary_by_function_name_and_block_id(const MachineSelectProgram *program,
    const char *function_name,
    size_t block_id,
    size_t *out_function_index,
    size_t *out_block_index,
    MachineSelectTerminatorSummary *out_summary) {
    const MachineSelectBasicBlock *block = NULL;

    if (!program || !function_name || !out_summary) {
        return 0;
    }
    if (!machine_select_program_get_block_by_function_name_and_block_id(
            program, function_name, block_id, out_function_index, out_block_index, NULL, &block) ||
        !block) {
        return 0;
    }
    return machine_select_basic_block_get_terminator_summary(block, out_summary);
}

int machine_select_basic_block_get_terminator_summary(const MachineSelectBasicBlock *block,
    MachineSelectTerminatorSummary *out_summary) {
    if (!block || !out_summary || !block->has_terminator) {
        return 0;
    }

    memset(out_summary, 0, sizeof(*out_summary));
    out_summary->kind = block->terminator.kind;
    switch (block->terminator.kind) {
        case MACHINE_SELECT_TERM_RETURN:
        case MACHINE_SELECT_TERM_RETURN_IMM:
        case MACHINE_SELECT_TERM_RETURN_SPILL:
            if (block->terminator.as.return_value.kind != MACHINE_SELECT_OPERAND_NONE) {
                out_summary->has_return_value = 1;
                out_summary->return_value = block->terminator.as.return_value;
            }
            break;
        case MACHINE_SELECT_TERM_JUMP:
            out_summary->has_primary_target = 1;
            out_summary->primary_target_block_id = block->terminator.as.jump_target;
            break;
        case MACHINE_SELECT_TERM_BRANCH:
            out_summary->has_primary_target = 1;
            out_summary->primary_target_block_id = block->terminator.as.branch.then_target;
            out_summary->has_secondary_target = 1;
            out_summary->secondary_target_block_id = block->terminator.as.branch.else_target;
            out_summary->has_condition = 1;
            out_summary->condition = block->terminator.as.branch.condition;
            break;
        case MACHINE_SELECT_TERM_COMPARE_BRANCH:
        case MACHINE_SELECT_TERM_COMPARE_BRANCH_IMM:
            out_summary->has_primary_target = 1;
            out_summary->primary_target_block_id = block->terminator.as.compare_branch.then_target;
            out_summary->has_secondary_target = 1;
            out_summary->secondary_target_block_id = block->terminator.as.compare_branch.else_target;
            out_summary->has_compare = 1;
            out_summary->compare_op = block->terminator.as.compare_branch.op;
            out_summary->compare_lhs = block->terminator.as.compare_branch.lhs;
            out_summary->compare_rhs = block->terminator.as.compare_branch.rhs;
            break;
        default:
            break;
    }
    return 1;
}

int machine_select_function_get_summary(const MachineSelectFunction *function,
    const char **out_name,
    int *out_has_body,
    size_t *out_parameter_count,
    size_t *out_local_count,
    size_t *out_block_count,
    size_t *out_spill_slot_count) {
    if (!function) {
        return 0;
    }
    if (out_name) {
        *out_name = function->name;
    }
    if (out_has_body) {
        *out_has_body = function->has_body;
    }
    if (out_parameter_count) {
        *out_parameter_count = function->parameter_count;
    }
    if (out_local_count) {
        *out_local_count = function->local_count;
    }
    if (out_block_count) {
        *out_block_count = function->block_count;
    }
    if (out_spill_slot_count) {
        *out_spill_slot_count = function->spill_slot_count;
    }
    return 1;
}

int machine_select_function_get_block(const MachineSelectFunction *function,
    size_t block_id,
    const MachineSelectBasicBlock **out_block) {
    size_t block_index;

    if (out_block) {
        *out_block = NULL;
    }
    if (!function || !out_block) {
        return 0;
    }
    if (function->block_count > 0 && !function->blocks) {
        return 0;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        if (function->blocks[block_index].id == block_id) {
            *out_block = &function->blocks[block_index];
            return 1;
        }
    }

    return 0;
}

int machine_select_function_compute_summary(const MachineSelectFunction *function,
    MachineSelectFunctionSummary *out_summary) {
    size_t block_index;

    if (!function || !out_summary) {
        return 0;
    }
    if (function->block_count > 0 && !function->blocks) {
        return 0;
    }

    memset(out_summary, 0, sizeof(*out_summary));
    out_summary->block_count = function->block_count;
    out_summary->has_spills = function->spill_slot_count > 0 ? 1 : 0;

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        const MachineSelectBasicBlock *block = &function->blocks[block_index];
        size_t op_index;
        size_t block_call_count = 0;
        size_t block_memory_op_count = 0;

        if (block->op_count > 0 && !block->ops) {
            return 0;
        }
        out_summary->op_count += block->op_count;
        for (op_index = 0; op_index < block->op_count; ++op_index) {
            const MachineSelectOp *op = &block->ops[op_index];

            switch (op->kind) {
                case MACHINE_SELECT_OP_CALL:
                    ++out_summary->call_count;
                    ++block_call_count;
                    break;
                case MACHINE_SELECT_OP_CALL_IMM:
                    ++out_summary->call_imm_count;
                    ++block_call_count;
                    break;
                case MACHINE_SELECT_OP_CALL_SPILL:
                    ++out_summary->call_spill_count;
                    ++block_call_count;
                    break;
                case MACHINE_SELECT_OP_CALL_IMM_SPILL:
                    ++out_summary->call_imm_spill_count;
                    ++block_call_count;
                    break;
                case MACHINE_SELECT_OP_CALL_VOID:
                    ++out_summary->call_void_count;
                    ++block_call_count;
                    break;
                case MACHINE_SELECT_OP_CALL_VOID_IMM:
                    ++out_summary->call_void_imm_count;
                    ++block_call_count;
                    break;
                case MACHINE_SELECT_OP_MATERIALIZE_IMM:
                    ++out_summary->materialize_imm_count;
                    break;
                case MACHINE_SELECT_OP_ALU:
                    ++out_summary->alu_count;
                    break;
                case MACHINE_SELECT_OP_ALU_IMM:
                    ++out_summary->alu_imm_count;
                    break;
                case MACHINE_SELECT_OP_CMP:
                    ++out_summary->cmp_count;
                    break;
                case MACHINE_SELECT_OP_CMP_IMM:
                    ++out_summary->cmp_imm_count;
                    break;
                case MACHINE_SELECT_OP_LOAD_LOCAL:
                    ++out_summary->load_local_count;
                    ++block_memory_op_count;
                    break;
                case MACHINE_SELECT_OP_STORE_LOCAL:
                    ++out_summary->store_local_count;
                    ++block_memory_op_count;
                    break;
                case MACHINE_SELECT_OP_STORE_LOCAL_IMM:
                    ++out_summary->store_local_imm_count;
                    ++block_memory_op_count;
                    break;
                case MACHINE_SELECT_OP_LOAD_GLOBAL:
                    ++out_summary->load_global_count;
                    ++block_memory_op_count;
                    break;
                case MACHINE_SELECT_OP_STORE_GLOBAL:
                    ++out_summary->store_global_count;
                    ++block_memory_op_count;
                    break;
                case MACHINE_SELECT_OP_STORE_GLOBAL_IMM:
                    ++out_summary->store_global_imm_count;
                    ++block_memory_op_count;
                    break;
                default:
                    break;
            }
        }
        if (block_call_count > 0) {
            ++out_summary->blocks_with_calls_count;
        }
        if (block_memory_op_count > 0) {
            ++out_summary->blocks_with_memory_ops_count;
        }
        switch (block->terminator.kind) {
            case MACHINE_SELECT_TERM_RETURN:
            case MACHINE_SELECT_TERM_RETURN_IMM:
            case MACHINE_SELECT_TERM_RETURN_SPILL:
                ++out_summary->return_block_count;
                break;
            case MACHINE_SELECT_TERM_JUMP:
                ++out_summary->jump_block_count;
                break;
            case MACHINE_SELECT_TERM_BRANCH:
                ++out_summary->branch_block_count;
                break;
            case MACHINE_SELECT_TERM_COMPARE_BRANCH:
            case MACHINE_SELECT_TERM_COMPARE_BRANCH_IMM:
                ++out_summary->branch_block_count;
                break;
            default:
                break;
        }
        switch (block->terminator.kind) {
            case MACHINE_SELECT_TERM_RETURN:
                ++out_summary->return_count;
                break;
            case MACHINE_SELECT_TERM_RETURN_IMM:
                ++out_summary->return_imm_count;
                break;
            case MACHINE_SELECT_TERM_RETURN_SPILL:
                ++out_summary->return_spill_count;
                break;
            case MACHINE_SELECT_TERM_BRANCH:
                ++out_summary->plain_branch_count;
                break;
            case MACHINE_SELECT_TERM_COMPARE_BRANCH:
                ++out_summary->compare_branch_count;
                break;
            case MACHINE_SELECT_TERM_COMPARE_BRANCH_IMM:
                ++out_summary->compare_branch_imm_count;
                break;
            default:
                break;
        }
    }

    return 1;
}

int machine_select_lower_report_get_summary(const MachineSelectLowerReport *report,
    size_t *out_register_count,
    size_t *out_global_count,
    size_t *out_function_count,
    size_t *out_total_block_summary_count,
    size_t *out_functions_with_calls_count,
    size_t *out_functions_with_spills_count,
    size_t *out_functions_with_memory_ops_count,
    size_t *out_functions_with_branches_count) {
    if (!report) {
        return 0;
    }

    if (out_functions_with_calls_count) {
        *out_functions_with_calls_count = report->functions_with_calls_count;
    }
    if (out_functions_with_spills_count) {
        *out_functions_with_spills_count = report->functions_with_spills_count;
    }
    if (out_functions_with_memory_ops_count) {
        *out_functions_with_memory_ops_count = report->functions_with_memory_ops_count;
    }
    if (out_functions_with_branches_count) {
        *out_functions_with_branches_count = report->functions_with_branches_count;
    }
    if (out_total_block_summary_count) {
        *out_total_block_summary_count = report->total_block_summary_count;
    }

    return machine_select_program_get_summary(
        &report->program, out_register_count, out_global_count, out_function_count);
}

int machine_select_lower_report_get_target_policy_summary_artifact(const MachineSelectLowerReport *report,
    const MachineSelectTargetPolicySummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->target_policy_summary;
    return 1;
}

int machine_select_lower_report_verify_current_riscv32_preview_compatibility(const MachineSelectLowerReport *report,
    MachineSelectError *error) {
    if (!report) {
        machine_select_set_error(
            error, 0, 0, "MACHINE-SELECT-421: invalid lower-report compatibility contract");
        return 0;
    }
    return machine_select_verify_current_riscv32_preview_compatibility(&report->program, error);
}

int machine_select_lower_report_verify_current_riscv32_preview_bytes_compatibility(const MachineSelectLowerReport *report,
    MachineSelectError *error) {
    if (!report) {
        machine_select_set_error(
            error, 0, 0, "MACHINE-SELECT-424: invalid lower-report bytes-compatibility contract");
        return 0;
    }
    return machine_select_verify_current_riscv32_preview_bytes_compatibility(&report->program, error);
}

int machine_select_lower_report_get_program(const MachineSelectLowerReport *report,
    const MachineSelectProgram **out_program) {
    if (!report || !out_program) {
        return 0;
    }

    *out_program = &report->program;
    return 1;
}

int machine_select_lower_report_get_function(const MachineSelectLowerReport *report,
    size_t function_index,
    const MachineSelectFunction **out_function) {
    if (!report) {
        return 0;
    }

    return machine_select_program_get_function(&report->program, function_index, out_function);
}

int machine_select_lower_report_get_function_artifact(const MachineSelectLowerReport *report,
    size_t function_index,
    const MachineSelectFunction **out_function,
    const MachineSelectFunctionSummary **out_summary) {
    if (out_function) {
        *out_function = NULL;
    }
    if (out_summary) {
        *out_summary = NULL;
    }
    if (!report || !out_summary ||
        !machine_select_lower_report_get_function(report, function_index, out_function) ||
        !machine_select_lower_report_get_function_summary_artifact(report, function_index, out_summary)) {
        return 0;
    }
    return 1;
}

int machine_select_lower_report_get_function_by_name(const MachineSelectLowerReport *report,
    const char *function_name,
    size_t *out_function_index,
    const MachineSelectFunction **out_function) {
    if (out_function_index) {
        *out_function_index = 0;
    }
    if (out_function) {
        *out_function = NULL;
    }
    if (!report || !function_name) {
        return 0;
    }
    return machine_select_program_get_function_by_name(&report->program, function_name, out_function_index, out_function);
}

int machine_select_lower_report_get_function_artifact_by_name(const MachineSelectLowerReport *report,
    const char *function_name,
    size_t *out_function_index,
    const MachineSelectFunction **out_function,
    const MachineSelectFunctionSummary **out_summary) {
    size_t function_index = 0;

    if (out_function_index) {
        *out_function_index = 0;
    }
    if (out_function) {
        *out_function = NULL;
    }
    if (out_summary) {
        *out_summary = NULL;
    }
    if (!report || !function_name || !out_summary) {
        return 0;
    }
    if (!machine_select_lower_report_get_function_by_name(report, function_name, &function_index, out_function) ||
        !machine_select_lower_report_get_function_summary_artifact(report, function_index, out_summary)) {
        return 0;
    }
    if (out_function_index) {
        *out_function_index = function_index;
    }
    return 1;
}

int machine_select_lower_report_get_function_summary_artifact(const MachineSelectLowerReport *report,
    size_t function_index,
    const MachineSelectFunctionSummary **out_summary) {
    if (!report || !out_summary || function_index >= report->function_summary_count || !report->function_summaries) {
        return 0;
    }

    *out_summary = &report->function_summaries[function_index];
    return 1;
}

int machine_select_lower_report_get_function_summary_artifact_by_name(const MachineSelectLowerReport *report,
    const char *function_name,
    size_t *out_function_index,
    const MachineSelectFunctionSummary **out_summary) {
    size_t function_index = 0;
    const MachineSelectFunction *function = NULL;

    if (out_function_index) {
        *out_function_index = 0;
    }
    if (out_summary) {
        *out_summary = NULL;
    }
    if (!report || !function_name || !out_summary) {
        return 0;
    }
    if (!machine_select_lower_report_get_function_by_name(report, function_name, &function_index, &function) || !function) {
        return 0;
    }
    if (function_index >= report->function_summary_count || !report->function_summaries) {
        return 0;
    }

    if (out_function_index) {
        *out_function_index = function_index;
    }
    *out_summary = &report->function_summaries[function_index];
    return 1;
}

int machine_select_lower_report_get_block_summary_artifact(const MachineSelectLowerReport *report,
    size_t function_index,
    size_t block_index,
    const MachineSelectBlockShapeSummary **out_summary) {
    size_t offset;
    size_t next_offset;

    if (!report || !out_summary || function_index >= report->program.function_count ||
        function_index >= report->function_summary_count ||
        !report->function_block_summary_offsets || !report->block_summaries) {
        return 0;
    }

    offset = report->function_block_summary_offsets[function_index];
    next_offset = function_index + 1 < report->program.function_count
        ? report->function_block_summary_offsets[function_index + 1]
        : report->total_block_summary_count;
    if (block_index >= next_offset - offset) {
        return 0;
    }

    *out_summary = &report->block_summaries[offset + block_index];
    return 1;
}

int machine_select_lower_report_get_block_summary_artifact_by_id(const MachineSelectLowerReport *report,
    size_t function_index,
    size_t block_id,
    size_t *out_block_index,
    const MachineSelectBlockShapeSummary **out_summary) {
    size_t offset;
    size_t next_offset;
    size_t block_index;

    if (out_block_index) {
        *out_block_index = 0;
    }
    if (out_summary) {
        *out_summary = NULL;
    }
    if (!report || !out_summary || function_index >= report->program.function_count ||
        function_index >= report->function_summary_count ||
        !report->function_block_summary_offsets || !report->block_summaries) {
        return 0;
    }

    offset = report->function_block_summary_offsets[function_index];
    next_offset = function_index + 1 < report->program.function_count
        ? report->function_block_summary_offsets[function_index + 1]
        : report->total_block_summary_count;
    for (block_index = 0; offset + block_index < next_offset; ++block_index) {
        const MachineSelectBlockShapeSummary *summary = &report->block_summaries[offset + block_index];
        if (summary->block_id == block_id) {
            if (out_block_index) {
                *out_block_index = block_index;
            }
            *out_summary = summary;
            return 1;
        }
    }

    return 0;
}

int machine_select_lower_report_get_block_summary_artifact_by_function_name_and_block_id(
    const MachineSelectLowerReport *report,
    const char *function_name,
    size_t block_id,
    size_t *out_function_index,
    size_t *out_block_index,
    const MachineSelectBlockShapeSummary **out_summary) {
    size_t function_index = 0;

    if (out_function_index) {
        *out_function_index = 0;
    }
    if (!report || !function_name || !out_summary) {
        return 0;
    }
    if (!machine_select_lower_report_get_function_by_name(report, function_name, &function_index, NULL)) {
        return 0;
    }
    if (!machine_select_lower_report_get_block_summary_artifact_by_id(
            report, function_index, block_id, out_block_index, out_summary)) {
        return 0;
    }
    if (out_function_index) {
        *out_function_index = function_index;
    }
    return 1;
}

int machine_select_lower_report_get_block_by_function_name_and_block_id(
    const MachineSelectLowerReport *report,
    const char *function_name,
    size_t block_id,
    size_t *out_function_index,
    size_t *out_block_index,
    const MachineSelectFunction **out_function,
    const MachineSelectBasicBlock **out_block) {
    if (out_function_index) {
        *out_function_index = 0;
    }
    if (out_block_index) {
        *out_block_index = 0;
    }
    if (out_function) {
        *out_function = NULL;
    }
    if (out_block) {
        *out_block = NULL;
    }
    if (!report || !function_name || !out_block) {
        return 0;
    }
    return machine_select_program_get_block_by_function_name_and_block_id(
        &report->program, function_name, block_id, out_function_index, out_block_index, out_function, out_block);
}

int machine_select_lower_report_get_block_artifact_by_function_name_and_block_id(
    const MachineSelectLowerReport *report,
    const char *function_name,
    size_t block_id,
    size_t *out_function_index,
    size_t *out_block_index,
    const MachineSelectFunction **out_function,
    const MachineSelectBasicBlock **out_block,
    const MachineSelectBlockShapeSummary **out_block_summary,
    MachineSelectTerminatorSummary *out_terminator_summary) {
    if (out_function_index) {
        *out_function_index = 0;
    }
    if (out_block_index) {
        *out_block_index = 0;
    }
    if (out_function) {
        *out_function = NULL;
    }
    if (out_block) {
        *out_block = NULL;
    }
    if (out_block_summary) {
        *out_block_summary = NULL;
    }
    if (!report || !function_name || !out_block || !out_block_summary || !out_terminator_summary) {
        return 0;
    }
    if (!machine_select_lower_report_get_block_by_function_name_and_block_id(
            report, function_name, block_id, out_function_index, out_block_index, out_function, out_block) ||
        !*out_block ||
        !machine_select_lower_report_get_block_summary_artifact_by_function_name_and_block_id(
            report, function_name, block_id, NULL, NULL, out_block_summary) ||
        !*out_block_summary ||
        !machine_select_basic_block_get_terminator_summary(*out_block, out_terminator_summary)) {
        return 0;
    }
    return 1;
}

static int machine_select_block_summary_matches_filter(const MachineSelectBlockShapeSummary *summary,
    MachineSelectBlockFilterKind filter_kind) {
    if (!summary) {
        return 0;
    }
    switch (filter_kind) {
        case MACHINE_SELECT_BLOCK_FILTER_CALLS:
            return summary->call_count > 0;
        case MACHINE_SELECT_BLOCK_FILTER_MEMORY_OPS:
            return summary->load_local_count + summary->store_local_count + summary->store_local_imm_count +
                    summary->load_global_count + summary->store_global_count + summary->store_global_imm_count >
                0;
        case MACHINE_SELECT_BLOCK_FILTER_RETURNS:
            return summary->has_return_terminator;
        case MACHINE_SELECT_BLOCK_FILTER_JUMPS:
            return summary->has_jump_terminator;
        case MACHINE_SELECT_BLOCK_FILTER_BRANCHES:
            return summary->has_branch_terminator;
        case MACHINE_SELECT_BLOCK_FILTER_COMPARE_BRANCHES:
            return summary->has_compare_branch_terminator;
        default:
            return 0;
    }
}

int machine_select_lower_report_get_function_block_filter_count(const MachineSelectLowerReport *report,
    size_t function_index,
    MachineSelectBlockFilterKind filter_kind,
    size_t *out_count) {
    size_t block_offset;
    size_t next_block_offset;
    size_t block_summary_index;
    size_t count = 0;

    if (out_count) {
        *out_count = 0;
    }
    if (!report || function_index >= report->program.function_count ||
        !report->function_block_summary_offsets || !report->block_summaries) {
        return 0;
    }

    block_offset = report->function_block_summary_offsets[function_index];
    next_block_offset = function_index + 1 < report->program.function_count
        ? report->function_block_summary_offsets[function_index + 1]
        : report->total_block_summary_count;
    for (block_summary_index = block_offset; block_summary_index < next_block_offset; ++block_summary_index) {
        if (machine_select_block_summary_matches_filter(&report->block_summaries[block_summary_index], filter_kind)) {
            ++count;
        }
    }
    if (out_count) {
        *out_count = count;
    }
    return 1;
}

int machine_select_lower_report_get_function_filtered_block_summary(const MachineSelectLowerReport *report,
    size_t function_index,
    MachineSelectBlockFilterKind filter_kind,
    size_t filtered_index,
    size_t *out_block_index,
    const MachineSelectBlockShapeSummary **out_summary) {
    size_t block_offset;
    size_t next_block_offset;
    size_t block_summary_index;
    size_t matched_index = 0;

    if (out_block_index) {
        *out_block_index = 0;
    }
    if (out_summary) {
        *out_summary = NULL;
    }
    if (!report || !out_summary || function_index >= report->program.function_count ||
        !report->function_block_summary_offsets || !report->block_summaries) {
        return 0;
    }

    block_offset = report->function_block_summary_offsets[function_index];
    next_block_offset = function_index + 1 < report->program.function_count
        ? report->function_block_summary_offsets[function_index + 1]
        : report->total_block_summary_count;
    for (block_summary_index = block_offset; block_summary_index < next_block_offset; ++block_summary_index) {
        if (!machine_select_block_summary_matches_filter(&report->block_summaries[block_summary_index], filter_kind)) {
            continue;
        }
        if (matched_index == filtered_index) {
            if (out_block_index) {
                *out_block_index = block_summary_index - block_offset;
            }
            *out_summary = &report->block_summaries[block_summary_index];
            return 1;
        }
        ++matched_index;
    }
    return 0;
}

int machine_select_lower_report_get_function_filtered_block_artifact(const MachineSelectLowerReport *report,
    size_t function_index,
    MachineSelectBlockFilterKind filter_kind,
    size_t filtered_index,
    size_t *out_block_index,
    const MachineSelectBasicBlock **out_block,
    const MachineSelectBlockShapeSummary **out_summary,
    MachineSelectTerminatorSummary *out_terminator_summary) {
    const MachineSelectFunction *function = NULL;
    size_t block_index = 0;

    if (out_block_index) {
        *out_block_index = 0;
    }
    if (out_block) {
        *out_block = NULL;
    }
    if (out_summary) {
        *out_summary = NULL;
    }
    if (!report || !out_block || !out_summary || !out_terminator_summary ||
        !machine_select_lower_report_get_function(report, function_index, &function) || !function ||
        !machine_select_lower_report_get_function_filtered_block_summary(
            report, function_index, filter_kind, filtered_index, &block_index, out_summary) ||
        !machine_select_function_get_block(function, (*out_summary)->block_id, out_block) || !*out_block ||
        !machine_select_basic_block_get_terminator_summary(*out_block, out_terminator_summary)) {
        return 0;
    }
    if (out_block_index) {
        *out_block_index = block_index;
    }
    return 1;
}

int machine_select_lower_report_get_function_filtered_block_summary_by_name(
    const MachineSelectLowerReport *report,
    const char *function_name,
    MachineSelectBlockFilterKind filter_kind,
    size_t filtered_index,
    size_t *out_function_index,
    size_t *out_block_index,
    const MachineSelectBlockShapeSummary **out_summary) {
    size_t function_index = 0;

    if (out_function_index) {
        *out_function_index = 0;
    }
    if (!report || !function_name || !out_summary) {
        return 0;
    }
    if (!machine_select_lower_report_get_function_by_name(report, function_name, &function_index, NULL) ||
        !machine_select_lower_report_get_function_filtered_block_summary(
            report, function_index, filter_kind, filtered_index, out_block_index, out_summary)) {
        return 0;
    }
    if (out_function_index) {
        *out_function_index = function_index;
    }
    return 1;
}

int machine_select_lower_report_get_function_filtered_block_artifact_by_name(
    const MachineSelectLowerReport *report,
    const char *function_name,
    MachineSelectBlockFilterKind filter_kind,
    size_t filtered_index,
    size_t *out_function_index,
    size_t *out_block_index,
    const MachineSelectBasicBlock **out_block,
    const MachineSelectBlockShapeSummary **out_summary,
    MachineSelectTerminatorSummary *out_terminator_summary) {
    size_t function_index = 0;

    if (out_function_index) {
        *out_function_index = 0;
    }
    if (!report || !function_name || !out_block || !out_summary || !out_terminator_summary) {
        return 0;
    }
    if (!machine_select_lower_report_get_function_by_name(report, function_name, &function_index, NULL) ||
        !machine_select_lower_report_get_function_filtered_block_artifact(
            report,
            function_index,
            filter_kind,
            filtered_index,
            out_block_index,
            out_block,
            out_summary,
            out_terminator_summary)) {
        return 0;
    }
    if (out_function_index) {
        *out_function_index = function_index;
    }
    return 1;
}

int machine_select_lower_report_get_block_terminator_summary(const MachineSelectLowerReport *report,
    size_t function_index,
    size_t block_index,
    MachineSelectTerminatorSummary *out_summary) {
    const MachineSelectFunction *function = NULL;

    if (!report || !out_summary || !machine_select_lower_report_get_function(report, function_index, &function) ||
        !function || block_index >= function->block_count) {
        return 0;
    }
    return machine_select_basic_block_get_terminator_summary(&function->blocks[block_index], out_summary);
}

int machine_select_lower_report_get_block_terminator_summary_by_id(const MachineSelectLowerReport *report,
    size_t function_index,
    size_t block_id,
    size_t *out_block_index,
    MachineSelectTerminatorSummary *out_summary) {
    const MachineSelectFunction *function = NULL;
    size_t block_index;

    if (out_block_index) {
        *out_block_index = 0;
    }
    if (!report || !out_summary || !machine_select_lower_report_get_function(report, function_index, &function) ||
        !function) {
        return 0;
    }
    for (block_index = 0; block_index < function->block_count; ++block_index) {
        if (function->blocks[block_index].id == block_id) {
            if (out_block_index) {
                *out_block_index = block_index;
            }
            return machine_select_basic_block_get_terminator_summary(&function->blocks[block_index], out_summary);
        }
    }
    return 0;
}

int machine_select_lower_report_get_block_terminator_summary_by_function_name_and_block_id(
    const MachineSelectLowerReport *report,
    const char *function_name,
    size_t block_id,
    size_t *out_function_index,
    size_t *out_block_index,
    MachineSelectTerminatorSummary *out_summary) {
    size_t function_index = 0;

    if (out_function_index) {
        *out_function_index = 0;
    }
    if (!report || !function_name || !out_summary) {
        return 0;
    }
    if (!machine_select_lower_report_get_function_by_name(report, function_name, &function_index, NULL)) {
        return 0;
    }
    if (!machine_select_lower_report_get_block_terminator_summary_by_id(
            report, function_index, block_id, out_block_index, out_summary)) {
        return 0;
    }
    if (out_function_index) {
        *out_function_index = function_index;
    }
    return 1;
}

int machine_select_lower_report_get_functions_with_calls(const MachineSelectLowerReport *report,
    size_t *out_count,
    const size_t **out_function_indices) {
    if (out_count) {
        *out_count = 0;
    }
    if (out_function_indices) {
        *out_function_indices = NULL;
    }
    if (!report) {
        return 0;
    }

    if (out_count) {
        *out_count = report->functions_with_calls_count;
    }
    if (out_function_indices) {
        *out_function_indices = report->function_indices_with_calls;
    }
    return 1;
}

int machine_select_lower_report_get_function_with_calls(const MachineSelectLowerReport *report,
    size_t filtered_index,
    size_t *out_function_index,
    const MachineSelectFunction **out_function,
    const MachineSelectFunctionSummary **out_summary) {
    size_t function_index;

    if (out_function_index) {
        *out_function_index = 0;
    }
    if (out_function) {
        *out_function = NULL;
    }
    if (out_summary) {
        *out_summary = NULL;
    }
    if (!report || filtered_index >= report->functions_with_calls_count || !report->function_indices_with_calls) {
        return 0;
    }
    function_index = report->function_indices_with_calls[filtered_index];
    if (!machine_select_lower_report_get_function(report, function_index, out_function) ||
        !machine_select_lower_report_get_function_summary_artifact(report, function_index, out_summary)) {
        return 0;
    }
    if (out_function_index) {
        *out_function_index = function_index;
    }
    return 1;
}

int machine_select_lower_report_get_function_with_calls_by_name(const MachineSelectLowerReport *report,
    const char *function_name,
    size_t *out_filtered_index,
    size_t *out_function_index,
    const MachineSelectFunction **out_function,
    const MachineSelectFunctionSummary **out_summary) {
    size_t filtered_index;

    if (out_filtered_index) {
        *out_filtered_index = 0;
    }
    if (!report || !function_name) {
        return 0;
    }
    for (filtered_index = 0; filtered_index < report->functions_with_calls_count; ++filtered_index) {
        size_t function_index = report->function_indices_with_calls[filtered_index];
        const MachineSelectFunction *function = NULL;
        if (!machine_select_lower_report_get_function(report, function_index, &function) || !function) {
            return 0;
        }
        if (function->name && strcmp(function->name, function_name) == 0) {
            if (out_filtered_index) {
                *out_filtered_index = filtered_index;
            }
            return machine_select_lower_report_get_function_with_calls(
                report, filtered_index, out_function_index, out_function, out_summary);
        }
    }
    return 0;
}

int machine_select_lower_report_get_functions_with_spills(const MachineSelectLowerReport *report,
    size_t *out_count,
    const size_t **out_function_indices) {
    if (out_count) {
        *out_count = 0;
    }
    if (out_function_indices) {
        *out_function_indices = NULL;
    }
    if (!report) {
        return 0;
    }

    if (out_count) {
        *out_count = report->functions_with_spills_count;
    }
    if (out_function_indices) {
        *out_function_indices = report->function_indices_with_spills;
    }
    return 1;
}

int machine_select_lower_report_get_function_with_spills(const MachineSelectLowerReport *report,
    size_t filtered_index,
    size_t *out_function_index,
    const MachineSelectFunction **out_function,
    const MachineSelectFunctionSummary **out_summary) {
    size_t function_index;

    if (out_function_index) {
        *out_function_index = 0;
    }
    if (out_function) {
        *out_function = NULL;
    }
    if (out_summary) {
        *out_summary = NULL;
    }
    if (!report || filtered_index >= report->functions_with_spills_count || !report->function_indices_with_spills) {
        return 0;
    }
    function_index = report->function_indices_with_spills[filtered_index];
    if (!machine_select_lower_report_get_function(report, function_index, out_function) ||
        !machine_select_lower_report_get_function_summary_artifact(report, function_index, out_summary)) {
        return 0;
    }
    if (out_function_index) {
        *out_function_index = function_index;
    }
    return 1;
}

int machine_select_lower_report_get_function_with_spills_by_name(const MachineSelectLowerReport *report,
    const char *function_name,
    size_t *out_filtered_index,
    size_t *out_function_index,
    const MachineSelectFunction **out_function,
    const MachineSelectFunctionSummary **out_summary) {
    size_t filtered_index;

    if (out_filtered_index) {
        *out_filtered_index = 0;
    }
    if (!report || !function_name) {
        return 0;
    }
    for (filtered_index = 0; filtered_index < report->functions_with_spills_count; ++filtered_index) {
        size_t function_index = report->function_indices_with_spills[filtered_index];
        const MachineSelectFunction *function = NULL;
        if (!machine_select_lower_report_get_function(report, function_index, &function) || !function) {
            return 0;
        }
        if (function->name && strcmp(function->name, function_name) == 0) {
            if (out_filtered_index) {
                *out_filtered_index = filtered_index;
            }
            return machine_select_lower_report_get_function_with_spills(
                report, filtered_index, out_function_index, out_function, out_summary);
        }
    }
    return 0;
}

int machine_select_lower_report_get_functions_with_memory_ops(const MachineSelectLowerReport *report,
    size_t *out_count,
    const size_t **out_function_indices) {
    if (out_count) {
        *out_count = 0;
    }
    if (out_function_indices) {
        *out_function_indices = NULL;
    }
    if (!report) {
        return 0;
    }

    if (out_count) {
        *out_count = report->functions_with_memory_ops_count;
    }
    if (out_function_indices) {
        *out_function_indices = report->function_indices_with_memory_ops;
    }
    return 1;
}

int machine_select_lower_report_get_function_with_memory_ops(const MachineSelectLowerReport *report,
    size_t filtered_index,
    size_t *out_function_index,
    const MachineSelectFunction **out_function,
    const MachineSelectFunctionSummary **out_summary) {
    size_t function_index;

    if (out_function_index) {
        *out_function_index = 0;
    }
    if (out_function) {
        *out_function = NULL;
    }
    if (out_summary) {
        *out_summary = NULL;
    }
    if (!report || filtered_index >= report->functions_with_memory_ops_count ||
        !report->function_indices_with_memory_ops) {
        return 0;
    }
    function_index = report->function_indices_with_memory_ops[filtered_index];
    if (!machine_select_lower_report_get_function(report, function_index, out_function) ||
        !machine_select_lower_report_get_function_summary_artifact(report, function_index, out_summary)) {
        return 0;
    }
    if (out_function_index) {
        *out_function_index = function_index;
    }
    return 1;
}

int machine_select_lower_report_get_function_with_memory_ops_by_name(const MachineSelectLowerReport *report,
    const char *function_name,
    size_t *out_filtered_index,
    size_t *out_function_index,
    const MachineSelectFunction **out_function,
    const MachineSelectFunctionSummary **out_summary) {
    size_t filtered_index;

    if (out_filtered_index) {
        *out_filtered_index = 0;
    }
    if (!report || !function_name) {
        return 0;
    }
    for (filtered_index = 0; filtered_index < report->functions_with_memory_ops_count; ++filtered_index) {
        size_t function_index = report->function_indices_with_memory_ops[filtered_index];
        const MachineSelectFunction *function = NULL;
        if (!machine_select_lower_report_get_function(report, function_index, &function) || !function) {
            return 0;
        }
        if (function->name && strcmp(function->name, function_name) == 0) {
            if (out_filtered_index) {
                *out_filtered_index = filtered_index;
            }
            return machine_select_lower_report_get_function_with_memory_ops(
                report, filtered_index, out_function_index, out_function, out_summary);
        }
    }
    return 0;
}

int machine_select_lower_report_get_functions_with_branches(const MachineSelectLowerReport *report,
    size_t *out_count,
    const size_t **out_function_indices) {
    if (out_count) {
        *out_count = 0;
    }
    if (out_function_indices) {
        *out_function_indices = NULL;
    }
    if (!report) {
        return 0;
    }

    if (out_count) {
        *out_count = report->functions_with_branches_count;
    }
    if (out_function_indices) {
        *out_function_indices = report->function_indices_with_branches;
    }
    return 1;
}

int machine_select_lower_report_get_function_with_branches(const MachineSelectLowerReport *report,
    size_t filtered_index,
    size_t *out_function_index,
    const MachineSelectFunction **out_function,
    const MachineSelectFunctionSummary **out_summary) {
    size_t function_index;

    if (out_function_index) {
        *out_function_index = 0;
    }
    if (out_function) {
        *out_function = NULL;
    }
    if (out_summary) {
        *out_summary = NULL;
    }
    if (!report || filtered_index >= report->functions_with_branches_count || !report->function_indices_with_branches) {
        return 0;
    }
    function_index = report->function_indices_with_branches[filtered_index];
    if (!machine_select_lower_report_get_function(report, function_index, out_function) ||
        !machine_select_lower_report_get_function_summary_artifact(report, function_index, out_summary)) {
        return 0;
    }
    if (out_function_index) {
        *out_function_index = function_index;
    }
    return 1;
}

int machine_select_lower_report_get_function_with_branches_by_name(const MachineSelectLowerReport *report,
    const char *function_name,
    size_t *out_filtered_index,
    size_t *out_function_index,
    const MachineSelectFunction **out_function,
    const MachineSelectFunctionSummary **out_summary) {
    size_t filtered_index;

    if (out_filtered_index) {
        *out_filtered_index = 0;
    }
    if (!report || !function_name) {
        return 0;
    }
    for (filtered_index = 0; filtered_index < report->functions_with_branches_count; ++filtered_index) {
        size_t function_index = report->function_indices_with_branches[filtered_index];
        const MachineSelectFunction *function = NULL;
        if (!machine_select_lower_report_get_function(report, function_index, &function) || !function) {
            return 0;
        }
        if (function->name && strcmp(function->name, function_name) == 0) {
            if (out_filtered_index) {
                *out_filtered_index = filtered_index;
            }
            return machine_select_lower_report_get_function_with_branches(
                report, filtered_index, out_function_index, out_function, out_summary);
        }
    }
    return 0;
}

#endif
