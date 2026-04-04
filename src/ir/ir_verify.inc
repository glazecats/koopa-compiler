#if defined(__INTELLISENSE__) && !defined(IR_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(IR_SPLIT_AGGREGATOR)
#define IR_SPLIT_INTELLISENSE_CONTEXT
#include "ir.c"
#else

static int ir_verify_value_ref(const IrProgram *program,
    const IrFunction *function,
    IrValueRef value,
    const char *role,
    IrError *error) {
    if (!function) {
        ir_set_error(error, 0, 0, "IR-VERIFY-001: invalid verifier contract");
        return 0;
    }

    switch (value.kind) {
    case IR_VALUE_IMMEDIATE:
        return 1;
    case IR_VALUE_LOCAL:
        if (value.id >= function->local_count) {
            ir_set_error(error,
                0,
                0,
                "IR-VERIFY-002: %s references local.%zu outside local table",
                role ? role : "value",
                value.id);
            return 0;
        }
        return 1;
    case IR_VALUE_GLOBAL:
        if (!program || value.id >= program->global_count) {
            ir_set_error(error,
                0,
                0,
                "IR-VERIFY-043: %s references global.%zu outside global table",
                role ? role : "value",
                value.id);
            return 0;
        }
        return 1;
    case IR_VALUE_TEMP:
        if (value.id >= function->next_temp_id) {
            ir_set_error(error,
                0,
                0,
                "IR-VERIFY-003: %s references tmp.%zu outside temp range",
                role ? role : "value",
                value.id);
            return 0;
        }
        return 1;
    default:
        ir_set_error(error,
            0,
            0,
            "IR-VERIFY-004: %s uses unknown value kind %d",
            role ? role : "value",
            (int)value.kind);
        return 0;
    }
}

static int ir_verify_block_targets(const IrProgram *program,
    const IrFunction *function,
    const IrBasicBlock *block,
    IrError *error) {
    if (!function || !block) {
        ir_set_error(error, 0, 0, "IR-VERIFY-001: invalid verifier contract");
        return 0;
    }

    if (!block->has_terminator) {
        ir_set_error(error,
            0,
            0,
            "IR-VERIFY-005: bb.%zu is missing a terminator",
            block->id);
        return 0;
    }

    switch (block->terminator.kind) {
    case IR_TERM_RETURN:
        return ir_verify_value_ref(program,
            function,
            block->terminator.as.return_value,
            "return terminator",
            error);
    case IR_TERM_JUMP:
        if (block->terminator.as.jump_target >= function->block_count) {
            ir_set_error(error,
                0,
                0,
                "IR-VERIFY-006: bb.%zu jumps to missing block bb.%zu",
                block->id,
                block->terminator.as.jump_target);
            return 0;
        }
        return 1;
    case IR_TERM_BRANCH:
        if (!ir_verify_value_ref(program,
            function,
                block->terminator.as.branch.condition,
                "branch condition",
                error)) {
            return 0;
        }
        if (block->terminator.as.branch.then_target >= function->block_count ||
            block->terminator.as.branch.else_target >= function->block_count) {
            ir_set_error(error,
                0,
                0,
                "IR-VERIFY-007: bb.%zu branches to missing successor",
                block->id);
            return 0;
        }
        return 1;
    default:
        ir_set_error(error,
            0,
            0,
            "IR-VERIFY-008: bb.%zu has unknown terminator kind %d",
            block->id,
            (int)block->terminator.kind);
        return 0;
    }
}

static int ir_verify_binary_op(IrBinaryOp op, size_t block_id, size_t instruction_index, IrError *error) {
    switch (op) {
    case IR_BINARY_ADD:
    case IR_BINARY_SUB:
    case IR_BINARY_MUL:
    case IR_BINARY_DIV:
    case IR_BINARY_MOD:
    case IR_BINARY_BIT_AND:
    case IR_BINARY_BIT_XOR:
    case IR_BINARY_BIT_OR:
    case IR_BINARY_SHIFT_LEFT:
    case IR_BINARY_SHIFT_RIGHT:
    case IR_BINARY_EQ:
    case IR_BINARY_NE:
    case IR_BINARY_LT:
    case IR_BINARY_LE:
    case IR_BINARY_GT:
    case IR_BINARY_GE:
        return 1;
    default:
        ir_set_error(error,
            0,
            0,
            "IR-VERIFY-020: bb.%zu instruction %zu uses unknown binary op %d",
            block_id,
            instruction_index,
            (int)op);
        return 0;
    }
}

static const IrFunction *ir_find_program_function(const IrProgram *program, const char *name) {
    size_t i;

    if (!program || !name || name[0] == '\0') {
        return NULL;
    }

    for (i = 0; i < program->function_count; ++i) {
        const IrFunction *function = &program->functions[i];

        if (function->name && strcmp(function->name, name) == 0) {
            return function;
        }
    }

    return NULL;
}

static int ir_instruction_is_zero_arg_call_to(const IrInstruction *instruction, const char *callee_name) {
    if (!instruction || !callee_name || callee_name[0] == '\0') {
        return 0;
    }
    if (instruction->kind != IR_INSTR_CALL) {
        return 0;
    }
    if (instruction->as.call.arg_count != 0 || !instruction->as.call.callee_name) {
        return 0;
    }

    return strcmp(instruction->as.call.callee_name, callee_name) == 0;
}

static int ir_entry_block_starts_with_call_to(const IrFunction *function, const char *callee_name) {
    const IrBasicBlock *entry_block;

    if (!function || !function->has_body || function->block_count == 0 || !function->blocks) {
        return 0;
    }

    entry_block = &function->blocks[0];
    if (entry_block->instruction_count == 0 || !entry_block->instructions) {
        return 0;
    }

    return ir_instruction_is_zero_arg_call_to(&entry_block->instructions[0], callee_name);
}

static int ir_verify_internal_initializer_call_policy(const IrFunction *caller,
    const IrBasicBlock *block,
    size_t instruction_index,
    const IrInstruction *instruction,
    IrError *error) {
    static const char *k_runtime_init_name = "__global.init";
    static const char *k_program_init_name = "__program.init";
    int is_runtime_init_call;
    int is_program_init_call;
    int allowed_runtime_init_callsite;

    if (!caller || !block || !instruction) {
        ir_set_error(error, 0, 0, "IR-VERIFY-001: invalid verifier contract");
        return 0;
    }

    if (instruction->kind != IR_INSTR_CALL ||
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
        ir_set_error(error,
            0,
            0,
            "IR-VERIFY-060: bb.%zu instruction %zu calls reserved internal startup helper '%s' from '%s'",
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
        ir_set_error(error,
            0,
            0,
            "IR-VERIFY-059: bb.%zu instruction %zu calls reserved internal init helper '%s' from non-startup entry site in '%s'",
            block->id,
            instruction_index,
            k_runtime_init_name,
            caller->name ? caller->name : "<anon>");
        return 0;
    }

    return 1;
}

static int ir_return_terminator_is_zero_immediate(const IrTerminator *terminator) {
    if (!terminator || terminator->kind != IR_TERM_RETURN) {
        return 0;
    }

    return terminator->as.return_value.kind == IR_VALUE_IMMEDIATE &&
        terminator->as.return_value.immediate == 0;
}

static int ir_verify_internal_init_function_shape(const IrFunction *function,
    const char *name,
    IrError *error) {
    const IrBasicBlock *entry_block;

    if (!function || !name || name[0] == '\0') {
        ir_set_error(error, 0, 0, "IR-VERIFY-001: invalid verifier contract");
        return 0;
    }

    if (function->parameter_count != 0 || function->local_count != 0) {
        ir_set_error(error,
            0,
            0,
            "IR-VERIFY-051: internal initializer function '%s' must have zero parameters and zero locals",
            name);
        return 0;
    }

    if (!function->has_body || function->block_count != 1 || !function->blocks) {
        ir_set_error(error,
            0,
            0,
            "IR-VERIFY-052: internal initializer function '%s' must have exactly one body block",
            name);
        return 0;
    }

    entry_block = &function->blocks[0];
    if (!entry_block->has_terminator ||
        !ir_return_terminator_is_zero_immediate(&entry_block->terminator)) {
        ir_set_error(error,
            0,
            0,
            "IR-VERIFY-053: internal initializer function '%s' entry block must terminate with 'ret 0'",
            name);
        return 0;
    }

    return 1;
}

static int ir_verify_runtime_global_initializer_contract(const IrProgram *program, IrError *error) {
    static const char *k_runtime_init_name = "__global.init";
    static const char *k_program_init_name = "__program.init";
    const IrFunction *runtime_init_function;
    const IrFunction *main_function;
    const IrFunction *program_init_function;
    int has_runtime_global_initializer = 0;
    int has_startup_call = 0;
    size_t i;

    if (!program) {
        ir_set_error(error, 0, 0, "IR-VERIFY-001: invalid verifier contract");
        return 0;
    }

    for (i = 0; i < program->global_count; ++i) {
        if (program->globals[i].has_runtime_initializer) {
            has_runtime_global_initializer = 1;
            break;
        }
    }

    runtime_init_function = ir_find_program_function(program, k_runtime_init_name);
    program_init_function = ir_find_program_function(program, k_program_init_name);

    if (!has_runtime_global_initializer &&
        (runtime_init_function || program_init_function)) {
        ir_set_error(error,
            0,
            0,
            "IR-VERIFY-056: internal initializer helper names ('%s'/'%s') are reserved for runtime global initialization only",
            k_runtime_init_name,
            k_program_init_name);
        return 0;
    }

    if (runtime_init_function &&
        !ir_verify_internal_init_function_shape(runtime_init_function,
            k_runtime_init_name,
            error)) {
        return 0;
    }
    if (program_init_function &&
        !ir_verify_internal_init_function_shape(program_init_function,
            k_program_init_name,
            error)) {
        return 0;
    }

    if (program_init_function) {
        const IrBasicBlock *program_init_entry = &program_init_function->blocks[0];

        if (program_init_entry->instruction_count != 1 ||
            !program_init_entry->instructions) {
            ir_set_error(error,
                0,
                0,
                "IR-VERIFY-055: internal initializer function '%s' must contain exactly one entry instruction before 'ret 0'",
                k_program_init_name);
            return 0;
        }

        if (!ir_instruction_is_zero_arg_call_to(&program_init_entry->instructions[0],
                k_runtime_init_name)) {
            ir_set_error(error,
                0,
                0,
                "IR-VERIFY-054: internal initializer function '%s' must begin with zero-arg call to '%s'",
                k_program_init_name,
                k_runtime_init_name);
            return 0;
        }
    }

    if (!has_runtime_global_initializer) {
        return 1;
    }

    if (!runtime_init_function) {
        ir_set_error(error,
            0,
            0,
            "IR-VERIFY-049: runtime global initializers require lowered helper function '%s'",
            k_runtime_init_name);
        return 0;
    }

    main_function = ir_find_program_function(program, "main");
    if (ir_entry_block_starts_with_call_to(main_function, k_runtime_init_name)) {
        has_startup_call = 1;
    }

    if (!has_startup_call) {
        if (ir_entry_block_starts_with_call_to(program_init_function, k_runtime_init_name)) {
            has_startup_call = 1;
        }
    }

    if (!has_startup_call) {
        ir_set_error(error,
            0,
            0,
            "IR-VERIFY-050: runtime global initializers require startup entry call to '%s' from main or '%s'",
            k_runtime_init_name,
            k_program_init_name);
        return 0;
    }

    return 1;
}

static int ir_verify_program_function_names(const IrProgram *program, IrError *error) {
    size_t i;
    size_t j;

    if (!program) {
        ir_set_error(error, 0, 0, "IR-VERIFY-001: invalid verifier contract");
        return 0;
    }

    for (i = 0; i < program->function_count; ++i) {
        const IrFunction *lhs = &program->functions[i];

        for (j = i + 1; j < program->function_count; ++j) {
            const IrFunction *rhs = &program->functions[j];

            if (lhs->name && rhs->name && strcmp(lhs->name, rhs->name) == 0) {
                ir_set_error(error,
                    0,
                    0,
                    "IR-VERIFY-027: program contains duplicate function entry '%s'",
                    lhs->name);
                return 0;
            }
        }
    }

    return 1;
}

static int ir_verify_program_global_names(const IrProgram *program, IrError *error) {
    size_t i;
    size_t j;

    if (!program) {
        ir_set_error(error, 0, 0, "IR-VERIFY-001: invalid verifier contract");
        return 0;
    }

    for (i = 0; i < program->global_count; ++i) {
        const IrGlobal *lhs = &program->globals[i];

        for (j = i + 1; j < program->global_count; ++j) {
            const IrGlobal *rhs = &program->globals[j];

            if (lhs->name && rhs->name && strcmp(lhs->name, rhs->name) == 0) {
                ir_set_error(error,
                    0,
                    0,
                    "IR-VERIFY-062: program contains duplicate global entry '%s'",
                    lhs->name);
                return 0;
            }
        }
    }

    return 1;
}

static int ir_verify_program_symbol_namespace_consistency(const IrProgram *program,
    IrError *error) {
    size_t i;
    size_t j;

    if (!program) {
        ir_set_error(error, 0, 0, "IR-VERIFY-001: invalid verifier contract");
        return 0;
    }

    for (i = 0; i < program->function_count; ++i) {
        const IrFunction *function = &program->functions[i];

        if (!function->name || function->name[0] == '\0') {
            continue;
        }

        for (j = 0; j < program->global_count; ++j) {
            const IrGlobal *global = &program->globals[j];

            if (global->name && strcmp(function->name, global->name) == 0) {
                ir_set_error(error,
                    0,
                    0,
                    "IR-VERIFY-063: symbol name '%s' is used by both function and global entry",
                    function->name);
                return 0;
            }
        }
    }

    return 1;
}

static int ir_verify_temp_definitions(const IrFunction *function, IrError *error) {
    size_t *temp_definition_counts = NULL;
    size_t *temp_last_definition_block = NULL;
    size_t i;

    if (!function) {
        ir_set_error(error, 0, 0, "IR-VERIFY-001: invalid verifier contract");
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
        ir_set_error(error, 0, 0, "IR-VERIFY-022: out of memory while verifying temp definitions");
        return 0;
    }
    for (i = 0; i < function->next_temp_id; ++i) {
        temp_last_definition_block[i] = (size_t)-1;
    }

    for (i = 0; i < function->block_count; ++i) {
        const IrBasicBlock *block = &function->blocks[i];
        size_t j;

        if (block->instruction_count > 0 && !block->instructions) {
            free(temp_definition_counts);
            free(temp_last_definition_block);
            ir_set_error(error,
                0,
                0,
                "IR-VERIFY-036: bb.%zu has instruction_count=%zu but instruction table is NULL",
                block->id,
                block->instruction_count);
            return 0;
        }

        for (j = 0; j < block->instruction_count; ++j) {
            const IrInstruction *instruction = &block->instructions[j];

            if (instruction->result.kind == IR_VALUE_TEMP) {
                size_t temp_id = instruction->result.id;

                if (temp_last_definition_block[temp_id] == block->id) {
                    free(temp_definition_counts);
                    free(temp_last_definition_block);
                    ir_set_error(error,
                        0,
                        0,
                        "IR-VERIFY-031: function '%s' temp.%zu is defined multiple times in bb.%zu",
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
            ir_set_error(error,
                0,
                0,
                "IR-VERIFY-023: function '%s' temp.%zu has no defining instruction",
                function->name ? function->name : "<anon>",
                i);
            return 0;
        }
    }

    free(temp_definition_counts);
    free(temp_last_definition_block);
    return 1;
}

static int ir_verify_temp_use_is_available(IrValueRef value,
    const unsigned char *available,
    size_t temp_count,
    size_t block_id,
    const char *role,
    IrError *error) {
    if (value.kind != IR_VALUE_TEMP) {
        return 1;
    }
    if (!available || value.id >= temp_count || !available[value.id]) {
        ir_set_error(error,
            0,
            0,
            "IR-VERIFY-024: bb.%zu uses tmp.%zu before it is defined on all incoming paths (%s)",
            block_id,
            value.id,
            role ? role : "value");
        return 0;
    }
    return 1;
}

static int ir_verify_temp_availability(const IrFunction *function, IrError *error) {
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
        ir_set_error(error, 0, 0, "IR-VERIFY-001: invalid verifier contract");
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
        ir_set_error(error, 0, 0, "IR-VERIFY-025: out of memory while verifying temp availability");
        return 0;
    }

    for (i = 0; i < block_count; ++i) {
        const IrBasicBlock *block = &function->blocks[i];
        size_t j;

        for (j = 0; j < block->instruction_count; ++j) {
            const IrInstruction *instruction = &block->instructions[j];

            if (instruction->result.kind == IR_VALUE_TEMP) {
                block_defs[i * temp_count + instruction->result.id] = 1;
            }
        }

        switch (block->terminator.kind) {
        case IR_TERM_JUMP:
            edges[i * block_count + block->terminator.as.jump_target] = 1;
            break;
        case IR_TERM_BRANCH:
            edges[i * block_count + block->terminator.as.branch.then_target] = 1;
            edges[i * block_count + block->terminator.as.branch.else_target] = 1;
            break;
        case IR_TERM_RETURN:
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
        const IrBasicBlock *block = &function->blocks[i];
        size_t j;

        memcpy(available, &in_sets[i * temp_count], temp_count * sizeof(unsigned char));
        for (j = 0; j < block->instruction_count; ++j) {
            const IrInstruction *instruction = &block->instructions[j];

            switch (instruction->kind) {
            case IR_INSTR_MOV:
                if (!ir_verify_temp_use_is_available(instruction->as.mov_value,
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
            case IR_INSTR_BINARY:
                if (!ir_verify_temp_use_is_available(instruction->as.binary.lhs,
                        available,
                        temp_count,
                        block->id,
                        "binary lhs",
                        error) ||
                    !ir_verify_temp_use_is_available(instruction->as.binary.rhs,
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
            case IR_INSTR_CALL:
                for (size_t arg_index = 0; arg_index < instruction->as.call.arg_count; ++arg_index) {
                    if (!ir_verify_temp_use_is_available(instruction->as.call.args[arg_index],
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
            default:
                break;
            }

            if (instruction->result.kind == IR_VALUE_TEMP) {
                available[instruction->result.id] = 1;
            }
        }

        switch (block->terminator.kind) {
        case IR_TERM_RETURN:
            if (!ir_verify_temp_use_is_available(block->terminator.as.return_value,
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
        case IR_TERM_BRANCH:
            if (!ir_verify_temp_use_is_available(block->terminator.as.branch.condition,
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
        case IR_TERM_JUMP:
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

static int ir_verify_reachability(const IrFunction *function, IrError *error) {
    unsigned char *reachable = NULL;
    size_t *pending = NULL;
    size_t pending_count = 0;
    size_t i;

    if (!function) {
        ir_set_error(error, 0, 0, "IR-VERIFY-001: invalid verifier contract");
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
        ir_set_error(error, 0, 0, "IR-VERIFY-009: out of memory while verifying reachability");
        return 0;
    }

    pending[pending_count++] = 0;
    reachable[0] = 1;
    while (pending_count > 0) {
        size_t block_id = pending[--pending_count];
        const IrBasicBlock *block = &function->blocks[block_id];

        switch (block->terminator.kind) {
        case IR_TERM_JUMP:
            if (!reachable[block->terminator.as.jump_target]) {
                reachable[block->terminator.as.jump_target] = 1;
                pending[pending_count++] = block->terminator.as.jump_target;
            }
            break;
        case IR_TERM_BRANCH:
            if (!reachable[block->terminator.as.branch.then_target]) {
                reachable[block->terminator.as.branch.then_target] = 1;
                pending[pending_count++] = block->terminator.as.branch.then_target;
            }
            if (!reachable[block->terminator.as.branch.else_target]) {
                reachable[block->terminator.as.branch.else_target] = 1;
                pending[pending_count++] = block->terminator.as.branch.else_target;
            }
            break;
        case IR_TERM_RETURN:
        default:
            break;
        }
    }

    for (i = 0; i < function->block_count; ++i) {
        if (!reachable[i]) {
            free(reachable);
            free(pending);
            ir_set_error(error,
                0,
                0,
                "IR-VERIFY-010: function '%s' contains unreachable block bb.%zu",
                function->name ? function->name : "<anon>",
                i);
            return 0;
        }
    }

    free(reachable);
    free(pending);
    return 1;
}

static int ir_verify_function(const IrProgram *program, const IrFunction *function, IrError *error) {
    size_t i;
    size_t parameter_count = 0;

    if (!program || !function) {
        ir_set_error(error, 0, 0, "IR-VERIFY-001: invalid verifier contract");
        return 0;
    }
    if (!function->name || function->name[0] == '\0') {
        ir_set_error(error, 0, 0, "IR-VERIFY-011: function is missing a name");
        return 0;
    }
    if (function->local_count > function->local_capacity) {
        ir_set_error(error,
            0,
            0,
            "IR-VERIFY-038: function '%s' has local_count=%zu exceeding local_capacity=%zu",
            function->name,
            function->local_count,
            function->local_capacity);
        return 0;
    }
    if (function->block_count > function->block_capacity) {
        ir_set_error(error,
            0,
            0,
            "IR-VERIFY-039: function '%s' has block_count=%zu exceeding block_capacity=%zu",
            function->name,
            function->block_count,
            function->block_capacity);
        return 0;
    }
    if (function->local_count > 0 && !function->locals) {
        ir_set_error(error,
            0,
            0,
            "IR-VERIFY-034: function '%s' has local_count=%zu but local table is NULL",
            function->name,
            function->local_count);
        return 0;
    }
    if (function->block_count > 0 && !function->blocks) {
        ir_set_error(error,
            0,
            0,
            "IR-VERIFY-035: function '%s' has block_count=%zu but block table is NULL",
            function->name,
            function->block_count);
        return 0;
    }
    if (!function->has_body) {
        if (function->block_count != 0) {
            ir_set_error(error,
                0,
                0,
                "IR-VERIFY-028: declaration-only function '%s' must not contain basic blocks",
                function->name);
            return 0;
        }
        if (function->next_temp_id != 0) {
            ir_set_error(error,
                0,
                0,
                "IR-VERIFY-029: declaration-only function '%s' must not allocate temporaries",
                function->name);
            return 0;
        }
    } else if (function->block_count == 0) {
        ir_set_error(error,
            0,
            0,
            "IR-VERIFY-012: function '%s' has no basic blocks",
            function->name);
        return 0;
    }

    for (i = 0; i < function->local_count; ++i) {
        const IrLocal *local = &function->locals[i];

        if (local->id != i) {
            ir_set_error(error,
                0,
                0,
                "IR-VERIFY-013: function '%s' local slot %zu stores mismatched id %zu",
                function->name,
                i,
                local->id);
            return 0;
        }
        if (!local->source_name || local->source_name[0] == '\0') {
            ir_set_error(error,
                0,
                0,
                "IR-VERIFY-014: function '%s' local.%zu is missing a source name",
                function->name,
                i);
            return 0;
        }
        if (local->is_parameter) {
            if (i != parameter_count) {
                ir_set_error(error,
                    0,
                    0,
                    "IR-VERIFY-015: function '%s' parameter locals must occupy a prefix",
                    function->name);
                return 0;
            }
            parameter_count += 1;
        }
    }
    if (parameter_count != function->parameter_count) {
        ir_set_error(error,
            0,
            0,
            "IR-VERIFY-016: function '%s' parameter count metadata is inconsistent",
            function->name);
        return 0;
    }
    if (!function->has_body && function->local_count != function->parameter_count) {
        ir_set_error(error,
            0,
            0,
            "IR-VERIFY-030: declaration-only function '%s' contains non-parameter locals",
            function->name);
        return 0;
    }

    if (!function->has_body) {
        return 1;
    }

    for (i = 0; i < function->block_count; ++i) {
        const IrBasicBlock *block = &function->blocks[i];

        if (block->instruction_count > block->instruction_capacity) {
            ir_set_error(error,
                0,
                0,
                "IR-VERIFY-040: bb.%zu has instruction_count=%zu exceeding instruction_capacity=%zu",
                block->id,
                block->instruction_count,
                block->instruction_capacity);
            return 0;
        }
        if (block->instruction_count > 0 && !block->instructions) {
            ir_set_error(error,
                0,
                0,
                "IR-VERIFY-036: bb.%zu has instruction_count=%zu but instruction table is NULL",
                block->id,
                block->instruction_count);
            return 0;
        }
    }

    if (!ir_verify_temp_definitions(function, error)) {
        return 0;
    }

    for (i = 0; i < function->block_count; ++i) {
        const IrBasicBlock *block = &function->blocks[i];
        size_t j;

        if (block->instruction_count > 0 && !block->instructions) {
            ir_set_error(error,
                0,
                0,
                "IR-VERIFY-036: bb.%zu has instruction_count=%zu but instruction table is NULL",
                block->id,
                block->instruction_count);
            return 0;
        }

        if (block->id != i) {
            ir_set_error(error,
                0,
                0,
                "IR-VERIFY-017: function '%s' block slot %zu stores mismatched id %zu",
                function->name,
                i,
                block->id);
            return 0;
        }

        for (j = 0; j < block->instruction_count; ++j) {
            const IrInstruction *instruction = &block->instructions[j];

            if (instruction->result.kind == IR_VALUE_IMMEDIATE) {
                ir_set_error(error,
                    0,
                    0,
                    "IR-VERIFY-018: bb.%zu instruction %zu writes into an immediate",
                    block->id,
                    j);
                return 0;
            }
            if (!ir_verify_value_ref(program,
                    function,
                    instruction->result,
                    "instruction result",
                    error)) {
                return 0;
            }

            switch (instruction->kind) {
            case IR_INSTR_MOV:
                if (!ir_verify_value_ref(program,
                        function,
                        instruction->as.mov_value,
                        "mov operand",
                        error)) {
                    return 0;
                }
                break;
            case IR_INSTR_BINARY:
                if (instruction->result.kind != IR_VALUE_TEMP) {
                    ir_set_error(error,
                        0,
                        0,
                        "IR-VERIFY-057: bb.%zu instruction %zu binary result must be a temporary",
                        block->id,
                        j);
                    return 0;
                }
                if (!ir_verify_binary_op(instruction->as.binary.op, block->id, j, error)) {
                    return 0;
                }
                if (!ir_verify_value_ref(program,
                        function,
                        instruction->as.binary.lhs,
                        "binary lhs",
                        error) ||
                    !ir_verify_value_ref(program,
                        function,
                        instruction->as.binary.rhs,
                        "binary rhs",
                        error)) {
                    return 0;
                }
                break;
            case IR_INSTR_CALL:
                {
                    const IrFunction *callee;

                if (instruction->result.kind != IR_VALUE_TEMP) {
                    ir_set_error(error,
                        0,
                        0,
                        "IR-VERIFY-058: bb.%zu instruction %zu call result must be a temporary",
                        block->id,
                        j);
                    return 0;
                }

                if (!instruction->as.call.callee_name || instruction->as.call.callee_name[0] == '\0') {
                    ir_set_error(error,
                        0,
                        0,
                        "IR-VERIFY-021: bb.%zu instruction %zu has an empty call callee",
                        block->id,
                        j);
                    return 0;
                }
                    callee = ir_find_program_function(program, instruction->as.call.callee_name);
                    if (!callee) {
                        ir_set_error(error,
                            0,
                            0,
                            "IR-VERIFY-032: bb.%zu instruction %zu calls unknown function '%s'",
                            block->id,
                            j,
                            instruction->as.call.callee_name);
                        return 0;
                    }
                    if (callee && instruction->as.call.arg_count != callee->parameter_count) {
                        ir_set_error(error,
                            0,
                            0,
                            "IR-VERIFY-026: bb.%zu instruction %zu calls '%s' with %zu args but function expects %zu",
                            block->id,
                            j,
                            instruction->as.call.callee_name,
                            instruction->as.call.arg_count,
                            callee->parameter_count);
                        return 0;
                    }
                    if (!ir_verify_internal_initializer_call_policy(function,
                            block,
                            j,
                            instruction,
                            error)) {
                        return 0;
                    }
                    if (instruction->as.call.arg_count > 0 && !instruction->as.call.args) {
                        ir_set_error(error,
                            0,
                            0,
                            "IR-VERIFY-033: bb.%zu instruction %zu has arg_count=%zu but call args payload is NULL",
                            block->id,
                            j,
                            instruction->as.call.arg_count);
                        return 0;
                    }
                    if (instruction->as.call.arg_count == 0 && instruction->as.call.args) {
                        ir_set_error(error,
                            0,
                            0,
                            "IR-VERIFY-037: bb.%zu instruction %zu has arg_count=0 but call args payload is non-NULL",
                            block->id,
                            j);
                        return 0;
                    }
                for (size_t arg_index = 0; arg_index < instruction->as.call.arg_count; ++arg_index) {
                    if (!ir_verify_value_ref(program,
                        function,
                            instruction->as.call.args[arg_index],
                            "call argument",
                            error)) {
                        return 0;
                    }
                }
                break;
                }
            default:
                ir_set_error(error,
                    0,
                    0,
                    "IR-VERIFY-019: bb.%zu instruction %zu uses unknown kind %d",
                    block->id,
                    j,
                    (int)instruction->kind);
                return 0;
            }
        }

        if (!ir_verify_block_targets(program, function, block, error)) {
            return 0;
        }
    }

    if (!ir_verify_temp_availability(function, error)) {
        return 0;
    }

    return ir_verify_reachability(function, error);
}

int ir_verify_program(const IrProgram *program, IrError *error) {
    size_t i;

    if (!program) {
        ir_set_error(error, 0, 0, "IR-VERIFY-001: invalid verifier contract");
        return 0;
    }

    if (program->function_count > program->function_capacity) {
        ir_set_error(error,
            0,
            0,
            "IR-VERIFY-041: program has function_count=%zu exceeding function_capacity=%zu",
            program->function_count,
            program->function_capacity);
        return 0;
    }
    if (program->function_count > 0 && !program->functions) {
        ir_set_error(error,
            0,
            0,
            "IR-VERIFY-042: program has function_count=%zu but function table is NULL",
            program->function_count);
        return 0;
    }
    if (program->global_count > program->global_capacity) {
        ir_set_error(error,
            0,
            0,
            "IR-VERIFY-044: program has global_count=%zu exceeding global_capacity=%zu",
            program->global_count,
            program->global_capacity);
        return 0;
    }
    if (program->global_count > 0 && !program->globals) {
        ir_set_error(error,
            0,
            0,
            "IR-VERIFY-045: program has global_count=%zu but global table is NULL",
            program->global_count);
        return 0;
    }

    if (error) {
        error->line = 0;
        error->column = 0;
        error->message[0] = '\0';
    }

    if (!ir_verify_program_function_names(program, error)) {
        return 0;
    }

    for (i = 0; i < program->global_count; ++i) {
        const IrGlobal *global = &program->globals[i];

        if (global->id != i) {
            ir_set_error(error,
                0,
                0,
                "IR-VERIFY-046: global slot %zu stores mismatched id %zu",
                i,
                global->id);
            return 0;
        }
        if (!global->name || global->name[0] == '\0') {
            ir_set_error(error,
                0,
                0,
                "IR-VERIFY-047: global.%zu is missing a source name",
                i);
            return 0;
        }
        if (global->has_initializer && global->has_runtime_initializer) {
            ir_set_error(error,
                0,
                0,
                "IR-VERIFY-048: global.%zu has both constant and runtime initializer flags set",
                i);
            return 0;
        }
    }

    if (!ir_verify_program_global_names(program, error)) {
        return 0;
    }

    if (!ir_verify_program_symbol_namespace_consistency(program, error)) {
        return 0;
    }

    if (!ir_verify_runtime_global_initializer_contract(program, error)) {
        return 0;
    }

    for (i = 0; i < program->function_count; ++i) {
        if (!ir_verify_function(program, &program->functions[i], error)) {
            return 0;
        }
    }

    return 1;
}

#endif