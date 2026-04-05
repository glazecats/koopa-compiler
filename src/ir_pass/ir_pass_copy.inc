#if defined(__INTELLISENSE__) && !defined(IR_PASS_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(IR_PASS_SPLIT_AGGREGATOR)
#define IR_PASS_SPLIT_INTELLISENSE_CONTEXT
#include "ir_pass.c"
#else

#ifndef IR_PASS_SPLIT_AGGREGATOR
#include "ir_pass.h"

#include <stdlib.h>
#endif

static IrValueRef *ir_pass_get_temp_use_site_value_mut(IrFunction *function,
    const IrPassTempUseSite *use_site) {
    size_t block_index;

    if (!function || !use_site || !function->blocks) {
        return NULL;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        IrBasicBlock *block = &function->blocks[block_index];

        if (block->id != use_site->block_id) {
            continue;
        }

        switch (use_site->kind) {
        case IR_PASS_TEMP_USE_MOV_VALUE:
            if (use_site->instruction_index >= block->instruction_count
                || block->instructions[use_site->instruction_index].kind != IR_INSTR_MOV) {
                return NULL;
            }
            return &block->instructions[use_site->instruction_index].as.mov_value;
        case IR_PASS_TEMP_USE_BINARY_LHS:
            if (use_site->instruction_index >= block->instruction_count
                || block->instructions[use_site->instruction_index].kind != IR_INSTR_BINARY) {
                return NULL;
            }
            return &block->instructions[use_site->instruction_index].as.binary.lhs;
        case IR_PASS_TEMP_USE_BINARY_RHS:
            if (use_site->instruction_index >= block->instruction_count
                || block->instructions[use_site->instruction_index].kind != IR_INSTR_BINARY) {
                return NULL;
            }
            return &block->instructions[use_site->instruction_index].as.binary.rhs;
        case IR_PASS_TEMP_USE_CALL_ARG:
            if (use_site->instruction_index >= block->instruction_count
                || block->instructions[use_site->instruction_index].kind != IR_INSTR_CALL
                || use_site->operand_index >= block->instructions[use_site->instruction_index].as.call.arg_count) {
                return NULL;
            }
            return &block->instructions[use_site->instruction_index].as.call.args[use_site->operand_index];
        case IR_PASS_TEMP_USE_TERM_RETURN:
            if (!block->has_terminator || block->terminator.kind != IR_TERM_RETURN) {
                return NULL;
            }
            return &block->terminator.as.return_value;
        case IR_PASS_TEMP_USE_TERM_BRANCH:
            if (!block->has_terminator || block->terminator.kind != IR_TERM_BRANCH) {
                return NULL;
            }
            return &block->terminator.as.branch.condition;
        default:
            return NULL;
        }
    }

    return NULL;
}

static int ir_pass_resolve_temp_copy_value(IrValueRef value,
    const IrPassTempAnalysis *analysis,
    size_t temp_count,
    const char *function_name,
    IrError *error,
    IrValueRef *out_value) {
    size_t depth = 0;
    IrValueRef resolved = value;

    if (!out_value) {
        return 0;
    }

    while (resolved.kind == IR_VALUE_TEMP) {
        if (!analysis || !analysis->has_temp_copy_value || !analysis->temp_copy_values || resolved.id >= temp_count) {
            ir_pass_set_error(error,
                0,
                0,
                "IR-PASS-015: function '%s' temp copy id %zu is out of range",
                function_name ? function_name : "<unnamed>",
                resolved.id);
            return 0;
        }
        if (!analysis->has_temp_copy_value[resolved.id]) {
            break;
        }

        resolved = analysis->temp_copy_values[resolved.id];
        depth += 1;
        if (depth > temp_count) {
            ir_pass_set_error(error,
                0,
                0,
                "IR-PASS-016: function '%s' temp copy chain exceeded safe resolution depth",
                function_name ? function_name : "<unnamed>");
            return 0;
        }
    }

    *out_value = resolved;
    return 1;
}

static int ir_pass_apply_temp_copy_propagation(IrFunction *function,
    const IrPassTempAnalysis *analysis,
    IrError *error) {
    size_t block_index;
    int requires_full_scan = 1;

    if (!function) {
        return 0;
    }

    if (analysis && analysis->use_counts && analysis->use_sites && analysis->has_unique_use_site
        && analysis->has_temp_copy_value && analysis->temp_copy_values) {
        size_t temp_id;

        requires_full_scan = 0;

        for (temp_id = 0; temp_id < analysis->temp_count; ++temp_id) {
            IrValueRef replacement;
            IrValueRef *use_value;

            if (!analysis->has_temp_copy_value[temp_id] || analysis->use_counts[temp_id] == 0) {
                continue;
            }
            if (!analysis->has_unique_use_site[temp_id]) {
                requires_full_scan = 1;
                continue;
            }
            if (!ir_pass_resolve_temp_copy_value(analysis->temp_copy_values[temp_id],
                    analysis,
                    function->next_temp_id,
                    function->name,
                    error,
                    &replacement)) {
                return 0;
            }

            use_value = ir_pass_get_temp_use_site_value_mut(function, &analysis->use_sites[temp_id]);
            if (!use_value) {
                ir_pass_set_error(error,
                    0,
                    0,
                    "IR-PASS-039: function '%s' single-use site for temp.%zu became invalid during copy propagation",
                    function->name ? function->name : "<unnamed>",
                    temp_id);
                return 0;
            }
            *use_value = replacement;
        }
    }

    if (!requires_full_scan) {
        return 1;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        IrBasicBlock *block = &function->blocks[block_index];
        size_t instruction_index;

        for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
            IrInstruction *instruction = &block->instructions[instruction_index];
            size_t arg_index;

            switch (instruction->kind) {
            case IR_INSTR_MOV:
                if (!ir_pass_resolve_temp_copy_value(instruction->as.mov_value,
                        analysis,
                        function->next_temp_id,
                        function->name,
                        error,
                        &instruction->as.mov_value)) {
                    return 0;
                }
                break;
            case IR_INSTR_BINARY:
                if (!ir_pass_resolve_temp_copy_value(instruction->as.binary.lhs,
                        analysis,
                        function->next_temp_id,
                        function->name,
                        error,
                        &instruction->as.binary.lhs)
                    || !ir_pass_resolve_temp_copy_value(instruction->as.binary.rhs,
                        analysis,
                        function->next_temp_id,
                        function->name,
                        error,
                        &instruction->as.binary.rhs)) {
                    return 0;
                }
                break;
            case IR_INSTR_CALL:
                for (arg_index = 0; arg_index < instruction->as.call.arg_count; ++arg_index) {
                    if (!ir_pass_resolve_temp_copy_value(instruction->as.call.args[arg_index],
                            analysis,
                            function->next_temp_id,
                            function->name,
                            error,
                            &instruction->as.call.args[arg_index])) {
                        return 0;
                    }
                }
                break;
            default:
                break;
            }
        }

        if (!block->has_terminator) {
            continue;
        }
        if (block->terminator.kind == IR_TERM_RETURN) {
            if (!ir_pass_resolve_temp_copy_value(block->terminator.as.return_value,
                    analysis,
                    function->next_temp_id,
                    function->name,
                    error,
                    &block->terminator.as.return_value)) {
                return 0;
            }
        } else if (block->terminator.kind == IR_TERM_BRANCH) {
            if (!ir_pass_resolve_temp_copy_value(block->terminator.as.branch.condition,
                    analysis,
                    function->next_temp_id,
                    function->name,
                    error,
                    &block->terminator.as.branch.condition)) {
                return 0;
            }
        }
    }

    return 1;
}

#endif

int ir_pass_propagate_temp_copies(IrProgram *program, IrError *error) {
    size_t function_index;

    if (!program) {
        ir_pass_set_error(error, 0, 0, "IR-PASS-018: null program for temp-copy propagation");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        IrFunction *function = &program->functions[function_index];
        IrPassTempAnalysis analysis;

        if (!function->has_body || function->next_temp_id == 0) {
            continue;
        }
        if (function->block_count > 0 && !function->blocks) {
            ir_pass_set_error(error,
                0,
                0,
                "IR-PASS-019: malformed function '%s' block table",
                function->name ? function->name : "<unnamed>");
            return 0;
        }

        if (!ir_pass_build_temp_analysis(function, &analysis, error)
            || !ir_pass_apply_temp_copy_propagation(function, &analysis, error)) {
            ir_pass_temp_analysis_free(&analysis);
            return 0;
        }

        ir_pass_temp_analysis_free(&analysis);
    }

    return 1;
}
