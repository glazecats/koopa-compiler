#if defined(__INTELLISENSE__) && !defined(IR_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(IR_SPLIT_AGGREGATOR)
#define IR_SPLIT_INTELLISENSE_CONTEXT
#include "ir.c"
#else

#define IR_RUNTIME_GLOBAL_INIT_FUNCTION_NAME "__global.init"
#define IR_PROGRAM_INIT_FUNCTION_NAME "__program.init"

#include <limits.h>

static int ir_function_parameter_count_matches_external(const IrFunction *function,
    const AstExternal *external) {
    return function && external && function->parameter_count == external->parameter_count;
}

static int ir_function_set_parameter_name(IrFunction *function,
    size_t parameter_index,
    const char *parameter_name,
    IrError *error) {
    IrLocal *local;
    char *name_copy;
    size_t name_length;

    if (!function || !parameter_name || parameter_name[0] == '\0') {
        return 1;
    }
    if (parameter_index >= function->parameter_count || parameter_index >= function->local_count) {
        ir_set_error(error, 0, 0, "IR-INT-030: invalid parameter rename target");
        return 0;
    }

    local = &function->locals[parameter_index];
    if (local->source_name && strcmp(local->source_name, parameter_name) == 0) {
        return 1;
    }

    name_length = strlen(parameter_name);
    name_copy = (char *)malloc(name_length + 1);
    if (!name_copy) {
        ir_set_error(error,
            0,
            0,
            "IR-INT-031: out of memory while renaming function parameter");
        return 0;
    }

    memcpy(name_copy, parameter_name, name_length + 1);
    free(local->source_name);
    local->source_name = name_copy;
    return 1;
}

static int ir_predeclare_program_globals(IrProgram *program,
    const AstProgram *ast_program,
    IrError *error) {
    size_t i;

    if (!program || !ast_program) {
        return 1;
    }

    for (i = 0; i < ast_program->count; ++i) {
        const AstExternal *external = &ast_program->externals[i];

        if (external->kind != AST_EXTERNAL_DECLARATION || !external->name ||
            external->name[0] == '\0') {
            continue;
        }

        if (!ir_program_find_global_mut(program, external->name) &&
            !ir_program_append_global(program, external->name, NULL, error)) {
            return 0;
        }
    }

    return 1;
}

static int ir_eval_global_binary(TokenType op,
    long long lhs,
    long long rhs,
    long long *out_value,
    int line,
    int column,
    IrError *error,
    int *out_invalid_constant) {
    const long long bit_width = (long long)(sizeof(long long) * 8);

    if (!out_value) {
        return 0;
    }

    switch (op) {
    case TOKEN_PLUS:
        if (__builtin_add_overflow(lhs, rhs, out_value)) {
            if (out_invalid_constant) {
                *out_invalid_constant = 1;
            }
            return 0;
        }
        return 1;
    case TOKEN_MINUS:
        if (__builtin_sub_overflow(lhs, rhs, out_value)) {
            if (out_invalid_constant) {
                *out_invalid_constant = 1;
            }
            return 0;
        }
        return 1;
    case TOKEN_STAR:
        if (__builtin_mul_overflow(lhs, rhs, out_value)) {
            if (out_invalid_constant) {
                *out_invalid_constant = 1;
            }
            return 0;
        }
        return 1;
    case TOKEN_SLASH:
        if (rhs == 0) {
            ir_set_error(error,
                line,
                column,
                "IR-LOWER-018: top-level initializer division by zero is not allowed");
            return 0;
        }
        if (lhs == LLONG_MIN && rhs == -1) {
            if (out_invalid_constant) {
                *out_invalid_constant = 1;
            }
            return 0;
        }
        *out_value = lhs / rhs;
        return 1;
    case TOKEN_PERCENT:
        if (rhs == 0) {
            ir_set_error(error,
                line,
                column,
                "IR-LOWER-018: top-level initializer modulo by zero is not allowed");
            return 0;
        }
        if (lhs == LLONG_MIN && rhs == -1) {
            if (out_invalid_constant) {
                *out_invalid_constant = 1;
            }
            return 0;
        }
        *out_value = lhs % rhs;
        return 1;
    case TOKEN_SHIFT_LEFT:
        if (rhs < 0 || rhs >= bit_width) {
            ir_set_error(error,
                line,
                column,
                "IR-LOWER-019: top-level initializer shift count %lld is out of range",
                rhs);
            return 0;
        }
        if (lhs < 0) {
            if (out_invalid_constant) {
                *out_invalid_constant = 1;
            }
            return 0;
        }
        if (rhs > 0 && lhs > (LLONG_MAX >> rhs)) {
            if (out_invalid_constant) {
                *out_invalid_constant = 1;
            }
            return 0;
        }
        *out_value = lhs << rhs;
        return 1;
    case TOKEN_SHIFT_RIGHT:
        if (rhs < 0 || rhs >= bit_width) {
            if (out_invalid_constant) {
                *out_invalid_constant = 1;
            }
            ir_set_error(error,
                line,
                column,
                "IR-LOWER-019: top-level initializer shift count %lld is out of range",
                rhs);
            return 0;
        }
        *out_value = lhs >> rhs;
        return 1;
    case TOKEN_AMP:
        *out_value = lhs & rhs;
        return 1;
    case TOKEN_CARET:
        *out_value = lhs ^ rhs;
        return 1;
    case TOKEN_PIPE:
        *out_value = lhs | rhs;
        return 1;
    case TOKEN_EQ:
        *out_value = (lhs == rhs) ? 1 : 0;
        return 1;
    case TOKEN_NE:
        *out_value = (lhs != rhs) ? 1 : 0;
        return 1;
    case TOKEN_LT:
        *out_value = (lhs < rhs) ? 1 : 0;
        return 1;
    case TOKEN_LE:
        *out_value = (lhs <= rhs) ? 1 : 0;
        return 1;
    case TOKEN_GT:
        *out_value = (lhs > rhs) ? 1 : 0;
        return 1;
    case TOKEN_GE:
        *out_value = (lhs >= rhs) ? 1 : 0;
        return 1;
    case TOKEN_AND_AND:
        *out_value = (lhs != 0 && rhs != 0) ? 1 : 0;
        return 1;
    case TOKEN_OR_OR:
        *out_value = (lhs != 0 || rhs != 0) ? 1 : 0;
        return 1;
    default:
        ir_set_error(error,
            line,
            column,
            "IR-LOWER-018: top-level initializer operator '%s' is not supported",
            lexer_token_type_name(op));
        return 0;
    }
}

static int ir_eval_global_initializer_expr(const IrProgram *program,
    const AstExpression *expr,
    long long *out_value,
    IrError *error,
    int *out_invalid_constant) {
    long long lhs;
    long long rhs;

    if (!expr || !out_value) {
        return 0;
    }
    if (out_invalid_constant) {
        *out_invalid_constant = 0;
    }

    switch (expr->kind) {
    case AST_EXPR_NUMBER:
        *out_value = expr->as.number_value;
        return 1;
    case AST_EXPR_IDENTIFIER:
        {
            const IrGlobal *global;

            if (!program || !expr->as.identifier.name || expr->as.identifier.name[0] == '\0') {
                if (out_invalid_constant) {
                    *out_invalid_constant = 1;
                }
                return 0;
            }

            global = ir_program_find_global(program, expr->as.identifier.name);
            if (!global || !global->has_initializer) {
                if (out_invalid_constant) {
                    *out_invalid_constant = 1;
                }
                return 0;
            }

            *out_value = global->initializer_value;
            return 1;
        }
    case AST_EXPR_PAREN:
        return ir_eval_global_initializer_expr(program,
            expr->as.inner,
            out_value,
            error,
            out_invalid_constant);
    case AST_EXPR_UNARY:
        if (!ir_eval_global_initializer_expr(program,
                expr->as.unary.operand,
                &rhs,
                error,
                out_invalid_constant)) {
            return 0;
        }
        switch (expr->as.unary.op) {
        case TOKEN_PLUS:
            *out_value = rhs;
            return 1;
        case TOKEN_MINUS:
            if (rhs == LLONG_MIN) {
                if (out_invalid_constant) {
                    *out_invalid_constant = 1;
                }
                return 0;
            }
            *out_value = -rhs;
            return 1;
        case TOKEN_BANG:
            *out_value = rhs == 0 ? 1 : 0;
            return 1;
        case TOKEN_TILDE:
            *out_value = ~rhs;
            return 1;
        default:
            if (out_invalid_constant) {
                *out_invalid_constant = 1;
            }
            return 0;
        }
    case AST_EXPR_TERNARY:
        if (!ir_eval_global_initializer_expr(program,
                expr->as.ternary.condition,
                &lhs,
                error,
                out_invalid_constant)) {
            return 0;
        }
        return ir_eval_global_initializer_expr(program,
            lhs != 0 ? expr->as.ternary.then_expr : expr->as.ternary.else_expr,
            out_value,
            error,
            out_invalid_constant);
    case AST_EXPR_BINARY:
        if (expr->as.binary.op == TOKEN_COMMA) {
            if (!ir_eval_global_initializer_expr(program,
                    expr->as.binary.left,
                    &lhs,
                    error,
                    out_invalid_constant)) {
                return 0;
            }
            return ir_eval_global_initializer_expr(program,
                expr->as.binary.right,
                out_value,
                error,
                out_invalid_constant);
        }

        if (expr->as.binary.op == TOKEN_AND_AND) {
            if (!ir_eval_global_initializer_expr(program,
                    expr->as.binary.left,
                    &lhs,
                    error,
                    out_invalid_constant)) {
                return 0;
            }
            if (lhs == 0) {
                *out_value = 0;
                return 1;
            }
            if (!ir_eval_global_initializer_expr(program,
                    expr->as.binary.right,
                    &rhs,
                    error,
                    out_invalid_constant)) {
                return 0;
            }
            *out_value = rhs != 0 ? 1 : 0;
            return 1;
        }

        if (expr->as.binary.op == TOKEN_OR_OR) {
            if (!ir_eval_global_initializer_expr(program,
                    expr->as.binary.left,
                    &lhs,
                    error,
                    out_invalid_constant)) {
                return 0;
            }
            if (lhs != 0) {
                *out_value = 1;
                return 1;
            }
            if (!ir_eval_global_initializer_expr(program,
                    expr->as.binary.right,
                    &rhs,
                    error,
                    out_invalid_constant)) {
                return 0;
            }
            *out_value = rhs != 0 ? 1 : 0;
            return 1;
        }

        switch (expr->as.binary.op) {
        case TOKEN_ASSIGN:
        case TOKEN_PLUS_ASSIGN:
        case TOKEN_MINUS_ASSIGN:
        case TOKEN_STAR_ASSIGN:
        case TOKEN_SLASH_ASSIGN:
        case TOKEN_PERCENT_ASSIGN:
        case TOKEN_AMP_ASSIGN:
        case TOKEN_CARET_ASSIGN:
        case TOKEN_PIPE_ASSIGN:
        case TOKEN_SHIFT_LEFT_ASSIGN:
        case TOKEN_SHIFT_RIGHT_ASSIGN:
            if (out_invalid_constant) {
                *out_invalid_constant = 1;
            }
            return 0;
        default:
            break;
        }

        if (!ir_eval_global_initializer_expr(program,
                expr->as.binary.left,
                &lhs,
                error,
                out_invalid_constant) ||
            !ir_eval_global_initializer_expr(program,
                expr->as.binary.right,
                &rhs,
                error,
                out_invalid_constant)) {
            return 0;
        }
        return ir_eval_global_binary(expr->as.binary.op,
            lhs,
            rhs,
            out_value,
            expr->line,
            expr->column,
            error,
            out_invalid_constant);
    case AST_EXPR_POSTFIX:
    case AST_EXPR_CALL:
    default:
        if (out_invalid_constant) {
            *out_invalid_constant = 1;
        }
        return 0;
    }
}

static int ir_ensure_function_signature(IrProgram *program,
    const AstExternal *external,
    IrFunction **out_function,
    IrError *error) {
    IrFunction *function;
    size_t i;

    if (!program || !external || !external->name || external->name[0] == '\0') {
        ir_set_error(error, 0, 0, "IR-INT-058: invalid function signature lowering contract");
        return 0;
    }

    function = ir_program_find_function_mut(program, external->name);
    if (!function) {
        if (!ir_program_append_function(program, external->name, &function, error)) {
            return 0;
        }
    }

    if (function->local_count == 0 && function->parameter_count == 0 &&
        external->parameter_count > 0) {
        for (i = 0; i < external->parameter_count; ++i) {
            const char *parameter_name =
                (external->parameter_names && external->parameter_names[i] &&
                    external->parameter_names[i][0] != '\0')
                ? external->parameter_names[i]
                : NULL;
            char fallback_name[32];

            if (!parameter_name) {
                snprintf(fallback_name, sizeof(fallback_name), "param%zu", i);
                parameter_name = fallback_name;
            }

            if (!ir_function_append_local(function, parameter_name, 1, NULL, error)) {
                return 0;
            }
        }
    }

    if (!ir_function_parameter_count_matches_external(function, external)) {
        ir_set_error(error,
            external->line,
            external->column,
            "IR-LOWER-020: function '%s' declaration arity mismatch during IR lowering",
            external->name);
        return 0;
    }

    for (i = 0; i < external->parameter_count; ++i) {
        const char *parameter_name =
            external->parameter_names ? external->parameter_names[i] : NULL;

        if (!ir_function_set_parameter_name(function, i, parameter_name, error)) {
            return 0;
        }
    }

    if (out_function) {
        *out_function = function;
    }
    return 1;
}

static int ir_ensure_runtime_global_init_function(IrProgram *program,
    IrFunction **out_function,
    IrError *error) {
    IrFunction *function;

    if (!program) {
        return 0;
    }

    function = ir_program_find_function_mut(program, IR_RUNTIME_GLOBAL_INIT_FUNCTION_NAME);
    if (!function) {
        if (!ir_program_append_function(program,
                IR_RUNTIME_GLOBAL_INIT_FUNCTION_NAME,
                &function,
                error)) {
            return 0;
        }
    }

    if (function->parameter_count != 0 || function->local_count != 0) {
        ir_set_error(error,
            0,
            0,
            "IR-LOWER-024: reserved internal helper '%s' cannot be user-defined with parameters/locals",
            IR_RUNTIME_GLOBAL_INIT_FUNCTION_NAME);
        return 0;
    }

    if (function->has_body) {
        if (function->block_count != 1 || !function->blocks || function->blocks[0].has_terminator) {
            ir_set_error(error,
                0,
                0,
                "IR-LOWER-024: reserved internal helper '%s' cannot be user-defined",
                IR_RUNTIME_GLOBAL_INIT_FUNCTION_NAME);
            return 0;
        }
    } else {
        if (!ir_function_append_block(function, NULL, NULL, error)) {
            return 0;
        }
        function->has_body = 1;
    }

    if (out_function) {
        *out_function = function;
    }
    return 1;
}

static int ir_emit_runtime_initializer_assignment(IrProgram *program,
    IrFunction *runtime_init_function,
    IrGlobal *global,
    const AstExpression *initializer_expr,
    IrError *error) {
    IrLowerContext ctx;
    IrValueRef initializer_value;

    if (!program || !runtime_init_function || !global || !initializer_expr) {
        return 0;
    }
    if (runtime_init_function->block_count == 0 || !runtime_init_function->blocks) {
        ir_set_error(error,
            initializer_expr->line,
            initializer_expr->column,
            "IR-INT-059: runtime initializer helper has no entry block");
        return 0;
    }
    if (runtime_init_function->blocks[0].has_terminator) {
        ir_set_error(error,
            initializer_expr->line,
            initializer_expr->column,
            "IR-INT-060: runtime initializer helper was unexpectedly terminated early");
        return 0;
    }

    memset(&ctx, 0, sizeof(ctx));
    ctx.program = program;
    ctx.function = runtime_init_function;
    ctx.block_id = 0;
    ctx.has_block = 1;
    ctx.error = error;

    if (!ir_lower_expression(&ctx, initializer_expr, &initializer_value)) {
        return 0;
    }

    return ir_emit_mov(&ctx, ir_value_global(global->id), initializer_value);
}

static int ir_prepend_startup_runtime_init_call(IrFunction *function,
    IrError *error) {
    IrInstruction call_instruction;
    size_t name_length;

    if (!function || function->block_count == 0 || !function->blocks) {
        return 0;
    }

    memset(&call_instruction, 0, sizeof(call_instruction));
    call_instruction.kind = IR_INSTR_CALL;
    call_instruction.result = ir_value_temp(ir_allocate_temp(function));
    call_instruction.as.call.arg_count = 0;
    call_instruction.as.call.args = NULL;

    name_length = strlen(IR_RUNTIME_GLOBAL_INIT_FUNCTION_NAME);
    call_instruction.as.call.callee_name = (char *)malloc(name_length + 1);
    if (!call_instruction.as.call.callee_name) {
        ir_set_error(error,
            0,
            0,
            "IR-INT-061: out of memory while preparing runtime startup call");
        return 0;
    }
    memcpy(call_instruction.as.call.callee_name,
        IR_RUNTIME_GLOBAL_INIT_FUNCTION_NAME,
        name_length + 1);

    if (!ir_block_prepend_instruction(&function->blocks[0], &call_instruction, error)) {
        free(call_instruction.as.call.callee_name);
        return 0;
    }

    return 1;
}

static int ir_lower_global_declaration(IrProgram *program,
    const AstProgram *ast_program,
    const AstExternal *external,
    IrFunction **runtime_init_function,
    IrError *error) {
    IrGlobal *global;
    int invalid_constant = 0;
    long long initializer_value = 0;

    if (!program || !external || external->kind != AST_EXTERNAL_DECLARATION) {
        return 1;
    }
    if (!external->name || external->name[0] == '\0') {
        return 1;
    }

    global = ir_program_find_global_mut(program, external->name);
    if (!global && !ir_program_append_global(program, external->name, &global, error)) {
        return 0;
    }
    if (!global) {
        return 0;
    }

    if (!external->has_initializer) {
        return 1;
    }
    if (!external->declaration_initializer) {
        ir_set_error(error,
            external->line,
            external->column,
            "IR-INT-062: top-level declaration '%s' is marked initialized but has no initializer AST",
            external->name);
        return 0;
    }

    if (ir_eval_global_initializer_expr(program,
            external->declaration_initializer,
            &initializer_value,
            error,
            &invalid_constant)) {
        global->has_initializer = 1;
        global->initializer_value = initializer_value;
        global->has_runtime_initializer = 0;
        return 1;
    }

    if (!invalid_constant) {
        return 0;
    }

    if (error) {
        error->line = 0;
        error->column = 0;
        error->message[0] = '\0';
    }

    {
        size_t *deps = NULL;
        size_t dep_count = 0;
        size_t dep_capacity = 0;
        size_t i;

        if (!ir_collect_global_initializer_dependencies(program,
                ast_program,
                external->declaration_initializer,
                &deps,
                &dep_count,
                &dep_capacity,
                error)) {
            free(deps);
            return 0;
        }

        for (i = 0; i < dep_count; ++i) {
            size_t dep_id = deps[i];
            const IrGlobal *dep_global = ir_program_get_global(program, dep_id);

            if (!dep_global) {
                continue;
            }
            if (!dep_global->has_initializer && !dep_global->has_runtime_initializer) {
                char dep_path[256];
                char cycle_chain[256];
                int via_callee = 0;
                const char *source_name = (global->name && global->name[0] != '\0')
                    ? global->name
                    : "<global>";
                const char *blocked_name = (dep_global->name && dep_global->name[0] != '\0')
                    ? dep_global->name
                    : "<global>";

                dep_path[0] = '\0';
                cycle_chain[0] = '\0';

                if (!ir_try_format_runtime_global_dependency_path(program,
                        ast_program,
                        global->id,
                        external->declaration_initializer,
                        dep_id,
                        dep_path,
                        sizeof(dep_path),
                        &via_callee)) {
                    snprintf(dep_path, sizeof(dep_path), "%s -> %s", source_name, blocked_name);
                }

                if (ir_try_format_runtime_global_cycle(program,
                        ast_program,
                        global->id,
                        dep_id,
                        cycle_chain,
                        sizeof(cycle_chain))) {
                    ir_set_error(error,
                        external->line,
                        external->column,
                        "IR-LOWER-022: top-level initializer for global '%s' depends on not-yet-initialized global '%s' (dependency cycle: %s)",
                        source_name,
                        blocked_name,
                        cycle_chain);
                } else if (via_callee) {
                    ir_set_error(error,
                        external->line,
                        external->column,
                        "IR-LOWER-022: top-level initializer for global '%s' depends on not-yet-initialized global '%s' (dependency path via callee body: %s)",
                        source_name,
                        blocked_name,
                        dep_path);
                } else {
                    ir_set_error(error,
                        external->line,
                        external->column,
                        "IR-LOWER-022: top-level initializer for global '%s' depends on not-yet-initialized global '%s' (dependency path: %s)",
                        source_name,
                        blocked_name,
                        dep_path);
                }

                free(deps);
                return 0;
            }
        }

        free(deps);
    }

    if (!ir_ensure_runtime_global_init_function(program, runtime_init_function, error)) {
        return 0;
    }
    if (!*runtime_init_function ||
        !ir_emit_runtime_initializer_assignment(program,
            *runtime_init_function,
            global,
            external->declaration_initializer,
            error)) {
        return 0;
    }

    global->has_initializer = 0;
    global->has_runtime_initializer = 1;
    return 1;
}

static int ir_lower_function(IrProgram *program,
    const AstExternal *external,
    IrError *error) {
    IrFunction *function;
    IrLowerContext ctx;
    size_t i;
    int scope_pushed = 0;
    int ok = 0;

    if (!program || !external || external->kind != AST_EXTERNAL_FUNCTION ||
        !external->name || external->name[0] == '\0') {
        ir_set_error(error, 0, 0, "IR-INT-063: invalid function lowering contract");
        return 0;
    }

    if (!ir_ensure_function_signature(program, external, &function, error)) {
        return 0;
    }

    if (!external->is_function_definition) {
        return 1;
    }

    if (!external->function_body) {
        ir_set_error(error,
            external->line,
            external->column,
            "IR-INT-064: function '%s' definition is missing body AST",
            external->name);
        return 0;
    }

    if (function->has_body) {
        ir_set_error(error,
            external->line,
            external->column,
            "IR-LOWER-021: duplicate function body lowering attempt for '%s'",
            external->name);
        return 0;
    }

    if (!ir_function_append_block(function, &ctx.block_id, NULL, error)) {
        return 0;
    }

    memset(&ctx, 0, sizeof(ctx));
    ctx.program = program;
    ctx.function = function;
    ctx.block_id = 0;
    ctx.has_block = 1;
    ctx.error = error;

    if (!ir_lower_scope_stack_push(&ctx.scopes)) {
        ir_set_error(error,
            external->line,
            external->column,
            "IR-INT-065: failed to initialize function lowering scope stack");
        return 0;
    }
    scope_pushed = 1;

    for (i = 0; i < function->parameter_count; ++i) {
        const char *parameter_name = function->locals[i].source_name;

        if (!parameter_name || parameter_name[0] == '\0') {
            ir_set_error(error,
                external->line,
                external->column,
                "IR-INT-066: function '%s' parameter slot %zu has no source name",
                external->name,
                i);
            goto cleanup;
        }

        if (!ir_lower_scope_add_binding(&ctx.scopes, parameter_name, i)) {
            ir_set_error(error,
                external->line,
                external->column,
                "IR-INT-067: failed to bind parameter '%s' during function lowering",
                parameter_name);
            goto cleanup;
        }
    }

    if (!ir_lower_statement(&ctx, external->function_body)) {
        goto cleanup;
    }

    if (ctx.has_block && (!ir_lower_current_block(&ctx) ||
            !ir_lower_current_block(&ctx)->has_terminator)) {
        ir_set_error(error,
            external->line,
            external->column,
            "IR-LOWER-009: function '%s' body does not terminate on all lowered paths",
            external->name);
        goto cleanup;
    }

    function->has_body = 1;
    ok = 1;

cleanup:
    if (scope_pushed) {
        ir_lower_scope_stack_pop(&ctx.scopes);
    }
    ir_lower_scope_stack_free(&ctx.scopes);
    ir_loop_target_stack_free(&ctx.loop_targets);
    return ok;
}

int ir_lower_program(const AstProgram *ast_program, IrProgram *out_program, IrError *error) {
    IrFunction *runtime_init_function = NULL;
    size_t i;

    if (!ast_program || !out_program) {
        ir_set_error(error, 0, 0, "IR-INT-068: invalid program lowering contract");
        return 0;
    }

    if (error) {
        error->line = 0;
        error->column = 0;
        error->message[0] = '\0';
    }

    ir_program_free(out_program);
    ir_program_init(out_program);

    if (!ir_predeclare_program_globals(out_program, ast_program, error)) {
        return 0;
    }

    for (i = 0; i < ast_program->count; ++i) {
        const AstExternal *external = &ast_program->externals[i];

        if (external->kind == AST_EXTERNAL_DECLARATION) {
            if (!ir_lower_global_declaration(out_program,
                    ast_program,
                    external,
                    &runtime_init_function,
                    error)) {
                return 0;
            }
            continue;
        }

        if (external->kind != AST_EXTERNAL_FUNCTION) {
            continue;
        }
        if (!external->name || external->name[0] == '\0') {
            continue;
        }

        if (!ir_ensure_function_signature(out_program, external, NULL, error)) {
            return 0;
        }
        if (external->is_function_definition && !ir_lower_function(out_program, external, error)) {
            return 0;
        }
    }

    if (runtime_init_function) {
        IrBasicBlock *runtime_entry = ir_function_get_block_mut(runtime_init_function, 0);
        IrFunction *main_function = ir_program_find_function_mut(out_program, "main");

        if (!runtime_entry) {
            ir_set_error(error, 0, 0, "IR-INT-069: runtime initializer helper entry block missing");
            return 0;
        }
        if (!runtime_entry->has_terminator &&
            !ir_block_set_return(runtime_entry, ir_value_immediate(0), error)) {
            return 0;
        }

        if (main_function && main_function->has_body) {
            if (!ir_prepend_startup_runtime_init_call(main_function, error)) {
                return 0;
            }
        } else {
            IrFunction *program_init = ir_program_find_function_mut(out_program,
                IR_PROGRAM_INIT_FUNCTION_NAME);
            IrLowerContext ctx;
            IrValueRef call_result;

            if (!program_init) {
                if (!ir_program_append_function(out_program,
                        IR_PROGRAM_INIT_FUNCTION_NAME,
                        &program_init,
                        error)) {
                    return 0;
                }
            }

            if (program_init->parameter_count != 0 || program_init->local_count != 0 ||
                program_init->has_body) {
                ir_set_error(error,
                    0,
                    0,
                    "IR-LOWER-024: reserved internal helper '%s' cannot be user-defined",
                    IR_PROGRAM_INIT_FUNCTION_NAME);
                return 0;
            }

            if (!ir_function_append_block(program_init, NULL, NULL, error)) {
                return 0;
            }
            program_init->has_body = 1;

            memset(&ctx, 0, sizeof(ctx));
            ctx.program = out_program;
            ctx.function = program_init;
            ctx.block_id = 0;
            ctx.has_block = 1;
            ctx.error = error;

            call_result = ir_value_temp(ir_allocate_temp(program_init));
            if (!ir_emit_call(&ctx,
                    call_result,
                    IR_RUNTIME_GLOBAL_INIT_FUNCTION_NAME,
                    NULL,
                    0) ||
                !ir_block_set_return(ir_lower_current_block(&ctx),
                    ir_value_immediate(0),
                    error)) {
                return 0;
            }
        }
    }

    if (!ir_verify_program(out_program, error)) {
        return 0;
    }
    return 1;
}

#endif

