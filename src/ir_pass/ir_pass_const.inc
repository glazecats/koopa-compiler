#if defined(__INTELLISENSE__) && !defined(IR_PASS_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(IR_PASS_SPLIT_AGGREGATOR)
#define IR_PASS_SPLIT_INTELLISENSE_CONTEXT
#include "ir_pass.c"
#else

#ifndef IR_PASS_SPLIT_AGGREGATOR
#include "ir_pass.h"
#endif

static int ir_pass_apply_temp_constant_value(IrValueRef *value,
    const IrPassTempAnalysis *analysis,
    const char *function_name,
    IrError *error) {
    long long constant_value;
    unsigned char has_constant = 0u;

    if (!value) {
        return 0;
    }
    if (!ir_pass_try_resolve_constant_value(*value,
            analysis,
            function_name,
            error,
            &constant_value,
            &has_constant,
            NULL)) {
        return 0;
    }
    if (has_constant) {
        *value = ir_pass_value_immediate(constant_value);
    }

    return 1;
}

#endif

static int ir_pass_apply_temp_constant_propagation(IrFunction *function,
    const IrPassTempAnalysis *analysis,
    IrError *error) {
    size_t block_index;

    if (!function) {
        return 0;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        IrBasicBlock *block = &function->blocks[block_index];
        size_t instruction_index;

        for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
            IrInstruction *instruction = &block->instructions[instruction_index];
            size_t arg_index;

            switch (instruction->kind) {
            case IR_INSTR_MOV:
                if (!ir_pass_apply_temp_constant_value(&instruction->as.mov_value,
                        analysis,
                        function->name,
                        error)) {
                    return 0;
                }
                break;
            case IR_INSTR_BINARY:
                if (!ir_pass_apply_temp_constant_value(&instruction->as.binary.lhs,
                        analysis,
                        function->name,
                        error)
                    || !ir_pass_apply_temp_constant_value(&instruction->as.binary.rhs,
                        analysis,
                        function->name,
                        error)) {
                    return 0;
                }
                break;
            case IR_INSTR_CALL:
                for (arg_index = 0; arg_index < instruction->as.call.arg_count; ++arg_index) {
                    if (!ir_pass_apply_temp_constant_value(&instruction->as.call.args[arg_index],
                            analysis,
                            function->name,
                            error)) {
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
            if (!ir_pass_apply_temp_constant_value(&block->terminator.as.return_value,
                    analysis,
                    function->name,
                    error)) {
                return 0;
            }
        } else if (block->terminator.kind == IR_TERM_BRANCH) {
            if (!ir_pass_apply_temp_constant_value(&block->terminator.as.branch.condition,
                    analysis,
                    function->name,
                    error)) {
                return 0;
            }
        }
    }

    return 1;
}

int ir_pass_propagate_temp_constants(IrProgram *program, IrError *error) {
    size_t function_index;

    if (!program) {
        ir_pass_set_error(error, 0, 0, "IR-PASS-024: null program for temp-constant propagation");
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
                "IR-PASS-025: malformed function '%s' block table",
                function->name ? function->name : "<unnamed>");
            return 0;
        }

        if (!ir_pass_build_temp_analysis(function, &analysis, error)
            || !ir_pass_apply_temp_constant_propagation(function, &analysis, error)) {
            ir_pass_temp_analysis_free(&analysis);
            return 0;
        }

        ir_pass_temp_analysis_free(&analysis);
    }

    return 1;
}