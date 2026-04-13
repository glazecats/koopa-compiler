#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_INTERP_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_INTERP_SPLIT_AGGREGATOR)
#define VALUE_SSA_INTERP_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa_interp.c"
#else

static int value_ssa_interp_execute_call(ValueSsaInterpContext *context,
    ValueSsaInterpFrame *frame,
    const ValueSsaInstruction *instruction,
    ValueSsaInterpError *error) {
    const ValueSsaFunction *callee;
    long long *args = NULL;
    size_t arg_index;
    long long result_value = 0;
    int ok = 0;

    if (!context || !frame || !instruction || instruction->kind != VALUE_SSA_INSTR_CALL) {
        value_ssa_interp_set_error(error, 0, 0, "VALUE-SSA-INTERP-010: invalid call execution contract");
        return 0;
    }

    args = instruction->as.call.arg_count == 0
        ? NULL
        : (long long *)malloc(instruction->as.call.arg_count * sizeof(long long));
    if (instruction->as.call.arg_count != 0 && !args) {
        value_ssa_interp_set_error(error,
            0,
            0,
            "VALUE-SSA-INTERP-011: out of memory while evaluating call to '%s'",
            instruction->as.call.callee_name ? instruction->as.call.callee_name : "<anon>");
        return 0;
    }

    for (arg_index = 0; arg_index < instruction->as.call.arg_count; ++arg_index) {
        if (!value_ssa_interp_eval_value(frame, instruction->as.call.args[arg_index], &args[arg_index], error)) {
            goto cleanup;
        }
    }

    callee = value_ssa_interp_find_function(context->program, instruction->as.call.callee_name);
    if (!callee) {
        value_ssa_interp_set_error(error,
            0,
            0,
            "VALUE-SSA-INTERP-012: call target '%s' not found",
            instruction->as.call.callee_name ? instruction->as.call.callee_name : "<anon>");
        goto cleanup;
    }

    if (callee->has_body) {
        if (!value_ssa_interp_execute_function_internal(
                context,
                callee,
                args,
                instruction->as.call.arg_count,
                &result_value,
                error)) {
            goto cleanup;
        }
    } else if (context->options->extern_call) {
        if (!context->options->extern_call(context->program,
                instruction->as.call.callee_name,
                args,
                instruction->as.call.arg_count,
                instruction->has_result,
                &result_value,
                context->global_values,
                context->program->global_count,
                context->options->extern_call_user_data,
                error)) {
            goto cleanup;
        }
    } else {
        value_ssa_interp_set_error(error,
            0,
            0,
            "VALUE-SSA-INTERP-013: external call '%s' requires a callback",
            instruction->as.call.callee_name ? instruction->as.call.callee_name : "<anon>");
        goto cleanup;
    }

    if (instruction->has_result && instruction->result.kind == VALUE_SSA_VALUE_ID) {
        frame->ssa_values[instruction->result.value_id] = result_value;
        frame->has_ssa_value[instruction->result.value_id] = 1;
    }

    ok = 1;

cleanup:
    free(args);
    return ok;
}

static int value_ssa_interp_execute_function_internal(ValueSsaInterpContext *context,
    const ValueSsaFunction *function,
    const long long *args,
    size_t arg_count,
    long long *out_return_value,
    ValueSsaInterpError *error) {
    ValueSsaInterpFrame frame;
    size_t local_index;
    size_t current_block_id = 0;
    size_t predecessor_block_id = (size_t)-1;
    int ok = 0;

    if (!context || !function || !out_return_value) {
        value_ssa_interp_set_error(error, 0, 0, "VALUE-SSA-INTERP-014: invalid function execution contract");
        return 0;
    }
    if (!function->has_body) {
        value_ssa_interp_set_error(error,
            0,
            0,
            "VALUE-SSA-INTERP-015: cannot directly execute declaration '%s'",
            function->name ? function->name : "<anon>");
        return 0;
    }
    if (arg_count != function->parameter_count) {
        value_ssa_interp_set_error(error,
            0,
            0,
            "VALUE-SSA-INTERP-016: function '%s' expected %zu args but got %zu",
            function->name ? function->name : "<anon>",
            function->parameter_count,
            arg_count);
        return 0;
    }
    if (context->call_depth >= context->options->max_call_depth) {
        value_ssa_interp_set_error(error,
            0,
            0,
            "VALUE-SSA-INTERP-017: call depth limit reached while entering '%s'",
            function->name ? function->name : "<anon>");
        return 0;
    }

    memset(&frame, 0, sizeof(frame));
    frame.function = function;
    frame.local_values = function->local_count == 0 ? NULL : (long long *)calloc(function->local_count, sizeof(long long));
    frame.has_local_value =
        function->local_count == 0 ? NULL : (unsigned char *)calloc(function->local_count, sizeof(unsigned char));
    frame.ssa_values =
        function->next_value_id == 0 ? NULL : (long long *)calloc(function->next_value_id, sizeof(long long));
    frame.has_ssa_value =
        function->next_value_id == 0 ? NULL : (unsigned char *)calloc(function->next_value_id, sizeof(unsigned char));
    if ((function->local_count != 0 && (!frame.local_values || !frame.has_local_value)) ||
        (function->next_value_id != 0 && (!frame.ssa_values || !frame.has_ssa_value))) {
        value_ssa_interp_set_error(error,
            0,
            0,
            "VALUE-SSA-INTERP-018: out of memory while entering '%s'",
            function->name ? function->name : "<anon>");
        goto cleanup;
    }

    for (local_index = 0; local_index < arg_count; ++local_index) {
        frame.local_values[local_index] = args[local_index];
        frame.has_local_value[local_index] = 1;
    }

    ++context->call_depth;
    while (1) {
        const ValueSsaBasicBlock *block;
        size_t phi_index;
        size_t instruction_index;
        long long terminator_value = 0;

        if (context->step_count >= context->options->max_steps) {
            value_ssa_interp_set_error(error,
                0,
                0,
                "VALUE-SSA-INTERP-019: step limit reached while executing '%s'",
                function->name ? function->name : "<anon>");
            goto cleanup;
        }

        if (current_block_id >= function->block_count) {
            value_ssa_interp_set_error(error,
                0,
                0,
                "VALUE-SSA-INTERP-020: control jumped to invalid bb.%zu in '%s'",
                current_block_id,
                function->name ? function->name : "<anon>");
            goto cleanup;
        }

        block = &function->blocks[current_block_id];
        for (phi_index = 0; phi_index < block->phi_count; ++phi_index) {
            const ValueSsaPhi *phi = &block->phis[phi_index];
            size_t input_index;
            int matched = 0;

            for (input_index = 0; input_index < phi->input_count; ++input_index) {
                if (phi->inputs[input_index].predecessor_block_id != predecessor_block_id) {
                    continue;
                }
                if (!value_ssa_interp_eval_value(&frame,
                        phi->inputs[input_index].value,
                        &frame.ssa_values[phi->result_id],
                        error)) {
                    goto cleanup;
                }
                frame.has_ssa_value[phi->result_id] = 1;
                matched = 1;
                break;
            }

            if (!matched) {
                value_ssa_interp_set_error(error,
                    0,
                    0,
                    "VALUE-SSA-INTERP-021: bb.%zu phi %zu has no input for predecessor bb.%zu in '%s'",
                    block->id,
                    phi_index,
                    predecessor_block_id,
                    function->name ? function->name : "<anon>");
                goto cleanup;
            }
        }

        for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
            const ValueSsaInstruction *instruction = &block->instructions[instruction_index];
            long long value;
            long long lhs;
            long long rhs;

            ++context->step_count;
            switch (instruction->kind) {
            case VALUE_SSA_INSTR_MOV:
                if (!value_ssa_interp_eval_value(&frame, instruction->as.mov_value, &value, error)) {
                    goto cleanup;
                }
                frame.ssa_values[instruction->result.value_id] = value;
                frame.has_ssa_value[instruction->result.value_id] = 1;
                break;
            case VALUE_SSA_INSTR_BINARY:
                if (!value_ssa_interp_eval_value(&frame, instruction->as.binary.lhs, &lhs, error) ||
                    !value_ssa_interp_eval_value(&frame, instruction->as.binary.rhs, &rhs, error) ||
                    !value_ssa_interp_eval_binary(instruction->as.binary.op, lhs, rhs, &value, error)) {
                    goto cleanup;
                }
                frame.ssa_values[instruction->result.value_id] = value;
                frame.has_ssa_value[instruction->result.value_id] = 1;
                break;
            case VALUE_SSA_INSTR_CALL:
                if (!value_ssa_interp_execute_call(context, &frame, instruction, error)) {
                    goto cleanup;
                }
                break;
            case VALUE_SSA_INSTR_LOAD_LOCAL:
                if (instruction->as.load_slot.kind != VALUE_SSA_SLOT_LOCAL ||
                    instruction->as.load_slot.id >= function->local_count ||
                    !frame.has_local_value[instruction->as.load_slot.id]) {
                    value_ssa_interp_set_error(error,
                        0,
                        0,
                        "VALUE-SSA-INTERP-022: invalid or uninitialized local load a.%zu in '%s'",
                        instruction->as.load_slot.id,
                        function->name ? function->name : "<anon>");
                    goto cleanup;
                }
                frame.ssa_values[instruction->result.value_id] = frame.local_values[instruction->as.load_slot.id];
                frame.has_ssa_value[instruction->result.value_id] = 1;
                break;
            case VALUE_SSA_INSTR_STORE_LOCAL:
                if (instruction->as.store.slot.kind != VALUE_SSA_SLOT_LOCAL ||
                    instruction->as.store.slot.id >= function->local_count ||
                    !value_ssa_interp_eval_value(&frame, instruction->as.store.value, &value, error)) {
                    goto cleanup;
                }
                frame.local_values[instruction->as.store.slot.id] = value;
                frame.has_local_value[instruction->as.store.slot.id] = 1;
                break;
            case VALUE_SSA_INSTR_LOAD_GLOBAL:
                if (instruction->as.load_slot.kind != VALUE_SSA_SLOT_GLOBAL ||
                    instruction->as.load_slot.id >= context->program->global_count) {
                    value_ssa_interp_set_error(error,
                        0,
                        0,
                        "VALUE-SSA-INTERP-023: invalid global load g.%zu in '%s'",
                        instruction->as.load_slot.id,
                        function->name ? function->name : "<anon>");
                    goto cleanup;
                }
                frame.ssa_values[instruction->result.value_id] = context->global_values[instruction->as.load_slot.id];
                frame.has_ssa_value[instruction->result.value_id] = 1;
                break;
            case VALUE_SSA_INSTR_STORE_GLOBAL:
                if (instruction->as.store.slot.kind != VALUE_SSA_SLOT_GLOBAL ||
                    instruction->as.store.slot.id >= context->program->global_count ||
                    !value_ssa_interp_eval_value(&frame, instruction->as.store.value, &value, error)) {
                    goto cleanup;
                }
                context->global_values[instruction->as.store.slot.id] = value;
                break;
            default:
                value_ssa_interp_set_error(error,
                    0,
                    0,
                    "VALUE-SSA-INTERP-024: unknown instruction kind %d in '%s'",
                    (int)instruction->kind,
                    function->name ? function->name : "<anon>");
                goto cleanup;
            }
        }

        ++context->step_count;
        switch (block->terminator.kind) {
        case VALUE_SSA_TERM_RETURN:
            if (!value_ssa_interp_eval_value(&frame, block->terminator.as.return_value, out_return_value, error)) {
                goto cleanup;
            }
            ok = 1;
            goto cleanup;
        case VALUE_SSA_TERM_JUMP:
            predecessor_block_id = current_block_id;
            current_block_id = block->terminator.as.jump_target;
            break;
        case VALUE_SSA_TERM_BRANCH:
            if (!value_ssa_interp_eval_value(
                    &frame,
                    block->terminator.as.branch.condition,
                    &terminator_value,
                    error)) {
                goto cleanup;
            }
            predecessor_block_id = current_block_id;
            current_block_id = terminator_value != 0 ? block->terminator.as.branch.then_target
                                                     : block->terminator.as.branch.else_target;
            break;
        default:
            value_ssa_interp_set_error(error,
                0,
                0,
                "VALUE-SSA-INTERP-025: unknown terminator kind %d in '%s'",
                (int)block->terminator.kind,
                function->name ? function->name : "<anon>");
            goto cleanup;
        }
    }

cleanup:
    if (context->call_depth > 0) {
        --context->call_depth;
    }
    free(frame.local_values);
    free(frame.has_local_value);
    free(frame.ssa_values);
    free(frame.has_ssa_value);
    return ok;
}

int value_ssa_interp_execute_function(const ValueSsaProgram *program,
    const char *function_name,
    const long long *args,
    size_t arg_count,
    const ValueSsaInterpOptions *options,
    ValueSsaInterpResult *out_result,
    ValueSsaInterpError *error) {
    ValueSsaInterpOptions default_options;
    ValueSsaInterpContext context;
    const ValueSsaFunction *function;
    size_t global_index;

    if (!program || !function_name || !out_result) {
        value_ssa_interp_set_error(error, 0, 0, "VALUE-SSA-INTERP-026: invalid execute contract");
        return 0;
    }

    value_ssa_interp_options_init(&default_options);
    if (!options) {
        options = &default_options;
    }

    function = value_ssa_interp_find_function(program, function_name);
    if (!function) {
        value_ssa_interp_set_error(error, 0, 0, "VALUE-SSA-INTERP-027: function '%s' not found", function_name);
        return 0;
    }

    value_ssa_interp_result_free(out_result);
    context.program = program;
    context.options = options;
    context.global_values =
        program->global_count == 0 ? NULL : (long long *)calloc(program->global_count, sizeof(long long));
    context.step_count = 0;
    context.call_depth = 0;
    if (program->global_count != 0 && !context.global_values) {
        value_ssa_interp_set_error(error, 0, 0, "VALUE-SSA-INTERP-028: out of memory for globals");
        return 0;
    }

    for (global_index = 0; global_index < program->global_count; ++global_index) {
        if (program->globals[global_index].has_initializer) {
            context.global_values[global_index] = program->globals[global_index].initializer_value;
        }
    }

    if (!value_ssa_interp_execute_function_internal(
            &context,
            function,
            args,
            arg_count,
            &out_result->return_value,
            error)) {
        free(context.global_values);
        return 0;
    }

    out_result->global_values = context.global_values;
    out_result->global_count = program->global_count;
    return 1;
}

int value_ssa_interp_execute_main(const ValueSsaProgram *program,
    const long long *args,
    size_t arg_count,
    const ValueSsaInterpOptions *options,
    ValueSsaInterpResult *out_result,
    ValueSsaInterpError *error) {
    return value_ssa_interp_execute_function(program, "main", args, arg_count, options, out_result, error);
}

#endif
