#if defined(__INTELLISENSE__) && !defined(LOWER_IR_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(LOWER_IR_SPLIT_AGGREGATOR)
#define LOWER_IR_SPLIT_INTELLISENSE_CONTEXT
#include "lower_ir.c"
#else

static int lower_ir_verify_value_ref(LowerIrValueRef value,
    size_t next_temp_id,
    const char *role,
    LowerIrError *error) {
    switch (value.kind) {
    case LOWER_IR_VALUE_IMMEDIATE:
        return 1;
    case LOWER_IR_VALUE_TEMP:
        if (value.temp_id >= next_temp_id) {
            lower_ir_set_error(error,
                0,
                0,
                "LOWER-IR-VERIFY-001: %s references tmp.%zu outside temp range",
                role ? role : "value",
                value.temp_id);
            return 0;
        }
        return 1;
    default:
        lower_ir_set_error(error,
            0,
            0,
            "LOWER-IR-VERIFY-002: %s uses unknown value kind %d",
            role ? role : "value",
            (int)value.kind);
        return 0;
    }
}

static int lower_ir_verify_slot_ref(const LowerIrProgram *program,
    const LowerIrFunction *function,
    LowerIrSlotRef slot,
    const char *role,
    LowerIrError *error) {
    switch (slot.kind) {
    case LOWER_IR_SLOT_LOCAL:
        if (!function || slot.id >= function->local_count) {
            lower_ir_set_error(error,
                0,
                0,
                "LOWER-IR-VERIFY-003: %s references local.%zu outside local table",
                role ? role : "slot",
                slot.id);
            return 0;
        }
        return 1;
    case LOWER_IR_SLOT_GLOBAL:
        if (!program || slot.id >= program->global_count) {
            lower_ir_set_error(error,
                0,
                0,
                "LOWER-IR-VERIFY-004: %s references global.%zu outside global table",
                role ? role : "slot",
                slot.id);
            return 0;
        }
        return 1;
    default:
        lower_ir_set_error(error,
            0,
            0,
            "LOWER-IR-VERIFY-005: %s uses unknown slot kind %d",
            role ? role : "slot",
            (int)slot.kind);
        return 0;
    }
}

static int lower_ir_verify_binary_op(LowerIrBinaryOp op, size_t block_id, size_t instruction_index, LowerIrError *error) {
    switch (op) {
    case LOWER_IR_BINARY_ADD:
    case LOWER_IR_BINARY_SUB:
    case LOWER_IR_BINARY_MUL:
    case LOWER_IR_BINARY_DIV:
    case LOWER_IR_BINARY_MOD:
    case LOWER_IR_BINARY_BIT_AND:
    case LOWER_IR_BINARY_BIT_XOR:
    case LOWER_IR_BINARY_BIT_OR:
    case LOWER_IR_BINARY_SHIFT_LEFT:
    case LOWER_IR_BINARY_SHIFT_RIGHT:
    case LOWER_IR_BINARY_EQ:
    case LOWER_IR_BINARY_NE:
    case LOWER_IR_BINARY_LT:
    case LOWER_IR_BINARY_LE:
    case LOWER_IR_BINARY_GT:
    case LOWER_IR_BINARY_GE:
        return 1;
    default:
        lower_ir_set_error(error,
            0,
            0,
            "LOWER-IR-VERIFY-006: bb.%zu instruction %zu uses unknown binary op %d",
            block_id,
            instruction_index,
            (int)op);
        return 0;
    }
}

static const LowerIrFunction *lower_ir_find_program_function(const LowerIrProgram *program, const char *name) {
    size_t i;

    if (!program || !name || name[0] == '\0') {
        return NULL;
    }

    for (i = 0; i < program->function_count; ++i) {
        const LowerIrFunction *function = &program->functions[i];

        if (function->name && strcmp(function->name, name) == 0) {
            return function;
        }
    }

    return NULL;
}

static int lower_ir_instruction_is_zero_arg_call_to(const LowerIrInstruction *instruction, const char *callee_name) {
    if (!instruction || !callee_name || callee_name[0] == '\0') {
        return 0;
    }
    if (instruction->kind != LOWER_IR_INSTR_CALL) {
        return 0;
    }
    if (instruction->as.call.arg_count != 0 || !instruction->as.call.callee_name) {
        return 0;
    }

    return strcmp(instruction->as.call.callee_name, callee_name) == 0;
}

static int lower_ir_function_has_reserved_internal_init_name(const LowerIrFunction *function) {
    if (!function || !function->name) {
        return 0;
    }

    return strcmp(function->name, "__global.init") == 0 ||
        strcmp(function->name, "__program.init") == 0;
}

static int lower_ir_entry_block_starts_with_call_to(const LowerIrFunction *function, const char *callee_name) {
    const LowerIrBasicBlock *entry_block;

    if (!function || !function->has_body || function->block_count == 0 || !function->blocks) {
        return 0;
    }

    entry_block = &function->blocks[0];
    if (entry_block->instruction_count == 0 || !entry_block->instructions) {
        return 0;
    }

    return lower_ir_instruction_is_zero_arg_call_to(&entry_block->instructions[0], callee_name);
}

static int lower_ir_verify_internal_initializer_call_policy(const LowerIrFunction *caller,
    const LowerIrBasicBlock *block,
    size_t instruction_index,
    const LowerIrInstruction *instruction,
    LowerIrError *error) {
    static const char *k_runtime_init_name = "__global.init";
    static const char *k_program_init_name = "__program.init";
    int is_runtime_init_call;
    int is_program_init_call;
    int allowed_runtime_init_callsite;

    if (!caller || !block || !instruction) {
        lower_ir_set_error(error, 0, 0, "LOWER-IR-VERIFY-007: invalid verifier contract");
        return 0;
    }

    if (instruction->kind != LOWER_IR_INSTR_CALL ||
        !instruction->as.call.callee_name ||
        instruction->as.call.callee_name[0] == '\0') {
        return 1;
    }

    is_runtime_init_call = strcmp(instruction->as.call.callee_name, k_runtime_init_name) == 0;
    is_program_init_call = strcmp(instruction->as.call.callee_name, k_program_init_name) == 0;

    if (!is_runtime_init_call && !is_program_init_call) {
        return 1;
    }

    if (is_program_init_call) {
        lower_ir_set_error(error,
            0,
            0,
            "LOWER-IR-VERIFY-044: bb.%zu instruction %zu calls reserved internal startup helper '%s' from '%s'",
            block->id,
            instruction_index,
            k_program_init_name,
            caller->name ? caller->name : "<anon>");
        return 0;
    }

    allowed_runtime_init_callsite =
        ((caller->name && strcmp(caller->name, "main") == 0) ||
            (caller->name && strcmp(caller->name, k_program_init_name) == 0)) &&
        block->id == 0 &&
        instruction_index == 0;

    if (!allowed_runtime_init_callsite) {
        lower_ir_set_error(error,
            0,
            0,
            "LOWER-IR-VERIFY-043: bb.%zu instruction %zu calls reserved internal init helper '%s' from non-startup entry site in '%s'",
            block->id,
            instruction_index,
            k_runtime_init_name,
            caller->name ? caller->name : "<anon>");
        return 0;
    }

    return 1;
}

static int lower_ir_verify_temp_definitions(const LowerIrFunction *function, LowerIrError *error) {
    size_t *temp_definition_counts = NULL;
    size_t *temp_last_definition_block = NULL;
    size_t i;

    if (!function) {
        lower_ir_set_error(error, 0, 0, "LOWER-IR-VERIFY-007: invalid verifier contract");
        return 0;
    }
    if (function->next_temp_id == 0) {
        return 1;
    }

    temp_definition_counts = (size_t *)calloc(function->next_temp_id, sizeof(size_t));
    temp_last_definition_block = (size_t *)malloc(function->next_temp_id * sizeof(size_t));
    if (!temp_definition_counts || !temp_last_definition_block) {
        free(temp_definition_counts);
        free(temp_last_definition_block);
        lower_ir_set_error(error,
            0,
            0,
            "LOWER-IR-VERIFY-055: out of memory while verifying temp definitions");
        return 0;
    }
    for (i = 0; i < function->next_temp_id; ++i) {
        temp_last_definition_block[i] = (size_t)-1;
    }

    for (i = 0; i < function->block_count; ++i) {
        const LowerIrBasicBlock *block = &function->blocks[i];
        size_t j;

        if (block->instruction_count > 0 && !block->instructions) {
            free(temp_definition_counts);
            free(temp_last_definition_block);
            lower_ir_set_error(error,
                0,
                0,
                "LOWER-IR-VERIFY-060: bb.%zu has instruction_count=%zu but instruction table is NULL",
                block->id,
                block->instruction_count);
            return 0;
        }

        for (j = 0; j < block->instruction_count; ++j) {
            const LowerIrInstruction *instruction = &block->instructions[j];

            if (instruction->has_result && instruction->result.kind == LOWER_IR_VALUE_TEMP) {
                size_t temp_id = instruction->result.temp_id;

                if (temp_last_definition_block[temp_id] == block->id) {
                    free(temp_definition_counts);
                    free(temp_last_definition_block);
                    lower_ir_set_error(error,
                        0,
                        0,
                        "LOWER-IR-VERIFY-057: function '%s' temp.%zu is defined multiple times in bb.%zu",
                        function->name ? function->name : "<anon>",
                        temp_id,
                        block->id);
                    return 0;
                }

                temp_definition_counts[temp_id] += 1;
                temp_last_definition_block[temp_id] = block->id;
            }
        }
    }

    for (i = 0; i < function->next_temp_id; ++i) {
        if (temp_definition_counts[i] == 0) {
            free(temp_definition_counts);
            free(temp_last_definition_block);
            lower_ir_set_error(error,
                0,
                0,
                "LOWER-IR-VERIFY-056: function '%s' temp.%zu has no defining instruction",
                function->name ? function->name : "<anon>",
                i);
            return 0;
        }
    }

    free(temp_definition_counts);
    free(temp_last_definition_block);
    return 1;
}

static int lower_ir_verify_temp_use_is_available(LowerIrValueRef value,
    const unsigned char *available,
    size_t temp_count,
    size_t block_id,
    const char *role,
    LowerIrError *error) {
    if (value.kind != LOWER_IR_VALUE_TEMP) {
        return 1;
    }
    if (!available || value.temp_id >= temp_count || !available[value.temp_id]) {
        lower_ir_set_error(error,
            0,
            0,
            "LOWER-IR-VERIFY-059: bb.%zu uses tmp.%zu before it is defined on all incoming paths (%s)",
            block_id,
            value.temp_id,
            role ? role : "value");
        return 0;
    }
    return 1;
}

static int lower_ir_verify_temp_availability(const LowerIrFunction *function, LowerIrError *error) {
    unsigned char *block_defs = NULL;
    unsigned char *in_sets = NULL;
    unsigned char *out_sets = NULL;
    unsigned char *edges = NULL;
    unsigned char *available = NULL;
    size_t temp_count;
    size_t block_count;
    int changed = 1;
    size_t i;

    if (!function) {
        lower_ir_set_error(error, 0, 0, "LOWER-IR-VERIFY-007: invalid verifier contract");
        return 0;
    }

    temp_count = function->next_temp_id;
    block_count = function->block_count;
    if (temp_count == 0 || block_count == 0) {
        return 1;
    }

    block_defs = (unsigned char *)calloc(block_count * temp_count, sizeof(unsigned char));
    in_sets = (unsigned char *)calloc(block_count * temp_count, sizeof(unsigned char));
    out_sets = (unsigned char *)calloc(block_count * temp_count, sizeof(unsigned char));
    edges = (unsigned char *)calloc(block_count * block_count, sizeof(unsigned char));
    available = (unsigned char *)malloc(temp_count * sizeof(unsigned char));
    if (!block_defs || !in_sets || !out_sets || !edges || !available) {
        free(block_defs);
        free(in_sets);
        free(out_sets);
        free(edges);
        free(available);
        lower_ir_set_error(error,
            0,
            0,
            "LOWER-IR-VERIFY-058: out of memory while verifying temp availability");
        return 0;
    }

    for (i = 0; i < block_count; ++i) {
        const LowerIrBasicBlock *block = &function->blocks[i];
        size_t j;

        for (j = 0; j < block->instruction_count; ++j) {
            const LowerIrInstruction *instruction = &block->instructions[j];

            if (instruction->has_result && instruction->result.kind == LOWER_IR_VALUE_TEMP) {
                block_defs[i * temp_count + instruction->result.temp_id] = 1;
            }
        }

        switch (block->terminator.kind) {
        case LOWER_IR_TERM_JUMP:
            edges[i * block_count + block->terminator.as.jump_target] = 1;
            break;
        case LOWER_IR_TERM_BRANCH:
            edges[i * block_count + block->terminator.as.branch.then_target] = 1;
            edges[i * block_count + block->terminator.as.branch.else_target] = 1;
            break;
        case LOWER_IR_TERM_RETURN:
        default:
            break;
        }
    }

    while (changed) {
        changed = 0;
        for (i = 0; i < block_count; ++i) {
            size_t t;

            for (t = 0; t < temp_count; ++t) {
                unsigned char next_in;
                unsigned char next_out;

                if (i == 0) {
                    next_in = 0;
                } else {
                    size_t pred;
                    int has_pred = 0;

                    next_in = 1;
                    for (pred = 0; pred < block_count; ++pred) {
                        if (!edges[pred * block_count + i]) {
                            continue;
                        }
                        has_pred = 1;
                        if (!out_sets[pred * temp_count + t]) {
                            next_in = 0;
                            break;
                        }
                    }
                    if (!has_pred) {
                        next_in = 0;
                    }
                }

                next_out = (unsigned char)(next_in || block_defs[i * temp_count + t]);
                if (in_sets[i * temp_count + t] != next_in ||
                    out_sets[i * temp_count + t] != next_out) {
                    in_sets[i * temp_count + t] = next_in;
                    out_sets[i * temp_count + t] = next_out;
                    changed = 1;
                }
            }
        }
    }

    for (i = 0; i < block_count; ++i) {
        const LowerIrBasicBlock *block = &function->blocks[i];
        size_t j;

        memcpy(available, &in_sets[i * temp_count], temp_count * sizeof(unsigned char));
        for (j = 0; j < block->instruction_count; ++j) {
            const LowerIrInstruction *instruction = &block->instructions[j];

            switch (instruction->kind) {
            case LOWER_IR_INSTR_MOV:
                if (!lower_ir_verify_temp_use_is_available(instruction->as.mov_value,
                        available,
                        temp_count,
                        block->id,
                        "mov operand",
                        error)) {
                    free(block_defs);
                    free(in_sets);
                    free(out_sets);
                    free(edges);
                    free(available);
                    return 0;
                }
                break;
            case LOWER_IR_INSTR_BINARY:
                if (!lower_ir_verify_temp_use_is_available(instruction->as.binary.lhs,
                        available,
                        temp_count,
                        block->id,
                        "binary lhs",
                        error) ||
                    !lower_ir_verify_temp_use_is_available(instruction->as.binary.rhs,
                        available,
                        temp_count,
                        block->id,
                        "binary rhs",
                        error)) {
                    free(block_defs);
                    free(in_sets);
                    free(out_sets);
                    free(edges);
                    free(available);
                    return 0;
                }
                break;
            case LOWER_IR_INSTR_CALL:
                for (size_t arg_index = 0; arg_index < instruction->as.call.arg_count; ++arg_index) {
                    if (!lower_ir_verify_temp_use_is_available(instruction->as.call.args[arg_index],
                            available,
                            temp_count,
                            block->id,
                            "call argument",
                            error)) {
                        free(block_defs);
                        free(in_sets);
                        free(out_sets);
                        free(edges);
                        free(available);
                        return 0;
                    }
                }
                break;
            case LOWER_IR_INSTR_STORE_LOCAL:
            case LOWER_IR_INSTR_STORE_GLOBAL:
                if (!lower_ir_verify_temp_use_is_available(instruction->as.store.value,
                        available,
                        temp_count,
                        block->id,
                        instruction->kind == LOWER_IR_INSTR_STORE_LOCAL
                            ? "store_local value"
                            : "store_global value",
                        error)) {
                    free(block_defs);
                    free(in_sets);
                    free(out_sets);
                    free(edges);
                    free(available);
                    return 0;
                }
                break;
            case LOWER_IR_INSTR_LOAD_LOCAL:
            case LOWER_IR_INSTR_LOAD_GLOBAL:
            default:
                break;
            }

            if (instruction->has_result && instruction->result.kind == LOWER_IR_VALUE_TEMP) {
                available[instruction->result.temp_id] = 1;
            }
        }

        switch (block->terminator.kind) {
        case LOWER_IR_TERM_RETURN:
            if (!lower_ir_verify_temp_use_is_available(block->terminator.as.return_value,
                    available,
                    temp_count,
                    block->id,
                    "return terminator",
                    error)) {
                free(block_defs);
                free(in_sets);
                free(out_sets);
                free(edges);
                free(available);
                return 0;
            }
            break;
        case LOWER_IR_TERM_BRANCH:
            if (!lower_ir_verify_temp_use_is_available(block->terminator.as.branch.condition,
                    available,
                    temp_count,
                    block->id,
                    "branch condition",
                    error)) {
                free(block_defs);
                free(in_sets);
                free(out_sets);
                free(edges);
                free(available);
                return 0;
            }
            break;
        case LOWER_IR_TERM_JUMP:
        default:
            break;
        }
    }

    free(block_defs);
    free(in_sets);
    free(out_sets);
    free(edges);
    free(available);
    return 1;
}

static int lower_ir_return_terminator_is_zero_immediate(const LowerIrTerminator *terminator) {
    if (!terminator || terminator->kind != LOWER_IR_TERM_RETURN) {
        return 0;
    }

    return terminator->as.return_value.kind == LOWER_IR_VALUE_IMMEDIATE &&
        terminator->as.return_value.immediate == 0;
}

static int lower_ir_verify_internal_init_function_shape(const LowerIrFunction *function,
    const char *name,
    LowerIrError *error) {
    const LowerIrBasicBlock *entry_block;

    if (!function || !name || name[0] == '\0') {
        lower_ir_set_error(error, 0, 0, "LOWER-IR-VERIFY-007: invalid verifier contract");
        return 0;
    }

    if (function->parameter_count != 0 || function->local_count != 0) {
        lower_ir_set_error(error,
            0,
            0,
            "LOWER-IR-VERIFY-035: internal initializer function '%s' must have zero parameters and zero locals",
            name);
        return 0;
    }

    if (!function->has_body || function->block_count != 1 || !function->blocks) {
        lower_ir_set_error(error,
            0,
            0,
            "LOWER-IR-VERIFY-036: internal initializer function '%s' must have exactly one body block",
            name);
        return 0;
    }

    entry_block = &function->blocks[0];
    if (!entry_block->has_terminator ||
        !lower_ir_return_terminator_is_zero_immediate(&entry_block->terminator)) {
        lower_ir_set_error(error,
            0,
            0,
            "LOWER-IR-VERIFY-037: internal initializer function '%s' entry block must terminate with 'ret 0'",
            name);
        return 0;
    }

    return 1;
}

static int lower_ir_verify_runtime_global_initializer_contract(const LowerIrProgram *program,
    LowerIrError *error) {
    static const char *k_runtime_init_name = "__global.init";
    static const char *k_program_init_name = "__program.init";
    const LowerIrFunction *runtime_init_function;
    const LowerIrFunction *main_function;
    const LowerIrFunction *program_init_function;
    int has_runtime_global_initializer = 0;
    int has_startup_call = 0;
    size_t i;

    if (!program) {
        lower_ir_set_error(error, 0, 0, "LOWER-IR-VERIFY-026: invalid program verifier contract");
        return 0;
    }

    for (i = 0; i < program->global_count; ++i) {
        if (program->globals[i].has_runtime_initializer) {
            has_runtime_global_initializer = 1;
            break;
        }
    }

    runtime_init_function = lower_ir_find_program_function(program, k_runtime_init_name);
    program_init_function = lower_ir_find_program_function(program, k_program_init_name);

    if (!has_runtime_global_initializer &&
        (runtime_init_function || program_init_function)) {
        lower_ir_set_error(error,
            0,
            0,
            "LOWER-IR-VERIFY-040: internal initializer helper names ('%s'/'%s') are reserved for runtime global initialization only",
            k_runtime_init_name,
            k_program_init_name);
        return 0;
    }

    if (runtime_init_function &&
        !lower_ir_verify_internal_init_function_shape(runtime_init_function,
            k_runtime_init_name,
            error)) {
        return 0;
    }
    if (program_init_function &&
        !lower_ir_verify_internal_init_function_shape(program_init_function,
            k_program_init_name,
            error)) {
        return 0;
    }

    if (program_init_function) {
        const LowerIrBasicBlock *program_init_entry = &program_init_function->blocks[0];

        if (program_init_entry->instruction_count != 1 ||
            !program_init_entry->instructions) {
            lower_ir_set_error(error,
                0,
                0,
                "LOWER-IR-VERIFY-041: internal initializer function '%s' must contain exactly one entry instruction before 'ret 0'",
                k_program_init_name);
            return 0;
        }

        if (!lower_ir_instruction_is_zero_arg_call_to(&program_init_entry->instructions[0],
                k_runtime_init_name)) {
            lower_ir_set_error(error,
                0,
                0,
                "LOWER-IR-VERIFY-042: internal initializer function '%s' must begin with zero-arg call to '%s'",
                k_program_init_name,
                k_runtime_init_name);
            return 0;
        }
    }

    if (!has_runtime_global_initializer) {
        return 1;
    }

    if (!runtime_init_function) {
        lower_ir_set_error(error,
            0,
            0,
            "LOWER-IR-VERIFY-038: runtime global initializers require lowered helper function '%s'",
            k_runtime_init_name);
        return 0;
    }

    main_function = lower_ir_find_program_function(program, "main");
    if (lower_ir_entry_block_starts_with_call_to(main_function, k_runtime_init_name)) {
        has_startup_call = 1;
    }

    if (!has_startup_call) {
        if (lower_ir_entry_block_starts_with_call_to(program_init_function, k_runtime_init_name)) {
            has_startup_call = 1;
        }
    }

    if (!has_startup_call) {
        lower_ir_set_error(error,
            0,
            0,
            "LOWER-IR-VERIFY-039: runtime global initializers require startup entry call to '%s' from main or '%s'",
            k_runtime_init_name,
            k_program_init_name);
        return 0;
    }

    return 1;
}

static int lower_ir_verify_instruction(const LowerIrProgram *program,
    const LowerIrFunction *function,
    const LowerIrBasicBlock *block,
    size_t instruction_index,
    const LowerIrInstruction *instruction,
    LowerIrError *error) {
    if (!program || !function || !block || !instruction) {
        lower_ir_set_error(error, 0, 0, "LOWER-IR-VERIFY-007: invalid verifier contract");
        return 0;
    }

    switch (instruction->kind) {
    case LOWER_IR_INSTR_MOV:
        if (!instruction->has_result || instruction->result.kind != LOWER_IR_VALUE_TEMP) {
            lower_ir_set_error(error,
                0,
                0,
                "LOWER-IR-VERIFY-008: bb.%zu instruction %zu mov must produce a temp result",
                block->id,
                instruction_index);
            return 0;
        }
        if (!lower_ir_verify_value_ref(instruction->result,
                function->next_temp_id,
                "mov result",
                error) ||
            !lower_ir_verify_value_ref(instruction->as.mov_value,
                function->next_temp_id,
                "mov operand",
                error)) {
            return 0;
        }
        return 1;
    case LOWER_IR_INSTR_BINARY:
        if (!instruction->has_result || instruction->result.kind != LOWER_IR_VALUE_TEMP) {
            lower_ir_set_error(error,
                0,
                0,
                "LOWER-IR-VERIFY-009: bb.%zu instruction %zu binary must produce a temp result",
                block->id,
                instruction_index);
            return 0;
        }
        if (!lower_ir_verify_value_ref(instruction->result,
                function->next_temp_id,
                "binary result",
                error) ||
            !lower_ir_verify_binary_op(instruction->as.binary.op, block->id, instruction_index, error) ||
            !lower_ir_verify_value_ref(instruction->as.binary.lhs,
                function->next_temp_id,
                "binary lhs",
                error) ||
            !lower_ir_verify_value_ref(instruction->as.binary.rhs,
                function->next_temp_id,
                "binary rhs",
                error)) {
            return 0;
        }
        return 1;
    case LOWER_IR_INSTR_CALL:
        {
            const LowerIrFunction *callee;

        if (!instruction->has_result || instruction->result.kind != LOWER_IR_VALUE_TEMP) {
            lower_ir_set_error(error,
                0,
                0,
                "LOWER-IR-VERIFY-010: bb.%zu instruction %zu call must produce a temp result",
                block->id,
                instruction_index);
            return 0;
        }
        if (!instruction->as.call.callee_name || instruction->as.call.callee_name[0] == '\0') {
            lower_ir_set_error(error,
                0,
                0,
                "LOWER-IR-VERIFY-011: bb.%zu instruction %zu call has empty callee name",
                block->id,
                instruction_index);
            return 0;
        }
        if (instruction->as.call.arg_count > 0 && !instruction->as.call.args) {
            lower_ir_set_error(error,
                0,
                0,
                "LOWER-IR-VERIFY-012: bb.%zu instruction %zu call has arg_count>0 but NULL args",
                block->id,
                instruction_index);
            return 0;
        }
        if (instruction->as.call.arg_count == 0 && instruction->as.call.args) {
            lower_ir_set_error(error,
                0,
                0,
                "LOWER-IR-VERIFY-047: bb.%zu instruction %zu call has arg_count=0 but non-NULL args payload",
                block->id,
                instruction_index);
            return 0;
        }
        callee = lower_ir_find_program_function(program, instruction->as.call.callee_name);
        if (!callee) {
            lower_ir_set_error(error,
                0,
                0,
                "LOWER-IR-VERIFY-045: bb.%zu instruction %zu calls unknown function '%s'",
                block->id,
                instruction_index,
                instruction->as.call.callee_name);
            return 0;
        }
        if (instruction->as.call.arg_count != callee->parameter_count) {
            lower_ir_set_error(error,
                0,
                0,
                "LOWER-IR-VERIFY-046: bb.%zu instruction %zu calls '%s' with %zu args but function expects %zu",
                block->id,
                instruction_index,
                instruction->as.call.callee_name,
                instruction->as.call.arg_count,
                callee->parameter_count);
            return 0;
        }
        if (!lower_ir_verify_value_ref(instruction->result,
                function->next_temp_id,
                "call result",
                error)) {
            return 0;
        }
        for (size_t i = 0; i < instruction->as.call.arg_count; ++i) {
            if (!lower_ir_verify_value_ref(instruction->as.call.args[i],
                    function->next_temp_id,
                    "call argument",
                    error)) {
                return 0;
            }
        }
        return 1;
        }
    case LOWER_IR_INSTR_LOAD_LOCAL:
        if (!instruction->has_result || instruction->result.kind != LOWER_IR_VALUE_TEMP) {
            lower_ir_set_error(error,
                0,
                0,
                "LOWER-IR-VERIFY-013: bb.%zu instruction %zu load_local must produce a temp result",
                block->id,
                instruction_index);
            return 0;
        }
        if (!lower_ir_verify_value_ref(instruction->result,
                function->next_temp_id,
                "load_local result",
                error) ||
            !lower_ir_verify_slot_ref(program,
                function,
                instruction->as.load_slot,
                "load_local slot",
                error)) {
            return 0;
        }
        if (instruction->as.load_slot.kind != LOWER_IR_SLOT_LOCAL) {
            lower_ir_set_error(error,
                0,
                0,
                "LOWER-IR-VERIFY-014: bb.%zu instruction %zu load_local must use a local slot",
                block->id,
                instruction_index);
            return 0;
        }
        return 1;
    case LOWER_IR_INSTR_STORE_LOCAL:
        if (instruction->has_result) {
            lower_ir_set_error(error,
                0,
                0,
                "LOWER-IR-VERIFY-015: bb.%zu instruction %zu store_local must not produce a result",
                block->id,
                instruction_index);
            return 0;
        }
        if (!lower_ir_verify_slot_ref(program,
                function,
                instruction->as.store.slot,
                "store_local slot",
                error) ||
            !lower_ir_verify_value_ref(instruction->as.store.value,
                function->next_temp_id,
                "store_local value",
                error)) {
            return 0;
        }
        if (instruction->as.store.slot.kind != LOWER_IR_SLOT_LOCAL) {
            lower_ir_set_error(error,
                0,
                0,
                "LOWER-IR-VERIFY-016: bb.%zu instruction %zu store_local must use a local slot",
                block->id,
                instruction_index);
            return 0;
        }
        return 1;
    case LOWER_IR_INSTR_LOAD_GLOBAL:
        if (!instruction->has_result || instruction->result.kind != LOWER_IR_VALUE_TEMP) {
            lower_ir_set_error(error,
                0,
                0,
                "LOWER-IR-VERIFY-017: bb.%zu instruction %zu load_global must produce a temp result",
                block->id,
                instruction_index);
            return 0;
        }
        if (!lower_ir_verify_value_ref(instruction->result,
                function->next_temp_id,
                "load_global result",
                error) ||
            !lower_ir_verify_slot_ref(program,
                function,
                instruction->as.load_slot,
                "load_global slot",
                error)) {
            return 0;
        }
        if (instruction->as.load_slot.kind != LOWER_IR_SLOT_GLOBAL) {
            lower_ir_set_error(error,
                0,
                0,
                "LOWER-IR-VERIFY-018: bb.%zu instruction %zu load_global must use a global slot",
                block->id,
                instruction_index);
            return 0;
        }
        return 1;
    case LOWER_IR_INSTR_STORE_GLOBAL:
        if (instruction->has_result) {
            lower_ir_set_error(error,
                0,
                0,
                "LOWER-IR-VERIFY-019: bb.%zu instruction %zu store_global must not produce a result",
                block->id,
                instruction_index);
            return 0;
        }
        if (!lower_ir_verify_slot_ref(program,
                function,
                instruction->as.store.slot,
                "store_global slot",
                error) ||
            !lower_ir_verify_value_ref(instruction->as.store.value,
                function->next_temp_id,
                "store_global value",
                error)) {
            return 0;
        }
        if (instruction->as.store.slot.kind != LOWER_IR_SLOT_GLOBAL) {
            lower_ir_set_error(error,
                0,
                0,
                "LOWER-IR-VERIFY-020: bb.%zu instruction %zu store_global must use a global slot",
                block->id,
                instruction_index);
            return 0;
        }
        return 1;
    default:
        lower_ir_set_error(error,
            0,
            0,
            "LOWER-IR-VERIFY-021: bb.%zu instruction %zu uses unknown instruction kind %d",
            block->id,
            instruction_index,
            (int)instruction->kind);
        return 0;
    }
}

static int lower_ir_verify_terminator(const LowerIrFunction *function,
    const LowerIrBasicBlock *block,
    LowerIrError *error) {
    if (!function || !block) {
        lower_ir_set_error(error, 0, 0, "LOWER-IR-VERIFY-007: invalid verifier contract");
        return 0;
    }

    if (!block->has_terminator) {
        lower_ir_set_error(error,
            0,
            0,
            "LOWER-IR-VERIFY-022: bb.%zu is missing a terminator",
            block->id);
        return 0;
    }

    switch (block->terminator.kind) {
    case LOWER_IR_TERM_RETURN:
        return lower_ir_verify_value_ref(block->terminator.as.return_value,
            function->next_temp_id,
            "return terminator",
            error);
    case LOWER_IR_TERM_JUMP:
        if (block->terminator.as.jump_target >= function->block_count) {
            lower_ir_set_error(error,
                0,
                0,
                "LOWER-IR-VERIFY-023: bb.%zu jumps to missing block bb.%zu",
                block->id,
                block->terminator.as.jump_target);
            return 0;
        }
        return 1;
    case LOWER_IR_TERM_BRANCH:
        if (!lower_ir_verify_value_ref(block->terminator.as.branch.condition,
                function->next_temp_id,
                "branch condition",
                error)) {
            return 0;
        }
        if (block->terminator.as.branch.then_target >= function->block_count ||
            block->terminator.as.branch.else_target >= function->block_count) {
            lower_ir_set_error(error,
                0,
                0,
                "LOWER-IR-VERIFY-024: bb.%zu branches to missing successor",
                block->id);
            return 0;
        }
        return 1;
    default:
        lower_ir_set_error(error,
            0,
            0,
            "LOWER-IR-VERIFY-025: bb.%zu has unknown terminator kind %d",
            block->id,
            (int)block->terminator.kind);
        return 0;
    }
}

static int lower_ir_verify_reachability(const LowerIrFunction *function, LowerIrError *error) {
    unsigned char *reachable = NULL;
    size_t *pending = NULL;
    size_t pending_count = 0;
    size_t i;

    if (!function) {
        lower_ir_set_error(error, 0, 0, "LOWER-IR-VERIFY-007: invalid verifier contract");
        return 0;
    }
    if (function->block_count == 0) {
        return 1;
    }

    reachable = (unsigned char *)calloc(function->block_count, sizeof(unsigned char));
    pending = (size_t *)malloc(function->block_count * sizeof(size_t));
    if (!reachable || !pending) {
        free(reachable);
        free(pending);
        lower_ir_set_error(error,
            0,
            0,
            "LOWER-IR-VERIFY-070: out of memory while verifying reachability");
        return 0;
    }

    pending[pending_count++] = 0;
    reachable[0] = 1;
    while (pending_count > 0) {
        size_t block_id = pending[--pending_count];
        const LowerIrBasicBlock *block = &function->blocks[block_id];

        switch (block->terminator.kind) {
        case LOWER_IR_TERM_JUMP:
            if (!reachable[block->terminator.as.jump_target]) {
                reachable[block->terminator.as.jump_target] = 1;
                pending[pending_count++] = block->terminator.as.jump_target;
            }
            break;
        case LOWER_IR_TERM_BRANCH:
            if (!reachable[block->terminator.as.branch.then_target]) {
                reachable[block->terminator.as.branch.then_target] = 1;
                pending[pending_count++] = block->terminator.as.branch.then_target;
            }
            if (!reachable[block->terminator.as.branch.else_target]) {
                reachable[block->terminator.as.branch.else_target] = 1;
                pending[pending_count++] = block->terminator.as.branch.else_target;
            }
            break;
        case LOWER_IR_TERM_RETURN:
        default:
            break;
        }
    }

    for (i = 0; i < function->block_count; ++i) {
        if (!reachable[i]) {
            free(reachable);
            free(pending);
            lower_ir_set_error(error,
                0,
                0,
                "LOWER-IR-VERIFY-071: function '%s' contains unreachable block bb.%zu",
                function->name ? function->name : "<anon>",
                i);
            return 0;
        }
    }

    free(reachable);
    free(pending);
    return 1;
}

int lower_ir_verify_program(const LowerIrProgram *program, LowerIrError *error) {
    size_t i;
    size_t j;

    if (!program) {
        lower_ir_set_error(error, 0, 0, "LOWER-IR-VERIFY-026: invalid program verifier contract");
        return 0;
    }
    if (program->global_count > 0 && !program->globals) {
        lower_ir_set_error(error,
            0,
            0,
            "LOWER-IR-VERIFY-051: program has global_count=%zu but global table is NULL",
            program->global_count);
        return 0;
    }
    if (program->global_count > program->global_capacity) {
        lower_ir_set_error(error,
            0,
            0,
            "LOWER-IR-VERIFY-065: program has global_count=%zu but global_capacity=%zu",
            program->global_count,
            program->global_capacity);
        return 0;
    }
    if (program->function_count > 0 && !program->functions) {
        lower_ir_set_error(error,
            0,
            0,
            "LOWER-IR-VERIFY-052: program has function_count=%zu but function table is NULL",
            program->function_count);
        return 0;
    }
    if (program->function_count > program->function_capacity) {
        lower_ir_set_error(error,
            0,
            0,
            "LOWER-IR-VERIFY-066: program has function_count=%zu but function_capacity=%zu",
            program->function_count,
            program->function_capacity);
        return 0;
    }

    for (i = 0; i < program->global_count; ++i) {
        const LowerIrGlobal *global = &program->globals[i];

        if (global->id != i) {
            lower_ir_set_error(error,
                0,
                0,
                "LOWER-IR-VERIFY-027: global table is not densely indexed at %zu",
                i);
            return 0;
        }
        if (!global->name || global->name[0] == '\0') {
            lower_ir_set_error(error,
                0,
                0,
                "LOWER-IR-VERIFY-033: global.%zu is missing a source name",
                i);
            return 0;
        }
        if (global->has_initializer && global->has_runtime_initializer) {
            lower_ir_set_error(error,
                0,
                0,
                "LOWER-IR-VERIFY-034: global.%zu has both constant and runtime initializer flags set",
                i);
            return 0;
        }
        for (j = i + 1; j < program->global_count; ++j) {
            const LowerIrGlobal *other = &program->globals[j];

            if (other->name && strcmp(global->name, other->name) == 0) {
                lower_ir_set_error(error,
                    0,
                    0,
                    "LOWER-IR-VERIFY-049: program contains duplicate global entry '%s'",
                    global->name);
                return 0;
            }
        }
    }

    for (i = 0; i < program->function_count; ++i) {
        const LowerIrFunction *function = &program->functions[i];

        if (!function->name || function->name[0] == '\0') {
            lower_ir_set_error(error,
                0,
                0,
                "LOWER-IR-VERIFY-028: function %zu has empty name",
                i);
            return 0;
        }
        for (j = i + 1; j < program->function_count; ++j) {
            const LowerIrFunction *other = &program->functions[j];

            if (other->name && strcmp(function->name, other->name) == 0) {
                lower_ir_set_error(error,
                    0,
                    0,
                    "LOWER-IR-VERIFY-048: program contains duplicate function entry '%s'",
                    function->name);
                return 0;
            }
        }
        for (j = 0; j < program->global_count; ++j) {
            const LowerIrGlobal *global = &program->globals[j];

            if (global->name && strcmp(function->name, global->name) == 0) {
                lower_ir_set_error(error,
                    0,
                    0,
                    "LOWER-IR-VERIFY-050: symbol name '%s' is used by both function and global entry",
                    function->name);
                return 0;
            }
        }

        if (function->local_count > 0 && !function->locals) {
            lower_ir_set_error(error,
                0,
                0,
                "LOWER-IR-VERIFY-053: function '%s' has local_count=%zu but local table is NULL",
                function->name,
                function->local_count);
            return 0;
        }
        if (function->local_count > function->local_capacity) {
            lower_ir_set_error(error,
                0,
                0,
                "LOWER-IR-VERIFY-067: function '%s' has local_count=%zu but local_capacity=%zu",
                function->name,
                function->local_count,
                function->local_capacity);
            return 0;
        }
        if (function->block_count > 0 && !function->blocks) {
            lower_ir_set_error(error,
                0,
                0,
                "LOWER-IR-VERIFY-054: function '%s' has block_count=%zu but block table is NULL",
                function->name,
                function->block_count);
            return 0;
        }
        if (function->block_count > function->block_capacity) {
            lower_ir_set_error(error,
                0,
                0,
                "LOWER-IR-VERIFY-068: function '%s' has block_count=%zu but block_capacity=%zu",
                function->name,
                function->block_count,
                function->block_capacity);
            return 0;
        }
        if (function->parameter_count > function->local_count) {
            lower_ir_set_error(error,
                0,
                0,
                "LOWER-IR-VERIFY-061: function '%s' declares %zu parameters but only has %zu locals",
                function->name,
                function->parameter_count,
                function->local_count);
            return 0;
        }

        for (j = 0; j < function->local_count; ++j) {
            if (function->locals[j].id != j) {
                lower_ir_set_error(error,
                    0,
                    0,
                    "LOWER-IR-VERIFY-031: function '%s' local table is not densely indexed at %zu",
                    function->name,
                    j);
                return 0;
            }
            if (!!function->locals[j].is_parameter != (j < function->parameter_count)) {
                lower_ir_set_error(error,
                    0,
                    0,
                    "LOWER-IR-VERIFY-062: function '%s' local.%zu parameter shape does not match parameter prefix",
                    function->name,
                    j);
                return 0;
            }
        }

        if (!function->has_body) {
            if (function->block_count != 0) {
                lower_ir_set_error(error,
                    0,
                    0,
                    "LOWER-IR-VERIFY-029: declaration '%s' must not own blocks",
                    function->name);
                return 0;
            }
            if (!lower_ir_function_has_reserved_internal_init_name(function) &&
                function->local_count != function->parameter_count) {
                lower_ir_set_error(error,
                    0,
                    0,
                    "LOWER-IR-VERIFY-063: declaration '%s' must not own non-parameter locals",
                    function->name);
                return 0;
            }
            if (!lower_ir_function_has_reserved_internal_init_name(function) &&
                function->next_temp_id != 0) {
                lower_ir_set_error(error,
                    0,
                    0,
                    "LOWER-IR-VERIFY-064: declaration '%s' must not allocate temps",
                    function->name);
                return 0;
            }
            continue;
        }

        if (function->block_count == 0) {
            lower_ir_set_error(error,
                0,
                0,
                "LOWER-IR-VERIFY-030: function '%s' has body but no blocks",
                function->name);
            return 0;
        }

        for (j = 0; j < function->block_count; ++j) {
            const LowerIrBasicBlock *block = &function->blocks[j];
            size_t k;

            if (block->id != j) {
                lower_ir_set_error(error,
                    0,
                    0,
                    "LOWER-IR-VERIFY-032: function '%s' block table is not densely indexed at %zu",
                    function->name,
                    j);
                return 0;
            }

            if (block->instruction_count > 0 && !block->instructions) {
                lower_ir_set_error(error,
                    0,
                    0,
                    "LOWER-IR-VERIFY-060: bb.%zu has instruction_count=%zu but instruction table is NULL",
                    block->id,
                    block->instruction_count);
                return 0;
            }
            if (block->instruction_count > block->instruction_capacity) {
                lower_ir_set_error(error,
                    0,
                    0,
                    "LOWER-IR-VERIFY-069: bb.%zu has instruction_count=%zu but instruction_capacity=%zu",
                    block->id,
                    block->instruction_count,
                    block->instruction_capacity);
                return 0;
            }

            for (k = 0; k < block->instruction_count; ++k) {
                if (!lower_ir_verify_instruction(program, function, block, k, &block->instructions[k], error)) {
                    return 0;
                }
                if (!lower_ir_verify_internal_initializer_call_policy(function,
                        block,
                        k,
                        &block->instructions[k],
                        error)) {
                    return 0;
                }
            }

            if (!lower_ir_verify_terminator(function, block, error)) {
                return 0;
            }
        }

        if (!lower_ir_verify_temp_definitions(function, error) ||
            !lower_ir_verify_temp_availability(function, error) ||
            !lower_ir_verify_reachability(function, error)) {
            return 0;
        }
    }

    return lower_ir_verify_runtime_global_initializer_contract(program, error);
}

#endif
