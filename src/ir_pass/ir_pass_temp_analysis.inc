#if defined(__INTELLISENSE__) && !defined(IR_PASS_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(IR_PASS_SPLIT_AGGREGATOR)
#define IR_PASS_SPLIT_INTELLISENSE_CONTEXT
#include "ir_pass.c"
#else

#ifndef IR_PASS_SPLIT_AGGREGATOR
#include "ir_pass.h"

#include <stdlib.h>
#endif

static void ir_pass_temp_analysis_free(IrPassTempAnalysis *analysis) {
    if (!analysis) {
        return;
    }

    free(analysis->use_counts);
    free(analysis->definition_counts);
    free(analysis->definition_sites);
    free(analysis->has_unique_definition_site);
    free(analysis->use_sites);
    free(analysis->has_unique_use_site);
    free(analysis->temp_copy_values);
    free(analysis->has_temp_copy_value);
    free(analysis->temp_defs);
    free(analysis->temp_constant_values);
    free(analysis->has_temp_constant_value);

    analysis->temp_count = 0;
    analysis->use_counts = NULL;
    analysis->definition_counts = NULL;
    analysis->definition_sites = NULL;
    analysis->has_unique_definition_site = NULL;
    analysis->use_sites = NULL;
    analysis->has_unique_use_site = NULL;
    analysis->temp_copy_values = NULL;
    analysis->has_temp_copy_value = NULL;
    analysis->temp_defs = NULL;
    analysis->temp_constant_values = NULL;
    analysis->has_temp_constant_value = NULL;
}

static int ir_pass_try_resolve_temp_constant(const IrPassTempAnalysis *analysis,
    size_t temp_id,
    const char *function_name,
    IrError *error,
    unsigned char *visit_state);

static int ir_pass_try_resolve_constant_value(IrValueRef value,
    const IrPassTempAnalysis *analysis,
    const char *function_name,
    IrError *error,
    long long *out_value,
    unsigned char *out_has_value,
    unsigned char *visit_state);

static int ir_pass_try_resolve_temp_constant(const IrPassTempAnalysis *analysis,
    size_t temp_id,
    const char *function_name,
    IrError *error,
    unsigned char *visit_state) {
    const IrPassTempDef *def;
    long long lhs_value;
    long long rhs_value;
    unsigned char has_lhs = 0;
    unsigned char has_rhs = 0;
    unsigned char has_mov = 0;

    if (!analysis || !analysis->temp_defs || !analysis->definition_counts || !analysis->temp_constant_values
        || !analysis->has_temp_constant_value || !visit_state || temp_id >= analysis->temp_count) {
        ir_pass_set_error(error,
            0,
            0,
            "IR-PASS-020: function '%s' temp constant id %zu is out of range",
            function_name ? function_name : "<unnamed>",
            temp_id);
        return 0;
    }

    if (analysis->has_temp_constant_value[temp_id]) {
        return 1;
    }
    if (visit_state[temp_id] == 2u) {
        return 1;
    }
    if (visit_state[temp_id] == 1u) {
        ir_pass_set_error(error,
            0,
            0,
            "IR-PASS-021: function '%s' temp.%zu constant resolution cycle detected",
            function_name ? function_name : "<unnamed>",
            temp_id);
        return 0;
    }

    visit_state[temp_id] = 1u;
    def = &analysis->temp_defs[temp_id];
    if (!def->has_definition) {
        visit_state[temp_id] = 2u;
        return 1;
    }
    if (analysis->definition_counts[temp_id] != 1) {
        visit_state[temp_id] = 2u;
        return 1;
    }

    switch (def->kind) {
    case IR_INSTR_MOV:
        if (!ir_pass_try_resolve_constant_value(def->mov_value,
                analysis,
                function_name,
                error,
                &lhs_value,
                &has_mov,
                visit_state)) {
            visit_state[temp_id] = 0u;
            return 0;
        }
        if (has_mov) {
            analysis->temp_constant_values[temp_id] = lhs_value;
            analysis->has_temp_constant_value[temp_id] = 1u;
        }
        break;
    case IR_INSTR_BINARY:
        if (!ir_pass_try_resolve_constant_value(def->binary_lhs,
                analysis,
                function_name,
                error,
                &lhs_value,
                &has_lhs,
                visit_state)
            || !ir_pass_try_resolve_constant_value(def->binary_rhs,
                analysis,
                function_name,
                error,
                &rhs_value,
                &has_rhs,
                visit_state)) {
            visit_state[temp_id] = 0u;
            return 0;
        }
        if (has_lhs && has_rhs
            && ir_pass_try_fold_binary_immediate(def->binary_op,
                lhs_value,
                rhs_value,
                &analysis->temp_constant_values[temp_id])) {
            analysis->has_temp_constant_value[temp_id] = 1u;
        }
        break;
    default:
        break;
    }

    visit_state[temp_id] = 2u;
    return 1;
}

static int ir_pass_try_resolve_constant_value(IrValueRef value,
    const IrPassTempAnalysis *analysis,
    const char *function_name,
    IrError *error,
    long long *out_value,
    unsigned char *out_has_value,
    unsigned char *visit_state) {
    if (!out_value || !out_has_value) {
        return 0;
    }

    *out_has_value = 0u;
    if (value.kind == IR_VALUE_IMMEDIATE) {
        *out_value = value.immediate;
        *out_has_value = 1u;
        return 1;
    }
    if (value.kind != IR_VALUE_TEMP) {
        return 1;
    }
    if (!analysis || value.id >= analysis->temp_count) {
        ir_pass_set_error(error,
            0,
            0,
            "IR-PASS-022: function '%s' temp constant use id %zu is out of range",
            function_name ? function_name : "<unnamed>",
            value.id);
        return 0;
    }
    if (analysis->has_temp_constant_value[value.id]) {
        *out_value = analysis->temp_constant_values[value.id];
        *out_has_value = 1u;
        return 1;
    }
    if (!visit_state) {
        return 1;
    }
    if (!ir_pass_try_resolve_temp_constant(analysis,
            value.id,
            function_name,
            error,
            visit_state)) {
        return 0;
    }
    if (!analysis->has_temp_constant_value[value.id]) {
        return 1;
    }

    *out_value = analysis->temp_constant_values[value.id];
    *out_has_value = 1u;
    return 1;
}

static int ir_pass_resolve_equivalent_value(IrValueRef value,
    const IrPassTempAnalysis *analysis,
    size_t temp_count,
    const char *function_name,
    IrError *error,
    IrValueRef *out_value) {
    unsigned char *visit_state = NULL;
    long long constant_value;
    unsigned char has_constant = 0u;

    if (!out_value) {
        return 0;
    }

    if (value.kind == IR_VALUE_TEMP && temp_count > 0) {
        visit_state = (unsigned char *)calloc(temp_count, sizeof(unsigned char));
        if (!visit_state) {
            ir_pass_set_error(error,
                0,
                0,
                "IR-PASS-038: out of memory resolving equivalent value for '%s'",
                function_name ? function_name : "<unnamed>");
            return 0;
        }
    }

    if (!ir_pass_try_resolve_constant_value(value,
            analysis,
            function_name,
            error,
            &constant_value,
            &has_constant,
            visit_state)) {
        free(visit_state);
        return 0;
    }
    free(visit_state);

    if (has_constant) {
        *out_value = ir_pass_value_immediate(constant_value);
        return 1;
    }

    return ir_pass_resolve_temp_copy_value(value,
        analysis,
        temp_count,
        function_name,
        error,
        out_value);
}

static int ir_pass_build_temp_analysis(const IrFunction *function,
    IrPassTempAnalysis *analysis,
    IrError *error) {
    size_t block_index;

    if (!function || !analysis) {
        return 0;
    }

    analysis->temp_count = 0;
    analysis->use_counts = NULL;
    analysis->definition_counts = NULL;
    analysis->definition_sites = NULL;
    analysis->has_unique_definition_site = NULL;
    analysis->use_sites = NULL;
    analysis->has_unique_use_site = NULL;
    analysis->temp_copy_values = NULL;
    analysis->has_temp_copy_value = NULL;
    analysis->temp_defs = NULL;
    analysis->temp_constant_values = NULL;
    analysis->has_temp_constant_value = NULL;

    if (function->next_temp_id == 0) {
        return 1;
    }
    if (function->next_temp_id > ((size_t)-1) / sizeof(size_t)) {
        ir_pass_set_error(error,
            0,
            0,
            "IR-PASS-010: function '%s' temp analysis capacity overflow",
            function->name ? function->name : "<unnamed>");
        return 0;
    }

    analysis->temp_count = function->next_temp_id;
    analysis->use_counts = (size_t *)calloc(analysis->temp_count, sizeof(size_t));
    analysis->definition_counts = (size_t *)calloc(analysis->temp_count, sizeof(size_t));
    analysis->definition_sites = (IrPassTempDefSite *)calloc(analysis->temp_count,
        sizeof(IrPassTempDefSite));
    analysis->has_unique_definition_site = (unsigned char *)calloc(analysis->temp_count,
        sizeof(unsigned char));
    analysis->use_sites = (IrPassTempUseSite *)calloc(analysis->temp_count,
        sizeof(IrPassTempUseSite));
    analysis->has_unique_use_site = (unsigned char *)calloc(analysis->temp_count,
        sizeof(unsigned char));
    analysis->temp_copy_values = (IrValueRef *)calloc(analysis->temp_count, sizeof(IrValueRef));
    analysis->has_temp_copy_value = (unsigned char *)calloc(analysis->temp_count, sizeof(unsigned char));
    analysis->temp_defs = (IrPassTempDef *)calloc(analysis->temp_count, sizeof(IrPassTempDef));
    analysis->temp_constant_values = (long long *)calloc(analysis->temp_count, sizeof(long long));
    analysis->has_temp_constant_value = (unsigned char *)calloc(analysis->temp_count, sizeof(unsigned char));
    if (!analysis->use_counts || !analysis->definition_counts || !analysis->definition_sites
        || !analysis->has_unique_definition_site || !analysis->use_sites
        || !analysis->has_unique_use_site || !analysis->temp_copy_values
        || !analysis->has_temp_copy_value || !analysis->temp_defs
        || !analysis->temp_constant_values || !analysis->has_temp_constant_value) {
        ir_pass_temp_analysis_free(analysis);
        ir_pass_set_error(error,
            0,
            0,
            "IR-PASS-011: out of memory building temp analysis for '%s'",
            function->name ? function->name : "<unnamed>");
        return 0;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        const IrBasicBlock *block = &function->blocks[block_index];
        size_t instruction_index;

        for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
            const IrInstruction *instruction = &block->instructions[instruction_index];
            size_t arg_index;

            if (instruction->result.kind == IR_VALUE_TEMP) {
                if (instruction->result.id >= analysis->temp_count) {
                    ir_pass_temp_analysis_free(analysis);
                    ir_pass_set_error(error,
                        0,
                        0,
                        "IR-PASS-023: function '%s' temp def id %zu is out of range",
                        function->name ? function->name : "<unnamed>",
                        instruction->result.id);
                    return 0;
                }

                analysis->definition_counts[instruction->result.id] += 1u;
                if (analysis->temp_defs[instruction->result.id].has_definition) {
                    analysis->has_unique_definition_site[instruction->result.id] = 0u;
                    analysis->has_temp_copy_value[instruction->result.id] = 0u;
                } else {
                    analysis->temp_defs[instruction->result.id].kind = instruction->kind;
                    analysis->temp_defs[instruction->result.id].has_definition = 1u;
                    analysis->definition_sites[instruction->result.id].block_id = block->id;
                    analysis->definition_sites[instruction->result.id].instruction_index = instruction_index;
                    analysis->has_unique_definition_site[instruction->result.id] = 1u;
                    if (instruction->kind == IR_INSTR_MOV) {
                        analysis->temp_defs[instruction->result.id].mov_value = instruction->as.mov_value;
                        analysis->temp_copy_values[instruction->result.id] = instruction->as.mov_value;
                        analysis->has_temp_copy_value[instruction->result.id] = 1u;
                    } else if (instruction->kind == IR_INSTR_BINARY) {
                        analysis->temp_defs[instruction->result.id].binary_op = instruction->as.binary.op;
                        analysis->temp_defs[instruction->result.id].binary_lhs = instruction->as.binary.lhs;
                        analysis->temp_defs[instruction->result.id].binary_rhs = instruction->as.binary.rhs;
                    }
                }
            }

            switch (instruction->kind) {
            case IR_INSTR_MOV:
                if (!ir_pass_note_temp_use(instruction->as.mov_value,
                        analysis->use_counts,
                        analysis->use_sites,
                        analysis->has_unique_use_site,
                        analysis->temp_count,
                        block->id,
                        instruction_index,
                        IR_PASS_TEMP_USE_MOV_VALUE,
                        0,
                        function->name,
                        error)) {
                    ir_pass_temp_analysis_free(analysis);
                    return 0;
                }
                break;
            case IR_INSTR_BINARY:
                if (!ir_pass_note_temp_use(instruction->as.binary.lhs,
                        analysis->use_counts,
                        analysis->use_sites,
                        analysis->has_unique_use_site,
                        analysis->temp_count,
                        block->id,
                        instruction_index,
                        IR_PASS_TEMP_USE_BINARY_LHS,
                        0,
                        function->name,
                        error)
                    || !ir_pass_note_temp_use(instruction->as.binary.rhs,
                        analysis->use_counts,
                        analysis->use_sites,
                        analysis->has_unique_use_site,
                        analysis->temp_count,
                        block->id,
                        instruction_index,
                        IR_PASS_TEMP_USE_BINARY_RHS,
                        0,
                        function->name,
                        error)) {
                    ir_pass_temp_analysis_free(analysis);
                    return 0;
                }
                break;
            case IR_INSTR_CALL:
                for (arg_index = 0; arg_index < instruction->as.call.arg_count; ++arg_index) {
                    if (!ir_pass_note_temp_use(instruction->as.call.args[arg_index],
                            analysis->use_counts,
                            analysis->use_sites,
                            analysis->has_unique_use_site,
                            analysis->temp_count,
                            block->id,
                            instruction_index,
                            IR_PASS_TEMP_USE_CALL_ARG,
                            arg_index,
                            function->name,
                            error)) {
                        ir_pass_temp_analysis_free(analysis);
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

        switch (block->terminator.kind) {
        case IR_TERM_RETURN:
            if (!ir_pass_note_temp_use(block->terminator.as.return_value,
                    analysis->use_counts,
                    analysis->use_sites,
                    analysis->has_unique_use_site,
                    analysis->temp_count,
                    block->id,
                    0,
                    IR_PASS_TEMP_USE_TERM_RETURN,
                    0,
                    function->name,
                    error)) {
                ir_pass_temp_analysis_free(analysis);
                return 0;
            }
            break;
        case IR_TERM_BRANCH:
            if (!ir_pass_note_temp_use(block->terminator.as.branch.condition,
                    analysis->use_counts,
                    analysis->use_sites,
                    analysis->has_unique_use_site,
                    analysis->temp_count,
                    block->id,
                    0,
                    IR_PASS_TEMP_USE_TERM_BRANCH,
                    0,
                    function->name,
                    error)) {
                ir_pass_temp_analysis_free(analysis);
                return 0;
            }
            break;
        case IR_TERM_JUMP:
        default:
            break;
        }
    }

    {
        unsigned char *visit_state = (unsigned char *)calloc(analysis->temp_count, sizeof(unsigned char));
        size_t temp_index;

        if (!visit_state) {
            ir_pass_temp_analysis_free(analysis);
            ir_pass_set_error(error,
                0,
                0,
                "IR-PASS-011: out of memory building temp analysis for '%s'",
                function->name ? function->name : "<unnamed>");
            return 0;
        }

        for (temp_index = 0; temp_index < analysis->temp_count; ++temp_index) {
            if (!ir_pass_try_resolve_temp_constant(analysis,
                    temp_index,
                    function->name,
                    error,
                    visit_state)) {
                free(visit_state);
                ir_pass_temp_analysis_free(analysis);
                return 0;
            }
        }

        free(visit_state);
    }

    return 1;
}

static int ir_pass_note_temp_use(IrValueRef value,
    size_t *use_counts,
    IrPassTempUseSite *use_sites,
    unsigned char *has_unique_use_site,
    size_t use_count_len,
    size_t block_id,
    size_t instruction_index,
    IrPassTempUseKind kind,
    size_t operand_index,
    const char *function_name,
    IrError *error) {
    if (value.kind != IR_VALUE_TEMP) {
        return 1;
    }

    if (!use_counts || !use_sites || !has_unique_use_site || value.id >= use_count_len) {
        ir_pass_set_error(error,
            0,
            0,
            "IR-PASS-007: function '%s' temp use id %zu is out of range",
            function_name ? function_name : "<unnamed>",
            value.id);
        return 0;
    }

    if (use_counts[value.id] == 0) {
        use_sites[value.id].block_id = block_id;
        use_sites[value.id].instruction_index = instruction_index;
        use_sites[value.id].operand_index = operand_index;
        use_sites[value.id].kind = kind;
        has_unique_use_site[value.id] = 1u;
    } else {
        has_unique_use_site[value.id] = 0u;
    }

    ++use_counts[value.id];
    return 1;
}

static int ir_pass_note_temp_remap_id(IrValueRef value,
    size_t *temp_remap,
    size_t temp_remap_len,
    size_t *next_temp_id,
    const char *function_name,
    IrError *error) {
    if (value.kind != IR_VALUE_TEMP) {
        return 1;
    }
    if (!temp_remap || !next_temp_id || value.id >= temp_remap_len) {
        ir_pass_set_error(error,
            0,
            0,
            "IR-PASS-012: function '%s' temp remap id %zu is out of range",
            function_name ? function_name : "<unnamed>",
            value.id);
        return 0;
    }
    if (temp_remap[value.id] != (size_t)-1) {
        return 1;
    }

    temp_remap[value.id] = *next_temp_id;
    *next_temp_id += 1;
    return 1;
}

static int ir_pass_apply_temp_remap(IrValueRef *value,
    const size_t *temp_remap,
    size_t temp_remap_len,
    const char *function_name,
    IrError *error) {
    if (!value || value->kind != IR_VALUE_TEMP) {
        return 1;
    }
    if (!temp_remap || value->id >= temp_remap_len || temp_remap[value->id] == (size_t)-1) {
        ir_pass_set_error(error,
            0,
            0,
            "IR-PASS-013: function '%s' temp.%zu has no remapped id",
            function_name ? function_name : "<unnamed>",
            value->id);
        return 0;
    }

    value->id = temp_remap[value->id];
    return 1;
}

static int ir_pass_is_removable_dead_temp_def(const IrInstruction *instruction,
    const size_t *use_counts,
    size_t use_count_len) {
    if (!instruction || !use_counts) {
        return 0;
    }
    if (instruction->result.kind != IR_VALUE_TEMP || instruction->result.id >= use_count_len) {
        return 0;
    }
    if (use_counts[instruction->result.id] != 0) {
        return 0;
    }

    return !ir_pass_instruction_has_side_effects(instruction);
}

static int ir_pass_compact_function_temp_ids(IrFunction *function, IrError *error) {
    size_t *temp_remap;
    size_t next_temp_id = 0;
    size_t block_index;

    if (!function) {
        return 0;
    }
    if (function->next_temp_id == 0) {
        return 1;
    }

    temp_remap = (size_t *)malloc(function->next_temp_id * sizeof(size_t));
    if (!temp_remap) {
        ir_pass_set_error(error,
            0,
            0,
            "IR-PASS-014: out of memory building temp remap for '%s'",
            function->name ? function->name : "<unnamed>");
        return 0;
    }

    for (block_index = 0; block_index < function->next_temp_id; ++block_index) {
        temp_remap[block_index] = (size_t)-1;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        IrBasicBlock *block = &function->blocks[block_index];
        size_t instruction_index;

        for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
            IrInstruction *instruction = &block->instructions[instruction_index];
            size_t arg_index;

            if (!ir_pass_note_temp_remap_id(instruction->result,
                    temp_remap,
                    function->next_temp_id,
                    &next_temp_id,
                    function->name,
                    error)) {
                free(temp_remap);
                return 0;
            }

            switch (instruction->kind) {
            case IR_INSTR_MOV:
                if (!ir_pass_note_temp_remap_id(instruction->as.mov_value,
                        temp_remap,
                        function->next_temp_id,
                        &next_temp_id,
                        function->name,
                        error)) {
                    free(temp_remap);
                    return 0;
                }
                break;
            case IR_INSTR_BINARY:
                if (!ir_pass_note_temp_remap_id(instruction->as.binary.lhs,
                        temp_remap,
                        function->next_temp_id,
                        &next_temp_id,
                        function->name,
                        error)
                    || !ir_pass_note_temp_remap_id(instruction->as.binary.rhs,
                        temp_remap,
                        function->next_temp_id,
                        &next_temp_id,
                        function->name,
                        error)) {
                    free(temp_remap);
                    return 0;
                }
                break;
            case IR_INSTR_CALL:
                for (arg_index = 0; arg_index < instruction->as.call.arg_count; ++arg_index) {
                    if (!ir_pass_note_temp_remap_id(instruction->as.call.args[arg_index],
                            temp_remap,
                            function->next_temp_id,
                            &next_temp_id,
                            function->name,
                            error)) {
                        free(temp_remap);
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
            if (!ir_pass_note_temp_remap_id(block->terminator.as.return_value,
                    temp_remap,
                    function->next_temp_id,
                    &next_temp_id,
                    function->name,
                    error)) {
                free(temp_remap);
                return 0;
            }
        } else if (block->terminator.kind == IR_TERM_BRANCH) {
            if (!ir_pass_note_temp_remap_id(block->terminator.as.branch.condition,
                    temp_remap,
                    function->next_temp_id,
                    &next_temp_id,
                    function->name,
                    error)) {
                free(temp_remap);
                return 0;
            }
        }
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        IrBasicBlock *block = &function->blocks[block_index];
        size_t instruction_index;

        for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
            IrInstruction *instruction = &block->instructions[instruction_index];
            size_t arg_index;

            if (!ir_pass_apply_temp_remap(&instruction->result,
                    temp_remap,
                    function->next_temp_id,
                    function->name,
                    error)) {
                free(temp_remap);
                return 0;
            }

            switch (instruction->kind) {
            case IR_INSTR_MOV:
                if (!ir_pass_apply_temp_remap(&instruction->as.mov_value,
                        temp_remap,
                        function->next_temp_id,
                        function->name,
                        error)) {
                    free(temp_remap);
                    return 0;
                }
                break;
            case IR_INSTR_BINARY:
                if (!ir_pass_apply_temp_remap(&instruction->as.binary.lhs,
                        temp_remap,
                        function->next_temp_id,
                        function->name,
                        error)
                    || !ir_pass_apply_temp_remap(&instruction->as.binary.rhs,
                        temp_remap,
                        function->next_temp_id,
                        function->name,
                        error)) {
                    free(temp_remap);
                    return 0;
                }
                break;
            case IR_INSTR_CALL:
                for (arg_index = 0; arg_index < instruction->as.call.arg_count; ++arg_index) {
                    if (!ir_pass_apply_temp_remap(&instruction->as.call.args[arg_index],
                            temp_remap,
                            function->next_temp_id,
                            function->name,
                            error)) {
                        free(temp_remap);
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
            if (!ir_pass_apply_temp_remap(&block->terminator.as.return_value,
                    temp_remap,
                    function->next_temp_id,
                    function->name,
                    error)) {
                free(temp_remap);
                return 0;
            }
        } else if (block->terminator.kind == IR_TERM_BRANCH) {
            if (!ir_pass_apply_temp_remap(&block->terminator.as.branch.condition,
                    temp_remap,
                    function->next_temp_id,
                    function->name,
                    error)) {
                free(temp_remap);
                return 0;
            }
        }
    }

    function->next_temp_id = next_temp_id;
    free(temp_remap);
    return 1;
}

#endif
