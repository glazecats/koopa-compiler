#include "ast.h"
#include "ir.h"
#include "lexer.h"
#include "parser.h"
#include "semantic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int lower_source_to_ir_text(const char *source, char **out_text) {
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;
    IrProgram ir_program;
    IrError ir_err;

    if (!source || !out_text) {
        return 0;
    }

    *out_text = NULL;
    lexer_init_tokens(&tokens);
    ast_program_init(&program);
    ir_program_init(&ir_program);

    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[ir-reg] FAIL: lexer failed for input\n");
        return 0;
    }

    if (!parser_parse_translation_unit_ast(&tokens, &program, &parse_err)) {
        fprintf(stderr,
            "[ir-reg] FAIL: parser failed at %d:%d: %s\n",
            parse_err.line,
            parse_err.column,
            parse_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (!semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
            "[ir-reg] FAIL: semantic failed at %d:%d: %s\n",
            sema_err.line,
            sema_err.column,
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (!ir_lower_program(&program, &ir_program, &ir_err)) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR lowering failed at %d:%d: %s\n",
            ir_err.line,
            ir_err.column,
            ir_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        ir_program_free(&ir_program);
        return 0;
    }

    if (!ir_dump_program(&ir_program, out_text)) {
        fprintf(stderr, "[ir-reg] FAIL: IR dump failed\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        ir_program_free(&ir_program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    ir_program_free(&ir_program);
    return 1;
}

static int expect_ir_dump(const char *case_id,
    const char *source,
    const char *expected_text) {
    char *actual_text = NULL;
    int ok;

    if (!case_id || !source || !expected_text) {
        return 0;
    }

    if (!lower_source_to_ir_text(source, &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = strcmp(actual_text, expected_text) == 0;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: %s IR mismatch\nexpected:\n%s\nactual:\n%s\n",
            case_id,
            expected_text,
            actual_text);
    }

    free(actual_text);
    return ok;
}

static int expect_ir_lower_fails(const char *case_id,
    const char *source,
    const char *expected_fragment) {
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;
    IrProgram ir_program;
    IrError ir_err;

    if (!case_id || !source || !expected_fragment) {
        return 0;
    }

    lexer_init_tokens(&tokens);
    ast_program_init(&program);
    ir_program_init(&ir_program);

    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[ir-reg] FAIL: %s lexer failed\n", case_id);
        return 0;
    }
    if (!parser_parse_translation_unit_ast(&tokens, &program, &parse_err)) {
        fprintf(stderr,
            "[ir-reg] FAIL: %s parser failed at %d:%d: %s\n",
            case_id,
            parse_err.line,
            parse_err.column,
            parse_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }
    if (!semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
            "[ir-reg] FAIL: %s semantic failed at %d:%d: %s\n",
            case_id,
            sema_err.line,
            sema_err.column,
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (ir_lower_program(&program, &ir_program, &ir_err)) {
        fprintf(stderr,
            "[ir-reg] FAIL: %s unexpectedly lowered successfully\n",
            case_id);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        ir_program_free(&ir_program);
        return 0;
    }

    if (!strstr(ir_err.message, expected_fragment)) {
        fprintf(stderr,
            "[ir-reg] FAIL: %s lowering error mismatch\nexpected fragment: %s\nactual: %s\n",
            case_id,
            expected_fragment,
            ir_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        ir_program_free(&ir_program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    ir_program_free(&ir_program);
    return 1;
}

static int expect_ir_lower_fails_without_semantic(const char *case_id,
    const char *source,
    const char *expected_fragment) {
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    IrProgram ir_program;
    IrError ir_err;

    if (!case_id || !source || !expected_fragment) {
        return 0;
    }

    lexer_init_tokens(&tokens);
    ast_program_init(&program);
    ir_program_init(&ir_program);

    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[ir-reg] FAIL: %s lexer failed\n", case_id);
        return 0;
    }
    if (!parser_parse_translation_unit_ast(&tokens, &program, &parse_err)) {
        fprintf(stderr,
            "[ir-reg] FAIL: %s parser failed at %d:%d: %s\n",
            case_id,
            parse_err.line,
            parse_err.column,
            parse_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (ir_lower_program(&program, &ir_program, &ir_err)) {
        fprintf(stderr,
            "[ir-reg] FAIL: %s unexpectedly lowered successfully (semantic skipped)\n",
            case_id);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        ir_program_free(&ir_program);
        return 0;
    }

    if (!strstr(ir_err.message, expected_fragment)) {
        fprintf(stderr,
            "[ir-reg] FAIL: %s lowering error mismatch (semantic skipped)\nexpected fragment: %s\nactual: %s\n",
            case_id,
            expected_fragment,
            ir_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        ir_program_free(&ir_program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    ir_program_free(&ir_program);
    return 1;
}

static int expect_ir_lower_succeeds(const char *case_id, const char *source) {
    char *actual_text = NULL;

    if (!case_id || !source) {
        return 0;
    }

    if (!lower_source_to_ir_text(source, &actual_text)) {
        fprintf(stderr,
            "[ir-reg] FAIL: %s unexpectedly failed to lower\n",
            case_id);
        free(actual_text);
        return 0;
    }

    free(actual_text);
    return 1;
}

static int test_ir_lowers_return_literal(void) {
    return expect_ir_dump("IR-RET-LIT",
        "int main(){return 0;}\n",
        "func main() {\n"
        "  bb.0:\n"
        "    ret 0\n"
        "}\n");
}

static int test_ir_lowers_return_parameter(void) {
    return expect_ir_dump("IR-RET-PARAM",
        "int id(int a){return a;}\n",
        "func id(a.0) {\n"
        "  bb.0:\n"
        "    ret a.0\n"
        "}\n");
}

static int test_ir_lowers_arithmetic_expression(void) {
    return expect_ir_dump("IR-ARITH",
        "int f(int a){return a+2*3;}\n",
        "func f(a.0) {\n"
        "  bb.0:\n"
        "    tmp.0 = mul 2, 3\n"
        "    tmp.1 = add a.0, tmp.0\n"
        "    ret tmp.1\n"
        "}\n");
}

static int test_ir_lowers_bitwise_expression_family(void) {
    return expect_ir_dump("IR-BITWISE-FAMILY",
        "int f(int a,int b){return ((a&b)^(a|b))+((a<<1)>>2);}\n",
        "func f(a.0, b.1) {\n"
        "  bb.0:\n"
        "    tmp.0 = and a.0, b.1\n"
        "    tmp.1 = or a.0, b.1\n"
        "    tmp.2 = xor tmp.0, tmp.1\n"
        "    tmp.3 = shl a.0, 1\n"
        "    tmp.4 = shr tmp.3, 2\n"
        "    tmp.5 = add tmp.2, tmp.4\n"
        "    ret tmp.5\n"
        "}\n");
}

static int test_ir_lowers_tilde_unary(void) {
    return expect_ir_dump("IR-TILDE",
        "int f(int a){return ~a;}\n",
        "func f(a.0) {\n"
        "  bb.0:\n"
        "    tmp.0 = xor a.0, -1\n"
        "    ret tmp.0\n"
        "}\n");
}

static int test_ir_lowers_comma_expression(void) {
    return expect_ir_dump("IR-COMMA",
        "int f(int a,int b){return (a=1, b=2, a+b);}\n",
        "func f(a.0, b.1) {\n"
        "  bb.0:\n"
        "    a.0 = mov 1\n"
        "    b.1 = mov 2\n"
        "    tmp.0 = add a.0, b.1\n"
        "    ret tmp.0\n"
        "}\n");
}

static int test_ir_lowers_global_compound_assignment(void) {
    return expect_ir_dump("IR-GLOBAL-COMPOUND-ASSIGN",
        "int g=1;\nint main(){g+=2; return g;}\n",
        "global g.0 = 1\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    tmp.0 = add g.0, 2\n"
        "    g.0 = mov tmp.0\n"
        "    ret g.0\n"
        "}\n");
}

    static int test_ir_lowers_global_shift_and_bitwise_compound_assignment(void) {
        return expect_ir_dump("IR-GLOBAL-BITWISE-SHIFT-COMPOUND",
        "int g=3;\nint main(){g<<=1; g&=6; g^=1; g|=8; return g;}\n",
        "global g.0 = 3\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    tmp.0 = shl g.0, 1\n"
        "    g.0 = mov tmp.0\n"
        "    tmp.1 = and g.0, 6\n"
        "    g.0 = mov tmp.1\n"
        "    tmp.2 = xor g.0, 1\n"
        "    g.0 = mov tmp.2\n"
        "    tmp.3 = or g.0, 8\n"
        "    g.0 = mov tmp.3\n"
        "    ret g.0\n"
        "}\n");
    }

static int test_ir_lowers_global_prefix_and_postfix_updates(void) {
    return expect_ir_dump("IR-GLOBAL-INCDEC",
        "int g=1;\nint main(){return ++g + g--;}\n",
        "global g.0 = 1\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    tmp.0 = add g.0, 1\n"
        "    g.0 = mov tmp.0\n"
        "    tmp.1 = mov g.0\n"
        "    tmp.2 = sub g.0, 1\n"
        "    g.0 = mov tmp.2\n"
        "    tmp.3 = add tmp.0, tmp.1\n"
        "    ret tmp.3\n"
        "}\n");
}

static int test_ir_lowers_global_initializer_dependency(void) {
    return expect_ir_dump("IR-GLOBAL-INIT-DEP",
        "int a=1;\nint b=a+2;\nint main(){return b;}\n",
        "global a.0 = 1\n"
        "global b.1 = 3\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    ret b.1\n"
        "}\n");
}

static int test_ir_lowers_local_shadow_over_global(void) {
    return expect_ir_dump("IR-GLOBAL-SHADOW-LOCAL",
        "int g=4;\nint main(){int g=1; g+=2; return g;}\n",
        "global g.0 = 4\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    g.0 = mov 1\n"
        "    tmp.0 = add g.0, 2\n"
        "    g.0 = mov tmp.0\n"
        "    ret g.0\n"
        "}\n");
}

    static int test_ir_lowers_runtime_global_initializer_via_internal_init_function(void) {
        return expect_ir_dump("IR-GLOBAL-RUNTIME-INIT",
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
        "    g.0 = mov tmp.0\n"
        "    ret 0\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    tmp.0 = call __global.init()\n"
        "    ret g.0\n"
        "}\n");
    }

    static int test_ir_exports_program_initializer_when_main_is_absent(void) {
        return expect_ir_dump("IR-GLOBAL-RUNTIME-INIT-NO-MAIN",
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
        "    g.0 = mov tmp.0\n"
        "    ret 0\n"
        "}\n"
        "\n"
        "func __program.init() {\n"
        "  bb.0:\n"
        "    tmp.0 = call __global.init()\n"
        "    ret 0\n"
        "}\n");
    }

static int test_ir_rejects_runtime_global_initializer_dep_on_uninitialized_global(void) {
    return expect_ir_lower_fails("IR-GLOBAL-RUNTIME-INIT-DEP-ORDER",
        "int a;\nint b;\nint a=b+1;\nint main(){return a;}\n",
        "dependency path: a -> b");
}

static int test_ir_rejects_runtime_global_initializer_indirect_dep_on_uninitialized_global(void) {
    return expect_ir_lower_fails("IR-GLOBAL-RUNTIME-INIT-INDIRECT-DEP-ORDER",
        "int b;\nint seed(){return 7;}\nint read_b(){return b;}\nint a=read_b();\nint b=seed();\nint main(){return a+b;}\n",
        "dependency path via callee body: a -> read_b() -> b");
}

static int test_ir_reports_runtime_global_initializer_dependency_cycle_chain(void) {
    return expect_ir_lower_fails("IR-GLOBAL-RUNTIME-INIT-CYCLE-CHAIN",
        "int a;\nint b;\nint a=b;\nint b=a;\nint main(){return 0;}\n",
        "dependency cycle: a -> b -> a");
}

static int test_ir_reports_runtime_global_dependency_cycle_chain_with_callee_segments(void) {
    return expect_ir_lower_fails("IR-GLOBAL-RUNTIME-INIT-CYCLE-CHAIN-CALLEE-AWARE",
        "int a;\nint b;\nint read_b(){return b;}\nint read_a(){return a;}\nint a=read_b();\nint b=read_a();\nint main(){return 0;}\n",
        "dependency cycle: a -> read_b() -> b -> read_a() -> a");
}

static int test_ir_accepts_runtime_global_initializer_when_only_unreachable_dead_read_mentions_uninitialized_global(void) {
    return expect_ir_lower_succeeds("IR-GLOBAL-RUNTIME-INIT-DEAD-READ-AFTER-RETURN",
        "int b;\nint f(){return 1; b;}\nint a=f();\nint b=7;\nint main(){return a+b;}\n");
}

static int test_ir_accepts_runtime_global_initializer_when_if_constant_true_makes_read_unreachable(void) {
    return expect_ir_lower_succeeds("IR-GLOBAL-RUNTIME-INIT-DEAD-READ-AFTER-IF-CONST",
    "int b;\nint f(){if(1) return 1; b; return 1;}\nint a=f();\nint b=7;\nint main(){return a+b;}\n");
}

static int test_ir_accepts_runtime_global_initializer_when_if_constant_true_minimal_shape_has_no_malformed_continuation_block(void) {
    return expect_ir_lower_succeeds("IR-GLOBAL-RUNTIME-INIT-DEAD-READ-AFTER-IF-CONST-MIN-TRUE",
        "int b;\nint f(){if(1) return 1; b;}\nint a=f();\nint b=7;\nint main(){return a+b;}\n");
}

static int test_ir_accepts_runtime_global_initializer_when_if_constant_false_else_return_minimal_shape_has_no_malformed_continuation_block(void) {
    return expect_ir_lower_succeeds("IR-GLOBAL-RUNTIME-INIT-DEAD-READ-AFTER-IF-CONST-MIN-FALSE-ELSE",
        "int b;\nint f(){if(0) b; else return 1; b;}\nint a=f();\nint b=7;\nint main(){return a+b;}\n");
}

static int test_ir_accepts_runtime_global_initializer_when_if_constant_true_else_dead_branch_minimal_shape_has_no_malformed_continuation_block(void) {
    return expect_ir_lower_succeeds("IR-GLOBAL-RUNTIME-INIT-DEAD-READ-AFTER-IF-CONST-MIN-TRUE-ELSE",
        "int b;\nint f(){if(1) return 1; else b; b;}\nint a=f();\nint b=7;\nint main(){return a+b;}\n");
}

static int test_ir_accepts_runtime_global_initializer_when_while_constant_false_makes_read_unreachable(void) {
    return expect_ir_lower_succeeds("IR-GLOBAL-RUNTIME-INIT-DEAD-READ-IN-WHILE-CONST-FALSE",
        "int b;\nint f(){while(0){return b;} return 1;}\nint a=f();\nint b=7;\nint main(){return a+b;}\n");
}

static int test_ir_accepts_runtime_global_initializer_when_while_constant_true_with_immediate_return_makes_following_read_unreachable(void) {
    return expect_ir_lower_succeeds("IR-GLOBAL-RUNTIME-INIT-DEAD-READ-AFTER-WHILE-CONST-TRUE-RET",
        "int b;\nint f(){while(1){return 1;} b; return 1;}\nint a=f();\nint b=7;\nint main(){return a+b;}\n");
}

static int test_ir_accepts_runtime_global_initializer_when_for_infinite_with_immediate_return_makes_following_read_unreachable(void) {
    return expect_ir_lower_succeeds("IR-GLOBAL-RUNTIME-INIT-DEAD-READ-AFTER-FOR-CONST-TRUE-RET",
        "int b;\nint f(){for(;;){return 1;} b; return 1;}\nint a=f();\nint b=7;\nint main(){return a+b;}\n");
}

static int test_ir_accepts_runtime_global_initializer_when_for_step_is_unreachable_due_to_immediate_return_in_body(void) {
    return expect_ir_lower_succeeds("IR-GLOBAL-RUNTIME-INIT-DEAD-READ-IN-FOR-STEP-UNREACHABLE",
        "int b;\nint f(){for(;1;b){return 1;} return 1;}\nint a=f();\nint b=7;\nint main(){return a+b;}\n");
}

static int test_ir_rejects_runtime_global_initializer_when_for_step_read_is_reachable_via_continue(void) {
    return expect_ir_lower_fails_without_semantic("IR-GLOBAL-RUNTIME-INIT-LIVE-READ-IN-FOR-STEP-VIA-CONTINUE",
        "int b;\nint f(){for(;1;b){continue;} return 1;}\nint a=f();\nint b=7;\nint main(){return a+b;}\n",
        "dependency path via callee body: a -> f() -> b");
}

static int test_ir_accepts_runtime_global_initializer_when_for_step_is_unreachable_due_to_unconditional_break(void) {
    return expect_ir_lower_succeeds("IR-GLOBAL-RUNTIME-INIT-DEAD-READ-IN-FOR-STEP-UNREACHABLE-BREAK",
        "int b;\nint f(){for(;1;b){break;} return 1;}\nint a=f();\nint b=7;\nint main(){return a+b;}\n");
}

static int test_ir_rejects_runtime_global_initializer_when_for_step_read_is_reachable_in_infinite_loop(void) {
    return expect_ir_lower_fails_without_semantic("IR-GLOBAL-RUNTIME-INIT-LIVE-READ-IN-FOR-STEP-INFINITE",
        "int b;\nint f(){for(;1;b){} return 1;}\nint a=f();\nint b=7;\nint main(){return a+b;}\n",
        "dependency path via callee body: a -> f() -> b");
}

static int test_ir_accepts_runtime_global_initializer_when_while_const_true_has_unreachable_break_and_dead_post_loop_read(void) {
    return expect_ir_lower_succeeds("IR-GLOBAL-RUNTIME-INIT-DEAD-READ-AFTER-WHILE-CONST-TRUE-IF0-BREAK",
        "int b;\nint f(){while(1){if(0) break; return 1;} b; return 1;}\nint a=f();\nint b=7;\nint main(){return a+b;}\n");
}

static int test_ir_accepts_runtime_global_initializer_when_while_const_true_has_unreachable_continue_and_post_loop_return_read(void) {
    return expect_ir_lower_succeeds("IR-GLOBAL-RUNTIME-INIT-DEAD-READ-AFTER-WHILE-CONST-TRUE-IF0-CONTINUE-RET",
        "int b;\nint f(){while(1){if(0) continue; return 1;} return b;}\nint a=f();\nint b=7;\nint main(){return a+b;}\n");
}

static int test_ir_rejects_runtime_global_initializer_when_while_const_true_breaks_and_post_loop_read_is_live(void) {
    return expect_ir_lower_fails("IR-GLOBAL-RUNTIME-INIT-LIVE-READ-AFTER-WHILE-CONST-TRUE-IF1-BREAK",
        "int b;\nint f(){while(1){if(1) break;} return b;}\nint a=f();\nint b=7;\nint main(){return a+b;}\n",
        "dependency path via callee body: a -> f() -> b");
}

static int test_ir_accepts_runtime_global_initializer_when_inner_loop_break_does_not_make_outer_post_loop_read_live(void) {
    return expect_ir_lower_succeeds("IR-GLOBAL-RUNTIME-INIT-DEAD-READ-AFTER-WHILE-CONST-TRUE-INNER-BREAK",
        "int b;\nint f(){while(1){while(1){break;} return 1;} b; return 1;}\nint a=f();\nint b=7;\nint main(){return a+b;}\n");
}

static int test_ir_accepts_runtime_global_initializer_when_for_step_is_unreachable_after_inner_break_then_outer_break(void) {
    return expect_ir_lower_succeeds("IR-GLOBAL-RUNTIME-INIT-DEAD-READ-IN-FOR-STEP-INNER-BREAK-OUTER-BREAK",
        "int b;\nint f(){for(;1;b){while(1){break;} break;} return 1;}\nint a=f();\nint b=7;\nint main(){return a+b;}\n");
}

static int test_ir_rejects_runtime_global_initializer_when_for_step_is_reachable_after_inner_break_then_outer_continue(void) {
    return expect_ir_lower_fails_without_semantic("IR-GLOBAL-RUNTIME-INIT-LIVE-READ-IN-FOR-STEP-INNER-BREAK-OUTER-CONTINUE",
        "int b;\nint f(){for(;1;b){while(1){break;} continue;} return 1;}\nint a=f();\nint b=7;\nint main(){return a+b;}\n",
        "dependency path via callee body: a -> f() -> b");
}

static int test_ir_accepts_runtime_global_initializer_when_ternary_constant_true_makes_else_read_unreachable(void) {
    return expect_ir_lower_succeeds("IR-GLOBAL-RUNTIME-INIT-DEAD-READ-IN-TERNARY-CONST-TRUE",
        "int b;\nint f(){return 1 ? 1 : b;}\nint a=f();\nint b=7;\nint main(){return a+b;}\n");
}

static int test_ir_accepts_runtime_global_initializer_when_ternary_constant_false_makes_then_read_unreachable(void) {
    return expect_ir_lower_succeeds("IR-GLOBAL-RUNTIME-INIT-DEAD-READ-IN-TERNARY-CONST-FALSE",
        "int b;\nint f(){return 0 ? b : 1;}\nint a=f();\nint b=7;\nint main(){return a+b;}\n");
}

static int test_ir_accepts_runtime_global_initializer_when_logical_and_constant_false_makes_rhs_read_unreachable(void) {
    return expect_ir_lower_succeeds("IR-GLOBAL-RUNTIME-INIT-DEAD-READ-IN-LOGICAL-AND-CONST-FALSE",
        "int b;\nint f(){return 0 && b;}\nint a=f();\nint b=7;\nint main(){return a+b;}\n");
}

static int test_ir_accepts_runtime_global_initializer_when_logical_or_constant_true_makes_rhs_read_unreachable(void) {
    return expect_ir_lower_succeeds("IR-GLOBAL-RUNTIME-INIT-DEAD-READ-IN-LOGICAL-OR-CONST-TRUE",
        "int b;\nint f(){return 1 || b;}\nint a=f();\nint b=7;\nint main(){return a+b;}\n");
}

static int test_ir_lowers_unary_minus_llong_min_initializer_without_host_ub(void) {
    return expect_ir_dump("IR-GLOBAL-INIT-NEG-LLONG-MIN",
        "int a=-(~9223372036854775807);\nint main(){return 0;}\n",
        "global a.0\n"
        "\n"
        "func __global.init() {\n"
        "  bb.0:\n"
        "    tmp.0 = xor 9223372036854775807, -1\n"
        "    tmp.1 = sub 0, tmp.0\n"
        "    a.0 = mov tmp.1\n"
        "    ret 0\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    tmp.0 = call __global.init()\n"
        "    ret 0\n"
        "}\n");
}

static int test_ir_lowers_add_overflow_initializer_without_host_ub(void) {
    return expect_ir_dump("IR-GLOBAL-INIT-ADD-OVERFLOW",
        "int a=9223372036854775807+1;\nint main(){return 0;}\n",
        "global a.0\n"
        "\n"
        "func __global.init() {\n"
        "  bb.0:\n"
        "    tmp.0 = add 9223372036854775807, 1\n"
        "    a.0 = mov tmp.0\n"
        "    ret 0\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    tmp.0 = call __global.init()\n"
        "    ret 0\n"
        "}\n");
}

static int test_ir_rejects_division_by_zero_in_top_level_constant_initializer(void) {
    return expect_ir_lower_fails("IR-GLOBAL-INIT-DIV-ZERO-REJECT",
        "int a=1/0;\nint main(){return 0;}\n",
        "IR-LOWER-018: top-level initializer division by zero is not allowed");
}

static int test_ir_rejects_modulo_by_zero_in_top_level_constant_initializer(void) {
    return expect_ir_lower_fails("IR-GLOBAL-INIT-MOD-ZERO-REJECT",
        "int a=1%0;\nint main(){return 0;}\n",
        "IR-LOWER-018: top-level initializer modulo by zero is not allowed");
}

static int test_ir_rejects_out_of_range_shift_count_in_top_level_constant_initializer(void) {
    return expect_ir_lower_fails("IR-GLOBAL-INIT-SHIFT-COUNT-REJECT",
        "int a=1<<-1;\nint main(){return 0;}\n",
        "IR-LOWER-019: top-level initializer shift count -1 is out of range");
}

static int test_ir_lowers_direct_call(void) {
    return expect_ir_dump("IR-CALL-DIRECT",
        "int id(int a){return a;}\nint main(){return id(1);}\n",
        "func id(a.0) {\n"
        "  bb.0:\n"
        "    ret a.0\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    tmp.0 = call id(1)\n"
        "    ret tmp.0\n"
        "}\n");
}

static int test_ir_lowers_call_with_expression_args(void) {
    return expect_ir_dump("IR-CALL-ARGS",
        "int add(int a,int b){return a+b;}\nint main(){int x=1; return add(x, x+1);}\n",
        "func add(a.0, b.1) {\n"
        "  bb.0:\n"
        "    tmp.0 = add a.0, b.1\n"
        "    ret tmp.0\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    x.0 = mov 1\n"
        "    tmp.0 = add x.0, 1\n"
        "    tmp.1 = call add(x.0, tmp.0)\n"
        "    ret tmp.1\n"
        "}\n");
}

static int test_ir_lower_rejects_unknown_call_target_without_semantic_gate(void) {
    return expect_ir_lower_fails_without_semantic("IR-CALL-UNKNOWN-CALLEE-LOWER-GATE",
        "int main(){return missing(1);}\n",
        "IR-LOWER-025");
}

static int test_ir_lower_rejects_call_arity_mismatch_without_semantic_gate(void) {
    return expect_ir_lower_fails_without_semantic("IR-CALL-ARITY-MISMATCH-LOWER-GATE",
        "int add(int a,int b);\nint main(){return add(1);}\n",
        "IR-LOWER-026");
}

static int test_ir_lower_rejects_declaration_arity_mismatch_without_semantic_gate(void) {
    return expect_ir_lower_fails_without_semantic("IR-DECL-ARITY-MISMATCH-LOWER-GATE",
        "int add(int a);\nint add(int a,int b);\nint main(){return 0;}\n",
        "IR-LOWER-020");
}

static int test_ir_lower_rejects_duplicate_definition_without_semantic_gate(void) {
    return expect_ir_lower_fails_without_semantic("IR-DUPLICATE-DEF-LOWER-GATE",
        "int f(){return 1;}\nint f(){return 2;}\nint main(){return f();}\n",
        "IR-LOWER-021");
}

static int test_ir_preserves_function_declaration_signature(void) {
    return expect_ir_dump("IR-DECL-CALL",
        "int add(int a,int b);\nint main(){return add(1,2);}\n",
        "declare add(a.0, b.1)\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    tmp.0 = call add(1, 2)\n"
        "    ret tmp.0\n"
        "}\n");
}

static int test_ir_merges_declaration_with_definition(void) {
    return expect_ir_dump("IR-DECL-MERGE",
        "int add(int a,int b);\nint add(int a,int b){return a+b;}\nint main(){return add(1,2);}\n",
        "func add(a.0, b.1) {\n"
        "  bb.0:\n"
        "    tmp.0 = add a.0, b.1\n"
        "    ret tmp.0\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    tmp.0 = call add(1, 2)\n"
        "    ret tmp.0\n"
        "}\n");
}

static int test_ir_merges_repeated_declarations_without_definition(void) {
    return expect_ir_dump("IR-DECL-REDECL",
        "int add(int x,int y);\nint add(int a,int b);\nint main(){return add(1,2);}\n",
        "declare add(a.0, b.1)\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    tmp.0 = call add(1, 2)\n"
        "    ret tmp.0\n"
        "}\n");
}

    static int test_ir_merges_unnamed_declaration_then_definition(void) {
        return expect_ir_dump("IR-DECL-UNNAMED-THEN-DEF",
        "int add(int,int);\nint add(int a,int b){return a+b;}\nint main(){return add(1,2);}\n",
        "func add(a.0, b.1) {\n"
        "  bb.0:\n"
        "    tmp.0 = add a.0, b.1\n"
        "    ret tmp.0\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    tmp.0 = call add(1, 2)\n"
        "    ret tmp.0\n"
        "}\n");
    }

    static int test_ir_merges_declaration_with_parameter_name_drift(void) {
        return expect_ir_dump("IR-DECL-MERGE-NAME-DRIFT",
        "int add(int x,int y);\nint add(int a,int b){return a+b;}\nint main(){return add(1,2);}\n",
        "func add(a.0, b.1) {\n"
        "  bb.0:\n"
        "    tmp.0 = add a.0, b.1\n"
        "    ret tmp.0\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    tmp.0 = call add(1, 2)\n"
        "    ret tmp.0\n"
        "}\n");
    }

static int test_ir_lowers_declared_call_in_if_condition(void) {
    return expect_ir_dump("IR-DECL-IF-CALL",
        "int pred(int a);\nint main(int x){if(pred(x)) return pred(1); return pred(2);}\n",
        "declare pred(a.0)\n"
        "\n"
        "func main(x.0) {\n"
        "  bb.0:\n"
        "    tmp.0 = call pred(x.0)\n"
        "    br tmp.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    tmp.1 = call pred(1)\n"
        "    ret tmp.1\n"
        "  bb.2:\n"
        "    tmp.2 = call pred(2)\n"
        "    ret tmp.2\n"
        "}\n");
}

    static int test_ir_lowers_declared_call_in_while_condition(void) {
        return expect_ir_dump("IR-DECL-WHILE-CALL",
        "int pred(int a);\nint main(int x){while(pred(x)){return pred(1);} return pred(2);}\n",
        "declare pred(a.0)\n"
        "\n"
        "func main(x.0) {\n"
        "  bb.0:\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    tmp.0 = call pred(x.0)\n"
        "    br tmp.0, bb.2, bb.3\n"
        "  bb.2:\n"
        "    tmp.1 = call pred(1)\n"
        "    ret tmp.1\n"
        "  bb.3:\n"
        "    tmp.2 = call pred(2)\n"
        "    ret tmp.2\n"
        "}\n");
    }

static int test_ir_lowers_declared_calls_in_for_condition_and_step(void) {
    return expect_ir_dump("IR-DECL-FOR-CALL-COND-STEP",
        "int pred(int a);\nint step(int a);\nint main(int x){for(;pred(x);x=step(x)){} return x;}\n",
        "declare pred(a.0)\n"
        "\n"
        "declare step(a.0)\n"
        "\n"
        "func main(x.0) {\n"
        "  bb.0:\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    tmp.0 = call pred(x.0)\n"
        "    br tmp.0, bb.2, bb.4\n"
        "  bb.2:\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    tmp.1 = call step(x.0)\n"
        "    x.0 = mov tmp.1\n"
        "    jmp bb.1\n"
        "  bb.4:\n"
        "    ret x.0\n"
        "}\n");
}

    static int test_ir_lowers_declared_calls_in_for_init_condition_and_step(void) {
        return expect_ir_dump("IR-DECL-FOR-CALL-INIT-COND-STEP",
        "int init(int a);\nint pred(int a);\nint step(int a);\nint main(int x){for(x=init(x);pred(x);x=step(x)){} return x;}\n",
        "declare init(a.0)\n"
        "\n"
        "declare pred(a.0)\n"
        "\n"
        "declare step(a.0)\n"
        "\n"
        "func main(x.0) {\n"
        "  bb.0:\n"
        "    tmp.0 = call init(x.0)\n"
        "    x.0 = mov tmp.0\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    tmp.1 = call pred(x.0)\n"
        "    br tmp.1, bb.2, bb.4\n"
        "  bb.2:\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    tmp.2 = call step(x.0)\n"
        "    x.0 = mov tmp.2\n"
        "    jmp bb.1\n"
        "  bb.4:\n"
        "    ret x.0\n"
        "}\n");
    }

    static int test_ir_lowers_declared_call_in_for_init(void) {
        return expect_ir_dump("IR-DECL-FOR-CALL-INIT",
        "int init(int a);\nint main(int x){for(x=init(x);x;x=x-1){} return x;}\n",
        "declare init(a.0)\n"
        "\n"
        "func main(x.0) {\n"
        "  bb.0:\n"
        "    tmp.0 = call init(x.0)\n"
        "    x.0 = mov tmp.0\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    br x.0, bb.2, bb.4\n"
        "  bb.2:\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    tmp.1 = sub x.0, 1\n"
        "    x.0 = mov tmp.1\n"
        "    jmp bb.1\n"
        "  bb.4:\n"
        "    ret x.0\n"
        "}\n");
    }

static int test_ir_lowers_declared_calls_in_for_logical_and_condition_and_step(void) {
    return expect_ir_dump("IR-DECL-FOR-CALL-COND-AND-STEP",
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
        "    tmp.0 = call pred(x.0)\n"
        "    br tmp.0, bb.5, bb.4\n"
        "  bb.2:\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    tmp.2 = call step(x.0)\n"
        "    x.0 = mov tmp.2\n"
        "    jmp bb.1\n"
        "  bb.4:\n"
        "    ret x.0\n"
        "  bb.5:\n"
        "    tmp.1 = call gate(x.0)\n"
        "    br tmp.1, bb.2, bb.4\n"
        "}\n");
}

static int test_ir_lowers_declared_calls_in_for_logical_or_condition_and_step(void) {
    return expect_ir_dump("IR-DECL-FOR-CALL-COND-OR-STEP",
        "int pred(int a);\nint gate(int a);\nint step(int a);\nint main(int x){for(;pred(x)||gate(x);x=step(x)){} return x;}\n",
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
        "    tmp.0 = call pred(x.0)\n"
        "    br tmp.0, bb.2, bb.5\n"
        "  bb.2:\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    tmp.2 = call step(x.0)\n"
        "    x.0 = mov tmp.2\n"
        "    jmp bb.1\n"
        "  bb.4:\n"
        "    ret x.0\n"
        "  bb.5:\n"
        "    tmp.1 = call gate(x.0)\n"
        "    br tmp.1, bb.2, bb.4\n"
        "}\n");
}

static int test_ir_lowers_declared_calls_in_for_step_comma_expression(void) {
    return expect_ir_dump("IR-DECL-FOR-CALL-STEP-COMMA",
        "int pred(int a);\nint step1(int a);\nint step2(int a);\nint main(int x){for(;pred(x);x=(step1(x),step2(x))){} return x;}\n",
        "declare pred(a.0)\n"
        "\n"
        "declare step1(a.0)\n"
        "\n"
        "declare step2(a.0)\n"
        "\n"
        "func main(x.0) {\n"
        "  bb.0:\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    tmp.0 = call pred(x.0)\n"
        "    br tmp.0, bb.2, bb.4\n"
        "  bb.2:\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    tmp.1 = call step1(x.0)\n"
        "    tmp.2 = call step2(x.0)\n"
        "    x.0 = mov tmp.2\n"
        "    jmp bb.1\n"
        "  bb.4:\n"
        "    ret x.0\n"
        "}\n");
}

static int test_ir_lowers_declared_calls_in_for_init_comma_expression(void) {
    return expect_ir_dump("IR-DECL-FOR-CALL-INIT-COMMA",
        "int init1(int a);\nint init2(int a);\nint main(int x){for(x=(init1(x),init2(x));x;x=x-1){} return x;}\n",
        "declare init1(a.0)\n"
        "\n"
        "declare init2(a.0)\n"
        "\n"
        "func main(x.0) {\n"
        "  bb.0:\n"
        "    tmp.0 = call init1(x.0)\n"
        "    tmp.1 = call init2(x.0)\n"
        "    x.0 = mov tmp.1\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    br x.0, bb.2, bb.4\n"
        "  bb.2:\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    tmp.2 = sub x.0, 1\n"
        "    x.0 = mov tmp.2\n"
        "    jmp bb.1\n"
        "  bb.4:\n"
        "    ret x.0\n"
        "}\n");
}

    static int test_ir_lowers_declared_calls_in_for_nested_short_circuit_condition(void) {
        return expect_ir_dump("IR-DECL-FOR-CALL-COND-NESTED-MIX-STEP",
        "int pred(int a);\nint gate(int a);\nint tail(int a);\nint step(int a);\nint main(int x){for(;(pred(x)&&gate(x))||tail(x);x=step(x)){} return x;}\n",
        "declare pred(a.0)\n"
        "\n"
        "declare gate(a.0)\n"
        "\n"
        "declare tail(a.0)\n"
        "\n"
        "declare step(a.0)\n"
        "\n"
        "func main(x.0) {\n"
        "  bb.0:\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    tmp.1 = call pred(x.0)\n"
        "    br tmp.1, bb.8, bb.7\n"
        "  bb.2:\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    tmp.4 = call step(x.0)\n"
        "    x.0 = mov tmp.4\n"
        "    jmp bb.1\n"
        "  bb.4:\n"
        "    ret x.0\n"
        "  bb.5:\n"
        "    tmp.3 = call tail(x.0)\n"
        "    br tmp.3, bb.2, bb.4\n"
        "  bb.6:\n"
        "    tmp.0 = mov 1\n"
        "    jmp bb.9\n"
        "  bb.7:\n"
        "    tmp.0 = mov 0\n"
        "    jmp bb.9\n"
        "  bb.8:\n"
        "    tmp.2 = call gate(x.0)\n"
        "    br tmp.2, bb.6, bb.7\n"
        "  bb.9:\n"
        "    br tmp.0, bb.2, bb.5\n"
        "}\n");
    }

static int test_ir_lowers_compound_assignment(void) {
    return expect_ir_dump("IR-COMPOUND-ASSIGN",
        "int f(){int x=1; return (x+=2) + (x<<=1);}\n",
        "func f() {\n"
        "  bb.0:\n"
        "    x.0 = mov 1\n"
        "    tmp.0 = add x.0, 2\n"
        "    x.0 = mov tmp.0\n"
        "    tmp.1 = shl x.0, 1\n"
        "    x.0 = mov tmp.1\n"
        "    tmp.2 = add tmp.0, tmp.1\n"
        "    ret tmp.2\n"
        "}\n");
}

    static int test_ir_lowers_declared_call_in_comma_expression(void) {
        return expect_ir_dump("IR-DECL-COMMA-CALL",
        "int pred(int a);\nint main(int x){return (pred(x), pred(1));}\n",
        "declare pred(a.0)\n"
        "\n"
        "func main(x.0) {\n"
        "  bb.0:\n"
        "    tmp.0 = call pred(x.0)\n"
        "    tmp.1 = call pred(1)\n"
        "    ret tmp.1\n"
        "}\n");
    }

static int test_ir_lowers_declared_call_in_if_logical_condition(void) {
    return expect_ir_dump("IR-DECL-IF-LOGICAL-CALL-COND",
        "int pred(int a);\nint main(int x){if(pred(x)&&pred(0)) return pred(1); return pred(2);}\n",
        "declare pred(a.0)\n"
        "\n"
        "func main(x.0) {\n"
        "  bb.0:\n"
        "    tmp.0 = call pred(x.0)\n"
        "    br tmp.0, bb.3, bb.2\n"
        "  bb.1:\n"
        "    tmp.2 = call pred(1)\n"
        "    ret tmp.2\n"
        "  bb.2:\n"
        "    tmp.3 = call pred(2)\n"
        "    ret tmp.3\n"
        "  bb.3:\n"
        "    tmp.1 = call pred(0)\n"
        "    br tmp.1, bb.1, bb.2\n"
        "}\n");
}

    static int test_ir_lowers_declared_call_in_if_logical_or_condition(void) {
        return expect_ir_dump("IR-DECL-IF-LOGICAL-OR-CALL-COND",
        "int pred(int a);\nint main(int x){if(pred(x)||pred(0)) return pred(1); return pred(2);}\n",
        "declare pred(a.0)\n"
        "\n"
        "func main(x.0) {\n"
        "  bb.0:\n"
        "    tmp.0 = call pred(x.0)\n"
        "    br tmp.0, bb.1, bb.3\n"
        "  bb.1:\n"
        "    tmp.2 = call pred(1)\n"
        "    ret tmp.2\n"
        "  bb.2:\n"
        "    tmp.3 = call pred(2)\n"
        "    ret tmp.3\n"
        "  bb.3:\n"
        "    tmp.1 = call pred(0)\n"
        "    br tmp.1, bb.1, bb.2\n"
        "}\n");
    }

static int test_ir_lowers_declared_call_in_logical_value_expression(void) {
    return expect_ir_dump("IR-DECL-LOGICAL-VALUE",
        "int pred(int a);\nint main(int x){return pred(x)&&pred(1);}\n",
        "declare pred(a.0)\n"
        "\n"
        "func main(x.0) {\n"
        "  bb.0:\n"
        "    tmp.1 = call pred(x.0)\n"
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

    static int test_ir_lowers_declared_call_in_logical_or_value_expression(void) {
        return expect_ir_dump("IR-DECL-LOGICAL-OR-VALUE",
        "int pred(int a);\nint main(int x){return pred(x)||pred(1);}\n",
        "declare pred(a.0)\n"
        "\n"
        "func main(x.0) {\n"
        "  bb.0:\n"
        "    tmp.1 = call pred(x.0)\n"
        "    br tmp.1, bb.1, bb.3\n"
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

static int test_ir_lowers_declared_call_in_ternary_value_expression(void) {
    return expect_ir_dump("IR-DECL-TERNARY-VALUE",
        "int pred(int a);\nint main(int x){return pred(x) ? pred(1) : pred(2);}\n",
        "declare pred(a.0)\n"
        "\n"
        "func main(x.0) {\n"
        "  bb.0:\n"
        "    tmp.1 = call pred(x.0)\n"
        "    br tmp.1, bb.1, bb.2\n"
        "  bb.1:\n"
        "    tmp.2 = call pred(1)\n"
        "    tmp.0 = mov tmp.2\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    tmp.3 = call pred(2)\n"
        "    tmp.0 = mov tmp.3\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ret tmp.0\n"
        "}\n");
}

    static int test_ir_lowers_declared_call_in_ternary_condition_expression(void) {
        return expect_ir_dump("IR-DECL-TERNARY-COND",
        "int pred(int a);\nint main(int x){if(pred(x)?pred(0):pred(1)) return pred(2); return pred(3);}\n",
        "declare pred(a.0)\n"
        "\n"
        "func main(x.0) {\n"
        "  bb.0:\n"
        "    tmp.0 = call pred(x.0)\n"
        "    br tmp.0, bb.3, bb.4\n"
        "  bb.1:\n"
        "    tmp.3 = call pred(2)\n"
        "    ret tmp.3\n"
        "  bb.2:\n"
        "    tmp.4 = call pred(3)\n"
        "    ret tmp.4\n"
        "  bb.3:\n"
        "    tmp.1 = call pred(0)\n"
        "    br tmp.1, bb.1, bb.2\n"
        "  bb.4:\n"
        "    tmp.2 = call pred(1)\n"
        "    br tmp.2, bb.1, bb.2\n"
        "}\n");
    }

static int test_ir_lowers_prefix_increment(void) {
    return expect_ir_dump("IR-PREFIX-INC",
        "int f(){int x=1; return ++x;}\n",
        "func f() {\n"
        "  bb.0:\n"
        "    x.0 = mov 1\n"
        "    tmp.0 = add x.0, 1\n"
        "    x.0 = mov tmp.0\n"
        "    ret tmp.0\n"
        "}\n");
}

static int test_ir_lowers_postfix_increment(void) {
    return expect_ir_dump("IR-POSTFIX-INC",
        "int f(){int x=1; return x++;}\n",
        "func f() {\n"
        "  bb.0:\n"
        "    x.0 = mov 1\n"
        "    tmp.0 = mov x.0\n"
        "    tmp.1 = add x.0, 1\n"
        "    x.0 = mov tmp.1\n"
        "    ret tmp.0\n"
        "}\n");
}

static int test_ir_lowers_prefix_decrement(void) {
    return expect_ir_dump("IR-PREFIX-DEC",
        "int f(){int x=1; return --x;}\n",
        "func f() {\n"
        "  bb.0:\n"
        "    x.0 = mov 1\n"
        "    tmp.0 = sub x.0, 1\n"
        "    x.0 = mov tmp.0\n"
        "    ret tmp.0\n"
        "}\n");
}

static int test_ir_lowers_postfix_decrement(void) {
    return expect_ir_dump("IR-POSTFIX-DEC",
        "int f(){int x=1; return x--;}\n",
        "func f() {\n"
        "  bb.0:\n"
        "    x.0 = mov 1\n"
        "    tmp.0 = mov x.0\n"
        "    tmp.1 = sub x.0, 1\n"
        "    x.0 = mov tmp.1\n"
        "    ret tmp.0\n"
        "}\n");
}

static int test_ir_lowers_ternary_value_expression(void) {
    return expect_ir_dump("IR-TERNARY-VALUE",
        "int f(int a,int b,int c){return a ? b+1 : c+2;}\n",
        "func f(a.0, b.1, c.2) {\n"
        "  bb.0:\n"
        "    br a.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    tmp.1 = add b.1, 1\n"
        "    tmp.0 = mov tmp.1\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    tmp.2 = add c.2, 2\n"
        "    tmp.0 = mov tmp.2\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ret tmp.0\n"
        "}\n");
}

static int test_ir_lowers_ternary_condition_expression(void) {
    return expect_ir_dump("IR-TERNARY-COND",
        "int f(int a,int b,int c){if(a ? b : c) return 1; return 0;}\n",
        "func f(a.0, b.1, c.2) {\n"
        "  bb.0:\n"
        "    br a.0, bb.3, bb.4\n"
        "  bb.1:\n"
        "    ret 1\n"
        "  bb.2:\n"
        "    ret 0\n"
        "  bb.3:\n"
        "    br b.1, bb.1, bb.2\n"
        "  bb.4:\n"
        "    br c.2, bb.1, bb.2\n"
        "}\n");
}

static int test_ir_lowers_logical_not_value(void) {
    return expect_ir_dump("IR-LOGICAL-NOT",
        "int f(int a){return !a;}\n",
        "func f(a.0) {\n"
        "  bb.0:\n"
        "    br a.0, bb.2, bb.1\n"
        "  bb.1:\n"
        "    tmp.0 = mov 1\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    tmp.0 = mov 0\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ret tmp.0\n"
        "}\n");
}

    static int test_ir_lowers_comparison_expression_family(void) {
        return expect_ir_dump("IR-CMP-FAMILY",
        "int f(int a,int b){return (a==b)+(a!=b)+(a<b)+(a<=b)+(a>b)+(a>=b);}\n",
        "func f(a.0, b.1) {\n"
        "  bb.0:\n"
        "    tmp.0 = eq a.0, b.1\n"
        "    tmp.1 = ne a.0, b.1\n"
        "    tmp.2 = add tmp.0, tmp.1\n"
        "    tmp.3 = lt a.0, b.1\n"
        "    tmp.4 = add tmp.2, tmp.3\n"
        "    tmp.5 = le a.0, b.1\n"
        "    tmp.6 = add tmp.4, tmp.5\n"
        "    tmp.7 = gt a.0, b.1\n"
        "    tmp.8 = add tmp.6, tmp.7\n"
        "    tmp.9 = ge a.0, b.1\n"
        "    tmp.10 = add tmp.8, tmp.9\n"
        "    ret tmp.10\n"
        "}\n");
    }

static int test_ir_lowers_logical_and_value(void) {
    return expect_ir_dump("IR-LOGICAL-AND",
        "int f(int a,int b){return a&&b;}\n",
        "func f(a.0, b.1) {\n"
        "  bb.0:\n"
        "    br a.0, bb.3, bb.2\n"
        "  bb.1:\n"
        "    tmp.0 = mov 1\n"
        "    jmp bb.4\n"
        "  bb.2:\n"
        "    tmp.0 = mov 0\n"
        "    jmp bb.4\n"
        "  bb.3:\n"
        "    br b.1, bb.1, bb.2\n"
        "  bb.4:\n"
        "    ret tmp.0\n"
        "}\n");
}

static int test_ir_lowers_declared_call_in_nested_logical_mix_value_expression(void) {
    return expect_ir_dump("IR-DECL-LOGICAL-NESTED-MIX-VALUE",
        "int pred(int a);\nint gate(int a);\nint tail(int a);\nint main(int x){return (pred(x)&&gate(x))||tail(x);}\n",
        "declare pred(a.0)\n"
        "\n"
        "declare gate(a.0)\n"
        "\n"
        "declare tail(a.0)\n"
        "\n"
        "func main(x.0) {\n"
        "  bb.0:\n"
        "    tmp.2 = call pred(x.0)\n"
        "    br tmp.2, bb.6, bb.5\n"
        "  bb.1:\n"
        "    tmp.0 = mov 1\n"
        "    jmp bb.8\n"
        "  bb.2:\n"
        "    tmp.0 = mov 0\n"
        "    jmp bb.8\n"
        "  bb.3:\n"
        "    tmp.4 = call tail(x.0)\n"
        "    br tmp.4, bb.1, bb.2\n"
        "  bb.4:\n"
        "    tmp.1 = mov 1\n"
        "    jmp bb.7\n"
        "  bb.5:\n"
        "    tmp.1 = mov 0\n"
        "    jmp bb.7\n"
        "  bb.6:\n"
        "    tmp.3 = call gate(x.0)\n"
        "    br tmp.3, bb.4, bb.5\n"
        "  bb.7:\n"
        "    br tmp.1, bb.1, bb.3\n"
        "  bb.8:\n"
        "    ret tmp.0\n"
        "}\n");
}

static int test_ir_lowers_declared_call_in_nested_logical_mix_condition(void) {
    return expect_ir_dump("IR-DECL-IF-LOGICAL-NESTED-MIX-COND",
        "int pred(int a);\nint gate(int a);\nint tail(int a);\nint main(int x){if((pred(x)||gate(x))&&tail(x)) return pred(1); return pred(2);}\n",
        "declare pred(a.0)\n"
        "\n"
        "declare gate(a.0)\n"
        "\n"
        "declare tail(a.0)\n"
        "\n"
        "func main(x.0) {\n"
        "  bb.0:\n"
        "    tmp.1 = call pred(x.0)\n"
        "    br tmp.1, bb.4, bb.6\n"
        "  bb.1:\n"
        "    tmp.4 = call pred(1)\n"
        "    ret tmp.4\n"
        "  bb.2:\n"
        "    tmp.5 = call pred(2)\n"
        "    ret tmp.5\n"
        "  bb.3:\n"
        "    tmp.3 = call tail(x.0)\n"
        "    br tmp.3, bb.1, bb.2\n"
        "  bb.4:\n"
        "    tmp.0 = mov 1\n"
        "    jmp bb.7\n"
        "  bb.5:\n"
        "    tmp.0 = mov 0\n"
        "    jmp bb.7\n"
        "  bb.6:\n"
        "    tmp.2 = call gate(x.0)\n"
        "    br tmp.2, bb.4, bb.5\n"
        "  bb.7:\n"
        "    br tmp.0, bb.3, bb.2\n"
        "}\n");
}

static int test_ir_lowers_logical_or_value(void) {
    return expect_ir_dump("IR-LOGICAL-OR",
        "int f(int a,int b){return a||b;}\n",
        "func f(a.0, b.1) {\n"
        "  bb.0:\n"
        "    br a.0, bb.1, bb.3\n"
        "  bb.1:\n"
        "    tmp.0 = mov 1\n"
        "    jmp bb.4\n"
        "  bb.2:\n"
        "    tmp.0 = mov 0\n"
        "    jmp bb.4\n"
        "  bb.3:\n"
        "    br b.1, bb.1, bb.2\n"
        "  bb.4:\n"
        "    ret tmp.0\n"
        "}\n");
}

static int test_ir_lowers_shadowed_local_identity(void) {
    return expect_ir_dump("IR-SHADOW-LOCAL",
        "int f(int a){{int a=1; return a;}}\n",
        "func f(a.0) {\n"
        "  bb.0:\n"
        "    a.1 = mov 1\n"
        "    ret a.1\n"
        "}\n");
}

static int test_ir_lowers_if_without_else_join(void) {
    return expect_ir_dump("IR-IF-JOIN",
        "int f(int a){int x=0; if(a) x=1; return x;}\n",
        "func f(a.0) {\n"
        "  bb.0:\n"
        "    x.1 = mov 0\n"
        "    br a.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    x.1 = mov 1\n"
        "    jmp bb.2\n"
        "  bb.2:\n"
        "    ret x.1\n"
        "}\n");
}

static int test_ir_lowers_if_else_join(void) {
    return expect_ir_dump("IR-IF-ELSE-JOIN",
        "int f(int a){int x=0; if(a) x=1; else x=2; return x;}\n",
        "func f(a.0) {\n"
        "  bb.0:\n"
        "    x.1 = mov 0\n"
        "    br a.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    x.1 = mov 1\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    x.1 = mov 2\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ret x.1\n"
        "}\n");
}

static int test_ir_lowers_if_else_return_arms(void) {
    return expect_ir_dump("IR-IF-ELSE-RET",
        "int f(int a){if(a) return 1; else return 2;}\n",
        "func f(a.0) {\n"
        "  bb.0:\n"
        "    br a.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    ret 1\n"
        "  bb.2:\n"
        "    ret 2\n"
        "}\n");
}

    static int test_ir_lowers_if_comparison_condition(void) {
        return expect_ir_dump("IR-IF-CMP",
        "int f(int a){if(a<3) return 1; return 0;}\n",
        "func f(a.0) {\n"
        "  bb.0:\n"
        "    tmp.0 = lt a.0, 3\n"
        "    br tmp.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    ret 1\n"
        "  bb.2:\n"
        "    ret 0\n"
        "}\n");
    }

static int test_ir_lowers_if_logical_short_circuit_condition(void) {
    return expect_ir_dump("IR-IF-LOGICAL-COND",
        "int f(int a,int b){if(a&&b) return 1; return 0;}\n",
        "func f(a.0, b.1) {\n"
        "  bb.0:\n"
        "    br a.0, bb.3, bb.2\n"
        "  bb.1:\n"
        "    ret 1\n"
        "  bb.2:\n"
        "    ret 0\n"
        "  bb.3:\n"
        "    br b.1, bb.1, bb.2\n"
        "}\n");
}

static int test_ir_lowers_while_backedge(void) {
    return expect_ir_dump("IR-WHILE-BACKEDGE",
        "int f(int a){while(a){a=a-1;} return a;}\n",
        "func f(a.0) {\n"
        "  bb.0:\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    br a.0, bb.2, bb.3\n"
        "  bb.2:\n"
        "    tmp.0 = sub a.0, 1\n"
        "    a.0 = mov tmp.0\n"
        "    jmp bb.1\n"
        "  bb.3:\n"
        "    ret a.0\n"
        "}\n");
}

static int test_ir_lowers_constant_true_while_return_without_malformed_exit_block(void) {
    return expect_ir_lower_succeeds("IR-WHILE-CONST-TRUE-RET-NO-MALFORMED-EXIT",
        "int f(){while(1){return 1;}}\n");
}

static int test_ir_lowers_infinite_for_return_without_malformed_exit_block(void) {
    return expect_ir_lower_succeeds("IR-FOR-INFINITE-RET-NO-MALFORMED-EXIT",
        "int f(){for(;;){return 1;}}\n");
}

static int test_ir_lowers_infinite_for_with_step_return_without_malformed_exit_or_step_block(void) {
    return expect_ir_lower_succeeds("IR-FOR-INFINITE-STEP-RET-NO-MALFORMED-BLOCK",
        "int f(){int i=0; for(;;i=i+1){return 1;}}\n");
}

static int test_ir_lowers_while_break_exit(void) {
    return expect_ir_dump("IR-WHILE-BREAK",
        "int f(int a){while(a){if(a<3) break; a=a-1;} return a;}\n",
        "func f(a.0) {\n"
        "  bb.0:\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    br a.0, bb.2, bb.3\n"
        "  bb.2:\n"
        "    tmp.0 = lt a.0, 3\n"
        "    br tmp.0, bb.4, bb.5\n"
        "  bb.3:\n"
        "    ret a.0\n"
        "  bb.4:\n"
        "    jmp bb.3\n"
        "  bb.5:\n"
        "    tmp.1 = sub a.0, 1\n"
        "    a.0 = mov tmp.1\n"
        "    jmp bb.1\n"
        "}\n");
}

static int test_ir_lowers_for_init_step_cfg(void) {
    return expect_ir_dump("IR-FOR-INIT-STEP",
        "int f(){for(int a=3;a;a=a-1){} return 0;}\n",
        "func f() {\n"
        "  bb.0:\n"
        "    a.0 = mov 3\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    br a.0, bb.2, bb.4\n"
        "  bb.2:\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    tmp.0 = sub a.0, 1\n"
        "    a.0 = mov tmp.0\n"
        "    jmp bb.1\n"
        "  bb.4:\n"
        "    ret 0\n"
        "}\n");
}

    static int test_ir_lowers_nested_loop_break_continue(void) {
        return expect_ir_dump("IR-NESTED-LOOP-CONTROL",
        "int f(int a){for(;a;a=a-1){while(a){break;} continue;} return a;}\n",
        "func f(a.0) {\n"
        "  bb.0:\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    br a.0, bb.2, bb.4\n"
        "  bb.2:\n"
        "    jmp bb.5\n"
        "  bb.3:\n"
        "    tmp.0 = sub a.0, 1\n"
        "    a.0 = mov tmp.0\n"
        "    jmp bb.1\n"
        "  bb.4:\n"
        "    ret a.0\n"
        "  bb.5:\n"
        "    br a.0, bb.6, bb.7\n"
        "  bb.6:\n"
        "    jmp bb.7\n"
        "  bb.7:\n"
        "    jmp bb.3\n"
        "}\n");
    }

int main(void) {
    int ok = 1;

    ok &= test_ir_lowers_return_literal();
    ok &= test_ir_reports_runtime_global_dependency_cycle_chain_with_callee_segments();
    ok &= test_ir_lowers_return_parameter();
    ok &= test_ir_lowers_arithmetic_expression();
    ok &= test_ir_lowers_bitwise_expression_family();
    ok &= test_ir_lowers_tilde_unary();
    ok &= test_ir_lowers_comma_expression();
    ok &= test_ir_lowers_global_compound_assignment();
    ok &= test_ir_lowers_global_shift_and_bitwise_compound_assignment();
    ok &= test_ir_lowers_global_prefix_and_postfix_updates();
    ok &= test_ir_lowers_global_initializer_dependency();
    ok &= test_ir_lowers_local_shadow_over_global();
    ok &= test_ir_lowers_runtime_global_initializer_via_internal_init_function();
    ok &= test_ir_exports_program_initializer_when_main_is_absent();
    ok &= test_ir_rejects_runtime_global_initializer_dep_on_uninitialized_global();
    ok &= test_ir_rejects_runtime_global_initializer_indirect_dep_on_uninitialized_global();
    ok &= test_ir_reports_runtime_global_initializer_dependency_cycle_chain();
    ok &= test_ir_accepts_runtime_global_initializer_when_only_unreachable_dead_read_mentions_uninitialized_global();
    ok &= test_ir_accepts_runtime_global_initializer_when_if_constant_true_makes_read_unreachable();
    ok &= test_ir_accepts_runtime_global_initializer_when_if_constant_true_minimal_shape_has_no_malformed_continuation_block();
    ok &= test_ir_accepts_runtime_global_initializer_when_if_constant_false_else_return_minimal_shape_has_no_malformed_continuation_block();
    ok &= test_ir_accepts_runtime_global_initializer_when_if_constant_true_else_dead_branch_minimal_shape_has_no_malformed_continuation_block();
    ok &= test_ir_accepts_runtime_global_initializer_when_while_constant_false_makes_read_unreachable();
    ok &= test_ir_accepts_runtime_global_initializer_when_while_constant_true_with_immediate_return_makes_following_read_unreachable();
    ok &= test_ir_accepts_runtime_global_initializer_when_for_infinite_with_immediate_return_makes_following_read_unreachable();
    ok &= test_ir_accepts_runtime_global_initializer_when_for_step_is_unreachable_due_to_immediate_return_in_body();
    ok &= test_ir_rejects_runtime_global_initializer_when_for_step_read_is_reachable_via_continue();
    ok &= test_ir_accepts_runtime_global_initializer_when_for_step_is_unreachable_due_to_unconditional_break();
    ok &= test_ir_rejects_runtime_global_initializer_when_for_step_read_is_reachable_in_infinite_loop();
    ok &= test_ir_accepts_runtime_global_initializer_when_while_const_true_has_unreachable_break_and_dead_post_loop_read();
    ok &= test_ir_accepts_runtime_global_initializer_when_while_const_true_has_unreachable_continue_and_post_loop_return_read();
    ok &= test_ir_rejects_runtime_global_initializer_when_while_const_true_breaks_and_post_loop_read_is_live();
    ok &= test_ir_accepts_runtime_global_initializer_when_inner_loop_break_does_not_make_outer_post_loop_read_live();
    ok &= test_ir_accepts_runtime_global_initializer_when_for_step_is_unreachable_after_inner_break_then_outer_break();
    ok &= test_ir_rejects_runtime_global_initializer_when_for_step_is_reachable_after_inner_break_then_outer_continue();
    ok &= test_ir_accepts_runtime_global_initializer_when_ternary_constant_true_makes_else_read_unreachable();
    ok &= test_ir_accepts_runtime_global_initializer_when_ternary_constant_false_makes_then_read_unreachable();
    ok &= test_ir_accepts_runtime_global_initializer_when_logical_and_constant_false_makes_rhs_read_unreachable();
    ok &= test_ir_accepts_runtime_global_initializer_when_logical_or_constant_true_makes_rhs_read_unreachable();
    ok &= test_ir_lowers_unary_minus_llong_min_initializer_without_host_ub();
    ok &= test_ir_lowers_add_overflow_initializer_without_host_ub();
    ok &= test_ir_rejects_division_by_zero_in_top_level_constant_initializer();
    ok &= test_ir_rejects_modulo_by_zero_in_top_level_constant_initializer();
    ok &= test_ir_rejects_out_of_range_shift_count_in_top_level_constant_initializer();
    ok &= test_ir_lowers_direct_call();
    ok &= test_ir_lowers_call_with_expression_args();
    ok &= test_ir_lower_rejects_unknown_call_target_without_semantic_gate();
    ok &= test_ir_lower_rejects_call_arity_mismatch_without_semantic_gate();
    ok &= test_ir_lower_rejects_declaration_arity_mismatch_without_semantic_gate();
    ok &= test_ir_lower_rejects_duplicate_definition_without_semantic_gate();
    ok &= test_ir_preserves_function_declaration_signature();
    ok &= test_ir_merges_declaration_with_definition();
    ok &= test_ir_merges_repeated_declarations_without_definition();
    ok &= test_ir_merges_unnamed_declaration_then_definition();
    ok &= test_ir_merges_declaration_with_parameter_name_drift();
    ok &= test_ir_lowers_declared_call_in_if_condition();
    ok &= test_ir_lowers_declared_call_in_while_condition();
    ok &= test_ir_lowers_declared_calls_in_for_condition_and_step();
    ok &= test_ir_lowers_declared_calls_in_for_init_condition_and_step();
    ok &= test_ir_lowers_declared_call_in_for_init();
    ok &= test_ir_lowers_declared_calls_in_for_logical_and_condition_and_step();
    ok &= test_ir_lowers_declared_calls_in_for_logical_or_condition_and_step();
    ok &= test_ir_lowers_declared_calls_in_for_step_comma_expression();
    ok &= test_ir_lowers_declared_calls_in_for_init_comma_expression();
    ok &= test_ir_lowers_declared_calls_in_for_nested_short_circuit_condition();
    ok &= test_ir_lowers_declared_call_in_comma_expression();
    ok &= test_ir_lowers_declared_call_in_if_logical_condition();
    ok &= test_ir_lowers_declared_call_in_if_logical_or_condition();
    ok &= test_ir_lowers_declared_call_in_logical_value_expression();
    ok &= test_ir_lowers_declared_call_in_logical_or_value_expression();
    ok &= test_ir_lowers_declared_call_in_nested_logical_mix_value_expression();
    ok &= test_ir_lowers_declared_call_in_nested_logical_mix_condition();
    ok &= test_ir_lowers_declared_call_in_ternary_value_expression();
    ok &= test_ir_lowers_declared_call_in_ternary_condition_expression();
    ok &= test_ir_lowers_compound_assignment();
    ok &= test_ir_lowers_prefix_increment();
    ok &= test_ir_lowers_postfix_increment();
    ok &= test_ir_lowers_prefix_decrement();
    ok &= test_ir_lowers_postfix_decrement();
    ok &= test_ir_lowers_ternary_value_expression();
    ok &= test_ir_lowers_ternary_condition_expression();
    ok &= test_ir_lowers_logical_not_value();
    ok &= test_ir_lowers_comparison_expression_family();
    ok &= test_ir_lowers_logical_and_value();
    ok &= test_ir_lowers_logical_or_value();
    ok &= test_ir_lowers_shadowed_local_identity();
    ok &= test_ir_lowers_if_without_else_join();
    ok &= test_ir_lowers_if_else_join();
    ok &= test_ir_lowers_if_else_return_arms();
    ok &= test_ir_lowers_if_comparison_condition();
    ok &= test_ir_lowers_if_logical_short_circuit_condition();
    ok &= test_ir_lowers_while_backedge();
    ok &= test_ir_lowers_constant_true_while_return_without_malformed_exit_block();
    ok &= test_ir_lowers_infinite_for_return_without_malformed_exit_block();
    ok &= test_ir_lowers_infinite_for_with_step_return_without_malformed_exit_or_step_block();
    ok &= test_ir_lowers_while_break_exit();
    ok &= test_ir_lowers_for_init_step_cfg();
    ok &= test_ir_lowers_nested_loop_break_continue();

    if (!ok) {
        return 1;
    }

    printf("[ir-reg] All IR regressions passed.\n");
    return 0;
}
