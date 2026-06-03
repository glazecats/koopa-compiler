#include "ast.h"
#include "compiler.h"
#include "ir.h"
#include "lexer.h"
#include "parser.h"
#include "semantic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t ir_test_count_fragment_occurrences(const char *text, const char *fragment) {
    const char *cursor;
    size_t count = 0u;

    if (!text || !fragment || fragment[0] == '\0') {
        return 0u;
    }

    cursor = text;
    while ((cursor = strstr(cursor, fragment)) != NULL) {
        ++count;
        cursor += strlen(fragment);
    }

    return count;
}

static int lower_source_to_ir_text_with_options(const char *source,
    int skip_all_paths_return_check,
    int allow_extension_features,
    char **out_text) {
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;
    SemanticOptions sema_options;
    IrProgram ir_program;
    IrError ir_err;

    if (!source || !out_text) {
        return 0;
    }

    *out_text = NULL;
    lexer_init_tokens(&tokens);
    ast_program_init(&program);
    ir_program_init(&ir_program);
    memset(&sema_options, 0, sizeof(sema_options));
    sema_options.skip_all_paths_return_check = skip_all_paths_return_check ? 1 : 0;
    sema_options.allow_extension_features = allow_extension_features ? 1 : 0;

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

    if (!semantic_analyze_program_with_options(&program, &sema_options, &sema_err)) {
        fprintf(stderr,
            "[ir-reg] FAIL: semantic failed at %d:%d: %s\n",
            sema_err.line,
            sema_err.column,
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (!ir_lower_program(&program, NULL, &ir_program, &ir_err)) {
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

static int lower_source_to_ir_text(const char *source, char **out_text) {
    return lower_source_to_ir_text_with_options(source, 0, 0, out_text);
}

static int lower_extension_source_to_ir_text(const char *source, char **out_text) {
    TokenArray tokens;
    AstProgram program;
    IrProgram ir_program;
    CompilerError error;
    CompilerOptions options;

    if (!source || !out_text) {
        return 0;
    }

    *out_text = NULL;
    memset(&error, 0, sizeof(error));
    memset(&options, 0, sizeof(options));
    lexer_init_tokens(&tokens);
    ast_program_init(&program);
    ir_program_init(&ir_program);
    options.skip_all_paths_return_check = 1;

    if (!compiler_frontend_lower_to_ir_for_testing(
            source,
            COMPILER_MODE_EXTENSION,
            &options,
            &tokens,
            &program,
            &ir_program,
            &error)) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR lowering failed at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
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

static int lower_extension_source_to_ir_text_without_semantic(const char *source, char **out_text) {
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
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
        fprintf(stderr, "[ir-reg] FAIL: lexer failed for extension input without semantic\n");
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

    if (!ir_lower_program(&program, NULL, &ir_program, &ir_err)) {
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

static int expect_extension_ir_dump(const char *case_id,
    const char *source,
    const char *expected_text) {
    char *actual_text = NULL;
    int ok;

    if (!case_id || !source || !expected_text) {
        return 0;
    }

    if (!lower_extension_source_to_ir_text(source, &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = strcmp(actual_text, expected_text) == 0;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: %s IR mismatch\nexpected:\n%s\nactual:\n%s\n",
            case_id,
            expected_text,
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int expect_extension_ir_dump_without_semantic(const char *case_id,
    const char *source,
    const char *expected_text) {
    char *actual_text = NULL;
    int ok;

    if (!case_id || !source || !expected_text) {
        return 0;
    }

    if (!lower_extension_source_to_ir_text_without_semantic(source, &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = strcmp(actual_text, expected_text) == 0;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: %s IR mismatch\nexpected:\n%s\nactual:\n%s\n",
            case_id,
            expected_text,
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int expect_extension_semantic_reject(const char *case_id,
    const char *source,
    const char *expected_fragment) {
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;
    SemanticOptions sema_options;

    if (!case_id || !source || !expected_fragment) {
        return 0;
    }

    lexer_init_tokens(&tokens);
    ast_program_init(&program);
    memset(&sema_options, 0, sizeof(sema_options));
    sema_options.allow_extension_features = 1;

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
    if (semantic_analyze_program_with_options(&program, &sema_options, &sema_err)) {
        fprintf(stderr,
            "[ir-reg] FAIL: %s should fail semantic analysis under -extension\n",
            case_id);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }
    if (strstr(sema_err.message, expected_fragment) == NULL) {
        fprintf(stderr,
            "[ir-reg] FAIL: %s expected semantic fragment '%s', got: %s\n",
            case_id,
            expected_fragment,
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int expect_ir_dump_skip_return_check(const char *case_id,
    const char *source,
    const char *expected_text) {
    char *actual_text = NULL;
    int ok;

    if (!case_id || !source || !expected_text) {
        return 0;
    }

    if (!lower_source_to_ir_text_with_options(source, 1, 0, &actual_text)) {
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

    if (ir_lower_program(&program, NULL, &ir_program, &ir_err)) {
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

    if (ir_lower_program(&program, NULL, &ir_program, &ir_err)) {
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

static int expect_extension_ir_lower_succeeds(const char *case_id, const char *source) {
    char *actual_text = NULL;

    if (!case_id || !source) {
        return 0;
    }

    if (!lower_extension_source_to_ir_text(source, &actual_text)) {
        fprintf(stderr, "[ir-reg] FAIL: %s unexpectedly failed to lower under extension mode\n", case_id);
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

static int test_ir_lowers_pair_return_via_hidden_result_slots_under_extension(void) {
    return expect_extension_ir_dump_without_semantic("IR-AGGRET-PAIR-LOCAL",
        "pair mk(){ pair p={1,2}; return p; }\n"
        "int main(){ return 0; }\n",
        "func mk(__ret0.0, __ret1.1) {\n"
        "  bb.0:\n"
        "    p$first.2 = mov 1\n"
        "    p$second.3 = mov 2\n"
        "    0 = store_indirect __ret0.0, p$first.2\n"
        "    0 = store_indirect __ret1.1, p$second.3\n"
        "    ret\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    ret 0\n"
        "}\n");
}

static int test_ir_lowers_pair_call_init_via_hidden_result_slots_under_extension(void) {
    return expect_extension_ir_dump_without_semantic("IR-AGGRET-PAIR-CALL-INIT",
        "pair mk(){ pair p={1,2}; return p; }\n"
        "int main(){ pair q = mk(); return q.second; }\n",
        "func mk(__ret0.0, __ret1.1) {\n"
        "  bb.0:\n"
        "    p$first.2 = mov 1\n"
        "    p$second.3 = mov 2\n"
        "    0 = store_indirect __ret0.0, p$first.2\n"
        "    0 = store_indirect __ret1.1, p$second.3\n"
        "    ret\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    tmp.0 = addr_local local.0\n"
        "    tmp.1 = addr_local local.1\n"
        "    tmp.2 = call mk(tmp.0, tmp.1)\n"
        "    tmp.3 = mov q$second.1\n"
        "    ret tmp.3\n"
        "}\n");
}

static int test_ir_lowers_pair_call_result_as_aggregate_call_argument_under_extension(void) {
    return expect_extension_ir_dump("IR-AGGCALL-PAIR-CALLARG",
        "pair mk(){ pair p={1,2}; return p; }\n"
        "int sum(pair p){ return p.first + p.second; }\n"
        "int main(){ return sum(mk()); }\n",
        "func mk(__ret0.0, __ret1.1) {\n"
        "  bb.0:\n"
        "    p$first.2 = mov 1\n"
        "    p$second.3 = mov 2\n"
        "    0 = store_indirect __ret0.0, p$first.2\n"
        "    0 = store_indirect __ret1.1, p$second.3\n"
        "    ret\n"
        "}\n"
        "\n"
        "func sum(p.0, p.1) {\n"
        "  bb.0:\n"
        "    tmp.0 = mov p.0\n"
        "    tmp.1 = mov p.1\n"
        "    tmp.2 = add tmp.0, tmp.1\n"
        "    ret tmp.2\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    tmp.0 = addr_local local.0\n"
        "    tmp.1 = addr_local local.1\n"
        "    tmp.2 = call mk(tmp.0, tmp.1)\n"
        "    tmp.3 = call sum(__aggarg_tmp_0.0, __aggarg_tmp_1.1)\n"
        "    ret tmp.3\n"
        "}\n");
}

static int test_ir_lowers_pair_multi_hop_return_chain_under_extension(void) {
    return expect_extension_ir_dump("IR-AGGRET-PAIR-MULTIHOP",
        "pair mk(){ pair p={1,2}; return p; }\n"
        "pair id(pair p){ return p; }\n"
        "pair wrap(){ return id(mk()); }\n"
        "int main(){ pair q=wrap(); return q.second; }\n",
        "func mk(__ret0.0, __ret1.1) {\n"
        "  bb.0:\n"
        "    p$first.2 = mov 1\n"
        "    p$second.3 = mov 2\n"
        "    0 = store_indirect __ret0.0, p$first.2\n"
        "    0 = store_indirect __ret1.1, p$second.3\n"
        "    ret\n"
        "}\n"
        "\n"
        "func id(__ret0.0, __ret1.1, p.2, p.3) {\n"
        "  bb.0:\n"
        "    0 = store_indirect __ret0.0, p.2\n"
        "    0 = store_indirect __ret1.1, p.3\n"
        "    ret\n"
        "}\n"
        "\n"
        "func wrap(__ret0.0, __ret1.1) {\n"
        "  bb.0:\n"
        "    tmp.0 = addr_local local.2\n"
        "    tmp.1 = addr_local local.3\n"
        "    tmp.2 = call mk(tmp.0, tmp.1)\n"
        "    tmp.3 = call id(__ret0.0, __ret1.1, __aggarg_tmp_2.2, __aggarg_tmp_3.3)\n"
        "    ret\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    tmp.0 = addr_local local.0\n"
        "    tmp.1 = addr_local local.1\n"
        "    tmp.2 = call wrap(tmp.0, tmp.1)\n"
        "    tmp.3 = mov q$second.1\n"
        "    ret tmp.3\n"
        "}\n");
}

static int test_ir_lowers_nested_struct_multi_hop_return_chain_under_extension(void) {
    return expect_extension_ir_dump("IR-AGGRET-NESTED-STRUCT-MULTIHOP",
        "struct Mid { pair p; int z; };\n"
        "struct Mid mk(){ struct Mid m={{1,2},3}; return m; }\n"
        "struct Mid id(struct Mid m){ return m; }\n"
        "struct Mid wrap(){ return id(mk()); }\n"
        "int main(){ struct Mid q=wrap(); return q.p.second + q.z; }\n",
        "func mk(__ret0.0, __ret1.1, __ret2.2) {\n"
        "  bb.0:\n"
        "    m$p$0.3 = mov 1\n"
        "    m$p$1.4 = mov 2\n"
        "    m$z.5 = mov 3\n"
        "    0 = store_indirect __ret0.0, m$p$0.3\n"
        "    0 = store_indirect __ret1.1, m$p$1.4\n"
        "    0 = store_indirect __ret2.2, m$z.5\n"
        "    ret\n"
        "}\n"
        "\n"
        "func id(__ret0.0, __ret1.1, __ret2.2, m.3, m.4, m.5) {\n"
        "  bb.0:\n"
        "    0 = store_indirect __ret0.0, m.3\n"
        "    0 = store_indirect __ret1.1, m.4\n"
        "    0 = store_indirect __ret2.2, m.5\n"
        "    ret\n"
        "}\n"
        "\n"
        "func wrap(__ret0.0, __ret1.1, __ret2.2) {\n"
        "  bb.0:\n"
        "    tmp.0 = addr_local local.3\n"
        "    tmp.1 = addr_local local.4\n"
        "    tmp.2 = addr_local local.5\n"
        "    tmp.3 = call mk(tmp.0, tmp.1, tmp.2)\n"
        "    tmp.4 = call id(__ret0.0, __ret1.1, __ret2.2, __aggarg_tmp_3.3, __aggarg_tmp_4.4, __aggarg_tmp_5.5)\n"
        "    ret\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    tmp.0 = addr_local local.0\n"
        "    tmp.1 = addr_local local.1\n"
        "    tmp.2 = addr_local local.2\n"
        "    tmp.3 = call wrap(tmp.0, tmp.1, tmp.2)\n"
        "    tmp.4 = mov q$p$1.1\n"
        "    tmp.5 = mov q$z.2\n"
        "    tmp.6 = add tmp.4, tmp.5\n"
        "    ret tmp.6\n"
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
        "    tmp.0 = mov g.0\n"
        "    tmp.1 = add tmp.0, 2\n"
        "    g.0 = mov tmp.1\n"
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
        "    tmp.0 = mov g.0\n"
        "    tmp.1 = shl tmp.0, 1\n"
        "    g.0 = mov tmp.1\n"
        "    tmp.2 = mov g.0\n"
        "    tmp.3 = and tmp.2, 6\n"
        "    g.0 = mov tmp.3\n"
        "    tmp.4 = mov g.0\n"
        "    tmp.5 = xor tmp.4, 1\n"
        "    g.0 = mov tmp.5\n"
        "    tmp.6 = mov g.0\n"
        "    tmp.7 = or tmp.6, 8\n"
        "    g.0 = mov tmp.7\n"
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
        "    tmp.0 = mov g.0\n"
        "    tmp.1 = add tmp.0, 1\n"
        "    g.0 = mov tmp.1\n"
        "    tmp.2 = mov g.0\n"
        "    tmp.3 = mov tmp.2\n"
        "    tmp.4 = sub tmp.2, 1\n"
        "    g.0 = mov tmp.4\n"
        "    tmp.5 = add tmp.1, tmp.3\n"
        "    ret tmp.5\n"
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
        "    tmp.0 = mov g.0\n"
        "    tmp.1 = add tmp.0, 2\n"
        "    g.0 = mov tmp.1\n"
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

static int test_ir_lowers_float_literal_runtime_global_initializer_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "float g = 1.25;\nfloat id(float x){ return x; }\nint main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "global g.0:float\n") != NULL &&
        strstr(actual_text, "func __global.init() {\n") != NULL &&
        strstr(actual_text, "g.0 = mov 1067450368\n") != NULL &&
        strstr(actual_text, "call __global.init()") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-GLOBAL-LITERAL-RUNTIME-INIT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
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

static int test_ir_rejects_runtime_global_initializer_dep_on_uninitialized_global_array_subscript(void) {
    return expect_ir_lower_fails("IR-GLOBAL-RUNTIME-INIT-DEP-ARRAY-SUBSCRIPT",
        "int a;\nint b[1];\nint a=b[0];\nint main(){return a;}\n",
        "depends on not-yet-initialized global 'b'");
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

static int test_ir_accepts_runtime_global_array_initializer_with_short_circuit_and_calls(void) {
    return expect_ir_lower_succeeds("IR-GLOBAL-RUNTIME-ARRAY-INIT-LOGICAL-CALLS",
        "int seed=2;\n"
        "int next(){seed=seed+3; return seed;}\n"
        "int arr[8]={1, next(), seed+10, 0&&next(), 1||next(), next(), {seed}, {}};\n"
        "int main(){return arr[0]+arr[1]+arr[2]+arr[3]+arr[4]+arr[5]+arr[6]+arr[7];}\n");
}

static int test_ir_accepts_multiple_runtime_global_initializers_after_branchy_helper_growth(void) {
    return expect_ir_lower_succeeds("IR-GLOBAL-RUNTIME-MULTI-INIT-AFTER-BRANCHY-HELPER",
        "int t(){return 5;}\n"
        "int z(){return 0;}\n"
        "int a=z()&&t();\n"
        "int b=t()||z();\n"
        "int c[2]={a+7,b+9};\n"
        "int main(){return c[0]+c[1];}\n");
}

static int test_ir_lowers_1d_array_initializer_mixing_nested_and_scalar_items(void) {
    return expect_ir_dump("IR-LOCAL-ARRAY-NESTED-SCALAR-MIX",
        "int main(){ int a[4]={{1,2},3}; return a[0]+a[1]*10+a[2]*100+a[3]*1000; }\n",
        "func main() {\n"
        "  bb.0:\n"
        "    a.0 = mov 1\n"
        "    a.1 = mov 3\n"
        "    a.2 = mov 0\n"
        "    a.3 = mov 0\n"
        "    tmp.0 = addr_local local.0\n"
        "    tmp.1 = mul 0, 4\n"
        "    tmp.2 = add tmp.0, tmp.1\n"
        "    tmp.3 = load_indirect tmp.2\n"
        "    tmp.4 = addr_local local.0\n"
        "    tmp.5 = mul 1, 4\n"
        "    tmp.6 = add tmp.4, tmp.5\n"
        "    tmp.7 = load_indirect tmp.6\n"
        "    tmp.8 = mul tmp.7, 10\n"
        "    tmp.9 = add tmp.3, tmp.8\n"
        "    tmp.10 = addr_local local.0\n"
        "    tmp.11 = mul 2, 4\n"
        "    tmp.12 = add tmp.10, tmp.11\n"
        "    tmp.13 = load_indirect tmp.12\n"
        "    tmp.14 = mul tmp.13, 100\n"
        "    tmp.15 = add tmp.9, tmp.14\n"
        "    tmp.16 = addr_local local.0\n"
        "    tmp.17 = mul 3, 4\n"
        "    tmp.18 = add tmp.16, tmp.17\n"
        "    tmp.19 = load_indirect tmp.18\n"
        "    tmp.20 = mul tmp.19, 1000\n"
        "    tmp.21 = add tmp.15, tmp.20\n"
        "    ret tmp.21\n"
        "}\n");
}

static int test_ir_compacts_sparse_large_local_array_initializer(void) {
    return expect_ir_dump("IR-LOCAL-ARRAY-SPARSE-LARGE-COMPACT",
        "int main(){ int a[256]={1}; return a[0]; }\n",
        "func main() {\n"
        "  bb.0:\n"
        "    __arr_init_i.256 = mov 0\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    tmp.0 = mov __arr_init_i.256\n"
        "    tmp.1 = lt tmp.0, 256\n"
        "    br tmp.1, bb.2, bb.3\n"
        "  bb.2:\n"
        "    tmp.2 = addr_local local.0\n"
        "    tmp.3 = mul tmp.0, 4\n"
        "    tmp.4 = add tmp.2, tmp.3\n"
        "    0 = store_indirect tmp.4, 0\n"
        "    tmp.5 = add tmp.0, 1\n"
        "    __arr_init_i.256 = mov tmp.5\n"
        "    jmp bb.1\n"
        "  bb.3:\n"
        "    a.0 = mov 1\n"
        "    tmp.6 = addr_local local.0\n"
        "    tmp.7 = mul 0, 4\n"
        "    tmp.8 = add tmp.6, tmp.7\n"
        "    tmp.9 = load_indirect tmp.8\n"
        "    ret tmp.9\n"
        "}\n");
}

static int test_ir_accepts_2d_array_initializer_mixing_nested_and_scalar_items(void) {
    return expect_ir_lower_succeeds("IR-2D-ARRAY-NESTED-SCALAR-MIX",
        "int a[2][3]={{1},2,3,{4,5}};\n"
        "int main(){return a[0][0]+a[0][1]*10+a[0][2]*100+a[1][0]*1000+a[1][1]*10000+a[1][2]*100000;}\n");
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

static int test_ir_preserves_global_array_declaration_metadata(void) {
    return expect_ir_dump("IR-DECL-GLOBAL-ARRAYS",
        "int a[10];\nint b[2][3] = {1, 2, 3};\nint main(){return 2;}\n",
        "global a.0[rank=1]\n"
        "global b.1[rank=2]\n"
        "\n"
        "func __global.init() {\n"
        "  bb.0:\n"
        "    tmp.0 = addr_global global.1\n"
        "    0 = store_indirect tmp.0, 1\n"
        "    tmp.1 = add tmp.0, 4\n"
        "    0 = store_indirect tmp.1, 2\n"
        "    tmp.2 = add tmp.0, 8\n"
        "    0 = store_indirect tmp.2, 3\n"
        "    ret 0\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    tmp.0 = call __global.init()\n"
        "    ret 2\n"
        "}\n");
}

static int test_ir_lowers_unused_local_array_declarations(void) {
    return expect_ir_dump("IR-DECL-LOCAL-ARRAYS",
        "int main(){int a[3] = {}; int b[2][4]; return 7;}\n",
        "func main() {\n"
        "  bb.0:\n"
        "    a.0 = mov 0\n"
        "    a.1 = mov 0\n"
        "    a.2 = mov 0\n"
        "    ret 7\n"
        "}\n");
}

static int test_ir_preserves_array_parameter_signature_metadata(void) {
    return expect_ir_dump("IR-DECL-ARRAY-PARAMS",
        "int f(int arr[], int grid[][]);\nint main(){return 0;}\n",
        "declare f(arr.0[rank=1], grid.1[rank=2])\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    ret 0\n"
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

static int test_ir_keeps_for_exit_when_function_step_updates_local_condition_value(void) {
    return expect_ir_dump("IR-FOR-LOCAL-COND-CALL-STEP",
        "int step(int a){return a+1;}\n"
        "int main(){int b=0; for(;b<2;b=step(b)){} return b;}\n",
        "func step(a.0) {\n"
        "  bb.0:\n"
        "    tmp.0 = add a.0, 1\n"
        "    ret tmp.0\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    b.0 = mov 0\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    tmp.0 = lt b.0, 2\n"
        "    br tmp.0, bb.2, bb.4\n"
        "  bb.2:\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    tmp.1 = call step(b.0)\n"
        "    b.0 = mov tmp.1\n"
        "    jmp bb.1\n"
        "  bb.4:\n"
        "    ret b.0\n"
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
        "    tmp.0 = mov x.0\n"
        "    tmp.1 = add tmp.0, 2\n"
        "    x.0 = mov tmp.1\n"
        "    tmp.2 = mov x.0\n"
        "    tmp.3 = shl tmp.2, 1\n"
        "    x.0 = mov tmp.3\n"
        "    tmp.4 = add tmp.1, tmp.3\n"
        "    ret tmp.4\n"
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
        "    tmp.0 = mov x.0\n"
        "    tmp.1 = add tmp.0, 1\n"
        "    x.0 = mov tmp.1\n"
        "    ret tmp.1\n"
        "}\n");
}

static int test_ir_lowers_postfix_increment(void) {
    return expect_ir_dump("IR-POSTFIX-INC",
        "int f(){int x=1; return x++;}\n",
        "func f() {\n"
        "  bb.0:\n"
        "    x.0 = mov 1\n"
        "    tmp.0 = mov x.0\n"
        "    tmp.1 = mov tmp.0\n"
        "    tmp.2 = add tmp.0, 1\n"
        "    x.0 = mov tmp.2\n"
        "    ret tmp.1\n"
        "}\n");
}

static int test_ir_lowers_prefix_decrement(void) {
    return expect_ir_dump("IR-PREFIX-DEC",
        "int f(){int x=1; return --x;}\n",
        "func f() {\n"
        "  bb.0:\n"
        "    x.0 = mov 1\n"
        "    tmp.0 = mov x.0\n"
        "    tmp.1 = sub tmp.0, 1\n"
        "    x.0 = mov tmp.1\n"
        "    ret tmp.1\n"
        "}\n");
}

static int test_ir_lowers_postfix_decrement(void) {
    return expect_ir_dump("IR-POSTFIX-DEC",
        "int f(){int x=1; return x--;}\n",
        "func f() {\n"
        "  bb.0:\n"
        "    x.0 = mov 1\n"
        "    tmp.0 = mov x.0\n"
        "    tmp.1 = mov tmp.0\n"
        "    tmp.2 = sub tmp.0, 1\n"
        "    x.0 = mov tmp.2\n"
        "    ret tmp.1\n"
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

static int test_ir_lowers_defer_before_return(void) {
    return expect_extension_ir_dump("IR-DEFER-RETURN",
        "int main(){ putint(1); defer putint(2); putint(3); return 0; }\n",
        "declare putint(param0.0)\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    tmp.0 = call putint(1)\n"
        "    tmp.1 = call putint(3)\n"
        "    tmp.2 = call putint(2)\n"
        "    ret 0\n"
        "}\n");
}

static int test_ir_lowers_nested_defer_fallthrough_order(void) {
    return expect_extension_ir_dump("IR-DEFER-NESTED-FALLTHROUGH",
        "int main(){ defer putint(1); { defer putint(2); } return 0; }\n",
        "declare putint(param0.0)\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    tmp.0 = call putint(2)\n"
        "    tmp.1 = call putint(1)\n"
        "    ret 0\n"
        "}\n");
}

static int test_ir_lowers_defer_with_loop_break(void) {
    return expect_extension_ir_lower_succeeds("IR-DEFER-LOOP-BREAK",
        "int main(){ while(1){ defer putint(1); break; } return 0; }\n");
}

static int test_ir_lowers_defer_with_loop_continue(void) {
    return expect_extension_ir_lower_succeeds("IR-DEFER-LOOP-CONTINUE",
        "int main(){ int i=0; while(i<2){ { defer putint(i); i=i+1; continue; } } return 0; }\n");
}

static int test_ir_lowers_nested_defer_body_scope_exit_order(void) {
    return expect_extension_ir_dump("IR-DEFER-NESTED-BODY-SCOPE-EXIT",
        "int main(){ defer { defer putint(1); { defer putint(2); } putint(3); } return 0; }\n",
        "declare putint(param0.0)\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    tmp.0 = call putint(2)\n"
        "    tmp.1 = call putint(3)\n"
        "    tmp.2 = call putint(1)\n"
        "    ret 0\n"
        "}\n");
}

static int test_ir_lowers_nested_multi_defer_body_lifo_order(void) {
    return expect_extension_ir_dump("IR-DEFER-NESTED-BODY-LIFO",
        "int main(){ defer { defer putint(1); defer putint(2); { defer putint(3); } putint(4); } return 0; }\n",
        "declare putint(param0.0)\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    tmp.0 = call putint(3)\n"
        "    tmp.1 = call putint(4)\n"
        "    tmp.2 = call putint(2)\n"
        "    tmp.3 = call putint(1)\n"
        "    ret 0\n"
        "}\n");
}

static int test_ir_lowers_defer_using_exit_time_local_value(void) {
    return expect_extension_ir_dump("IR-DEFER-EXIT-TIME-LOCAL-VALUE",
        "int main(){ int x=1; defer putint(x); x=2; return 0; }\n",
        "declare putint(param0.0)\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    x.0 = mov 1\n"
        "    x.0 = mov 2\n"
        "    tmp.0 = call putint(x.0)\n"
        "    ret 0\n"
        "}\n");
}

static int test_ir_lowers_defer_if_else_using_exit_time_condition_value(void) {
    return expect_extension_ir_dump("IR-DEFER-IF-ELSE-EXIT-TIME-COND",
        "int main(){ int x=0; defer if(x) putint(1); else putint(2); x=1; return 0; }\n",
        "declare putint(param0.0)\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    x.0 = mov 0\n"
        "    x.0 = mov 1\n"
        "    tmp.0 = call putint(1)\n"
        "    ret 0\n"
        "}\n");
}

static int test_ir_lowers_return_unwinding_inner_and_outer_defers(void) {
    return expect_extension_ir_dump("IR-DEFER-RETURN-UNWIND-INNER-OUTER",
        "int main(){ defer putint(1); { defer putint(2); return 0; } }\n",
        "declare putint(param0.0)\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    tmp.0 = call putint(2)\n"
        "    tmp.1 = call putint(1)\n"
        "    ret 0\n"
        "}\n");
}

static int test_ir_lowers_return_expression_before_defer_side_effects(void) {
    return expect_extension_ir_dump("IR-DEFER-RETURN-EXPR-BEFORE-DEFER",
        "int main(){ int x=1; defer x=2; return x; }\n",
        "func main() {\n"
        "  bb.0:\n"
        "    x.0 = mov 1\n"
        "    tmp.0 = mov x.0\n"
        "    x.0 = mov 2\n"
        "    ret tmp.0\n"
        "}\n");
}

static int test_ir_lowers_return_expression_side_effect_before_defer_unwind(void) {
    return expect_extension_ir_dump("IR-DEFER-RETURN-EXPR-SIDE-EFFECT-BEFORE-DEFER",
        "int main(){ int x=1; defer x=3; return x=2; }\n",
        "func main() {\n"
        "  bb.0:\n"
        "    x.0 = mov 1\n"
        "    x.0 = mov 2\n"
        "    x.0 = mov 3\n"
        "    ret 2\n"
        "}\n");
}

static int test_ir_lowers_defer_in_for_body_before_step_update(void) {
    return expect_extension_ir_dump("IR-DEFER-FOR-BODY-BEFORE-STEP",
        "int main(){ int i=0; for(;i<2;i=i+1){ defer putint(i); } return 0; }\n",
        "declare putint(param0.0)\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    i.0 = mov 0\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    tmp.0 = lt i.0, 2\n"
        "    br tmp.0, bb.2, bb.4\n"
        "  bb.2:\n"
        "    tmp.1 = call putint(i.0)\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    tmp.2 = add i.0, 1\n"
        "    i.0 = mov tmp.2\n"
        "    jmp bb.1\n"
        "  bb.4:\n"
        "    ret 0\n"
        "}\n");
}

static int test_ir_lowers_for_return_unwinding_body_then_outer_defers_without_step(void) {
    return expect_extension_ir_dump("IR-DEFER-FOR-RETURN-UNWIND-NO-STEP",
        "int main(){ defer putint(1); for(int i=0;i<3;i=i+1){ defer putint(2); return 0; } return 0; }\n",
        "declare putint(param0.0)\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    i.0 = mov 0\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    jmp bb.2\n"
        "  bb.2:\n"
        "    tmp.0 = call putint(2)\n"
        "    tmp.1 = call putint(1)\n"
        "    ret 0\n"
        "}\n");
}

static int test_ir_lowers_for_break_unwinding_body_then_outer_defers_without_step(void) {
    return expect_extension_ir_dump("IR-DEFER-FOR-BREAK-UNWIND-NO-STEP",
        "int main(){ defer putint(1); for(int i=0;i<3;i=i+1){ defer putint(2); break; } return 0; }\n",
        "declare putint(param0.0)\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    i.0 = mov 0\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    tmp.0 = lt i.0, 3\n"
        "    br tmp.0, bb.2, bb.3\n"
        "  bb.2:\n"
        "    tmp.1 = call putint(2)\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    tmp.2 = call putint(1)\n"
        "    ret 0\n"
        "}\n");
}

static int test_ir_lowers_unless_as_negated_if_under_extension(void) {
    return expect_extension_ir_dump("IR-UNLESS-NEGATED-IF",
        "int main(){ unless(0) putint(1); return 0; }\n",
        "declare putint(param0.0)\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    tmp.0 = call putint(1)\n"
        "    ret 0\n"
        "}\n");
}

static int test_ir_lowers_zero_arg_function_valued_parameter_specialization_under_extension(void) {
    return expect_extension_ir_dump("IR-FNVAL-ZERO-ARG-SPECIALIZATION",
        "int next(){ return 9; }\n"
        "int apply0(int f()){ return f(); }\n"
        "int main(){ return apply0(next); }\n",
        "shape shape.0() -> int\n"
        "\n"
        "func next() {\n"
        "  bb.0:\n"
        "    ret 9\n"
        "}\n"
        "\n"
        "declare apply0(f.0)\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    tmp.0 = fn_make __fnwrap_next, 0, shape.0\n"
        "    tmp.1 = call_indirect tmp.0()\n"
        "    ret tmp.1\n"
        "}\n"
        "\n"
        "func __fnwrap_next(__env.0) {\n"
        "  bb.0:\n"
        "    tmp.0 = call next()\n"
        "    ret tmp.0\n"
        "}\n");
}

static int test_ir_lowers_zero_arg_void_function_valued_parameter_specialization_under_extension(void) {
    return expect_extension_ir_dump("IR-FNVAL-ZERO-ARG-VOID-SPECIALIZATION",
        "void ping(){ putint(1); }\n"
        "void apply0(void f()){ f(); }\n"
        "int main(){ apply0(ping); return 0; }\n",
        "shape shape.0() -> void\n"
        "\n"
        "declare putint(param0.0)\n"
        "\n"
        "func ping() {\n"
        "  bb.0:\n"
        "    tmp.0 = call putint(1)\n"
        "    ret\n"
        "}\n"
        "\n"
        "declare apply0(f.0)\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    tmp.0 = fn_make __fnwrap_ping, 0, shape.0\n"
        "    tmp.1 = call_indirect tmp.0()\n"
        "    ret 0\n"
        "}\n"
        "\n"
        "func __fnwrap_ping(__env.0) {\n"
        "  bb.0:\n"
        "    tmp.0 = call ping()\n"
        "    ret\n"
        "}\n");
}

static int test_ir_lowers_capdefer_snapshot_bindings_under_extension(void) {
    return expect_extension_ir_dump("IR-CAPDEFER-SNAPSHOT-BINDINGS",
        "int main(){ int x=1; capdefer(y=x) putint(y); x=2; return 0; }\n",
        "declare putint(param0.0)\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    x.0 = mov 1\n"
        "    __capdefer_0_y.1 = mov x.0\n"
        "    x.0 = mov 2\n"
        "    tmp.0 = call putint(__capdefer_0_y.1)\n"
        "    ret 0\n"
        "}\n");
}

static int test_ir_lowers_capdefer_multiple_snapshot_bindings_under_extension(void) {
    return expect_extension_ir_dump("IR-CAPDEFER-MULTIPLE-SNAPSHOT-BINDINGS",
        "int main(){ int x=1; int z=2; capdefer(y=x, w=z) putint(y+w); x=3; z=4; return 0; }\n",
        "declare putint(param0.0)\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    x.0 = mov 1\n"
        "    z.1 = mov 2\n"
        "    __capdefer_0_y.2 = mov x.0\n"
        "    __capdefer_1_w.3 = mov z.1\n"
        "    x.0 = mov 3\n"
        "    z.1 = mov 4\n"
        "    tmp.0 = add __capdefer_0_y.2, __capdefer_1_w.3\n"
        "    tmp.1 = call putint(tmp.0)\n"
        "    ret 0\n"
        "}\n");
}

static int test_ir_lowers_multiple_fndefer_registrations_inside_loop_under_extension(void) {
    return expect_extension_ir_dump(
        "IR-FNDEFER-LOOP-DYNAMIC-REPLAY",
        "int a[8]={1,2,3,4,5,6,7,8};\n"
        "int main(){ for(int i=0;i<8;i=i+1){ fndefer putint(a[2]); } return 0; }\n",
        "global a.0[rank=1]\n"
        "\n"
        "declare putint(param0.0)\n"
        "\n"
        "func __global.init() {\n"
        "  bb.0:\n"
        "    tmp.0 = addr_global global.0\n"
        "    0 = store_indirect tmp.0, 1\n"
        "    tmp.1 = add tmp.0, 4\n"
        "    0 = store_indirect tmp.1, 2\n"
        "    tmp.2 = add tmp.0, 8\n"
        "    0 = store_indirect tmp.2, 3\n"
        "    tmp.3 = add tmp.0, 12\n"
        "    0 = store_indirect tmp.3, 4\n"
        "    tmp.4 = add tmp.0, 16\n"
        "    0 = store_indirect tmp.4, 5\n"
        "    tmp.5 = add tmp.0, 20\n"
        "    0 = store_indirect tmp.5, 6\n"
        "    tmp.6 = add tmp.0, 24\n"
        "    0 = store_indirect tmp.6, 7\n"
        "    tmp.7 = add tmp.0, 28\n"
        "    0 = store_indirect tmp.7, 8\n"
        "    ret 0\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    tmp.13 = call __global.init()\n"
        "    __fndefer_count_0.0 = mov 0\n"
        "    i.1 = mov 0\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    tmp.0 = lt i.1, 8\n"
        "    br tmp.0, bb.2, bb.4\n"
        "  bb.2:\n"
        "    tmp.1 = mov __fndefer_count_0.0\n"
        "    tmp.2 = add tmp.1, 1\n"
        "    __fndefer_count_0.0 = mov tmp.2\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    tmp.3 = add i.1, 1\n"
        "    i.1 = mov tmp.3\n"
        "    jmp bb.1\n"
        "  bb.4:\n"
        "    __fndefer_ret.2 = mov 0\n"
        "    __fndefer_i.3 = mov 0\n"
        "    jmp bb.5\n"
        "  bb.5:\n"
        "    tmp.4 = mov __fndefer_i.3\n"
        "    tmp.5 = mov __fndefer_count_0.0\n"
        "    tmp.6 = lt tmp.4, tmp.5\n"
        "    br tmp.6, bb.6, bb.7\n"
        "  bb.6:\n"
        "    tmp.7 = addr_global global.0\n"
        "    tmp.8 = mul 2, 4\n"
        "    tmp.9 = add tmp.7, tmp.8\n"
        "    tmp.10 = load_indirect tmp.9\n"
        "    tmp.11 = call putint(tmp.10)\n"
        "    tmp.12 = add __fndefer_i.3, 1\n"
        "    __fndefer_i.3 = mov tmp.12\n"
        "    jmp bb.5\n"
        "  bb.7:\n"
        "    ret __fndefer_ret.2\n"
        "}\n");
}

static int test_ir_lowers_float_transport_signature_under_extension(void) {
    return expect_extension_ir_dump("IR-FLOAT-TRANSPORT-SIGNATURE",
        "float g; float id(float x){ return x; } int main(){ return 0; }\n",
        "global g.0:float\n"
        "\n"
        "func id(x.0:float) {\n"
        "  bb.0:\n"
        "    ret x.0\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    ret 0\n"
        "}\n");
}

static int test_ir_lowers_float_literal_transport_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "float id(float x){ return x; }\nint main(){ float x = 1.25; id(1.25); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func id(x.0:float) {\n") != NULL &&
        strstr(actual_text, "x.0 = mov 1067450368") != NULL &&
        strstr(actual_text, "call id(1067450368)") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-LITERAL-TRANSPORT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_ir_lowers_float_global_initializer_transport_from_identifier_under_extension(void) {
    return expect_extension_ir_dump("IR-FLOAT-GLOBAL-IDENT-INIT",
        "float g = 1.25;\n"
        "float h = g;\n"
        "int main(){ return 0; }\n",
        "global g.0:float\n"
        "global h.1:float\n"
        "\n"
        "func __global.init() {\n"
        "  bb.0:\n"
        "    g.0 = mov 1067450368\n"
        "    h.1 = mov g.0\n"
        "    ret 0\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    tmp.0 = call __global.init()\n"
        "    ret 0\n"
        "}\n");
}

static int test_ir_lowers_float_global_initializer_transport_from_call_under_extension(void) {
    return expect_extension_ir_dump("IR-FLOAT-GLOBAL-CALL-INIT",
        "float g = 1.25;\n"
        "float id(float x){ return x; }\n"
        "float h = id(g);\n"
        "int main(){ return 0; }\n",
        "global g.0:float\n"
        "global h.1:float\n"
        "\n"
        "func __global.init() {\n"
        "  bb.0:\n"
        "    g.0 = mov 1067450368\n"
        "    tmp.0 = call id(g.0)\n"
        "    h.1 = mov tmp.0\n"
        "    ret 0\n"
        "}\n"
        "\n"
        "func id(x.0:float) {\n"
        "  bb.0:\n"
        "    ret x.0\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    tmp.0 = call __global.init()\n"
        "    ret 0\n"
        "}\n");
}

static int test_ir_lowers_float_return_transport_from_global_under_extension(void) {
    return expect_extension_ir_dump("IR-FLOAT-RETURN-GLOBAL",
        "float g = 1.25;\n"
        "float get(){ return g; }\n"
        "int main(){ return 0; }\n",
        "global g.0:float\n"
        "\n"
        "func __global.init() {\n"
        "  bb.0:\n"
        "    g.0 = mov 1067450368\n"
        "    ret 0\n"
        "}\n"
        "\n"
        "func get() {\n"
        "  bb.0:\n"
        "    ret g.0\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    tmp.0 = call __global.init()\n"
        "    ret 0\n"
        "}\n");
}

static int test_ir_lowers_float_return_transport_from_global_call_under_extension(void) {
    return expect_extension_ir_dump("IR-FLOAT-RETURN-GLOBAL-CALL",
        "float g = 1.25;\n"
        "float id(float x){ return x; }\n"
        "float get(){ return id(g); }\n"
        "int main(){ return 0; }\n",
        "global g.0:float\n"
        "\n"
        "func __global.init() {\n"
        "  bb.0:\n"
        "    g.0 = mov 1067450368\n"
        "    ret 0\n"
        "}\n"
        "\n"
        "func id(x.0:float) {\n"
        "  bb.0:\n"
        "    ret x.0\n"
        "}\n"
        "\n"
        "func get() {\n"
        "  bb.0:\n"
        "    tmp.0 = call id(g.0)\n"
        "    ret tmp.0\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    tmp.0 = call __global.init()\n"
        "    ret 0\n"
        "}\n");
}

static int test_ir_lowers_float_global_call_chain_transport_under_extension(void) {
    return expect_extension_ir_dump("IR-FLOAT-GLOBAL-CALL-CHAIN",
        "float g = 1.25;\n"
        "float id(float x){ return x; }\n"
        "float wrap(float x){ return id(x); }\n"
        "float getg(){ return wrap(g); }\n"
        "int main(){ return 0; }\n",
        "global g.0:float\n"
        "\n"
        "func __global.init() {\n"
        "  bb.0:\n"
        "    g.0 = mov 1067450368\n"
        "    ret 0\n"
        "}\n"
        "\n"
        "func id(x.0:float) {\n"
        "  bb.0:\n"
        "    ret x.0\n"
        "}\n"
        "\n"
        "func wrap(x.0:float) {\n"
        "  bb.0:\n"
        "    tmp.0 = call id(x.0)\n"
        "    ret tmp.0\n"
        "}\n"
        "\n"
        "func getg() {\n"
        "  bb.0:\n"
        "    tmp.0 = call wrap(g.0)\n"
        "    ret tmp.0\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    tmp.0 = call __global.init()\n"
        "    ret 0\n"
        "}\n");
}

static int test_ir_lowers_float_local_call_chain_transport_under_extension(void) {
    return expect_extension_ir_dump("IR-FLOAT-LOCAL-CALL-CHAIN",
        "float id(float x){ return x; }\n"
        "float wrap(float x){ return id(x); }\n"
        "float bounce(float x){ float y; y = x; return wrap(y); }\n"
        "int main(){ return 0; }\n",
        "func id(x.0:float) {\n"
        "  bb.0:\n"
        "    ret x.0\n"
        "}\n"
        "\n"
        "func wrap(x.0:float) {\n"
        "  bb.0:\n"
        "    tmp.0 = call id(x.0)\n"
        "    ret tmp.0\n"
        "}\n"
        "\n"
        "func bounce(x.0:float) {\n"
        "  bb.0:\n"
        "    y.1 = mov x.0\n"
        "    tmp.0 = call wrap(y.1)\n"
        "    ret tmp.0\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    ret 0\n"
        "}\n");
}

static int test_ir_lowers_returned_function_value_parameter_immediate_call_under_extension(void) {
    return expect_extension_ir_dump("IR-RETURNED-FUNCTION-VALUE-PARAM-IMMEDIATE-CALL",
        "int id(int f(int))(int) { return f; }\n"
        "int add1(int x){ return x+1; }\n"
        "int main(){ return id(add1)(41); }\n",
        "shape shape.0(int) -> int\n"
        "\n"
        "declare id(__ret0.0, f.1)\n"
        "\n"
        "func add1(x.0) {\n"
        "  bb.0:\n"
        "    tmp.0 = add x.0, 1\n"
        "    ret tmp.0\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    tmp.0 = fn_make __fnwrap_add1, 0, shape.0\n"
        "    tmp.1 = call_indirect tmp.0(41)\n"
        "    ret tmp.1\n"
        "}\n"
        "\n"
        "func __fnwrap_add1(__env.0, x.1) {\n"
        "  bb.0:\n"
        "    tmp.0 = call add1(x.1)\n"
        "    ret tmp.0\n"
        "}\n");
}

static int test_ir_accepts_returned_closure_backed_function_value_parameter_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int id(int f(int))(int) { return f; }\n"
            "int main(){ int x=3; int f(int)=closure [x] int (int y) { return x + y; }; return id(f)(4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func main() {\n") != NULL &&
        strstr(actual_text, "f$closurecap$0.1 = mov x.0\n") != NULL &&
        strstr(actual_text, "f$ftag.2 = mov 1\n") != NULL &&
        strstr(actual_text, "shape shape.0(int) -> int\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL &&
        strstr(actual_text, "__retclosure_declslot") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: returned closure-backed function-value parameter immediate call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_returned_closure_backed_function_value_parameter_bind_and_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int id(int f(int))(int) { return f; }\n"
            "int main(){ int x=3; int f(int)=closure [x] int (int y) { return x + y; }; int g(int)=id(f); return g(4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func main() {\n") != NULL &&
        strstr(actual_text, "f$closurecap$0.1 = mov x.0\n") != NULL &&
        strstr(actual_text, "f$ftag.2 = mov 1\n") != NULL &&
        strstr(actual_text, "g$ftag.3 = mov f$ftag.2\n") != NULL &&
        strstr(actual_text, "g$closurecap$0.4 = mov f$closurecap$0.1\n") != NULL &&
        strstr(actual_text, "shape shape.0(int) -> int\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "__retclosure_declslot") == NULL &&
        strstr(actual_text, "id__fv_0_main__closure_f_") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: returned closure-backed function-value parameter bind-and-call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_closure_backed_function_value_parameter_bind_and_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int id(int f(int))(int) { return f; }\n"
            "int main(){ int c=1; int x=3; int y=5; int f(int)=closure [x] int (int z) { return x + z; }; int g(int)=closure [y] int (int z) { return y + z; }; if(c) f=g; int h(int)=id(f); return h(4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func main() {\n") != NULL &&
        strstr(actual_text, "f$closurecap$0.3 = mov x.1\n") != NULL &&
        strstr(actual_text, "g$closurecap$0.5 = mov y.2\n") != NULL &&
        strstr(actual_text, "f$closurecap$0.3 = mov g$closurecap$0.5\n") != NULL &&
        strstr(actual_text, "f$ftag.4 = mov 2\n") != NULL &&
        strstr(actual_text, "h$ftag.7 = mov f$ftag.4\n") != NULL &&
        strstr(actual_text, "h$closurecap$0.8 = mov f$closurecap$0.3\n") != NULL &&
        strstr(actual_text, "shape shape.0(int) -> int\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "__retclosure_declslot") == NULL &&
        strstr(actual_text, "id__fv_0_main__closure_g_") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned closure-backed function-value parameter bind-and-call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_closure_backed_function_value_parameter_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int id(int f(int))(int) { return f; }\n"
            "int main(){ int c=1; int x=3; int y=5; int f(int)=closure [x] int (int z) { return x + z; }; int g(int)=closure [y] int (int z) { return y + z; }; if(c) f=g; return id(f)(4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "f$closurecap$0.3 = mov x.1\n") != NULL &&
        strstr(actual_text, "g$closurecap$0.5 = mov y.2\n") != NULL &&
        strstr(actual_text, "f$closurecap$0.3 = mov g$closurecap$0.5\n") != NULL &&
        strstr(actual_text, "f$ftag.4 = mov 2\n") != NULL &&
        strstr(actual_text, "shape shape.0(int) -> int\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "call id(") == NULL &&
        strstr(actual_text, "__retclosure_declslot") == NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned closure-backed function-value parameter immediate call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_two_arg_closure_backed_function_value_parameter_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int id(int f(int,int))(int,int) { return f; }\n"
            "int main(){ int c=1; int x=3; int y=5; int f(int,int)=closure [x] int (int a, int b) { return x + a + b; }; int g(int,int)=closure [y] int (int a, int b) { return y + a + b; }; if(c) f=g; return id(f)(3, 2); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "shape shape.0(int, int) -> int\n") != NULL &&
        strstr(actual_text, "f$closurecap$0.3 = mov x.1\n") != NULL &&
        strstr(actual_text, "g$closurecap$0.5 = mov y.2\n") != NULL &&
        strstr(actual_text, "f$closurecap$0.3 = mov g$closurecap$0.5\n") != NULL &&
        strstr(actual_text, "f$ftag.4 = mov 2\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.1(3, 2)\n") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "call id(") == NULL &&
        strstr(actual_text, "__retclosure_declslot") == NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned two-arg closure-backed function-value parameter immediate call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_zero_arg_closure_backed_function_value_parameter_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int id(int f())() { return f; }\n"
            "int main(){ int c=1; int x=3; int y=5; int f()=closure [x] int () { return x; }; int g()=closure [y] int () { return y; }; if(c) f=g; return id(f)(); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "shape shape.0() -> int\n") != NULL &&
        strstr(actual_text, "f$closurecap$0.3 = mov x.1\n") != NULL &&
        strstr(actual_text, "g$closurecap$0.5 = mov y.2\n") != NULL &&
        strstr(actual_text, "f$closurecap$0.3 = mov g$closurecap$0.5\n") != NULL &&
        strstr(actual_text, "f$ftag.4 = mov 2\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.1()\n") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "call id(") == NULL &&
        strstr(actual_text, "__retclosure_declslot") == NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned zero-arg closure-backed function-value parameter immediate call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_zero_arg_void_closure_backed_function_value_parameter_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "void id(void f())() { return f; }\n"
            "int main(){ int c=1; int x=3; int y=5; void f()=closure [x] void () { putint(x); return; }; void g()=closure [y] void () { putint(y); return; }; if(c) f=g; id(f)(); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "shape shape.0() -> void\n") != NULL &&
        strstr(actual_text, "f$closurecap$0.3 = mov x.1\n") != NULL &&
        strstr(actual_text, "g$closurecap$0.5 = mov y.2\n") != NULL &&
        strstr(actual_text, "f$closurecap$0.3 = mov g$closurecap$0.5\n") != NULL &&
        strstr(actual_text, "f$ftag.4 = mov 2\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.1()\n") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "call id(") == NULL &&
        strstr(actual_text, "__retclosure_declslot") == NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned zero-arg void closure-backed function-value parameter immediate call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_rejects_global_float_operator_expression_under_extension(void) {
    return expect_extension_semantic_reject(
        "IR-FLOAT-GLOBAL-OP-SEMANTIC-REJECT",
        "float g = 1.25;\nint main(){ return g + 1; }\n",
        "SEMA-EXT-035");
}

static int test_ir_rejects_global_float_operator_expression_in_initializer_under_extension(void) {
    return expect_extension_semantic_reject(
        "IR-FLOAT-GLOBAL-OP-INIT-SEMANTIC-REJECT",
        "float g = 1.25;\nint h = g + 1;\nint main(){ return 0; }\n",
        "SEMA-EXT-035");
}

static int test_ir_rejects_global_float_call_result_in_initializer_under_extension(void) {
    return expect_extension_semantic_reject(
        "IR-FLOAT-GLOBAL-CALL-INIT-TYPE-REJECT",
        "float g = 1.25;\nfloat id(float x){ return x; }\nint h = id(g);\nint main(){ return 0; }\n",
        "SEMA-TYPE-004");
}

static int test_ir_accepts_float_assignment_transport_under_extension(void) {
    return expect_extension_ir_dump("IR-FLOAT-ASSIGN-TRANSPORT",
        "float g = 1.25;\n"
        "float id(float x){ return x; }\n"
        "int main(){ float y; y = id(g); return 0; }\n",
        "global g.0:float\n"
        "\n"
        "func __global.init() {\n"
        "  bb.0:\n"
        "    g.0 = mov 1067450368\n"
        "    ret 0\n"
        "}\n"
        "\n"
        "func id(x.0:float) {\n"
        "  bb.0:\n"
        "    ret x.0\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    tmp.1 = call __global.init()\n"
        "    tmp.0 = call id(g.0)\n"
        "    y.0 = mov tmp.0\n"
        "    ret 0\n"
        "}\n");
}

static int test_ir_rejects_float_assignment_to_int_under_extension(void) {
    return expect_extension_semantic_reject(
        "IR-FLOAT-ASSIGN-TYPE-REJECT",
        "float g = 1.25;\nint main(){ int x = 0; x = g; return 0; }\n",
        "SEMA-TYPE-006");
}

static int test_ir_rejects_float_ternary_value_return_to_int_under_extension(void) {
    return expect_extension_semantic_reject(
        "IR-FLOAT-TERNARY-VALUE-RETURN-INT-REJECT",
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "int bad(){ return g ? h : h; }\n"
        "int main(){ return 0; }\n",
        "SEMA-TYPE-005");
}

static int test_ir_rejects_unary_call_ternary_value_return_to_int_under_extension(void) {
    return expect_extension_semantic_reject(
        "IR-FLOAT-UNARY-CALL-TERNARY-RETURN-INT-REJECT",
        "float id(float x){ return x; }\n"
        "int bad(){ return -id(1.0) ? 1.0 : 2.0; }\n"
        "int main(){ return 0; }\n",
        "SEMA-TYPE-005");
}

static int test_ir_rejects_float_if_condition_under_extension(void) {
    return expect_extension_ir_dump("IR-FLOAT-IF-COND-ACCEPT",
        "float g = 1.25;\nint main(){ if(g) return 1; return 0; }\n",
        "global g.0:float\n"
        "\n"
        "func __global.init() {\n"
        "  bb.0:\n"
        "    g.0 = mov 1067450368\n"
        "    ret 0\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    tmp.2 = call __global.init()\n"
        "    tmp.0 = and g.0, 2147483647\n"
        "    tmp.1 = ne tmp.0, 0\n"
        "    br tmp.1, bb.1, bb.2\n"
        "  bb.1:\n"
        "    ret 1\n"
        "  bb.2:\n"
        "    ret 0\n"
        "}\n");
}

static int test_ir_rejects_float_while_condition_under_extension(void) {
    return expect_extension_ir_dump("IR-FLOAT-WHILE-COND-ACCEPT",
        "float g = 1.25;\nint main(){ while(g) return 1; return 0; }\n",
        "global g.0:float\n"
        "\n"
        "func __global.init() {\n"
        "  bb.0:\n"
        "    g.0 = mov 1067450368\n"
        "    ret 0\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    tmp.2 = call __global.init()\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    tmp.0 = and g.0, 2147483647\n"
        "    tmp.1 = ne tmp.0, 0\n"
        "    br tmp.1, bb.2, bb.3\n"
        "  bb.2:\n"
        "    ret 1\n"
        "  bb.3:\n"
        "    ret 0\n"
        "}\n");
}

static int test_ir_rejects_float_for_condition_under_extension(void) {
    return expect_extension_ir_dump("IR-FLOAT-FOR-COND-ACCEPT",
        "float g = 1.25;\nint main(){ for(;g;) return 1; return 0; }\n",
        "global g.0:float\n"
        "\n"
        "func __global.init() {\n"
        "  bb.0:\n"
        "    g.0 = mov 1067450368\n"
        "    ret 0\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    tmp.2 = call __global.init()\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    tmp.0 = and g.0, 2147483647\n"
        "    tmp.1 = ne tmp.0, 0\n"
        "    br tmp.1, bb.2, bb.3\n"
        "  bb.2:\n"
        "    ret 1\n"
        "  bb.3:\n"
        "    ret 0\n"
        "}\n");
}

static int test_ir_accepts_float_logical_condition_composition_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "float g = 1.25;\n"
            "int main(){ if(!g || (g && 1.25)) return g ? 1 : 0; return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "and g.0, 2147483647") != NULL &&
        strstr(actual_text, "and 1067450368, 2147483647") != NULL &&
        strstr(actual_text, "br tmp.") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL &&
        strstr(actual_text, "ret 0") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-LOGICAL-COND-COMPOSE-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_explicit_float_condition_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int main(){ if(float(3)) return 1; return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare __builtin_i2f32(param0.0)\n") != NULL &&
        strstr(actual_text, "tmp.0 = call __builtin_i2f32(3)\n") != NULL &&
        strstr(actual_text, "and tmp.0, 2147483647\n") != NULL &&
        strstr(actual_text, "br tmp.2, bb.1, bb.2\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-CONVERT-COND-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_recursive_explicit_float_condition_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int add3(int a, int b, int c){ return (a + b) + c; }\n"
            "int main(){ if(float(add3(1, 2, 3))) return 1; return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func add3(a.0, b.1, c.2) {\n") != NULL &&
        strstr(actual_text, "tmp.0 = add a.0, b.1\n") != NULL &&
        strstr(actual_text, "tmp.1 = add tmp.0, c.2\n") != NULL &&
        strstr(actual_text, "declare __builtin_i2f32(param0.0)\n") != NULL &&
        strstr(actual_text, "call add3(1, 2, 3)\n") != NULL &&
        strstr(actual_text, "call __builtin_i2f32(") != NULL &&
        strstr(actual_text, "2147483647\n") != NULL &&
        strstr(actual_text, "br tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-CONVERT-RECURSIVE-COND-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_explicit_float_while_condition_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int main(){ while(float(3)) return 1; return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare __builtin_i2f32(param0.0)\n") != NULL &&
        strstr(actual_text, "jmp bb.1\n") != NULL &&
        strstr(actual_text, "call __builtin_i2f32(") != NULL &&
        strstr(actual_text, "2147483647\n") != NULL &&
        strstr(actual_text, "br tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-CONVERT-WHILE-COND-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_explicit_float_logical_condition_composition_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int add3(int a, int b, int c){ return (a + b) + c; }\n"
            "int main(){ if(!float(0) || (float(3) && float(add3(1, 2, 3)))) return 1; return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare __builtin_i2f32(param0.0)\n") != NULL &&
        strstr(actual_text, "call __builtin_i2f32(0)\n") != NULL &&
        strstr(actual_text, "call __builtin_i2f32(3)\n") != NULL &&
        strstr(actual_text, "call add3(1, 2, 3)\n") != NULL &&
        strstr(actual_text, "2147483647") != NULL &&
        strstr(actual_text, "br tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-CONVERT-LOGIC-COND-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_recursive_float_if_condition_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int f(float x, float y, float z){ if((x + y) + z) return 1; return 0; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func f(x.0:float, y.1:float, z.2:float) {\n") != NULL &&
        strstr(actual_text, "call __builtin_fadd32(x.0, y.1)\n") != NULL &&
        strstr(actual_text, "call __builtin_fadd32(tmp.0, z.2)\n") != NULL &&
        strstr(actual_text, "and tmp.1, 2147483647\n") != NULL &&
        strstr(actual_text, "ne tmp.2, 0\n") != NULL &&
        strstr(actual_text, "br tmp.3, bb.1, bb.2\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-RECURSIVE-IF-COND-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_recursive_float_while_condition_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int f(float a, float b, float c){ while(-a * (b / c)) return 1; return 0; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func f(a.0:float, b.1:float, c.2:float) {\n") != NULL &&
        strstr(actual_text, "tmp.0 = xor a.0, 2147483648\n") != NULL &&
        strstr(actual_text, "call __builtin_fdiv32(b.1, c.2)\n") != NULL &&
        strstr(actual_text, "call __builtin_fmul32(tmp.0, tmp.1)\n") != NULL &&
        strstr(actual_text, "and tmp.2, 2147483647\n") != NULL &&
        strstr(actual_text, "br tmp.4, bb.2, bb.3\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-RECURSIVE-WHILE-COND-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_recursive_float_for_condition_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int f(float x, float y, float z){ for(;(x + y) + z;) return 1; return 0; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func f(x.0:float, y.1:float, z.2:float) {\n") != NULL &&
        strstr(actual_text, "jmp bb.1\n") != NULL &&
        strstr(actual_text, "call __builtin_fadd32(x.0, y.1)\n") != NULL &&
        strstr(actual_text, "call __builtin_fadd32(tmp.0, z.2)\n") != NULL &&
        strstr(actual_text, "and tmp.1, 2147483647\n") != NULL &&
        strstr(actual_text, "br tmp.3, bb.2, bb.3\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-RECURSIVE-FOR-COND-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_rejects_recursive_float_condition_with_ternary_neighbor_under_extension(void) {
    return expect_extension_semantic_reject(
        "IR-FLOAT-RECURSIVE-COND-TERNARY-NEIGHBOR-REJECT",
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "int main(){ if((g ? h : h) + h) return 1; return 0; }\n",
        "SEMA-EXT-035");
}

static int test_ir_rejects_float_ternary_value_under_extension(void) {
    return expect_extension_semantic_reject(
        "IR-FLOAT-TERNARY-VALUE-SEMANTIC-REJECT",
        "float g = 1.25;\nfloat h = 1.25;\nint main(){ return g ? h : 0; }\n",
        "SEMA-EXT-035");
}

static int test_ir_accepts_same_type_float_ternary_value_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "float g = 1.25;\n"
            "float h = 1.25;\n"
            "float mainf(){ return g ? h : h; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func mainf() {\n") != NULL &&
        strstr(actual_text, "br tmp.") != NULL &&
        strstr(actual_text, "mov h.1") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-TERNARY-VALUE-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_float_ternary_value_assignment_to_float_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "float g = 1.25;\n"
            "float h = 2.5;\n"
            "int main(){ float y; y = (g ? h : h); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func main() {\n") != NULL &&
        strstr(actual_text, "br tmp.") != NULL &&
        strstr(actual_text, "y.0 = mov tmp.") != NULL &&
        strstr(actual_text, "ret 0\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-TERNARY-VALUE-ASSIGN-FLOAT-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_float_ternary_value_initializer_to_float_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "float g = 1.25;\n"
            "float h = 2.5;\n"
            "float y = (g ? h : h);\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "global y.2:float\n") != NULL &&
        strstr(actual_text, "func __global.init() {\n") != NULL &&
        strstr(actual_text, "br tmp.") != NULL &&
        strstr(actual_text, "y.2 = mov tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-TERNARY-VALUE-INIT-FLOAT-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_chained_float_addition_assignment_to_float_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "float f(float x, float y, float z){ float t; t = (x + y) + z; return t; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func f(x.0:float, y.1:float, z.2:float) {\n") != NULL &&
        strstr(actual_text, "call __builtin_fadd32(") != NULL &&
        strstr(actual_text, "t.3 = mov tmp.") != NULL &&
        strstr(actual_text, "ret t.3\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-CHAIN-ADD-ASSIGN-FLOAT-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_nested_float_mul_div_assignment_to_float_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "float f(float a, float b, float c){ float t; t = -a * (b / c); return t; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func f(a.0:float, b.1:float, c.2:float) {\n") != NULL &&
        strstr(actual_text, "call __builtin_fdiv32(") != NULL &&
        strstr(actual_text, "call __builtin_fmul32(") != NULL &&
        strstr(actual_text, "t.3 = mov tmp.") != NULL &&
        strstr(actual_text, "ret t.3\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-NESTED-MUL-DIV-ASSIGN-FLOAT-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_chained_float_addition_initializer_to_float_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "float f(float x, float y, float z){ float t = (x + y) + z; return t; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func f(x.0:float, y.1:float, z.2:float) {\n") != NULL &&
        strstr(actual_text, "call __builtin_fadd32(") != NULL &&
        strstr(actual_text, "t.3 = mov tmp.") != NULL &&
        strstr(actual_text, "ret t.3\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-CHAIN-ADD-INIT-FLOAT-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_nested_float_mul_div_initializer_to_float_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "float f(float a, float b, float c){ float t = -a * (b / c); return t; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func f(a.0:float, b.1:float, c.2:float) {\n") != NULL &&
        strstr(actual_text, "call __builtin_fdiv32(") != NULL &&
        strstr(actual_text, "call __builtin_fmul32(") != NULL &&
        strstr(actual_text, "t.3 = mov tmp.") != NULL &&
        strstr(actual_text, "ret t.3\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-NESTED-MUL-DIV-INIT-FLOAT-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_float_ternary_value_call_argument_to_float_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "float g = 1.25;\n"
            "float h = 2.5;\n"
            "float id(float x){ return x; }\n"
            "float wrap(float x){ return id(x); }\n"
            "float get(){ return wrap((g ? h : h)); }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func get() {\n") != NULL &&
        strstr(actual_text, "br tmp.") != NULL &&
        strstr(actual_text, "tmp.3 = call wrap(tmp.0)\n") != NULL &&
        strstr(actual_text, "ret tmp.3\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-TERNARY-VALUE-CALLARG-FLOAT-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_unary_call_float_ternary_value_call_argument_to_float_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "float id(float x){ return x; }\n"
            "float wrap(float x){ return id(x); }\n"
            "float get(){ return wrap((-id(1.0) ? 1.0 : 2.0)); }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "tmp.1 = call id(1065353216)\n") != NULL &&
        strstr(actual_text, "tmp.2 = xor tmp.1, 2147483648\n") != NULL &&
        strstr(actual_text, "br tmp.") != NULL &&
        strstr(actual_text, "tmp.5 = call wrap(tmp.0)\n") != NULL &&
        strstr(actual_text, "ret tmp.5\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-UNARY-CALL-TERNARY-CALLARG-FLOAT-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_chained_float_addition_call_argument_to_float_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "float wrap(float x){ return x; }\n"
            "float get(float x, float y, float z){ return wrap((x + y) + z); }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func wrap(x.0:float) {\n") != NULL &&
        strstr(actual_text, "func get(x.0:float, y.1:float, z.2:float) {\n") != NULL &&
        strstr(actual_text, "call __builtin_fadd32(") != NULL &&
        strstr(actual_text, "tmp.2 = call wrap(tmp.1)\n") != NULL &&
        strstr(actual_text, "ret tmp.2\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-CHAIN-ADD-CALLARG-FLOAT-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_nested_float_mul_div_call_argument_to_float_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "float wrap(float x){ return x; }\n"
            "float f(float a, float b, float c){ return wrap(-a * (b / c)); }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func wrap(x.0:float) {\n") != NULL &&
        strstr(actual_text, "func f(a.0:float, b.1:float, c.2:float) {\n") != NULL &&
        strstr(actual_text, "call __builtin_fdiv32(") != NULL &&
        strstr(actual_text, "call __builtin_fmul32(") != NULL &&
        strstr(actual_text, "tmp.3 = call wrap(tmp.2)\n") != NULL &&
        strstr(actual_text, "ret tmp.3\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-NESTED-MUL-DIV-CALLARG-FLOAT-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_float_helper_wrapped_ternary_call_arithmetic_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "float g = 1.25;\n"
            "float h = 2.5;\n"
            "float pick(){ return g ? h : h; }\n"
            "float get(){ return pick() + h; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func pick() {\n") != NULL &&
        strstr(actual_text, "func get() {\n") != NULL &&
        strstr(actual_text, "tmp.0 = call pick()\n") != NULL &&
        strstr(actual_text, "tmp.1 = call __builtin_fadd32(tmp.0, h.1)\n") != NULL &&
        strstr(actual_text, "ret tmp.1\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-HELPER-TERNARY-CALL-ARITH-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_unary_call_helper_wrapped_ternary_call_arithmetic_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "float id(float x){ return x; }\n"
            "float pick(float x){ return -id(x) ? x : x; }\n"
            "float f(float x){ return pick(x) + x; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func pick(x.0:float) {\n") != NULL &&
        strstr(actual_text, "tmp.0 = call pick(x.0)\n") != NULL &&
        strstr(actual_text, "tmp.1 = call __builtin_fadd32(tmp.0, x.0)\n") != NULL &&
        strstr(actual_text, "ret tmp.1\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-UNARY-HELPER-TERNARY-CALL-ARITH-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_float_helper_wrapped_ternary_call_compare_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "float g = 1.25;\n"
            "float h = 2.5;\n"
            "float pick(){ return g ? h : h; }\n"
            "int eq(){ return pick() == h; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func pick() {\n") != NULL &&
        strstr(actual_text, "func eq() {\n") != NULL &&
        strstr(actual_text, "tmp.0 = call pick()\n") != NULL &&
        strstr(actual_text, "tmp.1 = eq tmp.4, tmp.7\n") != NULL &&
        strstr(actual_text, "ret tmp.1\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-HELPER-TERNARY-CALL-COMPARE-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_unary_call_helper_wrapped_ternary_call_compare_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "float id(float x){ return x; }\n"
            "float pick(float x){ return -id(x) ? x : x; }\n"
            "int eq(float x){ return pick(x) == x; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func pick(x.0:float) {\n") != NULL &&
        strstr(actual_text, "func eq(x.0:float) {\n") != NULL &&
        strstr(actual_text, "tmp.0 = call pick(x.0)\n") != NULL &&
        strstr(actual_text, "tmp.1 = eq tmp.4, tmp.7\n") != NULL &&
        strstr(actual_text, "ret tmp.1\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-UNARY-HELPER-TERNARY-CALL-COMPARE-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_float_equality_compare_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int eq(float x, float y){ return x == y; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func eq(x.0:float, y.1:float) {\n") != NULL &&
        strstr(actual_text, "and x.0, 2147483647\n") != NULL &&
        strstr(actual_text, "and y.1, 2147483647\n") != NULL &&
        strstr(actual_text, "mul x.0, tmp.") != NULL &&
        strstr(actual_text, "mul y.1, tmp.") != NULL &&
        strstr(actual_text, "eq tmp.") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-EQ-COMPARE-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_float_inequality_compare_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int ne(float x, float y){ return x != y; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func ne(x.0:float, y.1:float) {\n") != NULL &&
        strstr(actual_text, "and x.0, 2147483647\n") != NULL &&
        strstr(actual_text, "and y.1, 2147483647\n") != NULL &&
        strstr(actual_text, "ne tmp.") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-NE-COMPARE-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_float_relational_compare_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int lt(float x, float y){ return x < y; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func lt(x.0:float, y.1:float) {\n") != NULL &&
        strstr(actual_text, "and x.0, 2147483647\n") != NULL &&
        strstr(actual_text, "mul x.0, tmp.") != NULL &&
        strstr(actual_text, "shr tmp.") != NULL &&
        strstr(actual_text, "and y.1, 2147483647\n") != NULL &&
        strstr(actual_text, "mul y.1, tmp.") != NULL &&
        strstr(actual_text, "or tmp.") != NULL &&
        strstr(actual_text, "xor tmp.") != NULL &&
        strstr(actual_text, "lt tmp.") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-LT-COMPARE-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_chained_float_equality_compare_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int eq(float x, float y, float z){ return ((x + y) + z) == z; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func eq(x.0:float, y.1:float, z.2:float) {\n") != NULL &&
        strstr(actual_text, "call __builtin_fadd32(") != NULL &&
        strstr(actual_text, "eq tmp.") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-CHAIN-ADD-EQ-COMPARE-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_chained_float_relational_compare_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int lt(float x, float y, float z){ return ((x + y) + z) < z; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func lt(x.0:float, y.1:float, z.2:float) {\n") != NULL &&
        strstr(actual_text, "call __builtin_fadd32(") != NULL &&
        strstr(actual_text, "lt tmp.") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-CHAIN-ADD-LT-COMPARE-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_nested_muldiv_float_equality_compare_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int eq(float a, float b, float c){ return (-a * (b / c)) == c; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func eq(a.0:float, b.1:float, c.2:float) {\n") != NULL &&
        strstr(actual_text, "call __builtin_fdiv32(") != NULL &&
        strstr(actual_text, "call __builtin_fmul32(") != NULL &&
        strstr(actual_text, "eq tmp.") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-NESTED-MUL-DIV-EQ-COMPARE-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_nested_muldiv_float_relational_compare_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int lt(float a, float b, float c){ return (-a * (b / c)) < c; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func lt(a.0:float, b.1:float, c.2:float) {\n") != NULL &&
        strstr(actual_text, "call __builtin_fdiv32(") != NULL &&
        strstr(actual_text, "call __builtin_fmul32(") != NULL &&
        strstr(actual_text, "lt tmp.") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-NESTED-MUL-DIV-LT-COMPARE-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_chained_float_inequality_compare_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int ne(float x, float y, float z){ return ((x + y) + z) != z; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func ne(x.0:float, y.1:float, z.2:float) {\n") != NULL &&
        strstr(actual_text, "call __builtin_fadd32(") != NULL &&
        strstr(actual_text, "ne tmp.") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-CHAIN-ADD-NE-COMPARE-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_chained_float_le_compare_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int le(float x, float y, float z){ return ((x + y) + z) <= z; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func le(x.0:float, y.1:float, z.2:float) {\n") != NULL &&
        strstr(actual_text, "call __builtin_fadd32(") != NULL &&
        strstr(actual_text, "le tmp.") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-CHAIN-ADD-LE-COMPARE-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_nested_muldiv_float_inequality_compare_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int ne(float a, float b, float c){ return (-a * (b / c)) != c; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func ne(a.0:float, b.1:float, c.2:float) {\n") != NULL &&
        strstr(actual_text, "call __builtin_fdiv32(") != NULL &&
        strstr(actual_text, "call __builtin_fmul32(") != NULL &&
        strstr(actual_text, "ne tmp.") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-NESTED-MUL-DIV-NE-COMPARE-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_nested_muldiv_float_ge_compare_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int ge(float a, float b, float c){ return (-a * (b / c)) >= c; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func ge(a.0:float, b.1:float, c.2:float) {\n") != NULL &&
        strstr(actual_text, "call __builtin_fdiv32(") != NULL &&
        strstr(actual_text, "call __builtin_fmul32(") != NULL &&
        strstr(actual_text, "ge tmp.") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-NESTED-MUL-DIV-GE-COMPARE-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_negative_float_literal_transport_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "float neg(){ return -1.25; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func neg() {\n") != NULL &&
        strstr(actual_text, "ret 3214934016\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-NEG-LITERAL-TRANSPORT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_unary_minus_float_identifier_transport_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "float g = 1.25;\n"
            "float neg(){ return -g; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func neg() {\n") != NULL &&
        strstr(actual_text, "tmp.0 = xor g.0, 2147483648\n") != NULL &&
        strstr(actual_text, "ret tmp.0\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-NEG-IDENT-TRANSPORT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_negative_zero_float_condition_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int main(){ if(-0.0) return 1; return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "and 2147483648, 2147483647\n") != NULL &&
        strstr(actual_text, "ne tmp.") != NULL &&
        strstr(actual_text, "br tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-NEG-ZERO-COND-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_negative_float_relational_compare_against_zero_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int lt(){ return -1.25 < 0.0; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func lt() {\n") != NULL &&
        strstr(actual_text, "and 3214934016, 2147483647\n") != NULL &&
        strstr(actual_text, "ne tmp.") != NULL &&
        strstr(actual_text, "mul 3214934016, tmp.") != NULL &&
        strstr(actual_text, "shr tmp.") != NULL &&
        strstr(actual_text, "or tmp.") != NULL &&
        strstr(actual_text, "xor tmp.") != NULL &&
        strstr(actual_text, "lt tmp.") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-NEG-LT-ZERO-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_negative_zero_le_zero_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int le(){ return -0.0 <= 0.0; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func le() {\n") != NULL &&
        strstr(actual_text, "and 2147483648, 2147483647\n") != NULL &&
        strstr(actual_text, "ne tmp.") != NULL &&
        strstr(actual_text, "mul 2147483648, tmp.") != NULL &&
        strstr(actual_text, "shr tmp.") != NULL &&
        strstr(actual_text, "or tmp.") != NULL &&
        strstr(actual_text, "xor tmp.") != NULL &&
        strstr(actual_text, "le tmp.") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-NEG-ZERO-LE-ZERO-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_float_addition_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "float add(float x, float y){ return x + y; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func add(x.0:float, y.1:float) {\n") != NULL &&
        strstr(actual_text, "call __builtin_fadd32(x.0, y.1)\n") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL &&
        strstr(actual_text, "declare __builtin_fadd32(param0.0:float, param1.1:float)\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-ADD-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_float_subtraction_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "float sub(float x, float y){ return x - y; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func sub(x.0:float, y.1:float) {\n") != NULL &&
        strstr(actual_text, "call __builtin_fsub32(x.0, y.1)\n") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL &&
        strstr(actual_text, "declare __builtin_fsub32(param0.0:float, param1.1:float)\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-SUB-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_explicit_int_from_float_conversion_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int conv(float x, float y){ return int(x + y); }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare __builtin_fadd32(param0.0:float, param1.1:float)\n") != NULL &&
        strstr(actual_text, "declare __builtin_f2i32(param0.0:float)\n") != NULL &&
        strstr(actual_text, "tmp.0 = call __builtin_fadd32(x.0, y.1)\n") != NULL &&
        strstr(actual_text, "tmp.1 = call __builtin_f2i32(tmp.0)\n") != NULL &&
        strstr(actual_text, "ret tmp.1") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-EXPLICIT-INT-FROM-FLOAT-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_explicit_float_from_int_conversion_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "float conv(int x, int y){ return float(x + y); }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare __builtin_i2f32(param0.0)\n") != NULL &&
        strstr(actual_text, "tmp.0 = add x.0, y.1\n") != NULL &&
        strstr(actual_text, "tmp.1 = call __builtin_i2f32(tmp.0)\n") != NULL &&
        strstr(actual_text, "ret tmp.1") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-EXPLICIT-FLOAT-FROM-INT-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_explicit_int_from_float_ternary_bridge_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "float g = 1.25;\n"
            "float h = 2.5;\n"
            "int main(){ return int(g ? h : h); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare __builtin_f2i32(param0.0:float)\n") != NULL &&
        strstr(actual_text, "func main() {\n") != NULL &&
        strstr(actual_text, "call __builtin_f2i32(") != NULL &&
        strstr(actual_text, "br ") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-EXPLICIT-INT-FROM-FLOAT-TERNARY-BRIDGE-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_explicit_int_from_recursive_float_call_argument_bridge_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int sink(int x){ return x; }\n"
            "float add3(float a, float b, float c){ return (a + b) + c; }\n"
            "int main(){ return sink(int(add3(1.0, 2.0, 3.0))); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare __builtin_fadd32(param0.0:float, param1.1:float)\n") != NULL &&
        strstr(actual_text, "declare __builtin_f2i32(param0.0:float)\n") != NULL &&
        strstr(actual_text, "call add3(") != NULL &&
        strstr(actual_text, "call __builtin_f2i32(") != NULL &&
        strstr(actual_text, "call sink(") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-EXPLICIT-INT-FROM-RECURSIVE-CALLARG-BRIDGE-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_explicit_int_from_recursive_float_initializer_bridge_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "float add3(float a, float b, float c){ return (a + b) + c; }\n"
            "int x = int(add3(1.0, 2.0, 3.0));\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "global x.0\n") != NULL &&
        strstr(actual_text, "func __global.init() {\n") != NULL &&
        strstr(actual_text, "call add3(") != NULL &&
        strstr(actual_text, "call __builtin_f2i32(") != NULL &&
        strstr(actual_text, "x.0 = mov tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-EXPLICIT-INT-FROM-RECURSIVE-INIT-BRIDGE-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_explicit_int_from_recursive_float_assignment_bridge_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "float add3(float a, float b, float c){ return (a + b) + c; }\n"
            "int main(){ int x=0; x = int(add3(1.0, 2.0, 3.0)); return x; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call add3(") != NULL &&
        strstr(actual_text, "call __builtin_f2i32(") != NULL &&
        strstr(actual_text, "x.0 = mov tmp.") != NULL &&
        strstr(actual_text, "ret x.0") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-EXPLICIT-INT-FROM-RECURSIVE-ASSIGN-BRIDGE-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_explicit_int_from_float_compare_bridge_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "float g = 1.25;\n"
            "float h = 2.5;\n"
            "int main(){ return int(g ? h : h) == 2; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call __builtin_f2i32(") != NULL &&
        strstr(actual_text, " = eq tmp.") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-EXPLICIT-INT-FROM-FLOAT-COMPARE-BRIDGE-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_explicit_int_from_recursive_float_arithmetic_bridge_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "float add3(float a, float b, float c){ return (a + b) + c; }\n"
            "int main(){ return int(add3(1.0, 2.0, 3.0)) + 1; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call add3(") != NULL &&
        strstr(actual_text, "call __builtin_f2i32(") != NULL &&
        strstr(actual_text, " = add tmp.") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-EXPLICIT-INT-FROM-RECURSIVE-ARITH-BRIDGE-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_explicit_float_from_recursive_int_initializer_bridge_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int add3(int a, int b, int c){ return (a + b) + c; }\n"
            "float z = float(add3(1, 2, 3));\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "global z.0:float\n") != NULL &&
        strstr(actual_text, "func __global.init() {\n") != NULL &&
        strstr(actual_text, "call add3(1, 2, 3)\n") != NULL &&
        strstr(actual_text, "call __builtin_i2f32(") != NULL &&
        strstr(actual_text, "z.0 = mov tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-EXPLICIT-FLOAT-FROM-RECURSIVE-INIT-BRIDGE-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_explicit_float_from_recursive_int_assignment_bridge_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int add3(int a, int b, int c){ return (a + b) + c; }\n"
            "float mainf(){ float y; y = float(add3(1, 2, 3)); return y; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call add3(1, 2, 3)\n") != NULL &&
        strstr(actual_text, "call __builtin_i2f32(") != NULL &&
        strstr(actual_text, "y.0 = mov tmp.") != NULL &&
        strstr(actual_text, "ret y.0") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-EXPLICIT-FLOAT-FROM-RECURSIVE-ASSIGN-BRIDGE-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_explicit_float_from_int_compare_bridge_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int add3(int a, int b, int c){ return (a + b) + c; }\n"
            "int main(){ return float(add3(1, 2, 3)) == float(6); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call __builtin_i2f32(") != NULL &&
        strstr(actual_text, " = eq tmp.") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-EXPLICIT-FLOAT-FROM-INT-COMPARE-BRIDGE-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_explicit_float_from_recursive_int_arithmetic_bridge_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int add3(int a, int b, int c){ return (a + b) + c; }\n"
            "float mainf(){ return float(add3(1, 2, 3)) + 1.25; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call __builtin_i2f32(") != NULL &&
        strstr(actual_text, " = add tmp.1, 1067450368") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-EXPLICIT-FLOAT-FROM-RECURSIVE-ARITH-BRIDGE-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_negative_float_addition_combo_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "float g = 1.25;\n"
            "float add(float y){ return -g + y; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func add(y.0:float) {\n") != NULL &&
        strstr(actual_text, "tmp.0 = xor g.0, 2147483648\n") != NULL &&
        strstr(actual_text, "call __builtin_fadd32(tmp.0, y.0)\n") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-NEG-ADD-COMBO-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_negative_float_subtraction_combo_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "float g = 1.25;\n"
            "float sub(float y){ return y - -g; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func sub(y.0:float) {\n") != NULL &&
        strstr(actual_text, "tmp.0 = xor g.0, 2147483648\n") != NULL &&
        strstr(actual_text, "call __builtin_fsub32(y.0, tmp.0)\n") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-NEG-SUB-COMBO-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_float_multiplication_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "float mul(float x, float y){ return x * y; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare __builtin_fmul32(param0.0:float, param1.1:float)\n") != NULL &&
        strstr(actual_text, "func mul(x.0:float, y.1:float) {\n") != NULL &&
        strstr(actual_text, "call __builtin_fmul32(x.0, y.1)\n") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-MUL-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_float_division_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "float divv(float x, float y){ return x / y; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare __builtin_fdiv32(param0.0:float, param1.1:float)\n") != NULL &&
        strstr(actual_text, "func divv(x.0:float, y.1:float) {\n") != NULL &&
        strstr(actual_text, "call __builtin_fdiv32(x.0, y.1)\n") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-DIV-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_negative_float_multiplication_combo_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "float g = 1.25;\n"
            "float mul(float y){ return -g * y; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func mul(y.0:float) {\n") != NULL &&
        strstr(actual_text, "tmp.0 = xor g.0, 2147483648\n") != NULL &&
        strstr(actual_text, "call __builtin_fmul32(tmp.0, y.0)\n") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-NEG-MUL-COMBO-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_negative_float_division_combo_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "float g = 1.25;\n"
            "float divv(float y){ return y / -g; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func divv(y.0:float) {\n") != NULL &&
        strstr(actual_text, "tmp.0 = xor g.0, 2147483648\n") != NULL &&
        strstr(actual_text, "call __builtin_fdiv32(y.0, tmp.0)\n") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-NEG-DIV-COMBO-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_chained_float_addition_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "float add3(float x, float y, float z){ return (x + y) + z; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func add3(x.0:float, y.1:float, z.2:float) {\n") != NULL &&
        strstr(actual_text, "tmp.0 = call __builtin_fadd32(x.0, y.1)\n") != NULL &&
        strstr(actual_text, "tmp.1 = call __builtin_fadd32(tmp.0, z.2)\n") != NULL &&
        strstr(actual_text, "ret tmp.1") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-CHAIN-ADD-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_nested_float_mul_div_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "float f(float a, float b, float c){ return -a * (b / c); }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func f(a.0:float, b.1:float, c.2:float) {\n") != NULL &&
        strstr(actual_text, "tmp.0 = xor a.0, 2147483648\n") != NULL &&
        strstr(actual_text, "tmp.1 = call __builtin_fdiv32(b.1, c.2)\n") != NULL &&
        strstr(actual_text, "tmp.2 = call __builtin_fmul32(tmp.0, tmp.1)\n") != NULL &&
        strstr(actual_text, "ret tmp.2") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: IR-FLOAT-NESTED-MUL-DIV-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_rejects_mixed_float_int_arithmetic_under_extension(void) {
    return expect_extension_semantic_reject(
        "IR-FLOAT-ARITH-INT-TYPE-REJECT",
        "float add(float x){ return x + 1; }\n"
        "int main(){ return 0; }\n",
        "SEMA-TYPE-008");
}

static int test_ir_rejects_float_literal_int_arithmetic_under_extension(void) {
    return expect_extension_semantic_reject(
        "IR-FLOAT-LITERAL-ARITH-INT-TYPE-REJECT",
        "float add(){ return 1.25 + 1; }\n"
        "int main(){ return 0; }\n",
        "SEMA-TYPE-008");
}

static int test_ir_rejects_float_call_int_arithmetic_under_extension(void) {
    return expect_extension_semantic_reject(
        "IR-FLOAT-CALL-ARITH-INT-TYPE-REJECT",
        "float id(float x){ return x; }\n"
        "float add(float x){ return id(x) + 1; }\n"
        "int main(){ return 0; }\n",
        "SEMA-TYPE-008");
}

static int test_ir_rejects_negative_float_call_int_arithmetic_under_extension(void) {
    return expect_extension_semantic_reject(
        "IR-FLOAT-NEG-CALL-ARITH-INT-TYPE-REJECT",
        "float id(float x){ return x; }\n"
        "float add(float x){ return -id(x) * 1; }\n"
        "int main(){ return 0; }\n",
        "SEMA-TYPE-008");
}

static int test_ir_rejects_global_float_int_condition_under_extension(void) {
    return expect_extension_semantic_reject(
        "IR-FLOAT-GLOBAL-COND-PLUS-INT-REJECT",
        "float g = 1.25;\n"
        "int main(){ if(g + 1) return 1; return 0; }\n",
        "SEMA-EXT-035");
}

static int test_ir_rejects_float_call_int_condition_under_extension(void) {
    return expect_extension_semantic_reject(
        "IR-FLOAT-CALL-COND-PLUS-INT-REJECT",
        "float id(float x){ return x; }\n"
        "int main(){ if(id(1.0) + 1) return 1; return 0; }\n",
        "SEMA-TYPE-008");
}

static int test_ir_rejects_negative_float_call_int_condition_under_extension(void) {
    return expect_extension_semantic_reject(
        "IR-FLOAT-NEG-CALL-COND-PLUS-INT-REJECT",
        "float id(float x){ return x; }\n"
        "int main(){ if(-id(1.0) + 1) return 1; return 0; }\n",
        "SEMA-TYPE-008");
}

static int test_ir_rejects_nested_float_tree_plus_int_condition_under_extension(void) {
    return expect_extension_semantic_reject(
        "IR-FLOAT-NESTED-TREE-COND-PLUS-INT-REJECT",
        "int main(float x, float y, float z){ if(((x + y) + z) + 1) return 1; return 0; }\n",
        "SEMA-EXT-035");
}

static int test_ir_rejects_nested_float_muldiv_plus_int_condition_under_extension(void) {
    return expect_extension_semantic_reject(
        "IR-FLOAT-NESTED-MULDIV-COND-PLUS-INT-REJECT",
        "int main(float a, float b, float c){ if((-a * (b / c)) + 1) return 1; return 0; }\n",
        "SEMA-EXT-035");
}

static int test_ir_rejects_float_compare_against_int_under_extension(void) {
    return expect_extension_semantic_reject(
        "IR-FLOAT-COMPARE-INT-TYPE-REJECT",
        "float g = 1.25;\nint main(){ return g == 1; }\n",
        "SEMA-TYPE-007");
}

static int test_ir_rejects_nested_float_tree_plus_int_under_extension(void) {
    return expect_extension_semantic_reject(
        "IR-FLOAT-NESTED-TREE-PLUS-INT-REJECT",
        "float add(float x, float y){ return (x + y) + 1; }\n"
        "int main(){ return 0; }\n",
        "SEMA-EXT-035");
}

static int test_ir_rejects_nested_float_muldiv_plus_int_under_extension(void) {
    return expect_extension_semantic_reject(
        "IR-FLOAT-NESTED-MULDIV-PLUS-INT-REJECT",
        "float f(float a, float b, float c){ return (-a * (b / c)) + 1; }\n"
        "int main(){ return 0; }\n",
        "SEMA-EXT-035");
}

static int test_ir_rejects_float_ternary_value_plus_int_under_extension(void) {
    return expect_extension_semantic_reject(
        "IR-FLOAT-TERNARY-VALUE-PLUS-INT-REJECT",
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "int main(){ return (g ? h : h) + 1; }\n",
        "SEMA-EXT-035");
}

static int test_ir_rejects_unary_call_ternary_value_plus_int_under_extension(void) {
    return expect_extension_semantic_reject(
        "IR-FLOAT-UNARY-CALL-TERNARY-PLUS-INT-REJECT",
        "float id(float x){ return x; }\n"
        "float add(float x){ return (-id(x) ? x : x) + 1; }\n"
        "int main(){ return 0; }\n",
        "SEMA-EXT-035");
}

static int test_ir_rejects_float_ternary_value_plus_float_call_argument_under_extension(void) {
    return expect_extension_semantic_reject(
        "IR-FLOAT-TERNARY-PLUS-FLOAT-CALLARG-REJECT",
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "float wrap(float x){ return x; }\n"
        "float get(){ return wrap((g ? h : h) + h); }\n"
        "int main(){ return 0; }\n",
        "SEMA-EXT-035");
}

static int test_ir_rejects_unary_call_ternary_value_plus_float_call_argument_under_extension(void) {
    return expect_extension_semantic_reject(
        "IR-FLOAT-UNARY-CALL-TERNARY-PLUS-FLOAT-CALLARG-REJECT",
        "float id(float x){ return x; }\n"
        "float wrap(float x){ return x; }\n"
        "float f(float x){ return wrap(((-id(x) ? x : x)) + x); }\n"
        "int main(){ return 0; }\n",
        "SEMA-EXT-035");
}

static int test_ir_accepts_dynamic_noncapturing_function_value_return_bind_and_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int add2(int x){ return x+2; }\n"
            "int pick(int c)(int) { int f(int)=add1; if(c) f=add2; return f; }\n"
            "int main(){ int g(int)=pick(1); return g(40); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "shape shape.0(int) -> int\n") != NULL &&
        strstr(actual_text, "func pick(__ret0.0, c.1) {\n") != NULL &&
        strstr(actual_text, "f$ftag.2 = mov 1\n") != NULL &&
        strstr(actual_text, "f$ftag.2 = mov 2\n") != NULL &&
        strstr(actual_text, "store_indirect __ret0.0, f$ftag.2\n") != NULL &&
        strstr(actual_text, "tmp.0 = addr_local") != NULL &&
        strstr(actual_text, "tmp.1 = call pick(tmp.0, 1)\n") != NULL &&
        strstr(actual_text, "eq g$ftag.0, 1\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add2, 0, shape.0\n") != NULL &&
        strstr(actual_text, "func __fnwrap_add1(__env.0, x.1) {\n") != NULL &&
        strstr(actual_text, "func __fnwrap_add2(__env.0, x.1) {\n") != NULL &&
        strstr(actual_text, "__retfn_declslot") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic noncapturing function-value return bind-and-call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_static_noncapturing_top_level_function_value_return_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int pick()(int) { return add1; }\n"
            "int main(){ int f(int)=pick(); return f(41); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func pick(__ret0.0) {\n") != NULL &&
        strstr(actual_text, "store_indirect __ret0.0, 1\n") != NULL &&
        strstr(actual_text, "tmp.0 = addr_local") != NULL &&
        strstr(actual_text, "tmp.1 = call pick(tmp.0)\n") != NULL &&
        strstr(actual_text, "shape shape.0(int) -> int\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.2(41)\n") != NULL &&
        strstr(actual_text, "func __fnwrap_add1(__env.0, x.1) {\n") != NULL &&
        strstr(actual_text, "__retfn_declslot") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: static top-level noncapturing function-value return mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_noncapturing_function_value_return_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_source_to_ir_text_with_options(
            "int add1(int x){ return x+1; }\n"
            "int add2(int x){ return x+2; }\n"
            "int pick(int c)(int) { int f(int)=add1; if(c) f=add2; return f; }\n"
            "int main(){ return pick(1)(40); }\n",
            1,
            1,
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "shape shape.0(int) -> int\n") != NULL &&
        strstr(actual_text, "func pick(__ret0.0, c.1) {\n") != NULL &&
        strstr(actual_text, "store_indirect __ret0.0, f$ftag.2\n") != NULL &&
        strstr(actual_text, "tmp.0 = addr_local local.0\n") != NULL &&
        strstr(actual_text, "tmp.1 = call pick(tmp.0, 1)\n") != NULL &&
        strstr(actual_text, "tmp.2 = eq __retfn_immediate_0.0, 1\n") != NULL &&
        strstr(actual_text, "tmp.4 = fn_make __fnwrap_add1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "tmp.3 = call_indirect tmp.4(40)\n") != NULL &&
        strstr(actual_text, "tmp.5 = fn_make __fnwrap_add2, 0, shape.0\n") != NULL &&
        strstr(actual_text, "func __fnwrap_add1(__env.0, x.1) {\n") != NULL &&
        strstr(actual_text, "func __fnwrap_add2(__env.0, x.1) {\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic noncapturing function-value return immediate-call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_noncapturing_two_arg_function_value_return_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_source_to_ir_text_with_options(
            "int add(int x,int y){ return x+y; }\n"
            "int sub(int x,int y){ return x-y; }\n"
            "int pick(int c)(int,int) { int f(int,int)=add; if(c) f=sub; return f; }\n"
            "int main(){ return pick(1)(9, 4); }\n",
            1,
            1,
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "shape shape.0(int, int) -> int\n") != NULL &&
        strstr(actual_text, "func pick(__ret0.0, c.1) {\n") != NULL &&
        strstr(actual_text, "store_indirect __ret0.0, f$ftag.2\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 1u &&
        strstr(actual_text, "eq __retfn_immediate_0.0, 1\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.4(9, 4)\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_sub, 0, shape.0\n") != NULL &&
        strstr(actual_text, "func __fnwrap_add(__env.0, x.1, y.2) {\n") != NULL &&
        strstr(actual_text, "func __fnwrap_sub(__env.0, x.1, y.2) {\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic noncapturing two-arg function-value return immediate-call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_local_function_value_forwarding_into_function_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int add2(int x){ return x+2; }\n"
            "int apply(int f(int), int x){ return f(x); }\n"
            "int main(){ int c=1; int g(int)=add1; if((putint(0), c)) g=add2; return apply(g, 40); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply(f.0, x.1)\n") != NULL &&
        strstr(actual_text, "g$ftag.1 = mov 1\n") != NULL &&
        strstr(actual_text, "tmp.0 = call putint(0)\n") != NULL &&
        strstr(actual_text, "g$ftag.1 = mov 2\n") != NULL &&
        strstr(actual_text, "tmp.1 = eq g$ftag.1, 1\n") != NULL &&
        strstr(actual_text, "shape shape.0(int) -> int\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add2, 0, shape.0\n") != NULL &&
        strstr(actual_text, "func __fnwrap_add1(__env.0, x.1) {\n") != NULL &&
        strstr(actual_text, "func __fnwrap_add2(__env.0, x.1) {\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic local function-value forwarding into function parameter mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_multiple_static_function_valued_parameters_without_specialization_shell_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int double1(int x){ return x*2; }\n"
            "int compose(int f(int), int g(int), int x){ return f(g(x)); }\n"
            "int main(){ return compose(add1, double1, 20); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare compose(f.0, g.1, x.2)\n") != NULL &&
        strstr(actual_text, "shape shape.0(int) -> int\n") != NULL &&
        strstr(actual_text, "shape shape.1(int) -> int\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_double1, 0, shape.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "func __fnwrap_double1(__env.0, x.1) {\n") != NULL &&
        strstr(actual_text, "func __fnwrap_add1(__env.0, x.1) {\n") != NULL &&
        strstr(actual_text, "compose__fv_0_add1_1_double1") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: multiple static function-valued parameters should lower without specialization shell\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_second_order_function_value_parameter_forwarding_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply(int f(int), int x){ return f(x); }\n"
            "int pass(int h(int f(int), int x), int f(int), int x){ return h(f, x); }\n"
            "int add1(int x){ return x+1; }\n"
            "int main(){ return pass(apply, add1, 41); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply(f.0, x.1)\n") != NULL &&
        strstr(actual_text, "declare pass(h.0, f.1, x.2)\n") != NULL &&
        strstr(actual_text, "func main() {\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.0(41)\n") != NULL &&
        strstr(actual_text, "pass__fv_0_apply_1_add1") == NULL &&
        strstr(actual_text, "apply__fv_0_add1") == NULL &&
        strstr(actual_text, "call pass(") == NULL &&
        strstr(actual_text, "call apply(") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: second-order function-value parameter forwarding mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_second_order_closure_function_value_parameter_forwarding_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply(int f(int), int x){ return f(x); }\n"
            "int pass(int h(int f(int), int x), int f(int), int x){ return h(f, x); }\n"
            "int main(){ int y=3; int f(int)=closure [y] int (int z){ return y+z; }; return pass(apply, f, 4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply(f.0, x.1)\n") != NULL &&
        strstr(actual_text, "declare pass(h.0, f.1, x.2)\n") != NULL &&
        strstr(actual_text, "func main__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "call pass(") == NULL &&
        strstr(actual_text, "pass__fv_0_apply_1_main__closure_f_") == NULL &&
        strstr(actual_text, "apply__fv_0_main__closure_f_") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: second-order closure function-value parameter forwarding mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_second_order_zero_arg_function_value_parameter_forwarding_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply0(int f()){ return f(); }\n"
            "int pass0(int h(int f()), int f()){ return h(f); }\n"
            "int next(){ return 7; }\n"
            "int main(){ return pass0(apply0, next); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply0(f.0)\n") != NULL &&
        strstr(actual_text, "declare pass0(h.0, f.1)\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_next, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "call pass0(") == NULL &&
        strstr(actual_text, "pass0__fv_0_apply0_1_next") == NULL &&
        strstr(actual_text, "apply0__fv_0_next") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: second-order zero-arg function-value parameter forwarding mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_second_order_zero_arg_wrapper_scalar_update_function_value_parameter_forwarding_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int next(){ return 1; }\n"
            "int apply0(int f()){ return f(); }\n"
            "int pass0(int h(int f()), int f()){ int x=0; x=x+1; return h(f); }\n"
            "int main(){ return pass0(apply0, next); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply0(f.0)\n") != NULL &&
        strstr(actual_text, "declare pass0(h.0, f.1)\n") != NULL &&
        strstr(actual_text, "tmp.0 = add 0, 1\n") != NULL &&
        (strstr(actual_text, "ret 1\n") != NULL ||
            (strstr(actual_text, "fn_make __fnwrap_next, 0, shape.0\n") != NULL &&
                strstr(actual_text, "call_indirect tmp.") != NULL)) &&
        strstr(actual_text, "call pass0(") == NULL &&
        strstr(actual_text, "pass0__fv_0_apply0_1_next") == NULL &&
        strstr(actual_text, "apply0__fv_0_next") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: second-order zero-arg wrapper scalar-update function-value parameter forwarding mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_second_order_returned_zero_arg_wrapper_scalar_update_function_value_parameter_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int next(){ return 1; }\n"
            "int apply0(int f()){ return f(); }\n"
            "int pass0(int h(int f()), int f()){ int x=0; x=x+1; return h(f); }\n"
            "int idh0(int q(int h(int f()), int f()))(int h(int f()), int f()){ return q; }\n"
            "int main(){ return idh0(pass0)(apply0, next); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare idh0(__ret0.0, q.1)\n") != NULL &&
        strstr(actual_text, "tmp.0 = add 0, 1\n") != NULL &&
        strstr(actual_text, "call idh0(") == NULL &&
        strstr(actual_text, "call pass0(") == NULL &&
        strstr(actual_text, "pass0__fv_0_apply0_1_next") == NULL &&
        strstr(actual_text, "apply0__fv_0_next") == NULL &&
        strstr(actual_text, "ret 1\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: second-order returned zero-arg wrapper scalar-update immediate-call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_second_order_returned_zero_arg_wrapper_scalar_update_function_value_parameter_bind_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int next(){ return 1; }\n"
            "int apply0(int f()){ return f(); }\n"
            "int pass0(int h(int f()), int f()){ int x=0; x=x+1; return h(f); }\n"
            "int idh0(int q(int h(int f()), int f()))(int h(int f()), int f()){ return q; }\n"
            "int main(){ int g(int h(int f()), int f())=idh0(pass0); return g(apply0, next); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare idh0(__ret0.0, q.1)\n") != NULL &&
        strstr(actual_text, "g$ftag.0 = mov 1\n") != NULL &&
        strstr(actual_text, "tmp.0 = add 0, 1\n") != NULL &&
        strstr(actual_text, "call idh0(") == NULL &&
        strstr(actual_text, "call pass0(") == NULL &&
        strstr(actual_text, "pass0__fv_0_apply0_1_next") == NULL &&
        strstr(actual_text, "apply0__fv_0_next") == NULL &&
        strstr(actual_text, "ret 1\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: second-order returned zero-arg wrapper scalar-update bind-call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_second_order_zero_arg_closure_function_value_parameter_forwarding_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply0(int f()){ return f(); }\n"
            "int pass0(int h(int f()), int f()){ return h(f); }\n"
            "int main(){ int y=3; int f()=closure [y] int (){ return y; }; return pass0(apply0, f); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply0(f.0)\n") != NULL &&
        strstr(actual_text, "declare pass0(h.0, f.1)\n") != NULL &&
        strstr(actual_text, "func main__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "call pass0(") == NULL &&
        strstr(actual_text, "pass0__fv_0_apply0_1_main__closure_f_") == NULL &&
        strstr(actual_text, "apply0__fv_0_main__closure_f_") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: second-order zero-arg closure function-value parameter forwarding mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_second_order_zero_arg_void_function_value_parameter_forwarding_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "void apply0(void f()){ f(); }\n"
            "void pass0(void h(void f()), void f()){ h(f); return; }\n"
            "void ping(){ putint(7); return; }\n"
            "int main(){ pass0(apply0, ping); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply0(f.0)\n") != NULL &&
        strstr(actual_text, "declare pass0(h.0, f.1)\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_ping, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "call pass0(") == NULL &&
        strstr(actual_text, "pass0__fv_0_apply0_1_ping") == NULL &&
        strstr(actual_text, "apply0__fv_0_ping") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: second-order zero-arg void function-value parameter forwarding mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_second_order_zero_arg_void_wrapper_scalar_update_function_value_parameter_forwarding_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "void ping(){ putint(1); return; }\n"
            "void apply0(void f()){ f(); }\n"
            "void pass0(void h(void f()), void f()){ int x=0; x=x+1; h(f); return; }\n"
            "int main(){ pass0(apply0, ping); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply0(f.0)\n") != NULL &&
        strstr(actual_text, "declare pass0(h.0, f.1)\n") != NULL &&
        strstr(actual_text, "tmp.0 = add 0, 1\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_ping, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "call pass0(") == NULL &&
        strstr(actual_text, "pass0__fv_0_apply0_1_ping") == NULL &&
        strstr(actual_text, "apply0__fv_0_ping") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: second-order zero-arg void wrapper scalar-update function-value parameter forwarding mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_second_order_returned_zero_arg_void_wrapper_scalar_update_function_value_parameter_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "void ping(){ putint(1); return; }\n"
            "void apply0(void f()){ f(); }\n"
            "void pass0(void h(void f()), void f()){ int x=0; x=x+1; h(f); return; }\n"
            "void idv0(void q(void h(void f()), void f()))(void h(void f()), void f()){ return q; }\n"
            "int main(){ idv0(pass0)(apply0, ping); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare idv0(__ret0.0, q.1)\n") != NULL &&
        strstr(actual_text, "tmp.0 = add 0, 1\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_ping, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "call idv0(") == NULL &&
        strstr(actual_text, "call pass0(") == NULL &&
        strstr(actual_text, "pass0__fv_0_apply0_1_ping") == NULL &&
        strstr(actual_text, "apply0__fv_0_ping") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: second-order returned zero-arg void wrapper scalar-update immediate-call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_second_order_returned_zero_arg_void_wrapper_scalar_update_function_value_parameter_bind_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "void ping(){ putint(1); return; }\n"
            "void apply0(void f()){ f(); }\n"
            "void pass0(void h(void f()), void f()){ int x=0; x=x+1; h(f); return; }\n"
            "void idv0(void q(void h(void f()), void f()))(void h(void f()), void f()){ return q; }\n"
            "int main(){ void g(void h(void f()), void f())=idv0(pass0); g(apply0, ping); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare idv0(__ret0.0, q.1)\n") != NULL &&
        strstr(actual_text, "g$ftag.0 = mov 1\n") != NULL &&
        strstr(actual_text, "tmp.0 = add 0, 1\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_ping, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "call idv0(") == NULL &&
        strstr(actual_text, "call pass0(") == NULL &&
        strstr(actual_text, "pass0__fv_0_apply0_1_ping") == NULL &&
        strstr(actual_text, "apply0__fv_0_ping") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: second-order returned zero-arg void wrapper scalar-update bind-call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_second_order_zero_arg_void_closure_function_value_parameter_forwarding_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "void apply0(void f()){ f(); }\n"
            "void pass0(void h(void f()), void f()){ h(f); return; }\n"
            "int main(){ int y=3; void f()=closure [y] void (){ putint(y); return; }; pass0(apply0, f); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply0(f.0)\n") != NULL &&
        strstr(actual_text, "declare pass0(h.0, f.1)\n") != NULL &&
        strstr(actual_text, "shape shape.0() -> void\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "call pass0(") == NULL &&
        strstr(actual_text, "apply0__fv_0_main__closure_f_") == NULL &&
        strstr(actual_text, "pass0__fv_0_apply0_1_main__closure_f_") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: second-order zero-arg void closure function-value parameter forwarding mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_second_order_returned_passthrough_dynamic_zero_arg_void_function_value_parameter_forwarding_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "void applyv(void f()){ f(); return; }\n"
            "void applyv_twice(void f()){ f(); f(); return; }\n"
            "void ping(){ putint(7); return; }\n"
            "void idhv(void h(void f()))(void f()){ return h; }\n"
            "void pickhv(int c)(void f()){ return c ? applyv : applyv_twice; }\n"
            "int main(){ idhv(pickhv(getint()))(ping); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare applyv(f.0)\n") != NULL &&
        strstr(actual_text, "declare applyv_twice(f.0)\n") != NULL &&
        strstr(actual_text, "declare idhv(__ret0.0, h.1)\n") != NULL &&
        strstr(actual_text, "tmp.2 = call pickhv(tmp.0, tmp.1)\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_ping, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.3()\n") != NULL &&
        strstr(actual_text, "applyv__fv_0_ping") == NULL &&
        strstr(actual_text, "applyv_twice__fv_0_ping") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: second-order returned passthrough dynamic zero-arg void function-value parameter forwarding mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_second_order_returned_passthrough_dynamic_zero_arg_function_value_parameter_forwarding_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply0(int f()){ return f(); }\n"
            "int apply0_twice(int f()){ return f() + f(); }\n"
            "int next(){ return 7; }\n"
            "int idh0(int h(int f()))(int f()){ return h; }\n"
            "int pickh0(int c)(int f()){ return c ? apply0 : apply0_twice; }\n"
            "int main(){ return idh0(pickh0(getint()))(next); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply0(f.0)\n") != NULL &&
        strstr(actual_text, "declare apply0_twice(f.0)\n") != NULL &&
        strstr(actual_text, "declare idh0(__ret0.0, h.1)\n") != NULL &&
        strstr(actual_text, "tmp.2 = call pickh0(tmp.0, tmp.1)\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_next, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.3()\n") != NULL &&
        strstr(actual_text, "apply0__fv_0_next") == NULL &&
        strstr(actual_text, "apply0_twice__fv_0_next") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: second-order returned passthrough dynamic zero-arg function-value parameter forwarding mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_second_order_local_function_value_parameter_forwarding_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply(int f(int), int x){ return f(x); }\n"
            "int pass(int h(int f(int), int x), int f(int), int x){ return h(f, x); }\n"
            "int add1(int x){ return x+1; }\n"
            "int main(){ int h(int f(int), int x)=apply; return pass(h, add1, 41); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "pass__fv_0_apply_1_add1") == NULL &&
        strstr(actual_text, "apply__fv_0_add1") == NULL &&
        ((strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.0\n") != NULL &&
            strstr(actual_text, "call_indirect tmp.") != NULL) ||
            strstr(actual_text, "tmp.0 = add 41, 1\n") != NULL) &&
        strstr(actual_text, "call pass(") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: second-order local function-value parameter forwarding mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_second_order_local_reassigned_function_value_parameter_forwarding_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply(int f(int), int x){ return f(x); }\n"
            "int pass(int h(int f(int), int x), int f(int), int x){ int k(int f(int), int x)=h; k=h; return k(f, x); }\n"
            "int add1(int x){ return x+1; }\n"
            "int main(){ int h(int f(int), int x)=apply; return pass(h, add1, 41); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare pass(h.0, f.1, x.2)\n") != NULL &&
        strstr(actual_text, "pass__fv_0_apply_1_add1") == NULL &&
        strstr(actual_text, "apply__fv_0_add1") == NULL &&
        ((strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.0\n") != NULL &&
            strstr(actual_text, "call_indirect tmp.") != NULL) ||
            strstr(actual_text, "tmp.0 = add 41, 1\n") != NULL) &&
        strstr(actual_text, "call pass(") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: second-order local reassigned function-value parameter forwarding mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_second_order_local_scalar_and_reassigned_function_value_parameter_forwarding_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply(int f(int), int x){ return f(x); }\n"
            "int pass(int h(int f(int), int x), int f(int), int x){ int y=x; int k(int f(int), int x)=h; k=h; return k(f, y); }\n"
            "int add1(int x){ return x+1; }\n"
            "int main(){ return pass(apply, add1, 41); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare pass(h.0, f.1, x.2)\n") != NULL &&
        strstr(actual_text, "pass__fv_0_apply_1_add1") == NULL &&
        strstr(actual_text, "apply__fv_0_add1") == NULL &&
        ((strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.0\n") != NULL &&
            strstr(actual_text, "call_indirect tmp.") != NULL) ||
            strstr(actual_text, "tmp.0 = add 41, 1\n") != NULL) &&
        strstr(actual_text, "call pass(") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: second-order local scalar+reassigned function-value parameter forwarding mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_second_order_local_wrapper_scalar_update_function_value_parameter_forwarding_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply(int f(int), int x){ return f(x); }\n"
            "int pass(int h(int f(int), int x), int f(int), int x){ int y=x; y=y+1; return h(f, y); }\n"
            "int add1(int x){ return x+1; }\n"
            "int main(){ int h(int f(int), int x)=apply; return pass(h, add1, 41); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare pass(h.0, f.1, x.2)\n") != NULL &&
        strstr(actual_text, "h$ftag.0 = mov 1\n") != NULL &&
        strstr(actual_text, "tmp.0 = add 41, 1\n") != NULL &&
        strstr(actual_text, "pass__fv_0_apply_1_add1") == NULL &&
        strstr(actual_text, "apply__fv_0_add1") == NULL &&
        ((strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.0\n") != NULL &&
            strstr(actual_text, "call_indirect tmp.") != NULL) ||
            strstr(actual_text, "tmp.1 = add tmp.0, 1\n") != NULL) &&
        strstr(actual_text, "call pass(") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: second-order local wrapper scalar-update function-value parameter forwarding mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_second_order_local_wrapper_repeated_scalar_update_function_value_parameter_forwarding_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply(int f(int), int x){ return f(x); }\n"
            "int pass(int h(int f(int), int x), int f(int), int x){ int y=x; y=y+1; y=y+1; return h(f, y); }\n"
            "int add1(int x){ return x+1; }\n"
            "int main(){ int h(int f(int), int x)=apply; return pass(h, add1, 41); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare pass(h.0, f.1, x.2)\n") != NULL &&
        strstr(actual_text, "h$ftag.0 = mov 1\n") != NULL &&
        strstr(actual_text, "tmp.0 = add 41, 1\n") != NULL &&
        strstr(actual_text, "tmp.1 = add tmp.0, 1\n") != NULL &&
        strstr(actual_text, "pass__fv_0_apply_1_add1") == NULL &&
        strstr(actual_text, "apply__fv_0_add1") == NULL &&
        ((strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.0\n") != NULL &&
            strstr(actual_text, "call_indirect tmp.") != NULL) ||
            strstr(actual_text, "tmp.2 = add tmp.1, 1\n") != NULL) &&
        strstr(actual_text, "call pass(") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: second-order local wrapper repeated scalar-update function-value parameter forwarding mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_third_order_function_value_parameter_forwarding_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int apply(int f(int), int x){ return f(x); }\n"
            "int pass(int h(int f(int), int x), int f(int), int x){ return h(f, x); }\n"
            "int relay(int q(int h(int f(int), int x), int f(int), int x), int h(int f(int), int x), int f(int), int x){ return q(h, f, x); }\n"
            "int main(){ return relay(pass, apply, add1, 41); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare relay(q.0, h.1, f.2, x.3)\n") != NULL &&
        ((strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.0\n") != NULL &&
            strstr(actual_text, "call_indirect tmp.") != NULL) ||
            strstr(actual_text, "tmp.0 = add 41, 1\n") != NULL) &&
        strstr(actual_text, "call relay(") == NULL &&
        strstr(actual_text, "call pass(") == NULL &&
        strstr(actual_text, "relay__fv_0_pass_1_apply_2_add1") == NULL &&
        strstr(actual_text, "pass__fv_0_apply_1_add1") == NULL &&
        strstr(actual_text, "apply__fv_0_add1") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: third-order function-value parameter forwarding mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_third_order_zero_arg_wrapper_scalar_update_function_value_parameter_forwarding_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int next(){ return 1; }\n"
            "int apply0(int f()){ return f(); }\n"
            "int pass0(int h(int f()), int f()){ int x=0; x=x+1; return h(f); }\n"
            "int relay0(int q(int h(int f()), int f()), int h(int f()), int f()){ return q(h, f); }\n"
            "int main(){ return relay0(pass0, apply0, next); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare relay0(q.0, h.1, f.2)\n") != NULL &&
        strstr(actual_text, "tmp.0 = add 0, 1\n") != NULL &&
        (strstr(actual_text, "ret 1\n") != NULL ||
            (strstr(actual_text, "fn_make __fnwrap_next, 0, shape.0\n") != NULL &&
                strstr(actual_text, "call_indirect tmp.") != NULL)) &&
        strstr(actual_text, "call relay0(") == NULL &&
        strstr(actual_text, "call pass0(") == NULL &&
        strstr(actual_text, "relay0__fv_0_pass0_1_apply0_2_next") == NULL &&
        strstr(actual_text, "pass0__fv_0_apply0_1_next") == NULL &&
        strstr(actual_text, "apply0__fv_0_next") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: third-order zero-arg wrapper scalar-update function-value parameter forwarding mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_third_order_wrapper_scalar_update_function_value_parameter_forwarding_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int apply(int f(int), int x){ return f(x); }\n"
            "int pass(int h(int f(int), int x), int f(int), int x){ return h(f, x); }\n"
            "int relay(int q(int h(int f(int), int x), int f(int), int x), int h(int f(int), int x), int f(int), int x){ int y=x; y=y+1; return q(h, f, y); }\n"
            "int main(){ return relay(pass, apply, add1, 41); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare relay(q.0, h.1, f.2, x.3)\n") != NULL &&
        strstr(actual_text, "relay__fv_0_pass_1_apply_2_add1") == NULL &&
        strstr(actual_text, "pass__fv_0_apply_1_add1") == NULL &&
        strstr(actual_text, "apply__fv_0_add1") == NULL &&
        strstr(actual_text, "tmp.0 = add 41, 1\n") != NULL &&
        strstr(actual_text, "tmp.1 = add tmp.0, 1\n") != NULL &&
        strstr(actual_text, "call relay(") == NULL &&
        strstr(actual_text, "call pass(") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: third-order wrapper scalar-update function-value parameter forwarding mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_third_order_zero_arg_void_wrapper_scalar_update_function_value_parameter_forwarding_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "void ping(){ putint(1); return; }\n"
            "void apply0(void f()){ f(); }\n"
            "void pass0(void h(void f()), void f()){ int x=0; x=x+1; h(f); return; }\n"
            "void relay0(void q(void h(void f()), void f()), void h(void f()), void f()){ q(h, f); return; }\n"
            "int main(){ relay0(pass0, apply0, ping); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare relay0(q.0, h.1, f.2)\n") != NULL &&
        strstr(actual_text, "tmp.0 = add 0, 1\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_ping, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "call relay0(") == NULL &&
        strstr(actual_text, "call pass0(") == NULL &&
        strstr(actual_text, "relay0__fv_0_pass0_1_apply0_2_ping") == NULL &&
        strstr(actual_text, "pass0__fv_0_apply0_1_ping") == NULL &&
        strstr(actual_text, "apply0__fv_0_ping") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: third-order zero-arg void wrapper scalar-update function-value parameter forwarding mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_third_order_wrapper_repeated_scalar_update_function_value_parameter_forwarding_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int apply(int f(int), int x){ return f(x); }\n"
            "int pass(int h(int f(int), int x), int f(int), int x){ int y=x; y=y+1; y=y+1; return h(f, y); }\n"
            "int relay(int q(int h(int f(int), int x), int f(int), int x), int h(int f(int), int x), int f(int), int x){ return q(h, f, x); }\n"
            "int main(){ return relay(pass, apply, add1, 41); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare relay(q.0, h.1, f.2, x.3)\n") != NULL &&
        strstr(actual_text, "relay__fv_0_pass_1_apply_2_add1") == NULL &&
        strstr(actual_text, "pass__fv_0_apply_1_add1") == NULL &&
        strstr(actual_text, "apply__fv_0_add1") == NULL &&
        strstr(actual_text, "tmp.0 = add 41, 1\n") != NULL &&
        strstr(actual_text, "tmp.1 = add tmp.0, 1\n") != NULL &&
        strstr(actual_text, "tmp.2 = add tmp.1, 1\n") != NULL &&
        strstr(actual_text, "call relay(") == NULL &&
        strstr(actual_text, "call pass(") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: third-order wrapper repeated scalar-update function-value parameter forwarding mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_third_order_returned_function_value_parameter_bind_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int apply(int f(int), int x){ return f(x); }\n"
            "int pass(int h(int f(int), int x), int f(int), int x){ return h(f, x); }\n"
            "int id3(int q(int h(int f(int), int x), int f(int), int x))(int h(int f(int), int x), int f(int), int x){ return q; }\n"
            "int main(){ int g(int h(int f(int), int x), int f(int), int x)=id3(pass); return g(apply, add1, 41); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare id3(__ret0.0, q.1)\n") != NULL &&
        strstr(actual_text, "g$ftag.0 = mov 1\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.0(41)\n") != NULL &&
        strstr(actual_text, "pass__fv_0_apply_1_add1") == NULL &&
        strstr(actual_text, "apply__fv_0_add1") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: third-order returned function-value parameter bind mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_third_order_local_function_value_parameter_forwarding_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int apply(int f(int), int x){ return f(x); }\n"
            "int pass(int h(int f(int), int x), int f(int), int x){ return h(f, x); }\n"
            "int relay(int q(int h(int f(int), int x), int f(int), int x), int h(int f(int), int x), int f(int), int x){ return q(h, f, x); }\n"
            "int main(){ int g(int q(int h(int f(int), int x), int f(int), int x), int h(int f(int), int x), int f(int), int x)=relay; return g(pass, apply, add1, 41); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare relay(q.0, h.1, f.2, x.3)\n") != NULL &&
        strstr(actual_text, "g$ftag.0 = mov 1\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.0(41)\n") != NULL &&
        strstr(actual_text, "relay__fv_0_pass_1_apply_2_add1") == NULL &&
        strstr(actual_text, "pass__fv_0_apply_1_add1") == NULL &&
        strstr(actual_text, "apply__fv_0_add1") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: third-order local function-value parameter forwarding mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_second_order_returned_function_value_parameter_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply(int f(int), int x){ return f(x); }\n"
            "int idh(int h(int f(int), int x))(int f(int), int x){ return h; }\n"
            "int add1(int x){ return x+1; }\n"
            "int main(){ return idh(apply)(add1, 41); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare idh(__ret0.0, h.1)\n") != NULL &&
        strstr(actual_text, "func main() {\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.0(41)\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "apply__fv_0_add1") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: second-order returned function-value parameter immediate-call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_second_order_returned_closure_function_value_parameter_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply(int f(int), int x){ return f(x); }\n"
            "int idh(int h(int f(int), int x))(int f(int), int x){ return h; }\n"
            "int main(){ int y=3; int f(int)=closure [y] int (int z){ return y+z; }; return idh(apply)(f, 4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare idh(__ret0.0, h.1)\n") != NULL &&
        strstr(actual_text, "shape shape.0(int) -> int\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "call idh(") == NULL &&
        strstr(actual_text, "apply__fv_0_main__closure_f_") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: second-order returned closure function-value parameter immediate-call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_second_order_dynamic_returned_function_value_parameter_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply(int f(int), int x){ return f(x); }\n"
            "int apply_twice(int f(int), int x){ return f(f(x)); }\n"
            "int pick(int c)(int f(int), int x){ return c ? apply : apply_twice; }\n"
            "int add1(int x){ return x+1; }\n"
            "int main(){ return pick(getint())(add1, 41); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply(f.0, x.1)\n") != NULL &&
        strstr(actual_text, "declare apply_twice(f.0, x.1)\n") != NULL &&
        strstr(actual_text, "tmp.2 = call pick(tmp.0, tmp.1)\n") != NULL &&
        strstr(actual_text, "tmp.3 = eq __retfn_immediate_0.0, 1\n") != NULL &&
        strstr(actual_text, "apply__fv_0_add1") == NULL &&
        strstr(actual_text, "apply_twice__fv_0_add1") == NULL &&
        ir_test_count_fragment_occurrences(actual_text, "fn_make __fnwrap_add1, 0, shape.") == 3u &&
        ir_test_count_fragment_occurrences(actual_text, "call_indirect tmp.") == 3u;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: second-order dynamic returned function-value parameter immediate-call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_second_order_returned_passthrough_dynamic_noncapturing_function_value_parameter_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int idh(int h(int f(int), int x))(int f(int), int x){ return h; }\n"
            "int apply(int f(int), int x){ return f(x); }\n"
            "int apply_twice(int f(int), int x){ return f(f(x)); }\n"
            "int pickh_nc(int c)(int f(int), int x){ return c ? apply : apply_twice; }\n"
            "int add1(int x){ return x+1; }\n"
            "int main(){ return idh(pickh_nc(getint()))(add1, 41); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare idh(__ret0.0, h.1)\n") != NULL &&
        strstr(actual_text, "tmp.2 = call pickh_nc(tmp.0, tmp.1)\n") != NULL &&
        strstr(actual_text, "tmp.3 = eq __retfn_argcap_0.0, 1\n") != NULL &&
        strstr(actual_text, "apply__fv_0_add1") == NULL &&
        strstr(actual_text, "apply_twice__fv_0_add1") == NULL &&
        ir_test_count_fragment_occurrences(actual_text, "fn_make __fnwrap_add1, 0, shape.") == 3u &&
        ir_test_count_fragment_occurrences(actual_text, "call_indirect tmp.") == 3u;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: second-order returned passthrough dynamic noncapturing function-value parameter immediate-call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_second_order_returned_passthrough_dynamic_function_value_parameter_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int idh(int h(int f(int), int x))(int f(int), int x){ return h; }\n"
            "int pickh(int base, int c)(int f(int), int x){ int h(int f(int), int x)=closure [base] int (int g(int), int y){ return g(y) + base; }; int k(int f(int), int x)=closure [base] int (int g(int), int y){ return g(g(y)) + base; }; if(c) h=k; return h; }\n"
            "int add1(int x){ return x+1; }\n"
            "int main(){ return idh(pickh(5, getint()))(add1, 41); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "tmp.3 = call pickh(tmp.0, tmp.1, 5, tmp.2)\n") != NULL &&
        strstr(actual_text, "tmp.4 = eq __retclosure_argcap_0.0, 1\n") != NULL &&
        strstr(actual_text, "call pickh__closure_h_") == NULL &&
        strstr(actual_text, "call pickh__closure_k_") == NULL &&
        strstr(actual_text, "__fv_1_add1") == NULL &&
        strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.1\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.2\n") != NULL &&
        strstr(actual_text, "func __fnwrap_add1(__env.0, x.1) {\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: second-order returned passthrough dynamic function-value parameter immediate-call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_second_order_returned_passthrough_dynamic_local_function_value_parameter_forwarding_with_unary_helper_eval_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int idh(int h(int f(int), int x))(int f(int), int x){ return h; }\n"
            "int pickh(int base, int c)(int f(int), int x){ int h(int f(int), int x)=closure [base] int (int g(int), int y){ return -g(y) + base; }; int k(int f(int), int x)=closure [base] int (int g(int), int y){ return -g(g(y)) + base; }; if(c) h=k; return h; }\n"
            "int add1(int x){ return x+1; }\n"
            "int main(){ return idh(pickh(5, getint()))(add1, 41); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "tmp.3 = call pickh(tmp.0, tmp.1, 5, tmp.2)\n") != NULL &&
        strstr(actual_text, "tmp.4 = eq __retclosure_argcap_0.0, 1\n") != NULL &&
        strstr(actual_text, "pickh__closure_h_") == NULL &&
        strstr(actual_text, "pickh__closure_k_") == NULL &&
        strstr(actual_text, "__fv_1_add1") == NULL &&
        ir_test_count_fragment_occurrences(actual_text, "fn_make __fnwrap_add1, 0, shape.") == 3u &&
        ir_test_count_fragment_occurrences(actual_text, "call_indirect tmp.") == 3u &&
        ir_test_count_fragment_occurrences(actual_text, "sub 0, tmp.") == 2u;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: second-order returned passthrough dynamic local function-value unary helper-eval mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_second_order_returned_passthrough_dynamic_local_function_value_parameter_forwarding_with_compound_update_helper_eval_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int idh(int h(int f(int), int x))(int f(int), int x){ return h; }\n"
            "int pickh(int base, int c)(int f(int), int x){ int h(int f(int), int x)=closure [base] int (int g(int), int y){ int z=y; z+=1; return g(z) + base; }; int k(int f(int), int x)=closure [base] int (int g(int), int y){ int z=y; z+=1; return g(g(z)) + base; }; if(c) h=k; return h; }\n"
            "int add1(int x){ return x+1; }\n"
            "int main(){ return idh(pickh(5, getint()))(add1, 41); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "tmp.3 = call pickh(tmp.0, tmp.1, 5, tmp.2)\n") != NULL &&
        strstr(actual_text, "pickh__closure_h_") == NULL &&
        strstr(actual_text, "pickh__closure_k_") == NULL &&
        strstr(actual_text, "__fv_1_add1") == NULL &&
        ir_test_count_fragment_occurrences(actual_text, "add 41, 1") == 2u &&
        ir_test_count_fragment_occurrences(actual_text, "fn_make __fnwrap_add1, 0, shape.") == 3u &&
        ir_test_count_fragment_occurrences(actual_text, "call_indirect tmp.") == 3u;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: second-order returned passthrough dynamic local function-value compound-update helper-eval mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_second_order_returned_passthrough_dynamic_local_function_value_parameter_forwarding_with_comma_helper_eval_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int idh(int h(int f(int), int x))(int f(int), int x){ return h; }\n"
            "int pickh(int base, int c)(int f(int), int x){ int h(int f(int), int x)=closure [base] int (int g(int), int y){ return (g(y), g(y) + base); }; int k(int f(int), int x)=closure [base] int (int g(int), int y){ return (g(g(y)), g(g(y)) + base); }; if(c) h=k; return h; }\n"
            "int add1(int x){ return x+1; }\n"
            "int main(){ return idh(pickh(5, getint()))(add1, 41); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "pickh__closure_h_") == NULL &&
        strstr(actual_text, "pickh__closure_k_") == NULL &&
        strstr(actual_text, "__fv_1_add1") == NULL &&
        ir_test_count_fragment_occurrences(actual_text, "fn_make __fnwrap_add1, 0, shape.") == 6u &&
        ir_test_count_fragment_occurrences(actual_text, "call_indirect tmp.") == 6u &&
        ir_test_count_fragment_occurrences(actual_text, "add tmp.") >= 2u;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: second-order returned passthrough dynamic local function-value comma helper-eval mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_second_order_returned_passthrough_dynamic_local_function_value_parameter_forwarding_with_ternary_helper_eval_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int idh(int h(int f(int), int x))(int f(int), int x){ return h; }\n"
            "int pickh(int base, int c)(int f(int), int x){ int h(int f(int), int x)=closure [base] int (int g(int), int y){ return g(y) ? g(y) + base : base; }; int k(int f(int), int x)=closure [base] int (int g(int), int y){ return g(g(y)) ? g(g(y)) + base : base; }; if(c) h=k; return h; }\n"
            "int add1(int x){ return x+1; }\n"
            "int main(){ return idh(pickh(5, getint()))(add1, 41); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "pickh__closure_h_") == NULL &&
        strstr(actual_text, "pickh__closure_k_") == NULL &&
        strstr(actual_text, "__fv_1_add1") == NULL &&
        ir_test_count_fragment_occurrences(actual_text, "fn_make __fnwrap_add1, 0, shape.") == 6u &&
        ir_test_count_fragment_occurrences(actual_text, "call_indirect tmp.") == 6u &&
        ir_test_count_fragment_occurrences(actual_text, "br tmp.") >= 3u;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: second-order returned passthrough dynamic local function-value ternary helper-eval mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_second_order_returned_passthrough_dynamic_local_function_value_parameter_forwarding_with_statement_call_prefix_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int idh(int h(int f(int), int x))(int f(int), int x){ return h; }\n"
            "int pickh(int base, int c)(int f(int), int x){ int h(int f(int), int x)=closure [base] int (int g(int), int y){ g(y); return g(y) + base; }; int k(int f(int), int x)=closure [base] int (int g(int), int y){ g(g(y)); return g(g(y)) + base; }; if(c) h=k; return h; }\n"
            "int add1(int x){ return x+1; }\n"
            "int main(){ return idh(pickh(5, getint()))(add1, 41); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "pickh__closure_h_") == NULL &&
        strstr(actual_text, "pickh__closure_k_") == NULL &&
        strstr(actual_text, "__fv_1_add1") == NULL &&
        ir_test_count_fragment_occurrences(actual_text, "fn_make __fnwrap_add1, 0, shape.") == 6u &&
        ir_test_count_fragment_occurrences(actual_text, "call_indirect tmp.") == 6u;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: second-order returned passthrough dynamic local function-value statement-call-prefix helper-eval mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_second_order_returned_passthrough_dynamic_local_function_value_parameter_forwarding_with_if_return_helper_eval_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int idh(int h(int f(int), int x))(int f(int), int x){ return h; }\n"
            "int pickh(int base, int c)(int f(int), int x){ int h(int f(int), int x)=closure [base] int (int g(int), int y){ if(g(y)) return g(y) + base; return base; }; int k(int f(int), int x)=closure [base] int (int g(int), int y){ if(g(g(y))) return g(g(y)) + base; return base; }; if(c) h=k; return h; }\n"
            "int add1(int x){ return x+1; }\n"
            "int main(){ return idh(pickh(5, getint()))(add1, 41); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "pickh__closure_h_") == NULL &&
        strstr(actual_text, "pickh__closure_k_") == NULL &&
        strstr(actual_text, "__fv_1_add1") == NULL &&
        ir_test_count_fragment_occurrences(actual_text, "fn_make __fnwrap_add1, 0, shape.") == 6u &&
        ir_test_count_fragment_occurrences(actual_text, "call_indirect tmp.") == 6u &&
        ir_test_count_fragment_occurrences(actual_text, "br tmp.") >= 3u;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: second-order returned passthrough dynamic local function-value if-return helper-eval mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_second_order_dynamic_local_function_value_parameter_forwarding_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply(int f(int), int x){ return f(x); }\n"
            "int apply_twice(int f(int), int x){ return f(f(x)); }\n"
            "int pass(int h(int f(int), int x), int f(int), int x){ return h(f, x); }\n"
            "int add1(int x){ return x+1; }\n"
            "int main(){ int c=getint(); int h(int f(int), int x)= c ? apply : apply_twice; return pass(h, add1, 41); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "tmp.0 = call getint()\n") != NULL &&
        strstr(actual_text, "pass__fv_0_apply_1_add1") == NULL &&
        strstr(actual_text, "pass__fv_0_apply_twice_1_add1") == NULL &&
        strstr(actual_text, "apply_twice__fv_0_add1") == NULL &&
        strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.1\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.2\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: second-order dynamic local function-value parameter forwarding mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_second_order_dynamic_local_closure_function_value_parameter_forwarding_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply(int f(int), int x){ return f(x); }\n"
            "int apply_twice(int f(int), int x){ return f(f(x)); }\n"
            "int pass(int h(int f(int), int x), int f(int), int x){ return h(f, x); }\n"
            "int main(){ int c=getint(); int y=3; int f(int)=closure [y] int (int z){ return y+z; }; int h(int f(int), int x)= c ? apply : apply_twice; return pass(h, f, 4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "tmp.0 = call getint()\n") != NULL &&
        strstr(actual_text, "tmp.2 = eq h$ftag.4, 1\n") != NULL &&
        strstr(actual_text, "func pass__fv_0_apply_1_main__closure_f_") == NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "call pass(") == NULL &&
        strstr(actual_text, "apply__fv_0_main__closure_f_") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: second-order dynamic local closure function-value parameter forwarding mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_second_order_returned_passthrough_dynamic_local_function_value_parameter_forwarding_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int idh(int h(int f(int), int x))(int f(int), int x){ return h; }\n"
            "int pickh(int base, int c)(int f(int), int x){ int h(int f(int), int x)=closure [base] int (int g(int), int y){ return g(y) + base; }; int k(int f(int), int x)=closure [base] int (int g(int), int y){ return g(g(y)) + base; }; if(c) h=k; return h; }\n"
            "int add1(int x){ return x+1; }\n"
            "int main(){ int g(int f(int), int x)=idh(pickh(5, getint())); return g(add1, 41); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "tmp.3 = call pickh(tmp.0, tmp.1, 5, tmp.2)\n") != NULL &&
        strstr(actual_text, "g$ftag.0 = mov __retclosure_declslot_0.2\n") != NULL &&
        strstr(actual_text, "g$closurecap$0.1 = mov __retclosure_declslot_1.3\n") != NULL &&
        strstr(actual_text, "tmp.4 = eq g$ftag.0, 1\n") != NULL &&
        strstr(actual_text, "pickh__closure_h_") == NULL &&
        strstr(actual_text, "pickh__closure_k_") == NULL &&
        strstr(actual_text, "__fv_1_add1") == NULL &&
        strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.1\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.2\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: second-order returned passthrough dynamic local function-value parameter forwarding mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_second_order_returned_closure_function_value_parameter_forwarding_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply(int f(int), int x){ return f(x); }\n"
            "int pass(int h(int f(int), int x), int f(int), int x){ return h(f, x); }\n"
            "int make(int y)(int){ return closure [y] int (int z){ return y+z; }; }\n"
            "int main(){ return pass(apply, make(3), 4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func make(__ret0.0, y.1) {\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call make(") == 1u &&
        strstr(actual_text, "func make__retclosure_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_make__retclosure_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "call pass(") == NULL &&
        strstr(actual_text, "pass__fv_0_apply_1_make__retclosure_") == NULL &&
        strstr(actual_text, "apply__fv_0_make__retclosure_") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: second-order returned closure function-value parameter forwarding mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_static_function_value_capture_inside_closure_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int main(){ int f(int)=add1; int g(int)=closure [f] int (int y){ return f(y); }; return g(4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "g$closurecap$0.") != NULL &&
        strstr(actual_text, "func main__closure_g_") != NULL &&
        strstr(actual_text, "__fv_0_add1(y.0) {\n") != NULL &&
        strstr(actual_text, "tmp.0 = call main__closure_g_") != NULL &&
        strstr(actual_text, "tmp.0 = fn_make __fnwrap_add1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "tmp.1 = call_indirect tmp.0(y.0)\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: static function-value capture inside closure mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_function_parameter_capture_inside_closure_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int wrap(int f(int), int x){ int g(int)=closure [f] int (int y){ return f(y); }; return g(x); }\n"
            "int add1(int x){ return x+1; }\n"
            "int main(){ return wrap(add1, 4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "g$closurecap$0.") != NULL &&
        strstr(actual_text, "g$closurecap$0.2 = mov f.0\n") != NULL &&
        strstr(actual_text, "declare wrap__closure_g_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_wrap__closure_g_") != NULL &&
        strstr(actual_text, "call wrap__closure_g_") != NULL &&
        strstr(actual_text, "func wrap__fv_0_add1(") != NULL &&
        strstr(actual_text, "__closure_g_65566__fv_0_add1") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.") != NULL &&
        strstr(actual_text, "call_indirect tmp.0(y.0)\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: function-parameter capture inside closure mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_returned_function_parameter_capture_inside_closure_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int make(int f(int))(int){ return closure [f] int (int y){ return f(y); }; }\n"
            "int add1(int x){ return x+1; }\n"
            "int main(){ int g(int)=make(add1); return g(4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func make(__ret0.0, f.1) {\n") != NULL &&
        strstr(actual_text, "0 = store_indirect __ret0.0, f.1\n") != NULL &&
        strstr(actual_text, "declare make__retclosure_1_35(f.0, y.1)\n") != NULL &&
        strstr(actual_text, "tmp.1 = call make(tmp.0, 0)\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_make__retclosure_1_35_1, tmp.2, shape.0\n") != NULL &&
        strstr(actual_text, "call make__retclosure_1_35(tmp.1, y.1)\n") != NULL &&
        strstr(actual_text, "tmp.4 = call_indirect tmp.3(4)\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: returned function-parameter capture inside closure mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_passthrough_decl_local_function_value_forwarding_without_specialization_shell_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int id(int f(int))(int){ return f; }\n"
            "int apply(int f(int), int x){ return f(x); }\n"
            "int wrapper(int f(int), int x){ int g(int)=id(f); return apply(g, x); }\n"
            "int main(){ return wrapper(add1, 41); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare id(__ret0.0, f.1)\n") != NULL &&
        strstr(actual_text, "declare apply(f.0, x.1)\n") != NULL &&
        strstr(actual_text, "declare wrapper(f.0, x.1)\n") != NULL &&
        strstr(actual_text, "func main() {\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "func __fnwrap_add1(__env.0, x.1) {\n") != NULL &&
        strstr(actual_text, "wrapper__fv_0_add1") == NULL &&
        strstr(actual_text, "call wrapper(") == NULL &&
        ir_test_count_fragment_occurrences(actual_text, "fn_make __fnwrap_add1, 0, shape.0\n") == 1u;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: passthrough decl-local function-value forwarding should lower without specialization shell\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_parameter_local_function_value_direct_call_without_specialization_shell_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int test(int f(int), int x){ int g(int)=f; return g(x); }\n"
            "int main(){ return test(add1, 41); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare test(f.0, x.1)\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "func __fnwrap_add1(__env.0, x.1) {\n") != NULL &&
        strstr(actual_text, "test__fv_0_add1") == NULL &&
        strstr(actual_text, "call test(") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: parameter-local function-value direct call should lower without specialization shell\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_parameter_local_scalar_rebind_function_value_direct_call_without_specialization_shell_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int test(int f(int), int x){ int y=x; return f(y); }\n"
            "int main(){ return test(add1, 41); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare test(f.0, x.1)\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "y.") == NULL &&
        strstr(actual_text, "test__fv_0_add1") == NULL &&
        strstr(actual_text, "call test(") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: parameter-local scalar rebind function-value direct call should lower without specialization shell\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_parameter_local_scalar_and_alias_function_value_direct_call_without_specialization_shell_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int test(int f(int), int x){ int y=x; int g(int)=f; return g(y); }\n"
            "int main(){ return test(add1, 41); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare test(f.0, x.1)\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "y.") == NULL &&
        strstr(actual_text, "g$ftag.") == NULL &&
        strstr(actual_text, "test__fv_0_add1") == NULL &&
        strstr(actual_text, "call test(") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: parameter-local scalar+alias function-value direct call should lower without specialization shell\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_parameter_local_scalar_update_function_value_direct_call_without_specialization_shell_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int test(int f(int), int x){ int y=x; y=y+1; return f(y); }\n"
            "int main(){ return test(add1, 41); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare test(f.0, x.1)\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "test__fv_0_add1") == NULL &&
        strstr(actual_text, "func test__fv_0_add1") == NULL &&
        strstr(actual_text, "call test(") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: parameter-local scalar update function-value direct call should lower without specialization shell\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_parameter_local_scalar_update_and_alias_function_value_direct_call_without_specialization_shell_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int test(int f(int), int x){ int y=x; y=y+1; int g(int)=f; g=f; return g(y); }\n"
            "int main(){ return test(add1, 41); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare test(f.0, x.1)\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "test__fv_0_add1") == NULL &&
        strstr(actual_text, "func test__fv_0_add1") == NULL &&
        strstr(actual_text, "call test(") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: parameter-local scalar update+alias function-value direct call should lower without specialization shell\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_parameter_local_call_update_function_value_direct_call_without_specialization_shell_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int test(int f(int), int x){ int y=x; y=f(y); return y; }\n"
            "int main(){ return test(add1, 41); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare test(f.0, x.1)\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "test__fv_0_add1") == NULL &&
        strstr(actual_text, "func test__fv_0_add1") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: parameter-local call update function-value direct call should lower without specialization shell\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_parameter_local_repeated_call_update_function_value_direct_call_without_specialization_shell_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int test(int f(int), int x){ int y=x; y=f(y); y=f(y); return y; }\n"
            "int main(){ return test(add1, 41); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare test(f.0, x.1)\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.1\n") != NULL &&
        strstr(actual_text, "test__fv_0_add1") == NULL &&
        strstr(actual_text, "func test__fv_0_add1") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: parameter-local repeated call update function-value direct call should lower without specialization shell\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_parameter_local_two_callable_update_without_specialization_shell_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int double1(int x){ return x*2; }\n"
            "int test(int f(int), int g(int), int x){ int y=x; y=g(y); y=f(y); return y; }\n"
            "int main(){ return test(add1, double1, 20); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare test(f.0, g.1, x.2)\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_double1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.1\n") != NULL &&
        strstr(actual_text, "compose__fv_0_add1_1_double1") == NULL &&
        strstr(actual_text, "test__fv_0_add1_1_double1") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: parameter-local two-callable update should lower without specialization shell\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_parameter_local_zero_arg_function_value_direct_call_without_specialization_shell_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int next(){ return 7; }\n"
            "int test(int f()){ int g()=f; return g(); }\n"
            "int main(){ return test(next); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare test(f.0)\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_next, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "func __fnwrap_next(__env.0) {\n") != NULL &&
        strstr(actual_text, "test__fv_0_next") == NULL &&
        strstr(actual_text, "call test(") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: parameter-local zero-arg function-value direct call should lower without specialization shell\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_parameter_local_zero_arg_void_function_value_direct_call_without_specialization_shell_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "void ping(){ putint(7); return; }\n"
            "int test(void f()){ void g()=f; g(); return 0; }\n"
            "int main(){ return test(ping); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare test(f.0)\n") != NULL &&
        strstr(actual_text, "shape shape.0() -> void\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_ping, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "func __fnwrap_ping(__env.0) {\n") != NULL &&
        strstr(actual_text, "test__fv_0_ping") == NULL &&
        strstr(actual_text, "call test(") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: parameter-local zero-arg void function-value direct call should lower without specialization shell\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_parameter_local_closure_direct_call_without_specialization_shell_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int test(int f(int), int x){ int g(int)=f; return g(x); }\n"
            "int main(){ int y=3; int f(int)=closure [y] int (int z){ return y+z; }; return test(f, 4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare test(f.0, x.1)\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "test__fv_0_main__closure_f_") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: parameter-local closure direct call should lower without specialization shell\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_parameter_local_closure_forward_without_specialization_shell_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply(int f(int), int x){ return f(x); }\n"
            "int test(int f(int), int x){ int g(int)=f; return apply(g, x); }\n"
            "int main(){ int y=3; int f(int)=closure [y] int (int z){ return y+z; }; return test(f, 4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply(f.0, x.1)\n") != NULL &&
        strstr(actual_text, "declare test(f.0, x.1)\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "test__fv_0_main__closure_f_") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: parameter-local closure forwarding should lower without specialization shell\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_parameter_local_zero_arg_void_function_value_forward_without_specialization_shell_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "void apply0(void f()){ f(); }\n"
            "void ping(){ putint(7); return; }\n"
            "int test(void f()){ void g()=f; apply0(g); return 0; }\n"
            "int main(){ return test(ping); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply0(f.0)\n") != NULL &&
        strstr(actual_text, "declare test(f.0)\n") != NULL &&
        strstr(actual_text, "shape shape.0() -> void\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_ping, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "func __fnwrap_ping(__env.0) {\n") != NULL &&
        strstr(actual_text, "test__fv_0_ping") == NULL &&
        strstr(actual_text, "call test(") == NULL &&
        strstr(actual_text, "call apply0(") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: parameter-local zero-arg void function-value forwarding should lower without specialization shell\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_local_zero_arg_function_value_forwarding_into_function_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int next1(){ return 1; }\n"
            "int next2(){ return 2; }\n"
            "int apply0(int f()){ return f(); }\n"
            "int main(){ int c=1; int g()=next1; if((putint(0), c)) g=next2; return apply0(g); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply0(f.0)\n") != NULL &&
        strstr(actual_text, "g$ftag.1 = mov 1\n") != NULL &&
        strstr(actual_text, "tmp.0 = call putint(0)\n") != NULL &&
        strstr(actual_text, "g$ftag.1 = mov 2\n") != NULL &&
        strstr(actual_text, "tmp.1 = eq g$ftag.1, 1\n") != NULL &&
        strstr(actual_text, "shape shape.0() -> int\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_next1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_next2, 0, shape.0\n") != NULL &&
        strstr(actual_text, "func __fnwrap_next1(__env.0) {\n") != NULL &&
        strstr(actual_text, "func __fnwrap_next2(__env.0) {\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic local zero-arg function-value forwarding mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_local_zero_arg_void_function_value_forwarding_into_function_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "void ping1(){ putint(1); }\n"
            "void ping2(){ putint(2); }\n"
            "void apply0(void f()){ f(); }\n"
            "int main(){ int c=1; void g()=ping1; if((putint(0), c)) g=ping2; apply0(g); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply0(f.0)\n") != NULL &&
        strstr(actual_text, "g$ftag.1 = mov 1\n") != NULL &&
        strstr(actual_text, "tmp.0 = call putint(0)\n") != NULL &&
        strstr(actual_text, "g$ftag.1 = mov 2\n") != NULL &&
        strstr(actual_text, "tmp.1 = eq g$ftag.1, 1\n") != NULL &&
        strstr(actual_text, "shape shape.0() -> void\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_ping1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_ping2, 0, shape.0\n") != NULL &&
        strstr(actual_text, "func __fnwrap_ping1(__env.0) {\n") != NULL &&
        strstr(actual_text, "func __fnwrap_ping2(__env.0) {\n") != NULL &&
        strstr(actual_text, "ret 0\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic local zero-arg void function-value forwarding mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_runtime_closure_local_forward_into_function_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply(int f(int), int x){ return f(x); }\n"
            "int main(){ int c=1; int x=3; int y=5; int f(int)=closure [x] int (int z) { return x + z; }; int g(int)=closure [y] int (int z) { return y + z; }; if((putint(0), c)) f=g; return apply(f, 4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply(f.0, x.1)\n") != NULL &&
        strstr(actual_text, "tmp.0 = call putint(0)\n") != NULL &&
        strstr(actual_text, "f$closurecap$0.3 = mov g$closurecap$0.5\n") != NULL &&
        strstr(actual_text, "f$ftag.4 = mov 2\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_g_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic runtime closure local forwarding into function parameter mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_runtime_zero_arg_void_closure_local_forward_into_function_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "void apply0(void f()){ f(); }\n"
            "int main(){ int c=1; int x=7; int y=9; void f()=closure [x] void () { putint(x); return; }; void g()=closure [y] void () { putint(y); return; }; if((putint(0), c)) f=g; apply0(f); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply0(f.0)\n") != NULL &&
        strstr(actual_text, "tmp.0 = call putint(0)\n") != NULL &&
        strstr(actual_text, "f$closurecap$0.3 = mov g$closurecap$0.5\n") != NULL &&
        strstr(actual_text, "f$ftag.4 = mov 2\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "ret 0\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic runtime zero-arg void closure local forwarding into function parameter mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_local_zero_arg_closure_direct_callee_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int main(){ int c=1; int x=3; int y=5; int f()=closure [x] int () { return x; }; int g()=closure [y] int () { return y; }; if(c) f=g; return f(); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "f$closurecap$0.3 = mov x.1\n") != NULL &&
        strstr(actual_text, "g$closurecap$0.5 = mov y.2\n") != NULL &&
        strstr(actual_text, "f$closurecap$0.3 = mov g$closurecap$0.5\n") != NULL &&
        strstr(actual_text, "f$ftag.4 = mov 2\n") != NULL &&
        strstr(actual_text, "shape shape.0() -> int\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "__closure_env_0.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic local zero-arg closure direct-callee mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_local_zero_arg_void_closure_direct_callee_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int main(){ int c=1; int x=3; int y=5; void f()=closure [x] void () { putint(x); return; }; void g()=closure [y] void () { putint(y); return; }; if(c) f=g; f(); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "f$closurecap$0.3 = mov x.1\n") != NULL &&
        strstr(actual_text, "g$closurecap$0.5 = mov y.2\n") != NULL &&
        strstr(actual_text, "f$closurecap$0.3 = mov g$closurecap$0.5\n") != NULL &&
        strstr(actual_text, "f$ftag.4 = mov 2\n") != NULL &&
        strstr(actual_text, "shape shape.0() -> void\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "__closure_env_0.") != NULL &&
        strstr(actual_text, "ret 0\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic local zero-arg void closure direct-callee mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_runtime_local_zero_arg_closure_direct_callee_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int main(){ int c=1; int x=3; int y=5; int f()=closure [x] int () { return x; }; int g()=closure [y] int () { return y; }; if((putint(0), c)) f=g; return f(); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "tmp.0 = call putint(0)\n") != NULL &&
        strstr(actual_text, "f$closurecap$0.3 = mov x.1\n") != NULL &&
        strstr(actual_text, "g$closurecap$0.5 = mov y.2\n") != NULL &&
        strstr(actual_text, "f$closurecap$0.3 = mov g$closurecap$0.5\n") != NULL &&
        strstr(actual_text, "f$ftag.4 = mov 2\n") != NULL &&
        strstr(actual_text, "eq f$ftag.4, 1\n") != NULL &&
        strstr(actual_text, "shape shape.0() -> int\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "__closure_env_0.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic runtime local zero-arg closure direct-callee mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_runtime_local_zero_arg_void_closure_direct_callee_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int main(){ int c=1; int x=3; int y=5; void f()=closure [x] void () { putint(x); return; }; void g()=closure [y] void () { putint(y); return; }; if((putint(0), c)) f=g; f(); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "tmp.0 = call putint(0)\n") != NULL &&
        strstr(actual_text, "f$closurecap$0.3 = mov x.1\n") != NULL &&
        strstr(actual_text, "g$closurecap$0.5 = mov y.2\n") != NULL &&
        strstr(actual_text, "f$closurecap$0.3 = mov g$closurecap$0.5\n") != NULL &&
        strstr(actual_text, "f$ftag.4 = mov 2\n") != NULL &&
        strstr(actual_text, "eq f$ftag.4, 1\n") != NULL &&
        strstr(actual_text, "shape shape.0() -> void\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "__closure_env_0.") != NULL &&
        strstr(actual_text, "ret 0\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic runtime local zero-arg void closure direct-callee mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_zero_arg_function_value_forwarding_into_function_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int next1(){ return 1; }\n"
            "int next2(){ return 2; }\n"
            "int apply0(int f()){ return f(); }\n"
            "int pick(int c)(){ int g()=next1; if(c) g=next2; return g; }\n"
            "int main(){ int g()=pick(1); return apply0(g); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func pick(__ret0.0, c.1) {\n") != NULL &&
        strstr(actual_text, "g$ftag.2 = mov 1\n") != NULL &&
        strstr(actual_text, "g$ftag.2 = mov 2\n") != NULL &&
        strstr(actual_text, "store_indirect __ret0.0, g$ftag.2\n") != NULL &&
        strstr(actual_text, "tmp.0 = addr_local") != NULL &&
        strstr(actual_text, "tmp.1 = call pick(tmp.0, 1)\n") != NULL &&
        strstr(actual_text, "tmp.2 = eq g$ftag.0, 1\n") != NULL &&
        strstr(actual_text, "shape shape.0() -> int\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_next1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_next2, 0, shape.0\n") != NULL &&
        strstr(actual_text, "func __fnwrap_next1(__env.0) {\n") != NULL &&
        strstr(actual_text, "func __fnwrap_next2(__env.0) {\n") != NULL &&
        strstr(actual_text, "__retfn_declslot") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned zero-arg function-value forwarding mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_returned_closure_forwarding_into_function_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply(int f(int), int x){ return f(x); }\n"
            "int make(int x)(int) { return closure [x] int (int y) { return x + y; }; }\n"
            "int main(){ int f(int)=make(3); return apply(f, 4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply(f.0, x.1)\n") != NULL &&
        strstr(actual_text, "func make(__ret0.0, x.1) {\n") != NULL &&
        strstr(actual_text, "store_indirect __ret0.0, x.1\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call make(") == 1u &&
        strstr(actual_text, "f$ftag.1 = mov 1\n") != NULL &&
        strstr(actual_text, "__closure_env_0.2 = mov f$closurecap$0.0\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_make__retclosure_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_make__retclosure_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: returned closure forwarding into function parameter mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_returned_closure_alias_forwarding_into_function_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply(int f(int), int x){ return f(x); }\n"
            "int make(int x)(int) { return closure [x] int (int y) { return x + y; }; }\n"
            "int main(){ int f(int)=make(3); int g(int)=f; return apply(g, 4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply(f.0, x.1)\n") != NULL &&
        strstr(actual_text, "func make(__ret0.0, x.1) {\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call make(") == 1u &&
        strstr(actual_text, "g$ftag.") != NULL &&
        strstr(actual_text, "g$closurecap$0.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_make__retclosure_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_make__retclosure_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: returned closure alias forwarding into function parameter mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_returned_single_capture_closure_forwarding_into_function_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply(int f(int), int x){ return f(x); }\n"
            "int make(int x)(int) { return closure [x] int (int y) { return x + y; }; }\n"
            "int main(){ int f(int)=make(3); return apply(f, 4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply(f.0, x.1)\n") != NULL &&
        strstr(actual_text, "func make(__ret0.0, x.1) {\n") != NULL &&
        strstr(actual_text, "store_indirect __ret0.0, x.1\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call make(") == 1u &&
        strstr(actual_text, "shape shape.0(int) -> int\n") != NULL &&
        strstr(actual_text, "f$ftag.1 = mov 1\n") != NULL &&
        strstr(actual_text, "__closure_env_0.2 = mov f$closurecap$0.0\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_make__retclosure_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_make__retclosure_") != NULL &&
        strstr(actual_text, "__retclosure_declslot") == NULL &&
        strstr(actual_text, "apply__fv_0_make__retclosure_") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: returned single-capture closure forwarding into function parameter mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_returned_multi_capture_closure_forwarding_into_function_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply(int f(int,int), int a, int b){ return f(a,b); }\n"
            "int make(int x, int y)(int,int) { return closure [x,y] int (int a, int b) { return x + y + a + b; }; }\n"
            "int main(){ return apply(make(3, 4), 5, 6); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply(f.0, a.1, b.2)\n") != NULL &&
        strstr(actual_text, "func make(__ret0.0, __ret1.1, x.2, y.3) {\n") != NULL &&
        strstr(actual_text, "store_indirect __ret0.0, x.2\n") != NULL &&
        strstr(actual_text, "store_indirect __ret1.1, y.3\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call make(") == 1u &&
        strstr(actual_text, "shape shape.0(int, int) -> int\n") != NULL &&
        strstr(actual_text, "__retclosure_argslot_0.0") != NULL &&
        strstr(actual_text, "__retclosure_argslot_1.1") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_make__retclosure_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_make__retclosure_") != NULL &&
        strstr(actual_text, "apply__fv_0_make__retclosure_") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: returned multi-capture closure forwarding into function parameter mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_returned_single_capture_closure_alias_forwarding_into_function_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply(int f(int), int x){ return f(x); }\n"
            "int make(int x)(int) { return closure [x] int (int y) { return x + y; }; }\n"
            "int main(){ int f(int)=make(3); int g(int)=f; return apply(g, 4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply(f.0, x.1)\n") != NULL &&
        strstr(actual_text, "func make(__ret0.0, x.1) {\n") != NULL &&
        strstr(actual_text, "g$ftag.") != NULL &&
        strstr(actual_text, "call make(") != NULL &&
        strstr(actual_text, "shape shape.0(int) -> int\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_make__retclosure_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_make__retclosure_") != NULL &&
        strstr(actual_text, "__retclosure_declslot") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: returned single-capture closure alias forwarding into function parameter mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_returned_single_capture_zero_arg_closure_forwarding_into_function_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply0(int f()){ return f(); }\n"
            "int make(int x)() { return closure [x] int () { return x; }; }\n"
            "int main(){ int f()=make(3); return apply0(f); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply0(f.0)\n") != NULL &&
        strstr(actual_text, "func make(__ret0.0, x.1) {\n") != NULL &&
        strstr(actual_text, "store_indirect __ret0.0, x.1\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call make(") == 1u &&
        strstr(actual_text, "shape shape.0() -> int\n") != NULL &&
        strstr(actual_text, "f$ftag.1 = mov 1\n") != NULL &&
        strstr(actual_text, "__closure_env_0.2 = mov f$closurecap$0.0\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_make__retclosure_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_make__retclosure_") != NULL &&
        strstr(actual_text, "__retclosure_declslot") == NULL &&
        strstr(actual_text, "apply0__fv_0_make__retclosure_") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: returned single-capture zero-arg closure forwarding into function parameter mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_returned_single_capture_zero_arg_void_closure_forwarding_into_function_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "void apply0(void f()){ f(); }\n"
            "void make(int x)() { return closure [x] void () { putint(x); return; }; }\n"
            "int main(){ void f()=make(7); apply0(f); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply0(f.0)\n") != NULL &&
        strstr(actual_text, "func make(__ret0.0, x.1) {\n") != NULL &&
        strstr(actual_text, "store_indirect __ret0.0, x.1\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call make(") == 1u &&
        strstr(actual_text, "shape shape.0() -> void\n") != NULL &&
        strstr(actual_text, "f$ftag.1 = mov 1\n") != NULL &&
        strstr(actual_text, "__closure_env_0.2 = mov f$closurecap$0.0\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_make__retclosure_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_make__retclosure_") != NULL &&
        strstr(actual_text, "__retclosure_declslot") == NULL &&
        strstr(actual_text, "apply0__fv_0_make__retclosure_") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: returned single-capture zero-arg void closure forwarding into function parameter mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_local_closure_forwarding_into_function_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply(int f(int), int x){ return f(x); }\n"
            "int pick(int x, int c)(int) { int f(int)=closure [x] int (int y) { return x + y; }; int g(int)=closure [x] int (int y) { return x - y; }; if(c) f=g; return f; }\n"
            "int main(){ return apply(pick(5, 1), 3); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply(f.0, x.1)\n") != NULL &&
        strstr(actual_text, "func pick(__ret0.0, __ret1.1, x.2, c.3) {\n") != NULL &&
        strstr(actual_text, "store_indirect __ret0.0, f$ftag.5\n") != NULL &&
        strstr(actual_text, "store_indirect __ret1.1, f$closurecap$0.4\n") != NULL &&
        strstr(actual_text, "tmp.2 = call pick(tmp.0, tmp.1, 5, 1)\n") != NULL &&
        strstr(actual_text, "shape shape.0(int) -> int\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call_indirect tmp.6(3)\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.7(3)\n") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "tmp.5 = call pick(") == NULL &&
        strstr(actual_text, "tmp.6 = call pick(") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned local closure forwarding into function parameter mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_direct_dynamic_returned_local_closure_function_value_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply(int f(int), int x){ return f(x); }\n"
            "int pick(int x, int c)(int) { int f(int)=closure [x] int (int y) { return x + y; }; int g(int)=closure [x] int (int y) { return x - y; }; if(c) f=g; return f; }\n"
            "int main(){ return apply(pick(5, 1), 3); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply(f.0, x.1)\n") != NULL &&
        strstr(actual_text, "func pick(__ret0.0, __ret1.1, x.2, c.3) {\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 1u &&
        strstr(actual_text, "__retclosure_argslot_0.0") != NULL &&
        strstr(actual_text, "__retclosure_argslot_1.1") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "apply__fv_0_pick__closure_") == NULL &&
        strstr(actual_text, "__retclosure_declslot") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: direct dynamic returned local closure function-value argument mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_direct_dynamic_returned_local_zero_arg_closure_function_value_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply0(int f()){ return f(); }\n"
            "int pick(int x, int c)(){ int f()=closure [x] int () { return x; }; int g()=closure [x] int () { return x + 1; }; if(c) f=g; return f; }\n"
            "int main(){ return apply0(pick(5, 1)); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply0(f.0)\n") != NULL &&
        strstr(actual_text, "func pick(__ret0.0, __ret1.1, x.2, c.3) {\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 1u &&
        strstr(actual_text, "__retclosure_argslot_0.0") != NULL &&
        strstr(actual_text, "__retclosure_argslot_1.1") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "apply0__fv_0_pick__closure_") == NULL &&
        strstr(actual_text, "__retclosure_declslot") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: direct dynamic returned local zero-arg closure function-value argument mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_direct_dynamic_returned_local_zero_arg_void_closure_function_value_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "void apply0(void f()){ f(); }\n"
            "void pick(int x, int c)(){ void f()=closure [x] void () { putint(x); return; }; void g()=closure [x] void () { putint(x + 1); return; }; if(c) f=g; return f; }\n"
            "int main(){ apply0(pick(5, 1)); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply0(f.0)\n") != NULL &&
        strstr(actual_text, "func pick(__ret0.0, __ret1.1, x.2, c.3) {\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 1u &&
        strstr(actual_text, "__retclosure_argslot_0.0") != NULL &&
        strstr(actual_text, "__retclosure_argslot_1.1") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "apply0__fv_0_pick__closure_") == NULL &&
        strstr(actual_text, "__retclosure_declslot") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: direct dynamic returned local zero-arg void closure function-value argument mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_multi_capture_closure_forwarding_into_function_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply(int f(int,int), int a, int b){ return f(a,b); }\n"
            "int main(){ int x=3; int y=4; int f(int,int)=closure [x,y] int (int a, int b) { return x + y + a + b; }; return apply(f, 5, 6); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply(f.0, a.1, b.2)\n") != NULL &&
        strstr(actual_text, "f$closurecap$0.") != NULL &&
        strstr(actual_text, "f$closurecap$1.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_f_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: multi-capture closure forwarding into function parameter mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_direct_closure_literal_argument_to_function_value_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply(int f(int), int x){ return f(x); }\n"
            "int main(){ int y=3; return apply(closure [y] int (int z) { return y + z; }, 4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply(f.0, x.1)\n") != NULL &&
        strstr(actual_text, "__argclosure_cap_0.") != NULL &&
        strstr(actual_text, "__closure_env_0.") != NULL &&
        strstr(actual_text, "shape shape.0(int) -> int\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure___argclosure_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "func main__closure___argclosure_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure___argclosure_") != NULL &&
        strstr(actual_text, "apply__fv_") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: direct closure literal argument to function-valued parameter mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_zero_arg_direct_closure_literal_argument_to_function_value_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply0(int f()){ return f(); }\n"
            "int main(){ int y=3; return apply0(closure [y] int () { return y; }); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply0(f.0)\n") != NULL &&
        strstr(actual_text, "__argclosure_cap_0.") != NULL &&
        strstr(actual_text, "__closure_env_0.") != NULL &&
        strstr(actual_text, "shape shape.0() -> int\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure___argclosure_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "func main__closure___argclosure_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure___argclosure_") != NULL &&
        strstr(actual_text, "apply0__fv_") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: zero-arg direct closure literal argument to function-valued parameter mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_void_direct_closure_literal_argument_to_function_value_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "void apply0(void f()){ f(); }\n"
            "int main(){ int y=7; apply0(closure [y] void () { putint(y); return; }); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply0(f.0)\n") != NULL &&
        strstr(actual_text, "__argclosure_cap_0.") != NULL &&
        strstr(actual_text, "__closure_env_0.") != NULL &&
        strstr(actual_text, "shape shape.0() -> void\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure___argclosure_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "func main__closure___argclosure_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure___argclosure_") != NULL &&
        strstr(actual_text, "apply0__fv_") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: void direct closure literal argument to function-valued parameter mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_multi_capture_direct_closure_literal_argument_to_function_value_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply(int f(int,int), int a, int b){ return f(a,b); }\n"
            "int main(){ int x=3; int y=4; return apply(closure [x,y] int (int a, int b) { return x + y + a + b; }, 5, 6); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply(f.0, a.1, b.2)\n") != NULL &&
        strstr(actual_text, "__argclosure_cap_0.") != NULL &&
        strstr(actual_text, "__argclosure_cap_1.") != NULL &&
        strstr(actual_text, "__closure_env_0.") != NULL &&
        strstr(actual_text, "__closure_env_1.") != NULL &&
        strstr(actual_text, "shape shape.0(int, int) -> int\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure___argclosure_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "func main__closure___argclosure_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure___argclosure_") != NULL &&
        strstr(actual_text, "apply__fv_") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: multi-capture direct closure literal argument to function-valued parameter mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_closure_alias_chain_callable_object_transport_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply(int f(int), int x){ return f(x); }\n"
            "int apply0(int f()){ return f(); }\n"
            "void apply0v(void f()){ f(); }\n"
            "int main(){ int y=3; int f(int)=closure [y] int (int z) { return y + z; }; int g(int)=f; int h(int)=g; int r1=apply(h, 4); int x=7; int a()=closure [x] int () { return x; }; int b()=a; int r2=apply0(b); int v=9; void p()=closure [v] void () { putint(v); return; }; void q()=p; apply0v(q); return r1+r2; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply(f.0, x.1)\n") != NULL &&
        strstr(actual_text, "declare apply0(f.0)\n") != NULL &&
        strstr(actual_text, "declare apply0v(f.0)\n") != NULL &&
        strstr(actual_text, "shape shape.0(int) -> int\n") != NULL &&
        strstr(actual_text, "shape shape.1() -> int\n") != NULL &&
        strstr(actual_text, "shape shape.2() -> void\n") != NULL &&
        strstr(actual_text, "g$ftag.3 = mov f$ftag.2\n") != NULL &&
        strstr(actual_text, "g$closurecap$0.4 = mov f$closurecap$0.1\n") != NULL &&
        strstr(actual_text, "h$ftag.5 = mov g$ftag.3\n") != NULL &&
        strstr(actual_text, "h$closurecap$0.6 = mov g$closurecap$0.4\n") != NULL &&
        strstr(actual_text, "b$ftag.12 = mov a$ftag.11\n") != NULL &&
        strstr(actual_text, "b$closurecap$0.13 = mov a$closurecap$0.10\n") != NULL &&
        strstr(actual_text, "q$ftag.19 = mov p$ftag.18\n") != NULL &&
        strstr(actual_text, "q$closurecap$0.20 = mov p$closurecap$0.17\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_a_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_p_") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call_indirect tmp.") == 3u &&
        strstr(actual_text, "call apply(") == NULL &&
        strstr(actual_text, "call apply0(") == NULL &&
        strstr(actual_text, "call apply0v(") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: closure alias chain callable-object transport mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_closure_direct_local_call_callable_object_transport_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int main(){ int x=3; int y=4; int z=9; int f(int)=closure [x] int (int a) { return x + a; }; int g()=closure [y] int () { return y; }; int h(int,int)=closure [x,y] int (int a, int b) { return x + y + a + b; }; void p()=closure [z] void () { putint(z); return; }; int r1=f(1); int r2=g(); int r3=h(5,6); p(); return r1 + r2 + r3; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "shape shape.0(int) -> int\n") != NULL &&
        strstr(actual_text, "shape shape.1() -> int\n") != NULL &&
        strstr(actual_text, "shape shape.2(int, int) -> int\n") != NULL &&
        strstr(actual_text, "shape shape.3() -> void\n") != NULL &&
        strstr(actual_text, "f$closurecap$0.3 = mov x.0\n") != NULL &&
        strstr(actual_text, "g$closurecap$0.5 = mov y.1\n") != NULL &&
        strstr(actual_text, "h$closurecap$0.7 = mov x.0\n") != NULL &&
        strstr(actual_text, "h$closurecap$1.8 = mov y.1\n") != NULL &&
        strstr(actual_text, "p$closurecap$0.10 = mov z.2\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_h_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_p_") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call_indirect tmp.") == 4u &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_h_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_p_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: closure direct local call callable-object transport mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_rich_body_closure_direct_local_call_callable_object_transport_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int main(){ int x=3; int y=8; int f(int)=closure [x] int (int a) { putint(x); return x + a; }; int g(int)=closure [y] int (int b) { return (b = b + 1, y + b); }; int r1=f(4); int r2=g(5); return r1 + r2; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "shape shape.0(int) -> int\n") != NULL &&
        strstr(actual_text, "shape shape.1(int) -> int\n") != NULL &&
        strstr(actual_text, "f$closurecap$0.") != NULL &&
        strstr(actual_text, "g$closurecap$0.") != NULL &&
        strstr(actual_text, "tmp.0 = call putint(x.0)\n") != NULL &&
        strstr(actual_text, "b.1 = mov tmp.0\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_g_") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call_indirect tmp.") == 2u &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_g_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: rich-body closure direct local call callable-object transport mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_returned_multi_capture_local_closure_bind_and_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int pick(int x,int y)(int,int) { int f(int,int)=closure [x,y] int (int a, int b) { return x + y + a + b; }; return f; }\n"
            "int main(){ int h(int,int)=pick(3,4); return h(5,6); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "shape shape.0(int, int) -> int\n") != NULL &&
        strstr(actual_text, "store_indirect __ret0.0, f$closurecap$0.") != NULL &&
        strstr(actual_text, "store_indirect __ret1.1, f$closurecap$1.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_pick__closure_f_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: returned multi-capture local closure bind-and-call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_multi_capture_local_closure_forwarding_into_function_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply(int f(int,int), int a, int b){ return f(a,b); }\n"
            "int pick(int x, int y, int c)(int,int) { int f(int,int)=closure [x,y] int (int a, int b) { return x + y + a + b; }; int g(int,int)=closure [x,y] int (int a, int b) { return x + y - a - b; }; if(c) f=g; return f; }\n"
            "int main(){ return apply(pick(5, 7, 1), 3, 2); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply(f.0, a.1, b.2)\n") != NULL &&
        strstr(actual_text, "func pick(__ret0.0, __ret1.1, __ret2.2, x.3, y.4, c.5) {\n") != NULL &&
        strstr(actual_text, "store_indirect __ret0.0, f$ftag.8\n") != NULL &&
        strstr(actual_text, "store_indirect __ret1.1, f$closurecap$0.6\n") != NULL &&
        strstr(actual_text, "store_indirect __ret2.2, f$closurecap$1.7\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_pick__closure_g_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned multi-capture local closure forwarding into function parameter mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_function_value_assignment_then_direct_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int add2(int x){ return x+2; }\n"
            "int main(){ int c=1; int f(int)=add1; int h(int)=add1; if((putint(0), c)) f=add2; h=f; return h(40); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "f$ftag.1 = mov 1\n") != NULL &&
        strstr(actual_text, "h$ftag.2 = mov 1\n") != NULL &&
        strstr(actual_text, "tmp.0 = call putint(0)\n") != NULL &&
        strstr(actual_text, "f$ftag.1 = mov 2\n") != NULL &&
        strstr(actual_text, "h$ftag.2 = mov f$ftag.1\n") != NULL &&
        strstr(actual_text, "tmp.1 = eq h$ftag.2, 1\n") != NULL &&
        strstr(actual_text, "shape shape.0(int) -> int\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add2, 0, shape.0\n") != NULL &&
        strstr(actual_text, "func __fnwrap_add1(__env.0, x.1) {\n") != NULL &&
        strstr(actual_text, "func __fnwrap_add2(__env.0, x.1) {\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic function-value assignment then direct call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_ternary_function_value_assignment_then_direct_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int add2(int x){ return x+2; }\n"
            "int main(){ int c=1; int f(int)=add1; int g(int)=add2; int h(int)=add1; h=((putint(0), c) ? f : g); return h(40); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "tmp.0 = call putint(0)\n") != NULL &&
        strstr(actual_text, "h$ftag.3 = mov 1\n") != NULL &&
        strstr(actual_text, "h$ftag.3 = mov 2\n") != NULL &&
        strstr(actual_text, "tmp.2 = eq h$ftag.3, 1\n") != NULL &&
        strstr(actual_text, "shape shape.0(int) -> int\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add2, 0, shape.0\n") != NULL &&
        strstr(actual_text, "func __fnwrap_add1(__env.0, x.1) {\n") != NULL &&
        strstr(actual_text, "func __fnwrap_add2(__env.0, x.1) {\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: ternary function-value assignment then direct call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_ternary_closure_assignment_then_direct_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int main(){ int c=1; int x=3; int y=5; int f(int)=closure [x] int (int z){ return x+z; }; int g(int)=closure [y] int (int z){ return y+z; }; int h(int)=f; h=((putint(0), c) ? f : g); return h(4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "tmp.0 = call putint(0)\n") != NULL &&
        strstr(actual_text, "f$closurecap$0.3 = mov x.1\n") != NULL &&
        strstr(actual_text, "g$closurecap$0.5 = mov y.2\n") != NULL &&
        strstr(actual_text, "h$ftag.7 = mov 1\n") != NULL &&
        strstr(actual_text, "h$ftag.7 = mov 2\n") != NULL &&
        strstr(actual_text, "h$closurecap$0.") != NULL &&
        strstr(actual_text, "eq h$ftag.7, 1\n") != NULL &&
        strstr(actual_text, "shape shape.0(int) -> int\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_g_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: ternary closure assignment then direct call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_returned_call_ternary_function_value_assignment_then_direct_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int add2(int x){ return x+2; }\n"
            "int pick(int c)(int){ int f(int)=add1; int g(int)=add2; return c ? f : g; }\n"
            "int main(){ int c=getint(); int h(int)=add1; h=((putint(0), c) ? pick(0) : pick(1)); return h(40); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        strstr(actual_text, "call putint(0)\n") != NULL &&
        strstr(actual_text, "h$ftag.") != NULL &&
        strstr(actual_text, "eq h$ftag.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add2, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: returned-call ternary function-value assignment then direct call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_returned_call_ternary_closure_assignment_then_direct_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int pick(int x, int c)(int){ int f(int)=closure [x] int (int z){ return x+z; }; int g(int)=closure [x] int (int z){ return x-z; }; return c ? f : g; }\n"
            "int main(){ int c=getint(); int y=9; int h(int)=closure [y] int (int z){ return y+z; }; h=((putint(0), c) ? pick(5,0) : pick(7,1)); return h(4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        strstr(actual_text, "call putint(0)\n") != NULL &&
        strstr(actual_text, "h$ftag.") != NULL &&
        strstr(actual_text, "h$closurecap$0.") != NULL &&
        strstr(actual_text, "eq h$ftag.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: returned-call ternary closure assignment then direct call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_ternary_function_value_local_initializer_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int add2(int x){ return x+2; }\n"
            "int main(){ int c=1; int f(int)=add1; int g(int)=add2; int h(int)=c ? f : g; return h(40); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "h$ftag.3 = mov 1\n") != NULL &&
        strstr(actual_text, "tmp.0 = ne c.0, 0\n") != NULL &&
        strstr(actual_text, "h$ftag.3 = mov 2\n") != NULL &&
        strstr(actual_text, "tmp.1 = eq h$ftag.3, 1\n") != NULL &&
        strstr(actual_text, "shape shape.0(int) -> int\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add2, 0, shape.0\n") != NULL &&
        strstr(actual_text, "func __fnwrap_add1(__env.0, x.1) {\n") != NULL &&
        strstr(actual_text, "func __fnwrap_add2(__env.0, x.1) {\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: ternary function value local initializer mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_function_value_assignment_then_return_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int add2(int x){ return x+2; }\n"
            "int pick(int c)(int){ int f(int)=add1; int h(int)=add1; if(c) f=add2; h=f; return h; }\n"
            "int main(){ return pick(1)(40); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func pick(__ret0.0, c.1) {\n") != NULL &&
        strstr(actual_text, "f$ftag.2 = mov 1\n") != NULL &&
        strstr(actual_text, "h$ftag.3 = mov 1\n") != NULL &&
        strstr(actual_text, "f$ftag.2 = mov 2\n") != NULL &&
        strstr(actual_text, "h$ftag.3 = mov f$ftag.2\n") != NULL &&
        strstr(actual_text, "store_indirect __ret0.0, h$ftag.3\n") != NULL &&
        strstr(actual_text, "tmp.2 = eq __retfn_immediate_0.0, 1\n") != NULL &&
        strstr(actual_text, "shape shape.0(int) -> int\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add2, 0, shape.0\n") != NULL &&
        strstr(actual_text, "func __fnwrap_add1(__env.0, x.1) {\n") != NULL &&
        strstr(actual_text, "func __fnwrap_add2(__env.0, x.1) {\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic function-value assignment then return mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_returned_closure_assignment_then_return_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int make(int x)(int) { return closure [x] int (int y) { return x + y; }; }\n"
            "int pick()(int){ int f(int)=make(3); int g(int)=make(5); g=f; return g; }\n"
            "int main(){ return pick()(4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func pick(__ret0.0) {\n") != NULL &&
        strstr(actual_text, "call make(") != NULL &&
        strstr(actual_text, "f$closurecap$0.") != NULL &&
        strstr(actual_text, "g$closurecap$0.") != NULL &&
        strstr(actual_text, "g$ftag.") != NULL &&
        strstr(actual_text, "0 = store_indirect __ret0.0, g$closurecap$0.") != NULL &&
        strstr(actual_text, "call pick(tmp.0)\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_make__retclosure_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_make__retclosure_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: returned closure assignment then return mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_returned_function_value_reassignment_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int add2(int x){ return x+2; }\n"
            "int pick(int c)(int){ int f(int)=add1; int g(int)=add2; if(c) f=g; return f; }\n"
            "int main(){ int h(int)=add1; h=pick(1); return h(40); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func pick(__ret0.0, c.1) {\n") != NULL &&
        strstr(actual_text, "h$ftag.0 = mov 1\n") != NULL &&
        strstr(actual_text, "tmp.0 = addr_local local.1\n") != NULL &&
        strstr(actual_text, "tmp.1 = call pick(tmp.0, 1)\n") != NULL &&
        strstr(actual_text, "h$ftag.0 = mov h$ftag.1\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add2, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: returned function-value reassignment mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_returned_zero_arg_function_value_reassignment_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int next1(){ return 1; }\n"
            "int next2(){ return 2; }\n"
            "int pick(int c)(){ int f()=next1; int g()=next2; if(c) f=g; return f; }\n"
            "int main(){ int h()=next1; h=pick(1); return h(); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func pick(__ret0.0, c.1) {\n") != NULL &&
        strstr(actual_text, "h$ftag.0 = mov 1\n") != NULL &&
        strstr(actual_text, "tmp.0 = addr_local local.1\n") != NULL &&
        strstr(actual_text, "tmp.1 = call pick(tmp.0, 1)\n") != NULL &&
        strstr(actual_text, "h$ftag.0 = mov h$ftag.1\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_next1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_next2, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: returned zero-arg function-value reassignment mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_returned_closure_reassignment_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int make(int x)(int){ return closure [x] int (int y){ return x+y; }; }\n"
            "int main(){ int h(int)=make(3); h=make(4); return h(5); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "tmp.0 = addr_local local.0\n") != NULL &&
        strstr(actual_text, "tmp.1 = call make(tmp.0, 3)\n") != NULL &&
        strstr(actual_text, "h$ftag.1 = mov 1\n") != NULL &&
        strstr(actual_text, "tmp.2 = addr_local local.2\n") != NULL &&
        strstr(actual_text, "tmp.3 = call make(tmp.2, 4)\n") != NULL &&
        strstr(actual_text, "h$closurecap$0.0 = mov h$closurecap$0.2\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_make__retclosure_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: returned closure reassignment mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_returned_zero_arg_void_closure_reassignment_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "void make(int x)(){ return closure [x] void (){ putint(x); return; }; }\n"
            "int main(){ void h()=make(3); h=make(4); h(); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "tmp.0 = addr_local local.0\n") != NULL &&
        strstr(actual_text, "tmp.1 = call make(tmp.0, 3)\n") != NULL &&
        strstr(actual_text, "h$ftag.1 = mov 1\n") != NULL &&
        strstr(actual_text, "tmp.2 = addr_local local.2\n") != NULL &&
        strstr(actual_text, "tmp.3 = call make(tmp.2, 4)\n") != NULL &&
        strstr(actual_text, "h$closurecap$0.0 = mov h$closurecap$0.2\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_make__retclosure_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: returned zero-arg void closure reassignment mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_wrapped_function_value_reassignment_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int add2(int x){ return x+2; }\n"
            "int main(){ int f(int)=add1; int g(int)=add2; f=(0, g); return f(40); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "f$ftag.0 = mov 1\n") != NULL &&
        strstr(actual_text, "f$ftag.0 = mov g$ftag.1\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add2, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: wrapped function-value reassignment mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_wrapped_returned_function_value_local_initializer_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int add2(int x){ return x+2; }\n"
            "int pick(int c)(int){ int f(int)=add1; int g(int)=add2; if(c) f=g; return f; }\n"
            "int main(){ int h(int)=(0, pick(1)); return h(40); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "tmp.1 = call pick(tmp.0, 1)\n") != NULL &&
        strstr(actual_text, "eq h$ftag.0, 1\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add2, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: wrapped returned function-value local initializer mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_wrapped_returned_zero_arg_function_value_local_initializer_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int next1(){ return 1; }\n"
            "int next2(){ return 2; }\n"
            "int pick(int c)(){ int f()=next1; int g()=next2; if(c) f=g; return f; }\n"
            "int main(){ int h()=(0, pick(1)); return h(); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "tmp.1 = call pick(tmp.0, 1)\n") != NULL &&
        strstr(actual_text, "eq h$ftag.0, 1\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_next1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_next2, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: wrapped returned zero-arg function-value local initializer mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_assignment_result_returned_closure_local_initializer_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int make(int x)(int){ return closure [x] int (int y){ return x+y; }; }\n"
            "int main(){ int g(int)=make(5); int h(int)=(g=make(4)); return h(5); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call make(") != NULL &&
        strstr(actual_text, "g$closurecap$0.0 = mov g$closurecap$0.2\n") != NULL &&
        strstr(actual_text, "h$ftag.3 = mov g$ftag.1\n") != NULL &&
        strstr(actual_text, "h$closurecap$0.4 = mov g$closurecap$0.0\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_make__retclosure_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: assignment-result returned closure local initializer mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_wrapped_returned_call_ternary_function_value_local_initializer_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int add2(int x){ return x+2; }\n"
            "int pick(int c)(int){ int f(int)=add1; int g(int)=add2; return ((putint(0), c) ? f : g); }\n"
            "int main(){ int h(int)=(0, pick(1)); return h(40); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call putint(0)\n") != NULL &&
        strstr(actual_text, "tmp.1 = call pick(tmp.0, 1)\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add2, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: wrapped returned-call ternary function-value local initializer mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_wrapped_returned_call_ternary_closure_local_initializer_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int pick(int c)(int){ int x=3; int y=5; int f(int)=closure [x] int (int z){ return x+z; }; int g(int)=closure [y] int (int z){ return y+z; }; return ((putint(0), c) ? f : g); }\n"
            "int main(){ int h(int)=(0, pick(1)); return h(4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call putint(0)\n") != NULL &&
        strstr(actual_text, "h$ftag.0 = mov __retclosure_declslot_0.2\n") != NULL &&
        strstr(actual_text, "h$closurecap$0.1 = mov __retclosure_declslot_1.3\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: wrapped returned-call ternary closure local initializer mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_wrapped_returned_call_ternary_function_value_local_bounce_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int add2(int x){ return x+2; }\n"
            "int pick(int c)(int){ int f(int)=add1; int g(int)=add2; return ((putint(0), c) ? f : g); }\n"
            "int main(){ int h(int)=(0, pick(1)); int g(int)=h; return g(40); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call putint(0)\n") != NULL &&
        strstr(actual_text, "g$ftag.1 = mov h$ftag.0\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add2, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: wrapped returned-call ternary function-value local bounce mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_wrapped_returned_call_ternary_function_value_forward_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int add2(int x){ return x+2; }\n"
            "int apply(int f(int), int x){ return f(x); }\n"
            "int pick(int c)(int){ int f(int)=add1; int g(int)=add2; return ((putint(0), c) ? f : g); }\n"
            "int main(){ int h(int)=(0, pick(1)); return apply(h, 40); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call putint(0)\n") != NULL &&
        strstr(actual_text, "call pick(") != NULL &&
        strstr(actual_text, "call apply(") == NULL &&
        strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add2, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: wrapped returned-call ternary function-value forward mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_wrapped_returned_call_ternary_closure_local_bounce_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int pick(int c)(int){ int x=3; int y=5; int f(int)=closure [x] int (int z){ return x+z; }; int g(int)=closure [y] int (int z){ return y+z; }; return ((putint(0), c) ? f : g); }\n"
            "int main(){ int h(int)=(0, pick(1)); int g(int)=h; return g(4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call putint(0)\n") != NULL &&
        strstr(actual_text, "g$ftag.4 = mov h$ftag.0\n") != NULL &&
        strstr(actual_text, "g$closurecap$0.5 = mov h$closurecap$0.1\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: wrapped returned-call ternary closure local bounce mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_wrapped_returned_call_ternary_closure_forward_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply(int f(int), int x){ return f(x); }\n"
            "int pick(int c)(int){ int x=3; int y=5; int f(int)=closure [x] int (int z){ return x+z; }; int g(int)=closure [y] int (int z){ return y+z; }; return ((putint(0), c) ? f : g); }\n"
            "int main(){ int h(int)=(0, pick(1)); return apply(h, 4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call putint(0)\n") != NULL &&
        strstr(actual_text, "call pick(") != NULL &&
        strstr(actual_text, "call apply(") == NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: wrapped returned-call ternary closure forward mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_wrapped_returned_call_ternary_function_value_bind_return_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int add2(int x){ return x+2; }\n"
            "int pick(int c)(int){ int f(int)=add1; int g(int)=add2; return ((putint(0), c) ? f : g); }\n"
            "int wrap()(int){ int h(int)=(0, pick(1)); return h; }\n"
            "int main(){ return wrap()(40); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call pick(") != NULL &&
        strstr(actual_text, "func wrap(__ret0.0) {\n") != NULL &&
        strstr(actual_text, "store_indirect __ret0.0, h$ftag.1\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add2, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: wrapped returned-call ternary function-value bind-return mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_wrapped_returned_call_ternary_function_value_bind_return_actual_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply(int f(int), int x){ return f(x); }\n"
            "int add1(int x){ return x+1; }\n"
            "int add2(int x){ return x+2; }\n"
            "int pick(int c)(int){ int f(int)=add1; int g(int)=add2; return ((putint(0), c) ? f : g); }\n"
            "int wrap()(int){ int h(int)=(0, pick(1)); return h; }\n"
            "int main(){ return apply(wrap(), 40); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call pick(") != NULL &&
        strstr(actual_text, "call wrap(") != NULL &&
        strstr(actual_text, "call apply(") == NULL &&
        strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add2, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: wrapped returned-call ternary function-value bind-return actual mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_wrapped_returned_call_ternary_closure_bind_return_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int pick(int c)(int){ int x=3; int y=5; int f(int)=closure [x] int (int z){ return x+z; }; int g(int)=closure [y] int (int z){ return y+z; }; return ((putint(0), c) ? f : g); }\n"
            "int wrap()(int){ int h(int)=(0, pick(1)); return h; }\n"
            "int main(){ return wrap()(4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call pick(") != NULL &&
        strstr(actual_text, "func wrap(__ret0.0, __ret1.1) {\n") != NULL &&
        strstr(actual_text, "store_indirect __ret0.0, h$ftag.2\n") != NULL &&
        strstr(actual_text, "store_indirect __ret1.1, h$closurecap$0.3\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: wrapped returned-call ternary closure bind-return mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_wrapped_returned_call_ternary_closure_bind_return_actual_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply(int f(int), int x){ return f(x); }\n"
            "int pick(int c)(int){ int x=3; int y=5; int f(int)=closure [x] int (int z){ return x+z; }; int g(int)=closure [y] int (int z){ return y+z; }; return ((putint(0), c) ? f : g); }\n"
            "int wrap()(int){ int h(int)=(0, pick(1)); return h; }\n"
            "int main(){ return apply(wrap(), 4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call pick(") != NULL &&
        strstr(actual_text, "call wrap(") != NULL &&
        strstr(actual_text, "call apply(") == NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: wrapped returned-call ternary closure bind-return actual mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_wrapped_returned_call_ternary_zero_arg_function_value_bind_return_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int next1(){ return 1; }\n"
            "int next2(){ return 2; }\n"
            "int pick(int c)(){ int f()=next1; int g()=next2; return ((putint(0), c) ? f : g); }\n"
            "int wrap()(){ int h()=(0, pick(1)); return h; }\n"
            "int main(){ return wrap()(); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call pick(") != NULL &&
        strstr(actual_text, "func wrap(__ret0.0) {\n") != NULL &&
        strstr(actual_text, "store_indirect __ret0.0, h$ftag.1\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_next1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_next2, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: wrapped returned-call ternary zero-arg function-value bind-return mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_wrapped_returned_call_ternary_zero_arg_function_value_bind_return_actual_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply0(int f()){ return f(); }\n"
            "int next1(){ return 1; }\n"
            "int next2(){ return 2; }\n"
            "int pick(int c)(){ int f()=next1; int g()=next2; return ((putint(0), c) ? f : g); }\n"
            "int wrap()(){ int h()=(0, pick(1)); return h; }\n"
            "int main(){ return apply0(wrap()); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call pick(") != NULL &&
        strstr(actual_text, "call wrap(") != NULL &&
        strstr(actual_text, "call apply0(") == NULL &&
        strstr(actual_text, "fn_make __fnwrap_next1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_next2, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: wrapped returned-call ternary zero-arg function-value bind-return actual mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_function_value_bind_return_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "void ping1(){ putint(1); return; }\n"
            "void ping2(){ putint(2); return; }\n"
            "void pick(int c)(){ void f()=ping1; void g()=ping2; return ((putint(0), c) ? f : g); }\n"
            "void wrap()(){ void h()=(0, pick(1)); return h; }\n"
            "int main(){ wrap()(); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call pick(") != NULL &&
        strstr(actual_text, "func wrap(__ret0.0) {\n") != NULL &&
        strstr(actual_text, "store_indirect __ret0.0, h$ftag.1\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_ping1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_ping2, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: wrapped returned-call ternary zero-arg void function-value bind-return mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_function_value_bind_return_actual_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "void apply0(void f()){ f(); }\n"
            "void ping1(){ putint(1); return; }\n"
            "void ping2(){ putint(2); return; }\n"
            "void pick(int c)(){ void f()=ping1; void g()=ping2; return ((putint(0), c) ? f : g); }\n"
            "void wrap()(){ void h()=(0, pick(1)); return h; }\n"
            "int main(){ apply0(wrap()); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call pick(") != NULL &&
        strstr(actual_text, "call wrap(") != NULL &&
        strstr(actual_text, "call apply0(") == NULL &&
        strstr(actual_text, "fn_make __fnwrap_ping1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_ping2, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: wrapped returned-call ternary zero-arg void function-value bind-return actual mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_wrapped_returned_call_ternary_zero_arg_closure_bind_return_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int pick(int c)(){ int x=3; int y=5; int f()=closure [x] int (){ return x; }; int g()=closure [y] int (){ return y; }; return ((putint(0), c) ? f : g); }\n"
            "int wrap()(){ int h()=(0, pick(1)); return h; }\n"
            "int main(){ return wrap()(); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call pick(") != NULL &&
        strstr(actual_text, "func wrap(__ret0.0, __ret1.1) {\n") != NULL &&
        strstr(actual_text, "store_indirect __ret0.0, h$ftag.2\n") != NULL &&
        strstr(actual_text, "store_indirect __ret1.1, h$closurecap$0.3\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: wrapped returned-call ternary zero-arg closure bind-return mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_wrapped_returned_call_ternary_zero_arg_closure_bind_return_actual_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply0(int f()){ return f(); }\n"
            "int pick(int c)(){ int x=3; int y=5; int f()=closure [x] int (){ return x; }; int g()=closure [y] int (){ return y; }; return ((putint(0), c) ? f : g); }\n"
            "int wrap()(){ int h()=(0, pick(1)); return h; }\n"
            "int main(){ return apply0(wrap()); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call pick(") != NULL &&
        strstr(actual_text, "call wrap(") != NULL &&
        strstr(actual_text, "call apply0(") == NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: wrapped returned-call ternary zero-arg closure bind-return actual mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_closure_bind_return_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "void pick(int c)(){ int x=3; int y=5; void f()=closure [x] void (){ putint(x); return; }; void g()=closure [y] void (){ putint(y); return; }; return ((putint(0), c) ? f : g); }\n"
            "void wrap()(){ void h()=(0, pick(1)); return h; }\n"
            "int main(){ wrap()(); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call pick(") != NULL &&
        strstr(actual_text, "func wrap(__ret0.0, __ret1.1) {\n") != NULL &&
        strstr(actual_text, "store_indirect __ret0.0, h$ftag.2\n") != NULL &&
        strstr(actual_text, "store_indirect __ret1.1, h$closurecap$0.3\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: wrapped returned-call ternary zero-arg void closure bind-return mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_closure_bind_return_actual_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "void apply0(void f()){ f(); }\n"
            "void pick(int c)(){ int x=3; int y=5; void f()=closure [x] void (){ putint(x); return; }; void g()=closure [y] void (){ putint(y); return; }; return ((putint(0), c) ? f : g); }\n"
            "void wrap()(){ void h()=(0, pick(1)); return h; }\n"
            "int main(){ apply0(wrap()); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call pick(") != NULL &&
        strstr(actual_text, "call wrap(") != NULL &&
        strstr(actual_text, "call apply0(") == NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: wrapped returned-call ternary zero-arg void closure bind-return actual mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_wrapped_returned_call_ternary_function_value_bounce_passthrough_bind_return_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int id(int f(int))(int){ return f; }\n"
            "int add1(int x){ return x+1; }\n"
            "int add2(int x){ return x+2; }\n"
            "int pick(int c)(int){ int f(int)=add1; int g(int)=add2; return ((putint(0), c) ? f : g); }\n"
            "int wrap()(int){ int h(int)=(0, pick(1)); int g(int)=h; int p(int)=id(g); return p; }\n"
            "int main(){ return wrap()(40); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call pick(") != NULL &&
        strstr(actual_text, "declare id(__ret0.0, f.1)\n") != NULL &&
        strstr(actual_text, "call id(") == NULL &&
        strstr(actual_text, "g$ftag.2 = mov h$ftag.1\n") != NULL &&
        strstr(actual_text, "p$ftag.3 = mov g$ftag.2\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add2, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: wrapped returned-call ternary function-value bounce passthrough bind-return mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_wrapped_returned_call_ternary_function_value_bounce_passthrough_bind_return_actual_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int id(int f(int))(int){ return f; }\n"
            "int apply(int f(int), int x){ return f(x); }\n"
            "int add1(int x){ return x+1; }\n"
            "int add2(int x){ return x+2; }\n"
            "int pick(int c)(int){ int f(int)=add1; int g(int)=add2; return ((putint(0), c) ? f : g); }\n"
            "int wrap()(int){ int h(int)=(0, pick(1)); int g(int)=h; int p(int)=id(g); return p; }\n"
            "int main(){ return apply(wrap(), 40); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call pick(") != NULL &&
        strstr(actual_text, "declare id(__ret0.0, f.1)\n") != NULL &&
        strstr(actual_text, "call wrap(") != NULL &&
        strstr(actual_text, "call id(") == NULL &&
        strstr(actual_text, "call apply(") == NULL &&
        strstr(actual_text, "p$ftag.3 = mov g$ftag.2\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: wrapped returned-call ternary function-value bounce passthrough bind-return actual mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_wrapped_returned_call_ternary_closure_bounce_passthrough_bind_return_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int id(int f(int))(int){ return f; }\n"
            "int pick(int c)(int){ int x=3; int y=5; int f(int)=closure [x] int (int z){ return x+z; }; int g(int)=closure [y] int (int z){ return y+z; }; return ((putint(0), c) ? f : g); }\n"
            "int wrap()(int){ int h(int)=(0, pick(1)); int g(int)=h; int p(int)=id(g); return p; }\n"
            "int main(){ return wrap()(4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call pick(") != NULL &&
        strstr(actual_text, "declare id(__ret0.0, f.1)\n") != NULL &&
        strstr(actual_text, "call id(") == NULL &&
        strstr(actual_text, "g$ftag.6 = mov h$ftag.2\n") != NULL &&
        strstr(actual_text, "g$closurecap$0.7 = mov h$closurecap$0.3\n") != NULL &&
        strstr(actual_text, "p$ftag.8 = mov g$ftag.6\n") != NULL &&
        strstr(actual_text, "p$closurecap$0.9 = mov g$closurecap$0.7\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: wrapped returned-call ternary closure bounce passthrough bind-return mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_wrapped_returned_call_ternary_closure_bounce_passthrough_bind_return_actual_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int id(int f(int))(int){ return f; }\n"
            "int apply(int f(int), int x){ return f(x); }\n"
            "int pick(int c)(int){ int x=3; int y=5; int f(int)=closure [x] int (int z){ return x+z; }; int g(int)=closure [y] int (int z){ return y+z; }; return ((putint(0), c) ? f : g); }\n"
            "int wrap()(int){ int h(int)=(0, pick(1)); int g(int)=h; int p(int)=id(g); return p; }\n"
            "int main(){ return apply(wrap(), 4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call pick(") != NULL &&
        strstr(actual_text, "declare id(__ret0.0, f.1)\n") != NULL &&
        strstr(actual_text, "call wrap(") != NULL &&
        strstr(actual_text, "call id(") == NULL &&
        strstr(actual_text, "call apply(") == NULL &&
        strstr(actual_text, "p$closurecap$0.9 = mov g$closurecap$0.7\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: wrapped returned-call ternary closure bounce passthrough bind-return actual mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_wrapped_returned_call_ternary_zero_arg_function_value_bounce_passthrough_bind_return_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int id0(int f())(){ return f; }\n"
            "int next1(){ return 1; }\n"
            "int next2(){ return 2; }\n"
            "int pick(int c)(){ int f()=next1; int g()=next2; return ((putint(0), c) ? f : g); }\n"
            "int wrap()(){ int h()=(0, pick(1)); int g()=h; int p()=id0(g); return p; }\n"
            "int main(){ return wrap()(); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call pick(") != NULL &&
        strstr(actual_text, "declare id0(__ret0.0, f.1)\n") != NULL &&
        strstr(actual_text, "call id0(") == NULL &&
        strstr(actual_text, "g$ftag.2 = mov h$ftag.1\n") != NULL &&
        strstr(actual_text, "p$ftag.3 = mov g$ftag.2\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_next1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_next2, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: wrapped returned-call ternary zero-arg function-value bounce passthrough bind-return mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_wrapped_returned_call_ternary_zero_arg_function_value_bounce_passthrough_bind_return_actual_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int id0(int f())(){ return f; }\n"
            "int apply0(int f()){ return f(); }\n"
            "int next1(){ return 1; }\n"
            "int next2(){ return 2; }\n"
            "int pick(int c)(){ int f()=next1; int g()=next2; return ((putint(0), c) ? f : g); }\n"
            "int wrap()(){ int h()=(0, pick(1)); int g()=h; int p()=id0(g); return p; }\n"
            "int main(){ return apply0(wrap()); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call pick(") != NULL &&
        strstr(actual_text, "declare id0(__ret0.0, f.1)\n") != NULL &&
        strstr(actual_text, "call wrap(") != NULL &&
        strstr(actual_text, "call id0(") == NULL &&
        strstr(actual_text, "call apply0(") == NULL &&
        strstr(actual_text, "p$ftag.3 = mov g$ftag.2\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: wrapped returned-call ternary zero-arg function-value bounce passthrough bind-return actual mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_function_value_bounce_passthrough_bind_return_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "void idv(void f())(){ return f; }\n"
            "void ping1(){ putint(1); return; }\n"
            "void ping2(){ putint(2); return; }\n"
            "void pick(int c)(){ void f()=ping1; void g()=ping2; return ((putint(0), c) ? f : g); }\n"
            "void wrap()(){ void h()=(0, pick(1)); void g()=h; void p()=idv(g); return p; }\n"
            "int main(){ wrap()(); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call pick(") != NULL &&
        strstr(actual_text, "declare idv(__ret0.0, f.1)\n") != NULL &&
        strstr(actual_text, "call idv(") == NULL &&
        strstr(actual_text, "g$ftag.2 = mov h$ftag.1\n") != NULL &&
        strstr(actual_text, "p$ftag.3 = mov g$ftag.2\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_ping1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_ping2, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: wrapped returned-call ternary zero-arg void function-value bounce passthrough bind-return mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_function_value_bounce_passthrough_bind_return_actual_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "void idv(void f())(){ return f; }\n"
            "void apply0(void f()){ f(); }\n"
            "void ping1(){ putint(1); return; }\n"
            "void ping2(){ putint(2); return; }\n"
            "void pick(int c)(){ void f()=ping1; void g()=ping2; return ((putint(0), c) ? f : g); }\n"
            "void wrap()(){ void h()=(0, pick(1)); void g()=h; void p()=idv(g); return p; }\n"
            "int main(){ apply0(wrap()); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call pick(") != NULL &&
        strstr(actual_text, "declare idv(__ret0.0, f.1)\n") != NULL &&
        strstr(actual_text, "call wrap(") != NULL &&
        strstr(actual_text, "call idv(") == NULL &&
        strstr(actual_text, "call apply0(") == NULL &&
        strstr(actual_text, "p$ftag.3 = mov g$ftag.2\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: wrapped returned-call ternary zero-arg void function-value bounce passthrough bind-return actual mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_wrapped_returned_call_ternary_zero_arg_closure_bounce_passthrough_bind_return_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int id0(int f())(){ return f; }\n"
            "int pick(int c)(){ int x=3; int y=5; int f()=closure [x] int (){ return x; }; int g()=closure [y] int (){ return y; }; return ((putint(0), c) ? f : g); }\n"
            "int wrap()(){ int h()=(0, pick(1)); int g()=h; int p()=id0(g); return p; }\n"
            "int main(){ return wrap()(); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call pick(") != NULL &&
        strstr(actual_text, "declare id0(__ret0.0, f.1)\n") != NULL &&
        strstr(actual_text, "call id0(") == NULL &&
        strstr(actual_text, "g$ftag.6 = mov h$ftag.2\n") != NULL &&
        strstr(actual_text, "g$closurecap$0.7 = mov h$closurecap$0.3\n") != NULL &&
        strstr(actual_text, "p$ftag.8 = mov g$ftag.6\n") != NULL &&
        strstr(actual_text, "p$closurecap$0.9 = mov g$closurecap$0.7\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: wrapped returned-call ternary zero-arg closure bounce passthrough bind-return mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_wrapped_returned_call_ternary_zero_arg_closure_bounce_passthrough_bind_return_actual_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int id0(int f())(){ return f; }\n"
            "int apply0(int f()){ return f(); }\n"
            "int pick(int c)(){ int x=3; int y=5; int f()=closure [x] int (){ return x; }; int g()=closure [y] int (){ return y; }; return ((putint(0), c) ? f : g); }\n"
            "int wrap()(){ int h()=(0, pick(1)); int g()=h; int p()=id0(g); return p; }\n"
            "int main(){ return apply0(wrap()); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call pick(") != NULL &&
        strstr(actual_text, "declare id0(__ret0.0, f.1)\n") != NULL &&
        strstr(actual_text, "call wrap(") != NULL &&
        strstr(actual_text, "call id0(") == NULL &&
        strstr(actual_text, "call apply0(") == NULL &&
        strstr(actual_text, "p$closurecap$0.9 = mov g$closurecap$0.7\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: wrapped returned-call ternary zero-arg closure bounce passthrough bind-return actual mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_closure_bounce_passthrough_bind_return_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "void idv(void f())(){ return f; }\n"
            "void pick(int c)(){ int x=3; int y=5; void f()=closure [x] void (){ putint(x); return; }; void g()=closure [y] void (){ putint(y); return; }; return ((putint(0), c) ? f : g); }\n"
            "void wrap()(){ void h()=(0, pick(1)); void g()=h; void p()=idv(g); return p; }\n"
            "int main(){ wrap()(); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call pick(") != NULL &&
        strstr(actual_text, "declare idv(__ret0.0, f.1)\n") != NULL &&
        strstr(actual_text, "call idv(") == NULL &&
        strstr(actual_text, "g$ftag.6 = mov h$ftag.2\n") != NULL &&
        strstr(actual_text, "g$closurecap$0.7 = mov h$closurecap$0.3\n") != NULL &&
        strstr(actual_text, "p$ftag.8 = mov g$ftag.6\n") != NULL &&
        strstr(actual_text, "p$closurecap$0.9 = mov g$closurecap$0.7\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: wrapped returned-call ternary zero-arg void closure bounce passthrough bind-return mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_closure_bounce_passthrough_bind_return_actual_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "void idv(void f())(){ return f; }\n"
            "void apply0(void f()){ f(); }\n"
            "void pick(int c)(){ int x=3; int y=5; void f()=closure [x] void (){ putint(x); return; }; void g()=closure [y] void (){ putint(y); return; }; return ((putint(0), c) ? f : g); }\n"
            "void wrap()(){ void h()=(0, pick(1)); void g()=h; void p()=idv(g); return p; }\n"
            "int main(){ apply0(wrap()); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call pick(") != NULL &&
        strstr(actual_text, "declare idv(__ret0.0, f.1)\n") != NULL &&
        strstr(actual_text, "call wrap(") != NULL &&
        strstr(actual_text, "call idv(") == NULL &&
        strstr(actual_text, "call apply0(") == NULL &&
        strstr(actual_text, "p$closurecap$0.9 = mov g$closurecap$0.7\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: wrapped returned-call ternary zero-arg void closure bounce passthrough bind-return actual mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_wrapped_returned_call_ternary_function_value_statement_reassign_bind_return_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int add2(int x){ return x+2; }\n"
            "int pick(int c)(int){ int f(int)=add1; int g(int)=add2; return ((putint(0), c) ? f : g); }\n"
            "int wrap()(int){ int h(int)=(0, pick(1)); int g(int)=h; int p(int)=g; p=h; return p; }\n"
            "int main(){ return wrap()(40); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call pick(") != NULL &&
        strstr(actual_text, "g$ftag.2 = mov h$ftag.1\n") != NULL &&
        strstr(actual_text, "p$ftag.3 = mov g$ftag.2\n") != NULL &&
        strstr(actual_text, "p$ftag.3 = mov h$ftag.1\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add2, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: wrapped returned-call ternary function-value statement reassign bind-return mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_wrapped_returned_call_ternary_function_value_statement_reassign_bind_return_actual_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply(int f(int), int x){ return f(x); }\n"
            "int add1(int x){ return x+1; }\n"
            "int add2(int x){ return x+2; }\n"
            "int pick(int c)(int){ int f(int)=add1; int g(int)=add2; return ((putint(0), c) ? f : g); }\n"
            "int wrap()(int){ int h(int)=(0, pick(1)); int g(int)=h; int p(int)=g; p=h; return p; }\n"
            "int main(){ return apply(wrap(), 40); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call pick(") != NULL &&
        strstr(actual_text, "call wrap(") != NULL &&
        strstr(actual_text, "call apply(") == NULL &&
        strstr(actual_text, "p$ftag.3 = mov h$ftag.1\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: wrapped returned-call ternary function-value statement reassign bind-return actual mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_wrapped_returned_call_ternary_closure_statement_reassign_bind_return_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int pick(int c)(int){ int x=3; int y=5; int f(int)=closure [x] int (int z){ return x+z; }; int g(int)=closure [y] int (int z){ return y+z; }; return ((putint(0), c) ? f : g); }\n"
            "int wrap()(int){ int h(int)=(0, pick(1)); int g(int)=h; int p(int)=g; p=h; return p; }\n"
            "int main(){ return wrap()(4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call pick(") != NULL &&
        strstr(actual_text, "g$ftag.6 = mov h$ftag.2\n") != NULL &&
        strstr(actual_text, "g$closurecap$0.7 = mov h$closurecap$0.3\n") != NULL &&
        strstr(actual_text, "p$ftag.8 = mov g$ftag.6\n") != NULL &&
        strstr(actual_text, "p$closurecap$0.9 = mov g$closurecap$0.7\n") != NULL &&
        strstr(actual_text, "p$closurecap$0.9 = mov h$closurecap$0.3\n") != NULL &&
        strstr(actual_text, "p$ftag.8 = mov h$ftag.2\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: wrapped returned-call ternary closure statement reassign bind-return mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_wrapped_returned_call_ternary_closure_statement_reassign_bind_return_actual_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply(int f(int), int x){ return f(x); }\n"
            "int pick(int c)(int){ int x=3; int y=5; int f(int)=closure [x] int (int z){ return x+z; }; int g(int)=closure [y] int (int z){ return y+z; }; return ((putint(0), c) ? f : g); }\n"
            "int wrap()(int){ int h(int)=(0, pick(1)); int g(int)=h; int p(int)=g; p=h; return p; }\n"
            "int main(){ return apply(wrap(), 4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call pick(") != NULL &&
        strstr(actual_text, "call wrap(") != NULL &&
        strstr(actual_text, "call apply(") == NULL &&
        strstr(actual_text, "p$closurecap$0.9 = mov h$closurecap$0.3\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: wrapped returned-call ternary closure statement reassign bind-return actual mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_wrapped_returned_call_ternary_zero_arg_function_value_statement_reassign_bind_return_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int next1(){ return 1; }\n"
            "int next2(){ return 2; }\n"
            "int pick(int c)(){ int f()=next1; int g()=next2; return ((putint(0), c) ? f : g); }\n"
            "int wrap()(){ int h()=(0, pick(1)); int g()=h; int p()=g; p=h; return p; }\n"
            "int main(){ return wrap()(); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call pick(") != NULL &&
        strstr(actual_text, "g$ftag.2 = mov h$ftag.1\n") != NULL &&
        strstr(actual_text, "p$ftag.3 = mov g$ftag.2\n") != NULL &&
        strstr(actual_text, "p$ftag.3 = mov h$ftag.1\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_next1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_next2, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: wrapped returned-call ternary zero-arg function-value statement reassign bind-return mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_wrapped_returned_call_ternary_zero_arg_function_value_statement_reassign_bind_return_actual_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply0(int f()){ return f(); }\n"
            "int next1(){ return 1; }\n"
            "int next2(){ return 2; }\n"
            "int pick(int c)(){ int f()=next1; int g()=next2; return ((putint(0), c) ? f : g); }\n"
            "int wrap()(){ int h()=(0, pick(1)); int g()=h; int p()=g; p=h; return p; }\n"
            "int main(){ return apply0(wrap()); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call pick(") != NULL &&
        strstr(actual_text, "call wrap(") != NULL &&
        strstr(actual_text, "call apply0(") == NULL &&
        strstr(actual_text, "p$ftag.3 = mov h$ftag.1\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: wrapped returned-call ternary zero-arg function-value statement reassign bind-return actual mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_function_value_statement_reassign_bind_return_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "void ping1(){ putint(1); return; }\n"
            "void ping2(){ putint(2); return; }\n"
            "void pick(int c)(){ void f()=ping1; void g()=ping2; return ((putint(0), c) ? f : g); }\n"
            "void wrap()(){ void h()=(0, pick(1)); void g()=h; void p()=g; p=h; return p; }\n"
            "int main(){ wrap()(); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call pick(") != NULL &&
        strstr(actual_text, "g$ftag.2 = mov h$ftag.1\n") != NULL &&
        strstr(actual_text, "p$ftag.3 = mov g$ftag.2\n") != NULL &&
        strstr(actual_text, "p$ftag.3 = mov h$ftag.1\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_ping1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_ping2, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: wrapped returned-call ternary zero-arg void function-value statement reassign bind-return mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_function_value_statement_reassign_bind_return_actual_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "void apply0(void f()){ f(); }\n"
            "void ping1(){ putint(1); return; }\n"
            "void ping2(){ putint(2); return; }\n"
            "void pick(int c)(){ void f()=ping1; void g()=ping2; return ((putint(0), c) ? f : g); }\n"
            "void wrap()(){ void h()=(0, pick(1)); void g()=h; void p()=g; p=h; return p; }\n"
            "int main(){ apply0(wrap()); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call pick(") != NULL &&
        strstr(actual_text, "call wrap(") != NULL &&
        strstr(actual_text, "call apply0(") == NULL &&
        strstr(actual_text, "p$ftag.3 = mov h$ftag.1\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: wrapped returned-call ternary zero-arg void function-value statement reassign bind-return actual mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_wrapped_returned_call_ternary_zero_arg_closure_statement_reassign_bind_return_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int pick(int c)(){ int x=3; int y=5; int f()=closure [x] int (){ return x; }; int g()=closure [y] int (){ return y; }; return ((putint(0), c) ? f : g); }\n"
            "int wrap()(){ int h()=(0, pick(1)); int g()=h; int p()=g; p=h; return p; }\n"
            "int main(){ return wrap()(); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call pick(") != NULL &&
        strstr(actual_text, "g$ftag.6 = mov h$ftag.2\n") != NULL &&
        strstr(actual_text, "g$closurecap$0.7 = mov h$closurecap$0.3\n") != NULL &&
        strstr(actual_text, "p$ftag.8 = mov g$ftag.6\n") != NULL &&
        strstr(actual_text, "p$closurecap$0.9 = mov g$closurecap$0.7\n") != NULL &&
        strstr(actual_text, "p$closurecap$0.9 = mov h$closurecap$0.3\n") != NULL &&
        strstr(actual_text, "p$ftag.8 = mov h$ftag.2\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: wrapped returned-call ternary zero-arg closure statement reassign bind-return mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_wrapped_returned_call_ternary_zero_arg_closure_statement_reassign_bind_return_actual_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply0(int f()){ return f(); }\n"
            "int pick(int c)(){ int x=3; int y=5; int f()=closure [x] int (){ return x; }; int g()=closure [y] int (){ return y; }; return ((putint(0), c) ? f : g); }\n"
            "int wrap()(){ int h()=(0, pick(1)); int g()=h; int p()=g; p=h; return p; }\n"
            "int main(){ return apply0(wrap()); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call pick(") != NULL &&
        strstr(actual_text, "call wrap(") != NULL &&
        strstr(actual_text, "call apply0(") == NULL &&
        strstr(actual_text, "p$closurecap$0.9 = mov h$closurecap$0.3\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: wrapped returned-call ternary zero-arg closure statement reassign bind-return actual mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_closure_statement_reassign_bind_return_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "void pick(int c)(){ int x=3; int y=5; void f()=closure [x] void (){ putint(x); return; }; void g()=closure [y] void (){ putint(y); return; }; return ((putint(0), c) ? f : g); }\n"
            "void wrap()(){ void h()=(0, pick(1)); void g()=h; void p()=g; p=h; return p; }\n"
            "int main(){ wrap()(); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call pick(") != NULL &&
        strstr(actual_text, "g$ftag.6 = mov h$ftag.2\n") != NULL &&
        strstr(actual_text, "g$closurecap$0.7 = mov h$closurecap$0.3\n") != NULL &&
        strstr(actual_text, "p$ftag.8 = mov g$ftag.6\n") != NULL &&
        strstr(actual_text, "p$closurecap$0.9 = mov g$closurecap$0.7\n") != NULL &&
        strstr(actual_text, "p$closurecap$0.9 = mov h$closurecap$0.3\n") != NULL &&
        strstr(actual_text, "p$ftag.8 = mov h$ftag.2\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: wrapped returned-call ternary zero-arg void closure statement reassign bind-return mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_closure_statement_reassign_bind_return_actual_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "void apply0(void f()){ f(); }\n"
            "void pick(int c)(){ int x=3; int y=5; void f()=closure [x] void (){ putint(x); return; }; void g()=closure [y] void (){ putint(y); return; }; return ((putint(0), c) ? f : g); }\n"
            "void wrap()(){ void h()=(0, pick(1)); void g()=h; void p()=g; p=h; return p; }\n"
            "int main(){ apply0(wrap()); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call pick(") != NULL &&
        strstr(actual_text, "call wrap(") != NULL &&
        strstr(actual_text, "call apply0(") == NULL &&
        strstr(actual_text, "p$closurecap$0.9 = mov h$closurecap$0.3\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: wrapped returned-call ternary zero-arg void closure statement reassign bind-return actual mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_assignment_result_function_value_reassignment_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int add2(int x){ return x+2; }\n"
            "int main(){ int f(int)=add1; int g(int)=add2; int h(int)=add1; f=(h=g); return f(40); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "h$ftag.2 = mov g$ftag.1\n") != NULL &&
        strstr(actual_text, "f$ftag.0 = mov h$ftag.2\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add2, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: assignment-result function-value reassignment mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_wrapped_returned_function_value_reassignment_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int add2(int x){ return x+2; }\n"
            "int pick(int c)(int){ int f(int)=add1; int g(int)=add2; if(c) f=g; return f; }\n"
            "int main(){ int h(int)=add1; h=(0, pick(1)); return h(40); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "tmp.1 = call pick(tmp.0, 1)\n") != NULL &&
        strstr(actual_text, "h$ftag.0 = mov h$ftag.1\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add2, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: wrapped returned function-value reassignment mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_assignment_result_returned_closure_reassignment_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int make(int x)(int){ return closure [x] int (int y){ return x+y; }; }\n"
            "int main(){ int h(int)=make(3); int g(int)=make(5); h=(g=make(4)); return h(5); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call make(") != NULL &&
        strstr(actual_text, "g$closurecap$0.2 = mov g$closurecap$0.4\n") != NULL &&
        strstr(actual_text, "h$closurecap$0.0 = mov g$closurecap$0.2\n") != NULL &&
        strstr(actual_text, "h$ftag.1 = mov g$ftag.3\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_make__retclosure_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: assignment-result returned closure reassignment mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_closure_rebind_then_return_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int pick(int x, int c)(int) { int f(int)=closure [x] int (int y) { return x + y; }; int g(int)=closure [x] int (int y) { return x - y; }; if(c) f=g; return f; }\n"
            "int wrap(int c)(int) { int h(int)=pick(5, c); return h; }\n"
            "int main(){ return wrap(1)(3); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func wrap(__ret0.0, __ret1.1, c.2) {\n") != NULL &&
        strstr(actual_text, "call pick(") != NULL &&
        strstr(actual_text, "store_indirect __ret0.0, h$ftag.") != NULL &&
        strstr(actual_text, "store_indirect __ret1.1, h$closurecap$0.") != NULL &&
        strstr(actual_text, "call wrap(") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "__retclosure_declslot") != NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned closure rebind then return mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_multi_capture_closure_rebind_then_return_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int pick(int x, int y, int c)(int,int) { int f(int,int)=closure [x,y] int (int a,int b){ return x+y+a+b; }; int g(int,int)=closure [x,y] int (int a,int b){ return x+y-a-b; }; if(c) f=g; return f; }\n"
            "int wrap(int c)(int,int) { int h(int,int)=pick(5,7,c); return h; }\n"
            "int main(){ return wrap(1)(3,2); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "shape shape.0(int, int) -> int\n") != NULL &&
        strstr(actual_text, "func wrap(__ret0.0, __ret1.1, __ret2.2, c.3) {\n") != NULL &&
        strstr(actual_text, "call pick(") != NULL &&
        strstr(actual_text, "store_indirect __ret0.0, h$ftag.") != NULL &&
        strstr(actual_text, "store_indirect __ret1.1, h$closurecap$0.") != NULL &&
        strstr(actual_text, "store_indirect __ret2.2, h$closurecap$1.") != NULL &&
        strstr(actual_text, "call wrap(") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "__retclosure_declslot") != NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned multi-capture closure rebind then return mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_zero_arg_void_closure_rebind_then_return_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "void pick(int x, int c)() { void f()=closure [x] void (){ putint(x); return; }; void g()=closure [x] void (){ putint(x+1); return; }; if(c) f=g; return f; }\n"
            "void wrap(int c)() { void h()=pick(5,c); return h; }\n"
            "int main(){ wrap(1)(); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "shape shape.0() -> void\n") != NULL &&
        strstr(actual_text, "func wrap(__ret0.0, __ret1.1, c.2) {\n") != NULL &&
        strstr(actual_text, "call pick(") != NULL &&
        strstr(actual_text, "store_indirect __ret0.0, h$ftag.") != NULL &&
        strstr(actual_text, "store_indirect __ret1.1, h$closurecap$0.") != NULL &&
        strstr(actual_text, "call wrap(") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "__retclosure_declslot") != NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned zero-arg void closure rebind then return mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_closure_passthrough_bind_then_return_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int id(int f(int))(int) { return f; }\n"
            "int pick(int x, int c)(int) { int f(int)=closure [x] int (int y){ return x+y; }; int g(int)=closure [x] int (int y){ return x-y; }; if(c) f=g; return f; }\n"
            "int wrap(int c)(int) { int h(int)=id(pick(5,c)); return h; }\n"
            "int main(){ return wrap(1)(3); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func wrap(__ret0.0, __ret1.1, c.2) {\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 1u &&
        strstr(actual_text, "store_indirect __ret0.0, h$ftag.") != NULL &&
        strstr(actual_text, "store_indirect __ret1.1, h$closurecap$0.") != NULL &&
        strstr(actual_text, "call wrap(") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "call id(") == NULL &&
        strstr(actual_text, "__retclosure_declslot") != NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned closure passthrough bind then return mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_closure_two_hop_passthrough_bind_then_return_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int id(int f(int))(int) { return f; }\n"
            "int id2(int f(int))(int) { return id(f); }\n"
            "int pick(int x, int c)(int) { int f(int)=closure [x] int (int y){ return x+y; }; int g(int)=closure [x] int (int y){ return x-y; }; if(c) f=g; return f; }\n"
            "int wrap(int c)(int) { int h(int)=id2(pick(5,c)); return h; }\n"
            "int main(){ return wrap(1)(3); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare id(__ret0.0, f.1)\n") != NULL &&
        strstr(actual_text, "declare id2(__ret0.0, f.1)\n") != NULL &&
        strstr(actual_text, "func wrap(__ret0.0, __ret1.1, c.2) {\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 1u &&
        ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "store_indirect __ret0.0, h$ftag.") != NULL &&
        strstr(actual_text, "store_indirect __ret1.1, h$closurecap$0.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "call id(") == NULL &&
        strstr(actual_text, "call id2(") == NULL &&
        strstr(actual_text, "__retclosure_declslot") != NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned closure two-hop passthrough bind then return mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_closure_two_hop_passthrough_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int id(int f(int))(int) { return f; }\n"
            "int id2(int f(int))(int) { return id(f); }\n"
            "int pick(int x, int c)(int) { int f(int)=closure [x] int (int y){ return x+y; }; int g(int)=closure [x] int (int y){ return x-y; }; if(c) f=g; return f; }\n"
            "int main(){ return id2(pick(5,1))(3); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare id(__ret0.0, f.1)\n") != NULL &&
        strstr(actual_text, "declare id2(__ret0.0, f.1)\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 1u &&
        strstr(actual_text, "__closure_env_0.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "call id(") == NULL &&
        strstr(actual_text, "call id2(") == NULL &&
        strstr(actual_text, "__retclosure_declslot") == NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned closure two-hop passthrough immediate call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_closure_three_hop_passthrough_bind_then_return_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int id(int f(int))(int) { return f; }\n"
            "int id2(int f(int))(int) { return id(f); }\n"
            "int id3(int f(int))(int) { int g(int)=id2(f); return g; }\n"
            "int pick(int x, int c)(int) { int f(int)=closure [x] int (int y){ return x+y; }; int g(int)=closure [x] int (int y){ return x-y; }; if(c) f=g; return f; }\n"
            "int wrap(int c)(int) { int h(int)=id3(pick(5,c)); return h; }\n"
            "int main(){ return wrap(1)(3); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare id(__ret0.0, f.1)\n") != NULL &&
        strstr(actual_text, "declare id2(__ret0.0, f.1)\n") != NULL &&
        strstr(actual_text, "declare id3(__ret0.0, f.1)\n") != NULL &&
        strstr(actual_text, "func wrap(__ret0.0, __ret1.1, c.2) {\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 1u &&
        ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "store_indirect __ret0.0, h$ftag.") != NULL &&
        strstr(actual_text, "store_indirect __ret1.1, h$closurecap$0.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "call id(") == NULL &&
        strstr(actual_text, "call id2(") == NULL &&
        strstr(actual_text, "call id3(") == NULL &&
        strstr(actual_text, "__retclosure_declslot_0.") != NULL &&
        strstr(actual_text, "__retclosure_declslot_1.") != NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned closure three-hop passthrough bind then return mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_closure_three_hop_passthrough_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int id(int f(int))(int) { return f; }\n"
            "int id2(int f(int))(int) { return id(f); }\n"
            "int id3(int f(int))(int) { int g(int)=id2(f); return g; }\n"
            "int pick(int x, int c)(int) { int f(int)=closure [x] int (int y){ return x+y; }; int g(int)=closure [x] int (int y){ return x-y; }; if(c) f=g; return f; }\n"
            "int main(){ return id3(pick(5,1))(3); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare id(__ret0.0, f.1)\n") != NULL &&
        strstr(actual_text, "declare id2(__ret0.0, f.1)\n") != NULL &&
        strstr(actual_text, "declare id3(__ret0.0, f.1)\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 1u &&
        strstr(actual_text, "__closure_env_0.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "call id(") == NULL &&
        strstr(actual_text, "call id2(") == NULL &&
        strstr(actual_text, "call id3(") == NULL &&
        strstr(actual_text, "__retclosure_declslot") == NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned closure three-hop passthrough immediate call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_closure_three_hop_passthrough_actual_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply(int f(int), int x){ return f(x); }\n"
            "int id(int f(int))(int) { return f; }\n"
            "int id2(int f(int))(int) { return id(f); }\n"
            "int id3(int f(int))(int) { int g(int)=id2(f); return g; }\n"
            "int pick(int x, int c)(int) { int f(int)=closure [x] int (int y){ return x+y; }; int g(int)=closure [x] int (int y){ return x-y; }; if(c) f=g; return f; }\n"
            "int main(){ return apply(id3(pick(5,1)), 3); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply(f.0, x.1)\n") != NULL &&
        strstr(actual_text, "declare id(__ret0.0, f.1)\n") != NULL &&
        strstr(actual_text, "declare id2(__ret0.0, f.1)\n") != NULL &&
        strstr(actual_text, "declare id3(__ret0.0, f.1)\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 1u &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "call apply(") == NULL &&
        strstr(actual_text, "call id(") == NULL &&
        strstr(actual_text, "call id2(") == NULL &&
        strstr(actual_text, "call id3(") == NULL &&
        strstr(actual_text, "__retclosure_declslot") == NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned closure three-hop passthrough actual argument mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_closure_local_bounce_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int pick(int x, int c)(int) { int f(int)=closure [x] int (int y){ return x+y; }; int g(int)=closure [x] int (int y){ return x-y; }; if(c) f=g; return f; }\n"
            "int wrap(int c)(int) { int h(int)=pick(5,c); int k(int)=h; return k; }\n"
            "int main(){ return wrap(1)(3); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func wrap(__ret0.0, __ret1.1, c.2) {\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 1u &&
        ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "store_indirect __ret0.0, k$ftag.") != NULL &&
        strstr(actual_text, "store_indirect __ret1.1, k$closurecap$0.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "__retclosure_declslot_0.") != NULL &&
        strstr(actual_text, "__retclosure_declslot_1.") != NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned closure local bounce immediate call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_closure_local_bounce_actual_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply(int f(int), int x){ return f(x); }\n"
            "int pick(int x, int c)(int) { int f(int)=closure [x] int (int y){ return x+y; }; int g(int)=closure [x] int (int y){ return x-y; }; if(c) f=g; return f; }\n"
            "int wrap(int c)(int) { int h(int)=pick(5,c); int k(int)=h; return k; }\n"
            "int main(){ return apply(wrap(1), 3); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply(f.0, x.1)\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 1u &&
        ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "call apply(") == NULL &&
        strstr(actual_text, "__retclosure_declslot") != NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned closure local bounce actual argument mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_closure_producer_ternary_merge_bind_and_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int pick(int x, int c)(int) { int f(int)=closure [x] int (int y){ return x+y; }; int g(int)=closure [x] int (int y){ return x-y; }; if(c) f=g; return f; }\n"
            "int wrap(int c, int d)(int) { int h(int)=pick(5,c); int k(int)=pick(7,c); int m(int)=d ? h : k; return m; }\n"
            "int main(){ int f(int)=wrap(1,0); return f(3); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func wrap(__ret0.0, __ret1.1, c.2, d.3) {\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "store_indirect __ret0.0, m$ftag.") != NULL &&
        strstr(actual_text, "store_indirect __ret1.1, m$closurecap$0.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "__retclosure_declslot") != NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned closure producer ternary merge bind-and-call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_closure_producer_ternary_merge_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int pick(int x, int c)(int) { int f(int)=closure [x] int (int y){ return x+y; }; int g(int)=closure [x] int (int y){ return x-y; }; if(c) f=g; return f; }\n"
            "int wrap(int c, int d)(int) { int h(int)=pick(5,c); int k(int)=pick(7,c); int m(int)=d ? h : k; return m; }\n"
            "int main(){ return wrap(1,0)(3); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func wrap(__ret0.0, __ret1.1, c.2, d.3) {\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "store_indirect __ret0.0, m$ftag.") != NULL &&
        strstr(actual_text, "store_indirect __ret1.1, m$closurecap$0.") != NULL &&
        strstr(actual_text, "__retclosure_immediate_0.") != NULL &&
        strstr(actual_text, "__retclosure_immediate_1.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "__retclosure_declslot") != NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned closure producer ternary merge immediate call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_closure_returned_call_ternary_merge_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int id(int f(int))(int){ return f; }\n"
            "int pick(int x, int c)(int){ int f(int)=closure [x] int (int y){ return x+y; }; int g(int)=closure [x] int (int y){ return x-y; }; if(c) f=g; return f; }\n"
            "int main(){ int c=1; int h(int)= c ? id(pick(5,1)) : id(pick(7,0)); return h(3); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare id(__ret0.0, f.1)\n") != NULL &&
        strstr(actual_text, "func main() {\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        strstr(actual_text, "h$ftag.") != NULL &&
        strstr(actual_text, "h$closurecap$0.") != NULL &&
        strstr(actual_text, "__retclosure_ternaryslot_0.") != NULL &&
        strstr(actual_text, "__retclosure_ternaryslot_1.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "call id(") == NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned closure returned-call ternary merge immediate call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_closure_returned_call_ternary_merge_local_bounce_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int id(int f(int))(int){ return f; }\n"
            "int pick(int x, int c)(int){ int f(int)=closure [x] int (int y){ return x+y; }; int g(int)=closure [x] int (int y){ return x-y; }; if(c) f=g; return f; }\n"
            "int main(){ int c=1; int h(int)= c ? id(pick(5,1)) : id(pick(7,0)); int g(int)=h; return g(3); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare id(__ret0.0, f.1)\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        strstr(actual_text, "h$ftag.") != NULL &&
        strstr(actual_text, "h$closurecap$0.") != NULL &&
        strstr(actual_text, "g$ftag.") != NULL &&
        strstr(actual_text, "g$closurecap$0.") != NULL &&
        strstr(actual_text, "g$ftag.7 = mov h$ftag.2\n") != NULL &&
        strstr(actual_text, "g$closurecap$0.8 = mov h$closurecap$0.1\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "call id(") == NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned closure returned-call ternary merge local bounce immediate call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_closure_returned_call_ternary_merge_bind_return_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int id(int f(int))(int){ return f; }\n"
            "int pick(int x, int c)(int){ int f(int)=closure [x] int (int y){ return x+y; }; int g(int)=closure [x] int (int y){ return x-y; }; if(c) f=g; return f; }\n"
            "int wrap(int c)(int){ int h(int)= c ? id(pick(5,1)) : id(pick(7,0)); return h; }\n"
            "int main(){ return wrap(1)(3); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare id(__ret0.0, f.1)\n") != NULL &&
        strstr(actual_text, "func wrap(__ret0.0, __ret1.1, c.2) {\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "store_indirect __ret0.0, h$ftag.") != NULL &&
        strstr(actual_text, "store_indirect __ret1.1, h$closurecap$0.") != NULL &&
        strstr(actual_text, "__retclosure_immediate_0.") != NULL &&
        strstr(actual_text, "__retclosure_immediate_1.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "call id(") == NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned closure returned-call ternary merge bind return immediate call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_closure_returned_call_ternary_merge_bind_return_actual_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply(int f(int), int x){ return f(x); }\n"
            "int id(int f(int))(int){ return f; }\n"
            "int pick(int x, int c)(int){ int f(int)=closure [x] int (int y){ return x+y; }; int g(int)=closure [x] int (int y){ return x-y; }; if(c) f=g; return f; }\n"
            "int wrap(int c)(int){ int h(int)= c ? id(pick(5,1)) : id(pick(7,0)); return h; }\n"
            "int main(){ return apply(wrap(1), 3); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply(f.0, x.1)\n") != NULL &&
        strstr(actual_text, "declare id(__ret0.0, f.1)\n") != NULL &&
        strstr(actual_text, "func wrap(__ret0.0, __ret1.1, c.2) {\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "store_indirect __ret0.0, h$ftag.") != NULL &&
        strstr(actual_text, "store_indirect __ret1.1, h$closurecap$0.") != NULL &&
        strstr(actual_text, "__retclosure_argslot_0.") != NULL &&
        strstr(actual_text, "__retclosure_argslot_1.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "call apply(") == NULL &&
        strstr(actual_text, "call id(") == NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned closure returned-call ternary merge bind return actual argument mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_closure_returned_call_ternary_merge_bounce_passthrough_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int id(int f(int))(int){ return f; }\n"
            "int pick(int x, int c)(int){ int f(int)=closure [x] int (int y){ return x+y; }; int g(int)=closure [x] int (int y){ return x-y; }; if(c) f=g; return f; }\n"
            "int main(){ int c=1; int h(int)= c ? id(pick(5,1)) : id(pick(7,0)); int g(int)=h; int p(int)=id(g); return p(3); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare id(__ret0.0, f.1)\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        strstr(actual_text, "g$ftag.") != NULL &&
        strstr(actual_text, "g$closurecap$0.") != NULL &&
        strstr(actual_text, "p$ftag.") != NULL &&
        strstr(actual_text, "p$closurecap$0.") != NULL &&
        strstr(actual_text, "p$ftag.9 = mov g$ftag.7\n") != NULL &&
        strstr(actual_text, "p$closurecap$0.10 = mov g$closurecap$0.8\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "call id(") == NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned closure returned-call ternary merge bounce passthrough immediate call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_closure_returned_call_ternary_merge_bounce_passthrough_bind_return_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int id(int f(int))(int){ return f; }\n"
            "int pick(int x, int c)(int){ int f(int)=closure [x] int (int y){ return x+y; }; int g(int)=closure [x] int (int y){ return x-y; }; if(c) f=g; return f; }\n"
            "int wrap(int c)(int){ int h(int)= c ? id(pick(5,1)) : id(pick(7,0)); int g(int)=h; int p(int)=id(g); return p; }\n"
            "int main(){ return wrap(1)(3); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare id(__ret0.0, f.1)\n") != NULL &&
        strstr(actual_text, "func wrap(__ret0.0, __ret1.1, c.2) {\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "g$ftag.") != NULL &&
        strstr(actual_text, "g$closurecap$0.") != NULL &&
        strstr(actual_text, "p$ftag.") != NULL &&
        strstr(actual_text, "p$closurecap$0.") != NULL &&
        strstr(actual_text, "store_indirect __ret0.0, p$ftag.") != NULL &&
        strstr(actual_text, "store_indirect __ret1.1, p$closurecap$0.") != NULL &&
        strstr(actual_text, "__retclosure_immediate_0.") != NULL &&
        strstr(actual_text, "__retclosure_immediate_1.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "call id(") == NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned closure returned-call ternary merge bounce passthrough bind return immediate call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_closure_returned_call_ternary_merge_bounce_passthrough_bind_return_actual_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply(int f(int), int x){ return f(x); }\n"
            "int id(int f(int))(int){ return f; }\n"
            "int pick(int x, int c)(int){ int f(int)=closure [x] int (int y){ return x+y; }; int g(int)=closure [x] int (int y){ return x-y; }; if(c) f=g; return f; }\n"
            "int wrap(int c)(int){ int h(int)= c ? id(pick(5,1)) : id(pick(7,0)); int g(int)=h; int p(int)=id(g); return p; }\n"
            "int main(){ return apply(wrap(1), 3); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply(f.0, x.1)\n") != NULL &&
        strstr(actual_text, "declare id(__ret0.0, f.1)\n") != NULL &&
        strstr(actual_text, "func wrap(__ret0.0, __ret1.1, c.2) {\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "__retclosure_argslot_0.") != NULL &&
        strstr(actual_text, "__retclosure_argslot_1.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "call apply(") == NULL &&
        strstr(actual_text, "call id(") == NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned closure returned-call ternary merge bounce passthrough bind return actual argument mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_closure_returned_call_ternary_merge_deeper_chain_bind_return_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int id(int f(int))(int){ return f; }\n"
            "int pick(int x, int c)(int){ int f(int)=closure [x] int (int y){ return x+y; }; int g(int)=closure [x] int (int y){ return x-y; }; if(c) f=g; return f; }\n"
            "int wrap(int c)(int){ int h(int)= c ? id(pick(5,1)) : id(pick(7,0)); int g(int)=h; int p(int)=id(g); int q(int)=p; q=h; return q; }\n"
            "int main(){ return wrap(1)(3); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare id(__ret0.0, f.1)\n") != NULL &&
        strstr(actual_text, "func wrap(__ret0.0, __ret1.1, c.2) {\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "g$ftag.") != NULL &&
        strstr(actual_text, "g$closurecap$0.") != NULL &&
        strstr(actual_text, "p$ftag.") != NULL &&
        strstr(actual_text, "p$closurecap$0.") != NULL &&
        strstr(actual_text, "q$ftag.") != NULL &&
        strstr(actual_text, "q$closurecap$0.") != NULL &&
        strstr(actual_text, "q$closurecap$0.14 = mov h$closurecap$0.3\n") != NULL &&
        strstr(actual_text, "q$ftag.13 = mov h$ftag.4\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "call id(") == NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned closure returned-call ternary merge deeper chain bind return immediate call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_closure_producer_ternary_merge_actual_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply(int f(int), int x){ return f(x); }\n"
            "int pick(int x, int c)(int) { int f(int)=closure [x] int (int y){ return x+y; }; int g(int)=closure [x] int (int y){ return x-y; }; if(c) f=g; return f; }\n"
            "int wrap(int c, int d)(int) { int h(int)=pick(5,c); int k(int)=pick(7,c); int m(int)=d ? h : k; return m; }\n"
            "int main(){ return apply(wrap(1,0), 3); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply(f.0, x.1)\n") != NULL &&
        strstr(actual_text, "func wrap(__ret0.0, __ret1.1, c.2, d.3) {\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "store_indirect __ret0.0, m$ftag.") != NULL &&
        strstr(actual_text, "store_indirect __ret1.1, m$closurecap$0.") != NULL &&
        strstr(actual_text, "__retclosure_argslot_0.") != NULL &&
        strstr(actual_text, "__retclosure_argslot_1.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "call apply(") == NULL &&
        strstr(actual_text, "__retclosure_declslot") != NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned closure producer ternary merge actual argument mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_closure_producer_ternary_merge_local_bounce_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int pick(int x, int c)(int) { int f(int)=closure [x] int (int y) { return x + y; }; int g(int)=closure [x] int (int y) { return x - y; }; if(c) f=g; return f; }\n"
            "int wrap(int c, int d)(int) { int h(int)=pick(5,c); int k(int)=pick(7,c); int m(int)=d ? h : k; int n(int)=m; return n; }\n"
            "int main(){ return wrap(1,0)(3); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func wrap(__ret0.0, __ret1.1, c.2, d.3) {\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "m$ftag.") != NULL &&
        strstr(actual_text, "m$closurecap$0.") != NULL &&
        strstr(actual_text, "n$ftag.") != NULL &&
        strstr(actual_text, "n$closurecap$0.") != NULL &&
        strstr(actual_text, "n$ftag.14 = mov m$ftag.13\n") != NULL &&
        strstr(actual_text, "n$closurecap$0.15 = mov m$closurecap$0.12\n") != NULL &&
        strstr(actual_text, "__retclosure_immediate_0.") != NULL &&
        strstr(actual_text, "__retclosure_immediate_1.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned closure producer ternary merge local bounce immediate call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_closure_producer_ternary_merge_local_bounce_actual_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply(int f(int), int x){ return f(x); }\n"
            "int pick(int x, int c)(int) { int f(int)=closure [x] int (int y) { return x + y; }; int g(int)=closure [x] int (int y) { return x - y; }; if(c) f=g; return f; }\n"
            "int wrap(int c, int d)(int) { int h(int)=pick(5,c); int k(int)=pick(7,c); int m(int)=d ? h : k; int n(int)=m; return n; }\n"
            "int main(){ return apply(wrap(1,0), 3); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply(f.0, x.1)\n") != NULL &&
        strstr(actual_text, "func wrap(__ret0.0, __ret1.1, c.2, d.3) {\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "n$ftag.") != NULL &&
        strstr(actual_text, "n$closurecap$0.") != NULL &&
        strstr(actual_text, "n$ftag.14 = mov m$ftag.13\n") != NULL &&
        strstr(actual_text, "n$closurecap$0.15 = mov m$closurecap$0.12\n") != NULL &&
        strstr(actual_text, "__retclosure_argslot_0.") != NULL &&
        strstr(actual_text, "__retclosure_argslot_1.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "call apply(") == NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned closure producer ternary merge local bounce actual argument mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_closure_producer_ternary_merge_local_bounce_passthrough_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int id(int f(int))(int){ return f; }\n"
            "int pick(int x, int c)(int) { int f(int)=closure [x] int (int y) { return x + y; }; int g(int)=closure [x] int (int y) { return x - y; }; if(c) f=g; return f; }\n"
            "int wrap(int c, int d)(int) { int h(int)=pick(5,c); int k(int)=pick(7,c); int m(int)=d ? h : k; int n(int)=m; int p(int)=id(n); return p; }\n"
            "int main(){ return wrap(1,0)(3); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare id(__ret0.0, f.1)\n") != NULL &&
        strstr(actual_text, "func wrap(__ret0.0, __ret1.1, c.2, d.3) {\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "n$ftag.") != NULL &&
        strstr(actual_text, "n$closurecap$0.") != NULL &&
        strstr(actual_text, "p$ftag.") != NULL &&
        strstr(actual_text, "p$closurecap$0.") != NULL &&
        strstr(actual_text, "p$ftag.16 = mov n$ftag.14\n") != NULL &&
        strstr(actual_text, "p$closurecap$0.17 = mov n$closurecap$0.15\n") != NULL &&
        strstr(actual_text, "__retclosure_immediate_0.") != NULL &&
        strstr(actual_text, "__retclosure_immediate_1.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned closure producer ternary merge local bounce passthrough immediate call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_closure_producer_ternary_merge_local_bounce_passthrough_actual_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply(int f(int), int x){ return f(x); }\n"
            "int id(int f(int))(int){ return f; }\n"
            "int pick(int x, int c)(int) { int f(int)=closure [x] int (int y) { return x + y; }; int g(int)=closure [x] int (int y) { return x - y; }; if(c) f=g; return f; }\n"
            "int wrap(int c, int d)(int) { int h(int)=pick(5,c); int k(int)=pick(7,c); int m(int)=d ? h : k; int n(int)=m; int p(int)=id(n); return p; }\n"
            "int main(){ return apply(wrap(1,0), 3); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply(f.0, x.1)\n") != NULL &&
        strstr(actual_text, "declare id(__ret0.0, f.1)\n") != NULL &&
        strstr(actual_text, "func wrap(__ret0.0, __ret1.1, c.2, d.3) {\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "p$ftag.") != NULL &&
        strstr(actual_text, "p$closurecap$0.") != NULL &&
        strstr(actual_text, "p$ftag.16 = mov n$ftag.14\n") != NULL &&
        strstr(actual_text, "p$closurecap$0.17 = mov n$closurecap$0.15\n") != NULL &&
        strstr(actual_text, "__retclosure_argslot_0.") != NULL &&
        strstr(actual_text, "__retclosure_argslot_1.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "call apply(") == NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned closure producer ternary merge local bounce passthrough actual argument mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_closure_producer_ternary_merge_local_reassign_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int pick(int x, int c)(int) { int f(int)=closure [x] int (int y){ return x+y; }; int g(int)=closure [x] int (int y){ return x-y; }; if(c) f=g; return f; }\n"
            "int wrap(int c, int d)(int) { int h(int)=pick(5,c); int k(int)=pick(7,c); int m(int)=d ? h : k; int n(int)=m; int p(int)=n; p=m; return p; }\n"
            "int main(){ return wrap(1,0)(3); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func wrap(__ret0.0, __ret1.1, c.2, d.3) {\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "n$ftag.14 = mov m$ftag.13\n") != NULL &&
        strstr(actual_text, "n$closurecap$0.15 = mov m$closurecap$0.12\n") != NULL &&
        strstr(actual_text, "p$ftag.16 = mov n$ftag.14\n") != NULL &&
        strstr(actual_text, "p$closurecap$0.17 = mov n$closurecap$0.15\n") != NULL &&
        strstr(actual_text, "p$ftag.16 = mov m$ftag.13\n") != NULL &&
        strstr(actual_text, "p$closurecap$0.17 = mov m$closurecap$0.12\n") != NULL &&
        strstr(actual_text, "__retclosure_immediate_0.") != NULL &&
        strstr(actual_text, "__retclosure_immediate_1.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned closure producer ternary merge local reassign immediate call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_closure_producer_ternary_merge_local_reassign_actual_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply(int f(int), int x){ return f(x); }\n"
            "int pick(int x, int c)(int) { int f(int)=closure [x] int (int y){ return x+y; }; int g(int)=closure [x] int (int y){ return x-y; }; if(c) f=g; return f; }\n"
            "int wrap(int c, int d)(int) { int h(int)=pick(5,c); int k(int)=pick(7,c); int m(int)=d ? h : k; int n(int)=m; int p(int)=n; p=m; return p; }\n"
            "int main(){ return apply(wrap(1,0), 3); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply(f.0, x.1)\n") != NULL &&
        strstr(actual_text, "func wrap(__ret0.0, __ret1.1, c.2, d.3) {\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "p$ftag.16 = mov n$ftag.14\n") != NULL &&
        strstr(actual_text, "p$closurecap$0.17 = mov n$closurecap$0.15\n") != NULL &&
        strstr(actual_text, "p$ftag.16 = mov m$ftag.13\n") != NULL &&
        strstr(actual_text, "p$closurecap$0.17 = mov m$closurecap$0.12\n") != NULL &&
        strstr(actual_text, "__retclosure_argslot_0.") != NULL &&
        strstr(actual_text, "__retclosure_argslot_1.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "call apply(") == NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned closure producer ternary merge local reassign actual argument mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_closure_producer_ternary_merge_returned_call_reassign_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int id(int f(int))(int){ return f; }\n"
            "int pick(int x, int c)(int) { int f(int)=closure [x] int (int y){ return x+y; }; int g(int)=closure [x] int (int y){ return x-y; }; if(c) f=g; return f; }\n"
            "int wrap(int c, int d)(int) { int h(int)=pick(5,c); int k(int)=pick(7,c); int m(int)=d ? h : k; m = id(m); return m; }\n"
            "int main(){ return wrap(1,0)(3); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare id(__ret0.0, f.1)\n") != NULL &&
        strstr(actual_text, "func wrap(__ret0.0, __ret1.1, c.2, d.3) {\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "m$closurecap$0.12 = mov m$closurecap$0.12\n") != NULL &&
        strstr(actual_text, "m$ftag.13 = mov m$ftag.13\n") != NULL &&
        strstr(actual_text, "__retclosure_immediate_0.") != NULL &&
        strstr(actual_text, "__retclosure_immediate_1.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "id__fv_0_") == NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned closure producer ternary merge returned-call reassign immediate call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_closure_producer_ternary_merge_returned_call_reassign_actual_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply(int f(int), int x){ return f(x); }\n"
            "int id(int f(int))(int){ return f; }\n"
            "int pick(int x, int c)(int) { int f(int)=closure [x] int (int y){ return x+y; }; int g(int)=closure [x] int (int y){ return x-y; }; if(c) f=g; return f; }\n"
            "int wrap(int c, int d)(int) { int h(int)=pick(5,c); int k(int)=pick(7,c); int m(int)=d ? h : k; m = id(m); return m; }\n"
            "int main(){ return apply(wrap(1,0), 3); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply(f.0, x.1)\n") != NULL &&
        strstr(actual_text, "declare id(__ret0.0, f.1)\n") != NULL &&
        strstr(actual_text, "func wrap(__ret0.0, __ret1.1, c.2, d.3) {\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "m$closurecap$0.12 = mov m$closurecap$0.12\n") != NULL &&
        strstr(actual_text, "m$ftag.13 = mov m$ftag.13\n") != NULL &&
        strstr(actual_text, "__retclosure_argslot_0.") != NULL &&
        strstr(actual_text, "__retclosure_argslot_1.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "call apply(") == NULL &&
        strstr(actual_text, "id__fv_0_") == NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned closure producer ternary merge returned-call reassign actual argument mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int pick(int x, int c)() { int f()=closure [x] int () { return x; }; int g()=closure [x] int () { return x + 1; }; if(c) f=g; return f; }\n"
            "int wrap(int c, int d)() { int h()=pick(5,c); int k()=pick(7,c); int m()=d ? h : k; return m; }\n"
            "int main(){ return wrap(1,0)(); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "shape shape.0() -> int\n") != NULL &&
        strstr(actual_text, "func wrap(__ret0.0, __ret1.1, c.2, d.3) {\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "store_indirect __ret0.0, m$ftag.") != NULL &&
        strstr(actual_text, "store_indirect __ret1.1, m$closurecap$0.") != NULL &&
        strstr(actual_text, "__retclosure_immediate_0.") != NULL &&
        strstr(actual_text, "__retclosure_immediate_1.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned zero-arg closure producer ternary merge immediate call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_actual_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply0(int f()){ return f(); }\n"
            "int pick(int x, int c)() { int f()=closure [x] int () { return x; }; int g()=closure [x] int () { return x + 1; }; if(c) f=g; return f; }\n"
            "int wrap(int c, int d)() { int h()=pick(5,c); int k()=pick(7,c); int m()=d ? h : k; return m; }\n"
            "int main(){ return apply0(wrap(1,0)); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "shape shape.0() -> int\n") != NULL &&
        strstr(actual_text, "declare apply0(f.0)\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "__retclosure_argslot_0.") != NULL &&
        strstr(actual_text, "__retclosure_argslot_1.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "call apply0(") == NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned zero-arg closure producer ternary merge actual argument mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "void pick(int x, int c)() { void f()=closure [x] void () { putint(x); return; }; void g()=closure [x] void () { putint(x + 1); return; }; if(c) f=g; return f; }\n"
            "void wrap(int c, int d)() { void h()=pick(5,c); void k()=pick(7,c); void m()=d ? h : k; return m; }\n"
            "int main(){ wrap(1,0)(); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "shape shape.0() -> void\n") != NULL &&
        strstr(actual_text, "declare putint(param0.0)\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "__retclosure_immediate_0.") != NULL &&
        strstr(actual_text, "__retclosure_immediate_1.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned zero-arg void closure producer ternary merge immediate call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_actual_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "void apply0(void f()){ f(); }\n"
            "void pick(int x, int c)() { void f()=closure [x] void () { putint(x); return; }; void g()=closure [x] void () { putint(x + 1); return; }; if(c) f=g; return f; }\n"
            "void wrap(int c, int d)() { void h()=pick(5,c); void k()=pick(7,c); void m()=d ? h : k; return m; }\n"
            "int main(){ apply0(wrap(1,0)); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "shape shape.0() -> void\n") != NULL &&
        strstr(actual_text, "declare apply0(f.0)\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "__retclosure_argslot_0.") != NULL &&
        strstr(actual_text, "__retclosure_argslot_1.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "call apply0(") == NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned zero-arg void closure producer ternary merge actual argument mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_local_bounce_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int pick(int x, int c)() { int f()=closure [x] int () { return x; }; int g()=closure [x] int () { return x + 1; }; if(c) f=g; return f; }\n"
            "int wrap(int c, int d)() { int h()=pick(5,c); int k()=pick(7,c); int m()=d ? h : k; int n()=m; return n; }\n"
            "int main(){ return wrap(1,0)(); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "n$ftag.") != NULL &&
        strstr(actual_text, "n$closurecap$0.") != NULL &&
        strstr(actual_text, "n$ftag.14 = mov m$ftag.13\n") != NULL &&
        strstr(actual_text, "n$closurecap$0.15 = mov m$closurecap$0.12\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned zero-arg closure producer ternary merge local bounce immediate call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_local_bounce_actual_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply0(int f()){ return f(); }\n"
            "int pick(int x, int c)() { int f()=closure [x] int () { return x; }; int g()=closure [x] int () { return x + 1; }; if(c) f=g; return f; }\n"
            "int wrap(int c, int d)() { int h()=pick(5,c); int k()=pick(7,c); int m()=d ? h : k; int n()=m; return n; }\n"
            "int main(){ return apply0(wrap(1,0)); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply0(f.0)\n") != NULL &&
        strstr(actual_text, "n$ftag.") != NULL &&
        strstr(actual_text, "n$closurecap$0.") != NULL &&
        strstr(actual_text, "__retclosure_argslot_0.") != NULL &&
        strstr(actual_text, "__retclosure_argslot_1.") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "call apply0(") == NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned zero-arg closure producer ternary merge local bounce actual argument mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_local_bounce_passthrough_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int id0(int f())(){ return f; }\n"
            "int pick(int x, int c)() { int f()=closure [x] int () { return x; }; int g()=closure [x] int () { return x + 1; }; if(c) f=g; return f; }\n"
            "int wrap(int c, int d)() { int h()=pick(5,c); int k()=pick(7,c); int m()=d ? h : k; int n()=m; int p()=id0(n); return p; }\n"
            "int main(){ return wrap(1,0)(); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare id0(__ret0.0, f.1)\n") != NULL &&
        strstr(actual_text, "p$ftag.") != NULL &&
        strstr(actual_text, "p$closurecap$0.") != NULL &&
        strstr(actual_text, "p$ftag.16 = mov n$ftag.14\n") != NULL &&
        strstr(actual_text, "p$closurecap$0.17 = mov n$closurecap$0.15\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "call id0(") == NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned zero-arg closure producer ternary merge local bounce passthrough immediate call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_local_bounce_passthrough_actual_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply0(int f()){ return f(); }\n"
            "int id0(int f())(){ return f; }\n"
            "int pick(int x, int c)() { int f()=closure [x] int () { return x; }; int g()=closure [x] int () { return x + 1; }; if(c) f=g; return f; }\n"
            "int wrap(int c, int d)() { int h()=pick(5,c); int k()=pick(7,c); int m()=d ? h : k; int n()=m; int p()=id0(n); return p; }\n"
            "int main(){ return apply0(wrap(1,0)); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply0(f.0)\n") != NULL &&
        strstr(actual_text, "declare id0(__ret0.0, f.1)\n") != NULL &&
        strstr(actual_text, "p$ftag.") != NULL &&
        strstr(actual_text, "p$closurecap$0.") != NULL &&
        strstr(actual_text, "__retclosure_argslot_0.") != NULL &&
        strstr(actual_text, "__retclosure_argslot_1.") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "call apply0(") == NULL &&
        strstr(actual_text, "call id0(") == NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned zero-arg closure producer ternary merge local bounce passthrough actual argument mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_local_reassign_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int pick(int x, int c)() { int f()=closure [x] int () { return x; }; int g()=closure [x] int () { return x + 1; }; if(c) f=g; return f; }\n"
            "int wrap(int c, int d)() { int h()=pick(5,c); int k()=pick(7,c); int m()=d ? h : k; int n()=m; int p()=n; p=m; return p; }\n"
            "int main(){ return wrap(1,0)(); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "p$ftag.16 = mov n$ftag.14\n") != NULL &&
        strstr(actual_text, "p$closurecap$0.17 = mov n$closurecap$0.15\n") != NULL &&
        strstr(actual_text, "p$ftag.16 = mov m$ftag.13\n") != NULL &&
        strstr(actual_text, "p$closurecap$0.17 = mov m$closurecap$0.12\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned zero-arg closure producer ternary merge local reassign immediate call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_local_reassign_actual_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply0(int f()){ return f(); }\n"
            "int pick(int x, int c)() { int f()=closure [x] int () { return x; }; int g()=closure [x] int () { return x + 1; }; if(c) f=g; return f; }\n"
            "int wrap(int c, int d)() { int h()=pick(5,c); int k()=pick(7,c); int m()=d ? h : k; int n()=m; int p()=n; p=m; return p; }\n"
            "int main(){ return apply0(wrap(1,0)); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply0(f.0)\n") != NULL &&
        strstr(actual_text, "__retclosure_argslot_0.") != NULL &&
        strstr(actual_text, "__retclosure_argslot_1.") != NULL &&
        strstr(actual_text, "p$ftag.16 = mov n$ftag.14\n") != NULL &&
        strstr(actual_text, "p$closurecap$0.17 = mov n$closurecap$0.15\n") != NULL &&
        strstr(actual_text, "p$ftag.16 = mov m$ftag.13\n") != NULL &&
        strstr(actual_text, "p$closurecap$0.17 = mov m$closurecap$0.12\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "call apply0(") == NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned zero-arg closure producer ternary merge local reassign actual argument mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_local_bounce_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "void pick(int x, int c)() { void f()=closure [x] void () { putint(x); return; }; void g()=closure [x] void () { putint(x + 1); return; }; if(c) f=g; return f; }\n"
            "void wrap(int c, int d)() { void h()=pick(5,c); void k()=pick(7,c); void m()=d ? h : k; void n()=m; return n; }\n"
            "int main(){ wrap(1,0)(); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "n$ftag.") != NULL &&
        strstr(actual_text, "n$closurecap$0.") != NULL &&
        strstr(actual_text, "n$ftag.14 = mov m$ftag.13\n") != NULL &&
        strstr(actual_text, "n$closurecap$0.15 = mov m$closurecap$0.12\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned zero-arg void closure producer ternary merge local bounce immediate call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_local_bounce_actual_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "void apply0(void f()){ f(); }\n"
            "void pick(int x, int c)() { void f()=closure [x] void () { putint(x); return; }; void g()=closure [x] void () { putint(x + 1); return; }; if(c) f=g; return f; }\n"
            "void wrap(int c, int d)() { void h()=pick(5,c); void k()=pick(7,c); void m()=d ? h : k; void n()=m; return n; }\n"
            "int main(){ apply0(wrap(1,0)); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply0(f.0)\n") != NULL &&
        strstr(actual_text, "n$ftag.") != NULL &&
        strstr(actual_text, "n$closurecap$0.") != NULL &&
        strstr(actual_text, "__retclosure_argslot_0.") != NULL &&
        strstr(actual_text, "__retclosure_argslot_1.") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "call apply0(") == NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned zero-arg void closure producer ternary merge local bounce actual argument mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_local_bounce_passthrough_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "void idv(void f())(){ return f; }\n"
            "void pick(int x, int c)() { void f()=closure [x] void () { putint(x); return; }; void g()=closure [x] void () { putint(x + 1); return; }; if(c) f=g; return f; }\n"
            "void wrap(int c, int d)() { void h()=pick(5,c); void k()=pick(7,c); void m()=d ? h : k; void n()=m; void p()=idv(n); return p; }\n"
            "int main(){ wrap(1,0)(); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare idv(__ret0.0, f.1)\n") != NULL &&
        strstr(actual_text, "p$ftag.") != NULL &&
        strstr(actual_text, "p$closurecap$0.") != NULL &&
        strstr(actual_text, "p$ftag.16 = mov n$ftag.14\n") != NULL &&
        strstr(actual_text, "p$closurecap$0.17 = mov n$closurecap$0.15\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "call idv(") == NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned zero-arg void closure producer ternary merge local bounce passthrough immediate call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_local_bounce_passthrough_actual_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "void apply0(void f()){ f(); }\n"
            "void idv(void f())(){ return f; }\n"
            "void pick(int x, int c)() { void f()=closure [x] void () { putint(x); return; }; void g()=closure [x] void () { putint(x + 1); return; }; if(c) f=g; return f; }\n"
            "void wrap(int c, int d)() { void h()=pick(5,c); void k()=pick(7,c); void m()=d ? h : k; void n()=m; void p()=idv(n); return p; }\n"
            "int main(){ apply0(wrap(1,0)); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply0(f.0)\n") != NULL &&
        strstr(actual_text, "declare idv(__ret0.0, f.1)\n") != NULL &&
        strstr(actual_text, "p$ftag.") != NULL &&
        strstr(actual_text, "p$closurecap$0.") != NULL &&
        strstr(actual_text, "__retclosure_argslot_0.") != NULL &&
        strstr(actual_text, "__retclosure_argslot_1.") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "call apply0(") == NULL &&
        strstr(actual_text, "call idv(") == NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned zero-arg void closure producer ternary merge local bounce passthrough actual argument mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_local_reassign_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "void pick(int x, int c)() { void f()=closure [x] void () { putint(x); return; }; void g()=closure [x] void () { putint(x + 1); return; }; if(c) f=g; return f; }\n"
            "void wrap(int c, int d)() { void h()=pick(5,c); void k()=pick(7,c); void m()=d ? h : k; void n()=m; void p()=n; p=m; return p; }\n"
            "int main(){ wrap(1,0)(); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "p$ftag.16 = mov n$ftag.14\n") != NULL &&
        strstr(actual_text, "p$closurecap$0.17 = mov n$closurecap$0.15\n") != NULL &&
        strstr(actual_text, "p$ftag.16 = mov m$ftag.13\n") != NULL &&
        strstr(actual_text, "p$closurecap$0.17 = mov m$closurecap$0.12\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned zero-arg void closure producer ternary merge local reassign immediate call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_local_reassign_actual_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "void apply0(void f()){ f(); }\n"
            "void pick(int x, int c)() { void f()=closure [x] void () { putint(x); return; }; void g()=closure [x] void () { putint(x + 1); return; }; if(c) f=g; return f; }\n"
            "void wrap(int c, int d)() { void h()=pick(5,c); void k()=pick(7,c); void m()=d ? h : k; void n()=m; void p()=n; p=m; return p; }\n"
            "int main(){ apply0(wrap(1,0)); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply0(f.0)\n") != NULL &&
        strstr(actual_text, "__retclosure_argslot_0.") != NULL &&
        strstr(actual_text, "__retclosure_argslot_1.") != NULL &&
        strstr(actual_text, "p$ftag.16 = mov n$ftag.14\n") != NULL &&
        strstr(actual_text, "p$closurecap$0.17 = mov n$closurecap$0.15\n") != NULL &&
        strstr(actual_text, "p$ftag.16 = mov m$ftag.13\n") != NULL &&
        strstr(actual_text, "p$closurecap$0.17 = mov m$closurecap$0.12\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "call apply0(") == NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned zero-arg void closure producer ternary merge local reassign actual argument mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_returned_call_reassign_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int id0(int f())(){ return f; }\n"
            "int pick(int x, int c)() { int f()=closure [x] int () { return x; }; int g()=closure [x] int () { return x + 1; }; if(c) f=g; return f; }\n"
            "int wrap(int c, int d)() { int h()=pick(5,c); int k()=pick(7,c); int m()=d ? h : k; m = id0(m); return m; }\n"
            "int main(){ return wrap(1,0)(); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare id0(__ret0.0, f.1)\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "m$closurecap$0.12 = mov m$closurecap$0.12\n") != NULL &&
        strstr(actual_text, "m$ftag.13 = mov m$ftag.13\n") != NULL &&
        strstr(actual_text, "call id0(") == NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "__retclosure_immediate_0.") != NULL &&
        strstr(actual_text, "__retclosure_immediate_1.") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned zero-arg closure producer ternary merge returned-call reassign immediate call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_returned_call_reassign_actual_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply0(int f()){ return f(); }\n"
            "int id0(int f())(){ return f; }\n"
            "int pick(int x, int c)() { int f()=closure [x] int () { return x; }; int g()=closure [x] int () { return x + 1; }; if(c) f=g; return f; }\n"
            "int wrap(int c, int d)() { int h()=pick(5,c); int k()=pick(7,c); int m()=d ? h : k; m = id0(m); return m; }\n"
            "int main(){ return apply0(wrap(1,0)); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply0(f.0)\n") != NULL &&
        strstr(actual_text, "declare id0(__ret0.0, f.1)\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "m$closurecap$0.12 = mov m$closurecap$0.12\n") != NULL &&
        strstr(actual_text, "m$ftag.13 = mov m$ftag.13\n") != NULL &&
        strstr(actual_text, "call apply0(") == NULL &&
        strstr(actual_text, "call id0(") == NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "__retclosure_argslot_0.") != NULL &&
        strstr(actual_text, "__retclosure_argslot_1.") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned zero-arg closure producer ternary merge returned-call reassign actual argument mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_returned_call_reassign_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "void idv(void f())(){ return f; }\n"
            "void pick(int x, int c)() { void f()=closure [x] void () { putint(x); return; }; void g()=closure [x] void () { putint(x + 1); return; }; if(c) f=g; return f; }\n"
            "void wrap(int c, int d)() { void h()=pick(5,c); void k()=pick(7,c); void m()=d ? h : k; m = idv(m); return m; }\n"
            "int main(){ wrap(1,0)(); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare idv(__ret0.0, f.1)\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "m$closurecap$0.12 = mov m$closurecap$0.12\n") != NULL &&
        strstr(actual_text, "m$ftag.13 = mov m$ftag.13\n") != NULL &&
        strstr(actual_text, "call idv(") == NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "__retclosure_immediate_0.") != NULL &&
        strstr(actual_text, "__retclosure_immediate_1.") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned zero-arg void closure producer ternary merge returned-call reassign immediate call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_returned_call_reassign_actual_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "void apply0(void f()){ f(); }\n"
            "void idv(void f())(){ return f; }\n"
            "void pick(int x, int c)() { void f()=closure [x] void () { putint(x); return; }; void g()=closure [x] void () { putint(x + 1); return; }; if(c) f=g; return f; }\n"
            "void wrap(int c, int d)() { void h()=pick(5,c); void k()=pick(7,c); void m()=d ? h : k; m = idv(m); return m; }\n"
            "int main(){ apply0(wrap(1,0)); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply0(f.0)\n") != NULL &&
        strstr(actual_text, "declare idv(__ret0.0, f.1)\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "m$closurecap$0.12 = mov m$closurecap$0.12\n") != NULL &&
        strstr(actual_text, "m$ftag.13 = mov m$ftag.13\n") != NULL &&
        strstr(actual_text, "call apply0(") == NULL &&
        strstr(actual_text, "call idv(") == NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "__retclosure_argslot_0.") != NULL &&
        strstr(actual_text, "__retclosure_argslot_1.") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned zero-arg void closure producer ternary merge returned-call reassign actual argument mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_zero_arg_closure_statement_passthrough_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int id0(int f())(){ return f; }\n"
            "int pick(int x, int c)() { int f()=closure [x] int () { return x; }; int g()=closure [x] int () { return x + 1; }; if(c) f=g; return f; }\n"
            "int main(){ int m()=pick(5,1); id0(m); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare id0(__ret0.0, f.1)\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 1u &&
        strstr(actual_text, "m$ftag.0 = mov __retclosure_declslot_0.2\n") != NULL &&
        strstr(actual_text, "m$closurecap$0.1 = mov __retclosure_declslot_1.3\n") != NULL &&
        strstr(actual_text, "call id0(") == NULL &&
        strstr(actual_text, "id0__fv_0_") == NULL &&
        strstr(actual_text, "call_indirect") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned zero-arg closure statement passthrough call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_zero_arg_void_closure_statement_passthrough_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "void idv(void f())(){ return f; }\n"
            "void pick(int x, int c)() { void f()=closure [x] void () { putint(x); return; }; void g()=closure [x] void () { putint(x + 1); return; }; if(c) f=g; return f; }\n"
            "int main(){ void m()=pick(5,1); idv(m); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare idv(__ret0.0, f.1)\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 1u &&
        strstr(actual_text, "m$ftag.0 = mov __retclosure_declslot_0.2\n") != NULL &&
        strstr(actual_text, "m$closurecap$0.1 = mov __retclosure_declslot_1.3\n") != NULL &&
        strstr(actual_text, "call idv(") == NULL &&
        strstr(actual_text, "idv__fv_0_") == NULL &&
        strstr(actual_text, "call_indirect") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned zero-arg void closure statement passthrough call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_returned_closure_statement_passthrough_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int id(int f(int))(int){ return f; }\n"
            "int pick(int x, int c)(int){ int f(int)=closure [x] int (int y){ return x+y; }; int g(int)=closure [x] int (int y){ return x-y; }; if(c) f=g; return f; }\n"
            "int main(){ int m(int)=pick(5,1); id(m); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare id(__ret0.0, f.1)\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 1u &&
        strstr(actual_text, "m$ftag.0 = mov __retclosure_declslot_0.2\n") != NULL &&
        strstr(actual_text, "m$closurecap$0.1 = mov __retclosure_declslot_1.3\n") != NULL &&
        strstr(actual_text, "call id(") == NULL &&
        strstr(actual_text, "id__fv_0_") == NULL &&
        strstr(actual_text, "call_indirect") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic returned closure statement passthrough call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_passthrough_ternary_closure_local_initializer_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int id(int f(int))(int){ return f; }\n"
            "int main(){ int c=1; int x=3; int y=5; int f(int)=closure [x] int (int z){ return x+z; }; int g(int)=closure [y] int (int z){ return y+z; }; int h(int)= c ? id(f) : id(g); return h(4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare id(__ret0.0, f.1)\n") != NULL &&
        strstr(actual_text, "h$ftag.") != NULL &&
        strstr(actual_text, "h$closurecap$0.") != NULL &&
        strstr(actual_text, "mov f$closurecap$0.") != NULL &&
        strstr(actual_text, "mov g$closurecap$0.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "call id(") == NULL &&
        strstr(actual_text, "id__fv_0_") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: passthrough ternary closure local initializer mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_passthrough_ternary_noncapturing_function_value_return_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int id(int f(int))(int){ return f; }\n"
            "int add1(int x){ return x+1; }\n"
            "int add2(int x){ return x+2; }\n"
            "int pick(int c)(int){ int f(int)=add1; int g(int)=add2; return c ? id(f) : id(g); }\n"
            "int main(){ return pick(1)(40); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare id(__ret0.0, f.1)\n") != NULL &&
        strstr(actual_text, "func pick(__ret0.0, c.1) {\n") != NULL &&
        strstr(actual_text, "store_indirect __ret0.0, 1\n") != NULL &&
        strstr(actual_text, "store_indirect __ret0.0, 2\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 1u &&
        strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add2, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "call id(") == NULL &&
        strstr(actual_text, "id__fv_0_") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: passthrough ternary noncapturing function-value return immediate call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_passthrough_ternary_noncapturing_function_value_actual_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int id(int f(int))(int){ return f; }\n"
            "int apply(int f(int), int x){ return f(x); }\n"
            "int add1(int x){ return x+1; }\n"
            "int add2(int x){ return x+2; }\n"
            "int main(){ int c=1; int f(int)=add1; int g(int)=add2; return apply(c ? id(f) : id(g), 40); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare id(__ret0.0, f.1)\n") != NULL &&
        strstr(actual_text, "declare apply(f.0, x.1)\n") != NULL &&
        strstr(actual_text, "__ternary_fn_argtag.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add2, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "call id(") == NULL &&
        strstr(actual_text, "call apply(") == NULL &&
        strstr(actual_text, "id__fv_0_") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: passthrough ternary noncapturing function-value actual argument mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_passthrough_ternary_closure_function_value_actual_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int id(int f(int))(int){ return f; }\n"
            "int apply(int f(int), int x){ return f(x); }\n"
            "int main(){ int c=1; int x=3; int y=5; int f(int)=closure [x] int (int z){ return x+z; }; int g(int)=closure [y] int (int z){ return y+z; }; return apply(c ? id(f) : id(g), 4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare id(__ret0.0, f.1)\n") != NULL &&
        strstr(actual_text, "declare apply(f.0, x.1)\n") != NULL &&
        strstr(actual_text, "__ternary_fn_argtag.") != NULL &&
        strstr(actual_text, "__ternary_fn_argcap_0.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "call id(") == NULL &&
        strstr(actual_text, "call apply(") == NULL &&
        strstr(actual_text, "id__fv_0_") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: passthrough ternary closure function-value actual argument mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_passthrough_ternary_zero_arg_function_value_actual_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int id0(int f())(){ return f; }\n"
            "int apply0(int f()){ return f(); }\n"
            "int next1(){ return 1; }\n"
            "int next2(){ return 2; }\n"
            "int main(){ int c=1; int f()=next1; int g()=next2; return apply0(c ? id0(f) : id0(g)); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare id0(__ret0.0, f.1)\n") != NULL &&
        strstr(actual_text, "declare apply0(f.0)\n") != NULL &&
        strstr(actual_text, "__ternary_fn_argtag.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_next1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_next2, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "call id0(") == NULL &&
        strstr(actual_text, "call apply0(") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: passthrough ternary zero-arg function-value actual argument mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_passthrough_ternary_zero_arg_void_function_value_actual_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "void idv(void f())(){ return f; }\n"
            "void apply0(void f()){ f(); }\n"
            "void ping1(){ putint(1); return; }\n"
            "void ping2(){ putint(2); return; }\n"
            "int main(){ int c=1; void f()=ping1; void g()=ping2; apply0(c ? idv(f) : idv(g)); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare idv(__ret0.0, f.1)\n") != NULL &&
        strstr(actual_text, "declare apply0(f.0)\n") != NULL &&
        strstr(actual_text, "__ternary_fn_argtag.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_ping1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_ping2, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "call idv(") == NULL &&
        strstr(actual_text, "call apply0(") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: passthrough ternary zero-arg void function-value actual argument mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_passthrough_ternary_zero_arg_closure_function_value_actual_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int id0(int f())(){ return f; }\n"
            "int apply0(int f()){ return f(); }\n"
            "int main(){ int c=1; int x=3; int y=5; int f()=closure [x] int () { return x; }; int g()=closure [y] int () { return y; }; return apply0(c ? id0(f) : id0(g)); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare id0(__ret0.0, f.1)\n") != NULL &&
        strstr(actual_text, "declare apply0(f.0)\n") != NULL &&
        strstr(actual_text, "__ternary_fn_argtag.") != NULL &&
        strstr(actual_text, "__ternary_fn_argcap_0.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "call id0(") == NULL &&
        strstr(actual_text, "call apply0(") == NULL &&
        strstr(actual_text, "apply0__fv_0_") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: passthrough ternary zero-arg closure function-value actual argument mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_passthrough_ternary_zero_arg_void_closure_function_value_actual_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "void idv(void f())(){ return f; }\n"
            "void apply0(void f()){ f(); }\n"
            "int main(){ int c=1; int x=3; int y=5; void f()=closure [x] void () { putint(x); return; }; void g()=closure [y] void () { putint(y); return; }; apply0(c ? idv(f) : idv(g)); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare idv(__ret0.0, f.1)\n") != NULL &&
        strstr(actual_text, "declare apply0(f.0)\n") != NULL &&
        strstr(actual_text, "__ternary_fn_argtag.") != NULL &&
        strstr(actual_text, "__ternary_fn_argcap_0.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "call idv(") == NULL &&
        strstr(actual_text, "call apply0(") == NULL &&
        strstr(actual_text, "apply0__fv_0_") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: passthrough ternary zero-arg void closure function-value actual argument mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_wrapped_function_value_return_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int add2(int x){ return x+2; }\n"
            "int pick()(int){ int f(int)=add1; int h(int)=add1; return (h=f); }\n"
            "int main(){ return pick()(41); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "h$ftag.2 = mov 1\n") != NULL &&
        strstr(actual_text, "h$ftag.2 = mov f$ftag.1\n") != NULL &&
        strstr(actual_text, "store_indirect __ret0.0, h$ftag.2\n") != NULL &&
        strstr(actual_text, "shape shape.0(int) -> int\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "func __fnwrap_add1(__env.0, x.1) {\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: wrapped function-value return mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_wrapped_function_value_return_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int add2(int x){ return x+2; }\n"
            "int pick(int c)(int){ int f(int)=add1; int h(int)=add1; if(c) f=add2; return (h=f); }\n"
            "int main(){ return pick(1)(40); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "h$ftag.3 = mov 1\n") != NULL &&
        strstr(actual_text, "f$ftag.2 = mov 2\n") != NULL &&
        strstr(actual_text, "h$ftag.3 = mov f$ftag.2\n") != NULL &&
        strstr(actual_text, "store_indirect __ret0.0, h$ftag.3\n") != NULL &&
        strstr(actual_text, "shape shape.0(int) -> int\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add2, 0, shape.0\n") != NULL &&
        strstr(actual_text, "func __fnwrap_add1(__env.0, x.1) {\n") != NULL &&
        strstr(actual_text, "func __fnwrap_add2(__env.0, x.1) {\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic wrapped function-value return mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_ternary_noncapturing_function_value_return_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int add2(int x){ return x+2; }\n"
            "int pick(int c)(int){ int f(int)=add1; int g(int)=add2; return ((putint(0), c) ? f : g); }\n"
            "int main(){ return pick(1)(40); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "tmp.0 = call putint(0)\n") != NULL &&
        strstr(actual_text, "store_indirect __ret0.0, 1\n") != NULL &&
        strstr(actual_text, "store_indirect __ret0.0, 2\n") != NULL &&
        strstr(actual_text, "shape shape.0(int) -> int\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add2, 0, shape.0\n") != NULL &&
        strstr(actual_text, "func __fnwrap_add1(__env.0, x.1) {\n") != NULL &&
        strstr(actual_text, "func __fnwrap_add2(__env.0, x.1) {\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: ternary noncapturing function-value return immediate call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_ternary_noncapturing_function_value_return_bind_and_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int add2(int x){ return x+2; }\n"
            "int pick(int c)(int){ int f(int)=add1; int g(int)=add2; return ((putint(0), c) ? f : g); }\n"
            "int main(){ int h(int)=pick(1); return h(40); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "tmp.0 = call putint(0)\n") != NULL &&
        strstr(actual_text, "store_indirect __ret0.0, 1\n") != NULL &&
        strstr(actual_text, "store_indirect __ret0.0, 2\n") != NULL &&
        strstr(actual_text, "tmp.2 = eq h$ftag.0, 1\n") != NULL &&
        strstr(actual_text, "shape shape.0(int) -> int\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add2, 0, shape.0\n") != NULL &&
        strstr(actual_text, "func __fnwrap_add1(__env.0, x.1) {\n") != NULL &&
        strstr(actual_text, "func __fnwrap_add2(__env.0, x.1) {\n") != NULL &&
        strstr(actual_text, "__retfn_declslot") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: ternary noncapturing function-value return bind-and-call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_ternary_closure_function_value_return_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int pick(int c)(int){ int x=3; int y=5; int f(int)=closure [x] int (int z){ return x+z; }; int g(int)=closure [y] int (int z){ return y+z; }; return ((putint(0), c) ? f : g); }\n"
            "int main(){ return pick(1)(4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "tmp.0 = call putint(0)\n") != NULL &&
        strstr(actual_text, "store_indirect __ret0.0, 1\n") != NULL &&
        strstr(actual_text, "store_indirect __ret1.1, f$closurecap$0.5\n") != NULL &&
        strstr(actual_text, "store_indirect __ret0.0, 2\n") != NULL &&
        strstr(actual_text, "store_indirect __ret1.1, g$closurecap$0.7\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 1u &&
        strstr(actual_text, "eq __retclosure_immediate_0.0, 1\n") != NULL &&
        strstr(actual_text, "shape shape.0(int) -> int\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: ternary closure function-value return immediate call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_ternary_closure_function_value_return_bind_and_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int pick(int c)(int){ int x=3; int y=5; int f(int)=closure [x] int (int z){ return x+z; }; int g(int)=closure [y] int (int z){ return y+z; }; return ((putint(0), c) ? f : g); }\n"
            "int main(){ int h(int)=pick(1); return h(4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "tmp.0 = call putint(0)\n") != NULL &&
        strstr(actual_text, "store_indirect __ret0.0, 1\n") != NULL &&
        strstr(actual_text, "store_indirect __ret1.1, f$closurecap$0.5\n") != NULL &&
        strstr(actual_text, "store_indirect __ret0.0, 2\n") != NULL &&
        strstr(actual_text, "store_indirect __ret1.1, g$closurecap$0.7\n") != NULL &&
        strstr(actual_text, "eq h$ftag.0, 1\n") != NULL &&
        strstr(actual_text, "shape shape.0(int) -> int\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_pick__closure_g_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: ternary closure function-value return bind-and-call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_direct_dynamic_returned_multi_capture_closure_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int pick(int x, int y, int c)(int,int) { int f(int,int)=closure [x,y] int (int a, int b) { return x + y + a + b; }; int g(int,int)=closure [x,y] int (int a, int b) { return x + y - a - b; }; if(c) f=g; return f; }\n"
            "int main(){ return pick(5, 7, 1)(3, 2); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "shape shape.0(int, int) -> int\n") != NULL &&
        ir_test_count_fragment_occurrences(actual_text, "call pick(") == 1u &&
        strstr(actual_text, "__closure_env_0.") != NULL &&
        strstr(actual_text, "__closure_env_1.") != NULL &&
        strstr(actual_text, "eq __retclosure_immediate_0.0, 1\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: direct dynamic returned multi-capture closure immediate call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_ternary_closure_local_initializer_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int main(){ int c=1; int x=3; int y=5; int f(int)=closure [x] int (int z) { return x + z; }; int g(int)=closure [y] int (int z) { return y + z; }; int h(int)=c ? f : g; return h(4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "f$closurecap$0.3 = mov x.1\n") != NULL &&
        strstr(actual_text, "g$closurecap$0.5 = mov y.2\n") != NULL &&
        strstr(actual_text, "tmp.0 = ne c.0, 0\n") != NULL &&
        strstr(actual_text, "h$ftag.8 = mov 1\n") != NULL &&
        strstr(actual_text, "h$closurecap$0.7 = mov f$closurecap$0.3\n") != NULL &&
        strstr(actual_text, "h$ftag.8 = mov 2\n") != NULL &&
        strstr(actual_text, "h$closurecap$0.7 = mov g$closurecap$0.5\n") != NULL &&
        strstr(actual_text, "eq h$ftag.8, 1\n") != NULL &&
        strstr(actual_text, "shape shape.0(int) -> int\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_g_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: ternary closure local initializer mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_ternary_function_value_actual_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int add2(int x){ return x+2; }\n"
            "int apply(int f(int), int x){ return f(x); }\n"
            "int main(){ int c=1; int f(int)=add1; int g(int)=add2; return apply(((putint(0), c) ? f : g), 40); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "tmp.0 = call putint(0)\n") != NULL &&
        strstr(actual_text, "tmp.1 = ne c.0, 0\n") != NULL &&
        strstr(actual_text, "__ternary_fn_argtag.3 = mov 1\n") != NULL &&
        strstr(actual_text, "__ternary_fn_argtag.3 = mov 2\n") != NULL &&
        strstr(actual_text, "tmp.2 = eq __ternary_fn_argtag.3, 1\n") != NULL &&
        strstr(actual_text, "shape shape.0(int) -> int\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add2, 0, shape.0\n") != NULL &&
        strstr(actual_text, "func __fnwrap_add1(__env.0, x.1) {\n") != NULL &&
        strstr(actual_text, "func __fnwrap_add2(__env.0, x.1) {\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: ternary function value actual argument mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_ternary_closure_actual_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply(int f(int), int x){ return f(x); }\n"
            "int main(){ int c=1; int x=3; int y=5; int f(int)=closure [x] int (int z) { return x + z; }; int g(int)=closure [y] int (int z) { return y + z; }; return apply(((putint(0), c) ? f : g), 4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "tmp.0 = call putint(0)\n") != NULL &&
        strstr(actual_text, "__ternary_fn_argtag.") != NULL &&
        strstr(actual_text, "__ternary_fn_argcap_0.") != NULL &&
        strstr(actual_text, "mov f$closurecap$0.3\n") != NULL &&
        strstr(actual_text, "mov g$closurecap$0.5\n") != NULL &&
        strstr(actual_text, "eq __ternary_fn_argtag.") != NULL &&
        strstr(actual_text, "shape shape.0(int) -> int\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_g_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: ternary closure actual argument mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_second_order_dynamic_ternary_function_value_actual_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply(int f(int), int x){ return f(x); }\n"
            "int apply_twice(int f(int), int x){ return f(f(x)); }\n"
            "int pass(int h(int f(int), int x), int f(int), int x){ return h(f, x); }\n"
            "int add1(int x){ return x+1; }\n"
            "int main(){ int c=getint(); return pass(c ? apply : apply_twice, add1, 41); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "tmp.0 = call getint()\n") != NULL &&
        strstr(actual_text, "__ternary_fn_argtag.1 = mov 1\n") != NULL &&
        strstr(actual_text, "__ternary_fn_argtag.1 = mov 2\n") != NULL &&
        strstr(actual_text, "pass__fv_0_apply_1_add1") == NULL &&
        strstr(actual_text, "pass__fv_0_apply_twice_1_add1") == NULL &&
        strstr(actual_text, "apply_twice__fv_0_add1") == NULL &&
        strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.1\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.2\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: second-order dynamic ternary function value actual argument mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_comma_wrapped_closure_forward_into_function_value_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply(int f(int), int x){ return f(x); }\n"
            "int main(){ int y=3; int f(int)=closure [y] int (int z) { return y + z; }; return apply((f, f), 4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply(f.0, x.1)\n") != NULL &&
        strstr(actual_text, "f$closurecap$0.1 = mov y.0\n") != NULL &&
        strstr(actual_text, "shape shape.0(int) -> int\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_f_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: comma-wrapped closure forwarding into function-valued parameter mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_assignment_result_closure_forward_into_function_value_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int apply(int f(int), int x){ return f(x); }\n"
            "int main(){ int y=3; int f(int)=closure [y] int (int z) { return y + z; }; return apply((f = f), 4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply(f.0, x.1)\n") != NULL &&
        strstr(actual_text, "f$closurecap$0.1 = mov y.0\n") != NULL &&
        strstr(actual_text, "f$ftag.2 = mov 1\n") != NULL &&
        strstr(actual_text, "shape shape.0(int) -> int\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_f_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: assignment-result closure forwarding into function-valued parameter mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_comma_wrapped_closure_forward_into_function_value_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;
    const char *scan = NULL;
    size_t putint_count = 0u;

    if (!lower_extension_source_to_ir_text(
            "int apply(int f(int), int x){ return f(x); }\n"
            "int main(){ int c=1; int x=3; int y=5; int f(int)=closure [x] int (int z) { return x + z; }; int g(int)=closure [y] int (int z) { return y + z; }; return apply(((putint(0), c), f), 4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    scan = actual_text;
    while (scan && (scan = strstr(scan, "call putint(0)\n")) != NULL) {
        ++putint_count;
        scan += strlen("call putint(0)\n");
    }

    ok = actual_text &&
        strstr(actual_text, "declare putint(param0.0)\n") != NULL &&
        strstr(actual_text, "tmp.0 = call putint(0)\n") != NULL &&
        putint_count == 1u &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_f_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic comma-wrapped closure forwarding into function-valued parameter mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_assignment_result_closure_forward_into_function_value_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;
    const char *scan = NULL;
    size_t putint_count = 0u;

    if (!lower_extension_source_to_ir_text(
            "int apply(int f(int), int x){ return f(x); }\n"
            "int main(){ int c=1; int x=3; int y=5; int f(int)=closure [x] int (int z) { return x + z; }; int g(int)=closure [y] int (int z) { return y + z; }; int h(int)=f; if((putint(0), c)) h=g; return apply((f=h), 4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    scan = actual_text;
    while (scan && (scan = strstr(scan, "call putint(0)\n")) != NULL) {
        ++putint_count;
        scan += strlen("call putint(0)\n");
    }

    ok = actual_text &&
        strstr(actual_text, "declare putint(param0.0)\n") != NULL &&
        strstr(actual_text, "tmp.0 = call putint(0)\n") != NULL &&
        putint_count == 1u &&
        strstr(actual_text, "br c.0, bb.1, bb.2\n") != NULL &&
        strstr(actual_text, "f$closurecap$0.3 = mov h$closurecap$0.8\n") != NULL &&
        strstr(actual_text, "f$ftag.4 = mov h$ftag.7\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_g_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic assignment-result closure forwarding into function-valued parameter mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_comma_wrapped_zero_arg_closure_forward_into_function_value_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;
    const char *scan = NULL;
    size_t wrapped_putint_count = 0u;

    if (!lower_extension_source_to_ir_text(
            "int apply0(int f()){ return f(); }\n"
            "int main(){ int c=1; int x=7; int y=9; int f()=closure [x] int () { return x; }; int g()=closure [y] int () { return y; }; int h()=f; if((putint(0), c)) h=g; return apply0((0, h)); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    scan = actual_text;
    while (scan && (scan = strstr(scan, "tmp.0 = call putint(0)\n")) != NULL) {
        ++wrapped_putint_count;
        scan += strlen("tmp.0 = call putint(0)\n");
    }

    ok = actual_text &&
        wrapped_putint_count == 1u &&
        strstr(actual_text, "declare apply0(f.0)\n") != NULL &&
        strstr(actual_text, "h$ftag.7 = mov 2\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic comma-wrapped zero-arg closure forwarding into function-valued parameter mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_assignment_result_zero_arg_closure_forward_into_function_value_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;
    const char *scan = NULL;
    size_t wrapped_putint_count = 0u;

    if (!lower_extension_source_to_ir_text(
            "int apply0(int f()){ return f(); }\n"
            "int main(){ int c=1; int x=7; int y=9; int f()=closure [x] int () { return x; }; int g()=closure [y] int () { return y; }; int h()=f; if((putint(0), c)) h=g; return apply0((f=h)); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    scan = actual_text;
    while (scan && (scan = strstr(scan, "tmp.0 = call putint(0)\n")) != NULL) {
        ++wrapped_putint_count;
        scan += strlen("tmp.0 = call putint(0)\n");
    }

    ok = actual_text &&
        wrapped_putint_count == 1u &&
        strstr(actual_text, "declare apply0(f.0)\n") != NULL &&
        strstr(actual_text, "h$ftag.7 = mov f$ftag.4\n") != NULL &&
        strstr(actual_text, "f$ftag.4 = mov h$ftag.7\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic assignment-result zero-arg closure forwarding into function-valued parameter mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_comma_wrapped_zero_arg_void_closure_forward_into_function_value_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;
    const char *scan = NULL;
    size_t wrapped_putint_count = 0u;

    if (!lower_extension_source_to_ir_text(
            "void apply0(void f()){ f(); }\n"
            "int main(){ int c=1; int x=7; int y=9; void f()=closure [x] void () { putint(x); return; }; void g()=closure [y] void () { putint(y); return; }; void h()=f; if((putint(0), c)) h=g; return (apply0((0, h)), 0); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    scan = actual_text;
    while (scan && (scan = strstr(scan, "tmp.0 = call putint(0)\n")) != NULL) {
        ++wrapped_putint_count;
        scan += strlen("tmp.0 = call putint(0)\n");
    }

    ok = actual_text &&
        wrapped_putint_count == 1u &&
        strstr(actual_text, "declare apply0(f.0)\n") != NULL &&
        strstr(actual_text, "h$ftag.7 = mov 2\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "ret 0\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic comma-wrapped zero-arg void closure forwarding into function-valued parameter mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_assignment_result_zero_arg_void_closure_forward_into_function_value_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;
    const char *scan = NULL;
    size_t wrapped_putint_count = 0u;

    if (!lower_extension_source_to_ir_text(
            "void apply0(void f()){ f(); }\n"
            "int main(){ int c=1; int x=7; int y=9; void f()=closure [x] void () { putint(x); return; }; void g()=closure [y] void () { putint(y); return; }; void h()=f; if((putint(0), c)) h=g; return (apply0((f=h)), 0); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    scan = actual_text;
    while (scan && (scan = strstr(scan, "tmp.0 = call putint(0)\n")) != NULL) {
        ++wrapped_putint_count;
        scan += strlen("tmp.0 = call putint(0)\n");
    }

    ok = actual_text &&
        wrapped_putint_count == 1u &&
        strstr(actual_text, "declare apply0(f.0)\n") != NULL &&
        strstr(actual_text, "f$closurecap$0.3 = mov h$closurecap$0.8\n") != NULL &&
        strstr(actual_text, "f$ftag.4 = mov h$ftag.7\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "ret 0\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic assignment-result zero-arg void closure forwarding into function-valued parameter mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_comma_wrapped_function_value_forwarding_into_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;
    const char *scan = NULL;
    size_t putint_count = 0u;

    if (!lower_extension_source_to_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int add2(int x){ return x+2; }\n"
            "int apply(int f(int), int x){ return f(x); }\n"
            "int main(){ int c=1; int g(int)=add1; if((putint(0), c)) g=add2; return apply((0, g), 40); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    scan = actual_text;
    while (scan && (scan = strstr(scan, "call putint(0)\n")) != NULL) {
        ++putint_count;
        scan += strlen("call putint(0)\n");
    }

    ok = actual_text &&
        strstr(actual_text, "declare putint(param0.0)\n") != NULL &&
        putint_count == 1u &&
        strstr(actual_text, "tmp.1 = eq g$ftag.1, 1\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.3(40)\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add2, 0, shape.0\n") != NULL &&
        strstr(actual_text, "func __fnwrap_add1(__env.0, x.1) {\n") != NULL &&
        strstr(actual_text, "func __fnwrap_add2(__env.0, x.1) {\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic comma-wrapped function-value forwarding into parameter mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_assignment_result_function_value_forwarding_into_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;
    const char *scan = NULL;
    size_t putint_count = 0u;

    if (!lower_extension_source_to_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int add2(int x){ return x+2; }\n"
            "int apply(int f(int), int x){ return f(x); }\n"
            "int main(){ int c=1; int f(int)=add1; int h(int)=add1; if((putint(0), c)) f=add2; return apply((h=f), 40); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    scan = actual_text;
    while (scan && (scan = strstr(scan, "call putint(0)\n")) != NULL) {
        ++putint_count;
        scan += strlen("call putint(0)\n");
    }

    ok = actual_text &&
        strstr(actual_text, "declare putint(param0.0)\n") != NULL &&
        putint_count == 1u &&
        strstr(actual_text, "h$ftag.2 = mov f$ftag.1\n") != NULL &&
        strstr(actual_text, "tmp.1 = eq h$ftag.2, 1\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.3(40)\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add2, 0, shape.0\n") != NULL &&
        strstr(actual_text, "func __fnwrap_add1(__env.0, x.1) {\n") != NULL &&
        strstr(actual_text, "func __fnwrap_add2(__env.0, x.1) {\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic assignment-result function-value forwarding into parameter mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_comma_wrapped_zero_arg_function_value_forwarding_into_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;
    const char *scan = NULL;
    size_t putint_count = 0u;

    if (!lower_extension_source_to_ir_text(
            "int next1(){ return 1; }\n"
            "int next2(){ return 2; }\n"
            "int apply0(int f()){ return f(); }\n"
            "int main(){ int c=1; int g()=next1; if((putint(0), c)) g=next2; return apply0((0, g)); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    scan = actual_text;
    while (scan && (scan = strstr(scan, "call putint(0)\n")) != NULL) {
        ++putint_count;
        scan += strlen("call putint(0)\n");
    }

    ok = actual_text &&
        strstr(actual_text, "declare putint(param0.0)\n") != NULL &&
        putint_count == 1u &&
        strstr(actual_text, "shape shape.0() -> int\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_next1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.3()\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_next2, 0, shape.0\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic comma-wrapped zero-arg function-value forwarding into parameter mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_assignment_result_zero_arg_function_value_forwarding_into_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;
    const char *scan = NULL;
    size_t putint_count = 0u;

    if (!lower_extension_source_to_ir_text(
            "int next1(){ return 1; }\n"
            "int next2(){ return 2; }\n"
            "int apply0(int f()){ return f(); }\n"
            "int main(){ int c=1; int f()=next1; int h()=next1; if((putint(0), c)) f=next2; return apply0((h=f)); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    scan = actual_text;
    while (scan && (scan = strstr(scan, "call putint(0)\n")) != NULL) {
        ++putint_count;
        scan += strlen("call putint(0)\n");
    }

    ok = actual_text &&
        strstr(actual_text, "declare putint(param0.0)\n") != NULL &&
        putint_count == 1u &&
        strstr(actual_text, "h$ftag.2 = mov f$ftag.1\n") != NULL &&
        strstr(actual_text, "shape shape.0() -> int\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_next1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.3()\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_next2, 0, shape.0\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic assignment-result zero-arg function-value forwarding into parameter mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_comma_wrapped_zero_arg_void_function_value_forwarding_into_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;
    const char *scan = NULL;
    size_t putint_count = 0u;

    if (!lower_extension_source_to_ir_text(
            "void ping1(){ putint(1); }\n"
            "void ping2(){ putint(2); }\n"
            "void apply0(void f()){ f(); }\n"
            "int main(){ int c=1; void g()=ping1; if((putint(0), c)) g=ping2; apply0((0, g)); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    scan = actual_text;
    while (scan && (scan = strstr(scan, "call putint(0)\n")) != NULL) {
        ++putint_count;
        scan += strlen("call putint(0)\n");
    }

    ok = actual_text &&
        strstr(actual_text, "declare putint(param0.0)\n") != NULL &&
        putint_count == 1u &&
        strstr(actual_text, "shape shape.0() -> void\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_ping1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.3()\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_ping2, 0, shape.0\n") != NULL &&
        strstr(actual_text, "ret 0\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic comma-wrapped zero-arg void function-value forwarding into parameter mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_dynamic_assignment_result_zero_arg_void_function_value_forwarding_into_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;
    const char *scan = NULL;
    size_t putint_count = 0u;

    if (!lower_extension_source_to_ir_text(
            "void ping1(){ putint(1); }\n"
            "void ping2(){ putint(2); }\n"
            "void apply0(void f()){ f(); }\n"
            "int main(){ int c=1; void f()=ping1; void h()=ping1; if((putint(0), c)) f=ping2; apply0((h=f)); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    scan = actual_text;
    while (scan && (scan = strstr(scan, "call putint(0)\n")) != NULL) {
        ++putint_count;
        scan += strlen("call putint(0)\n");
    }

    ok = actual_text &&
        strstr(actual_text, "declare putint(param0.0)\n") != NULL &&
        putint_count == 1u &&
        strstr(actual_text, "h$ftag.2 = mov f$ftag.1\n") != NULL &&
        strstr(actual_text, "shape shape.0() -> void\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_ping1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.3()\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_ping2, 0, shape.0\n") != NULL &&
        strstr(actual_text, "ret 0\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: dynamic assignment-result zero-arg void function-value forwarding into parameter mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_ternary_function_value_callee_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int add2(int x){ return x+2; }\n"
            "int main(){ int c=1; int f(int)=add1; int g(int)=add2; return (((putint(0), c) ? f : g))(40); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "tmp.0 = call putint(0)\n") != NULL &&
        strstr(actual_text, "tmp.1 = ne c.0, 0\n") != NULL &&
        strstr(actual_text, "tmp.2 = eq ") != NULL &&
        strstr(actual_text, "shape shape.0(int) -> int\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add1, 0, shape.0\n") != NULL &&
        strstr(actual_text, "call_indirect tmp.4(40)\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_add2, 0, shape.0\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: ternary function value callee mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_ternary_closure_callee_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int main(){ int c=1; int x=3; int y=5; int f(int)=closure [x] int (int z) { return x + z; }; int g(int)=closure [y] int (int z) { return y + z; }; return (((putint(0), c) ? f : g))(4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "tmp.0 = call putint(0)\n") != NULL &&
        strstr(actual_text, "mov f$closurecap$0.3\n") != NULL &&
        strstr(actual_text, "mov g$closurecap$0.5\n") != NULL &&
        strstr(actual_text, "__ternary_callee_tag.7 = mov 1\n") != NULL &&
        strstr(actual_text, "__ternary_callee_cap_0.8 = mov f$closurecap$0.3\n") != NULL &&
        strstr(actual_text, "__ternary_callee_tag.7 = mov 2\n") != NULL &&
        strstr(actual_text, "__ternary_callee_cap_0.8 = mov g$closurecap$0.5\n") != NULL &&
        strstr(actual_text, "shape shape.0(int) -> int\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_g_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: ternary closure callee mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_ternary_multi_capture_closure_callee_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int main(){ int c=1; int x=3; int y=5; int f(int,int)=closure [x,y] int (int a, int b) { return x + y + a + b; }; int g(int,int)=closure [x,y] int (int a, int b) { return x + y - a - b; }; return (((putint(0), c) ? f : g))(4, 2); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "tmp.0 = call putint(0)\n") != NULL &&
        strstr(actual_text, "__ternary_callee_tag.") != NULL &&
        strstr(actual_text, "__ternary_callee_cap_0.") != NULL &&
        strstr(actual_text, "__ternary_callee_cap_1.") != NULL &&
        strstr(actual_text, "shape shape.0(int, int) -> int\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_g_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: ternary multi-capture closure callee mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_ternary_zero_arg_closure_callee_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int main(){ int c=1; int x=3; int y=5; int f()=closure [x] int () { return x; }; int g()=closure [y] int () { return y; }; return (((putint(0), c) ? f : g))(); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "tmp.0 = call putint(0)\n") != NULL &&
        strstr(actual_text, "__ternary_callee_tag.7 = mov 1\n") != NULL &&
        strstr(actual_text, "__ternary_callee_cap_0.8 = mov f$closurecap$0.3\n") != NULL &&
        strstr(actual_text, "__ternary_callee_tag.7 = mov 2\n") != NULL &&
        strstr(actual_text, "__ternary_callee_cap_0.8 = mov g$closurecap$0.5\n") != NULL &&
        strstr(actual_text, "shape shape.0() -> int\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_g_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: ternary zero-arg closure callee mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_accepts_ternary_zero_arg_void_closure_callee_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int main(){ int c=1; int x=3; int y=5; void f()=closure [x] void () { putint(x); return; }; void g()=closure [y] void () { putint(y); return; }; (((putint(0), c) ? f : g))(); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "tmp.0 = call putint(0)\n") != NULL &&
        strstr(actual_text, "__ternary_callee_tag.7 = mov 1\n") != NULL &&
        strstr(actual_text, "__ternary_callee_cap_0.8 = mov f$closurecap$0.3\n") != NULL &&
        strstr(actual_text, "__ternary_callee_tag.7 = mov 2\n") != NULL &&
        strstr(actual_text, "__ternary_callee_cap_0.8 = mov g$closurecap$0.5\n") != NULL &&
        strstr(actual_text, "shape shape.0() -> void\n") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "call_indirect tmp.") != NULL &&
        strstr(actual_text, "fn_make __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "ret 0\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: ternary zero-arg void closure callee mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_lowers_pair_copy_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "int main(){ pair a={1,2}; pair b=a; b=a; return b.first + b.second; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "a$first.0 = mov 1") != NULL &&
        strstr(actual_text, "a$second.1 = mov 2") != NULL &&
        strstr(actual_text, "b$first.2 = mov a$first.0") != NULL &&
        strstr(actual_text, "b$second.3 = mov a$second.1") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: pair copy lowering should use split scalar locals\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
}

static int test_ir_lowers_struct_copy_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_ir_text(
            "struct Point { int x; int y; int z; };\n"
            "int main(){ struct Point a={1,2,3}; struct Point b=a; b=a; return b.x + b.y + b.z; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "a$x.0 = mov 1") != NULL &&
        strstr(actual_text, "a$y.1 = mov 2") != NULL &&
        strstr(actual_text, "a$z.2 = mov 3") != NULL &&
        strstr(actual_text, "b$x.3 = mov a$x.0") != NULL &&
        strstr(actual_text, "b$y.4 = mov a$y.1") != NULL &&
        strstr(actual_text, "b$z.5 = mov a$z.2") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[ir-reg] FAIL: struct copy lowering should use split scalar locals across all fields\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
    }

    free(actual_text);
    return ok;
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

static int test_ir_lowers_strict_local_state_while_return_family(void) {
    return expect_ir_lower_succeeds("IR-STRICT-LOCAL-STATE-WHILE-RETURN",
        "int f(){int b=1; while(b<2){ if(b){ return 2; } }}\n");
}

static int test_ir_lowers_strict_local_state_for_return_family(void) {
    return expect_ir_lower_succeeds("IR-STRICT-LOCAL-STATE-FOR-RETURN",
        "int f(){int b=1; for(;b<2;b=b+1){ if(b){ return 2; } }}\n");
}

static int test_ir_lowers_strict_assigned_local_state_loop_return_family(void) {
    return expect_ir_lower_succeeds("IR-STRICT-ASSIGNED-LOCAL-STATE-LOOP-RETURN",
        "int f(){int b=0; b=1; while(b<2){ if(b){ return 2; } }}\n");
}

static int test_ir_keeps_while_exit_when_postfix_update_changes_condition_local(void) {
    return expect_ir_dump("IR-WHILE-POSTFIX-UPDATE-EXIT",
        "int main(){ int b=0; while(b<3){ b++; } return b; }\n",
        "func main() {\n"
        "  bb.0:\n"
        "    b.0 = mov 0\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    tmp.0 = lt b.0, 3\n"
        "    br tmp.0, bb.2, bb.3\n"
        "  bb.2:\n"
        "    tmp.1 = mov b.0\n"
        "    tmp.2 = mov tmp.1\n"
        "    tmp.3 = add tmp.1, 1\n"
        "    b.0 = mov tmp.3\n"
        "    jmp bb.1\n"
        "  bb.3:\n"
        "    ret b.0\n"
        "}\n");
}

static int test_ir_keeps_for_exit_when_compound_step_changes_condition_local(void) {
    return expect_ir_dump("IR-FOR-COMPOUND-STEP-EXIT",
        "int main(){ int b=0; for(; b<3; b+=1){ } return b; }\n",
        "func main() {\n"
        "  bb.0:\n"
        "    b.0 = mov 0\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    tmp.0 = lt b.0, 3\n"
        "    br tmp.0, bb.2, bb.4\n"
        "  bb.2:\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    tmp.1 = mov b.0\n"
        "    tmp.2 = add tmp.1, 1\n"
        "    b.0 = mov tmp.2\n"
        "    jmp bb.1\n"
        "  bb.4:\n"
        "    ret b.0\n"
        "}\n");
}

static int test_ir_preserves_loop_condition_after_unknown_if_state_update(void) {
    return expect_ir_dump("IR-LOOP-COND-PRESERVED-AFTER-UNKNOWN-IF-STATE",
        "int getint(); int main(){ int mon = 1; while(mon <= 12){ if(getint()) mon = mon + 1; } return 0; }\n",
        "declare getint()\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    mon.0 = mov 1\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    tmp.0 = le mon.0, 12\n"
        "    br tmp.0, bb.2, bb.3\n"
        "  bb.2:\n"
        "    tmp.1 = call getint()\n"
        "    br tmp.1, bb.4, bb.5\n"
        "  bb.3:\n"
        "    ret 0\n"
        "  bb.4:\n"
        "    tmp.2 = add mon.0, 1\n"
        "    mon.0 = mov tmp.2\n"
        "    jmp bb.5\n"
        "  bb.5:\n"
        "    jmp bb.1\n"
        "}\n");
}

static int test_ir_keeps_post_if_local_condition_when_branch_facts_disagree(void) {
    return expect_ir_dump("IR-IF-MERGE-LOCAL-COND",
        "int putch(int x);\n"
        "int getint();\n"
        "int main(){int x=getint();int flag;if(x>0) flag=0; else flag=1; if(flag==1) putch(45); return 0;}\n",
        "declare getint()\n"
        "\n"
        "declare putch(x.0)\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    tmp.0 = call getint()\n"
        "    x.0 = mov tmp.0\n"
        "    tmp.1 = gt x.0, 0\n"
        "    br tmp.1, bb.1, bb.2\n"
        "  bb.1:\n"
        "    flag.1 = mov 0\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    flag.1 = mov 1\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    tmp.2 = eq flag.1, 1\n"
        "    br tmp.2, bb.4, bb.5\n"
        "  bb.4:\n"
        "    tmp.3 = call putch(45)\n"
        "    jmp bb.5\n"
        "  bb.5:\n"
        "    ret 0\n"
        "}\n");
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

static int test_ir_lowers_if_condition_side_effect_even_when_truthiness_known(void) {
    return expect_ir_dump("IR-IF-CONDITION-SIDE-EFFECT",
        "int main(){ int i; i = 0; if (i = 1) { } return i; }\n",
        "func main() {\n"
        "  bb.0:\n"
        "    i.0 = mov 0\n"
        "    i.0 = mov 1\n"
        "    br 1, bb.1, bb.2\n"
        "  bb.1:\n"
        "    jmp bb.2\n"
        "  bb.2:\n"
        "    ret i.0\n"
        "}\n");
}

static int test_ir_lowers_while_condition_side_effect_even_when_truthiness_known(void) {
    return expect_ir_dump_skip_return_check("IR-WHILE-CONDITION-SIDE-EFFECT",
        "int main(){ int i; i = 1; while (i = 0) { } return i; }\n",
        "func main() {\n"
        "  bb.0:\n"
        "    i.0 = mov 1\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    i.0 = mov 0\n"
        "    br 0, bb.2, bb.3\n"
        "  bb.2:\n"
        "    jmp bb.1\n"
        "  bb.3:\n"
        "    ret i.0\n"
        "}\n");
}

static int test_ir_lowers_for_condition_side_effect_even_when_truthiness_known(void) {
    return expect_ir_dump_skip_return_check("IR-FOR-CONDITION-SIDE-EFFECT",
        "int main(){ int i; i = 1; for (; i = 0; ) { } return i; }\n",
        "func main() {\n"
        "  bb.0:\n"
        "    i.0 = mov 1\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    i.0 = mov 0\n"
        "    br 0, bb.2, bb.3\n"
        "  bb.2:\n"
        "    jmp bb.1\n"
        "  bb.3:\n"
        "    ret i.0\n"
        "}\n");
}

static int test_ir_merges_no_else_branch_facts_with_false_path(void) {
    return expect_ir_dump("IR-IF-NO-ELSE-MERGE",
        "int getint(){ return 0; }\n"
        "int main(){ int a; int i; a = getint(); i = 1; if (a) { i = 2; } if (i == 1) return 1; else return 0; }\n",
        "func getint() {\n"
        "  bb.0:\n"
        "    ret 0\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    tmp.0 = call getint()\n"
        "    a.0 = mov tmp.0\n"
        "    i.1 = mov 1\n"
        "    br a.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    i.1 = mov 2\n"
        "    jmp bb.2\n"
        "  bb.2:\n"
        "    tmp.1 = eq i.1, 1\n"
        "    br tmp.1, bb.3, bb.4\n"
        "  bb.3:\n"
        "    ret 1\n"
        "  bb.4:\n"
        "    ret 0\n"
        "}\n");
}

static int test_ir_merges_no_else_condition_side_effect_fact_to_continue(void) {
    return expect_ir_dump("IR-IF-NO-ELSE-CONDITION-SIDE-EFFECT-MERGE",
        "int main(){ int i; i = 0; if ((i = 1) && 0) { } if (i) return 1; else return 0; }\n",
        "func main() {\n"
        "  bb.0:\n"
        "    i.0 = mov 0\n"
        "    i.0 = mov 1\n"
        "    br 1, bb.3, bb.2\n"
        "  bb.1:\n"
        "    jmp bb.2\n"
        "  bb.2:\n"
        "    br i.0, bb.4, bb.5\n"
        "  bb.3:\n"
        "    br 0, bb.1, bb.2\n"
        "  bb.4:\n"
        "    ret 1\n"
        "  bb.5:\n"
        "    ret 0\n"
        "}\n");
}

static int test_ir_clears_ternary_branch_mutation_facts_after_join(void) {
    return expect_ir_dump("IR-TERNARY-JOIN-MUTATION-FACTS",
        "int getint(){ return 1; }\n"
        "int main(){ int a; int i; int x; a = getint(); i = 0; x = a ? (i = 1) : (i = 2); if (i == 1) return 1; else return 0; }\n",
        "func getint() {\n"
        "  bb.0:\n"
        "    ret 1\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    tmp.0 = call getint()\n"
        "    a.0 = mov tmp.0\n"
        "    i.1 = mov 0\n"
        "    br a.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    i.1 = mov 1\n"
        "    tmp.1 = mov 1\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    i.1 = mov 2\n"
        "    tmp.1 = mov 2\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    x.2 = mov tmp.1\n"
        "    tmp.2 = eq i.1, 1\n"
        "    br tmp.2, bb.4, bb.5\n"
        "  bb.4:\n"
        "    ret 1\n"
        "  bb.5:\n"
        "    ret 0\n"
        "}\n");
}

static int test_ir_clears_logical_value_branch_mutation_facts_after_join(void) {
    return expect_ir_dump("IR-LOGICAL-VALUE-JOIN-MUTATION-FACTS",
        "int getint(){ return 1; }\n"
        "int main(){ int a; int i; int x; a = getint(); i = 0; x = a && (i = 1); if (i == 1) return 1; else return 0; }\n",
        "func getint() {\n"
        "  bb.0:\n"
        "    ret 1\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    tmp.0 = call getint()\n"
        "    a.0 = mov tmp.0\n"
        "    i.1 = mov 0\n"
        "    br a.0, bb.3, bb.2\n"
        "  bb.1:\n"
        "    tmp.1 = mov 1\n"
        "    jmp bb.4\n"
        "  bb.2:\n"
        "    tmp.1 = mov 0\n"
        "    jmp bb.4\n"
        "  bb.3:\n"
        "    i.1 = mov 1\n"
        "    br 1, bb.1, bb.2\n"
        "  bb.4:\n"
        "    x.2 = mov tmp.1\n"
        "    tmp.2 = eq i.1, 1\n"
        "    br tmp.2, bb.5, bb.6\n"
        "  bb.5:\n"
        "    ret 1\n"
        "  bb.6:\n"
        "    ret 0\n"
        "}\n");
}

static int test_ir_keeps_pre_loop_local_fact_after_unknown_calling_while_body(void) {
    return expect_ir_dump("IR-WHILE-CALL-BODY-SCOPE-RESTORE",
        "int getv(){ return 49; }\n"
        "int main(){"
        "  int ch; int x; int f;"
        "  ch = getv();"
        "  x = 1;"
        "  f = 0;"
        "  while (ch < 48 || ch > 57) {"
        "    if (ch == 45) f = 1;"
        "    ch = getv();"
        "  }"
        "  if (f) return -x; else return x;"
        "}\n",
        "func getv() {\n"
        "  bb.0:\n"
        "    ret 49\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    tmp.0 = call getv()\n"
        "    ch.0 = mov tmp.0\n"
        "    x.1 = mov 1\n"
        "    f.2 = mov 0\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    tmp.1 = lt ch.0, 48\n"
        "    br tmp.1, bb.2, bb.4\n"
        "  bb.2:\n"
        "    tmp.3 = eq ch.0, 45\n"
        "    br tmp.3, bb.5, bb.6\n"
        "  bb.3:\n"
        "    br f.2, bb.7, bb.8\n"
        "  bb.4:\n"
        "    tmp.2 = gt ch.0, 57\n"
        "    br tmp.2, bb.2, bb.3\n"
        "  bb.5:\n"
        "    f.2 = mov 1\n"
        "    jmp bb.6\n"
        "  bb.6:\n"
        "    tmp.4 = call getv()\n"
        "    ch.0 = mov tmp.4\n"
        "    jmp bb.1\n"
        "  bb.7:\n"
        "    tmp.5 = sub 0, x.1\n"
        "    ret tmp.5\n"
        "  bb.8:\n"
        "    ret x.1\n"
        "}\n");
}

static int test_ir_clears_mutated_local_fact_after_unknown_calling_for_loop(void) {
    return expect_ir_dump("IR-FOR-CALL-BODY-SCOPE-RESTORE",
        "int getv(){ return 1; }\n"
        "int main(){"
        "  int x; int f;"
        "  x = 1;"
        "  f = 0;"
        "  for (; x && getv(); x = x - 1) {"
        "    f = 1;"
        "  }"
        "  if (f) return 111; else return 222;"
        "}\n",
        "func getv() {\n"
        "  bb.0:\n"
        "    ret 1\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    x.0 = mov 1\n"
        "    f.1 = mov 0\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    br x.0, bb.5, bb.4\n"
        "  bb.2:\n"
        "    f.1 = mov 1\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    tmp.1 = sub x.0, 1\n"
        "    x.0 = mov tmp.1\n"
        "    jmp bb.1\n"
        "  bb.4:\n"
        "    br f.1, bb.6, bb.7\n"
        "  bb.5:\n"
        "    tmp.0 = call getv()\n"
        "    br tmp.0, bb.2, bb.4\n"
        "  bb.6:\n"
        "    ret 111\n"
        "  bb.7:\n"
        "    ret 222\n"
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

#include "strict_loop_return_alias.cases.inc"

int main(void) {
    const char *filter = getenv("IR_REG_FILTER");
    int ok = 1;

    if (filter && filter[0] != '\0') {
        if (strstr("IR-FLOAT-TRANSPORT-SIGNATURE", filter) != NULL) {
            return test_ir_lowers_float_transport_signature_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-LITERAL-TRANSPORT", filter) != NULL) {
            return test_ir_lowers_float_literal_transport_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-GLOBAL-IDENT-INIT", filter) != NULL) {
            return test_ir_lowers_float_global_initializer_transport_from_identifier_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-GLOBAL-CALL-INIT", filter) != NULL) {
            return test_ir_lowers_float_global_initializer_transport_from_call_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-RETURN-GLOBAL", filter) != NULL) {
            return test_ir_lowers_float_return_transport_from_global_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-RETURN-GLOBAL-CALL", filter) != NULL) {
            return test_ir_lowers_float_return_transport_from_global_call_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-GLOBAL-CALL-CHAIN", filter) != NULL) {
            return test_ir_lowers_float_global_call_chain_transport_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-LOCAL-CALL-CHAIN", filter) != NULL) {
            return test_ir_lowers_float_local_call_chain_transport_under_extension() ? 0 : 1;
        }
        if (strstr("IR-RETURNED-FUNCTION-VALUE-PARAM-IMMEDIATE-CALL", filter) != NULL) {
            return test_ir_lowers_returned_function_value_parameter_immediate_call_under_extension() ? 0 : 1;
        }
        if (strstr("IR-RETURNED-CLOSURE-BACKED-FUNCTION-VALUE-PARAM-IMMEDIATE-CALL", filter) != NULL) {
            return test_ir_accepts_returned_closure_backed_function_value_parameter_immediate_call_under_extension() ? 0 : 1;
        }
        if (strstr("IR-RETURNED-CLOSURE-BACKED-FUNCTION-VALUE-PARAM-BIND-AND-CALL", filter) != NULL) {
            return test_ir_accepts_returned_closure_backed_function_value_parameter_bind_and_call_under_extension() ? 0 : 1;
        }
        if (strstr("IR-DYNAMIC-RETURNED-CLOSURE-BACKED-FUNCTION-VALUE-PARAM-IMMEDIATE-CALL", filter) != NULL) {
            return test_ir_accepts_dynamic_returned_closure_backed_function_value_parameter_immediate_call_under_extension() ? 0 : 1;
        }
        if (strstr("IR-DYNAMIC-RETURNED-CLOSURE-BACKED-FUNCTION-VALUE-PARAM-BIND-AND-CALL", filter) != NULL) {
            return test_ir_accepts_dynamic_returned_closure_backed_function_value_parameter_bind_and_call_under_extension() ? 0 : 1;
        }
        if (strstr("IR-DYNAMIC-RETURNED-CLOSURE-PRODUCER-TERNARY-MERGE-BIND", filter) != NULL) {
            return test_ir_accepts_dynamic_returned_closure_producer_ternary_merge_bind_and_call_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-DYNAMIC-RETURNED-CLOSURE-PRODUCER-TERNARY-MERGE-IMMEDIATE", filter) != NULL) {
            return test_ir_accepts_dynamic_returned_closure_producer_ternary_merge_immediate_call_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-DYNAMIC-RETURNED-CLOSURE-RETURNED-CALL-TERNARY-MERGE-IMMEDIATE", filter) != NULL) {
            return test_ir_accepts_dynamic_returned_closure_returned_call_ternary_merge_immediate_call_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-DYNAMIC-RETURNED-CLOSURE-RETURNED-CALL-TERNARY-MERGE-LOCAL-BOUNCE-IMMEDIATE", filter) != NULL) {
            return test_ir_accepts_dynamic_returned_closure_returned_call_ternary_merge_local_bounce_immediate_call_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-DYNAMIC-RETURNED-CLOSURE-RETURNED-CALL-TERNARY-MERGE-BIND-RETURN-IMMEDIATE", filter) != NULL) {
            return test_ir_accepts_dynamic_returned_closure_returned_call_ternary_merge_bind_return_immediate_call_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-DYNAMIC-RETURNED-CLOSURE-RETURNED-CALL-TERNARY-MERGE-BIND-RETURN-ACTUAL", filter) != NULL) {
            return test_ir_accepts_dynamic_returned_closure_returned_call_ternary_merge_bind_return_actual_argument_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-DYNAMIC-RETURNED-CLOSURE-RETURNED-CALL-TERNARY-MERGE-BOUNCE-PASSTHROUGH-IMMEDIATE", filter) != NULL) {
            return test_ir_accepts_dynamic_returned_closure_returned_call_ternary_merge_bounce_passthrough_immediate_call_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-DYNAMIC-RETURNED-CLOSURE-RETURNED-CALL-TERNARY-MERGE-BOUNCE-PASSTHROUGH-BIND-RETURN-IMMEDIATE", filter) != NULL) {
            return test_ir_accepts_dynamic_returned_closure_returned_call_ternary_merge_bounce_passthrough_bind_return_immediate_call_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-DYNAMIC-RETURNED-CLOSURE-RETURNED-CALL-TERNARY-MERGE-BOUNCE-PASSTHROUGH-BIND-RETURN-ACTUAL", filter) != NULL) {
            return test_ir_accepts_dynamic_returned_closure_returned_call_ternary_merge_bounce_passthrough_bind_return_actual_argument_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-DYNAMIC-RETURNED-CLOSURE-RETURNED-CALL-TERNARY-MERGE-DEEPER-CHAIN-BIND-RETURN-IMMEDIATE", filter) != NULL) {
            return test_ir_accepts_dynamic_returned_closure_returned_call_ternary_merge_deeper_chain_bind_return_immediate_call_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-DYNAMIC-RETURNED-CLOSURE-PRODUCER-TERNARY-MERGE-ACTUAL", filter) != NULL) {
            return test_ir_accepts_dynamic_returned_closure_producer_ternary_merge_actual_argument_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-DYNAMIC-RETURNED-CLOSURE-PRODUCER-TERNARY-MERGE-BOUNCE-IMMEDIATE", filter) != NULL) {
            return test_ir_accepts_dynamic_returned_closure_producer_ternary_merge_local_bounce_immediate_call_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-DYNAMIC-RETURNED-CLOSURE-PRODUCER-TERNARY-MERGE-BOUNCE-ACTUAL", filter) != NULL) {
            return test_ir_accepts_dynamic_returned_closure_producer_ternary_merge_local_bounce_actual_argument_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-DYNAMIC-RETURNED-CLOSURE-PRODUCER-TERNARY-MERGE-BOUNCE-PASSTHROUGH-IMMEDIATE", filter) != NULL) {
            return test_ir_accepts_dynamic_returned_closure_producer_ternary_merge_local_bounce_passthrough_immediate_call_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-DYNAMIC-RETURNED-CLOSURE-PRODUCER-TERNARY-MERGE-BOUNCE-PASSTHROUGH-ACTUAL", filter) != NULL) {
            return test_ir_accepts_dynamic_returned_closure_producer_ternary_merge_local_bounce_passthrough_actual_argument_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-DYNAMIC-RETURNED-CLOSURE-PRODUCER-TERNARY-MERGE-REASSIGN-IMMEDIATE", filter) != NULL) {
            return test_ir_accepts_dynamic_returned_closure_producer_ternary_merge_local_reassign_immediate_call_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-DYNAMIC-RETURNED-CLOSURE-PRODUCER-TERNARY-MERGE-REASSIGN-ACTUAL", filter) != NULL) {
            return test_ir_accepts_dynamic_returned_closure_producer_ternary_merge_local_reassign_actual_argument_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-DYNAMIC-RETURNED-CLOSURE-PRODUCER-TERNARY-MERGE-RETURNED-CALL-REASSIGN-IMMEDIATE", filter) != NULL) {
            return test_ir_accepts_dynamic_returned_closure_producer_ternary_merge_returned_call_reassign_immediate_call_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-DYNAMIC-RETURNED-CLOSURE-PRODUCER-TERNARY-MERGE-RETURNED-CALL-REASSIGN-ACTUAL", filter) != NULL) {
            return test_ir_accepts_dynamic_returned_closure_producer_ternary_merge_returned_call_reassign_actual_argument_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-DYNAMIC-RETURNED-ZERO-ARG-CLOSURE-PRODUCER-TERNARY-MERGE-IMMEDIATE", filter) != NULL) {
            return test_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_immediate_call_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-DYNAMIC-RETURNED-ZERO-ARG-CLOSURE-PRODUCER-TERNARY-MERGE-ACTUAL", filter) != NULL) {
            return test_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_actual_argument_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-DYNAMIC-RETURNED-ZERO-ARG-VOID-CLOSURE-PRODUCER-TERNARY-MERGE-IMMEDIATE", filter) != NULL) {
            return test_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_immediate_call_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-DYNAMIC-RETURNED-ZERO-ARG-VOID-CLOSURE-PRODUCER-TERNARY-MERGE-ACTUAL", filter) != NULL) {
            return test_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_actual_argument_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-DYNAMIC-RETURNED-ZERO-ARG-CLOSURE-PRODUCER-TERNARY-MERGE-BOUNCE-IMMEDIATE", filter) != NULL) {
            return test_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_local_bounce_immediate_call_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-DYNAMIC-RETURNED-ZERO-ARG-CLOSURE-PRODUCER-TERNARY-MERGE-BOUNCE-ACTUAL", filter) != NULL) {
            return test_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_local_bounce_actual_argument_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-DYNAMIC-RETURNED-ZERO-ARG-CLOSURE-PRODUCER-TERNARY-MERGE-BOUNCE-PASSTHROUGH-IMMEDIATE", filter) != NULL) {
            return test_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_local_bounce_passthrough_immediate_call_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-DYNAMIC-RETURNED-ZERO-ARG-CLOSURE-PRODUCER-TERNARY-MERGE-BOUNCE-PASSTHROUGH-ACTUAL", filter) != NULL) {
            return test_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_local_bounce_passthrough_actual_argument_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-DYNAMIC-RETURNED-ZERO-ARG-CLOSURE-PRODUCER-TERNARY-MERGE-REASSIGN-IMMEDIATE", filter) != NULL) {
            return test_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_local_reassign_immediate_call_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-DYNAMIC-RETURNED-ZERO-ARG-CLOSURE-PRODUCER-TERNARY-MERGE-REASSIGN-ACTUAL", filter) != NULL) {
            return test_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_local_reassign_actual_argument_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-DYNAMIC-RETURNED-ZERO-ARG-VOID-CLOSURE-PRODUCER-TERNARY-MERGE-BOUNCE-IMMEDIATE", filter) != NULL) {
            return test_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_local_bounce_immediate_call_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-DYNAMIC-RETURNED-ZERO-ARG-VOID-CLOSURE-PRODUCER-TERNARY-MERGE-BOUNCE-ACTUAL", filter) != NULL) {
            return test_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_local_bounce_actual_argument_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-DYNAMIC-RETURNED-ZERO-ARG-VOID-CLOSURE-PRODUCER-TERNARY-MERGE-BOUNCE-PASSTHROUGH-IMMEDIATE", filter) != NULL) {
            return test_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_local_bounce_passthrough_immediate_call_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-DYNAMIC-RETURNED-ZERO-ARG-VOID-CLOSURE-PRODUCER-TERNARY-MERGE-BOUNCE-PASSTHROUGH-ACTUAL", filter) != NULL) {
            return test_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_local_bounce_passthrough_actual_argument_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-DYNAMIC-RETURNED-ZERO-ARG-VOID-CLOSURE-PRODUCER-TERNARY-MERGE-REASSIGN-IMMEDIATE", filter) != NULL) {
            return test_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_local_reassign_immediate_call_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-DYNAMIC-RETURNED-ZERO-ARG-VOID-CLOSURE-PRODUCER-TERNARY-MERGE-REASSIGN-ACTUAL", filter) != NULL) {
            return test_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_local_reassign_actual_argument_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-DYNAMIC-RETURNED-ZERO-ARG-CLOSURE-PRODUCER-TERNARY-MERGE-RETURNED-CALL-REASSIGN-IMMEDIATE", filter) != NULL) {
            return test_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_returned_call_reassign_immediate_call_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-DYNAMIC-RETURNED-ZERO-ARG-CLOSURE-PRODUCER-TERNARY-MERGE-RETURNED-CALL-REASSIGN-ACTUAL", filter) != NULL) {
            return test_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_returned_call_reassign_actual_argument_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-DYNAMIC-RETURNED-ZERO-ARG-VOID-CLOSURE-PRODUCER-TERNARY-MERGE-RETURNED-CALL-REASSIGN-IMMEDIATE", filter) != NULL) {
            return test_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_returned_call_reassign_immediate_call_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-DYNAMIC-RETURNED-ZERO-ARG-VOID-CLOSURE-PRODUCER-TERNARY-MERGE-RETURNED-CALL-REASSIGN-ACTUAL", filter) != NULL) {
            return test_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_returned_call_reassign_actual_argument_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-WRAPPED-FNVAL-REASSIGN", filter) != NULL) {
            return test_ir_accepts_wrapped_function_value_reassignment_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-ASSIGNMENT-RESULT-FNVAL-REASSIGN", filter) != NULL) {
            return test_ir_accepts_assignment_result_function_value_reassignment_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-WRAPPED-RETURNED-FNVAL-REASSIGN", filter) != NULL) {
            return test_ir_accepts_wrapped_returned_function_value_reassignment_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-WRAPPED-RETURNED-FNVAL-LOCAL-INIT", filter) != NULL) {
            return test_ir_accepts_wrapped_returned_function_value_local_initializer_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-WRAPPED-RETURNED-ZERO-FNVAL-LOCAL-INIT", filter) != NULL) {
            return test_ir_accepts_wrapped_returned_zero_arg_function_value_local_initializer_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-ASSIGNMENT-RESULT-RETURNED-CLOSURE-LOCAL-INIT", filter) != NULL) {
            return test_ir_accepts_assignment_result_returned_closure_local_initializer_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-WRAPPED-RETURNED-CALL-TERNARY-FNVAL-LOCAL-INIT", filter) != NULL) {
            return test_ir_accepts_wrapped_returned_call_ternary_function_value_local_initializer_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-WRAPPED-RETURNED-CALL-TERNARY-CLOSURE-LOCAL-INIT", filter) != NULL) {
            return test_ir_accepts_wrapped_returned_call_ternary_closure_local_initializer_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-WRAPPED-RETURNED-CALL-TERNARY-FNVAL-BOUNCE", filter) != NULL) {
            return test_ir_accepts_wrapped_returned_call_ternary_function_value_local_bounce_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-WRAPPED-RETURNED-CALL-TERNARY-FNVAL-FORWARD", filter) != NULL) {
            return test_ir_accepts_wrapped_returned_call_ternary_function_value_forward_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-WRAPPED-RETURNED-CALL-TERNARY-CLOSURE-BOUNCE", filter) != NULL) {
            return test_ir_accepts_wrapped_returned_call_ternary_closure_local_bounce_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-WRAPPED-RETURNED-CALL-TERNARY-CLOSURE-FORWARD", filter) != NULL) {
            return test_ir_accepts_wrapped_returned_call_ternary_closure_forward_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-WRAPPED-RETURNED-CALL-TERNARY-FNVAL-BIND-RETURN", filter) != NULL) {
            return test_ir_accepts_wrapped_returned_call_ternary_function_value_bind_return_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-WRAPPED-RETURNED-CALL-TERNARY-FNVAL-BIND-RETURN-ACTUAL", filter) != NULL) {
            return test_ir_accepts_wrapped_returned_call_ternary_function_value_bind_return_actual_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-WRAPPED-RETURNED-CALL-TERNARY-CLOSURE-BIND-RETURN", filter) != NULL) {
            return test_ir_accepts_wrapped_returned_call_ternary_closure_bind_return_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-WRAPPED-RETURNED-CALL-TERNARY-CLOSURE-BIND-RETURN-ACTUAL", filter) != NULL) {
            return test_ir_accepts_wrapped_returned_call_ternary_closure_bind_return_actual_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-FNVAL-BIND-RETURN", filter) != NULL) {
            return test_ir_accepts_wrapped_returned_call_ternary_zero_arg_function_value_bind_return_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-FNVAL-BIND-RETURN-ACTUAL", filter) != NULL) {
            return test_ir_accepts_wrapped_returned_call_ternary_zero_arg_function_value_bind_return_actual_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-VOID-FNVAL-BIND-RETURN", filter) != NULL) {
            return test_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_function_value_bind_return_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-VOID-FNVAL-BIND-RETURN-ACTUAL", filter) != NULL) {
            return test_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_function_value_bind_return_actual_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-CLOSURE-BIND-RETURN", filter) != NULL) {
            return test_ir_accepts_wrapped_returned_call_ternary_zero_arg_closure_bind_return_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-CLOSURE-BIND-RETURN-ACTUAL", filter) != NULL) {
            return test_ir_accepts_wrapped_returned_call_ternary_zero_arg_closure_bind_return_actual_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-VOID-CLOSURE-BIND-RETURN", filter) != NULL) {
            return test_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_closure_bind_return_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-VOID-CLOSURE-BIND-RETURN-ACTUAL", filter) != NULL) {
            return test_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_closure_bind_return_actual_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-WRAPPED-RETURNED-CALL-TERNARY-FNVAL-BOUNCE-PASSTHROUGH-BIND-RETURN", filter) != NULL) {
            return test_ir_accepts_wrapped_returned_call_ternary_function_value_bounce_passthrough_bind_return_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-WRAPPED-RETURNED-CALL-TERNARY-FNVAL-BOUNCE-PASSTHROUGH-BIND-RETURN-ACTUAL", filter) != NULL) {
            return test_ir_accepts_wrapped_returned_call_ternary_function_value_bounce_passthrough_bind_return_actual_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-WRAPPED-RETURNED-CALL-TERNARY-CLOSURE-BOUNCE-PASSTHROUGH-BIND-RETURN", filter) != NULL) {
            return test_ir_accepts_wrapped_returned_call_ternary_closure_bounce_passthrough_bind_return_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-WRAPPED-RETURNED-CALL-TERNARY-CLOSURE-BOUNCE-PASSTHROUGH-BIND-RETURN-ACTUAL", filter) != NULL) {
            return test_ir_accepts_wrapped_returned_call_ternary_closure_bounce_passthrough_bind_return_actual_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-FNVAL-BOUNCE-PASSTHROUGH-BIND-RETURN", filter) != NULL) {
            return test_ir_accepts_wrapped_returned_call_ternary_zero_arg_function_value_bounce_passthrough_bind_return_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-FNVAL-BOUNCE-PASSTHROUGH-BIND-RETURN-ACTUAL", filter) != NULL) {
            return test_ir_accepts_wrapped_returned_call_ternary_zero_arg_function_value_bounce_passthrough_bind_return_actual_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-VOID-FNVAL-BOUNCE-PASSTHROUGH-BIND-RETURN", filter) != NULL) {
            return test_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_function_value_bounce_passthrough_bind_return_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-VOID-FNVAL-BOUNCE-PASSTHROUGH-BIND-RETURN-ACTUAL", filter) != NULL) {
            return test_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_function_value_bounce_passthrough_bind_return_actual_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-CLOSURE-BOUNCE-PASSTHROUGH-BIND-RETURN", filter) != NULL) {
            return test_ir_accepts_wrapped_returned_call_ternary_zero_arg_closure_bounce_passthrough_bind_return_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-CLOSURE-BOUNCE-PASSTHROUGH-BIND-RETURN-ACTUAL", filter) != NULL) {
            return test_ir_accepts_wrapped_returned_call_ternary_zero_arg_closure_bounce_passthrough_bind_return_actual_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-VOID-CLOSURE-BOUNCE-PASSTHROUGH-BIND-RETURN", filter) != NULL) {
            return test_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_closure_bounce_passthrough_bind_return_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-VOID-CLOSURE-BOUNCE-PASSTHROUGH-BIND-RETURN-ACTUAL", filter) != NULL) {
            return test_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_closure_bounce_passthrough_bind_return_actual_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-WRAPPED-RETURNED-CALL-TERNARY-FNVAL-STMT-REASSIGN-BIND-RETURN", filter) != NULL) {
            return test_ir_accepts_wrapped_returned_call_ternary_function_value_statement_reassign_bind_return_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-WRAPPED-RETURNED-CALL-TERNARY-FNVAL-STMT-REASSIGN-BIND-RETURN-ACTUAL", filter) != NULL) {
            return test_ir_accepts_wrapped_returned_call_ternary_function_value_statement_reassign_bind_return_actual_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-WRAPPED-RETURNED-CALL-TERNARY-CLOSURE-STMT-REASSIGN-BIND-RETURN", filter) != NULL) {
            return test_ir_accepts_wrapped_returned_call_ternary_closure_statement_reassign_bind_return_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-WRAPPED-RETURNED-CALL-TERNARY-CLOSURE-STMT-REASSIGN-BIND-RETURN-ACTUAL", filter) != NULL) {
            return test_ir_accepts_wrapped_returned_call_ternary_closure_statement_reassign_bind_return_actual_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-FNVAL-STMT-REASSIGN-BIND-RETURN", filter) != NULL) {
            return test_ir_accepts_wrapped_returned_call_ternary_zero_arg_function_value_statement_reassign_bind_return_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-FNVAL-STMT-REASSIGN-BIND-RETURN-ACTUAL", filter) != NULL) {
            return test_ir_accepts_wrapped_returned_call_ternary_zero_arg_function_value_statement_reassign_bind_return_actual_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-VOID-FNVAL-STMT-REASSIGN-BIND-RETURN", filter) != NULL) {
            return test_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_function_value_statement_reassign_bind_return_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-VOID-FNVAL-STMT-REASSIGN-BIND-RETURN-ACTUAL", filter) != NULL) {
            return test_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_function_value_statement_reassign_bind_return_actual_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-CLOSURE-STMT-REASSIGN-BIND-RETURN", filter) != NULL) {
            return test_ir_accepts_wrapped_returned_call_ternary_zero_arg_closure_statement_reassign_bind_return_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-CLOSURE-STMT-REASSIGN-BIND-RETURN-ACTUAL", filter) != NULL) {
            return test_ir_accepts_wrapped_returned_call_ternary_zero_arg_closure_statement_reassign_bind_return_actual_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-VOID-CLOSURE-STMT-REASSIGN-BIND-RETURN", filter) != NULL) {
            return test_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_closure_statement_reassign_bind_return_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-VOID-CLOSURE-STMT-REASSIGN-BIND-RETURN-ACTUAL", filter) != NULL) {
            return test_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_closure_statement_reassign_bind_return_actual_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-ASSIGNMENT-RESULT-RETURNED-CLOSURE-REASSIGN", filter) != NULL) {
            return test_ir_accepts_assignment_result_returned_closure_reassignment_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-DYNAMIC-RETURNED-ZERO-ARG-CLOSURE-STATEMENT-PASSTHROUGH-CALL", filter) != NULL) {
            return test_ir_accepts_dynamic_returned_zero_arg_closure_statement_passthrough_call_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-DYNAMIC-RETURNED-ZERO-ARG-VOID-CLOSURE-STATEMENT-PASSTHROUGH-CALL", filter) != NULL) {
            return test_ir_accepts_dynamic_returned_zero_arg_void_closure_statement_passthrough_call_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-DYNAMIC-RETURNED-CLOSURE-STATEMENT-PASSTHROUGH-CALL", filter) != NULL) {
            return test_ir_accepts_dynamic_returned_closure_statement_passthrough_call_under_extension()
                ? 0
                : 1;
        }
        if (strstr("IR-PASSTHROUGH-TERNARY-CLOSURE-LOCAL-INIT", filter) != NULL) {
            return test_ir_accepts_passthrough_ternary_closure_local_initializer_under_extension() ? 0 : 1;
        }
        if (strstr("IR-PASSTHROUGH-TERNARY-NONCAPTURING-RETURN-IMMEDIATE", filter) != NULL) {
            return test_ir_accepts_passthrough_ternary_noncapturing_function_value_return_immediate_call_under_extension() ? 0 : 1;
        }
        if (strstr("IR-PASSTHROUGH-TERNARY-NONCAPTURING-ACTUAL", filter) != NULL) {
            return test_ir_accepts_passthrough_ternary_noncapturing_function_value_actual_argument_under_extension() ? 0 : 1;
        }
        if (strstr("IR-PASSTHROUGH-TERNARY-CLOSURE-ACTUAL", filter) != NULL) {
            return test_ir_accepts_passthrough_ternary_closure_function_value_actual_argument_under_extension() ? 0 : 1;
        }
        if (strstr("IR-PASSTHROUGH-TERNARY-ZERO-ACTUAL", filter) != NULL) {
            return test_ir_accepts_passthrough_ternary_zero_arg_function_value_actual_argument_under_extension() ? 0 : 1;
        }
        if (strstr("IR-PASSTHROUGH-TERNARY-ZERO-VOID-ACTUAL", filter) != NULL) {
            return test_ir_accepts_passthrough_ternary_zero_arg_void_function_value_actual_argument_under_extension() ? 0 : 1;
        }
        if (strstr("IR-PASSTHROUGH-TERNARY-ZERO-CLOSURE-ACTUAL", filter) != NULL) {
            return test_ir_accepts_passthrough_ternary_zero_arg_closure_function_value_actual_argument_under_extension() ? 0 : 1;
        }
        if (strstr("IR-PASSTHROUGH-TERNARY-ZERO-VOID-CLOSURE-ACTUAL", filter) != NULL) {
            return test_ir_accepts_passthrough_ternary_zero_arg_void_closure_function_value_actual_argument_under_extension() ? 0 : 1;
        }
        if (strstr("IR-RETURNED-CLOSURE-FORWARD", filter) != NULL) {
            return test_ir_accepts_returned_closure_forwarding_into_function_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("IR-RETURNED-CLOSURE-ALIAS-FORWARD", filter) != NULL) {
            return test_ir_accepts_returned_closure_alias_forwarding_into_function_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("IR-RETURNED-SINGLE-CAPTURE-CLOSURE-FORWARD", filter) != NULL) {
            return test_ir_accepts_returned_single_capture_closure_forwarding_into_function_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("IR-RETURNED-MULTI-CAPTURE-CLOSURE-FORWARD", filter) != NULL) {
            return test_ir_accepts_returned_multi_capture_closure_forwarding_into_function_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("IR-RETURNED-SINGLE-CAPTURE-CLOSURE-ALIAS-FORWARD", filter) != NULL) {
            return test_ir_accepts_returned_single_capture_closure_alias_forwarding_into_function_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("IR-RETURNED-SINGLE-CAPTURE-ZERO-CLOSURE-FORWARD", filter) != NULL) {
            return test_ir_accepts_returned_single_capture_zero_arg_closure_forwarding_into_function_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("IR-RETURNED-SINGLE-CAPTURE-ZERO-VOID-CLOSURE-FORWARD", filter) != NULL) {
            return test_ir_accepts_returned_single_capture_zero_arg_void_closure_forwarding_into_function_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("IR-DYNAMIC-RETURNED-LOCAL-CLOSURE-FORWARD", filter) != NULL) {
            return test_ir_accepts_dynamic_returned_local_closure_forwarding_into_function_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("IR-TERNARY-ZERO-CLOSURE-CALLEE", filter) != NULL) {
            return test_ir_accepts_ternary_zero_arg_closure_callee_under_extension() ? 0 : 1;
        }
        if (strstr("IR-TERNARY-ZERO-VOID-CLOSURE-CALLEE", filter) != NULL) {
            return test_ir_accepts_ternary_zero_arg_void_closure_callee_under_extension() ? 0 : 1;
        }
        if (strstr("IR-DYNAMIC-LOCAL-ZERO-CLOSURE-CALLEE", filter) != NULL) {
            return test_ir_accepts_dynamic_local_zero_arg_closure_direct_callee_under_extension() ? 0 : 1;
        }
        if (strstr("IR-DYNAMIC-LOCAL-ZERO-VOID-CLOSURE-CALLEE", filter) != NULL) {
            return test_ir_accepts_dynamic_local_zero_arg_void_closure_direct_callee_under_extension() ? 0 : 1;
        }
        if (strstr("IR-DYNAMIC-RUNTIME-LOCAL-ZERO-CLOSURE-CALLEE", filter) != NULL) {
            return test_ir_accepts_dynamic_runtime_local_zero_arg_closure_direct_callee_under_extension() ? 0 : 1;
        }
        if (strstr("IR-DYNAMIC-RUNTIME-LOCAL-ZERO-VOID-CLOSURE-CALLEE", filter) != NULL) {
            return test_ir_accepts_dynamic_runtime_local_zero_arg_void_closure_direct_callee_under_extension() ? 0 : 1;
        }
        if (strstr("IR-DIRECT-CLOSURE-LITERAL-ARG", filter) != NULL) {
            return test_ir_accepts_direct_closure_literal_argument_to_function_value_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("IR-ZERO-ARG-DIRECT-CLOSURE-LITERAL-ARG", filter) != NULL) {
            return test_ir_accepts_zero_arg_direct_closure_literal_argument_to_function_value_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("IR-VOID-DIRECT-CLOSURE-LITERAL-ARG", filter) != NULL) {
            return test_ir_accepts_void_direct_closure_literal_argument_to_function_value_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("IR-MULTI-CAPTURE-DIRECT-CLOSURE-LITERAL-ARG", filter) != NULL) {
            return test_ir_accepts_multi_capture_direct_closure_literal_argument_to_function_value_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("IR-CLOSURE-ALIAS-CHAIN-CALLABLE-OBJECT-TRANSPORT", filter) != NULL) {
            return test_ir_accepts_closure_alias_chain_callable_object_transport_under_extension() ? 0 : 1;
        }
        if (strstr("IR-CLOSURE-DIRECT-LOCAL-CALL-CALLABLE-OBJECT-TRANSPORT", filter) != NULL) {
            return test_ir_accepts_closure_direct_local_call_callable_object_transport_under_extension() ? 0 : 1;
        }
        if (strstr("IR-RICH-BODY-CLOSURE-DIRECT-LOCAL-CALL-CALLABLE-OBJECT-TRANSPORT", filter) != NULL) {
            return test_ir_accepts_rich_body_closure_direct_local_call_callable_object_transport_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-GLOBAL-LITERAL-RUNTIME-INIT", filter) != NULL) {
            return test_ir_lowers_float_literal_runtime_global_initializer_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-GLOBAL-OP-SEMANTIC-REJECT", filter) != NULL) {
            return test_ir_rejects_global_float_operator_expression_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-GLOBAL-OP-INIT-SEMANTIC-REJECT", filter) != NULL) {
            return test_ir_rejects_global_float_operator_expression_in_initializer_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-GLOBAL-CALL-INIT-TYPE-REJECT", filter) != NULL) {
            return test_ir_rejects_global_float_call_result_in_initializer_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-ASSIGN-TRANSPORT", filter) != NULL) {
            return test_ir_accepts_float_assignment_transport_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-ASSIGN-TYPE-REJECT", filter) != NULL) {
            return test_ir_rejects_float_assignment_to_int_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-IF-COND-ACCEPT", filter) != NULL) {
            return test_ir_rejects_float_if_condition_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-WHILE-COND-ACCEPT", filter) != NULL) {
            return test_ir_rejects_float_while_condition_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-FOR-COND-ACCEPT", filter) != NULL) {
            return test_ir_rejects_float_for_condition_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-RECURSIVE-IF-COND-ACCEPT", filter) != NULL) {
            return test_ir_accepts_recursive_float_if_condition_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-RECURSIVE-WHILE-COND-ACCEPT", filter) != NULL) {
            return test_ir_accepts_recursive_float_while_condition_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-RECURSIVE-FOR-COND-ACCEPT", filter) != NULL) {
            return test_ir_accepts_recursive_float_for_condition_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-RECURSIVE-COND-TERNARY-NEIGHBOR-REJECT", filter) != NULL) {
            return test_ir_rejects_recursive_float_condition_with_ternary_neighbor_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-LOGICAL-COND-COMPOSE-ACCEPT", filter) != NULL) {
            return test_ir_accepts_float_logical_condition_composition_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-EQ-COMPARE-ACCEPT", filter) != NULL) {
            return test_ir_accepts_float_equality_compare_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-NE-COMPARE-ACCEPT", filter) != NULL) {
            return test_ir_accepts_float_inequality_compare_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-LT-COMPARE-ACCEPT", filter) != NULL) {
            return test_ir_accepts_float_relational_compare_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-CHAIN-ADD-EQ-COMPARE-ACCEPT", filter) != NULL) {
            return test_ir_accepts_chained_float_equality_compare_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-CHAIN-ADD-LT-COMPARE-ACCEPT", filter) != NULL) {
            return test_ir_accepts_chained_float_relational_compare_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-CHAIN-ADD-NE-COMPARE-ACCEPT", filter) != NULL) {
            return test_ir_accepts_chained_float_inequality_compare_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-CHAIN-ADD-LE-COMPARE-ACCEPT", filter) != NULL) {
            return test_ir_accepts_chained_float_le_compare_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-NESTED-MUL-DIV-EQ-COMPARE-ACCEPT", filter) != NULL) {
            return test_ir_accepts_nested_muldiv_float_equality_compare_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-NESTED-MUL-DIV-LT-COMPARE-ACCEPT", filter) != NULL) {
            return test_ir_accepts_nested_muldiv_float_relational_compare_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-NESTED-MUL-DIV-NE-COMPARE-ACCEPT", filter) != NULL) {
            return test_ir_accepts_nested_muldiv_float_inequality_compare_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-NESTED-MUL-DIV-GE-COMPARE-ACCEPT", filter) != NULL) {
            return test_ir_accepts_nested_muldiv_float_ge_compare_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-NEG-LITERAL-TRANSPORT", filter) != NULL) {
            return test_ir_accepts_negative_float_literal_transport_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-NEG-IDENT-TRANSPORT", filter) != NULL) {
            return test_ir_accepts_unary_minus_float_identifier_transport_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-NEG-ZERO-COND-ACCEPT", filter) != NULL) {
            return test_ir_accepts_negative_zero_float_condition_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-NEG-LT-ZERO-ACCEPT", filter) != NULL) {
            return test_ir_accepts_negative_float_relational_compare_against_zero_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-NEG-ZERO-LE-ZERO-ACCEPT", filter) != NULL) {
            return test_ir_accepts_negative_zero_le_zero_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-ADD-ACCEPT", filter) != NULL) {
            return test_ir_accepts_float_addition_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-SUB-ACCEPT", filter) != NULL) {
            return test_ir_accepts_float_subtraction_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-EXPLICIT-INT-FROM-FLOAT-ACCEPT", filter) != NULL) {
            return test_ir_accepts_explicit_int_from_float_conversion_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-EXPLICIT-FLOAT-FROM-INT-ACCEPT", filter) != NULL) {
            return test_ir_accepts_explicit_float_from_int_conversion_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-EXPLICIT-INT-FROM-FLOAT-TERNARY-BRIDGE-ACCEPT", filter) != NULL) {
            return test_ir_accepts_explicit_int_from_float_ternary_bridge_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-EXPLICIT-INT-FROM-RECURSIVE-CALLARG-BRIDGE-ACCEPT", filter) != NULL) {
            return test_ir_accepts_explicit_int_from_recursive_float_call_argument_bridge_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-EXPLICIT-INT-FROM-RECURSIVE-INIT-BRIDGE-ACCEPT", filter) != NULL) {
            return test_ir_accepts_explicit_int_from_recursive_float_initializer_bridge_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-EXPLICIT-INT-FROM-RECURSIVE-ASSIGN-BRIDGE-ACCEPT", filter) != NULL) {
            return test_ir_accepts_explicit_int_from_recursive_float_assignment_bridge_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-EXPLICIT-INT-FROM-FLOAT-COMPARE-BRIDGE-ACCEPT", filter) != NULL) {
            return test_ir_accepts_explicit_int_from_float_compare_bridge_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-EXPLICIT-INT-FROM-RECURSIVE-ARITH-BRIDGE-ACCEPT", filter) != NULL) {
            return test_ir_accepts_explicit_int_from_recursive_float_arithmetic_bridge_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-EXPLICIT-FLOAT-FROM-RECURSIVE-INIT-BRIDGE-ACCEPT", filter) != NULL) {
            return test_ir_accepts_explicit_float_from_recursive_int_initializer_bridge_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-EXPLICIT-FLOAT-FROM-RECURSIVE-ASSIGN-BRIDGE-ACCEPT", filter) != NULL) {
            return test_ir_accepts_explicit_float_from_recursive_int_assignment_bridge_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-EXPLICIT-FLOAT-FROM-INT-COMPARE-BRIDGE-ACCEPT", filter) != NULL) {
            return test_ir_accepts_explicit_float_from_int_compare_bridge_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-EXPLICIT-FLOAT-FROM-RECURSIVE-ARITH-BRIDGE-ACCEPT", filter) != NULL) {
            return test_ir_accepts_explicit_float_from_recursive_int_arithmetic_bridge_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-CONVERT-COND-ACCEPT", filter) != NULL) {
            return test_ir_accepts_explicit_float_condition_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-CONVERT-RECURSIVE-COND-ACCEPT", filter) != NULL) {
            return test_ir_accepts_recursive_explicit_float_condition_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-CONVERT-WHILE-COND-ACCEPT", filter) != NULL) {
            return test_ir_accepts_explicit_float_while_condition_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-CONVERT-LOGIC-COND-ACCEPT", filter) != NULL) {
            return test_ir_accepts_explicit_float_logical_condition_composition_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-NEG-ADD-COMBO-ACCEPT", filter) != NULL) {
            return test_ir_accepts_negative_float_addition_combo_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-NEG-SUB-COMBO-ACCEPT", filter) != NULL) {
            return test_ir_accepts_negative_float_subtraction_combo_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-MUL-ACCEPT", filter) != NULL) {
            return test_ir_accepts_float_multiplication_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-DIV-ACCEPT", filter) != NULL) {
            return test_ir_accepts_float_division_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-NEG-MUL-COMBO-ACCEPT", filter) != NULL) {
            return test_ir_accepts_negative_float_multiplication_combo_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-NEG-DIV-COMBO-ACCEPT", filter) != NULL) {
            return test_ir_accepts_negative_float_division_combo_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-CHAIN-ADD-ACCEPT", filter) != NULL) {
            return test_ir_accepts_chained_float_addition_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-NESTED-MUL-DIV-ACCEPT", filter) != NULL) {
            return test_ir_accepts_nested_float_mul_div_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-ARITH-INT-TYPE-REJECT", filter) != NULL) {
            return test_ir_rejects_mixed_float_int_arithmetic_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-LITERAL-ARITH-INT-TYPE-REJECT", filter) != NULL) {
            return test_ir_rejects_float_literal_int_arithmetic_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-CALL-ARITH-INT-TYPE-REJECT", filter) != NULL) {
            return test_ir_rejects_float_call_int_arithmetic_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-NEG-CALL-ARITH-INT-TYPE-REJECT", filter) != NULL) {
            return test_ir_rejects_negative_float_call_int_arithmetic_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-GLOBAL-COND-PLUS-INT-REJECT", filter) != NULL) {
            return test_ir_rejects_global_float_int_condition_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-CALL-COND-PLUS-INT-REJECT", filter) != NULL) {
            return test_ir_rejects_float_call_int_condition_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-NEG-CALL-COND-PLUS-INT-REJECT", filter) != NULL) {
            return test_ir_rejects_negative_float_call_int_condition_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-NESTED-TREE-COND-PLUS-INT-REJECT", filter) != NULL) {
            return test_ir_rejects_nested_float_tree_plus_int_condition_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-NESTED-MULDIV-COND-PLUS-INT-REJECT", filter) != NULL) {
            return test_ir_rejects_nested_float_muldiv_plus_int_condition_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-COMPARE-INT-TYPE-REJECT", filter) != NULL) {
            return test_ir_rejects_float_compare_against_int_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-NESTED-TREE-PLUS-INT-REJECT", filter) != NULL) {
            return test_ir_rejects_nested_float_tree_plus_int_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-NESTED-MULDIV-PLUS-INT-REJECT", filter) != NULL) {
            return test_ir_rejects_nested_float_muldiv_plus_int_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-TERNARY-VALUE-PLUS-INT-REJECT", filter) != NULL) {
            return test_ir_rejects_float_ternary_value_plus_int_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-UNARY-CALL-TERNARY-PLUS-INT-REJECT", filter) != NULL) {
            return test_ir_rejects_unary_call_ternary_value_plus_int_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-TERNARY-PLUS-FLOAT-CALLARG-REJECT", filter) != NULL) {
            return test_ir_rejects_float_ternary_value_plus_float_call_argument_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-UNARY-CALL-TERNARY-PLUS-FLOAT-CALLARG-REJECT", filter) != NULL) {
            return test_ir_rejects_unary_call_ternary_value_plus_float_call_argument_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-TERNARY-VALUE-RETURN-INT-REJECT", filter) != NULL) {
            return test_ir_rejects_float_ternary_value_return_to_int_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-UNARY-CALL-TERNARY-RETURN-INT-REJECT", filter) != NULL) {
            return test_ir_rejects_unary_call_ternary_value_return_to_int_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-TERNARY-VALUE-SEMANTIC-REJECT", filter) != NULL) {
            return test_ir_rejects_float_ternary_value_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-TERNARY-VALUE-ACCEPT", filter) != NULL) {
            return test_ir_accepts_same_type_float_ternary_value_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-TERNARY-VALUE-ASSIGN-FLOAT-ACCEPT", filter) != NULL) {
            return test_ir_accepts_float_ternary_value_assignment_to_float_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-TERNARY-VALUE-INIT-FLOAT-ACCEPT", filter) != NULL) {
            return test_ir_accepts_float_ternary_value_initializer_to_float_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-CHAIN-ADD-ASSIGN-FLOAT-ACCEPT", filter) != NULL) {
            return test_ir_accepts_chained_float_addition_assignment_to_float_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-NESTED-MUL-DIV-ASSIGN-FLOAT-ACCEPT", filter) != NULL) {
            return test_ir_accepts_nested_float_mul_div_assignment_to_float_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-CHAIN-ADD-INIT-FLOAT-ACCEPT", filter) != NULL) {
            return test_ir_accepts_chained_float_addition_initializer_to_float_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-NESTED-MUL-DIV-INIT-FLOAT-ACCEPT", filter) != NULL) {
            return test_ir_accepts_nested_float_mul_div_initializer_to_float_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-TERNARY-VALUE-CALLARG-FLOAT-ACCEPT", filter) != NULL) {
            return test_ir_accepts_float_ternary_value_call_argument_to_float_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-UNARY-CALL-TERNARY-CALLARG-FLOAT-ACCEPT", filter) != NULL) {
            return test_ir_accepts_unary_call_float_ternary_value_call_argument_to_float_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-CHAIN-ADD-CALLARG-FLOAT-ACCEPT", filter) != NULL) {
            return test_ir_accepts_chained_float_addition_call_argument_to_float_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-NESTED-MUL-DIV-CALLARG-FLOAT-ACCEPT", filter) != NULL) {
            return test_ir_accepts_nested_float_mul_div_call_argument_to_float_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-HELPER-TERNARY-CALL-ARITH-ACCEPT", filter) != NULL) {
            return test_ir_accepts_float_helper_wrapped_ternary_call_arithmetic_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-UNARY-HELPER-TERNARY-CALL-ARITH-ACCEPT", filter) != NULL) {
            return test_ir_accepts_unary_call_helper_wrapped_ternary_call_arithmetic_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-HELPER-TERNARY-CALL-COMPARE-ACCEPT", filter) != NULL) {
            return test_ir_accepts_float_helper_wrapped_ternary_call_compare_under_extension() ? 0 : 1;
        }
        if (strstr("IR-FLOAT-UNARY-HELPER-TERNARY-CALL-COMPARE-ACCEPT", filter) != NULL) {
            return test_ir_accepts_unary_call_helper_wrapped_ternary_call_compare_under_extension() ? 0 : 1;
        }
        if (strstr("IR-DYNAMIC-FNVAL-RETURN-BIND", filter) != NULL) {
            return test_ir_accepts_dynamic_noncapturing_function_value_return_bind_and_call_under_extension() ? 0 : 1;
        }
        if (strstr("IR-STATIC-TOPLEVEL-FNVAL-RETURN-BIND", filter) != NULL) {
            return test_ir_accepts_static_noncapturing_top_level_function_value_return_under_extension() ? 0 : 1;
        }
        if (strstr("IR-DYNAMIC-FNVAL-RETURN-IMMEDIATE", filter) != NULL) {
            return test_ir_accepts_dynamic_noncapturing_function_value_return_immediate_call_under_extension() ? 0 : 1;
        }
        if (strstr("IR-DYNAMIC-FNVAL-FORWARD", filter) != NULL) {
            return test_ir_accepts_dynamic_local_function_value_forwarding_into_function_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("IR-MULTI-STATIC-FNVAL-PARAM-NO-SHELL", filter) != NULL) {
            return test_ir_accepts_multiple_static_function_valued_parameters_without_specialization_shell_under_extension() ? 0 : 1;
        }
        if (strstr("IR-SECOND-ORDER-FNVAL-FORWARD", filter) != NULL) {
            return test_ir_accepts_second_order_function_value_parameter_forwarding_under_extension() ? 0 : 1;
        }
        if (strstr("IR-SECOND-ORDER-CLOSURE-FNVAL-FORWARD", filter) != NULL) {
            return test_ir_accepts_second_order_closure_function_value_parameter_forwarding_under_extension() ? 0 : 1;
        }
        if (strstr("IR-SECOND-ORDER-ZERO-FNVAL-FORWARD", filter) != NULL) {
            return test_ir_accepts_second_order_zero_arg_function_value_parameter_forwarding_under_extension() ? 0 : 1;
        }
        if (strstr("IR-SECOND-ORDER-ZERO-CLOSURE-FNVAL-FORWARD", filter) != NULL) {
            return test_ir_accepts_second_order_zero_arg_closure_function_value_parameter_forwarding_under_extension() ? 0 : 1;
        }
        if (strstr("IR-SECOND-ORDER-ZERO-VOID-FNVAL-FORWARD", filter) != NULL) {
            return test_ir_accepts_second_order_zero_arg_void_function_value_parameter_forwarding_under_extension() ? 0 : 1;
        }
        if (strstr("IR-SECOND-ORDER-ZERO-VOID-CLOSURE-FNVAL-FORWARD", filter) != NULL) {
            return test_ir_accepts_second_order_zero_arg_void_closure_function_value_parameter_forwarding_under_extension() ? 0 : 1;
        }
        if (strstr("IR-SECOND-ORDER-RETURNED-PASSTHROUGH-DYNAMIC-ZERO-VOID-FNVAL-FORWARD", filter) != NULL) {
            return test_ir_accepts_second_order_returned_passthrough_dynamic_zero_arg_void_function_value_parameter_forwarding_under_extension() ? 0 : 1;
        }
        if (strstr("IR-SECOND-ORDER-RETURNED-PASSTHROUGH-DYNAMIC-ZERO-FNVAL-FORWARD", filter) != NULL) {
            return test_ir_accepts_second_order_returned_passthrough_dynamic_zero_arg_function_value_parameter_forwarding_under_extension() ? 0 : 1;
        }
        if (strstr("IR-SECOND-ORDER-LOCAL-FNVAL-FORWARD", filter) != NULL) {
            return test_ir_accepts_second_order_local_function_value_parameter_forwarding_under_extension() ? 0 : 1;
        }
        if (strstr("IR-SECOND-ORDER-LOCAL-REASSIGNED-FNVAL-FORWARD", filter) != NULL) {
            return test_ir_accepts_second_order_local_reassigned_function_value_parameter_forwarding_under_extension() ? 0 : 1;
        }
        if (strstr("IR-SECOND-ORDER-LOCAL-SCALAR-REASSIGNED-FNVAL-FORWARD", filter) != NULL) {
            return test_ir_accepts_second_order_local_scalar_and_reassigned_function_value_parameter_forwarding_under_extension() ? 0 : 1;
        }
        if (strstr("IR-SECOND-ORDER-LOCAL-WRAPPER-SCALAR-UPDATE-FNVAL-FORWARD", filter) != NULL) {
            return test_ir_accepts_second_order_local_wrapper_scalar_update_function_value_parameter_forwarding_under_extension() ? 0 : 1;
        }
        if (strstr("IR-SECOND-ORDER-LOCAL-WRAPPER-REPEATED-SCALAR-UPDATE-FNVAL-FORWARD", filter) != NULL) {
            return test_ir_accepts_second_order_local_wrapper_repeated_scalar_update_function_value_parameter_forwarding_under_extension() ? 0 : 1;
        }
        if (strstr("IR-THIRD-ORDER-FNVAL-FORWARD", filter) != NULL) {
            return test_ir_accepts_third_order_function_value_parameter_forwarding_under_extension() ? 0 : 1;
        }
        if (strstr("IR-THIRD-ORDER-WRAPPER-SCALAR-UPDATE-FNVAL-FORWARD", filter) != NULL) {
            return test_ir_accepts_third_order_wrapper_scalar_update_function_value_parameter_forwarding_under_extension() ? 0 : 1;
        }
        if (strstr("IR-THIRD-ORDER-WRAPPER-REPEATED-SCALAR-UPDATE-FNVAL-FORWARD", filter) != NULL) {
            return test_ir_accepts_third_order_wrapper_repeated_scalar_update_function_value_parameter_forwarding_under_extension() ? 0 : 1;
        }
        if (strstr("IR-THIRD-ORDER-RETURNED-FNVAL-BIND", filter) != NULL) {
            return test_ir_accepts_third_order_returned_function_value_parameter_bind_under_extension() ? 0 : 1;
        }
        if (strstr("IR-THIRD-ORDER-LOCAL-FNVAL-FORWARD", filter) != NULL) {
            return test_ir_accepts_third_order_local_function_value_parameter_forwarding_under_extension() ? 0 : 1;
        }
        if (strstr("IR-SECOND-ORDER-RETURNED-FNVAL-IMM-CALL", filter) != NULL) {
            return test_ir_accepts_second_order_returned_function_value_parameter_immediate_call_under_extension() ? 0 : 1;
        }
        if (strstr("IR-SECOND-ORDER-RETURNED-CLOSURE-FNVAL-IMM-CALL", filter) != NULL) {
            return test_ir_accepts_second_order_returned_closure_function_value_parameter_immediate_call_under_extension() ? 0 : 1;
        }
        if (strstr("IR-SECOND-ORDER-DYNAMIC-RETURNED-FNVAL-IMM-CALL", filter) != NULL) {
            return test_ir_accepts_second_order_dynamic_returned_function_value_parameter_immediate_call_under_extension() ? 0 : 1;
        }
        if (strstr("IR-SECOND-ORDER-RETURNED-PASSTHROUGH-DYNAMIC-FNVAL-IMM-CALL", filter) != NULL) {
            return test_ir_accepts_second_order_returned_passthrough_dynamic_function_value_parameter_immediate_call_under_extension() ? 0 : 1;
        }
        if (strstr("IR-SECOND-ORDER-DYNAMIC-LOCAL-FNVAL-FORWARD", filter) != NULL) {
            return test_ir_accepts_second_order_dynamic_local_function_value_parameter_forwarding_under_extension() ? 0 : 1;
        }
        if (strstr("IR-SECOND-ORDER-DYNAMIC-LOCAL-CLOSURE-FNVAL-FORWARD", filter) != NULL) {
            return test_ir_accepts_second_order_dynamic_local_closure_function_value_parameter_forwarding_under_extension() ? 0 : 1;
        }
        if (strstr("IR-SECOND-ORDER-RETURNED-PASSTHROUGH-DYNAMIC-LOCAL-FNVAL-FORWARD", filter) != NULL) {
            return test_ir_accepts_second_order_returned_passthrough_dynamic_local_function_value_parameter_forwarding_under_extension() ? 0 : 1;
        }
        if (strstr("IR-SECOND-ORDER-RETURNED-PASSTHROUGH-DYNAMIC-LOCAL-FNVAL-UNARY-EVAL", filter) != NULL) {
            return test_ir_accepts_second_order_returned_passthrough_dynamic_local_function_value_parameter_forwarding_with_unary_helper_eval_under_extension() ? 0 : 1;
        }
        if (strstr("IR-SECOND-ORDER-RETURNED-PASSTHROUGH-DYNAMIC-LOCAL-FNVAL-COMPOUND-UPDATE-EVAL", filter) != NULL) {
            return test_ir_accepts_second_order_returned_passthrough_dynamic_local_function_value_parameter_forwarding_with_compound_update_helper_eval_under_extension() ? 0 : 1;
        }
        if (strstr("IR-SECOND-ORDER-RETURNED-PASSTHROUGH-DYNAMIC-LOCAL-FNVAL-COMMA-EVAL", filter) != NULL) {
            return test_ir_accepts_second_order_returned_passthrough_dynamic_local_function_value_parameter_forwarding_with_comma_helper_eval_under_extension() ? 0 : 1;
        }
        if (strstr("IR-SECOND-ORDER-RETURNED-PASSTHROUGH-DYNAMIC-LOCAL-FNVAL-TERNARY-EVAL", filter) != NULL) {
            return test_ir_accepts_second_order_returned_passthrough_dynamic_local_function_value_parameter_forwarding_with_ternary_helper_eval_under_extension() ? 0 : 1;
        }
        if (strstr("IR-SECOND-ORDER-RETURNED-PASSTHROUGH-DYNAMIC-LOCAL-FNVAL-STMT-CALL-PREFIX", filter) != NULL) {
            return test_ir_accepts_second_order_returned_passthrough_dynamic_local_function_value_parameter_forwarding_with_statement_call_prefix_under_extension() ? 0 : 1;
        }
        if (strstr("IR-SECOND-ORDER-RETURNED-PASSTHROUGH-DYNAMIC-LOCAL-FNVAL-IF-RETURN-EVAL", filter) != NULL) {
            return test_ir_accepts_second_order_returned_passthrough_dynamic_local_function_value_parameter_forwarding_with_if_return_helper_eval_under_extension() ? 0 : 1;
        }
        if (strstr("IR-SECOND-ORDER-RETURNED-CLOSURE-FNVAL-FORWARD", filter) != NULL) {
            return test_ir_accepts_second_order_returned_closure_function_value_parameter_forwarding_under_extension() ? 0 : 1;
        }
        if (strstr("IR-CLOSURE-CAPTURE-STATIC-FNVAL", filter) != NULL) {
            return test_ir_accepts_static_function_value_capture_inside_closure_under_extension() ? 0 : 1;
        }
        if (strstr("IR-CLOSURE-CAPTURE-PARAM-FNVAL", filter) != NULL) {
            return test_ir_accepts_function_parameter_capture_inside_closure_under_extension() ? 0 : 1;
        }
        if (strstr("IR-RETURNED-CLOSURE-CAPTURE-PARAM-FNVAL", filter) != NULL) {
            return test_ir_accepts_returned_function_parameter_capture_inside_closure_under_extension() ? 0 : 1;
        }
        if (strstr("IR-PASSTHROUGH-DECL-LOCAL-FNVAL-NO-SHELL", filter) != NULL) {
            return test_ir_accepts_passthrough_decl_local_function_value_forwarding_without_specialization_shell_under_extension() ? 0 : 1;
        }
        if (strstr("IR-SECOND-ORDER-DYNAMIC-TERNARY-FNVAL-ACTUAL", filter) != NULL) {
            return test_ir_accepts_second_order_dynamic_ternary_function_value_actual_argument_under_extension() ? 0 : 1;
        }
        if (strstr("IR-PARAM-LOCAL-FNVAL-DIRECT-NO-SHELL", filter) != NULL) {
            return test_ir_accepts_parameter_local_function_value_direct_call_without_specialization_shell_under_extension() ? 0 : 1;
        }
        if (strstr("IR-PARAM-LOCAL-ZERO-FNVAL-DIRECT-NO-SHELL", filter) != NULL) {
            return test_ir_accepts_parameter_local_zero_arg_function_value_direct_call_without_specialization_shell_under_extension() ? 0 : 1;
        }
        if (strstr("IR-PARAM-LOCAL-CLOSURE-DIRECT-NO-SHELL", filter) != NULL) {
            return test_ir_accepts_parameter_local_closure_direct_call_without_specialization_shell_under_extension() ? 0 : 1;
        }
        if (strstr("IR-PARAM-LOCAL-CLOSURE-FORWARD-NO-SHELL", filter) != NULL) {
            return test_ir_accepts_parameter_local_closure_forward_without_specialization_shell_under_extension() ? 0 : 1;
        }
        if (strstr("IR-PARAM-LOCAL-ZERO-VOID-FNVAL-DIRECT-NO-SHELL", filter) != NULL) {
            return test_ir_accepts_parameter_local_zero_arg_void_function_value_direct_call_without_specialization_shell_under_extension() ? 0 : 1;
        }
        if (strstr("IR-PARAM-LOCAL-ZERO-VOID-FNVAL-FORWARD-NO-SHELL", filter) != NULL) {
            return test_ir_accepts_parameter_local_zero_arg_void_function_value_forward_without_specialization_shell_under_extension() ? 0 : 1;
        }
        if (strstr("IR-DYNAMIC-FNVAL-ZERO-FORWARD", filter) != NULL) {
            return test_ir_accepts_dynamic_local_zero_arg_function_value_forwarding_into_function_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("IR-DYNAMIC-FNVAL-ZERO-VOID-FORWARD", filter) != NULL) {
            return test_ir_accepts_dynamic_local_zero_arg_void_function_value_forwarding_into_function_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("IR-DYNAMIC-RETURNED-ZERO-FORWARD", filter) != NULL) {
            return test_ir_accepts_dynamic_returned_zero_arg_function_value_forwarding_into_function_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("IR-DYNAMIC-FNVAL-ASSIGN-DIRECT", filter) != NULL) {
            return test_ir_accepts_dynamic_function_value_assignment_then_direct_call_under_extension() ? 0 : 1;
        }
        if (strstr("IR-DYNAMIC-FNVAL-ASSIGN-RETURN", filter) != NULL) {
            return test_ir_accepts_dynamic_function_value_assignment_then_return_under_extension() ? 0 : 1;
        }
        if (strstr("IR-DYNAMIC-WRAPPED-FNVAL-RETURN", filter) != NULL) {
            return test_ir_accepts_dynamic_wrapped_function_value_return_under_extension() ? 0 : 1;
        }
        if (strstr("IR-TERNARY-FNVAL-RETURN-IMMEDIATE", filter) != NULL) {
            return test_ir_accepts_ternary_noncapturing_function_value_return_immediate_call_under_extension() ? 0 : 1;
        }
        if (strstr("IR-TERNARY-FNVAL-RETURN-BIND", filter) != NULL) {
            return test_ir_accepts_ternary_noncapturing_function_value_return_bind_and_call_under_extension() ? 0 : 1;
        }
        if (strstr("IR-TERNARY-FNVAL-CALLEE", filter) != NULL) {
            return test_ir_accepts_ternary_function_value_callee_under_extension() ? 0 : 1;
        }
        if (strstr("IR-TERNARY-MULTI-CAPTURE-CLOSURE-CALLEE", filter) != NULL) {
            return test_ir_accepts_ternary_multi_capture_closure_callee_under_extension() ? 0 : 1;
        }
        if (strstr("IR-COMMA-WRAPPED-CLOSURE-FORWARD", filter) != NULL) {
            return test_ir_accepts_comma_wrapped_closure_forward_into_function_value_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("IR-ASSIGNMENT-RESULT-CLOSURE-FORWARD", filter) != NULL) {
            return test_ir_accepts_assignment_result_closure_forward_into_function_value_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("IR-DYNAMIC-COMMA-WRAPPED-CLOSURE-FORWARD", filter) != NULL) {
            return test_ir_accepts_dynamic_comma_wrapped_closure_forward_into_function_value_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("IR-DYNAMIC-ASSIGNMENT-RESULT-CLOSURE-FORWARD", filter) != NULL) {
            return test_ir_accepts_dynamic_assignment_result_closure_forward_into_function_value_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("IR-DYNAMIC-COMMA-WRAPPED-ZERO-CLOSURE-FORWARD", filter) != NULL) {
            return test_ir_accepts_dynamic_comma_wrapped_zero_arg_closure_forward_into_function_value_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("IR-DYNAMIC-ASSIGNMENT-RESULT-ZERO-CLOSURE-FORWARD", filter) != NULL) {
            return test_ir_accepts_dynamic_assignment_result_zero_arg_closure_forward_into_function_value_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("IR-DYNAMIC-COMMA-WRAPPED-ZERO-VOID-CLOSURE-FORWARD", filter) != NULL) {
            return test_ir_accepts_dynamic_comma_wrapped_zero_arg_void_closure_forward_into_function_value_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("IR-DYNAMIC-ASSIGNMENT-RESULT-ZERO-VOID-CLOSURE-FORWARD", filter) != NULL) {
            return test_ir_accepts_dynamic_assignment_result_zero_arg_void_closure_forward_into_function_value_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("IR-DYNAMIC-COMMA-WRAPPED-FNVAL-FORWARD", filter) != NULL) {
            return test_ir_accepts_dynamic_comma_wrapped_function_value_forwarding_into_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("IR-DYNAMIC-ASSIGNMENT-RESULT-FNVAL-FORWARD", filter) != NULL) {
            return test_ir_accepts_dynamic_assignment_result_function_value_forwarding_into_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("IR-DYNAMIC-COMMA-WRAPPED-ZERO-FNVAL-FORWARD", filter) != NULL) {
            return test_ir_accepts_dynamic_comma_wrapped_zero_arg_function_value_forwarding_into_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("IR-DYNAMIC-ASSIGNMENT-RESULT-ZERO-FNVAL-FORWARD", filter) != NULL) {
            return test_ir_accepts_dynamic_assignment_result_zero_arg_function_value_forwarding_into_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("IR-DYNAMIC-COMMA-WRAPPED-ZERO-VOID-FNVAL-FORWARD", filter) != NULL) {
            return test_ir_accepts_dynamic_comma_wrapped_zero_arg_void_function_value_forwarding_into_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("IR-DYNAMIC-ASSIGNMENT-RESULT-ZERO-VOID-FNVAL-FORWARD", filter) != NULL) {
            return test_ir_accepts_dynamic_assignment_result_zero_arg_void_function_value_forwarding_into_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("IR-TERNARY-CLOSURE-CALLEE", filter) != NULL) {
            return test_ir_accepts_ternary_closure_callee_under_extension() ? 0 : 1;
        }
    }

    ok &= test_ir_lowers_return_literal();
    ok &= test_ir_reports_runtime_global_dependency_cycle_chain_with_callee_segments();
    ok &= test_ir_lowers_return_parameter();
    ok &= test_ir_lowers_pair_return_via_hidden_result_slots_under_extension();
    ok &= test_ir_lowers_pair_call_init_via_hidden_result_slots_under_extension();
    ok &= test_ir_lowers_pair_call_result_as_aggregate_call_argument_under_extension();
    ok &= test_ir_lowers_pair_multi_hop_return_chain_under_extension();
    ok &= test_ir_lowers_nested_struct_multi_hop_return_chain_under_extension();
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
    ok &= test_ir_rejects_runtime_global_initializer_dep_on_uninitialized_global_array_subscript();
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
    ok &= test_ir_accepts_runtime_global_array_initializer_with_short_circuit_and_calls();
    ok &= test_ir_accepts_multiple_runtime_global_initializers_after_branchy_helper_growth();
    ok &= test_ir_lowers_1d_array_initializer_mixing_nested_and_scalar_items();
    ok &= test_ir_compacts_sparse_large_local_array_initializer();
    ok &= test_ir_accepts_2d_array_initializer_mixing_nested_and_scalar_items();
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
    ok &= test_ir_preserves_global_array_declaration_metadata();
    ok &= test_ir_lowers_unused_local_array_declarations();
    ok &= test_ir_preserves_array_parameter_signature_metadata();
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
    ok &= test_ir_keeps_for_exit_when_function_step_updates_local_condition_value();
    ok &= test_ir_keeps_while_exit_when_postfix_update_changes_condition_local();
    ok &= test_ir_keeps_for_exit_when_compound_step_changes_condition_local();
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
    ok &= test_ir_lowers_defer_before_return();
    ok &= test_ir_lowers_nested_defer_fallthrough_order();
    ok &= test_ir_lowers_defer_with_loop_break();
    ok &= test_ir_lowers_defer_with_loop_continue();
    ok &= test_ir_lowers_nested_defer_body_scope_exit_order();
    ok &= test_ir_lowers_nested_multi_defer_body_lifo_order();
    ok &= test_ir_lowers_defer_using_exit_time_local_value();
    ok &= test_ir_lowers_defer_if_else_using_exit_time_condition_value();
    ok &= test_ir_lowers_return_unwinding_inner_and_outer_defers();
    ok &= test_ir_lowers_return_expression_before_defer_side_effects();
    ok &= test_ir_lowers_return_expression_side_effect_before_defer_unwind();
    ok &= test_ir_lowers_defer_in_for_body_before_step_update();
    ok &= test_ir_lowers_for_return_unwinding_body_then_outer_defers_without_step();
    ok &= test_ir_lowers_for_break_unwinding_body_then_outer_defers_without_step();
    ok &= test_ir_lowers_unless_as_negated_if_under_extension();
    ok &= test_ir_lowers_zero_arg_function_valued_parameter_specialization_under_extension();
    ok &= test_ir_lowers_zero_arg_void_function_valued_parameter_specialization_under_extension();
    ok &= test_ir_lowers_capdefer_snapshot_bindings_under_extension();
    ok &= test_ir_lowers_capdefer_multiple_snapshot_bindings_under_extension();
    ok &= test_ir_lowers_multiple_fndefer_registrations_inside_loop_under_extension();
    ok &= test_ir_lowers_float_transport_signature_under_extension();
    ok &= test_ir_lowers_float_literal_transport_under_extension();
    ok &= test_ir_lowers_float_global_initializer_transport_from_identifier_under_extension();
    ok &= test_ir_lowers_float_global_initializer_transport_from_call_under_extension();
    ok &= test_ir_lowers_float_return_transport_from_global_under_extension();
    ok &= test_ir_lowers_float_return_transport_from_global_call_under_extension();
    ok &= test_ir_lowers_float_global_call_chain_transport_under_extension();
    ok &= test_ir_lowers_float_local_call_chain_transport_under_extension();
    ok &= test_ir_lowers_float_literal_runtime_global_initializer_under_extension();
    ok &= test_ir_rejects_global_float_operator_expression_under_extension();
    ok &= test_ir_rejects_global_float_operator_expression_in_initializer_under_extension();
    ok &= test_ir_rejects_global_float_call_result_in_initializer_under_extension();
    ok &= test_ir_accepts_float_assignment_transport_under_extension();
    ok &= test_ir_rejects_float_assignment_to_int_under_extension();
    ok &= test_ir_rejects_float_if_condition_under_extension();
    ok &= test_ir_rejects_float_while_condition_under_extension();
    ok &= test_ir_rejects_float_for_condition_under_extension();
    ok &= test_ir_accepts_recursive_float_if_condition_under_extension();
    ok &= test_ir_accepts_recursive_float_while_condition_under_extension();
    ok &= test_ir_accepts_recursive_float_for_condition_under_extension();
    ok &= test_ir_rejects_recursive_float_condition_with_ternary_neighbor_under_extension();
    ok &= test_ir_accepts_float_logical_condition_composition_under_extension();
    ok &= test_ir_accepts_float_equality_compare_under_extension();
    ok &= test_ir_accepts_float_inequality_compare_under_extension();
    ok &= test_ir_accepts_float_relational_compare_under_extension();
    ok &= test_ir_accepts_chained_float_equality_compare_under_extension();
    ok &= test_ir_accepts_chained_float_relational_compare_under_extension();
    ok &= test_ir_accepts_chained_float_inequality_compare_under_extension();
    ok &= test_ir_accepts_chained_float_le_compare_under_extension();
    ok &= test_ir_accepts_nested_muldiv_float_equality_compare_under_extension();
    ok &= test_ir_accepts_nested_muldiv_float_relational_compare_under_extension();
    ok &= test_ir_accepts_nested_muldiv_float_inequality_compare_under_extension();
    ok &= test_ir_accepts_nested_muldiv_float_ge_compare_under_extension();
    ok &= test_ir_accepts_negative_float_literal_transport_under_extension();
    ok &= test_ir_accepts_unary_minus_float_identifier_transport_under_extension();
    ok &= test_ir_accepts_negative_zero_float_condition_under_extension();
    ok &= test_ir_accepts_negative_float_relational_compare_against_zero_under_extension();
    ok &= test_ir_accepts_negative_zero_le_zero_under_extension();
    ok &= test_ir_accepts_float_addition_under_extension();
    ok &= test_ir_accepts_float_subtraction_under_extension();
    ok &= test_ir_accepts_explicit_int_from_float_conversion_under_extension();
    ok &= test_ir_accepts_explicit_float_from_int_conversion_under_extension();
    ok &= test_ir_accepts_explicit_int_from_float_ternary_bridge_under_extension();
    ok &= test_ir_accepts_explicit_int_from_recursive_float_call_argument_bridge_under_extension();
    ok &= test_ir_accepts_explicit_int_from_recursive_float_initializer_bridge_under_extension();
    ok &= test_ir_accepts_explicit_int_from_recursive_float_assignment_bridge_under_extension();
    ok &= test_ir_accepts_explicit_int_from_float_compare_bridge_under_extension();
    ok &= test_ir_accepts_explicit_int_from_recursive_float_arithmetic_bridge_under_extension();
    ok &= test_ir_accepts_explicit_float_from_recursive_int_initializer_bridge_under_extension();
    ok &= test_ir_accepts_explicit_float_from_recursive_int_assignment_bridge_under_extension();
    ok &= test_ir_accepts_explicit_float_from_int_compare_bridge_under_extension();
    ok &= test_ir_accepts_explicit_float_from_recursive_int_arithmetic_bridge_under_extension();
    ok &= test_ir_accepts_explicit_float_condition_under_extension();
    ok &= test_ir_accepts_recursive_explicit_float_condition_under_extension();
    ok &= test_ir_accepts_explicit_float_while_condition_under_extension();
    ok &= test_ir_accepts_explicit_float_logical_condition_composition_under_extension();
    ok &= test_ir_accepts_same_type_float_ternary_value_under_extension();
    ok &= test_ir_accepts_float_ternary_value_assignment_to_float_under_extension();
    ok &= test_ir_accepts_multi_capture_direct_closure_literal_argument_to_function_value_parameter_under_extension();
    ok &= test_ir_accepts_closure_alias_chain_callable_object_transport_under_extension();
    ok &= test_ir_accepts_closure_direct_local_call_callable_object_transport_under_extension();
    ok &= test_ir_accepts_rich_body_closure_direct_local_call_callable_object_transport_under_extension();
    ok &= test_ir_accepts_float_ternary_value_initializer_to_float_under_extension();
    ok &= test_ir_accepts_chained_float_addition_assignment_to_float_under_extension();
    ok &= test_ir_accepts_nested_float_mul_div_assignment_to_float_under_extension();
    ok &= test_ir_accepts_chained_float_addition_initializer_to_float_under_extension();
    ok &= test_ir_accepts_nested_float_mul_div_initializer_to_float_under_extension();
    ok &= test_ir_accepts_float_ternary_value_call_argument_to_float_under_extension();
    ok &= test_ir_lowers_returned_function_value_parameter_immediate_call_under_extension();
    ok &= test_ir_accepts_returned_closure_forwarding_into_function_parameter_under_extension();
    ok &= test_ir_accepts_returned_closure_alias_forwarding_into_function_parameter_under_extension();
    ok &= test_ir_accepts_returned_closure_backed_function_value_parameter_immediate_call_under_extension();
    ok &= test_ir_accepts_returned_closure_backed_function_value_parameter_bind_and_call_under_extension();
    ok &= test_ir_accepts_dynamic_returned_closure_backed_function_value_parameter_immediate_call_under_extension();
    ok &= test_ir_accepts_dynamic_returned_two_arg_closure_backed_function_value_parameter_immediate_call_under_extension();
    ok &= test_ir_accepts_dynamic_returned_zero_arg_closure_backed_function_value_parameter_immediate_call_under_extension();
    ok &= test_ir_accepts_dynamic_returned_zero_arg_void_closure_backed_function_value_parameter_immediate_call_under_extension();
    ok &= test_ir_accepts_dynamic_returned_closure_backed_function_value_parameter_bind_and_call_under_extension();
    ok &= test_ir_accepts_returned_single_capture_closure_forwarding_into_function_parameter_under_extension();
    ok &= test_ir_accepts_returned_multi_capture_closure_forwarding_into_function_parameter_under_extension();
    ok &= test_ir_accepts_returned_single_capture_closure_alias_forwarding_into_function_parameter_under_extension();
    ok &= test_ir_accepts_returned_single_capture_zero_arg_closure_forwarding_into_function_parameter_under_extension();
    ok &= test_ir_accepts_returned_single_capture_zero_arg_void_closure_forwarding_into_function_parameter_under_extension();
    ok &= test_ir_accepts_dynamic_returned_local_closure_forwarding_into_function_parameter_under_extension();
    ok &= test_ir_accepts_direct_dynamic_returned_local_closure_function_value_argument_under_extension();
    ok &= test_ir_accepts_direct_dynamic_returned_local_zero_arg_closure_function_value_argument_under_extension();
    ok &= test_ir_accepts_direct_dynamic_returned_local_zero_arg_void_closure_function_value_argument_under_extension();
    ok &= test_ir_accepts_direct_closure_literal_argument_to_function_value_parameter_under_extension();
    ok &= test_ir_accepts_zero_arg_direct_closure_literal_argument_to_function_value_parameter_under_extension();
    ok &= test_ir_accepts_void_direct_closure_literal_argument_to_function_value_parameter_under_extension();
    ok &= test_ir_accepts_multi_capture_closure_forwarding_into_function_parameter_under_extension();
    ok &= test_ir_accepts_returned_multi_capture_local_closure_bind_and_call_under_extension();
    ok &= test_ir_accepts_dynamic_returned_multi_capture_local_closure_forwarding_into_function_parameter_under_extension();
    ok &= test_ir_accepts_returned_function_value_reassignment_under_extension();
    ok &= test_ir_accepts_returned_zero_arg_function_value_reassignment_under_extension();
    ok &= test_ir_accepts_returned_closure_reassignment_under_extension();
    ok &= test_ir_accepts_returned_zero_arg_void_closure_reassignment_under_extension();
    ok &= test_ir_accepts_ternary_zero_arg_closure_callee_under_extension();
    ok &= test_ir_accepts_ternary_zero_arg_void_closure_callee_under_extension();
    ok &= test_ir_accepts_unary_call_float_ternary_value_call_argument_to_float_under_extension();
    ok &= test_ir_accepts_chained_float_addition_call_argument_to_float_under_extension();
    ok &= test_ir_accepts_nested_float_mul_div_call_argument_to_float_under_extension();
    ok &= test_ir_accepts_float_helper_wrapped_ternary_call_arithmetic_under_extension();
    ok &= test_ir_accepts_unary_call_helper_wrapped_ternary_call_arithmetic_under_extension();
    ok &= test_ir_accepts_float_helper_wrapped_ternary_call_compare_under_extension();
    ok &= test_ir_accepts_unary_call_helper_wrapped_ternary_call_compare_under_extension();
    ok &= test_ir_accepts_negative_float_addition_combo_under_extension();
    ok &= test_ir_accepts_negative_float_subtraction_combo_under_extension();
    ok &= test_ir_accepts_float_multiplication_under_extension();
    ok &= test_ir_accepts_float_division_under_extension();
    ok &= test_ir_accepts_negative_float_multiplication_combo_under_extension();
    ok &= test_ir_accepts_negative_float_division_combo_under_extension();
    ok &= test_ir_accepts_chained_float_addition_under_extension();
    ok &= test_ir_accepts_nested_float_mul_div_under_extension();
    ok &= test_ir_rejects_mixed_float_int_arithmetic_under_extension();
    ok &= test_ir_rejects_float_literal_int_arithmetic_under_extension();
    ok &= test_ir_rejects_float_call_int_arithmetic_under_extension();
    ok &= test_ir_rejects_negative_float_call_int_arithmetic_under_extension();
    ok &= test_ir_rejects_global_float_int_condition_under_extension();
    ok &= test_ir_rejects_float_call_int_condition_under_extension();
    ok &= test_ir_rejects_negative_float_call_int_condition_under_extension();
    ok &= test_ir_rejects_nested_float_tree_plus_int_condition_under_extension();
    ok &= test_ir_rejects_nested_float_muldiv_plus_int_condition_under_extension();
    ok &= test_ir_rejects_float_compare_against_int_under_extension();
    ok &= test_ir_rejects_nested_float_tree_plus_int_under_extension();
    ok &= test_ir_rejects_nested_float_muldiv_plus_int_under_extension();
    ok &= test_ir_rejects_float_ternary_value_plus_int_under_extension();
    ok &= test_ir_rejects_unary_call_ternary_value_plus_int_under_extension();
    ok &= test_ir_rejects_float_ternary_value_plus_float_call_argument_under_extension();
    ok &= test_ir_rejects_unary_call_ternary_value_plus_float_call_argument_under_extension();
    ok &= test_ir_accepts_dynamic_noncapturing_function_value_return_bind_and_call_under_extension();
    ok &= test_ir_accepts_static_noncapturing_top_level_function_value_return_under_extension();
    ok &= test_ir_accepts_dynamic_noncapturing_function_value_return_immediate_call_under_extension();
    ok &= test_ir_accepts_dynamic_noncapturing_two_arg_function_value_return_immediate_call_under_extension();
    ok &= test_ir_accepts_dynamic_local_function_value_forwarding_into_function_parameter_under_extension();
    ok &= test_ir_accepts_second_order_dynamic_returned_function_value_parameter_immediate_call_under_extension();
    ok &= test_ir_accepts_second_order_returned_passthrough_dynamic_noncapturing_function_value_parameter_immediate_call_under_extension();
    ok &= test_ir_accepts_second_order_returned_passthrough_dynamic_function_value_parameter_immediate_call_under_extension();
    ok &= test_ir_accepts_second_order_local_function_value_parameter_forwarding_under_extension();
    ok &= test_ir_accepts_second_order_local_reassigned_function_value_parameter_forwarding_under_extension();
    ok &= test_ir_accepts_second_order_local_scalar_and_reassigned_function_value_parameter_forwarding_under_extension();
    ok &= test_ir_accepts_second_order_local_wrapper_scalar_update_function_value_parameter_forwarding_under_extension();
    ok &= test_ir_accepts_second_order_local_wrapper_repeated_scalar_update_function_value_parameter_forwarding_under_extension();
    ok &= test_ir_accepts_second_order_zero_arg_wrapper_scalar_update_function_value_parameter_forwarding_under_extension();
    ok &= test_ir_accepts_second_order_zero_arg_void_wrapper_scalar_update_function_value_parameter_forwarding_under_extension();
    ok &= test_ir_accepts_second_order_returned_zero_arg_wrapper_scalar_update_function_value_parameter_immediate_call_under_extension();
    ok &= test_ir_accepts_second_order_returned_zero_arg_wrapper_scalar_update_function_value_parameter_bind_call_under_extension();
    ok &= test_ir_accepts_second_order_returned_zero_arg_void_wrapper_scalar_update_function_value_parameter_immediate_call_under_extension();
    ok &= test_ir_accepts_second_order_returned_zero_arg_void_wrapper_scalar_update_function_value_parameter_bind_call_under_extension();
    ok &= test_ir_accepts_second_order_dynamic_ternary_function_value_actual_argument_under_extension();
    ok &= test_ir_accepts_multiple_static_function_valued_parameters_without_specialization_shell_under_extension();
    ok &= test_ir_accepts_second_order_closure_function_value_parameter_forwarding_under_extension();
    ok &= test_ir_accepts_third_order_function_value_parameter_forwarding_under_extension();
    ok &= test_ir_accepts_third_order_zero_arg_wrapper_scalar_update_function_value_parameter_forwarding_under_extension();
    ok &= test_ir_accepts_third_order_zero_arg_void_wrapper_scalar_update_function_value_parameter_forwarding_under_extension();
    ok &= test_ir_accepts_third_order_wrapper_scalar_update_function_value_parameter_forwarding_under_extension();
    ok &= test_ir_accepts_third_order_wrapper_repeated_scalar_update_function_value_parameter_forwarding_under_extension();
    ok &= test_ir_accepts_second_order_zero_arg_closure_function_value_parameter_forwarding_under_extension();
    ok &= test_ir_accepts_second_order_zero_arg_void_closure_function_value_parameter_forwarding_under_extension();
    ok &= test_ir_accepts_second_order_returned_closure_function_value_parameter_immediate_call_under_extension();
    ok &= test_ir_accepts_second_order_dynamic_local_closure_function_value_parameter_forwarding_under_extension();
    ok &= test_ir_accepts_second_order_returned_closure_function_value_parameter_forwarding_under_extension();
    ok &= test_ir_accepts_static_function_value_capture_inside_closure_under_extension();
    ok &= test_ir_accepts_function_parameter_capture_inside_closure_under_extension();
    ok &= test_ir_accepts_returned_function_parameter_capture_inside_closure_under_extension();
    ok &= test_ir_accepts_passthrough_decl_local_function_value_forwarding_without_specialization_shell_under_extension();
    ok &= test_ir_accepts_parameter_local_function_value_direct_call_without_specialization_shell_under_extension();
    ok &= test_ir_accepts_parameter_local_scalar_rebind_function_value_direct_call_without_specialization_shell_under_extension();
    ok &= test_ir_accepts_parameter_local_scalar_and_alias_function_value_direct_call_without_specialization_shell_under_extension();
    ok &= test_ir_accepts_parameter_local_scalar_update_function_value_direct_call_without_specialization_shell_under_extension();
    ok &= test_ir_accepts_parameter_local_scalar_update_and_alias_function_value_direct_call_without_specialization_shell_under_extension();
    ok &= test_ir_accepts_parameter_local_call_update_function_value_direct_call_without_specialization_shell_under_extension();
    ok &= test_ir_accepts_parameter_local_repeated_call_update_function_value_direct_call_without_specialization_shell_under_extension();
    ok &= test_ir_accepts_parameter_local_two_callable_update_without_specialization_shell_under_extension();
    ok &= test_ir_accepts_parameter_local_zero_arg_function_value_direct_call_without_specialization_shell_under_extension();
    ok &= test_ir_accepts_parameter_local_closure_direct_call_without_specialization_shell_under_extension();
    ok &= test_ir_accepts_parameter_local_closure_forward_without_specialization_shell_under_extension();
    ok &= test_ir_accepts_dynamic_local_zero_arg_function_value_forwarding_into_function_parameter_under_extension();
    ok &= test_ir_accepts_dynamic_local_zero_arg_void_function_value_forwarding_into_function_parameter_under_extension();
    ok &= test_ir_accepts_dynamic_runtime_closure_local_forward_into_function_parameter_under_extension();
    ok &= test_ir_accepts_dynamic_runtime_zero_arg_void_closure_local_forward_into_function_parameter_under_extension();
    ok &= test_ir_accepts_dynamic_local_zero_arg_closure_direct_callee_under_extension();
    ok &= test_ir_accepts_dynamic_local_zero_arg_void_closure_direct_callee_under_extension();
    ok &= test_ir_accepts_dynamic_runtime_local_zero_arg_closure_direct_callee_under_extension();
    ok &= test_ir_accepts_dynamic_runtime_local_zero_arg_void_closure_direct_callee_under_extension();
    ok &= test_ir_accepts_dynamic_returned_zero_arg_function_value_forwarding_into_function_parameter_under_extension();
    ok &= test_ir_accepts_dynamic_function_value_assignment_then_direct_call_under_extension();
    ok &= test_ir_accepts_ternary_function_value_assignment_then_direct_call_under_extension();
    ok &= test_ir_accepts_ternary_closure_assignment_then_direct_call_under_extension();
    ok &= test_ir_accepts_returned_call_ternary_function_value_assignment_then_direct_call_under_extension();
    ok &= test_ir_accepts_returned_call_ternary_closure_assignment_then_direct_call_under_extension();
    ok &= test_ir_accepts_ternary_function_value_local_initializer_under_extension();
    ok &= test_ir_accepts_dynamic_function_value_assignment_then_return_under_extension();
    ok &= test_ir_accepts_returned_closure_assignment_then_return_under_extension();
    ok &= test_ir_accepts_dynamic_returned_closure_rebind_then_return_under_extension();
    ok &= test_ir_accepts_dynamic_returned_multi_capture_closure_rebind_then_return_under_extension();
    ok &= test_ir_accepts_dynamic_returned_zero_arg_void_closure_rebind_then_return_under_extension();
    ok &= test_ir_accepts_dynamic_returned_closure_passthrough_bind_then_return_under_extension();
    ok &= test_ir_accepts_dynamic_returned_closure_two_hop_passthrough_bind_then_return_under_extension();
    ok &= test_ir_accepts_dynamic_returned_closure_two_hop_passthrough_immediate_call_under_extension();
    ok &= test_ir_accepts_dynamic_returned_closure_three_hop_passthrough_bind_then_return_under_extension();
    ok &= test_ir_accepts_dynamic_returned_closure_three_hop_passthrough_immediate_call_under_extension();
    ok &= test_ir_accepts_dynamic_returned_closure_three_hop_passthrough_actual_argument_under_extension();
    ok &= test_ir_accepts_dynamic_returned_closure_local_bounce_immediate_call_under_extension();
    ok &= test_ir_accepts_dynamic_returned_closure_local_bounce_actual_argument_under_extension();
    ok &= test_ir_accepts_dynamic_returned_closure_producer_ternary_merge_bind_and_call_under_extension();
    ok &= test_ir_accepts_dynamic_returned_closure_producer_ternary_merge_immediate_call_under_extension();
    ok &= test_ir_accepts_dynamic_returned_closure_returned_call_ternary_merge_immediate_call_under_extension();
    ok &= test_ir_accepts_dynamic_returned_closure_returned_call_ternary_merge_local_bounce_immediate_call_under_extension();
    ok &= test_ir_accepts_dynamic_returned_closure_returned_call_ternary_merge_bind_return_immediate_call_under_extension();
    ok &= test_ir_accepts_dynamic_returned_closure_returned_call_ternary_merge_bind_return_actual_argument_under_extension();
    ok &= test_ir_accepts_dynamic_returned_closure_returned_call_ternary_merge_bounce_passthrough_immediate_call_under_extension();
    ok &= test_ir_accepts_dynamic_returned_closure_returned_call_ternary_merge_bounce_passthrough_bind_return_immediate_call_under_extension();
    ok &= test_ir_accepts_dynamic_returned_closure_returned_call_ternary_merge_bounce_passthrough_bind_return_actual_argument_under_extension();
    ok &= test_ir_accepts_dynamic_returned_closure_returned_call_ternary_merge_deeper_chain_bind_return_immediate_call_under_extension();
    ok &= test_ir_accepts_dynamic_returned_closure_producer_ternary_merge_actual_argument_under_extension();
    ok &= test_ir_accepts_dynamic_returned_closure_producer_ternary_merge_local_bounce_immediate_call_under_extension();
    ok &= test_ir_accepts_dynamic_returned_closure_producer_ternary_merge_local_bounce_actual_argument_under_extension();
    ok &= test_ir_accepts_dynamic_returned_closure_producer_ternary_merge_local_bounce_passthrough_immediate_call_under_extension();
    ok &= test_ir_accepts_dynamic_returned_closure_producer_ternary_merge_local_bounce_passthrough_actual_argument_under_extension();
    ok &= test_ir_accepts_dynamic_returned_closure_producer_ternary_merge_local_reassign_immediate_call_under_extension();
    ok &= test_ir_accepts_dynamic_returned_closure_producer_ternary_merge_local_reassign_actual_argument_under_extension();
    ok &= test_ir_accepts_dynamic_returned_closure_producer_ternary_merge_returned_call_reassign_immediate_call_under_extension();
    ok &= test_ir_accepts_dynamic_returned_closure_producer_ternary_merge_returned_call_reassign_actual_argument_under_extension();
    ok &= test_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_immediate_call_under_extension();
    ok &= test_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_actual_argument_under_extension();
    ok &= test_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_immediate_call_under_extension();
    ok &= test_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_actual_argument_under_extension();
    ok &= test_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_local_bounce_immediate_call_under_extension();
    ok &= test_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_local_bounce_actual_argument_under_extension();
    ok &= test_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_local_bounce_passthrough_immediate_call_under_extension();
    ok &= test_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_local_bounce_passthrough_actual_argument_under_extension();
    ok &= test_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_local_reassign_immediate_call_under_extension();
    ok &= test_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_local_reassign_actual_argument_under_extension();
    ok &= test_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_local_bounce_immediate_call_under_extension();
    ok &= test_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_local_bounce_actual_argument_under_extension();
    ok &= test_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_local_bounce_passthrough_immediate_call_under_extension();
    ok &= test_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_local_bounce_passthrough_actual_argument_under_extension();
    ok &= test_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_local_reassign_immediate_call_under_extension();
    ok &= test_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_local_reassign_actual_argument_under_extension();
    ok &= test_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_returned_call_reassign_immediate_call_under_extension();
    ok &= test_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_returned_call_reassign_actual_argument_under_extension();
    ok &= test_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_returned_call_reassign_immediate_call_under_extension();
    ok &= test_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_returned_call_reassign_actual_argument_under_extension();
    ok &= test_ir_accepts_wrapped_function_value_reassignment_under_extension();
    ok &= test_ir_accepts_assignment_result_function_value_reassignment_under_extension();
    ok &= test_ir_accepts_wrapped_returned_function_value_reassignment_under_extension();
    ok &= test_ir_accepts_wrapped_returned_function_value_local_initializer_under_extension();
    ok &= test_ir_accepts_wrapped_returned_zero_arg_function_value_local_initializer_under_extension();
    ok &= test_ir_accepts_assignment_result_returned_closure_local_initializer_under_extension();
    ok &= test_ir_accepts_wrapped_returned_call_ternary_function_value_local_initializer_under_extension();
    ok &= test_ir_accepts_wrapped_returned_call_ternary_closure_local_initializer_under_extension();
    ok &= test_ir_accepts_wrapped_returned_call_ternary_function_value_local_bounce_under_extension();
    ok &= test_ir_accepts_wrapped_returned_call_ternary_function_value_forward_under_extension();
    ok &= test_ir_accepts_wrapped_returned_call_ternary_closure_local_bounce_under_extension();
    ok &= test_ir_accepts_wrapped_returned_call_ternary_closure_forward_under_extension();
    ok &= test_ir_accepts_wrapped_returned_call_ternary_function_value_bind_return_under_extension();
    ok &= test_ir_accepts_wrapped_returned_call_ternary_function_value_bind_return_actual_under_extension();
    ok &= test_ir_accepts_wrapped_returned_call_ternary_closure_bind_return_under_extension();
    ok &= test_ir_accepts_wrapped_returned_call_ternary_closure_bind_return_actual_under_extension();
    ok &= test_ir_accepts_wrapped_returned_call_ternary_zero_arg_function_value_bind_return_under_extension();
    ok &= test_ir_accepts_wrapped_returned_call_ternary_zero_arg_function_value_bind_return_actual_under_extension();
    ok &= test_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_function_value_bind_return_under_extension();
    ok &= test_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_function_value_bind_return_actual_under_extension();
    ok &= test_ir_accepts_wrapped_returned_call_ternary_zero_arg_closure_bind_return_under_extension();
    ok &= test_ir_accepts_wrapped_returned_call_ternary_zero_arg_closure_bind_return_actual_under_extension();
    ok &= test_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_closure_bind_return_under_extension();
    ok &= test_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_closure_bind_return_actual_under_extension();
    ok &= test_ir_accepts_wrapped_returned_call_ternary_function_value_bounce_passthrough_bind_return_under_extension();
    ok &= test_ir_accepts_wrapped_returned_call_ternary_function_value_bounce_passthrough_bind_return_actual_under_extension();
    ok &= test_ir_accepts_wrapped_returned_call_ternary_closure_bounce_passthrough_bind_return_under_extension();
    ok &= test_ir_accepts_wrapped_returned_call_ternary_closure_bounce_passthrough_bind_return_actual_under_extension();
    ok &= test_ir_accepts_wrapped_returned_call_ternary_zero_arg_function_value_bounce_passthrough_bind_return_under_extension();
    ok &= test_ir_accepts_wrapped_returned_call_ternary_zero_arg_function_value_bounce_passthrough_bind_return_actual_under_extension();
    ok &= test_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_function_value_bounce_passthrough_bind_return_under_extension();
    ok &= test_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_function_value_bounce_passthrough_bind_return_actual_under_extension();
    ok &= test_ir_accepts_wrapped_returned_call_ternary_zero_arg_closure_bounce_passthrough_bind_return_under_extension();
    ok &= test_ir_accepts_wrapped_returned_call_ternary_zero_arg_closure_bounce_passthrough_bind_return_actual_under_extension();
    ok &= test_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_closure_bounce_passthrough_bind_return_under_extension();
    ok &= test_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_closure_bounce_passthrough_bind_return_actual_under_extension();
    ok &= test_ir_accepts_wrapped_returned_call_ternary_function_value_statement_reassign_bind_return_under_extension();
    ok &= test_ir_accepts_wrapped_returned_call_ternary_function_value_statement_reassign_bind_return_actual_under_extension();
    ok &= test_ir_accepts_wrapped_returned_call_ternary_closure_statement_reassign_bind_return_under_extension();
    ok &= test_ir_accepts_wrapped_returned_call_ternary_closure_statement_reassign_bind_return_actual_under_extension();
    ok &= test_ir_accepts_wrapped_returned_call_ternary_zero_arg_function_value_statement_reassign_bind_return_under_extension();
    ok &= test_ir_accepts_wrapped_returned_call_ternary_zero_arg_function_value_statement_reassign_bind_return_actual_under_extension();
    ok &= test_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_function_value_statement_reassign_bind_return_under_extension();
    ok &= test_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_function_value_statement_reassign_bind_return_actual_under_extension();
    ok &= test_ir_accepts_wrapped_returned_call_ternary_zero_arg_closure_statement_reassign_bind_return_under_extension();
    ok &= test_ir_accepts_wrapped_returned_call_ternary_zero_arg_closure_statement_reassign_bind_return_actual_under_extension();
    ok &= test_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_closure_statement_reassign_bind_return_under_extension();
    ok &= test_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_closure_statement_reassign_bind_return_actual_under_extension();
    ok &= test_ir_accepts_assignment_result_returned_closure_reassignment_under_extension();
    ok &= test_ir_accepts_dynamic_returned_zero_arg_closure_statement_passthrough_call_under_extension();
    ok &= test_ir_accepts_dynamic_returned_zero_arg_void_closure_statement_passthrough_call_under_extension();
    ok &= test_ir_accepts_dynamic_returned_closure_statement_passthrough_call_under_extension();
    ok &= test_ir_accepts_passthrough_ternary_closure_local_initializer_under_extension();
    ok &= test_ir_accepts_passthrough_ternary_noncapturing_function_value_return_immediate_call_under_extension();
    ok &= test_ir_accepts_passthrough_ternary_noncapturing_function_value_actual_argument_under_extension();
    ok &= test_ir_accepts_passthrough_ternary_closure_function_value_actual_argument_under_extension();
    ok &= test_ir_accepts_passthrough_ternary_zero_arg_function_value_actual_argument_under_extension();
    ok &= test_ir_accepts_passthrough_ternary_zero_arg_void_function_value_actual_argument_under_extension();
    ok &= test_ir_accepts_passthrough_ternary_zero_arg_closure_function_value_actual_argument_under_extension();
    ok &= test_ir_accepts_passthrough_ternary_zero_arg_void_closure_function_value_actual_argument_under_extension();
    ok &= test_ir_accepts_wrapped_function_value_return_under_extension();
    ok &= test_ir_accepts_dynamic_wrapped_function_value_return_under_extension();
    ok &= test_ir_accepts_ternary_noncapturing_function_value_return_immediate_call_under_extension();
    ok &= test_ir_accepts_ternary_noncapturing_function_value_return_bind_and_call_under_extension();
    ok &= test_ir_accepts_ternary_closure_function_value_return_immediate_call_under_extension();
    ok &= test_ir_accepts_ternary_closure_function_value_return_bind_and_call_under_extension();
    ok &= test_ir_accepts_direct_dynamic_returned_multi_capture_closure_immediate_call_under_extension();
    ok &= test_ir_accepts_ternary_closure_local_initializer_under_extension();
    ok &= test_ir_accepts_ternary_function_value_actual_argument_under_extension();
    ok &= test_ir_accepts_ternary_closure_actual_argument_under_extension();
    ok &= test_ir_accepts_comma_wrapped_closure_forward_into_function_value_parameter_under_extension();
    ok &= test_ir_accepts_assignment_result_closure_forward_into_function_value_parameter_under_extension();
    ok &= test_ir_accepts_dynamic_comma_wrapped_closure_forward_into_function_value_parameter_under_extension();
    ok &= test_ir_accepts_dynamic_assignment_result_closure_forward_into_function_value_parameter_under_extension();
    ok &= test_ir_accepts_dynamic_comma_wrapped_zero_arg_closure_forward_into_function_value_parameter_under_extension();
    ok &= test_ir_accepts_dynamic_assignment_result_zero_arg_closure_forward_into_function_value_parameter_under_extension();
    ok &= test_ir_accepts_dynamic_comma_wrapped_zero_arg_void_closure_forward_into_function_value_parameter_under_extension();
    ok &= test_ir_accepts_dynamic_assignment_result_zero_arg_void_closure_forward_into_function_value_parameter_under_extension();
    ok &= test_ir_accepts_dynamic_comma_wrapped_function_value_forwarding_into_parameter_under_extension();
    ok &= test_ir_accepts_dynamic_assignment_result_function_value_forwarding_into_parameter_under_extension();
    ok &= test_ir_accepts_dynamic_comma_wrapped_zero_arg_function_value_forwarding_into_parameter_under_extension();
    ok &= test_ir_accepts_dynamic_assignment_result_zero_arg_function_value_forwarding_into_parameter_under_extension();
    ok &= test_ir_accepts_dynamic_comma_wrapped_zero_arg_void_function_value_forwarding_into_parameter_under_extension();
    ok &= test_ir_accepts_dynamic_assignment_result_zero_arg_void_function_value_forwarding_into_parameter_under_extension();
    ok &= test_ir_accepts_ternary_function_value_callee_under_extension();
    ok &= test_ir_accepts_ternary_closure_callee_under_extension();
    ok &= test_ir_accepts_ternary_multi_capture_closure_callee_under_extension();
    ok &= test_ir_rejects_float_ternary_value_under_extension();
    ok &= test_ir_lowers_pair_copy_under_extension();
    ok &= test_ir_lowers_struct_copy_under_extension();
    ok &= test_ir_lowers_constant_true_while_return_without_malformed_exit_block();
    ok &= test_ir_lowers_infinite_for_return_without_malformed_exit_block();
    ok &= test_ir_lowers_infinite_for_with_step_return_without_malformed_exit_or_step_block();
    ok &= test_ir_lowers_strict_local_state_while_return_family();
    ok &= test_ir_lowers_strict_local_state_for_return_family();
    ok &= test_ir_lowers_strict_assigned_local_state_loop_return_family();
    ok &= test_ir_preserves_loop_condition_after_unknown_if_state_update();
    ok &= test_ir_keeps_post_if_local_condition_when_branch_facts_disagree();
    ok &= test_ir_lowers_nested_loop_alias_return_family_without_malformed_exit_block();
    ok &= test_ir_lowers_while_break_exit();
    ok &= test_ir_lowers_for_init_step_cfg();
    ok &= test_ir_lowers_if_condition_side_effect_even_when_truthiness_known();
    ok &= test_ir_lowers_while_condition_side_effect_even_when_truthiness_known();
    ok &= test_ir_lowers_for_condition_side_effect_even_when_truthiness_known();
    ok &= test_ir_merges_no_else_branch_facts_with_false_path();
    ok &= test_ir_merges_no_else_condition_side_effect_fact_to_continue();
    ok &= test_ir_clears_ternary_branch_mutation_facts_after_join();
    ok &= test_ir_clears_logical_value_branch_mutation_facts_after_join();
    ok &= test_ir_keeps_pre_loop_local_fact_after_unknown_calling_while_body();
    ok &= test_ir_clears_mutated_local_fact_after_unknown_calling_for_loop();
    ok &= test_ir_lowers_nested_loop_break_continue();

    if (!ok) {
        return 1;
    }

    printf("[ir-reg] All IR regressions passed.\n");
    return 0;
}
