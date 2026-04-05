#include "lower_ir.h"

#include "ast.h"
#include "lexer.h"
#include "parser.h"
#include "semantic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int lower_source_to_lower_ir_program(const char *source, LowerIrProgram *out_program) {
    TokenArray tokens;
    AstProgram ast_program;
    ParserError parse_err;
    SemanticError sema_err;
    IrProgram ir_program;
    IrError ir_err;
    LowerIrError lower_err;

    if (!source || !out_program) {
        return 0;
    }

    lexer_init_tokens(&tokens);
    ast_program_init(&ast_program);
    ir_program_init(&ir_program);
    lower_ir_program_init(out_program);

    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[lower-ir-verify] FAIL: lexer failed for input\n");
        return 0;
    }

    if (!parser_parse_translation_unit_ast(&tokens, &ast_program, &parse_err)) {
        fprintf(stderr,
            "[lower-ir-verify] FAIL: parser failed at %d:%d: %s\n",
            parse_err.line,
            parse_err.column,
            parse_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&ast_program);
        return 0;
    }

    if (!semantic_analyze_program(&ast_program, &sema_err)) {
        fprintf(stderr,
            "[lower-ir-verify] FAIL: semantic failed at %d:%d: %s\n",
            sema_err.line,
            sema_err.column,
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&ast_program);
        return 0;
    }

    if (!ir_lower_program(&ast_program, &ir_program, &ir_err)) {
        fprintf(stderr,
            "[lower-ir-verify] FAIL: canonical IR lowering failed at %d:%d: %s\n",
            ir_err.line,
            ir_err.column,
            ir_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&ast_program);
        ir_program_free(&ir_program);
        return 0;
    }

    if (!lower_ir_lower_from_ir(&ir_program, out_program, &lower_err)) {
        fprintf(stderr,
            "[lower-ir-verify] FAIL: lower IR lowering failed at %d:%d: %s\n",
            lower_err.line,
            lower_err.column,
            lower_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&ast_program);
        ir_program_free(&ir_program);
        lower_ir_program_free(out_program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&ast_program);
    ir_program_free(&ir_program);
    return 1;
}

static LowerIrFunction *find_function_mut(LowerIrProgram *program, const char *name) {
    size_t i;

    if (!program || !name || name[0] == '\0') {
        return NULL;
    }

    for (i = 0; i < program->function_count; ++i) {
        LowerIrFunction *function = &program->functions[i];

        if (function->name && strcmp(function->name, name) == 0) {
            return function;
        }
    }

    return NULL;
}

static LowerIrInstruction *find_first_call_instruction_mut(LowerIrFunction *function) {
    size_t i;

    if (!function || !function->has_body || !function->blocks) {
        return NULL;
    }

    for (i = 0; i < function->block_count; ++i) {
        LowerIrBasicBlock *block = &function->blocks[i];
        size_t j;

        if (!block->instructions) {
            continue;
        }

        for (j = 0; j < block->instruction_count; ++j) {
            LowerIrInstruction *instruction = &block->instructions[j];

            if (instruction->kind == LOWER_IR_INSTR_CALL) {
                return instruction;
            }
        }
    }

    return NULL;
}

static int replace_call_callee_name(LowerIrInstruction *instruction, const char *callee_name) {
    char *replacement;
    size_t len;

    if (!instruction || instruction->kind != LOWER_IR_INSTR_CALL || !callee_name || callee_name[0] == '\0') {
        return 0;
    }

    len = strlen(callee_name);
    replacement = (char *)malloc(len + 1);
    if (!replacement) {
        return 0;
    }

    memcpy(replacement, callee_name, len + 1);
    free(instruction->as.call.callee_name);
    instruction->as.call.callee_name = replacement;
    return 1;
}

static int build_valid_program(LowerIrProgram *program, LowerIrError *error) {
    LowerIrFunction *function = NULL;
    LowerIrBasicBlock *block = NULL;
    size_t local_a_id;
    size_t temp0;
    size_t temp1;
    LowerIrInstruction instruction;

    lower_ir_program_init(program);

    if (!lower_ir_program_append_global(program, "g", NULL, error) ||
        !lower_ir_program_append_function(program, "main", 1, &function, error) ||
        !lower_ir_function_append_local(function, "a", 1, &local_a_id, error) ||
        !lower_ir_function_append_block(function, NULL, &block, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    temp0 = lower_ir_function_allocate_temp(function);
    temp1 = lower_ir_function_allocate_temp(function);
    if (temp0 == (size_t)-1 || temp1 == (size_t)-1) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp0);
    instruction.as.load_slot = lower_ir_slot_local(local_a_id);
    if (!lower_ir_block_append_instruction(block, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp1);
    instruction.as.binary.op = LOWER_IR_BINARY_ADD;
    instruction.as.binary.lhs = lower_ir_value_temp(temp0);
    instruction.as.binary.rhs = lower_ir_value_immediate(1);
    if (!lower_ir_block_append_instruction(block, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_STORE_GLOBAL;
    instruction.has_result = 0;
    instruction.as.store.slot = lower_ir_slot_global(0);
    instruction.as.store.value = lower_ir_value_temp(temp1);
    if (!lower_ir_block_append_instruction(block, &instruction, error) ||
        !lower_ir_block_set_return(block, lower_ir_value_temp(temp1), error)) {
        lower_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int expect_verifier_rejects(const char *case_id,
    LowerIrProgram *program,
    const char *expected_fragment) {
    LowerIrError error;

    if (lower_ir_verify_program(program, &error)) {
        fprintf(stderr,
            "[lower-ir-verify] FAIL: %s unexpectedly passed verifier\n",
            case_id);
        return 0;
    }

    if (!strstr(error.message, expected_fragment)) {
        fprintf(stderr,
            "[lower-ir-verify] FAIL: %s mismatch\nexpected fragment: %s\nactual: %s\n",
            case_id,
            expected_fragment,
            error.message);
        return 0;
    }

    return 1;
}

static int test_lower_ir_verifier_accepts_valid_program(void) {
    LowerIrProgram program;
    LowerIrError error;
    int ok;

    if (!build_valid_program(&program, &error)) {
        fprintf(stderr,
            "[lower-ir-verify] FAIL: valid setup failed at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
        return 0;
    }

    ok = lower_ir_verify_program(&program, &error);
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-verify] FAIL: valid program rejected at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
    }

    lower_ir_program_free(&program);
    return ok;
}

static int test_lower_ir_verifier_accepts_runtime_global_startup_helper_flow(void) {
    LowerIrProgram program;
    LowerIrError error;
    int ok;

    if (!lower_source_to_lower_ir_program(
            "int seed(){return 1;}\nint g=seed();\nint main(){return g;}\n",
            &program)) {
        return 0;
    }

    ok = lower_ir_verify_program(&program, &error);
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-verify] FAIL: runtime-global startup helper flow rejected at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
    }

    lower_ir_program_free(&program);
    return ok;
}

static int test_lower_ir_verifier_rejects_invalid_local_slot(void) {
    LowerIrProgram program;
    LowerIrError error;
    LowerIrInstruction *instruction;
    int ok;

    if (!build_valid_program(&program, &error)) {
        return 0;
    }

    instruction = &program.functions[0].blocks[0].instructions[0];
    instruction->as.load_slot.id = 99;

    ok = expect_verifier_rejects("LOWER-IR-VERIFY-LOCAL-SLOT",
        &program,
        "LOWER-IR-VERIFY-003");
    lower_ir_program_free(&program);
    return ok;
}

static int test_lower_ir_verifier_rejects_store_result_shape(void) {
    LowerIrProgram program;
    LowerIrError error;
    LowerIrInstruction *instruction;
    int ok;

    if (!build_valid_program(&program, &error)) {
        return 0;
    }

    instruction = &program.functions[0].blocks[0].instructions[2];
    instruction->has_result = 1;
    instruction->result = lower_ir_value_temp(1);

    ok = expect_verifier_rejects("LOWER-IR-VERIFY-STORE-RESULT",
        &program,
        "LOWER-IR-VERIFY-019");
    lower_ir_program_free(&program);
    return ok;
}

static int test_lower_ir_verifier_rejects_immediate_binary_result(void) {
    LowerIrProgram program;
    LowerIrError error;
    LowerIrInstruction *instruction;
    int ok;

    if (!build_valid_program(&program, &error)) {
        return 0;
    }

    instruction = &program.functions[0].blocks[0].instructions[1];
    instruction->result = lower_ir_value_immediate(0);

    ok = expect_verifier_rejects("LOWER-IR-VERIFY-BINARY-RESULT",
        &program,
        "LOWER-IR-VERIFY-009");
    lower_ir_program_free(&program);
    return ok;
}

static int test_lower_ir_verifier_rejects_missing_terminator(void) {
    LowerIrProgram program;
    LowerIrError error;
    int ok;

    if (!build_valid_program(&program, &error)) {
        return 0;
    }

    program.functions[0].blocks[0].has_terminator = 0;

    ok = expect_verifier_rejects("LOWER-IR-VERIFY-MISSING-TERM",
        &program,
        "LOWER-IR-VERIFY-022");
    lower_ir_program_free(&program);
    return ok;
}

static int test_lower_ir_verifier_rejects_unreachable_block(void) {
    LowerIrProgram program;
    LowerIrError error;
    LowerIrFunction *function;
    LowerIrBasicBlock *unreachable_block;
    int ok;

    if (!build_valid_program(&program, &error)) {
        return 0;
    }

    function = &program.functions[0];
    if (!lower_ir_function_append_block(function, NULL, &unreachable_block, &error) ||
        !lower_ir_block_set_return(unreachable_block, lower_ir_value_immediate(1), &error)) {
        lower_ir_program_free(&program);
        return 0;
    }

    ok = expect_verifier_rejects("LOWER-IR-VERIFY-UNREACHABLE-BLOCK",
        &program,
        "LOWER-IR-VERIFY-071");
    lower_ir_program_free(&program);
    return ok;
}

static int test_lower_ir_verifier_rejects_temp_without_definition(void) {
    LowerIrProgram program;
    LowerIrError error;
    int ok;

    if (!build_valid_program(&program, &error)) {
        return 0;
    }

    program.functions[0].next_temp_id = 3;
    program.functions[0].blocks[0].terminator.as.return_value = lower_ir_value_temp(2);

    ok = expect_verifier_rejects("LOWER-IR-VERIFY-TEMP-WITHOUT-DEF",
        &program,
        "LOWER-IR-VERIFY-056");
    lower_ir_program_free(&program);
    return ok;
}

static int test_lower_ir_verifier_rejects_temp_use_before_definition(void) {
    LowerIrProgram program;
    LowerIrError error;
    LowerIrInstruction instruction;
    int ok;

    if (!build_valid_program(&program, &error)) {
        return 0;
    }

    instruction = program.functions[0].blocks[0].instructions[0];
    program.functions[0].blocks[0].instructions[0] = program.functions[0].blocks[0].instructions[1];
    program.functions[0].blocks[0].instructions[1] = instruction;

    ok = expect_verifier_rejects("LOWER-IR-VERIFY-TEMP-USE-BEFORE-DEF",
        &program,
        "LOWER-IR-VERIFY-059");
    lower_ir_program_free(&program);
    return ok;
}

static int test_lower_ir_verifier_rejects_duplicate_temp_definition_in_block(void) {
    LowerIrProgram program;
    LowerIrError error;
    int ok;

    if (!build_valid_program(&program, &error)) {
        return 0;
    }

    program.functions[0].blocks[0].instructions[1].result = lower_ir_value_temp(0);
    program.functions[0].blocks[0].instructions[2].as.store.value = lower_ir_value_temp(0);
    program.functions[0].blocks[0].terminator.as.return_value = lower_ir_value_temp(0);
    program.functions[0].next_temp_id = 1;

    ok = expect_verifier_rejects("LOWER-IR-VERIFY-DUP-TEMP-DEF",
        &program,
        "LOWER-IR-VERIFY-057");
    lower_ir_program_free(&program);
    return ok;
}

static int test_lower_ir_verifier_rejects_hidden_store_result_fake_temp_definition(void) {
    LowerIrProgram program;
    LowerIrError error;
    int ok;

    if (!build_valid_program(&program, &error)) {
        return 0;
    }

    program.functions[0].blocks[0].instructions[2].result = lower_ir_value_temp(2);
    program.functions[0].blocks[0].terminator.as.return_value = lower_ir_value_temp(2);
    program.functions[0].next_temp_id = 3;

    ok = expect_verifier_rejects("LOWER-IR-VERIFY-HIDDEN-STORE-RESULT",
        &program,
        "LOWER-IR-VERIFY-056");
    lower_ir_program_free(&program);
    return ok;
}

static int test_lower_ir_verifier_accepts_join_temp_multi_definition(void) {
    LowerIrProgram program;
    LowerIrError error;
    int ok;

    if (!lower_source_to_lower_ir_program("int f(int a,int b){return a&&b;}\n", &program)) {
        return 0;
    }

    ok = lower_ir_verify_program(&program, &error);
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-verify] FAIL: valid join-temp multi-definition rejected at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
    }

    lower_ir_program_free(&program);
    return ok;
}

static int test_lower_ir_verifier_rejects_declaration_with_extra_nonparameter_local(void) {
    LowerIrProgram program;
    LowerIrError error;
    LowerIrFunction *decl;
    int ok;

    lower_ir_program_init(&program);
    if (!lower_ir_program_append_function(&program, "decl", 0, &decl, &error) ||
        !lower_ir_function_append_local(decl, "x", 1, NULL, &error) ||
        !lower_ir_function_append_local(decl, "scratch", 0, NULL, &error)) {
        lower_ir_program_free(&program);
        return 0;
    }

    ok = expect_verifier_rejects("LOWER-IR-VERIFY-DECL-EXTRA-LOCAL",
        &program,
        "LOWER-IR-VERIFY-063");
    lower_ir_program_free(&program);
    return ok;
}

static int test_lower_ir_verifier_rejects_declaration_with_temp_state(void) {
    LowerIrProgram program;
    LowerIrError error;
    LowerIrFunction *decl;
    int ok;

    lower_ir_program_init(&program);
    if (!lower_ir_program_append_function(&program, "decl", 0, &decl, &error) ||
        !lower_ir_function_append_local(decl, "x", 1, NULL, &error)) {
        lower_ir_program_free(&program);
        return 0;
    }
    decl->next_temp_id = 3;

    ok = expect_verifier_rejects("LOWER-IR-VERIFY-DECL-TEMP-STATE",
        &program,
        "LOWER-IR-VERIFY-064");
    lower_ir_program_free(&program);
    return ok;
}

static int test_lower_ir_verifier_rejects_unknown_callee(void) {
    LowerIrProgram program;
    LowerIrError error;
    LowerIrFunction *function;
    LowerIrBasicBlock *block;
    LowerIrInstruction instruction;
    size_t temp0;
    int ok;

    lower_ir_program_init(&program);
    if (!lower_ir_program_append_function(&program, "main", 1, &function, &error) ||
        !lower_ir_function_append_block(function, NULL, &block, &error)) {
        lower_ir_program_free(&program);
        return 0;
    }

    temp0 = lower_ir_function_allocate_temp(function);
    if (temp0 == (size_t)-1) {
        lower_ir_program_free(&program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp0);
    instruction.as.call.callee_name = (char *)malloc(sizeof("missing"));
    if (!instruction.as.call.callee_name) {
        lower_ir_program_free(&program);
        return 0;
    }
    memcpy(instruction.as.call.callee_name, "missing", sizeof("missing"));
    if (!lower_ir_block_append_instruction(block, &instruction, &error) ||
        !lower_ir_block_set_return(block, lower_ir_value_temp(temp0), &error)) {
        free(instruction.as.call.callee_name);
        lower_ir_program_free(&program);
        return 0;
    }
    free(instruction.as.call.callee_name);

    ok = expect_verifier_rejects("LOWER-IR-VERIFY-UNKNOWN-CALLEE",
        &program,
        "LOWER-IR-VERIFY-045");
    lower_ir_program_free(&program);
    return ok;
}

static int test_lower_ir_verifier_rejects_call_arg_mismatch(void) {
    LowerIrProgram program;
    LowerIrError error;
    LowerIrFunction *main_function;
    LowerIrBasicBlock *block;
    LowerIrInstruction instruction;
    size_t temp0;
    int ok;

    lower_ir_program_init(&program);
    if (!lower_ir_program_append_function(&program, "id", 0, NULL, &error) ||
        !lower_ir_program_append_function(&program, "main", 1, &main_function, &error) ||
        !lower_ir_function_append_local(&program.functions[0], "x", 1, NULL, &error) ||
        !lower_ir_function_append_block(main_function, NULL, &block, &error)) {
        lower_ir_program_free(&program);
        return 0;
    }

    temp0 = lower_ir_function_allocate_temp(main_function);
    if (temp0 == (size_t)-1) {
        lower_ir_program_free(&program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp0);
    instruction.as.call.callee_name = (char *)malloc(sizeof("id"));
    instruction.as.call.args = (LowerIrValueRef *)malloc(2 * sizeof(LowerIrValueRef));
    if (!instruction.as.call.callee_name || !instruction.as.call.args) {
        free(instruction.as.call.callee_name);
        free(instruction.as.call.args);
        lower_ir_program_free(&program);
        return 0;
    }
    memcpy(instruction.as.call.callee_name, "id", sizeof("id"));
    instruction.as.call.arg_count = 2;
    instruction.as.call.args[0] = lower_ir_value_immediate(1);
    instruction.as.call.args[1] = lower_ir_value_immediate(2);
    if (!lower_ir_block_append_instruction(block, &instruction, &error) ||
        !lower_ir_block_set_return(block, lower_ir_value_temp(temp0), &error)) {
        free(instruction.as.call.callee_name);
        free(instruction.as.call.args);
        lower_ir_program_free(&program);
        return 0;
    }
    free(instruction.as.call.callee_name);
    free(instruction.as.call.args);

    ok = expect_verifier_rejects("LOWER-IR-VERIFY-CALL-ARGC",
        &program,
        "LOWER-IR-VERIFY-046");
    lower_ir_program_free(&program);
    return ok;
}

static int test_lower_ir_verifier_rejects_duplicate_function_name(void) {
    LowerIrProgram program;
    LowerIrError error;
    int ok;

    lower_ir_program_init(&program);
    if (!lower_ir_program_append_function(&program, "dup", 0, NULL, &error) ||
        !lower_ir_program_append_function(&program, "dup", 0, NULL, &error)) {
        lower_ir_program_free(&program);
        return 0;
    }

    ok = expect_verifier_rejects("LOWER-IR-VERIFY-DUP-FUNC-NAME",
        &program,
        "LOWER-IR-VERIFY-048");
    lower_ir_program_free(&program);
    return ok;
}

static int test_lower_ir_verifier_rejects_duplicate_global_name(void) {
    LowerIrProgram program;
    LowerIrError error;
    int ok;

    lower_ir_program_init(&program);
    if (!lower_ir_program_append_global(&program, "dup", NULL, &error) ||
        !lower_ir_program_append_global(&program, "dup", NULL, &error)) {
        lower_ir_program_free(&program);
        return 0;
    }

    ok = expect_verifier_rejects("LOWER-IR-VERIFY-DUP-GLOBAL-NAME",
        &program,
        "LOWER-IR-VERIFY-049");
    lower_ir_program_free(&program);
    return ok;
}

static int test_lower_ir_verifier_rejects_function_global_name_collision(void) {
    LowerIrProgram program;
    LowerIrError error;
    int ok;

    lower_ir_program_init(&program);
    if (!lower_ir_program_append_global(&program, "shared", NULL, &error) ||
        !lower_ir_program_append_function(&program, "shared", 0, NULL, &error)) {
        lower_ir_program_free(&program);
        return 0;
    }

    ok = expect_verifier_rejects("LOWER-IR-VERIFY-FUNC-GLOBAL-NAME-COLLISION",
        &program,
        "LOWER-IR-VERIFY-050");
    lower_ir_program_free(&program);
    return ok;
}

static int test_lower_ir_verifier_rejects_null_global_table(void) {
    LowerIrProgram program;
    LowerIrError error;
    LowerIrGlobal *saved_globals;
    int ok;

    if (!build_valid_program(&program, &error)) {
        return 0;
    }

    saved_globals = program.globals;
    program.globals = NULL;
    ok = expect_verifier_rejects("LOWER-IR-VERIFY-NULL-GLOBAL-TABLE",
        &program,
        "LOWER-IR-VERIFY-051");
    program.globals = saved_globals;

    lower_ir_program_free(&program);
    return ok;
}

static int test_lower_ir_verifier_rejects_global_count_over_capacity(void) {
    LowerIrProgram program;
    LowerIrError error;
    size_t saved_capacity;
    int ok;

    if (!build_valid_program(&program, &error)) {
        return 0;
    }

    saved_capacity = program.global_capacity;
    program.global_capacity = program.global_count - 1;
    ok = expect_verifier_rejects("LOWER-IR-VERIFY-GLOBAL-COUNT-CAPACITY",
        &program,
        "LOWER-IR-VERIFY-065");
    program.global_capacity = saved_capacity;

    lower_ir_program_free(&program);
    return ok;
}

static int test_lower_ir_verifier_rejects_null_function_table(void) {
    LowerIrProgram program;
    LowerIrError error;
    LowerIrFunction *saved_functions;
    int ok;

    if (!build_valid_program(&program, &error)) {
        return 0;
    }

    saved_functions = program.functions;
    program.functions = NULL;
    ok = expect_verifier_rejects("LOWER-IR-VERIFY-NULL-FUNCTION-TABLE",
        &program,
        "LOWER-IR-VERIFY-052");
    program.functions = saved_functions;

    lower_ir_program_free(&program);
    return ok;
}

static int test_lower_ir_verifier_rejects_function_count_over_capacity(void) {
    LowerIrProgram program;
    LowerIrError error;
    size_t saved_capacity;
    int ok;

    if (!build_valid_program(&program, &error)) {
        return 0;
    }

    saved_capacity = program.function_capacity;
    program.function_capacity = program.function_count - 1;
    ok = expect_verifier_rejects("LOWER-IR-VERIFY-FUNCTION-COUNT-CAPACITY",
        &program,
        "LOWER-IR-VERIFY-066");
    program.function_capacity = saved_capacity;

    lower_ir_program_free(&program);
    return ok;
}

static int test_lower_ir_verifier_rejects_null_local_table(void) {
    LowerIrProgram program;
    LowerIrError error;
    LowerIrLocal *saved_locals;
    int ok;

    if (!build_valid_program(&program, &error)) {
        return 0;
    }

    saved_locals = program.functions[0].locals;
    program.functions[0].locals = NULL;
    ok = expect_verifier_rejects("LOWER-IR-VERIFY-NULL-LOCAL-TABLE",
        &program,
        "LOWER-IR-VERIFY-053");
    program.functions[0].locals = saved_locals;

    lower_ir_program_free(&program);
    return ok;
}

static int test_lower_ir_verifier_rejects_local_count_over_capacity(void) {
    LowerIrProgram program;
    LowerIrError error;
    size_t saved_capacity;
    int ok;

    if (!build_valid_program(&program, &error)) {
        return 0;
    }

    saved_capacity = program.functions[0].local_capacity;
    program.functions[0].local_capacity = program.functions[0].local_count - 1;
    ok = expect_verifier_rejects("LOWER-IR-VERIFY-LOCAL-COUNT-CAPACITY",
        &program,
        "LOWER-IR-VERIFY-067");
    program.functions[0].local_capacity = saved_capacity;

    lower_ir_program_free(&program);
    return ok;
}

static int test_lower_ir_verifier_rejects_null_block_table(void) {
    LowerIrProgram program;
    LowerIrError error;
    LowerIrBasicBlock *saved_blocks;
    int ok;

    if (!build_valid_program(&program, &error)) {
        return 0;
    }

    saved_blocks = program.functions[0].blocks;
    program.functions[0].blocks = NULL;
    ok = expect_verifier_rejects("LOWER-IR-VERIFY-NULL-BLOCK-TABLE",
        &program,
        "LOWER-IR-VERIFY-054");
    program.functions[0].blocks = saved_blocks;

    lower_ir_program_free(&program);
    return ok;
}

static int test_lower_ir_verifier_rejects_block_count_over_capacity(void) {
    LowerIrProgram program;
    LowerIrError error;
    size_t saved_capacity;
    int ok;

    if (!build_valid_program(&program, &error)) {
        return 0;
    }

    saved_capacity = program.functions[0].block_capacity;
    program.functions[0].block_capacity = program.functions[0].block_count - 1;
    ok = expect_verifier_rejects("LOWER-IR-VERIFY-BLOCK-COUNT-CAPACITY",
        &program,
        "LOWER-IR-VERIFY-068");
    program.functions[0].block_capacity = saved_capacity;

    lower_ir_program_free(&program);
    return ok;
}

static int test_lower_ir_verifier_rejects_null_instruction_table(void) {
    LowerIrProgram program;
    LowerIrError error;
    LowerIrInstruction *saved_instructions;
    int ok;

    if (!build_valid_program(&program, &error)) {
        return 0;
    }

    saved_instructions = program.functions[0].blocks[0].instructions;
    program.functions[0].blocks[0].instructions = NULL;
    ok = expect_verifier_rejects("LOWER-IR-VERIFY-NULL-INSTRUCTION-TABLE",
        &program,
        "LOWER-IR-VERIFY-060");
    program.functions[0].blocks[0].instructions = saved_instructions;

    lower_ir_program_free(&program);
    return ok;
}

static int test_lower_ir_verifier_rejects_instruction_count_over_capacity(void) {
    LowerIrProgram program;
    LowerIrError error;
    size_t saved_capacity;
    int ok;

    if (!build_valid_program(&program, &error)) {
        return 0;
    }

    saved_capacity = program.functions[0].blocks[0].instruction_capacity;
    program.functions[0].blocks[0].instruction_capacity =
        program.functions[0].blocks[0].instruction_count - 1;
    ok = expect_verifier_rejects("LOWER-IR-VERIFY-INSTR-COUNT-CAPACITY",
        &program,
        "LOWER-IR-VERIFY-069");
    program.functions[0].blocks[0].instruction_capacity = saved_capacity;

    lower_ir_program_free(&program);
    return ok;
}

static int test_lower_ir_verifier_rejects_runtime_global_missing_helper_function(void) {
    LowerIrProgram program;
    LowerIrFunction *runtime_init_function;
    int ok;

    if (!lower_source_to_lower_ir_program(
            "int seed(){return 1;}\nint g=seed();\nint main(){return g;}\n",
            &program)) {
        return 0;
    }

    runtime_init_function = find_function_mut(&program, "__global.init");
    if (!runtime_init_function) {
        fprintf(stderr,
            "[lower-ir-verify] FAIL: runtime-init helper missing in setup case\n");
        lower_ir_program_free(&program);
        return 0;
    }

    runtime_init_function->has_body = 0;
    runtime_init_function->block_count = 0;
    ok = expect_verifier_rejects("LOWER-IR-VERIFY-RUNTIME-GLOBAL-NO-HELPER",
        &program,
        "LOWER-IR-VERIFY-036");

    lower_ir_program_free(&program);
    return ok;
}

static int test_lower_ir_verifier_rejects_runtime_global_missing_startup_call(void) {
    LowerIrProgram program;
    LowerIrFunction *main_function;
    LowerIrInstruction *startup_call;
    int ok;

    if (!lower_source_to_lower_ir_program(
            "int seed(){return 1;}\nint g=seed();\nint main(){return g;}\n",
            &program)) {
        return 0;
    }

    main_function = find_function_mut(&program, "main");
    if (!main_function || !main_function->has_body || main_function->block_count == 0 ||
        main_function->blocks[0].instruction_count == 0 ||
        !main_function->blocks[0].instructions) {
        fprintf(stderr,
            "[lower-ir-verify] FAIL: startup-call case missing main entry instruction\n");
        lower_ir_program_free(&program);
        return 0;
    }

    startup_call = &main_function->blocks[0].instructions[0];
    if (!replace_call_callee_name(startup_call, "seed")) {
        fprintf(stderr,
            "[lower-ir-verify] FAIL: unable to rewrite startup call callee name\n");
        lower_ir_program_free(&program);
        return 0;
    }

    ok = expect_verifier_rejects("LOWER-IR-VERIFY-RUNTIME-GLOBAL-NO-ENTRY-CALL",
        &program,
        "LOWER-IR-VERIFY-039");

    lower_ir_program_free(&program);
    return ok;
}

static int test_lower_ir_verifier_rejects_reserved_runtime_init_call_outside_startup_entry(void) {
    LowerIrProgram program;
    LowerIrFunction *runtime_init_function;
    LowerIrInstruction *instruction;
    int ok;

    if (!lower_source_to_lower_ir_program(
            "int seed(){return 1;}\nint g=seed();\nint main(){return g;}\n",
            &program)) {
        return 0;
    }

    runtime_init_function = find_function_mut(&program, "__global.init");
    instruction = find_first_call_instruction_mut(runtime_init_function);
    if (!instruction) {
        fprintf(stderr,
            "[lower-ir-verify] FAIL: no call instruction found in __global.init for reserved-call policy case\n");
        lower_ir_program_free(&program);
        return 0;
    }
    if (!replace_call_callee_name(instruction, "__global.init")) {
        fprintf(stderr,
            "[lower-ir-verify] FAIL: unable to rewrite call callee in reserved-call policy case\n");
        lower_ir_program_free(&program);
        return 0;
    }

    ok = expect_verifier_rejects("LOWER-IR-VERIFY-INIT-HELPER-NONSTARTUP-CALL",
        &program,
        "LOWER-IR-VERIFY-043");

    lower_ir_program_free(&program);
    return ok;
}

static int test_lower_ir_verifier_rejects_reserved_init_helper_name_without_runtime_globals(void) {
    LowerIrProgram program;
    char *renamed;
    int ok;

    if (!lower_source_to_lower_ir_program("int f(){return 0;}\n", &program)) {
        return 0;
    }

    renamed = (char *)malloc(sizeof("__global.init"));
    if (!renamed) {
        lower_ir_program_free(&program);
        return 0;
    }
    memcpy(renamed, "__global.init", sizeof("__global.init"));
    free(program.functions[0].name);
    program.functions[0].name = renamed;

    ok = expect_verifier_rejects("LOWER-IR-VERIFY-RESERVED-INIT-NAME",
        &program,
        "LOWER-IR-VERIFY-040");

    lower_ir_program_free(&program);
    return ok;
}

int main(void) {
    int ok = 1;

    ok &= test_lower_ir_verifier_accepts_valid_program();
    ok &= test_lower_ir_verifier_accepts_runtime_global_startup_helper_flow();
    ok &= test_lower_ir_verifier_rejects_invalid_local_slot();
    ok &= test_lower_ir_verifier_rejects_store_result_shape();
    ok &= test_lower_ir_verifier_rejects_immediate_binary_result();
    ok &= test_lower_ir_verifier_rejects_missing_terminator();
    ok &= test_lower_ir_verifier_rejects_unreachable_block();
    ok &= test_lower_ir_verifier_rejects_temp_without_definition();
    ok &= test_lower_ir_verifier_rejects_temp_use_before_definition();
    ok &= test_lower_ir_verifier_rejects_duplicate_temp_definition_in_block();
    ok &= test_lower_ir_verifier_rejects_hidden_store_result_fake_temp_definition();
    ok &= test_lower_ir_verifier_accepts_join_temp_multi_definition();
    ok &= test_lower_ir_verifier_rejects_declaration_with_extra_nonparameter_local();
    ok &= test_lower_ir_verifier_rejects_declaration_with_temp_state();
    ok &= test_lower_ir_verifier_rejects_unknown_callee();
    ok &= test_lower_ir_verifier_rejects_call_arg_mismatch();
    ok &= test_lower_ir_verifier_rejects_duplicate_function_name();
    ok &= test_lower_ir_verifier_rejects_duplicate_global_name();
    ok &= test_lower_ir_verifier_rejects_function_global_name_collision();
    ok &= test_lower_ir_verifier_rejects_null_global_table();
    ok &= test_lower_ir_verifier_rejects_global_count_over_capacity();
    ok &= test_lower_ir_verifier_rejects_null_function_table();
    ok &= test_lower_ir_verifier_rejects_function_count_over_capacity();
    ok &= test_lower_ir_verifier_rejects_null_local_table();
    ok &= test_lower_ir_verifier_rejects_local_count_over_capacity();
    ok &= test_lower_ir_verifier_rejects_null_block_table();
    ok &= test_lower_ir_verifier_rejects_block_count_over_capacity();
    ok &= test_lower_ir_verifier_rejects_null_instruction_table();
    ok &= test_lower_ir_verifier_rejects_instruction_count_over_capacity();
    ok &= test_lower_ir_verifier_rejects_runtime_global_missing_helper_function();
    ok &= test_lower_ir_verifier_rejects_runtime_global_missing_startup_call();
    ok &= test_lower_ir_verifier_rejects_reserved_runtime_init_call_outside_startup_entry();
    ok &= test_lower_ir_verifier_rejects_reserved_init_helper_name_without_runtime_globals();

    if (!ok) {
        return 1;
    }

    printf("[lower-ir-verify] PASS\n");
    return 0;
}
