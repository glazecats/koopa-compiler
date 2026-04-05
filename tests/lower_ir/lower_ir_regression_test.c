#include "lower_ir.h"

#include "ast.h"
#include "lexer.h"
#include "parser.h"
#include "semantic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int lower_source_to_lower_ir_text(const char *source, char **out_text) {
    TokenArray tokens;
    AstProgram ast_program;
    ParserError parse_err;
    SemanticError sema_err;
    IrProgram ir_program;
    IrError ir_err;
    LowerIrProgram lower_program;
    LowerIrError lower_err;

    if (!source || !out_text) {
        return 0;
    }

    *out_text = NULL;
    lexer_init_tokens(&tokens);
    ast_program_init(&ast_program);
    ir_program_init(&ir_program);
    lower_ir_program_init(&lower_program);

    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[lower-ir-reg] FAIL: lexer failed for input\n");
        return 0;
    }

    if (!parser_parse_translation_unit_ast(&tokens, &ast_program, &parse_err)) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: parser failed at %d:%d: %s\n",
            parse_err.line,
            parse_err.column,
            parse_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&ast_program);
        return 0;
    }

    if (!semantic_analyze_program(&ast_program, &sema_err)) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: semantic failed at %d:%d: %s\n",
            sema_err.line,
            sema_err.column,
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&ast_program);
        return 0;
    }

    if (!ir_lower_program(&ast_program, &ir_program, &ir_err)) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: canonical IR lowering failed at %d:%d: %s\n",
            ir_err.line,
            ir_err.column,
            ir_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&ast_program);
        ir_program_free(&ir_program);
        return 0;
    }

    if (!lower_ir_lower_from_ir(&ir_program, &lower_program, &lower_err)) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: lower IR lowering failed at %d:%d: %s\n",
            lower_err.line,
            lower_err.column,
            lower_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&ast_program);
        ir_program_free(&ir_program);
        lower_ir_program_free(&lower_program);
        return 0;
    }

    if (!lower_ir_verify_program(&lower_program, &lower_err)) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: lowered program rejected at %d:%d: %s\n",
            lower_err.line,
            lower_err.column,
            lower_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&ast_program);
        ir_program_free(&ir_program);
        lower_ir_program_free(&lower_program);
        return 0;
    }

    if (!lower_ir_dump_program(&lower_program, out_text)) {
        fprintf(stderr, "[lower-ir-reg] FAIL: lower IR dump failed\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&ast_program);
        ir_program_free(&ir_program);
        lower_ir_program_free(&lower_program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&ast_program);
    ir_program_free(&ir_program);
    lower_ir_program_free(&lower_program);
    return 1;
}

static int build_sample_program(LowerIrProgram *program, LowerIrError *error) {
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

static int expect_dump(const char *case_id, const char *expected_text) {
    LowerIrProgram program;
    LowerIrError error;
    char *actual_text = NULL;
    int ok;

    if (!build_sample_program(&program, &error)) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: %s setup failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        return 0;
    }

    if (!lower_ir_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[lower-ir-reg] FAIL: %s dump failed\n", case_id);
        lower_ir_program_free(&program);
        return 0;
    }

    ok = strcmp(actual_text, expected_text) == 0;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: %s dump mismatch\nexpected:\n%s\nactual:\n%s\n",
            case_id,
            expected_text,
            actual_text);
    }

    free(actual_text);
    lower_ir_program_free(&program);
    return ok;
}

static int expect_lowered_source_dump(const char *case_id,
    const char *source,
    const char *expected_text) {
    char *actual_text = NULL;
    int ok;

    if (!case_id || !source || !expected_text) {
        return 0;
    }

    if (!lower_source_to_lower_ir_text(source, &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = strcmp(actual_text, expected_text) == 0;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: %s dump mismatch\nexpected:\n%s\nactual:\n%s\n",
            case_id,
            expected_text,
            actual_text);
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_builder_rejects_declaration_block_append(void) {
    LowerIrProgram program;
    LowerIrError error;
    LowerIrFunction *decl;
    int ok;

    lower_ir_program_init(&program);
    if (!lower_ir_program_append_function(&program, "decl", 0, &decl, &error)) {
        return 0;
    }

    ok = !lower_ir_function_append_block(decl, NULL, NULL, &error);
    if (!ok || !strstr(error.message, "LOWER-IR-038")) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-BUILDER-DECL-BLOCK expected LOWER-IR-038, got: %s\n",
            ok ? error.message : "<success>");
        lower_ir_program_free(&program);
        return 0;
    }

    lower_ir_program_free(&program);
    return 1;
}

static int test_lower_ir_builder_rejects_temp_allocation_on_declaration(void) {
    LowerIrProgram program;
    LowerIrError error;
    LowerIrFunction *decl;
    size_t temp_id;

    lower_ir_program_init(&program);
    if (!lower_ir_program_append_function(&program, "decl", 0, &decl, &error)) {
        return 0;
    }

    temp_id = lower_ir_function_allocate_temp(decl);
    if (temp_id != (size_t)-1) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-BUILDER-DECL-TEMP expected temp allocation to fail\n");
        lower_ir_program_free(&program);
        return 0;
    }

    lower_ir_program_free(&program);
    return 1;
}

static int test_lower_ir_builder_rejects_parameter_after_nonparameter_local(void) {
    LowerIrProgram program;
    LowerIrError error;
    LowerIrFunction *function;
    int ok;

    lower_ir_program_init(&program);
    if (!lower_ir_program_append_function(&program, "main", 1, &function, &error) ||
        !lower_ir_function_append_local(function, "x", 0, NULL, &error)) {
        lower_ir_program_free(&program);
        return 0;
    }

    ok = !lower_ir_function_append_local(function, "y", 1, NULL, &error);
    if (!ok || !strstr(error.message, "LOWER-IR-037")) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-BUILDER-PARAM-PREFIX expected LOWER-IR-037, got: %s\n",
            ok ? error.message : "<success>");
        lower_ir_program_free(&program);
        return 0;
    }

    lower_ir_program_free(&program);
    return 1;
}

static int test_lower_ir_builder_rejects_instruction_append_after_terminator(void) {
    LowerIrProgram program;
    LowerIrError error;
    LowerIrFunction *function;
    LowerIrBasicBlock *block;
    LowerIrInstruction instruction;
    int ok;

    lower_ir_program_init(&program);
    if (!lower_ir_program_append_function(&program, "main", 1, &function, &error) ||
        !lower_ir_function_append_block(function, NULL, &block, &error) ||
        !lower_ir_block_set_return(block, lower_ir_value_immediate(0), &error)) {
        lower_ir_program_free(&program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(0);
    instruction.as.mov_value = lower_ir_value_immediate(1);

    ok = !lower_ir_block_append_instruction(block, &instruction, &error);
    if (!ok || !strstr(error.message, "LOWER-IR-039")) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-BUILDER-POST-TERM-INSTR expected LOWER-IR-039, got: %s\n",
            ok ? error.message : "<success>");
        lower_ir_program_free(&program);
        return 0;
    }

    lower_ir_program_free(&program);
    return 1;
}

static int test_lower_ir_builder_rejects_terminator_overwrite(void) {
    LowerIrProgram program;
    LowerIrError error;
    LowerIrFunction *function;
    LowerIrBasicBlock *block;
    int ok;

    lower_ir_program_init(&program);
    if (!lower_ir_program_append_function(&program, "main", 1, &function, &error) ||
        !lower_ir_function_append_block(function, NULL, &block, &error) ||
        !lower_ir_block_set_return(block, lower_ir_value_immediate(0), &error)) {
        lower_ir_program_free(&program);
        return 0;
    }

    ok = !lower_ir_block_set_jump(block, 0, &error);
    if (!ok || !strstr(error.message, "LOWER-IR-040")) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-BUILDER-TERM-OVERWRITE expected LOWER-IR-040, got: %s\n",
            ok ? error.message : "<success>");
        lower_ir_program_free(&program);
        return 0;
    }

    lower_ir_program_free(&program);
    return 1;
}

static int test_lower_ir_dump_explicit_memory_flow(void) {
    return expect_dump("LOWER-IR-DUMP-001",
        "global g.0\n"
        "\n"
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    tmp.0 = load_local a.0\n"
        "    tmp.1 = add tmp.0, 1\n"
        "    store_global g.0, tmp.1\n"
        "    ret tmp.1\n"
        "}\n");
}

static int test_lower_ir_lowers_return_parameter_to_explicit_load(void) {
    return expect_lowered_source_dump("LOWER-IR-LOWER-RET-PARAM",
        "int id(int a){return a;}\n",
        "func id(a.0) {\n"
        "  bb.0:\n"
        "    tmp.0 = load_local a.0\n"
        "    ret tmp.0\n"
        "}\n");
}

static int test_lower_ir_lowers_assignment_and_return_via_store_and_load(void) {
    return expect_lowered_source_dump("LOWER-IR-LOWER-ASSIGN-RET",
        "int f(int a,int b){a=b+1; return a;}\n",
        "func f(a.0, b.1) {\n"
        "  bb.0:\n"
        "    tmp.1 = load_local b.1\n"
        "    tmp.0 = add tmp.1, 1\n"
        "    store_local a.0, tmp.0\n"
        "    tmp.2 = load_local a.0\n"
        "    ret tmp.2\n"
        "}\n");
}

static int test_lower_ir_lowers_global_store_and_reload(void) {
    return expect_lowered_source_dump("LOWER-IR-LOWER-GLOBAL-ROUNDTRIP",
        "int g=1;\nint main(int a){g=a; return g;}\n",
        "global g.0 = 1\n"
        "\n"
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    tmp.0 = load_local a.0\n"
        "    store_global g.0, tmp.0\n"
        "    tmp.1 = load_global g.0\n"
        "    ret tmp.1\n"
        "}\n");
}

static int test_lower_ir_lowers_branch_condition_via_load(void) {
    return expect_lowered_source_dump("LOWER-IR-LOWER-BRANCH-COND",
        "int f(int a){if(a)return 1; return 0;}\n",
        "func f(a.0) {\n"
        "  bb.0:\n"
        "    tmp.0 = load_local a.0\n"
        "    br tmp.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    ret 1\n"
        "  bb.2:\n"
        "    ret 0\n"
        "}\n");
}

static int test_lower_ir_lowers_call_argument_via_load(void) {
    return expect_lowered_source_dump("LOWER-IR-LOWER-CALL-ARG",
        "int id(int x){return x;}\nint main(int a){return id(a);}\n",
        "func id(x.0) {\n"
        "  bb.0:\n"
        "    tmp.0 = load_local x.0\n"
        "    ret tmp.0\n"
        "}\n"
        "\n"
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    tmp.1 = load_local a.0\n"
        "    tmp.0 = call id(tmp.1)\n"
        "    ret tmp.0\n"
        "}\n");
}

static int test_lower_ir_lowers_if_without_else_join(void) {
    return expect_lowered_source_dump("LOWER-IR-LOWER-IF-JOIN",
        "int f(int a){int x=0; if(a) x=1; return x;}\n",
        "func f(a.0) {\n"
        "  bb.0:\n"
        "    store_local x.1, 0\n"
        "    tmp.0 = load_local a.0\n"
        "    br tmp.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    store_local x.1, 1\n"
        "    jmp bb.2\n"
        "  bb.2:\n"
        "    tmp.1 = load_local x.1\n"
        "    ret tmp.1\n"
        "}\n");
}

static int test_lower_ir_lowers_if_else_join(void) {
    return expect_lowered_source_dump("LOWER-IR-LOWER-IF-ELSE-JOIN",
        "int f(int a){int x=0; if(a) x=1; else x=2; return x;}\n",
        "func f(a.0) {\n"
        "  bb.0:\n"
        "    store_local x.1, 0\n"
        "    tmp.0 = load_local a.0\n"
        "    br tmp.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    store_local x.1, 1\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    store_local x.1, 2\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    tmp.1 = load_local x.1\n"
        "    ret tmp.1\n"
        "}\n");
}

static int test_lower_ir_lowers_if_logical_short_circuit_condition(void) {
    return expect_lowered_source_dump("LOWER-IR-LOWER-IF-LOGICAL-COND",
        "int f(int a,int b){if(a&&b) return 1; return 0;}\n",
        "func f(a.0, b.1) {\n"
        "  bb.0:\n"
        "    tmp.0 = load_local a.0\n"
        "    br tmp.0, bb.3, bb.2\n"
        "  bb.1:\n"
        "    ret 1\n"
        "  bb.2:\n"
        "    ret 0\n"
        "  bb.3:\n"
        "    tmp.1 = load_local b.1\n"
        "    br tmp.1, bb.1, bb.2\n"
        "}\n");
}

static int test_lower_ir_lowers_while_backedge(void) {
    return expect_lowered_source_dump("LOWER-IR-LOWER-WHILE-BACKEDGE",
        "int f(int a){while(a){a=a-1;} return a;}\n",
        "func f(a.0) {\n"
        "  bb.0:\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    tmp.1 = load_local a.0\n"
        "    br tmp.1, bb.2, bb.3\n"
        "  bb.2:\n"
        "    tmp.2 = load_local a.0\n"
        "    tmp.0 = sub tmp.2, 1\n"
        "    store_local a.0, tmp.0\n"
        "    jmp bb.1\n"
        "  bb.3:\n"
        "    tmp.3 = load_local a.0\n"
        "    ret tmp.3\n"
        "}\n");
}

static int test_lower_ir_lowers_while_break_exit(void) {
    return expect_lowered_source_dump("LOWER-IR-LOWER-WHILE-BREAK",
        "int f(int a){while(a){if(a<3) break; a=a-1;} return a;}\n",
        "func f(a.0) {\n"
        "  bb.0:\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    tmp.2 = load_local a.0\n"
        "    br tmp.2, bb.2, bb.3\n"
        "  bb.2:\n"
        "    tmp.3 = load_local a.0\n"
        "    tmp.0 = lt tmp.3, 3\n"
        "    br tmp.0, bb.4, bb.5\n"
        "  bb.3:\n"
        "    tmp.4 = load_local a.0\n"
        "    ret tmp.4\n"
        "  bb.4:\n"
        "    jmp bb.3\n"
        "  bb.5:\n"
        "    tmp.5 = load_local a.0\n"
        "    tmp.1 = sub tmp.5, 1\n"
        "    store_local a.0, tmp.1\n"
        "    jmp bb.1\n"
        "}\n");
}

static int test_lower_ir_lowers_for_init_step_cfg(void) {
    return expect_lowered_source_dump("LOWER-IR-LOWER-FOR-INIT-STEP",
        "int f(){for(int a=3;a;a=a-1){} return 0;}\n",
        "func f() {\n"
        "  bb.0:\n"
        "    store_local a.0, 3\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    tmp.1 = load_local a.0\n"
        "    br tmp.1, bb.2, bb.4\n"
        "  bb.2:\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    tmp.2 = load_local a.0\n"
        "    tmp.0 = sub tmp.2, 1\n"
        "    store_local a.0, tmp.0\n"
        "    jmp bb.1\n"
        "  bb.4:\n"
        "    ret 0\n"
        "}\n");
}

static int test_lower_ir_lowers_nested_loop_break_continue(void) {
    return expect_lowered_source_dump("LOWER-IR-LOWER-NESTED-LOOP-CONTROL",
        "int f(int a){for(;a;a=a-1){while(a){break;} continue;} return a;}\n",
        "func f(a.0) {\n"
        "  bb.0:\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    tmp.1 = load_local a.0\n"
        "    br tmp.1, bb.2, bb.4\n"
        "  bb.2:\n"
        "    jmp bb.5\n"
        "  bb.3:\n"
        "    tmp.2 = load_local a.0\n"
        "    tmp.0 = sub tmp.2, 1\n"
        "    store_local a.0, tmp.0\n"
        "    jmp bb.1\n"
        "  bb.4:\n"
        "    tmp.3 = load_local a.0\n"
        "    ret tmp.3\n"
        "  bb.5:\n"
        "    tmp.4 = load_local a.0\n"
        "    br tmp.4, bb.6, bb.7\n"
        "  bb.6:\n"
        "    jmp bb.7\n"
        "  bb.7:\n"
        "    jmp bb.3\n"
        "}\n");
}

static int test_lower_ir_lowers_global_if_else_join(void) {
    return expect_lowered_source_dump("LOWER-IR-LOWER-GLOBAL-IF-ELSE-JOIN",
        "int g=0;\nint f(int a){if(a) g=1; else g=2; return g;}\n",
        "global g.0 = 0\n"
        "\n"
        "func f(a.0) {\n"
        "  bb.0:\n"
        "    tmp.0 = load_local a.0\n"
        "    br tmp.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    store_global g.0, 1\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    store_global g.0, 2\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    tmp.1 = load_global g.0\n"
        "    ret tmp.1\n"
        "}\n");
}

static int test_lower_ir_lowers_declared_calls_in_for_condition_and_step(void) {
    return expect_lowered_source_dump("LOWER-IR-LOWER-DECL-FOR-COND-STEP",
        "int pred(int a);\nint step(int a);\nint main(int x){for(;pred(x);x=step(x)){} return x;}\n",
        "declare pred(a.0)\n"
        "\n"
        "declare step(a.0)\n"
        "\n"
        "func main(x.0) {\n"
        "  bb.0:\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    tmp.2 = load_local x.0\n"
        "    tmp.0 = call pred(tmp.2)\n"
        "    br tmp.0, bb.2, bb.4\n"
        "  bb.2:\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    tmp.3 = load_local x.0\n"
        "    tmp.1 = call step(tmp.3)\n"
        "    store_local x.0, tmp.1\n"
        "    jmp bb.1\n"
        "  bb.4:\n"
        "    tmp.4 = load_local x.0\n"
        "    ret tmp.4\n"
        "}\n");
}

static int test_lower_ir_lowers_declared_calls_in_for_init_condition_and_step(void) {
    return expect_lowered_source_dump("LOWER-IR-LOWER-DECL-FOR-INIT-COND-STEP",
        "int init(int a);\nint pred(int a);\nint step(int a);\nint main(int x){for(x=init(x);pred(x);x=step(x)){} return x;}\n",
        "declare init(a.0)\n"
        "\n"
        "declare pred(a.0)\n"
        "\n"
        "declare step(a.0)\n"
        "\n"
        "func main(x.0) {\n"
        "  bb.0:\n"
        "    tmp.3 = load_local x.0\n"
        "    tmp.0 = call init(tmp.3)\n"
        "    store_local x.0, tmp.0\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    tmp.4 = load_local x.0\n"
        "    tmp.1 = call pred(tmp.4)\n"
        "    br tmp.1, bb.2, bb.4\n"
        "  bb.2:\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    tmp.5 = load_local x.0\n"
        "    tmp.2 = call step(tmp.5)\n"
        "    store_local x.0, tmp.2\n"
        "    jmp bb.1\n"
        "  bb.4:\n"
        "    tmp.6 = load_local x.0\n"
        "    ret tmp.6\n"
        "}\n");
}

static int test_lower_ir_lowers_declared_calls_in_for_logical_and_condition_and_step(void) {
    return expect_lowered_source_dump("LOWER-IR-LOWER-DECL-FOR-COND-AND-STEP",
        "int pred(int a);\nint gate(int a);\nint step(int a);\nint main(int x){for(;pred(x)&&gate(x);x=step(x)){} return x;}\n",
        "declare pred(a.0)\n"
        "\n"
        "declare gate(a.0)\n"
        "\n"
        "declare step(a.0)\n"
        "\n"
        "func main(x.0) {\n"
        "  bb.0:\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    tmp.3 = load_local x.0\n"
        "    tmp.0 = call pred(tmp.3)\n"
        "    br tmp.0, bb.5, bb.4\n"
        "  bb.2:\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    tmp.4 = load_local x.0\n"
        "    tmp.2 = call step(tmp.4)\n"
        "    store_local x.0, tmp.2\n"
        "    jmp bb.1\n"
        "  bb.4:\n"
        "    tmp.5 = load_local x.0\n"
        "    ret tmp.5\n"
        "  bb.5:\n"
        "    tmp.6 = load_local x.0\n"
        "    tmp.1 = call gate(tmp.6)\n"
        "    br tmp.1, bb.2, bb.4\n"
        "}\n");
}

static int test_lower_ir_lowers_logical_and_value(void) {
    return expect_lowered_source_dump("LOWER-IR-LOWER-LOGICAL-AND-VALUE",
        "int f(int a,int b){return a&&b;}\n",
        "func f(a.0, b.1) {\n"
        "  bb.0:\n"
        "    tmp.1 = load_local a.0\n"
        "    br tmp.1, bb.3, bb.2\n"
        "  bb.1:\n"
        "    tmp.0 = mov 1\n"
        "    jmp bb.4\n"
        "  bb.2:\n"
        "    tmp.0 = mov 0\n"
        "    jmp bb.4\n"
        "  bb.3:\n"
        "    tmp.2 = load_local b.1\n"
        "    br tmp.2, bb.1, bb.2\n"
        "  bb.4:\n"
        "    ret tmp.0\n"
        "}\n");
}

static int test_lower_ir_lowers_ternary_value_expression(void) {
    return expect_lowered_source_dump("LOWER-IR-LOWER-TERNARY-VALUE",
        "int f(int a,int b,int c){return a ? b+1 : c+2;}\n",
        "func f(a.0, b.1, c.2) {\n"
        "  bb.0:\n"
        "    tmp.3 = load_local a.0\n"
        "    br tmp.3, bb.1, bb.2\n"
        "  bb.1:\n"
        "    tmp.4 = load_local b.1\n"
        "    tmp.1 = add tmp.4, 1\n"
        "    tmp.0 = mov tmp.1\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    tmp.5 = load_local c.2\n"
        "    tmp.2 = add tmp.5, 2\n"
        "    tmp.0 = mov tmp.2\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ret tmp.0\n"
        "}\n");
}

static int test_lower_ir_lowers_declared_call_in_logical_value_expression(void) {
    return expect_lowered_source_dump("LOWER-IR-LOWER-DECL-LOGICAL-VALUE",
        "int pred(int a);\nint main(int x){return pred(x)&&pred(1);}\n",
        "declare pred(a.0)\n"
        "\n"
        "func main(x.0) {\n"
        "  bb.0:\n"
        "    tmp.3 = load_local x.0\n"
        "    tmp.1 = call pred(tmp.3)\n"
        "    br tmp.1, bb.3, bb.2\n"
        "  bb.1:\n"
        "    tmp.0 = mov 1\n"
        "    jmp bb.4\n"
        "  bb.2:\n"
        "    tmp.0 = mov 0\n"
        "    jmp bb.4\n"
        "  bb.3:\n"
        "    tmp.2 = call pred(1)\n"
        "    br tmp.2, bb.1, bb.2\n"
        "  bb.4:\n"
        "    ret tmp.0\n"
        "}\n");
}

static int test_lower_ir_lowers_declared_call_in_nested_logical_mix_value_expression(void) {
    return expect_lowered_source_dump("LOWER-IR-LOWER-DECL-LOGICAL-NESTED-MIX-VALUE",
        "int pred(int a);\nint gate(int a);\nint tail(int a);\nint main(int x){return (pred(x)&&gate(x))||tail(x);}\n",
        "declare pred(a.0)\n"
        "\n"
        "declare gate(a.0)\n"
        "\n"
        "declare tail(a.0)\n"
        "\n"
        "func main(x.0) {\n"
        "  bb.0:\n"
        "    tmp.5 = load_local x.0\n"
        "    tmp.2 = call pred(tmp.5)\n"
        "    br tmp.2, bb.6, bb.5\n"
        "  bb.1:\n"
        "    tmp.0 = mov 1\n"
        "    jmp bb.8\n"
        "  bb.2:\n"
        "    tmp.0 = mov 0\n"
        "    jmp bb.8\n"
        "  bb.3:\n"
        "    tmp.6 = load_local x.0\n"
        "    tmp.4 = call tail(tmp.6)\n"
        "    br tmp.4, bb.1, bb.2\n"
        "  bb.4:\n"
        "    tmp.1 = mov 1\n"
        "    jmp bb.7\n"
        "  bb.5:\n"
        "    tmp.1 = mov 0\n"
        "    jmp bb.7\n"
        "  bb.6:\n"
        "    tmp.7 = load_local x.0\n"
        "    tmp.3 = call gate(tmp.7)\n"
        "    br tmp.3, bb.4, bb.5\n"
        "  bb.7:\n"
        "    br tmp.1, bb.1, bb.3\n"
        "  bb.8:\n"
        "    ret tmp.0\n"
        "}\n");
}

static int test_lower_ir_lowers_runtime_global_initializer_startup_flow(void) {
    return expect_lowered_source_dump("LOWER-IR-LOWER-RUNTIME-GLOBAL-INIT",
        "int seed();\nint g=seed();\nint seed(){return 7;}\nint main(){return g;}\n",
        "global g.0\n"
        "\n"
        "func seed() {\n"
        "  bb.0:\n"
        "    ret 7\n"
        "}\n"
        "\n"
        "func __global.init() {\n"
        "  bb.0:\n"
        "    tmp.0 = call seed()\n"
        "    store_global g.0, tmp.0\n"
        "    ret 0\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    tmp.0 = call __global.init()\n"
        "    tmp.1 = load_global g.0\n"
        "    ret tmp.1\n"
        "}\n");
}

static int test_lower_ir_lowers_runtime_global_startup_with_branchy_main(void) {
    return expect_lowered_source_dump("LOWER-IR-LOWER-RUNTIME-GLOBAL-BRANCHY-MAIN",
        "int seed(){return 7;}\nint g=seed();\nint main(int a){if(a) return g; return g+1;}\n",
        "global g.0\n"
        "\n"
        "func seed() {\n"
        "  bb.0:\n"
        "    ret 7\n"
        "}\n"
        "\n"
        "func __global.init() {\n"
        "  bb.0:\n"
        "    tmp.0 = call seed()\n"
        "    store_global g.0, tmp.0\n"
        "    ret 0\n"
        "}\n"
        "\n"
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    tmp.1 = call __global.init()\n"
        "    tmp.2 = load_local a.0\n"
        "    br tmp.2, bb.1, bb.2\n"
        "  bb.1:\n"
        "    tmp.3 = load_global g.0\n"
        "    ret tmp.3\n"
        "  bb.2:\n"
        "    tmp.4 = load_global g.0\n"
        "    tmp.0 = add tmp.4, 1\n"
        "    ret tmp.0\n"
        "}\n");
}

static int test_lower_ir_lowers_program_initializer_fallback(void) {
    return expect_lowered_source_dump("LOWER-IR-LOWER-PROGRAM-INIT-FALLBACK",
        "int seed();\nint g=seed();\nint seed(){return 9;}\n",
        "global g.0\n"
        "\n"
        "func seed() {\n"
        "  bb.0:\n"
        "    ret 9\n"
        "}\n"
        "\n"
        "func __global.init() {\n"
        "  bb.0:\n"
        "    tmp.0 = call seed()\n"
        "    store_global g.0, tmp.0\n"
        "    ret 0\n"
        "}\n"
        "\n"
        "func __program.init() {\n"
        "  bb.0:\n"
        "    tmp.0 = call __global.init()\n"
        "    ret 0\n"
        "}\n");
}

int main(void) {
    int ok = 1;

    ok &= test_lower_ir_builder_rejects_declaration_block_append();
    ok &= test_lower_ir_builder_rejects_temp_allocation_on_declaration();
    ok &= test_lower_ir_builder_rejects_parameter_after_nonparameter_local();
    ok &= test_lower_ir_builder_rejects_instruction_append_after_terminator();
    ok &= test_lower_ir_builder_rejects_terminator_overwrite();
    ok &= test_lower_ir_dump_explicit_memory_flow();
    ok &= test_lower_ir_lowers_return_parameter_to_explicit_load();
    ok &= test_lower_ir_lowers_assignment_and_return_via_store_and_load();
    ok &= test_lower_ir_lowers_global_store_and_reload();
    ok &= test_lower_ir_lowers_branch_condition_via_load();
    ok &= test_lower_ir_lowers_call_argument_via_load();
    ok &= test_lower_ir_lowers_if_without_else_join();
    ok &= test_lower_ir_lowers_if_else_join();
    ok &= test_lower_ir_lowers_if_logical_short_circuit_condition();
    ok &= test_lower_ir_lowers_while_backedge();
    ok &= test_lower_ir_lowers_while_break_exit();
    ok &= test_lower_ir_lowers_for_init_step_cfg();
    ok &= test_lower_ir_lowers_nested_loop_break_continue();
    ok &= test_lower_ir_lowers_global_if_else_join();
    ok &= test_lower_ir_lowers_declared_calls_in_for_condition_and_step();
    ok &= test_lower_ir_lowers_declared_calls_in_for_init_condition_and_step();
    ok &= test_lower_ir_lowers_declared_calls_in_for_logical_and_condition_and_step();
    ok &= test_lower_ir_lowers_logical_and_value();
    ok &= test_lower_ir_lowers_ternary_value_expression();
    ok &= test_lower_ir_lowers_declared_call_in_logical_value_expression();
    ok &= test_lower_ir_lowers_declared_call_in_nested_logical_mix_value_expression();
    ok &= test_lower_ir_lowers_runtime_global_initializer_startup_flow();
    ok &= test_lower_ir_lowers_runtime_global_startup_with_branchy_main();
    ok &= test_lower_ir_lowers_program_initializer_fallback();

    if (!ok) {
        return 1;
    }

    printf("[lower-ir-reg] PASS\n");
    return 0;
}
