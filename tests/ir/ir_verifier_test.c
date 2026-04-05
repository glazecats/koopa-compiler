#include "ast.h"
#include "ir.h"
#include "lexer.h"
#include "parser.h"
#include "semantic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int lower_source_to_ir_program(const char *source, IrProgram *out_program) {
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;
    IrError ir_err;

    if (!source || !out_program) {
        return 0;
    }

    lexer_init_tokens(&tokens);
    ast_program_init(&program);
    ir_program_init(out_program);

    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[ir-verify] FAIL: lexer failed for input\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (!parser_parse_translation_unit_ast(&tokens, &program, &parse_err)) {
        fprintf(stderr,
            "[ir-verify] FAIL: parser failed at %d:%d: %s\n",
            parse_err.line,
            parse_err.column,
            parse_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (!semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
            "[ir-verify] FAIL: semantic failed at %d:%d: %s\n",
            sema_err.line,
            sema_err.column,
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (!ir_lower_program(&program, out_program, &ir_err)) {
        fprintf(stderr,
            "[ir-verify] FAIL: IR lowering failed at %d:%d: %s\n",
            ir_err.line,
            ir_err.column,
            ir_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        ir_program_free(out_program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int expect_verifier_rejects(const char *case_id,
    IrProgram *program,
    const char *expected_fragment) {
    IrError error;

    if (!case_id || !program || !expected_fragment) {
        return 0;
    }

    if (ir_verify_program(program, &error)) {
        fprintf(stderr, "[ir-verify] FAIL: %s unexpectedly passed verifier\n", case_id);
        return 0;
    }

    if (!strstr(error.message, expected_fragment)) {
        fprintf(stderr,
            "[ir-verify] FAIL: %s mismatch\nexpected fragment: %s\nactual: %s\n",
            case_id,
            expected_fragment,
            error.message);
        return 0;
    }

    return 1;
}

static IrFunction *find_function_mut(IrProgram *program, const char *name) {
    size_t i;

    if (!program || !name || name[0] == '\0') {
        return NULL;
    }

    for (i = 0; i < program->function_count; ++i) {
        IrFunction *function = &program->functions[i];

        if (function->name && strcmp(function->name, name) == 0) {
            return function;
        }
    }

    return NULL;
}

static IrInstruction *find_first_call_instruction_mut(IrFunction *function) {
    size_t i;

    if (!function || !function->has_body || !function->blocks) {
        return NULL;
    }

    for (i = 0; i < function->block_count; ++i) {
        IrBasicBlock *block = &function->blocks[i];
        size_t j;

        if (!block->instructions) {
            continue;
        }

        for (j = 0; j < block->instruction_count; ++j) {
            IrInstruction *instruction = &block->instructions[j];

            if (instruction->kind == IR_INSTR_CALL) {
                return instruction;
            }
        }
    }

    return NULL;
}

static int replace_call_callee_name(IrInstruction *instruction, const char *callee_name) {
    char *replacement;
    size_t len;

    if (!instruction || instruction->kind != IR_INSTR_CALL || !callee_name || callee_name[0] == '\0') {
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

static int test_ir_verifier_accepts_lowered_program(void) {
    IrProgram program;
    IrError error;
    int ok;

    if (!lower_source_to_ir_program(
            "int id(int a){return a;}\nint main(){return id(1);}\n",
            &program)) {
        return 0;
    }

    ok = ir_verify_program(&program, &error);
    if (!ok) {
        fprintf(stderr,
            "[ir-verify] FAIL: lowered program rejected at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
    }

    ir_program_free(&program);
    return ok;
}

static int test_ir_verifier_accepts_declaration_only_signature(void) {
    IrProgram program;
    IrError error;
    int ok;

    if (!lower_source_to_ir_program(
            "int add(int a,int b);\nint main(){return add(1,2);}\n",
            &program)) {
        return 0;
    }

    ok = ir_verify_program(&program, &error);
    if (!ok) {
        fprintf(stderr,
            "[ir-verify] FAIL: declaration-aware lowered program rejected at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
    }

    ir_program_free(&program);
    return ok;
}

static int test_ir_verifier_accepts_runtime_global_startup_helper_flow(void) {
    IrProgram program;
    IrError error;
    int ok;

    if (!lower_source_to_ir_program(
            "int seed(){return 1;}\nint g=seed();\nint main(){return g;}\n",
            &program)) {
        return 0;
    }

    ok = ir_verify_program(&program, &error);
    if (!ok) {
        fprintf(stderr,
            "[ir-verify] FAIL: runtime-global startup helper flow rejected at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
    }

    ir_program_free(&program);
    return ok;
}

static int test_ir_verifier_rejects_missing_terminator(void) {
    IrProgram program;
    int ok;

    if (!lower_source_to_ir_program("int f(){return 0;}\n", &program)) {
        return 0;
    }

    program.functions[0].blocks[0].has_terminator = 0;
    ok = expect_verifier_rejects("IR-VERIFY-MISSING-TERM",
        &program,
        "IR-VERIFY-005");

    ir_program_free(&program);
    return ok;
}

static int test_ir_verifier_rejects_unreachable_block(void) {
    IrProgram program;
    IrFunction *function;
    IrBasicBlock *block;
    int ok;

    if (!lower_source_to_ir_program("int f(){return 0;}\n", &program)) {
        return 0;
    }

    function = &program.functions[0];
    if (function->block_capacity < 2) {
        fprintf(stderr, "[ir-verify] FAIL: unexpected block capacity in verifier test\n");
        ir_program_free(&program);
        return 0;
    }

    block = &function->blocks[1];
    block->id = 1;
    block->instructions = NULL;
    block->instruction_count = 0;
    block->instruction_capacity = 0;
    block->has_terminator = 1;
    block->terminator.kind = IR_TERM_RETURN;
    block->terminator.as.return_value.kind = IR_VALUE_IMMEDIATE;
    block->terminator.as.return_value.immediate = 0;
    block->terminator.as.return_value.id = 0;
    function->block_count = 2;

    ok = expect_verifier_rejects("IR-VERIFY-UNREACHABLE",
        &program,
        "IR-VERIFY-010");

    ir_program_free(&program);
    return ok;
}

static int test_ir_verifier_rejects_unknown_binary_op(void) {
    IrProgram program;
    IrInstruction *instruction;
    int ok;

    if (!lower_source_to_ir_program("int f(int a){return a+1;}\n", &program)) {
        return 0;
    }

    instruction = &program.functions[0].blocks[0].instructions[0];
    instruction->kind = IR_INSTR_BINARY;
    instruction->result.kind = IR_VALUE_TEMP;
    instruction->result.id = 0;
    instruction->as.binary.op = (IrBinaryOp)999;
    instruction->as.binary.lhs.kind = IR_VALUE_LOCAL;
    instruction->as.binary.lhs.id = 0;
    instruction->as.binary.rhs.kind = IR_VALUE_IMMEDIATE;
    instruction->as.binary.rhs.immediate = 1;
    instruction->as.binary.rhs.id = 0;

    ok = expect_verifier_rejects("IR-VERIFY-BAD-BINARY-OP",
        &program,
        "IR-VERIFY-020");

    ir_program_free(&program);
    return ok;
}

static int test_ir_verifier_rejects_non_temp_binary_result(void) {
    IrProgram program;
    IrInstruction *instruction;
    int ok;

    if (!lower_source_to_ir_program("int f(int a){return a+1;}\n", &program)) {
        return 0;
    }

    instruction = &program.functions[0].blocks[0].instructions[0];
    instruction->result.kind = IR_VALUE_LOCAL;
    instruction->result.id = 0;
    program.functions[0].next_temp_id = 0;
    ok = expect_verifier_rejects("IR-VERIFY-BINARY-RESULT-NON-TEMP",
        &program,
        "IR-VERIFY-057");

    ir_program_free(&program);
    return ok;
}

static int test_ir_verifier_rejects_empty_call_callee(void) {
    IrProgram program;
    IrInstruction *instruction;
    int ok;

    if (!lower_source_to_ir_program("int id(int a){return a;}\nint main(){return id(1);}\n", &program)) {
        return 0;
    }

    instruction = &program.functions[1].blocks[0].instructions[0];
    instruction->as.call.callee_name[0] = '\0';
    ok = expect_verifier_rejects("IR-VERIFY-EMPTY-CALLEE",
        &program,
        "IR-VERIFY-021");

    ir_program_free(&program);
    return ok;
}

static int test_ir_verifier_rejects_non_temp_call_result(void) {
    IrProgram program;
    IrInstruction *instruction;
    int ok;

    if (!lower_source_to_ir_program(
            "int id(int a){return a;}\nint main(){int x=0; x=id(1); return x;}\n",
            &program)) {
        return 0;
    }

    instruction = &program.functions[1].blocks[0].instructions[1];
    instruction->result.kind = IR_VALUE_LOCAL;
    instruction->result.id = 0;
    program.functions[1].next_temp_id = 0;
    ok = expect_verifier_rejects("IR-VERIFY-CALL-RESULT-NON-TEMP",
        &program,
        "IR-VERIFY-058");

    ir_program_free(&program);
    return ok;
}

static int test_ir_verifier_rejects_undefined_temp_slot(void) {
    IrProgram program;
    int ok;

    if (!lower_source_to_ir_program("int f(){return 0;}\n", &program)) {
        return 0;
    }

    program.functions[0].next_temp_id = 1;
    ok = expect_verifier_rejects("IR-VERIFY-UNDEFINED-TEMP",
        &program,
        "IR-VERIFY-023");

    ir_program_free(&program);
    return ok;
}

static int test_ir_verifier_rejects_temp_use_before_definition(void) {
    IrProgram program;
    IrInstruction *instruction;
    int ok;

    if (!lower_source_to_ir_program("int f(int a){return a+1;}\n", &program)) {
        return 0;
    }

    instruction = &program.functions[0].blocks[0].instructions[0];
    instruction->as.binary.lhs.kind = IR_VALUE_TEMP;
    instruction->as.binary.lhs.id = 0;
    ok = expect_verifier_rejects("IR-VERIFY-USE-BEFORE-DEF",
        &program,
        "IR-VERIFY-024");

    ir_program_free(&program);
    return ok;
}

static int test_ir_verifier_rejects_duplicate_temp_definition(void) {
    IrProgram program;
    IrInstruction *instruction;
    int ok;

    if (!lower_source_to_ir_program("int f(int a){a+1; a+2; return a;}\n", &program)) {
        return 0;
    }

    instruction = &program.functions[0].blocks[0].instructions[1];
    instruction->result.kind = IR_VALUE_TEMP;
    instruction->result.id = 0;
    program.functions[0].next_temp_id = 1;
    ok = expect_verifier_rejects("IR-VERIFY-DUP-TEMP-DEF",
        &program,
        "IR-VERIFY-031");

    ir_program_free(&program);
    return ok;
}

static int test_ir_verifier_rejects_internal_call_argument_mismatch(void) {
    IrProgram program;
    IrInstruction *instruction;
    int ok;

    if (!lower_source_to_ir_program(
            "int add(int a,int b){return a+b;}\nint main(){return add(1,2);}\n",
            &program)) {
        return 0;
    }

    instruction = &program.functions[1].blocks[0].instructions[0];
    instruction->as.call.arg_count = 1;
    ok = expect_verifier_rejects("IR-VERIFY-CALL-ARGC",
        &program,
        "IR-VERIFY-026");

    ir_program_free(&program);
    return ok;
}

static int test_ir_verifier_rejects_declared_call_argument_mismatch(void) {
    IrProgram program;
    IrInstruction *instruction;
    int ok;

    if (!lower_source_to_ir_program(
            "int add(int a,int b);\nint main(){return add(1,2);}\n",
            &program)) {
        return 0;
    }

    instruction = &program.functions[1].blocks[0].instructions[0];
    instruction->as.call.arg_count = 1;
    ok = expect_verifier_rejects("IR-VERIFY-DECL-CALL-ARGC",
        &program,
        "IR-VERIFY-026");

    ir_program_free(&program);
    return ok;
}

static int test_ir_verifier_rejects_unknown_call_target(void) {
    IrProgram program;
    IrInstruction *instruction;
    int ok;

    if (!lower_source_to_ir_program(
            "int add(int a,int b){return a+b;}\nint main(){return add(1,2);}\n",
            &program)) {
        return 0;
    }

    instruction = &program.functions[1].blocks[0].instructions[0];
    memcpy(instruction->as.call.callee_name, "foo", 4);
    ok = expect_verifier_rejects("IR-VERIFY-UNKNOWN-CALLEE",
        &program,
        "IR-VERIFY-032");

    ir_program_free(&program);
    return ok;
}

static int test_ir_verifier_rejects_null_call_args_payload(void) {
    IrProgram program;
    IrInstruction *instruction;
    IrValueRef *saved_args;
    int ok;

    if (!lower_source_to_ir_program(
            "int add(int a,int b){return a+b;}\nint main(){return add(1,2);}\n",
            &program)) {
        return 0;
    }

    instruction = &program.functions[1].blocks[0].instructions[0];
    saved_args = instruction->as.call.args;
    instruction->as.call.args = NULL;
    ok = expect_verifier_rejects("IR-VERIFY-NULL-CALL-ARGS",
        &program,
        "IR-VERIFY-033");

    free(saved_args);
    ir_program_free(&program);
    return ok;
}

static int test_ir_verifier_rejects_duplicate_function_entry_names(void) {
    IrProgram program;
    char *renamed = NULL;
    int ok;

    if (!lower_source_to_ir_program(
            "int add(int a,int b);\nint main(){return add(1,2);}\n",
            &program)) {
        return 0;
    }

    renamed = (char *)malloc(4);
    if (!renamed) {
        ir_program_free(&program);
        return 0;
    }
    memcpy(renamed, "add", 4);
    free(program.functions[1].name);
    program.functions[1].name = renamed;

    ok = expect_verifier_rejects("IR-VERIFY-DUP-FUNC-NAME",
        &program,
        "IR-VERIFY-027");

    ir_program_free(&program);
    return ok;
}

static int test_ir_verifier_rejects_temps_on_declaration(void) {
    IrProgram program;
    int ok;

    if (!lower_source_to_ir_program(
            "int add(int a,int b);\nint main(){return add(1,2);}\n",
            &program)) {
        return 0;
    }

    program.functions[0].next_temp_id = 1;
    ok = expect_verifier_rejects("IR-VERIFY-DECL-TEMPS",
        &program,
        "IR-VERIFY-029");

    ir_program_free(&program);
    return ok;
}

static int test_ir_verifier_rejects_non_parameter_local_on_declaration(void) {
    IrProgram program;
    int ok;

    if (!lower_source_to_ir_program(
            "int add(int a,int b);\nint main(){return add(1,2);}\n",
            &program)) {
        return 0;
    }

    program.functions[0].locals[1].is_parameter = 0;
    program.functions[0].parameter_count = 1;
    ok = expect_verifier_rejects("IR-VERIFY-DECL-LOCAL",
        &program,
        "IR-VERIFY-030");

    ir_program_free(&program);
    return ok;
}

static int test_ir_verifier_rejects_local_count_exceeding_capacity(void) {
    IrProgram program;
    int ok;

    if (!lower_source_to_ir_program("int add(int a,int b){return a+b;}\n", &program)) {
        return 0;
    }

    program.functions[0].local_capacity = 0;
    ok = expect_verifier_rejects("IR-VERIFY-LOCAL-COUNT-CAP",
        &program,
        "IR-VERIFY-038");

    ir_program_free(&program);
    return ok;
}

static int test_ir_verifier_rejects_block_count_exceeding_capacity(void) {
    IrProgram program;
    int ok;

    if (!lower_source_to_ir_program("int add(int a,int b){return a+b;}\n", &program)) {
        return 0;
    }

    program.functions[0].block_capacity = 0;
    ok = expect_verifier_rejects("IR-VERIFY-BLOCK-COUNT-CAP",
        &program,
        "IR-VERIFY-039");

    ir_program_free(&program);
    return ok;
}

static int test_ir_verifier_rejects_instruction_count_exceeding_capacity(void) {
    IrProgram program;
    int ok;

    if (!lower_source_to_ir_program("int add(int a,int b){return a+b;}\n", &program)) {
        return 0;
    }

    program.functions[0].blocks[0].instruction_capacity = 0;
    ok = expect_verifier_rejects("IR-VERIFY-INSTR-COUNT-CAP",
        &program,
        "IR-VERIFY-040");

    ir_program_free(&program);
    return ok;
}

static int test_ir_verifier_rejects_null_local_table(void) {
    IrProgram program;
    IrFunction *function;
    IrLocal *saved_locals;
    int ok;

    if (!lower_source_to_ir_program(
            "int add(int a,int b){return a+b;}\n",
            &program)) {
        return 0;
    }

    function = &program.functions[0];
    saved_locals = function->locals;
    function->locals = NULL;
    ok = expect_verifier_rejects("IR-VERIFY-NULL-LOCALS",
        &program,
        "IR-VERIFY-034");
    function->locals = saved_locals;

    ir_program_free(&program);
    return ok;
}

static int test_ir_verifier_rejects_null_block_table(void) {
    IrProgram program;
    IrFunction *function;
    IrBasicBlock *saved_blocks;
    int ok;

    if (!lower_source_to_ir_program(
            "int add(int a,int b){return a+b;}\n",
            &program)) {
        return 0;
    }

    function = &program.functions[0];
    saved_blocks = function->blocks;
    function->blocks = NULL;
    ok = expect_verifier_rejects("IR-VERIFY-NULL-BLOCKS",
        &program,
        "IR-VERIFY-035");
    function->blocks = saved_blocks;

    ir_program_free(&program);
    return ok;
}

static int test_ir_verifier_rejects_null_instruction_table(void) {
    IrProgram program;
    IrBasicBlock *block;
    IrInstruction *saved_instructions;
    int ok;

    if (!lower_source_to_ir_program(
            "int add(int a,int b){return a+b;}\n",
            &program)) {
        return 0;
    }

    block = &program.functions[0].blocks[0];
    saved_instructions = block->instructions;
    block->instructions = NULL;
    ok = expect_verifier_rejects("IR-VERIFY-NULL-INSTRS",
        &program,
        "IR-VERIFY-036");
    block->instructions = saved_instructions;

    ir_program_free(&program);
    return ok;
}

static int test_ir_verifier_rejects_zero_arg_call_with_non_null_payload(void) {
    IrProgram program;
    IrInstruction *instruction;
    IrValueRef *bogus_args;
    int ok;

    if (!lower_source_to_ir_program(
            "int f(){return 1;}\nint main(){return f();}\n",
            &program)) {
        return 0;
    }

    instruction = &program.functions[1].blocks[0].instructions[0];
    bogus_args = (IrValueRef *)malloc(sizeof(IrValueRef));
    if (!bogus_args) {
        ir_program_free(&program);
        return 0;
    }
    bogus_args[0].kind = IR_VALUE_IMMEDIATE;
    bogus_args[0].immediate = 0;
    bogus_args[0].id = 0;
    instruction->as.call.arg_count = 0;
    instruction->as.call.args = bogus_args;
    ok = expect_verifier_rejects("IR-VERIFY-ZERO-ARG-NONNULL-PAYLOAD",
        &program,
        "IR-VERIFY-037");

    ir_program_free(&program);
    return ok;
}

static int test_ir_verifier_rejects_blocks_on_declaration(void) {
    IrProgram program;
    IrFunction *function;
    IrBasicBlock *injected_blocks;
    int ok;

    if (!lower_source_to_ir_program(
            "int add(int a,int b);\nint main(){return add(1,2);}\n",
            &program)) {
        return 0;
    }

    function = &program.functions[0];
    injected_blocks = (IrBasicBlock *)calloc(1, sizeof(IrBasicBlock));
    if (!injected_blocks) {
        ir_program_free(&program);
        return 0;
    }
    injected_blocks[0].id = 0;
    injected_blocks[0].instructions = NULL;
    injected_blocks[0].instruction_count = 0;
    injected_blocks[0].instruction_capacity = 0;
    injected_blocks[0].has_terminator = 0;
    function->blocks = injected_blocks;
    function->block_capacity = 1;
    function->block_count = 1;
    ok = expect_verifier_rejects("IR-VERIFY-DECL-BLOCKS",
        &program,
        "IR-VERIFY-028");

    ir_program_free(&program);
    return ok;
}

static int test_ir_verifier_rejects_program_function_count_exceeding_capacity(void) {
    IrProgram program;
    int ok;

    if (!lower_source_to_ir_program("int f(){return 0;}\n", &program)) {
        return 0;
    }

    program.function_capacity = 0;
    ok = expect_verifier_rejects("IR-VERIFY-PROGRAM-FUNC-COUNT-CAP",
        &program,
        "IR-VERIFY-041");

    ir_program_free(&program);
    return ok;
}

static int test_ir_verifier_rejects_out_of_range_global_value_reference(void) {
    IrProgram program;
    int ok;

    if (!lower_source_to_ir_program("int g=1;\nint f(){return g;}\n", &program)) {
        return 0;
    }

    program.functions[0].blocks[0].terminator.as.return_value.kind = IR_VALUE_GLOBAL;
    program.functions[0].blocks[0].terminator.as.return_value.id = 99;
    ok = expect_verifier_rejects("IR-VERIFY-BAD-GLOBAL-REF",
        &program,
        "IR-VERIFY-043");

    ir_program_free(&program);
    return ok;
}

static int test_ir_verifier_rejects_program_global_count_exceeding_capacity(void) {
    IrProgram program;
    int ok;

    if (!lower_source_to_ir_program("int g=1;\nint f(){return g;}\n", &program)) {
        return 0;
    }

    program.global_capacity = 0;
    ok = expect_verifier_rejects("IR-VERIFY-PROGRAM-GLOBAL-COUNT-CAP",
        &program,
        "IR-VERIFY-044");

    ir_program_free(&program);
    return ok;
}

static int test_ir_verifier_rejects_program_null_global_table(void) {
    IrProgram program;
    IrGlobal *saved_globals;
    int ok;

    if (!lower_source_to_ir_program("int g=1;\nint f(){return g;}\n", &program)) {
        return 0;
    }

    saved_globals = program.globals;
    program.globals = NULL;
    ok = expect_verifier_rejects("IR-VERIFY-PROGRAM-NULL-GLOBALS",
        &program,
        "IR-VERIFY-045");
    program.globals = saved_globals;

    ir_program_free(&program);
    return ok;
}

static int test_ir_verifier_rejects_mismatched_global_id_slot(void) {
    IrProgram program;
    int ok;

    if (!lower_source_to_ir_program("int g=1;\nint f(){return g;}\n", &program)) {
        return 0;
    }

    program.globals[0].id = 1;
    ok = expect_verifier_rejects("IR-VERIFY-GLOBAL-ID-MISMATCH",
        &program,
        "IR-VERIFY-046");

    ir_program_free(&program);
    return ok;
}

static int test_ir_verifier_rejects_global_without_name(void) {
    IrProgram program;
    int ok;

    if (!lower_source_to_ir_program("int g=1;\nint f(){return g;}\n", &program)) {
        return 0;
    }

    program.globals[0].name[0] = '\0';
    ok = expect_verifier_rejects("IR-VERIFY-GLOBAL-NAME",
        &program,
        "IR-VERIFY-047");

    ir_program_free(&program);
    return ok;
}

static int test_ir_verifier_rejects_duplicate_global_names(void) {
    IrProgram program;
    char *renamed;
    int ok;

    if (!lower_source_to_ir_program(
            "int a=1;\nint b=2;\nint main(){return a+b;}\n",
            &program)) {
        return 0;
    }

    if (program.global_count < 2) {
        fprintf(stderr,
            "[ir-verify] FAIL: expected at least two globals in duplicate-name setup\n");
        ir_program_free(&program);
        return 0;
    }

    renamed = (char *)malloc(sizeof("a"));
    if (!renamed) {
        ir_program_free(&program);
        return 0;
    }
    memcpy(renamed, "a", sizeof("a"));
    free(program.globals[1].name);
    program.globals[1].name = renamed;

    ok = expect_verifier_rejects("IR-VERIFY-DUP-GLOBAL-NAME",
        &program,
        "IR-VERIFY-062");

    ir_program_free(&program);
    return ok;
}

static int test_ir_verifier_rejects_function_global_symbol_name_collision(void) {
    IrProgram program;
    char *renamed;
    size_t len;
    int ok;

    if (!lower_source_to_ir_program(
            "int g=1;\nint f(){return g;}\n",
            &program)) {
        return 0;
    }

    if (program.global_count == 0 || !program.globals[0].name ||
        !program.functions[0].name) {
        fprintf(stderr,
            "[ir-verify] FAIL: unexpected symbol table shape in namespace-collision setup\n");
        ir_program_free(&program);
        return 0;
    }

    len = strlen(program.globals[0].name);
    renamed = (char *)malloc(len + 1);
    if (!renamed) {
        ir_program_free(&program);
        return 0;
    }
    memcpy(renamed, program.globals[0].name, len + 1);
    free(program.functions[0].name);
    program.functions[0].name = renamed;

    ok = expect_verifier_rejects("IR-VERIFY-FUNC-GLOBAL-NAME-COLLISION",
        &program,
        "IR-VERIFY-063");

    ir_program_free(&program);
    return ok;
}

static int test_ir_verifier_rejects_global_with_both_initializer_kinds(void) {
    IrProgram program;
    int ok;

    if (!lower_source_to_ir_program("int g=1;\nint f(){return g;}\n", &program)) {
        return 0;
    }

    program.globals[0].has_runtime_initializer = 1;
    ok = expect_verifier_rejects("IR-VERIFY-GLOBAL-INIT-FLAGS",
        &program,
        "IR-VERIFY-048");

    ir_program_free(&program);
    return ok;
}

static int test_ir_verifier_rejects_runtime_global_missing_helper_function(void) {
    IrProgram program;
    IrFunction *runtime_init_function;
    int ok;

    if (!lower_source_to_ir_program(
            "int seed(){return 1;}\nint g=seed();\nint main(){return g;}\n",
            &program)) {
        return 0;
    }

    runtime_init_function = find_function_mut(&program, "__global.init");
    if (!runtime_init_function) {
        fprintf(stderr,
            "[ir-verify] FAIL: runtime-init helper missing in setup case\n");
        ir_program_free(&program);
        return 0;
    }

    runtime_init_function->has_body = 0;
    runtime_init_function->block_count = 0;
    ok = expect_verifier_rejects("IR-VERIFY-RUNTIME-GLOBAL-NO-HELPER",
        &program,
        "IR-VERIFY-052");

    ir_program_free(&program);
    return ok;
}

static int test_ir_verifier_rejects_runtime_global_helper_nonzero_parameter_signature(void) {
    IrProgram program;
    IrFunction *runtime_init_function;
    int ok;

    if (!lower_source_to_ir_program(
            "int seed(){return 1;}\nint g=seed();\nint main(){return g;}\n",
            &program)) {
        return 0;
    }

    runtime_init_function = find_function_mut(&program, "__global.init");
    if (!runtime_init_function) {
        fprintf(stderr,
            "[ir-verify] FAIL: runtime-init helper missing in signature case\n");
        ir_program_free(&program);
        return 0;
    }

    runtime_init_function->parameter_count = 1;
    ok = expect_verifier_rejects("IR-VERIFY-RUNTIME-GLOBAL-BAD-SIG",
        &program,
        "IR-VERIFY-051");

    ir_program_free(&program);
    return ok;
}

static int test_ir_verifier_rejects_runtime_global_helper_nonzero_entry_return(void) {
    IrProgram program;
    IrFunction *runtime_init_function;
    int ok;

    if (!lower_source_to_ir_program(
            "int seed(){return 1;}\nint g=seed();\nint main(){return g;}\n",
            &program)) {
        return 0;
    }

    runtime_init_function = find_function_mut(&program, "__global.init");
    if (!runtime_init_function || !runtime_init_function->has_body ||
        runtime_init_function->block_count == 0 || !runtime_init_function->blocks) {
        fprintf(stderr,
            "[ir-verify] FAIL: runtime-init helper missing body in return-shape case\n");
        ir_program_free(&program);
        return 0;
    }

    runtime_init_function->blocks[0].terminator.as.return_value.kind = IR_VALUE_IMMEDIATE;
    runtime_init_function->blocks[0].terminator.as.return_value.immediate = 1;
    ok = expect_verifier_rejects("IR-VERIFY-RUNTIME-GLOBAL-BAD-RET",
        &program,
        "IR-VERIFY-053");

    ir_program_free(&program);
    return ok;
}

static int test_ir_verifier_rejects_runtime_global_missing_startup_call(void) {
    IrProgram program;
    IrFunction *main_function;
    IrInstruction *startup_call;
    int ok;

    if (!lower_source_to_ir_program(
            "int seed(){return 1;}\nint g=seed();\nint main(){return g;}\n",
            &program)) {
        return 0;
    }

    main_function = find_function_mut(&program, "main");
    if (!main_function || !main_function->has_body || main_function->block_count == 0 ||
        main_function->blocks[0].instruction_count == 0 ||
        !main_function->blocks[0].instructions) {
        fprintf(stderr,
            "[ir-verify] FAIL: startup-call case missing main entry instruction\n");
        ir_program_free(&program);
        return 0;
    }

    startup_call = &main_function->blocks[0].instructions[0];
    if (startup_call->kind != IR_INSTR_CALL || !startup_call->as.call.callee_name) {
        fprintf(stderr,
            "[ir-verify] FAIL: startup-call case does not begin with runtime init call\n");
        ir_program_free(&program);
        return 0;
    }

    memcpy(startup_call->as.call.callee_name, "seed", 5);
    ok = expect_verifier_rejects("IR-VERIFY-RUNTIME-GLOBAL-NO-ENTRY-CALL",
        &program,
        "IR-VERIFY-050");

    ir_program_free(&program);
    return ok;
}

static int test_ir_verifier_rejects_reserved_runtime_init_call_outside_startup_entry(void) {
    IrProgram program;
    IrFunction *runtime_init_function;
    IrInstruction *instruction;
    int ok;

    if (!lower_source_to_ir_program(
            "int seed(){return 1;}\nint g=seed();\nint main(){return g;}\n",
            &program)) {
        return 0;
    }

    runtime_init_function = find_function_mut(&program, "__global.init");
    instruction = find_first_call_instruction_mut(runtime_init_function);
    if (!instruction) {
        fprintf(stderr,
            "[ir-verify] FAIL: no call instruction found in __global.init for reserved-call policy case\n");
        ir_program_free(&program);
        return 0;
    }
    if (!replace_call_callee_name(instruction, "__global.init")) {
        fprintf(stderr,
            "[ir-verify] FAIL: unable to rewrite call callee in reserved-call policy case\n");
        ir_program_free(&program);
        return 0;
    }

    ok = expect_verifier_rejects("IR-VERIFY-INIT-HELPER-NONSTARTUP-CALL",
        &program,
        "IR-VERIFY-059");

    ir_program_free(&program);
    return ok;
}

static int test_ir_verifier_rejects_reserved_program_init_call_from_ir_body(void) {
    IrProgram program;
    IrFunction *runtime_init_function;
    IrInstruction *instruction;
    int ok;

    if (!lower_source_to_ir_program(
            "int seed(){return 9;}\nint g=seed();\n",
            &program)) {
        return 0;
    }

    runtime_init_function = find_function_mut(&program, "__global.init");
    instruction = find_first_call_instruction_mut(runtime_init_function);
    if (!instruction) {
        fprintf(stderr,
            "[ir-verify] FAIL: no call instruction found in __global.init for program-init policy case\n");
        ir_program_free(&program);
        return 0;
    }
    if (!replace_call_callee_name(instruction, "__program.init")) {
        fprintf(stderr,
            "[ir-verify] FAIL: unable to rewrite call callee in program-init policy case\n");
        ir_program_free(&program);
        return 0;
    }

    ok = expect_verifier_rejects("IR-VERIFY-PROGRAM-INIT-CALLED-FROM-BODY",
        &program,
        "IR-VERIFY-060");

    ir_program_free(&program);
    return ok;
}

static int test_ir_verifier_rejects_no_main_program_init_with_non_init_first_call(void) {
    IrProgram program;
    IrFunction *program_init_function;
    IrInstruction *first_instruction;
    int ok;

    if (!lower_source_to_ir_program(
            "int seed(){return 9;}\nint g=seed();\n",
            &program)) {
        return 0;
    }

    program_init_function = find_function_mut(&program, "__program.init");
    if (!program_init_function || !program_init_function->has_body ||
        program_init_function->block_count == 0 || !program_init_function->blocks ||
        program_init_function->blocks[0].instruction_count == 0 ||
        !program_init_function->blocks[0].instructions) {
        fprintf(stderr,
            "[ir-verify] FAIL: no-main fallback missing __program.init entry instruction\n");
        ir_program_free(&program);
        return 0;
    }

    first_instruction = &program_init_function->blocks[0].instructions[0];
    if (first_instruction->kind != IR_INSTR_CALL || !first_instruction->as.call.callee_name) {
        fprintf(stderr,
            "[ir-verify] FAIL: no-main fallback does not begin with call instruction\n");
        ir_program_free(&program);
        return 0;
    }

    memcpy(first_instruction->as.call.callee_name, "seed", 5);
    ok = expect_verifier_rejects("IR-VERIFY-NO-MAIN-PROGRAM-INIT-CALL",
        &program,
        "IR-VERIFY-054");

    ir_program_free(&program);
    return ok;
}

static int test_ir_verifier_rejects_no_main_program_init_with_extra_entry_instruction(void) {
    IrProgram program;
    IrFunction *program_init_function;
    IrBasicBlock *entry_block;
    int ok;

    if (!lower_source_to_ir_program(
            "int seed(){return 9;}\nint g=seed();\n",
            &program)) {
        return 0;
    }

    program_init_function = find_function_mut(&program, "__program.init");
    if (!program_init_function || !program_init_function->has_body ||
        program_init_function->block_count == 0 || !program_init_function->blocks) {
        fprintf(stderr,
            "[ir-verify] FAIL: no-main fallback missing __program.init in extra-instr case\n");
        ir_program_free(&program);
        return 0;
    }

    entry_block = &program_init_function->blocks[0];
    if (entry_block->instruction_capacity < 2 || !entry_block->instructions ||
        entry_block->instruction_count != 1) {
        fprintf(stderr,
            "[ir-verify] FAIL: unexpected __program.init entry block shape in extra-instr case\n");
        ir_program_free(&program);
        return 0;
    }

    entry_block->instructions[1].kind = IR_INSTR_MOV;
    entry_block->instructions[1].result.kind = IR_VALUE_TEMP;
    entry_block->instructions[1].result.id = 999;
    entry_block->instructions[1].result.immediate = 0;
    entry_block->instructions[1].as.mov_value.kind = IR_VALUE_IMMEDIATE;
    entry_block->instructions[1].as.mov_value.immediate = 0;
    entry_block->instructions[1].as.mov_value.id = 0;
    entry_block->instruction_count = 2;
    ok = expect_verifier_rejects("IR-VERIFY-NO-MAIN-PROGRAM-INIT-EXTRA-INSTR",
        &program,
        "IR-VERIFY-055");

    ir_program_free(&program);
    return ok;
}

static int test_ir_verifier_rejects_reserved_init_helper_name_without_runtime_globals(void) {
    IrProgram program;
    char *renamed;
    int ok;

    if (!lower_source_to_ir_program("int f(){return 0;}\n", &program)) {
        return 0;
    }

    renamed = (char *)malloc(sizeof("__global.init"));
    if (!renamed) {
        ir_program_free(&program);
        return 0;
    }
    memcpy(renamed, "__global.init", sizeof("__global.init"));
    free(program.functions[0].name);
    program.functions[0].name = renamed;

    ok = expect_verifier_rejects("IR-VERIFY-RESERVED-INIT-NAME",
        &program,
        "IR-VERIFY-056");

    ir_program_free(&program);
    return ok;
}

static int test_ir_verifier_rejects_program_null_function_table(void) {
    IrProgram program;
    IrFunction *saved_functions;
    int ok;

    if (!lower_source_to_ir_program("int f(){return 0;}\n", &program)) {
        return 0;
    }

    saved_functions = program.functions;
    program.functions = NULL;
    ok = expect_verifier_rejects("IR-VERIFY-PROGRAM-NULL-FUNCS",
        &program,
        "IR-VERIFY-042");
    program.functions = saved_functions;

    ir_program_free(&program);
    return ok;
}

int main(void) {
    int ok = 1;

    ok &= test_ir_verifier_accepts_lowered_program();
    ok &= test_ir_verifier_accepts_declaration_only_signature();
    ok &= test_ir_verifier_accepts_runtime_global_startup_helper_flow();
    ok &= test_ir_verifier_rejects_missing_terminator();
    ok &= test_ir_verifier_rejects_unreachable_block();
    ok &= test_ir_verifier_rejects_unknown_binary_op();
    ok &= test_ir_verifier_rejects_non_temp_binary_result();
    ok &= test_ir_verifier_rejects_empty_call_callee();
    ok &= test_ir_verifier_rejects_non_temp_call_result();
    ok &= test_ir_verifier_rejects_undefined_temp_slot();
    ok &= test_ir_verifier_rejects_temp_use_before_definition();
    ok &= test_ir_verifier_rejects_duplicate_temp_definition();
    ok &= test_ir_verifier_rejects_internal_call_argument_mismatch();
    ok &= test_ir_verifier_rejects_declared_call_argument_mismatch();
    ok &= test_ir_verifier_rejects_unknown_call_target();
    ok &= test_ir_verifier_rejects_null_call_args_payload();
    ok &= test_ir_verifier_rejects_duplicate_function_entry_names();
    ok &= test_ir_verifier_rejects_temps_on_declaration();
    ok &= test_ir_verifier_rejects_non_parameter_local_on_declaration();
    ok &= test_ir_verifier_rejects_local_count_exceeding_capacity();
    ok &= test_ir_verifier_rejects_block_count_exceeding_capacity();
    ok &= test_ir_verifier_rejects_instruction_count_exceeding_capacity();
    ok &= test_ir_verifier_rejects_null_local_table();
    ok &= test_ir_verifier_rejects_null_block_table();
    ok &= test_ir_verifier_rejects_null_instruction_table();
    ok &= test_ir_verifier_rejects_zero_arg_call_with_non_null_payload();
    ok &= test_ir_verifier_rejects_blocks_on_declaration();
    ok &= test_ir_verifier_rejects_program_function_count_exceeding_capacity();
    ok &= test_ir_verifier_rejects_out_of_range_global_value_reference();
    ok &= test_ir_verifier_rejects_program_global_count_exceeding_capacity();
    ok &= test_ir_verifier_rejects_program_null_global_table();
    ok &= test_ir_verifier_rejects_mismatched_global_id_slot();
    ok &= test_ir_verifier_rejects_global_without_name();
    ok &= test_ir_verifier_rejects_duplicate_global_names();
    ok &= test_ir_verifier_rejects_function_global_symbol_name_collision();
    ok &= test_ir_verifier_rejects_global_with_both_initializer_kinds();
    ok &= test_ir_verifier_rejects_runtime_global_missing_helper_function();
    ok &= test_ir_verifier_rejects_runtime_global_helper_nonzero_parameter_signature();
    ok &= test_ir_verifier_rejects_runtime_global_helper_nonzero_entry_return();
    ok &= test_ir_verifier_rejects_runtime_global_missing_startup_call();
    ok &= test_ir_verifier_rejects_reserved_runtime_init_call_outside_startup_entry();
    ok &= test_ir_verifier_rejects_reserved_program_init_call_from_ir_body();
    ok &= test_ir_verifier_rejects_no_main_program_init_with_non_init_first_call();
    ok &= test_ir_verifier_rejects_no_main_program_init_with_extra_entry_instruction();
    ok &= test_ir_verifier_rejects_reserved_init_helper_name_without_runtime_globals();
    ok &= test_ir_verifier_rejects_program_null_function_table();

    if (!ok) {
        return 1;
    }

    printf("[ir-verify] All IR verifier tests passed.\n");
    return 0;
}
