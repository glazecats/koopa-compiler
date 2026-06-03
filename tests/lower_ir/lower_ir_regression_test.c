#include "lower_ir.h"

#include "ast.h"
#include "compiler.h"
#include "lexer.h"
#include "parser.h"
#include "semantic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t lower_ir_test_count_fragment_occurrences(const char *text, const char *fragment) {
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

    if (!ir_lower_program(&ast_program, NULL, &ir_program, &ir_err)) {
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

    if (!lower_ir_lower_from_ir(&ir_program, NULL, &lower_program, &lower_err)) {
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

static int lower_extension_source_to_lower_ir_text(const char *source, char **out_text) {
    TokenArray tokens;
    AstProgram ast_program;
    IrProgram ir_program;
    LowerIrProgram lower_program;
    CompilerError error;
    CompilerOptions options;

    if (!source || !out_text) {
        return 0;
    }

    *out_text = NULL;
    memset(&error, 0, sizeof(error));
    memset(&options, 0, sizeof(options));
    lexer_init_tokens(&tokens);
    ast_program_init(&ast_program);
    ir_program_init(&ir_program);
    lower_ir_program_init(&lower_program);
    options.skip_all_paths_return_check = 1;

    if (!compiler_frontend_lower_to_lower_ir_for_testing(
            source,
            COMPILER_MODE_EXTENSION,
            &options,
            &tokens,
            &ast_program,
            &ir_program,
            &lower_program,
            &error)) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: lower IR lowering failed at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
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

static int lower_extension_source_to_lower_ir_text_without_semantic(const char *source, char **out_text) {
    TokenArray tokens;
    AstProgram ast_program;
    ParserError parse_err;
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
        fprintf(stderr, "[lower-ir-reg] FAIL: lexer failed for extension input without semantic\n");
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

    if (!ir_lower_program(&ast_program, NULL, &ir_program, &ir_err)) {
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

    if (!lower_ir_lower_from_ir(&ir_program, NULL, &lower_program, &lower_err)) {
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

static int expect_lower_ir_lower_succeeds(const char *case_id, const char *source) {
    char *actual_text = NULL;

    if (!case_id || !source) {
        return 0;
    }
    if (!lower_source_to_lower_ir_text(source, &actual_text)) {
        free(actual_text);
        return 0;
    }

    free(actual_text);
    return 1;
}

static int expect_extension_lower_ir_dump_without_semantic(const char *case_id,
    const char *source,
    const char *expected_text) {
    char *actual_text = NULL;
    int ok;

    if (!case_id || !source || !expected_text) {
        return 0;
    }

    if (!lower_extension_source_to_lower_ir_text_without_semantic(source, &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = strcmp(actual_text, expected_text) == 0;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: %s lower IR mismatch\nexpected:\n%s\nactual:\n%s\n",
            case_id,
            expected_text,
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int expect_extension_lower_ir_dump(const char *case_id,
    const char *source,
    const char *expected_text) {
    char *actual_text = NULL;
    int ok;

    if (!case_id || !source || !expected_text) {
        return 0;
    }

    if (!lower_extension_source_to_lower_ir_text(source, &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = strcmp(actual_text, expected_text) == 0;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: %s lower IR mismatch\nexpected:\n%s\nactual:\n%s\n",
            case_id,
            expected_text,
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
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

static int test_lower_ir_lowers_pair_return_via_hidden_result_slots_under_extension(void) {
    return expect_extension_lower_ir_dump_without_semantic("LOWER-IR-AGGRET-PAIR-LOCAL",
        "pair mk(){ pair p={1,2}; return p; }\n"
        "int main(){ return 0; }\n",
        "func mk(__ret0.0, __ret1.1) {\n"
        "  bb.0:\n"
        "    store_local p$first.2, 1\n"
        "    store_local p$second.3, 2\n"
        "    tmp.0 = load_local __ret0.0\n"
        "    tmp.1 = load_local p$first.2\n"
        "    store_indirect tmp.0, tmp.1\n"
        "    tmp.2 = load_local __ret1.1\n"
        "    tmp.3 = load_local p$second.3\n"
        "    store_indirect tmp.2, tmp.3\n"
        "    ret\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    ret 0\n"
        "}\n");
}

static int test_lower_ir_lowers_pair_call_init_via_hidden_result_slots_under_extension(void) {
    return expect_extension_lower_ir_dump_without_semantic("LOWER-IR-AGGRET-PAIR-CALL-INIT",
        "pair mk(){ pair p={1,2}; return p; }\n"
        "int main(){ pair q = mk(); return q.second; }\n",
        "func mk(__ret0.0, __ret1.1) {\n"
        "  bb.0:\n"
        "    store_local p$first.2, 1\n"
        "    store_local p$second.3, 2\n"
        "    tmp.0 = load_local __ret0.0\n"
        "    tmp.1 = load_local p$first.2\n"
        "    store_indirect tmp.0, tmp.1\n"
        "    tmp.2 = load_local __ret1.1\n"
        "    tmp.3 = load_local p$second.3\n"
        "    store_indirect tmp.2, tmp.3\n"
        "    ret\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    tmp.0 = addr_local q$first.0\n"
        "    tmp.1 = addr_local q$second.1\n"
        "    call mk(tmp.0, tmp.1)\n"
        "    tmp.4 = load_local q$second.1\n"
        "    tmp.3 = mov tmp.4\n"
        "    ret tmp.3\n"
        "}\n");
}

static int test_lower_ir_lowers_pair_call_result_as_aggregate_call_argument_under_extension(void) {
    return expect_extension_lower_ir_dump("LOWER-IR-AGGCALL-PAIR-CALLARG",
        "pair mk(){ pair p={1,2}; return p; }\n"
        "int sum(pair p){ return p.first + p.second; }\n"
        "int main(){ return sum(mk()); }\n",
        "func mk(__ret0.0, __ret1.1) {\n"
        "  bb.0:\n"
        "    store_local p$first.2, 1\n"
        "    store_local p$second.3, 2\n"
        "    tmp.0 = load_local __ret0.0\n"
        "    tmp.1 = load_local p$first.2\n"
        "    store_indirect tmp.0, tmp.1\n"
        "    tmp.2 = load_local __ret1.1\n"
        "    tmp.3 = load_local p$second.3\n"
        "    store_indirect tmp.2, tmp.3\n"
        "    ret\n"
        "}\n"
        "\n"
        "func sum(p.0, p.1) {\n"
        "  bb.0:\n"
        "    tmp.3 = load_local p.0\n"
        "    tmp.0 = mov tmp.3\n"
        "    tmp.4 = load_local p.1\n"
        "    tmp.1 = mov tmp.4\n"
        "    tmp.2 = add tmp.0, tmp.1\n"
        "    ret tmp.2\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    tmp.0 = addr_local __aggarg_tmp_0.0\n"
        "    tmp.1 = addr_local __aggarg_tmp_1.1\n"
        "    call mk(tmp.0, tmp.1)\n"
        "    tmp.4 = load_local __aggarg_tmp_0.0\n"
        "    tmp.5 = load_local __aggarg_tmp_1.1\n"
        "    tmp.3 = call sum(tmp.4, tmp.5)\n"
        "    ret tmp.3\n"
        "}\n");
}

static int test_lower_ir_lowers_pair_multi_hop_return_chain_under_extension(void) {
    return expect_extension_lower_ir_dump("LOWER-IR-AGGRET-PAIR-MULTIHOP",
        "pair mk(){ pair p={1,2}; return p; }\n"
        "pair id(pair p){ return p; }\n"
        "pair wrap(){ return id(mk()); }\n"
        "int main(){ pair q=wrap(); return q.second; }\n",
        "func mk(__ret0.0, __ret1.1) {\n"
        "  bb.0:\n"
        "    store_local p$first.2, 1\n"
        "    store_local p$second.3, 2\n"
        "    tmp.0 = load_local __ret0.0\n"
        "    tmp.1 = load_local p$first.2\n"
        "    store_indirect tmp.0, tmp.1\n"
        "    tmp.2 = load_local __ret1.1\n"
        "    tmp.3 = load_local p$second.3\n"
        "    store_indirect tmp.2, tmp.3\n"
        "    ret\n"
        "}\n"
        "\n"
        "func id(__ret0.0, __ret1.1, p.2, p.3) {\n"
        "  bb.0:\n"
        "    tmp.0 = load_local __ret0.0\n"
        "    tmp.1 = load_local p.2\n"
        "    store_indirect tmp.0, tmp.1\n"
        "    tmp.2 = load_local __ret1.1\n"
        "    tmp.3 = load_local p.3\n"
        "    store_indirect tmp.2, tmp.3\n"
        "    ret\n"
        "}\n"
        "\n"
        "func wrap(__ret0.0, __ret1.1) {\n"
        "  bb.0:\n"
        "    tmp.0 = addr_local __aggarg_tmp_2.2\n"
        "    tmp.1 = addr_local __aggarg_tmp_3.3\n"
        "    call mk(tmp.0, tmp.1)\n"
        "    tmp.4 = load_local __ret0.0\n"
        "    tmp.5 = load_local __ret1.1\n"
        "    tmp.6 = load_local __aggarg_tmp_2.2\n"
        "    tmp.7 = load_local __aggarg_tmp_3.3\n"
        "    call id(tmp.4, tmp.5, tmp.6, tmp.7)\n"
        "    ret\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    tmp.0 = addr_local q$first.0\n"
        "    tmp.1 = addr_local q$second.1\n"
        "    call wrap(tmp.0, tmp.1)\n"
        "    tmp.4 = load_local q$second.1\n"
        "    tmp.3 = mov tmp.4\n"
        "    ret tmp.3\n"
        "}\n");
}

static int test_lower_ir_lowers_nested_struct_multi_hop_return_chain_under_extension(void) {
    return expect_extension_lower_ir_dump("LOWER-IR-AGGRET-NESTED-STRUCT-MULTIHOP",
        "struct Mid { pair p; int z; };\n"
        "struct Mid mk(){ struct Mid m={{1,2},3}; return m; }\n"
        "struct Mid id(struct Mid m){ return m; }\n"
        "struct Mid wrap(){ return id(mk()); }\n"
        "int main(){ struct Mid q=wrap(); return q.p.second + q.z; }\n",
        "func mk(__ret0.0, __ret1.1, __ret2.2) {\n"
        "  bb.0:\n"
        "    store_local m$p$0.3, 1\n"
        "    store_local m$p$1.4, 2\n"
        "    store_local m$z.5, 3\n"
        "    tmp.0 = load_local __ret0.0\n"
        "    tmp.1 = load_local m$p$0.3\n"
        "    store_indirect tmp.0, tmp.1\n"
        "    tmp.2 = load_local __ret1.1\n"
        "    tmp.3 = load_local m$p$1.4\n"
        "    store_indirect tmp.2, tmp.3\n"
        "    tmp.4 = load_local __ret2.2\n"
        "    tmp.5 = load_local m$z.5\n"
        "    store_indirect tmp.4, tmp.5\n"
        "    ret\n"
        "}\n"
        "\n"
        "func id(__ret0.0, __ret1.1, __ret2.2, m.3, m.4, m.5) {\n"
        "  bb.0:\n"
        "    tmp.0 = load_local __ret0.0\n"
        "    tmp.1 = load_local m.3\n"
        "    store_indirect tmp.0, tmp.1\n"
        "    tmp.2 = load_local __ret1.1\n"
        "    tmp.3 = load_local m.4\n"
        "    store_indirect tmp.2, tmp.3\n"
        "    tmp.4 = load_local __ret2.2\n"
        "    tmp.5 = load_local m.5\n"
        "    store_indirect tmp.4, tmp.5\n"
        "    ret\n"
        "}\n"
        "\n"
        "func wrap(__ret0.0, __ret1.1, __ret2.2) {\n"
        "  bb.0:\n"
        "    tmp.0 = addr_local __aggarg_tmp_3.3\n"
        "    tmp.1 = addr_local __aggarg_tmp_4.4\n"
        "    tmp.2 = addr_local __aggarg_tmp_5.5\n"
        "    call mk(tmp.0, tmp.1, tmp.2)\n"
        "    tmp.5 = load_local __ret0.0\n"
        "    tmp.6 = load_local __ret1.1\n"
        "    tmp.7 = load_local __ret2.2\n"
        "    tmp.8 = load_local __aggarg_tmp_3.3\n"
        "    tmp.9 = load_local __aggarg_tmp_4.4\n"
        "    tmp.10 = load_local __aggarg_tmp_5.5\n"
        "    call id(tmp.5, tmp.6, tmp.7, tmp.8, tmp.9, tmp.10)\n"
        "    ret\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    tmp.0 = addr_local q$p$0.0\n"
        "    tmp.1 = addr_local q$p$1.1\n"
        "    tmp.2 = addr_local q$z.2\n"
        "    call wrap(tmp.0, tmp.1, tmp.2)\n"
        "    tmp.7 = load_local q$p$1.1\n"
        "    tmp.4 = mov tmp.7\n"
        "    tmp.8 = load_local q$z.2\n"
        "    tmp.5 = mov tmp.8\n"
        "    tmp.6 = add tmp.4, tmp.5\n"
        "    ret tmp.6\n"
        "}\n");
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

static int build_callable_object_bridge_ir_program(IrProgram *program) {
    IrFunctionShape *new_shapes;
    AstFunctionReturnType *param_types = NULL;
    IrFunction *callee = NULL;
    IrFunction *main_function = NULL;
    IrBasicBlock *callee_block = NULL;
    IrBasicBlock *main_block = NULL;
    IrInstruction *instructions = NULL;
    char *callee_name = NULL;
    char *main_name = NULL;
    char *env_name = NULL;
    char *x_name = NULL;
    char *fn_make_name = NULL;

    if (!program) {
        return 0;
    }

    ir_program_init(program);

    new_shapes = (IrFunctionShape *)realloc(program->function_shapes, sizeof(IrFunctionShape));
    if (!new_shapes) {
        ir_program_free(program);
        return 0;
    }
    program->function_shapes = new_shapes;
    program->function_shape_capacity = 1u;
    param_types = (AstFunctionReturnType *)malloc(sizeof(AstFunctionReturnType));
    if (!param_types) {
        ir_program_free(program);
        return 0;
    }
    param_types[0] = AST_FUNCTION_RETURN_INT;
    program->function_shapes[0].id = 0u;
    program->function_shapes[0].return_type = AST_FUNCTION_RETURN_INT;
    program->function_shapes[0].parameter_types = param_types;
    program->function_shapes[0].parameter_count = 1u;
    program->function_shape_count = 1u;

    program->functions = (IrFunction *)calloc(2u, sizeof(IrFunction));
    if (!program->functions) {
        ir_program_free(program);
        return 0;
    }
    program->function_capacity = 2u;
    program->function_count = 2u;

    callee = &program->functions[0];
    main_function = &program->functions[1];

    callee_name = (char *)malloc(sizeof("add1_env"));
    main_name = (char *)malloc(sizeof("main"));
    env_name = (char *)malloc(sizeof("env"));
    x_name = (char *)malloc(sizeof("x"));
    fn_make_name = (char *)malloc(sizeof("add1_env"));
    if (!callee_name || !main_name || !env_name || !x_name || !fn_make_name) {
        ir_program_free(program);
        free(callee_name);
        free(main_name);
        free(env_name);
        free(x_name);
        free(fn_make_name);
        return 0;
    }
    memcpy(callee_name, "add1_env", sizeof("add1_env"));
    memcpy(main_name, "main", sizeof("main"));
    memcpy(env_name, "env", sizeof("env"));
    memcpy(x_name, "x", sizeof("x"));
    memcpy(fn_make_name, "add1_env", sizeof("add1_env"));
    callee->name = callee_name;
    main_function->name = main_name;

    callee->has_body = 1;
    callee->return_type = AST_FUNCTION_RETURN_INT;
    callee->parameter_count = 2u;
    callee->local_count = 2u;
    callee->local_capacity = 2u;
    callee->next_temp_id = 1u;
    callee->locals = (IrLocal *)calloc(2u, sizeof(IrLocal));
    callee->blocks = (IrBasicBlock *)calloc(1u, sizeof(IrBasicBlock));
    if (!callee->locals || !callee->blocks) {
        ir_program_free(program);
        return 0;
    }
    callee->block_count = 1u;
    callee->block_capacity = 1u;
    callee->locals[0].id = 0u;
    callee->locals[0].source_name = env_name;
    callee->locals[0].is_parameter = 1;
    callee->locals[0].value_type = AST_FUNCTION_RETURN_INT;
    callee->locals[1].id = 1u;
    callee->locals[1].source_name = x_name;
    callee->locals[1].is_parameter = 1;
    callee->locals[1].value_type = AST_FUNCTION_RETURN_INT;
    if (!callee->locals[0].source_name || !callee->locals[1].source_name) {
        ir_program_free(program);
        return 0;
    }
    callee_block = &callee->blocks[0];
    callee_block->id = 0u;
    callee_block->instruction_count = 1u;
    callee_block->instruction_capacity = 1u;
    callee_block->instructions = (IrInstruction *)calloc(1u, sizeof(IrInstruction));
    if (!callee_block->instructions) {
        ir_program_free(program);
        return 0;
    }
    callee_block->instructions[0].kind = IR_INSTR_BINARY;
    callee_block->instructions[0].result.kind = IR_VALUE_TEMP;
    callee_block->instructions[0].result.id = 0u;
    callee_block->instructions[0].result.immediate = 0;
    callee_block->instructions[0].as.binary.op = IR_BINARY_ADD;
    callee_block->instructions[0].as.binary.lhs.kind = IR_VALUE_LOCAL;
    callee_block->instructions[0].as.binary.lhs.id = 1u;
    callee_block->instructions[0].as.binary.rhs.kind = IR_VALUE_IMMEDIATE;
    callee_block->instructions[0].as.binary.rhs.immediate = 1;
    callee_block->has_terminator = 1;
    callee_block->terminator.kind = IR_TERM_RETURN;
    callee_block->terminator.has_return_value = 1;
    callee_block->terminator.as.return_value.kind = IR_VALUE_TEMP;
    callee_block->terminator.as.return_value.id = 0u;

    main_function->has_body = 1;
    main_function->return_type = AST_FUNCTION_RETURN_INT;
    main_function->parameter_count = 0u;
    main_function->local_count = 0u;
    main_function->local_capacity = 0u;
    main_function->next_temp_id = 2u;
    main_function->blocks = (IrBasicBlock *)calloc(1u, sizeof(IrBasicBlock));
    if (!main_function->blocks) {
        ir_program_free(program);
        return 0;
    }
    main_function->block_count = 1u;
    main_function->block_capacity = 1u;
    main_block = &main_function->blocks[0];
    main_block->id = 0u;
    main_block->instruction_count = 2u;
    main_block->instruction_capacity = 2u;
    instructions = (IrInstruction *)calloc(2u, sizeof(IrInstruction));
    if (!instructions) {
        ir_program_free(program);
        return 0;
    }
    main_block->instructions = instructions;

    instructions[0].kind = IR_INSTR_FN_MAKE;
    instructions[0].result.kind = IR_VALUE_TEMP;
    instructions[0].result.id = 0u;
    instructions[0].as.fn_make.callee_name = fn_make_name;
    instructions[0].as.fn_make.env.kind = IR_VALUE_IMMEDIATE;
    instructions[0].as.fn_make.env.immediate = 0;
    instructions[0].as.fn_make.shape_id = 0u;
    instructions[1].kind = IR_INSTR_CALL_INDIRECT;
    instructions[1].result.kind = IR_VALUE_TEMP;
    instructions[1].result.id = 1u;
    instructions[1].as.call_indirect.callee.kind = IR_VALUE_TEMP;
    instructions[1].as.call_indirect.callee.id = 0u;
    instructions[1].as.call_indirect.arg_count = 1u;
    instructions[1].as.call_indirect.args = (IrValueRef *)calloc(1u, sizeof(IrValueRef));
    if (!instructions[1].as.call_indirect.args) {
        ir_program_free(program);
        return 0;
    }
    instructions[1].as.call_indirect.args[0].kind = IR_VALUE_IMMEDIATE;
    instructions[1].as.call_indirect.args[0].immediate = 41;

    main_block->has_terminator = 1;
    main_block->terminator.kind = IR_TERM_RETURN;
    main_block->terminator.has_return_value = 1;
    main_block->terminator.as.return_value.kind = IR_VALUE_TEMP;
    main_block->terminator.as.return_value.id = 1u;
    main_block->terminator.as.return_value.immediate = 0;
    return 1;
}

static int test_lower_ir_bridges_callable_object_indirect_call(void) {
    IrProgram ir_program;
    LowerIrProgram lower_program;
    LowerIrError lower_err;
    char *actual_text = NULL;
    int ok = 0;

    if (!build_callable_object_bridge_ir_program(&ir_program)) {
        fprintf(stderr, "[lower-ir-reg] FAIL: callable-object bridge IR setup failed\n");
        return 0;
    }
    lower_ir_program_init(&lower_program);

    if (!lower_ir_lower_from_ir(&ir_program, NULL, &lower_program, &lower_err)) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: callable-object bridge lowering failed at %d:%d: %s\n",
            lower_err.line,
            lower_err.column,
            lower_err.message);
        ir_program_free(&ir_program);
        lower_ir_program_free(&lower_program);
        return 0;
    }
    if (!lower_ir_dump_program(&lower_program, &actual_text) || !actual_text) {
        fprintf(stderr, "[lower-ir-reg] FAIL: callable-object bridge lower-IR dump failed\n");
        ir_program_free(&ir_program);
        lower_ir_program_free(&lower_program);
        return 0;
    }

    ok = strstr(actual_text, "func add1_env(env.0, x.1) {\n") != NULL &&
        strstr(actual_text, "call add1_env(0, 41)\n") != NULL &&
        strstr(actual_text, "ret tmp.1\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: callable-object bridge mismatch\nactual:\n%s\n",
            actual_text);
    }

    free(actual_text);
    ir_program_free(&ir_program);
    lower_ir_program_free(&lower_program);
    return ok;
}

static int test_lower_ir_bridges_moved_callable_object_indirect_call(void) {
    IrProgram ir_program;
    LowerIrProgram lower_program;
    LowerIrError lower_err;
    IrInstruction *grown_instructions;
    IrInstruction *instructions;
    char *actual_text = NULL;
    int ok = 0;

    if (!build_callable_object_bridge_ir_program(&ir_program)) {
        fprintf(stderr, "[lower-ir-reg] FAIL: moved callable-object bridge IR setup failed\n");
        return 0;
    }

    grown_instructions = (IrInstruction *)realloc(
        ir_program.functions[1].blocks[0].instructions,
        3u * sizeof(IrInstruction));
    if (!grown_instructions) {
        fprintf(stderr, "[lower-ir-reg] FAIL: out of memory growing moved callable-object bridge IR\n");
        ir_program_free(&ir_program);
        return 0;
    }
    ir_program.functions[1].blocks[0].instructions = grown_instructions;
    ir_program.functions[1].blocks[0].instruction_capacity = 3u;
    ir_program.functions[1].blocks[0].instruction_count = 3u;
    ir_program.functions[1].next_temp_id = 3u;
    instructions = ir_program.functions[1].blocks[0].instructions;

    memset(&instructions[2], 0, sizeof(instructions[2]));
    instructions[2] = instructions[1];
    instructions[1].kind = IR_INSTR_MOV;
    instructions[1].result.kind = IR_VALUE_TEMP;
    instructions[1].result.id = 1u;
    instructions[1].result.immediate = 0;
    instructions[1].as.mov_value.kind = IR_VALUE_TEMP;
    instructions[1].as.mov_value.id = 0u;
    instructions[1].as.mov_value.immediate = 0;
    instructions[2].as.call_indirect.callee.kind = IR_VALUE_TEMP;
    instructions[2].as.call_indirect.callee.id = 1u;
    instructions[2].result.kind = IR_VALUE_TEMP;
    instructions[2].result.id = 2u;
    instructions[2].result.immediate = 0;
    ir_program.functions[1].blocks[0].terminator.as.return_value.kind = IR_VALUE_TEMP;
    ir_program.functions[1].blocks[0].terminator.as.return_value.id = 2u;
    ir_program.functions[1].blocks[0].terminator.as.return_value.immediate = 0;

    lower_ir_program_init(&lower_program);
    if (!lower_ir_lower_from_ir(&ir_program, NULL, &lower_program, &lower_err)) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: moved callable-object bridge lowering failed at %d:%d: %s\n",
            lower_err.line,
            lower_err.column,
            lower_err.message);
        ir_program_free(&ir_program);
        lower_ir_program_free(&lower_program);
        return 0;
    }
    if (!lower_ir_dump_program(&lower_program, &actual_text) || !actual_text) {
        fprintf(stderr, "[lower-ir-reg] FAIL: moved callable-object bridge lower-IR dump failed\n");
        ir_program_free(&ir_program);
        lower_ir_program_free(&lower_program);
        free(actual_text);
        return 0;
    }

    ok = strstr(actual_text, "func add1_env(env.0, x.1) {\n") != NULL &&
        strstr(actual_text, "call add1_env(0, 41)\n") != NULL &&
        strstr(actual_text, "ret tmp.2\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: moved callable-object bridge mismatch\nactual:\n%s\n",
            actual_text);
    }

    free(actual_text);
    ir_program_free(&ir_program);
    lower_ir_program_free(&lower_program);
    return ok;
}

static int test_lower_ir_bridges_callable_object_env_and_shape_projection(void) {
    IrProgram ir_program;
    LowerIrProgram lower_program;
    LowerIrError lower_err;
    IrInstruction *grown_instructions;
    IrInstruction *instructions;
    char *actual_text = NULL;
    int ok = 0;

    if (!build_callable_object_bridge_ir_program(&ir_program)) {
        fprintf(stderr, "[lower-ir-reg] FAIL: callable-object projection IR setup failed\n");
        return 0;
    }

    grown_instructions = (IrInstruction *)realloc(
        ir_program.functions[1].blocks[0].instructions,
        4u * sizeof(IrInstruction));
    if (!grown_instructions) {
        fprintf(stderr, "[lower-ir-reg] FAIL: out of memory growing callable-object projection IR\n");
        ir_program_free(&ir_program);
        return 0;
    }
    ir_program.functions[1].blocks[0].instructions = grown_instructions;
    ir_program.functions[1].blocks[0].instruction_capacity = 4u;
    ir_program.functions[1].blocks[0].instruction_count = 4u;
    ir_program.functions[1].next_temp_id = 4u;
    instructions = ir_program.functions[1].blocks[0].instructions;

    memset(&instructions[3], 0, sizeof(instructions[3]));
    instructions[3] = instructions[1];
    instructions[0].as.fn_make.env.immediate = 7;
    instructions[1].kind = IR_INSTR_FN_ENV;
    instructions[1].result.kind = IR_VALUE_TEMP;
    instructions[1].result.id = 1u;
    instructions[1].result.immediate = 0;
    instructions[1].as.fn_project_operand.kind = IR_VALUE_TEMP;
    instructions[1].as.fn_project_operand.id = 0u;
    instructions[1].as.fn_project_operand.immediate = 0;
    instructions[2].kind = IR_INSTR_FN_SHAPE;
    instructions[2].result.kind = IR_VALUE_TEMP;
    instructions[2].result.id = 2u;
    instructions[2].result.immediate = 0;
    instructions[2].as.fn_project_operand.kind = IR_VALUE_TEMP;
    instructions[2].as.fn_project_operand.id = 0u;
    instructions[2].as.fn_project_operand.immediate = 0;
    instructions[3].kind = IR_INSTR_BINARY;
    instructions[3].result.kind = IR_VALUE_TEMP;
    instructions[3].result.id = 3u;
    instructions[3].result.immediate = 0;
    instructions[3].as.binary.op = IR_BINARY_ADD;
    instructions[3].as.binary.lhs.kind = IR_VALUE_TEMP;
    instructions[3].as.binary.lhs.id = 1u;
    instructions[3].as.binary.lhs.immediate = 0;
    instructions[3].as.binary.rhs.kind = IR_VALUE_TEMP;
    instructions[3].as.binary.rhs.id = 2u;
    instructions[3].as.binary.rhs.immediate = 0;
    ir_program.functions[1].blocks[0].terminator.as.return_value.kind = IR_VALUE_TEMP;
    ir_program.functions[1].blocks[0].terminator.as.return_value.id = 3u;
    ir_program.functions[1].blocks[0].terminator.as.return_value.immediate = 0;

    lower_ir_program_init(&lower_program);
    if (!lower_ir_lower_from_ir(&ir_program, NULL, &lower_program, &lower_err)) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: callable-object projection lowering failed at %d:%d: %s\n",
            lower_err.line,
            lower_err.column,
            lower_err.message);
        ir_program_free(&ir_program);
        lower_ir_program_free(&lower_program);
        return 0;
    }
    if (!lower_ir_dump_program(&lower_program, &actual_text) || !actual_text) {
        fprintf(stderr, "[lower-ir-reg] FAIL: callable-object projection lower-IR dump failed\n");
        ir_program_free(&ir_program);
        lower_ir_program_free(&lower_program);
        free(actual_text);
        return 0;
    }

    ok = strstr(actual_text, "tmp.1 = mov 7\n") != NULL &&
        strstr(actual_text, "tmp.2 = mov 0\n") != NULL &&
        strstr(actual_text, "tmp.3 = add tmp.1, tmp.2\n") != NULL &&
        strstr(actual_text, "ret tmp.3\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: callable-object projection bridge mismatch\nactual:\n%s\n",
            actual_text);
    }

    free(actual_text);
    ir_program_free(&ir_program);
    lower_ir_program_free(&lower_program);
    return ok;
}

static int test_lower_ir_bridges_callable_object_code_projection(void) {
    IrProgram ir_program;
    LowerIrProgram lower_program;
    LowerIrError lower_err;
    IrInstruction *grown_instructions;
    IrInstruction *instructions;
    char *actual_text = NULL;
    int ok = 0;

    if (!build_callable_object_bridge_ir_program(&ir_program)) {
        fprintf(stderr, "[lower-ir-reg] FAIL: callable-object code projection IR setup failed\n");
        return 0;
    }

    grown_instructions = (IrInstruction *)realloc(
        ir_program.functions[1].blocks[0].instructions,
        3u * sizeof(IrInstruction));
    if (!grown_instructions) {
        fprintf(stderr, "[lower-ir-reg] FAIL: out of memory growing callable-object code projection IR\n");
        ir_program_free(&ir_program);
        return 0;
    }
    ir_program.functions[1].blocks[0].instructions = grown_instructions;
    ir_program.functions[1].blocks[0].instruction_capacity = 3u;
    ir_program.functions[1].blocks[0].instruction_count = 3u;
    ir_program.functions[1].next_temp_id = 3u;
    instructions = ir_program.functions[1].blocks[0].instructions;

    memset(&instructions[2], 0, sizeof(instructions[2]));
    instructions[2] = instructions[1];
    instructions[1].kind = IR_INSTR_FN_CODE;
    instructions[1].result.kind = IR_VALUE_TEMP;
    instructions[1].result.id = 1u;
    instructions[1].result.immediate = 0;
    instructions[1].as.fn_project_operand.kind = IR_VALUE_TEMP;
    instructions[1].as.fn_project_operand.id = 0u;
    instructions[1].as.fn_project_operand.immediate = 0;
    instructions[2].kind = IR_INSTR_MOV;
    instructions[2].result.kind = IR_VALUE_TEMP;
    instructions[2].result.id = 2u;
    instructions[2].result.immediate = 0;
    instructions[2].as.mov_value.kind = IR_VALUE_TEMP;
    instructions[2].as.mov_value.id = 1u;
    instructions[2].as.mov_value.immediate = 0;
    ir_program.functions[1].blocks[0].terminator.as.return_value.kind = IR_VALUE_TEMP;
    ir_program.functions[1].blocks[0].terminator.as.return_value.id = 2u;
    ir_program.functions[1].blocks[0].terminator.as.return_value.immediate = 0;

    lower_ir_program_init(&lower_program);
    if (!lower_ir_lower_from_ir(&ir_program, NULL, &lower_program, &lower_err)) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: callable-object code projection lowering failed at %d:%d: %s\n",
            lower_err.line,
            lower_err.column,
            lower_err.message);
        ir_program_free(&ir_program);
        lower_ir_program_free(&lower_program);
        return 0;
    }
    if (!lower_ir_dump_program(&lower_program, &actual_text) || !actual_text) {
        fprintf(stderr, "[lower-ir-reg] FAIL: callable-object code projection lower-IR dump failed\n");
        ir_program_free(&ir_program);
        lower_ir_program_free(&lower_program);
        free(actual_text);
        return 0;
    }

    ok = strstr(actual_text, "tmp.1 = addr_function add1_env\n") != NULL &&
        strstr(actual_text, "tmp.2 = mov tmp.1\n") != NULL &&
        strstr(actual_text, "ret tmp.2\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: callable-object code projection bridge mismatch\nactual:\n%s\n",
            actual_text);
    }

    free(actual_text);
    ir_program_free(&ir_program);
    lower_ir_program_free(&lower_program);
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

static int build_diamond_join_temp_program(LowerIrProgram *program, LowerIrError *error) {
    LowerIrFunction *function = NULL;
    LowerIrBasicBlock *entry = NULL;
    LowerIrBasicBlock *then_block = NULL;
    LowerIrBasicBlock *else_block = NULL;
    LowerIrBasicBlock *join = NULL;
    size_t local_a_id;
    size_t join_temp;
    size_t cond_temp;
    LowerIrInstruction instruction;

    lower_ir_program_init(program);
    if (!lower_ir_program_append_function(program, "main", 1, &function, error) ||
        !lower_ir_function_append_local(function, "a", 1, &local_a_id, error) ||
        !lower_ir_function_append_block(function, NULL, &entry, error) ||
        !lower_ir_function_append_block(function, NULL, &then_block, error) ||
        !lower_ir_function_append_block(function, NULL, &else_block, error) ||
        !lower_ir_function_append_block(function, NULL, &join, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    join_temp = lower_ir_function_allocate_temp(function);
    cond_temp = lower_ir_function_allocate_temp(function);
    if (join_temp == (size_t)-1 || cond_temp == (size_t)-1) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(cond_temp);
    instruction.as.load_slot = lower_ir_slot_local(local_a_id);
    if (!lower_ir_block_append_instruction(entry, &instruction, error) ||
        !lower_ir_block_set_branch(entry, lower_ir_value_temp(cond_temp), 1, 2, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(join_temp);
    instruction.as.mov_value = lower_ir_value_immediate(1);
    if (!lower_ir_block_append_instruction(then_block, &instruction, error) ||
        !lower_ir_block_set_jump(then_block, 3, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    instruction.as.mov_value = lower_ir_value_immediate(2);
    if (!lower_ir_block_append_instruction(else_block, &instruction, error) ||
        !lower_ir_block_set_jump(else_block, 3, error) ||
        !lower_ir_block_set_return(join, lower_ir_value_temp(join_temp), error)) {
        lower_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_loop_header_join_temp_program(LowerIrProgram *program, LowerIrError *error) {
    LowerIrFunction *function = NULL;
    LowerIrBasicBlock *entry = NULL;
    LowerIrBasicBlock *header = NULL;
    LowerIrBasicBlock *body = NULL;
    LowerIrBasicBlock *exit_block = NULL;
    size_t loop_temp;
    LowerIrInstruction instruction;

    lower_ir_program_init(program);
    if (!lower_ir_program_append_function(program, "main", 1, &function, error) ||
        !lower_ir_function_append_block(function, NULL, &entry, error) ||
        !lower_ir_function_append_block(function, NULL, &header, error) ||
        !lower_ir_function_append_block(function, NULL, &body, error) ||
        !lower_ir_function_append_block(function, NULL, &exit_block, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    loop_temp = lower_ir_function_allocate_temp(function);
    if (loop_temp == (size_t)-1) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(loop_temp);
    instruction.as.mov_value = lower_ir_value_immediate(1);
    if (!lower_ir_block_append_instruction(entry, &instruction, error) ||
        !lower_ir_block_set_jump(entry, 1, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    if (!lower_ir_block_set_branch(header, lower_ir_value_temp(loop_temp), 2, 3, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    instruction.as.mov_value = lower_ir_value_immediate(2);
    if (!lower_ir_block_append_instruction(body, &instruction, error) ||
        !lower_ir_block_set_jump(body, 1, error) ||
        !lower_ir_block_set_return(exit_block, lower_ir_value_temp(loop_temp), error)) {
        lower_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_multi_backedge_loop_header_join_temp_program(LowerIrProgram *program, LowerIrError *error) {
    LowerIrFunction *function = NULL;
    LowerIrBasicBlock *entry = NULL;
    LowerIrBasicBlock *header = NULL;
    LowerIrBasicBlock *split = NULL;
    LowerIrBasicBlock *left_backedge = NULL;
    LowerIrBasicBlock *right_backedge = NULL;
    LowerIrBasicBlock *exit_block = NULL;
    LowerIrInstruction instruction;
    size_t join_temp;

    lower_ir_program_init(program);
    if (!lower_ir_program_append_function(program, "main", 1, &function, error) ||
        !lower_ir_function_append_block(function, NULL, &entry, error) ||
        !lower_ir_function_append_block(function, NULL, &header, error) ||
        !lower_ir_function_append_block(function, NULL, &split, error) ||
        !lower_ir_function_append_block(function, NULL, &left_backedge, error) ||
        !lower_ir_function_append_block(function, NULL, &right_backedge, error) ||
        !lower_ir_function_append_block(function, NULL, &exit_block, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    entry = &function->blocks[0];
    header = &function->blocks[1];
    split = &function->blocks[2];
    left_backedge = &function->blocks[3];
    right_backedge = &function->blocks[4];
    exit_block = &function->blocks[5];

    join_temp = lower_ir_function_allocate_temp(function);
    if (join_temp == (size_t)-1) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(join_temp);
    instruction.as.mov_value = lower_ir_value_immediate(1);
    if (!lower_ir_block_append_instruction(entry, &instruction, error) ||
        !lower_ir_block_set_jump(entry, 1, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    if (!lower_ir_block_set_branch(header, lower_ir_value_temp(join_temp), 2, 5, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    if (!lower_ir_block_set_branch(split, lower_ir_value_temp(join_temp), 3, 4, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(join_temp);
    instruction.as.mov_value = lower_ir_value_immediate(2);
    if (!lower_ir_block_append_instruction(left_backedge, &instruction, error) ||
        !lower_ir_block_set_jump(left_backedge, 1, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(join_temp);
    instruction.as.mov_value = lower_ir_value_immediate(3);
    if (!lower_ir_block_append_instruction(right_backedge, &instruction, error) ||
        !lower_ir_block_set_jump(right_backedge, 1, error) ||
        !lower_ir_block_set_return(exit_block, lower_ir_value_temp(join_temp), error)) {
        lower_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_two_carried_temps_loop_header_program(LowerIrProgram *program, LowerIrError *error) {
    LowerIrFunction *function = NULL;
    LowerIrBasicBlock *entry = NULL;
    LowerIrBasicBlock *header = NULL;
    LowerIrBasicBlock *body = NULL;
    LowerIrBasicBlock *exit_block = NULL;
    LowerIrInstruction instruction;
    size_t temp0;
    size_t temp1;

    lower_ir_program_init(program);
    if (!lower_ir_program_append_function(program, "main", 1, &function, error) ||
        !lower_ir_function_append_block(function, NULL, &entry, error) ||
        !lower_ir_function_append_block(function, NULL, &header, error) ||
        !lower_ir_function_append_block(function, NULL, &body, error) ||
        !lower_ir_function_append_block(function, NULL, &exit_block, error)) {
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
    instruction.kind = LOWER_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp0);
    instruction.as.mov_value = lower_ir_value_immediate(1);
    if (!lower_ir_block_append_instruction(entry, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    instruction.result = lower_ir_value_temp(temp1);
    instruction.as.mov_value = lower_ir_value_immediate(10);
    if (!lower_ir_block_append_instruction(entry, &instruction, error) ||
        !lower_ir_block_set_jump(entry, 1, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    if (!lower_ir_block_set_branch(header, lower_ir_value_temp(temp0), 2, 3, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp0);
    instruction.as.mov_value = lower_ir_value_immediate(2);
    if (!lower_ir_block_append_instruction(body, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    instruction.result = lower_ir_value_temp(temp1);
    instruction.as.mov_value = lower_ir_value_immediate(11);
    if (!lower_ir_block_append_instruction(body, &instruction, error) ||
        !lower_ir_block_set_jump(body, 1, error) ||
        !lower_ir_block_set_return(exit_block, lower_ir_value_temp(temp1), error)) {
        lower_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_nested_loop_carried_temps_program(LowerIrProgram *program, LowerIrError *error) {
    LowerIrFunction *function = NULL;
    LowerIrBasicBlock *entry = NULL;
    LowerIrBasicBlock *outer_header = NULL;
    LowerIrBasicBlock *inner_entry = NULL;
    LowerIrBasicBlock *inner_header = NULL;
    LowerIrBasicBlock *inner_body = NULL;
    LowerIrBasicBlock *outer_body = NULL;
    LowerIrBasicBlock *exit_block = NULL;
    LowerIrInstruction instruction;
    size_t temp0;
    size_t temp1;

    lower_ir_program_init(program);
    if (!lower_ir_program_append_function(program, "main", 1, &function, error) ||
        !lower_ir_function_append_block(function, NULL, &entry, error) ||
        !lower_ir_function_append_block(function, NULL, &outer_header, error) ||
        !lower_ir_function_append_block(function, NULL, &inner_entry, error) ||
        !lower_ir_function_append_block(function, NULL, &inner_header, error) ||
        !lower_ir_function_append_block(function, NULL, &inner_body, error) ||
        !lower_ir_function_append_block(function, NULL, &outer_body, error) ||
        !lower_ir_function_append_block(function, NULL, &exit_block, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    entry = &function->blocks[0];
    outer_header = &function->blocks[1];
    inner_entry = &function->blocks[2];
    inner_header = &function->blocks[3];
    inner_body = &function->blocks[4];
    outer_body = &function->blocks[5];
    exit_block = &function->blocks[6];

    temp0 = lower_ir_function_allocate_temp(function);
    temp1 = lower_ir_function_allocate_temp(function);
    if (temp0 == (size_t)-1 || temp1 == (size_t)-1) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp0);
    instruction.as.mov_value = lower_ir_value_immediate(1);
    if (!lower_ir_block_append_instruction(entry, &instruction, error) ||
        !lower_ir_block_set_jump(entry, 1, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    if (!lower_ir_block_set_branch(outer_header, lower_ir_value_temp(temp0), 2, 6, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp1);
    instruction.as.mov_value = lower_ir_value_immediate(10);
    if (!lower_ir_block_append_instruction(inner_entry, &instruction, error) ||
        !lower_ir_block_set_jump(inner_entry, 3, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    if (!lower_ir_block_set_branch(inner_header, lower_ir_value_temp(temp1), 4, 5, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp1);
    instruction.as.mov_value = lower_ir_value_immediate(11);
    if (!lower_ir_block_append_instruction(inner_body, &instruction, error) ||
        !lower_ir_block_set_jump(inner_body, 3, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp0);
    instruction.as.mov_value = lower_ir_value_immediate(2);
    if (!lower_ir_block_append_instruction(outer_body, &instruction, error) ||
        !lower_ir_block_set_jump(outer_body, 1, error) ||
        !lower_ir_block_set_return(exit_block, lower_ir_value_temp(temp0), error)) {
        lower_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_nested_same_temp_double_loop_program(LowerIrProgram *program, LowerIrError *error) {
    LowerIrFunction *function = NULL;
    LowerIrBasicBlock *entry = NULL;
    LowerIrBasicBlock *outer_header = NULL;
    LowerIrBasicBlock *inner_entry = NULL;
    LowerIrBasicBlock *inner_header = NULL;
    LowerIrBasicBlock *inner_body = NULL;
    LowerIrBasicBlock *outer_body = NULL;
    LowerIrBasicBlock *exit_block = NULL;
    LowerIrInstruction instruction;
    size_t temp0;

    lower_ir_program_init(program);
    if (!lower_ir_program_append_function(program, "main", 1, &function, error) ||
        !lower_ir_function_append_block(function, NULL, &entry, error) ||
        !lower_ir_function_append_block(function, NULL, &outer_header, error) ||
        !lower_ir_function_append_block(function, NULL, &inner_entry, error) ||
        !lower_ir_function_append_block(function, NULL, &inner_header, error) ||
        !lower_ir_function_append_block(function, NULL, &inner_body, error) ||
        !lower_ir_function_append_block(function, NULL, &outer_body, error) ||
        !lower_ir_function_append_block(function, NULL, &exit_block, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    entry = &function->blocks[0];
    outer_header = &function->blocks[1];
    inner_entry = &function->blocks[2];
    inner_header = &function->blocks[3];
    inner_body = &function->blocks[4];
    outer_body = &function->blocks[5];
    exit_block = &function->blocks[6];

    temp0 = lower_ir_function_allocate_temp(function);
    if (temp0 == (size_t)-1) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp0);
    instruction.as.mov_value = lower_ir_value_immediate(1);
    if (!lower_ir_block_append_instruction(entry, &instruction, error) ||
        !lower_ir_block_set_jump(entry, 1, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    if (!lower_ir_block_set_branch(outer_header, lower_ir_value_temp(temp0), 2, 6, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    if (!lower_ir_block_set_jump(inner_entry, 3, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    if (!lower_ir_block_set_branch(inner_header, lower_ir_value_temp(temp0), 4, 5, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp0);
    instruction.as.mov_value = lower_ir_value_immediate(2);
    if (!lower_ir_block_append_instruction(inner_body, &instruction, error) ||
        !lower_ir_block_set_jump(inner_body, 3, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp0);
    instruction.as.mov_value = lower_ir_value_immediate(3);
    if (!lower_ir_block_append_instruction(outer_body, &instruction, error) ||
        !lower_ir_block_set_jump(outer_body, 1, error) ||
        !lower_ir_block_set_return(exit_block, lower_ir_value_temp(temp0), error)) {
        lower_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_nested_same_temp_multi_backedge_inner_loop_program(LowerIrProgram *program, LowerIrError *error) {
    LowerIrFunction *function = NULL;
    LowerIrBasicBlock *entry = NULL;
    LowerIrBasicBlock *outer_header = NULL;
    LowerIrBasicBlock *inner_entry = NULL;
    LowerIrBasicBlock *inner_header = NULL;
    LowerIrBasicBlock *inner_split = NULL;
    LowerIrBasicBlock *inner_left = NULL;
    LowerIrBasicBlock *inner_right = NULL;
    LowerIrBasicBlock *outer_body = NULL;
    LowerIrBasicBlock *exit_block = NULL;
    LowerIrInstruction instruction;
    size_t temp0;

    lower_ir_program_init(program);
    if (!lower_ir_program_append_function(program, "main", 1, &function, error) ||
        !lower_ir_function_append_block(function, NULL, &entry, error) ||
        !lower_ir_function_append_block(function, NULL, &outer_header, error) ||
        !lower_ir_function_append_block(function, NULL, &inner_entry, error) ||
        !lower_ir_function_append_block(function, NULL, &inner_header, error) ||
        !lower_ir_function_append_block(function, NULL, &inner_split, error) ||
        !lower_ir_function_append_block(function, NULL, &inner_left, error) ||
        !lower_ir_function_append_block(function, NULL, &inner_right, error) ||
        !lower_ir_function_append_block(function, NULL, &outer_body, error) ||
        !lower_ir_function_append_block(function, NULL, &exit_block, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    entry = &function->blocks[0];
    outer_header = &function->blocks[1];
    inner_entry = &function->blocks[2];
    inner_header = &function->blocks[3];
    inner_split = &function->blocks[4];
    inner_left = &function->blocks[5];
    inner_right = &function->blocks[6];
    outer_body = &function->blocks[7];
    exit_block = &function->blocks[8];

    temp0 = lower_ir_function_allocate_temp(function);
    if (temp0 == (size_t)-1) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp0);
    instruction.as.mov_value = lower_ir_value_immediate(1);
    if (!lower_ir_block_append_instruction(entry, &instruction, error) ||
        !lower_ir_block_set_jump(entry, 1, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    if (!lower_ir_block_set_branch(outer_header, lower_ir_value_temp(temp0), 2, 8, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    if (!lower_ir_block_set_jump(inner_entry, 3, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    if (!lower_ir_block_set_branch(inner_header, lower_ir_value_temp(temp0), 4, 7, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    if (!lower_ir_block_set_branch(inner_split, lower_ir_value_temp(temp0), 5, 6, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp0);
    instruction.as.mov_value = lower_ir_value_immediate(2);
    if (!lower_ir_block_append_instruction(inner_left, &instruction, error) ||
        !lower_ir_block_set_jump(inner_left, 3, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    instruction.as.mov_value = lower_ir_value_immediate(4);
    if (!lower_ir_block_append_instruction(inner_right, &instruction, error) ||
        !lower_ir_block_set_jump(inner_right, 3, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    instruction.as.mov_value = lower_ir_value_temp(temp0);
    if (!lower_ir_block_append_instruction(outer_body, &instruction, error) ||
        !lower_ir_block_set_jump(outer_body, 1, error) ||
        !lower_ir_block_set_return(exit_block, lower_ir_value_temp(temp0), error)) {
        lower_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int expect_cfg_analysis(const char *case_id,
    int (*builder)(LowerIrProgram *program, LowerIrError *error),
    int (*checker)(const LowerIrProgram *program, const LowerIrCfgAnalysis *analysis)) {
    LowerIrProgram program;
    LowerIrCfgAnalysis analysis;
    LowerIrError error;
    int ok;

    if (!builder || !checker) {
        return 0;
    }

    if (!builder(&program, &error)) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: %s setup failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        return 0;
    }

    lower_ir_cfg_analysis_init(&analysis);
    if (!lower_ir_compute_cfg_analysis(&program.functions[0], &analysis, &error)) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: %s analysis failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        lower_ir_cfg_analysis_free(&analysis);
        lower_ir_program_free(&program);
        return 0;
    }

    ok = checker(&program, &analysis);
    if (!ok) {
        fprintf(stderr, "[lower-ir-reg] FAIL: %s analysis checker failed\n", case_id);
    }

    lower_ir_cfg_analysis_free(&analysis);
    lower_ir_program_free(&program);
    return ok;
}

static int expect_phi_placement(const char *case_id,
    int (*builder)(LowerIrProgram *program, LowerIrError *error),
    size_t temp_id,
    const unsigned char *expected_phi_blocks,
    size_t expected_block_count) {
    LowerIrProgram program;
    LowerIrCfgAnalysis analysis;
    LowerIrError error;
    unsigned char *definition_blocks = NULL;
    unsigned char *phi_blocks = NULL;
    size_t block_index;
    int ok = 1;

    if (!builder || !expected_phi_blocks) {
        return 0;
    }

    if (!builder(&program, &error)) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: %s setup failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        return 0;
    }

    lower_ir_cfg_analysis_init(&analysis);
    if (!lower_ir_compute_cfg_analysis(&program.functions[0], &analysis, &error)) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: %s analysis failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        lower_ir_cfg_analysis_free(&analysis);
        lower_ir_program_free(&program);
        return 0;
    }

    if (analysis.block_count != expected_block_count) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: %s expected block_count=%zu, got %zu\n",
            case_id,
            expected_block_count,
            analysis.block_count);
        lower_ir_cfg_analysis_free(&analysis);
        lower_ir_program_free(&program);
        return 0;
    }

    definition_blocks = (unsigned char *)calloc(analysis.block_count, sizeof(unsigned char));
    phi_blocks = (unsigned char *)calloc(analysis.block_count, sizeof(unsigned char));
    if (!definition_blocks || !phi_blocks) {
        fprintf(stderr, "[lower-ir-reg] FAIL: %s out of memory\n", case_id);
        free(definition_blocks);
        free(phi_blocks);
        lower_ir_cfg_analysis_free(&analysis);
        lower_ir_program_free(&program);
        return 0;
    }

    if (!lower_ir_collect_temp_definition_blocks(&program.functions[0], temp_id, definition_blocks, &error) ||
        !lower_ir_compute_phi_placement(&program.functions[0], &analysis, definition_blocks, phi_blocks, &error)) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: %s phi placement failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        free(definition_blocks);
        free(phi_blocks);
        lower_ir_cfg_analysis_free(&analysis);
        lower_ir_program_free(&program);
        return 0;
    }

    for (block_index = 0; block_index < analysis.block_count; ++block_index) {
        if (phi_blocks[block_index] != expected_phi_blocks[block_index]) {
            ok = 0;
            fprintf(stderr,
                "[lower-ir-reg] FAIL: %s expected phi_blocks[%zu]=%u, got %u\n",
                case_id,
                block_index,
                (unsigned)expected_phi_blocks[block_index],
                (unsigned)phi_blocks[block_index]);
            break;
        }
    }

    free(definition_blocks);
    free(phi_blocks);
    lower_ir_cfg_analysis_free(&analysis);
    lower_ir_program_free(&program);
    return ok;
}

static int expect_temp_phi_candidates(const char *case_id,
    int (*builder)(LowerIrProgram *program, LowerIrError *error),
    const unsigned char *expected_phi_candidates,
    size_t expected_block_count,
    size_t expected_temp_count) {
    LowerIrProgram program;
    LowerIrCfgAnalysis analysis;
    LowerIrError error;
    unsigned char *phi_candidates = NULL;
    int ok = 1;
    size_t block_index;
    size_t temp_id;

    if (!builder || !expected_phi_candidates) {
        return 0;
    }

    if (!builder(&program, &error)) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: %s setup failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        return 0;
    }

    lower_ir_cfg_analysis_init(&analysis);
    if (!lower_ir_compute_cfg_analysis(&program.functions[0], &analysis, &error)) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: %s analysis failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        lower_ir_cfg_analysis_free(&analysis);
        lower_ir_program_free(&program);
        return 0;
    }

    if (analysis.block_count != expected_block_count ||
        program.functions[0].next_temp_id != expected_temp_count) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: %s expected block_count=%zu temp_count=%zu, got %zu and %zu\n",
            case_id,
            expected_block_count,
            expected_temp_count,
            analysis.block_count,
            program.functions[0].next_temp_id);
        lower_ir_cfg_analysis_free(&analysis);
        lower_ir_program_free(&program);
        return 0;
    }

    if (expected_block_count > 0 && expected_temp_count > 0) {
        phi_candidates = (unsigned char *)calloc(expected_block_count * expected_temp_count, sizeof(unsigned char));
        if (!phi_candidates) {
            fprintf(stderr, "[lower-ir-reg] FAIL: %s out of memory\n", case_id);
            lower_ir_cfg_analysis_free(&analysis);
            lower_ir_program_free(&program);
            return 0;
        }
    }

    if (!lower_ir_compute_temp_phi_candidates(&program.functions[0], &analysis, phi_candidates, &error)) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: %s temp phi-candidate precompute failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        free(phi_candidates);
        lower_ir_cfg_analysis_free(&analysis);
        lower_ir_program_free(&program);
        return 0;
    }

    for (block_index = 0; block_index < expected_block_count; ++block_index) {
        for (temp_id = 0; temp_id < expected_temp_count; ++temp_id) {
            size_t flat_index = block_index * expected_temp_count + temp_id;

            if (phi_candidates[flat_index] != expected_phi_candidates[flat_index]) {
                ok = 0;
                fprintf(stderr,
                    "[lower-ir-reg] FAIL: %s expected phi_candidates[bb.%zu,tmp.%zu]=%u, got %u\n",
                    case_id,
                    block_index,
                    temp_id,
                    (unsigned)expected_phi_candidates[flat_index],
                    (unsigned)phi_candidates[flat_index]);
                goto done;
            }
        }
    }

done:
    free(phi_candidates);
    lower_ir_cfg_analysis_free(&analysis);
    lower_ir_program_free(&program);
    return ok;
}

static int expect_block_phi_candidate_lists(const char *case_id,
    int (*builder)(LowerIrProgram *program, LowerIrError *error),
    const size_t *expected_counts,
    const size_t *expected_temps,
    size_t expected_block_count,
    size_t expected_temp_count) {
    LowerIrProgram program;
    LowerIrCfgAnalysis analysis;
    LowerIrError error;
    size_t *phi_counts = NULL;
    size_t *phi_temps = NULL;
    int ok = 1;
    size_t block_index;
    size_t list_index;

    if (!builder || !expected_counts || !expected_temps) {
        return 0;
    }

    if (!builder(&program, &error)) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: %s setup failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        return 0;
    }

    lower_ir_cfg_analysis_init(&analysis);
    if (!lower_ir_compute_cfg_analysis(&program.functions[0], &analysis, &error)) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: %s analysis failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        lower_ir_cfg_analysis_free(&analysis);
        lower_ir_program_free(&program);
        return 0;
    }

    if (analysis.block_count != expected_block_count ||
        program.functions[0].next_temp_id != expected_temp_count) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: %s expected block_count=%zu temp_count=%zu, got %zu and %zu\n",
            case_id,
            expected_block_count,
            expected_temp_count,
            analysis.block_count,
            program.functions[0].next_temp_id);
        lower_ir_cfg_analysis_free(&analysis);
        lower_ir_program_free(&program);
        return 0;
    }

    if (expected_block_count > 0 && expected_temp_count > 0) {
        phi_counts = (size_t *)calloc(expected_block_count, sizeof(size_t));
        phi_temps = (size_t *)calloc(expected_block_count * expected_temp_count, sizeof(size_t));
        if (!phi_counts || !phi_temps) {
            fprintf(stderr, "[lower-ir-reg] FAIL: %s out of memory\n", case_id);
            free(phi_counts);
            free(phi_temps);
            lower_ir_cfg_analysis_free(&analysis);
            lower_ir_program_free(&program);
            return 0;
        }
    }

    if (!lower_ir_compute_temp_phi_candidate_lists(&program.functions[0], &analysis, phi_counts, phi_temps, &error)) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: %s block phi-candidate list failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        free(phi_counts);
        free(phi_temps);
        lower_ir_cfg_analysis_free(&analysis);
        lower_ir_program_free(&program);
        return 0;
    }

    for (block_index = 0; block_index < expected_block_count; ++block_index) {
        if (phi_counts[block_index] != expected_counts[block_index]) {
            ok = 0;
            fprintf(stderr,
                "[lower-ir-reg] FAIL: %s expected phi_count[bb.%zu]=%zu, got %zu\n",
                case_id,
                block_index,
                expected_counts[block_index],
                phi_counts[block_index]);
            goto done;
        }
        for (list_index = 0; list_index < expected_counts[block_index]; ++list_index) {
            size_t flat_index = block_index * expected_temp_count + list_index;
            if (phi_temps[flat_index] != expected_temps[flat_index]) {
                ok = 0;
                fprintf(stderr,
                    "[lower-ir-reg] FAIL: %s expected phi_temps[bb.%zu][%zu]=tmp.%zu, got tmp.%zu\n",
                    case_id,
                    block_index,
                    list_index,
                    expected_temps[flat_index],
                    phi_temps[flat_index]);
                goto done;
            }
        }
    }

done:
    free(phi_counts);
    free(phi_temps);
    lower_ir_cfg_analysis_free(&analysis);
    lower_ir_program_free(&program);
    return ok;
}

static int expect_pruned_block_phi_candidate_lists(const char *case_id,
    int (*builder)(LowerIrProgram *program, LowerIrError *error),
    const size_t *expected_counts,
    const size_t *expected_temps,
    size_t expected_block_count,
    size_t expected_temp_count) {
    LowerIrProgram program;
    LowerIrCfgAnalysis analysis;
    LowerIrError error;
    size_t *phi_counts = NULL;
    size_t *phi_temps = NULL;
    int ok = 1;
    size_t block_index;
    size_t list_index;

    if (!builder || !expected_counts || !expected_temps) {
        return 0;
    }

    if (!builder(&program, &error)) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: %s setup failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        return 0;
    }

    lower_ir_cfg_analysis_init(&analysis);
    if (!lower_ir_compute_cfg_analysis(&program.functions[0], &analysis, &error)) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: %s analysis failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        lower_ir_cfg_analysis_free(&analysis);
        lower_ir_program_free(&program);
        return 0;
    }

    if (analysis.block_count != expected_block_count ||
        program.functions[0].next_temp_id != expected_temp_count) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: %s expected block_count=%zu temp_count=%zu, got %zu and %zu\n",
            case_id,
            expected_block_count,
            expected_temp_count,
            analysis.block_count,
            program.functions[0].next_temp_id);
        lower_ir_cfg_analysis_free(&analysis);
        lower_ir_program_free(&program);
        return 0;
    }

    if (expected_block_count > 0 && expected_temp_count > 0) {
        phi_counts = (size_t *)calloc(expected_block_count, sizeof(size_t));
        phi_temps = (size_t *)calloc(expected_block_count * expected_temp_count, sizeof(size_t));
        if (!phi_counts || !phi_temps) {
            fprintf(stderr, "[lower-ir-reg] FAIL: %s out of memory\n", case_id);
            free(phi_counts);
            free(phi_temps);
            lower_ir_cfg_analysis_free(&analysis);
            lower_ir_program_free(&program);
            return 0;
        }
    }

    if (!lower_ir_compute_pruned_temp_phi_candidate_lists(&program.functions[0], &analysis, phi_counts, phi_temps, &error)) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: %s pruned phi-candidate list failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        free(phi_counts);
        free(phi_temps);
        lower_ir_cfg_analysis_free(&analysis);
        lower_ir_program_free(&program);
        return 0;
    }

    for (block_index = 0; block_index < expected_block_count; ++block_index) {
        if (phi_counts[block_index] != expected_counts[block_index]) {
            ok = 0;
            fprintf(stderr,
                "[lower-ir-reg] FAIL: %s expected pruned_phi_count[bb.%zu]=%zu, got %zu\n",
                case_id,
                block_index,
                expected_counts[block_index],
                phi_counts[block_index]);
            goto done;
        }
        for (list_index = 0; list_index < expected_counts[block_index]; ++list_index) {
            size_t flat_index = block_index * expected_temp_count + list_index;
            if (phi_temps[flat_index] != expected_temps[flat_index]) {
                ok = 0;
                fprintf(stderr,
                    "[lower-ir-reg] FAIL: %s expected pruned_phi_temps[bb.%zu][%zu]=tmp.%zu, got tmp.%zu\n",
                    case_id,
                    block_index,
                    list_index,
                    expected_temps[flat_index],
                    phi_temps[flat_index]);
                goto done;
            }
        }
    }

done:
    free(phi_counts);
    free(phi_temps);
    lower_ir_cfg_analysis_free(&analysis);
    lower_ir_program_free(&program);
    return ok;
}

static int expect_block_successor_phi_use_lists(const char *case_id,
    int (*builder)(LowerIrProgram *program, LowerIrError *error),
    const size_t *expected_counts,
    const size_t *expected_temps,
    size_t expected_block_count,
    size_t expected_temp_count) {
    LowerIrProgram program;
    LowerIrCfgAnalysis analysis;
    LowerIrError error;
    size_t *phi_counts = NULL;
    size_t *phi_temps = NULL;
    size_t *successor_use_counts = NULL;
    size_t *successor_use_temps = NULL;
    int ok = 1;
    size_t block_index;
    size_t list_index;

    if (!builder || !expected_counts || !expected_temps) {
        return 0;
    }

    if (!builder(&program, &error)) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: %s setup failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        return 0;
    }

    lower_ir_cfg_analysis_init(&analysis);
    if (!lower_ir_compute_cfg_analysis(&program.functions[0], &analysis, &error)) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: %s analysis failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        lower_ir_cfg_analysis_free(&analysis);
        lower_ir_program_free(&program);
        return 0;
    }

    if (analysis.block_count != expected_block_count ||
        program.functions[0].next_temp_id != expected_temp_count) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: %s expected block_count=%zu temp_count=%zu, got %zu and %zu\n",
            case_id,
            expected_block_count,
            expected_temp_count,
            analysis.block_count,
            program.functions[0].next_temp_id);
        lower_ir_cfg_analysis_free(&analysis);
        lower_ir_program_free(&program);
        return 0;
    }

    if (expected_block_count > 0 && expected_temp_count > 0) {
        phi_counts = (size_t *)calloc(expected_block_count, sizeof(size_t));
        phi_temps = (size_t *)calloc(expected_block_count * expected_temp_count, sizeof(size_t));
        successor_use_counts = (size_t *)calloc(expected_block_count, sizeof(size_t));
        successor_use_temps = (size_t *)calloc(expected_block_count * expected_temp_count, sizeof(size_t));
        if (!phi_counts || !phi_temps || !successor_use_counts || !successor_use_temps) {
            fprintf(stderr, "[lower-ir-reg] FAIL: %s out of memory\n", case_id);
            free(phi_counts);
            free(phi_temps);
            free(successor_use_counts);
            free(successor_use_temps);
            lower_ir_cfg_analysis_free(&analysis);
            lower_ir_program_free(&program);
            return 0;
        }
    }

    if (!lower_ir_compute_temp_phi_candidate_lists(&program.functions[0], &analysis, phi_counts, phi_temps, &error) ||
        !lower_ir_compute_block_successor_phi_use_lists(&program.functions[0],
            &analysis,
            phi_counts,
            phi_temps,
            successor_use_counts,
            successor_use_temps,
            &error)) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: %s successor phi-use list failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        free(phi_counts);
        free(phi_temps);
        free(successor_use_counts);
        free(successor_use_temps);
        lower_ir_cfg_analysis_free(&analysis);
        lower_ir_program_free(&program);
        return 0;
    }

    for (block_index = 0; block_index < expected_block_count; ++block_index) {
        if (successor_use_counts[block_index] != expected_counts[block_index]) {
            ok = 0;
            fprintf(stderr,
                "[lower-ir-reg] FAIL: %s expected successor_phi_use_count[bb.%zu]=%zu, got %zu\n",
                case_id,
                block_index,
                expected_counts[block_index],
                successor_use_counts[block_index]);
            goto done;
        }
        for (list_index = 0; list_index < expected_counts[block_index]; ++list_index) {
            size_t flat_index = block_index * expected_temp_count + list_index;
            if (successor_use_temps[flat_index] != expected_temps[flat_index]) {
                ok = 0;
                fprintf(stderr,
                    "[lower-ir-reg] FAIL: %s expected successor_phi_use_temps[bb.%zu][%zu]=tmp.%zu, got tmp.%zu\n",
                    case_id,
                    block_index,
                    list_index,
                    expected_temps[flat_index],
                    successor_use_temps[flat_index]);
                goto done;
            }
        }
    }

done:
    free(phi_counts);
    free(phi_temps);
    free(successor_use_counts);
    free(successor_use_temps);
    lower_ir_cfg_analysis_free(&analysis);
    lower_ir_program_free(&program);
    return ok;
}

typedef struct {
    long *events;
    size_t event_count;
    size_t event_capacity;
} LowerIrWalkTrace;

static int lower_ir_trace_walk_enter(const LowerIrFunction *function,
    const LowerIrCfgAnalysis *analysis,
    size_t block_id,
    void *user_data,
    LowerIrError *error) {
    LowerIrWalkTrace *trace = (LowerIrWalkTrace *)user_data;

    (void)function;
    (void)analysis;
    (void)error;

    if (!trace || trace->event_count >= trace->event_capacity) {
        return 0;
    }

    trace->events[trace->event_count++] = (long)block_id + 1;
    return 1;
}

static int lower_ir_trace_walk_leave(const LowerIrFunction *function,
    const LowerIrCfgAnalysis *analysis,
    size_t block_id,
    void *user_data,
    LowerIrError *error) {
    LowerIrWalkTrace *trace = (LowerIrWalkTrace *)user_data;

    (void)function;
    (void)analysis;
    (void)error;

    if (!trace || trace->event_count >= trace->event_capacity) {
        return 0;
    }

    trace->events[trace->event_count++] = -((long)block_id + 1);
    return 1;
}

static int expect_dominator_preorder(const char *case_id,
    int (*builder)(LowerIrProgram *program, LowerIrError *error),
    const size_t *expected_order,
    size_t expected_count) {
    LowerIrProgram program;
    LowerIrCfgAnalysis analysis;
    LowerIrError error;
    size_t *order = NULL;
    size_t actual_count = 0;
    int ok = 1;
    size_t index;

    if (!builder || !expected_order) {
        return 0;
    }

    if (!builder(&program, &error)) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: %s setup failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        return 0;
    }

    lower_ir_cfg_analysis_init(&analysis);
    if (!lower_ir_compute_cfg_analysis(&program.functions[0], &analysis, &error)) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: %s cfg analysis failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        lower_ir_program_free(&program);
        return 0;
    }

    order = (size_t *)malloc(analysis.block_count * sizeof(size_t));
    if (!order) {
        fprintf(stderr, "[lower-ir-reg] FAIL: %s out of memory allocating preorder buffer\n", case_id);
        lower_ir_cfg_analysis_free(&analysis);
        lower_ir_program_free(&program);
        return 0;
    }

    if (!lower_ir_compute_dominator_tree_preorder(&program.functions[0], &analysis, order, &actual_count, &error)) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: %s preorder failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        free(order);
        lower_ir_cfg_analysis_free(&analysis);
        lower_ir_program_free(&program);
        return 0;
    }

    if (actual_count != expected_count) {
        ok = 0;
    } else {
        for (index = 0; index < expected_count; ++index) {
            if (order[index] != expected_order[index]) {
                ok = 0;
                break;
            }
        }
    }

    if (!ok) {
        fprintf(stderr, "[lower-ir-reg] FAIL: %s dominator preorder mismatch\n", case_id);
    }

    free(order);
    lower_ir_cfg_analysis_free(&analysis);
    lower_ir_program_free(&program);
    return ok;
}

static int expect_dominator_walk(const char *case_id,
    int (*builder)(LowerIrProgram *program, LowerIrError *error),
    const long *expected_events,
    size_t expected_event_count) {
    LowerIrProgram program;
    LowerIrCfgAnalysis analysis;
    LowerIrError error;
    long *events = NULL;
    LowerIrWalkTrace trace;
    int ok = 1;
    size_t index;

    if (!builder || !expected_events) {
        return 0;
    }

    if (!builder(&program, &error)) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: %s setup failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        return 0;
    }

    lower_ir_cfg_analysis_init(&analysis);
    if (!lower_ir_compute_cfg_analysis(&program.functions[0], &analysis, &error)) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: %s cfg analysis failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        lower_ir_program_free(&program);
        return 0;
    }

    events = (long *)malloc(analysis.block_count * 2 * sizeof(long));
    if (!events) {
        fprintf(stderr, "[lower-ir-reg] FAIL: %s out of memory allocating walk trace\n", case_id);
        lower_ir_cfg_analysis_free(&analysis);
        lower_ir_program_free(&program);
        return 0;
    }

    trace.events = events;
    trace.event_count = 0;
    trace.event_capacity = analysis.block_count * 2;
    if (!lower_ir_walk_dominator_tree(&program.functions[0],
            &analysis,
            lower_ir_trace_walk_enter,
            lower_ir_trace_walk_leave,
            &trace,
            &error)) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: %s dominator walk failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        free(events);
        lower_ir_cfg_analysis_free(&analysis);
        lower_ir_program_free(&program);
        return 0;
    }

    if (trace.event_count != expected_event_count) {
        ok = 0;
    } else {
        for (index = 0; index < expected_event_count; ++index) {
            if (trace.events[index] != expected_events[index]) {
                ok = 0;
                break;
            }
        }
    }

    if (!ok) {
        fprintf(stderr, "[lower-ir-reg] FAIL: %s dominator walk mismatch\n", case_id);
    }

    free(events);
    lower_ir_cfg_analysis_free(&analysis);
    lower_ir_program_free(&program);
    return ok;
}

static int check_diamond_cfg_analysis(const LowerIrProgram *program, const LowerIrCfgAnalysis *analysis) {
    const LowerIrFunction *function = program ? &program->functions[0] : NULL;
    size_t count = analysis ? analysis->block_count : 0;

    if (!function || !analysis || count != 4) {
        return 0;
    }

    return analysis->predecessor_counts[0] == 0 &&
        analysis->predecessor_counts[1] == 1 &&
        analysis->predecessor_counts[2] == 1 &&
        analysis->predecessor_counts[3] == 2 &&
        analysis->predecessors[1 * count + 0] == 0 &&
        analysis->predecessors[2 * count + 0] == 0 &&
        analysis->predecessors[3 * count + 0] == 1 &&
        analysis->predecessors[3 * count + 1] == 2 &&
        analysis->successor_counts[0] == 2 &&
        analysis->successor_counts[1] == 1 &&
        analysis->successor_counts[2] == 1 &&
        analysis->successor_counts[3] == 0 &&
        analysis->successors[0 * count + 0] == 1 &&
        analysis->successors[0 * count + 1] == 2 &&
        analysis->successors[1 * count + 0] == 3 &&
        analysis->successors[2 * count + 0] == 3 &&
        analysis->reachable[0] &&
        analysis->reachable[1] &&
        analysis->reachable[2] &&
        analysis->reachable[3] &&
        analysis->immediate_dominator[0] == (size_t)-1 &&
        analysis->immediate_dominator[1] == 0 &&
        analysis->immediate_dominator[2] == 0 &&
        analysis->immediate_dominator[3] == 0 &&
        analysis->dominator_tree_child_counts[0] == 3 &&
        analysis->dominator_tree_child_counts[1] == 0 &&
        analysis->dominator_tree_child_counts[2] == 0 &&
        analysis->dominator_tree_child_counts[3] == 0 &&
        analysis->dominance_frontier_counts[0] == 0 &&
        analysis->dominance_frontier_counts[1] == 1 &&
        analysis->dominance_frontier_counts[2] == 1 &&
        analysis->dominance_frontier_counts[3] == 0 &&
        analysis->dominance_frontier[1 * count + 3] &&
        analysis->dominance_frontier[2 * count + 3];
}

static int check_loop_cfg_analysis(const LowerIrProgram *program, const LowerIrCfgAnalysis *analysis) {
    const LowerIrFunction *function = program ? &program->functions[0] : NULL;
    size_t count = analysis ? analysis->block_count : 0;

    if (!function || !analysis || count != 4) {
        return 0;
    }

    return analysis->predecessor_counts[0] == 0 &&
        analysis->predecessor_counts[1] == 2 &&
        analysis->predecessor_counts[2] == 1 &&
        analysis->predecessor_counts[3] == 1 &&
        analysis->predecessors[1 * count + 0] == 0 &&
        analysis->predecessors[1 * count + 1] == 2 &&
        analysis->predecessors[2 * count + 0] == 1 &&
        analysis->predecessors[3 * count + 0] == 1 &&
        analysis->successor_counts[0] == 1 &&
        analysis->successor_counts[1] == 2 &&
        analysis->successor_counts[2] == 1 &&
        analysis->successor_counts[3] == 0 &&
        analysis->successors[0 * count + 0] == 1 &&
        analysis->successors[1 * count + 0] == 2 &&
        analysis->successors[1 * count + 1] == 3 &&
        analysis->successors[2 * count + 0] == 1 &&
        analysis->reachable[0] &&
        analysis->reachable[1] &&
        analysis->reachable[2] &&
        analysis->reachable[3] &&
        analysis->immediate_dominator[0] == (size_t)-1 &&
        analysis->immediate_dominator[1] == 0 &&
        analysis->immediate_dominator[2] == 1 &&
        analysis->immediate_dominator[3] == 1 &&
        analysis->dominator_tree_child_counts[0] == 1 &&
        analysis->dominator_tree_child_counts[1] == 2 &&
        analysis->dominance_frontier_counts[0] == 0 &&
        analysis->dominance_frontier_counts[1] == 1 &&
        analysis->dominance_frontier_counts[2] == 1 &&
        analysis->dominance_frontier_counts[3] == 0 &&
        analysis->dominance_frontier[1 * count + 1] &&
        analysis->dominance_frontier[2 * count + 1];
}

static int check_multi_backedge_loop_cfg_analysis(const LowerIrProgram *program, const LowerIrCfgAnalysis *analysis) {
    const LowerIrFunction *function = program ? &program->functions[0] : NULL;
    size_t count = analysis ? analysis->block_count : 0;

    if (!function || !analysis || count != 6) {
        return 0;
    }

    return analysis->predecessor_counts[0] == 0 &&
        analysis->predecessor_counts[1] == 3 &&
        analysis->predecessor_counts[2] == 1 &&
        analysis->predecessor_counts[3] == 1 &&
        analysis->predecessor_counts[4] == 1 &&
        analysis->predecessor_counts[5] == 1 &&
        analysis->predecessors[1 * count + 0] == 0 &&
        analysis->predecessors[1 * count + 1] == 3 &&
        analysis->predecessors[1 * count + 2] == 4 &&
        analysis->predecessors[2 * count + 0] == 1 &&
        analysis->predecessors[3 * count + 0] == 2 &&
        analysis->predecessors[4 * count + 0] == 2 &&
        analysis->predecessors[5 * count + 0] == 1 &&
        analysis->successor_counts[0] == 1 &&
        analysis->successor_counts[1] == 2 &&
        analysis->successor_counts[2] == 2 &&
        analysis->successor_counts[3] == 1 &&
        analysis->successor_counts[4] == 1 &&
        analysis->successor_counts[5] == 0 &&
        analysis->successors[0 * count + 0] == 1 &&
        analysis->successors[1 * count + 0] == 2 &&
        analysis->successors[1 * count + 1] == 5 &&
        analysis->successors[2 * count + 0] == 3 &&
        analysis->successors[2 * count + 1] == 4 &&
        analysis->successors[3 * count + 0] == 1 &&
        analysis->successors[4 * count + 0] == 1 &&
        analysis->reachable[0] &&
        analysis->reachable[1] &&
        analysis->reachable[2] &&
        analysis->reachable[3] &&
        analysis->reachable[4] &&
        analysis->reachable[5] &&
        analysis->immediate_dominator[0] == (size_t)-1 &&
        analysis->immediate_dominator[1] == 0 &&
        analysis->immediate_dominator[2] == 1 &&
        analysis->immediate_dominator[3] == 2 &&
        analysis->immediate_dominator[4] == 2 &&
        analysis->immediate_dominator[5] == 1 &&
        analysis->dominator_tree_child_counts[0] == 1 &&
        analysis->dominator_tree_child_counts[1] == 2 &&
        analysis->dominator_tree_child_counts[2] == 2 &&
        analysis->dominator_tree_child_counts[3] == 0 &&
        analysis->dominator_tree_child_counts[4] == 0 &&
        analysis->dominator_tree_child_counts[5] == 0 &&
        analysis->dominance_frontier_counts[0] == 0 &&
        analysis->dominance_frontier_counts[1] == 1 &&
        analysis->dominance_frontier_counts[2] == 1 &&
        analysis->dominance_frontier_counts[3] == 1 &&
        analysis->dominance_frontier_counts[4] == 1 &&
        analysis->dominance_frontier_counts[5] == 0 &&
        analysis->dominance_frontier[1 * count + 1] &&
        analysis->dominance_frontier[2 * count + 1] &&
        analysis->dominance_frontier[3 * count + 1] &&
        analysis->dominance_frontier[4 * count + 1];
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

static int test_lower_ir_cfg_analysis_diamond_join_temp(void) {
    return expect_cfg_analysis("LOWER-IR-ANALYSIS-DIAMOND",
        build_diamond_join_temp_program,
        check_diamond_cfg_analysis);
}

static int test_lower_ir_phi_placement_diamond_join_temp(void) {
    static const unsigned char expected_phi_blocks[] = {0, 0, 0, 1};

    return expect_phi_placement("LOWER-IR-PHI-PLACEMENT-DIAMOND",
        build_diamond_join_temp_program,
        0,
        expected_phi_blocks,
        sizeof(expected_phi_blocks) / sizeof(expected_phi_blocks[0]));
}

static int test_lower_ir_temp_phi_candidates_diamond_join_temp(void) {
    static const unsigned char expected_phi_candidates[] = {
        0, 0,
        0, 0,
        0, 0,
        1, 0,
    };

    return expect_temp_phi_candidates("LOWER-IR-TEMP-PHI-CANDIDATES-DIAMOND",
        build_diamond_join_temp_program,
        expected_phi_candidates,
        4,
        2);
}

static int test_lower_ir_block_phi_candidate_lists_diamond_join_temp(void) {
    static const size_t expected_counts[] = {0, 0, 0, 1};
    static const size_t expected_temps[] = {
        0, 0,
        0, 0,
        0, 0,
        0, 0,
    };

    return expect_block_phi_candidate_lists("LOWER-IR-BLOCK-PHI-CANDIDATE-LISTS-DIAMOND",
        build_diamond_join_temp_program,
        expected_counts,
        expected_temps,
        4,
        2);
}

static int test_lower_ir_block_successor_phi_use_lists_diamond_join_temp(void) {
    static const size_t expected_counts[] = {0, 1, 1, 0};
    static const size_t expected_temps[] = {
        0, 0,
        0, 0,
        0, 0,
        0, 0,
    };

    return expect_block_successor_phi_use_lists("LOWER-IR-BLOCK-SUCCESSOR-PHI-USE-LISTS-DIAMOND",
        build_diamond_join_temp_program,
        expected_counts,
        expected_temps,
        4,
        2);
}

static int test_lower_ir_dominator_preorder_diamond_join_temp(void) {
    static const size_t expected_order[] = {0, 1, 2, 3};

    return expect_dominator_preorder("LOWER-IR-DOM-PREORDER-DIAMOND",
        build_diamond_join_temp_program,
        expected_order,
        sizeof(expected_order) / sizeof(expected_order[0]));
}

static int test_lower_ir_dominator_walk_diamond_join_temp(void) {
    static const long expected_events[] = {1, 2, -2, 3, -3, 4, -4, -1};

    return expect_dominator_walk("LOWER-IR-DOM-WALK-DIAMOND",
        build_diamond_join_temp_program,
        expected_events,
        sizeof(expected_events) / sizeof(expected_events[0]));
}

static int test_lower_ir_cfg_analysis_loop_header_join_temp(void) {
    return expect_cfg_analysis("LOWER-IR-ANALYSIS-LOOP",
        build_loop_header_join_temp_program,
        check_loop_cfg_analysis);
}

static int test_lower_ir_phi_placement_loop_header_join_temp(void) {
    static const unsigned char expected_phi_blocks[] = {0, 1, 0, 0};

    return expect_phi_placement("LOWER-IR-PHI-PLACEMENT-LOOP",
        build_loop_header_join_temp_program,
        0,
        expected_phi_blocks,
        sizeof(expected_phi_blocks) / sizeof(expected_phi_blocks[0]));
}

static int test_lower_ir_temp_phi_candidates_loop_header_join_temp(void) {
    static const unsigned char expected_phi_candidates[] = {
        0,
        1,
        0,
        0,
    };

    return expect_temp_phi_candidates("LOWER-IR-TEMP-PHI-CANDIDATES-LOOP",
        build_loop_header_join_temp_program,
        expected_phi_candidates,
        4,
        1);
}

static int test_lower_ir_block_phi_candidate_lists_loop_header_join_temp(void) {
    static const size_t expected_counts[] = {0, 1, 0, 0};
    static const size_t expected_temps[] = {
        0,
        0,
        0,
        0,
    };

    return expect_block_phi_candidate_lists("LOWER-IR-BLOCK-PHI-CANDIDATE-LISTS-LOOP",
        build_loop_header_join_temp_program,
        expected_counts,
        expected_temps,
        4,
        1);
}

static int test_lower_ir_block_successor_phi_use_lists_loop_header_join_temp(void) {
    static const size_t expected_counts[] = {1, 0, 1, 0};
    static const size_t expected_temps[] = {
        0,
        0,
        0,
        0,
    };

    return expect_block_successor_phi_use_lists("LOWER-IR-BLOCK-SUCCESSOR-PHI-USE-LISTS-LOOP",
        build_loop_header_join_temp_program,
        expected_counts,
        expected_temps,
        4,
        1);
}

static int test_lower_ir_temp_phi_candidates_two_carried_temps_loop_header(void) {
    static const unsigned char expected_phi_candidates[] = {
        0, 0,
        1, 1,
        0, 0,
        0, 0,
    };

    return expect_temp_phi_candidates("LOWER-IR-TEMP-PHI-CANDIDATES-TWO-CARRIED-TEMPS-LOOP",
        build_two_carried_temps_loop_header_program,
        expected_phi_candidates,
        4,
        2);
}

static int test_lower_ir_temp_phi_candidates_nested_loop_carried_temps(void) {
    static const unsigned char expected_phi_candidates[] = {
        0, 0,
        1, 1,
        0, 0,
        0, 1,
        0, 0,
        0, 0,
        0, 0,
    };

    return expect_temp_phi_candidates("LOWER-IR-TEMP-PHI-CANDIDATES-NESTED-LOOP-CARRIED-TEMPS",
        build_nested_loop_carried_temps_program,
        expected_phi_candidates,
        7,
        2);
}

static int test_lower_ir_pruned_block_phi_candidate_lists_nested_loop_carried_temps(void) {
    static const size_t expected_counts[] = {0, 1, 0, 1, 0, 0, 0};
    static const size_t expected_temps[] = {
        0, 0,
        0, 0,
        0, 0,
        1, 0,
        0, 0,
        0, 0,
        0, 0,
    };

    return expect_pruned_block_phi_candidate_lists("LOWER-IR-PRUNED-BLOCK-PHI-CANDIDATE-LISTS-NESTED-LOOP-CARRIED-TEMPS",
        build_nested_loop_carried_temps_program,
        expected_counts,
        expected_temps,
        7,
        2);
}

static int test_lower_ir_temp_phi_candidates_nested_same_temp_double_loop(void) {
    static const unsigned char expected_phi_candidates[] = {
        0,
        1,
        0,
        1,
        0,
        0,
        0,
    };

    return expect_temp_phi_candidates("LOWER-IR-TEMP-PHI-CANDIDATES-NESTED-SAME-TEMP-DOUBLE-LOOP",
        build_nested_same_temp_double_loop_program,
        expected_phi_candidates,
        7,
        1);
}

static int test_lower_ir_temp_phi_candidates_nested_same_temp_multi_backedge_inner_loop(void) {
    static const unsigned char expected_phi_candidates[] = {
        0,
        1,
        0,
        1,
        0,
        0,
        0,
        0,
        0,
    };

    return expect_temp_phi_candidates("LOWER-IR-TEMP-PHI-CANDIDATES-NESTED-SAME-TEMP-MULTI-BACKEDGE-INNER-LOOP",
        build_nested_same_temp_multi_backedge_inner_loop_program,
        expected_phi_candidates,
        9,
        1);
}

static int test_lower_ir_dominator_preorder_loop_header_join_temp(void) {
    static const size_t expected_order[] = {0, 1, 2, 3};

    return expect_dominator_preorder("LOWER-IR-DOM-PREORDER-LOOP",
        build_loop_header_join_temp_program,
        expected_order,
        sizeof(expected_order) / sizeof(expected_order[0]));
}

static int test_lower_ir_dominator_walk_loop_header_join_temp(void) {
    static const long expected_events[] = {1, 2, 3, -3, 4, -4, -2, -1};

    return expect_dominator_walk("LOWER-IR-DOM-WALK-LOOP",
        build_loop_header_join_temp_program,
        expected_events,
        sizeof(expected_events) / sizeof(expected_events[0]));
}

static int test_lower_ir_cfg_analysis_multi_backedge_loop_header_join_temp(void) {
    return expect_cfg_analysis("LOWER-IR-ANALYSIS-MULTI-BACKEDGE-LOOP",
        build_multi_backedge_loop_header_join_temp_program,
        check_multi_backedge_loop_cfg_analysis);
}

static int test_lower_ir_phi_placement_multi_backedge_loop_header_join_temp(void) {
    static const unsigned char expected_phi_blocks[] = {0, 1, 0, 0, 0, 0};

    return expect_phi_placement("LOWER-IR-PHI-PLACEMENT-MULTI-BACKEDGE-LOOP",
        build_multi_backedge_loop_header_join_temp_program,
        0,
        expected_phi_blocks,
        sizeof(expected_phi_blocks) / sizeof(expected_phi_blocks[0]));
}

static int test_lower_ir_temp_phi_candidates_multi_backedge_loop_header_join_temp(void) {
    static const unsigned char expected_phi_candidates[] = {
        0,
        1,
        0,
        0,
        0,
        0,
    };

    return expect_temp_phi_candidates("LOWER-IR-TEMP-PHI-CANDIDATES-MULTI-BACKEDGE-LOOP",
        build_multi_backedge_loop_header_join_temp_program,
        expected_phi_candidates,
        6,
        1);
}

static int test_lower_ir_block_phi_candidate_lists_multi_backedge_loop_header_join_temp(void) {
    static const size_t expected_counts[] = {0, 1, 0, 0, 0, 0};
    static const size_t expected_temps[] = {
        0,
        0,
        0,
        0,
        0,
        0,
    };

    return expect_block_phi_candidate_lists("LOWER-IR-BLOCK-PHI-CANDIDATE-LISTS-MULTI-BACKEDGE-LOOP",
        build_multi_backedge_loop_header_join_temp_program,
        expected_counts,
        expected_temps,
        6,
        1);
}

static int test_lower_ir_block_successor_phi_use_lists_multi_backedge_loop_header_join_temp(void) {
    static const size_t expected_counts[] = {1, 0, 0, 1, 1, 0};
    static const size_t expected_temps[] = {
        0,
        0,
        0,
        0,
        0,
        0,
    };

    return expect_block_successor_phi_use_lists("LOWER-IR-BLOCK-SUCCESSOR-PHI-USE-LISTS-MULTI-BACKEDGE-LOOP",
        build_multi_backedge_loop_header_join_temp_program,
        expected_counts,
        expected_temps,
        6,
        1);
}

static int test_lower_ir_dominator_preorder_multi_backedge_loop_header_join_temp(void) {
    static const size_t expected_order[] = {0, 1, 2, 3, 4, 5};

    return expect_dominator_preorder("LOWER-IR-DOM-PREORDER-MULTI-BACKEDGE-LOOP",
        build_multi_backedge_loop_header_join_temp_program,
        expected_order,
        sizeof(expected_order) / sizeof(expected_order[0]));
}

static int test_lower_ir_dominator_walk_multi_backedge_loop_header_join_temp(void) {
    static const long expected_events[] = {1, 2, 3, 4, -4, 5, -5, -3, 6, -6, -2, -1};

    return expect_dominator_walk("LOWER-IR-DOM-WALK-MULTI-BACKEDGE-LOOP",
        build_multi_backedge_loop_header_join_temp_program,
        expected_events,
        sizeof(expected_events) / sizeof(expected_events[0]));
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

static int test_lower_ir_lowers_returned_function_value_parameter_bind_and_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int id(int f(int))(int) { return f; }\n"
            "int add1(int x){ return x+1; }\n"
            "int main(){ int g(int)=id(add1); return g(41); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare id(__ret0.0, f.1)\n") != NULL &&
        strstr(actual_text, "func add1(x.0) {\n") != NULL &&
        strstr(actual_text, "store_local g$ftag.0, 1\n") != NULL &&
        strstr(actual_text, "tmp.1 = call __fnwrap_add1(0, 41)\n") != NULL &&
        strstr(actual_text, "func __fnwrap_add1(__env.0, x.1) {\n") != NULL &&
        strstr(actual_text, "__retfn_declslot") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-LOWER-RETURNED-FUNCTION-VALUE-PARAM-BIND mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_lowers_dynamic_returned_function_value_parameter_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int id(int f(int))(int) { return f; }\n"
            "int add1(int x){ return x+1; }\n"
            "int add2(int x){ return x+2; }\n"
            "int main(){ int c=1; int f(int)=add1; if(c) f=add2; return id(f)(40); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare id(__ret0.0, f.1)\n") != NULL &&
        strstr(actual_text, "func add1(x.0) {\n") != NULL &&
        strstr(actual_text, "func add2(x.0) {\n") != NULL &&
        strstr(actual_text, "store_local c.0, 1\n") != NULL &&
        strstr(actual_text, "store_local f$ftag.1, 1\n") != NULL &&
        strstr(actual_text, "store_local f$ftag.1, 2\n") != NULL &&
        strstr(actual_text, "call __fnwrap_add2(0, 40)\n") != NULL &&
        strstr(actual_text, "func __fnwrap_add2(__env.0, x.1) {\n") != NULL &&
        strstr(actual_text, "__retfn_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-LOWER-DYNAMIC-RETURNED-FUNCTION-VALUE-PARAM-IMM mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_lowers_returned_closure_backed_function_value_parameter_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int id(int f(int))(int) { return f; }\n"
            "int main(){ int x=3; int f(int)=closure [x] int (int y) { return x + y; }; return id(f)(4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare id(__ret0.0, f.1)\n") != NULL &&
        strstr(actual_text, "store_local x.0, 3\n") != NULL &&
        strstr(actual_text, "store_local f$closurecap$0.1, ") != NULL &&
        strstr(actual_text, "store_local f$ftag.2, 1\n") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "call main__closure_f_131094(") != NULL &&
        strstr(actual_text, "func main__closure_f_131094(x.0, y.1) {\n") != NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL &&
        strstr(actual_text, "__retclosure_declslot") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-LOWER-RETURNED-CLOSURE-BACKED-FUNCTION-VALUE-PARAM-IMM mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_lowers_returned_closure_backed_function_value_parameter_bind_and_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int id(int f(int))(int) { return f; }\n"
            "int main(){ int x=3; int f(int)=closure [x] int (int y) { return x + y; }; int g(int)=id(f); return g(4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare id(__ret0.0, f.1)\n") != NULL &&
        strstr(actual_text, "store_local x.0, 3\n") != NULL &&
        strstr(actual_text, "store_local f$closurecap$0.1, ") != NULL &&
        strstr(actual_text, "store_local f$ftag.2, 1\n") != NULL &&
        strstr(actual_text, "store_local g$ftag.3, ") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "call main__closure_f_131094(") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL &&
        strstr(actual_text, "func main__closure_f_131094(x.0, y.1) {\n") != NULL &&
        strstr(actual_text, "__retclosure_declslot") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-LOWER-RETURNED-CLOSURE-BACKED-FUNCTION-VALUE-PARAM-BIND mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_returned_function_value_reassignment_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "store_local h$ftag.0, 1\n") != NULL &&
        strstr(actual_text, "addr_local h$ftag.1\n") != NULL &&
        strstr(actual_text, "call pick(") != NULL &&
        strstr(actual_text, "store_local h$ftag.0, tmp.") != NULL &&
        strstr(actual_text, "call __fnwrap_add1(0, 40)") != NULL &&
        strstr(actual_text, "call __fnwrap_add2(0, 40)") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-RETURNED-FNVAL-REASSIGN mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_returned_zero_arg_function_value_reassignment_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "store_local h$ftag.0, 1\n") != NULL &&
        strstr(actual_text, "addr_local h$ftag.1\n") != NULL &&
        strstr(actual_text, "call pick(") != NULL &&
        strstr(actual_text, "store_local h$ftag.0, tmp.") != NULL &&
        strstr(actual_text, "call __fnwrap_next1(0)") != NULL &&
        strstr(actual_text, "call __fnwrap_next2(0)") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-RETURNED-ZERO-FNVAL-REASSIGN mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_returned_closure_reassignment_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int make(int x)(int){ return closure [x] int (int y){ return x+y; }; }\n"
            "int main(){ int h(int)=make(3); h=make(4); return h(5); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call make(") != NULL &&
        strstr(actual_text, "addr_local h$closurecap$0.0\n") != NULL &&
        strstr(actual_text, "addr_local h$closurecap$0.2\n") != NULL &&
        strstr(actual_text, "store_local h$closurecap$0.0, tmp.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_make__retclosure_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-RETURNED-CLOSURE-REASSIGN mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_returned_zero_arg_void_closure_reassignment_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "void make(int x)(){ return closure [x] void (){ putint(x); return; }; }\n"
            "int main(){ void h()=make(3); h=make(4); h(); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare putint(param0.0)\n") != NULL &&
        strstr(actual_text, "call make(") != NULL &&
        strstr(actual_text, "addr_local h$closurecap$0.0\n") != NULL &&
        strstr(actual_text, "addr_local h$closurecap$0.2\n") != NULL &&
        strstr(actual_text, "store_local h$closurecap$0.0, tmp.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_make__retclosure_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-RETURNED-ZERO-VOID-CLOSURE-REASSIGN mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_wrapped_function_value_reassignment_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int add2(int x){ return x+2; }\n"
            "int main(){ int f(int)=add1; int g(int)=add2; f=(0, g); return f(40); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "store_local f$ftag.0, 1\n") != NULL &&
        strstr(actual_text, "store_local f$ftag.0, tmp.") != NULL &&
        strstr(actual_text, "call __fnwrap_add2(0, 40)") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-WRAPPED-FNVAL-REASSIGN mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_assignment_result_function_value_reassignment_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int add2(int x){ return x+2; }\n"
            "int main(){ int f(int)=add1; int g(int)=add2; int h(int)=add1; f=(h=g); return f(40); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "store_local h$ftag.2, tmp.") != NULL &&
        strstr(actual_text, "store_local f$ftag.0, tmp.") != NULL &&
        strstr(actual_text, "call __fnwrap_add2(0, 40)") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-ASSIGNMENT-RESULT-FNVAL-REASSIGN mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_wrapped_returned_function_value_reassignment_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int add2(int x){ return x+2; }\n"
            "int pick(int c)(int){ int f(int)=add1; int g(int)=add2; if(c) f=g; return f; }\n"
            "int main(){ int h(int)=add1; h=(0, pick(1)); return h(40); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call pick(") != NULL &&
        strstr(actual_text, "store_local h$ftag.0, tmp.") != NULL &&
        strstr(actual_text, "call __fnwrap_add2(0, 40)") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-WRAPPED-RETURNED-FNVAL-REASSIGN mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_wrapped_returned_function_value_local_initializer_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int add2(int x){ return x+2; }\n"
            "int pick(int c)(int){ int f(int)=add1; int g(int)=add2; if(c) f=g; return f; }\n"
            "int main(){ int h(int)=(0, pick(1)); return h(40); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call pick(") != NULL &&
        strstr(actual_text, "load_local h$ftag.0\n") != NULL &&
        strstr(actual_text, "call __fnwrap_add1(0, 40)") != NULL &&
        strstr(actual_text, "call __fnwrap_add2(0, 40)") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-WRAPPED-RETURNED-FNVAL-LOCAL-INIT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_wrapped_returned_zero_arg_function_value_local_initializer_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int next1(){ return 1; }\n"
            "int next2(){ return 2; }\n"
            "int pick(int c)(){ int f()=next1; int g()=next2; if(c) f=g; return f; }\n"
            "int main(){ int h()=(0, pick(1)); return h(); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call pick(") != NULL &&
        strstr(actual_text, "load_local h$ftag.0\n") != NULL &&
        strstr(actual_text, "call __fnwrap_next1(0)") != NULL &&
        strstr(actual_text, "call __fnwrap_next2(0)") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-WRAPPED-RETURNED-ZERO-FNVAL-LOCAL-INIT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_assignment_result_returned_closure_local_initializer_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int make(int x)(int){ return closure [x] int (int y){ return x+y; }; }\n"
            "int main(){ int g(int)=make(5); int h(int)=(g=make(4)); return h(5); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call make(") == 2u &&
        strstr(actual_text, "load_local g$closurecap$0.2\n") != NULL &&
        strstr(actual_text, "store_local g$closurecap$0.0, tmp.") != NULL &&
        strstr(actual_text, "load_local g$ftag.1\n") != NULL &&
        strstr(actual_text, "store_local h$ftag.3, tmp.") != NULL &&
        strstr(actual_text, "store_local h$closurecap$0.4, tmp.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_make__retclosure_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-ASSIGNMENT-RESULT-RETURNED-CLOSURE-LOCAL-INIT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_wrapped_returned_call_ternary_function_value_local_initializer_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "call pick(") != NULL &&
        strstr(actual_text, "call __fnwrap_add1(0, 40)") != NULL &&
        strstr(actual_text, "call __fnwrap_add2(0, 40)") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-FNVAL-LOCAL-INIT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_wrapped_returned_call_ternary_closure_local_initializer_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int pick(int c)(int){ int x=3; int y=5; int f(int)=closure [x] int (int z){ return x+z; }; int g(int)=closure [y] int (int z){ return y+z; }; return ((putint(0), c) ? f : g); }\n"
            "int main(){ int h(int)=(0, pick(1)); return h(4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call putint(0)\n") != NULL &&
        strstr(actual_text, "store_local h$ftag.0, tmp.") != NULL &&
        strstr(actual_text, "store_local h$closurecap$0.1, tmp.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-CLOSURE-LOCAL-INIT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_wrapped_returned_call_ternary_function_value_local_bounce_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "store_local g$ftag.1, tmp.") != NULL &&
        strstr(actual_text, "call __fnwrap_add1(0, 40)") != NULL &&
        strstr(actual_text, "call __fnwrap_add2(0, 40)") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-FNVAL-BOUNCE mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_wrapped_returned_call_ternary_function_value_forward_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "call __fnwrap_add1(0, 40)") != NULL &&
        strstr(actual_text, "call __fnwrap_add2(0, 40)") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-FNVAL-FORWARD mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_wrapped_returned_call_ternary_closure_local_bounce_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int pick(int c)(int){ int x=3; int y=5; int f(int)=closure [x] int (int z){ return x+z; }; int g(int)=closure [y] int (int z){ return y+z; }; return ((putint(0), c) ? f : g); }\n"
            "int main(){ int h(int)=(0, pick(1)); int g(int)=h; return g(4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call putint(0)\n") != NULL &&
        strstr(actual_text, "store_local g$ftag.4, tmp.") != NULL &&
        strstr(actual_text, "store_local g$closurecap$0.5, tmp.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-CLOSURE-BOUNCE mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_wrapped_returned_call_ternary_closure_forward_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-CLOSURE-FORWARD mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_wrapped_returned_call_ternary_function_value_bind_return_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "store_indirect tmp.2, tmp.3\n") != NULL &&
        strstr(actual_text, "call __fnwrap_add1(0, 40)") != NULL &&
        strstr(actual_text, "call __fnwrap_add2(0, 40)") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-FNVAL-BIND-RETURN mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_wrapped_returned_call_ternary_function_value_bind_return_actual_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "call __fnwrap_add1(0, 40)") != NULL &&
        strstr(actual_text, "call __fnwrap_add2(0, 40)") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-FNVAL-BIND-RETURN-ACTUAL mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_wrapped_returned_call_ternary_closure_bind_return_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "store_indirect tmp.5, tmp.6\n") != NULL &&
        strstr(actual_text, "store_indirect tmp.7, tmp.8\n") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-CLOSURE-BIND-RETURN mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_wrapped_returned_call_ternary_closure_bind_return_actual_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-CLOSURE-BIND-RETURN-ACTUAL mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_function_value_bind_return_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "store_indirect tmp.2, tmp.3\n") != NULL &&
        strstr(actual_text, "call __fnwrap_next1(0)") != NULL &&
        strstr(actual_text, "call __fnwrap_next2(0)") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-FNVAL-BIND-RETURN mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_function_value_bind_return_actual_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "call __fnwrap_next1(0)") != NULL &&
        strstr(actual_text, "call __fnwrap_next2(0)") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-FNVAL-BIND-RETURN-ACTUAL mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_function_value_bind_return_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "store_indirect tmp.2, tmp.3\n") != NULL &&
        strstr(actual_text, "call __fnwrap_ping1(0)") != NULL &&
        strstr(actual_text, "call __fnwrap_ping2(0)") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-VOID-FNVAL-BIND-RETURN mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_function_value_bind_return_actual_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "call __fnwrap_ping1(0)") != NULL &&
        strstr(actual_text, "call __fnwrap_ping2(0)") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-VOID-FNVAL-BIND-RETURN-ACTUAL mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_closure_bind_return_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "store_indirect tmp.5, tmp.6\n") != NULL &&
        strstr(actual_text, "store_indirect tmp.7, tmp.8\n") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-CLOSURE-BIND-RETURN mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_closure_bind_return_actual_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-CLOSURE-BIND-RETURN-ACTUAL mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_closure_bind_return_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "store_indirect tmp.5, tmp.6\n") != NULL &&
        strstr(actual_text, "store_indirect tmp.7, tmp.8\n") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-VOID-CLOSURE-BIND-RETURN mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_closure_bind_return_actual_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-VOID-CLOSURE-BIND-RETURN-ACTUAL mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_wrapped_returned_call_ternary_function_value_bounce_passthrough_bind_return_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "store_local g$ftag.2, tmp.") != NULL &&
        strstr(actual_text, "store_local p$ftag.3, tmp.") != NULL &&
        strstr(actual_text, "call __fnwrap_add1(0, 40)") != NULL &&
        strstr(actual_text, "call __fnwrap_add2(0, 40)") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-FNVAL-BOUNCE-PASSTHROUGH-BIND-RETURN mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_wrapped_returned_call_ternary_function_value_bounce_passthrough_bind_return_actual_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "store_local p$ftag.3, tmp.") != NULL &&
        strstr(actual_text, "call __fnwrap_add1(0, 40)") != NULL &&
        strstr(actual_text, "call __fnwrap_add2(0, 40)") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-FNVAL-BOUNCE-PASSTHROUGH-BIND-RETURN-ACTUAL mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_wrapped_returned_call_ternary_closure_bounce_passthrough_bind_return_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "store_local g$ftag.6, tmp.") != NULL &&
        strstr(actual_text, "store_local g$closurecap$0.7, tmp.") != NULL &&
        strstr(actual_text, "store_local p$ftag.8, tmp.") != NULL &&
        strstr(actual_text, "store_local p$closurecap$0.9, tmp.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-CLOSURE-BOUNCE-PASSTHROUGH-BIND-RETURN mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_wrapped_returned_call_ternary_closure_bounce_passthrough_bind_return_actual_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "store_local p$closurecap$0.9, tmp.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-CLOSURE-BOUNCE-PASSTHROUGH-BIND-RETURN-ACTUAL mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_function_value_bounce_passthrough_bind_return_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "store_local g$ftag.2, tmp.") != NULL &&
        strstr(actual_text, "store_local p$ftag.3, tmp.") != NULL &&
        strstr(actual_text, "call __fnwrap_next1(0)") != NULL &&
        strstr(actual_text, "call __fnwrap_next2(0)") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-FNVAL-BOUNCE-PASSTHROUGH-BIND-RETURN mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_function_value_bounce_passthrough_bind_return_actual_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "store_local p$ftag.3, tmp.") != NULL &&
        strstr(actual_text, "call __fnwrap_next1(0)") != NULL &&
        strstr(actual_text, "call __fnwrap_next2(0)") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-FNVAL-BOUNCE-PASSTHROUGH-BIND-RETURN-ACTUAL mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_function_value_bounce_passthrough_bind_return_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "store_local g$ftag.2, tmp.") != NULL &&
        strstr(actual_text, "store_local p$ftag.3, tmp.") != NULL &&
        strstr(actual_text, "call __fnwrap_ping1(0)") != NULL &&
        strstr(actual_text, "call __fnwrap_ping2(0)") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-VOID-FNVAL-BOUNCE-PASSTHROUGH-BIND-RETURN mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_function_value_bounce_passthrough_bind_return_actual_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "store_local p$ftag.3, tmp.") != NULL &&
        strstr(actual_text, "call __fnwrap_ping1(0)") != NULL &&
        strstr(actual_text, "call __fnwrap_ping2(0)") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-VOID-FNVAL-BOUNCE-PASSTHROUGH-BIND-RETURN-ACTUAL mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_closure_bounce_passthrough_bind_return_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "store_local g$ftag.6, tmp.") != NULL &&
        strstr(actual_text, "store_local g$closurecap$0.7, tmp.") != NULL &&
        strstr(actual_text, "store_local p$ftag.8, tmp.") != NULL &&
        strstr(actual_text, "store_local p$closurecap$0.9, tmp.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-CLOSURE-BOUNCE-PASSTHROUGH-BIND-RETURN mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_closure_bounce_passthrough_bind_return_actual_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "store_local p$closurecap$0.9, tmp.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-CLOSURE-BOUNCE-PASSTHROUGH-BIND-RETURN-ACTUAL mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_closure_bounce_passthrough_bind_return_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "store_local g$ftag.6, tmp.") != NULL &&
        strstr(actual_text, "store_local g$closurecap$0.7, tmp.") != NULL &&
        strstr(actual_text, "store_local p$ftag.8, tmp.") != NULL &&
        strstr(actual_text, "store_local p$closurecap$0.9, tmp.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-VOID-CLOSURE-BOUNCE-PASSTHROUGH-BIND-RETURN mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_closure_bounce_passthrough_bind_return_actual_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "store_local p$closurecap$0.9, tmp.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-VOID-CLOSURE-BOUNCE-PASSTHROUGH-BIND-RETURN-ACTUAL mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_wrapped_returned_call_ternary_function_value_statement_reassign_bind_return_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "store_local g$ftag.2, tmp.") != NULL &&
        strstr(actual_text, "store_local p$ftag.3, tmp.") != NULL &&
        lower_ir_test_count_fragment_occurrences(actual_text, "store_local p$ftag.3, tmp.") >= 2u &&
        strstr(actual_text, "call __fnwrap_add1(0, 40)") != NULL &&
        strstr(actual_text, "call __fnwrap_add2(0, 40)") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-FNVAL-STMT-REASSIGN-BIND-RETURN mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_wrapped_returned_call_ternary_function_value_statement_reassign_bind_return_actual_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        lower_ir_test_count_fragment_occurrences(actual_text, "store_local p$ftag.3, tmp.") >= 2u &&
        strstr(actual_text, "call __fnwrap_add1(0, 40)") != NULL &&
        strstr(actual_text, "call __fnwrap_add2(0, 40)") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-FNVAL-STMT-REASSIGN-BIND-RETURN-ACTUAL mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_wrapped_returned_call_ternary_closure_statement_reassign_bind_return_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int pick(int c)(int){ int x=3; int y=5; int f(int)=closure [x] int (int z){ return x+z; }; int g(int)=closure [y] int (int z){ return y+z; }; return ((putint(0), c) ? f : g); }\n"
            "int wrap()(int){ int h(int)=(0, pick(1)); int g(int)=h; int p(int)=g; p=h; return p; }\n"
            "int main(){ return wrap()(4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call pick(") != NULL &&
        strstr(actual_text, "store_local g$ftag.6, tmp.") != NULL &&
        strstr(actual_text, "store_local g$closurecap$0.7, tmp.") != NULL &&
        lower_ir_test_count_fragment_occurrences(actual_text, "store_local p$ftag.8, tmp.") >= 2u &&
        lower_ir_test_count_fragment_occurrences(actual_text, "store_local p$closurecap$0.9, tmp.") >= 2u &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-CLOSURE-STMT-REASSIGN-BIND-RETURN mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_wrapped_returned_call_ternary_closure_statement_reassign_bind_return_actual_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        lower_ir_test_count_fragment_occurrences(actual_text, "store_local p$closurecap$0.9, tmp.") >= 2u &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-CLOSURE-STMT-REASSIGN-BIND-RETURN-ACTUAL mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_function_value_statement_reassign_bind_return_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "store_local g$ftag.2, tmp.") != NULL &&
        lower_ir_test_count_fragment_occurrences(actual_text, "store_local p$ftag.3, tmp.") >= 2u &&
        strstr(actual_text, "call __fnwrap_next1(0)") != NULL &&
        strstr(actual_text, "call __fnwrap_next2(0)") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-FNVAL-STMT-REASSIGN-BIND-RETURN mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_function_value_statement_reassign_bind_return_actual_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        lower_ir_test_count_fragment_occurrences(actual_text, "store_local p$ftag.3, tmp.") >= 2u &&
        strstr(actual_text, "call __fnwrap_next1(0)") != NULL &&
        strstr(actual_text, "call __fnwrap_next2(0)") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-FNVAL-STMT-REASSIGN-BIND-RETURN-ACTUAL mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_function_value_statement_reassign_bind_return_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "store_local g$ftag.2, tmp.") != NULL &&
        lower_ir_test_count_fragment_occurrences(actual_text, "store_local p$ftag.3, tmp.") >= 2u &&
        strstr(actual_text, "call __fnwrap_ping1(0)") != NULL &&
        strstr(actual_text, "call __fnwrap_ping2(0)") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-VOID-FNVAL-STMT-REASSIGN-BIND-RETURN mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_function_value_statement_reassign_bind_return_actual_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        lower_ir_test_count_fragment_occurrences(actual_text, "store_local p$ftag.3, tmp.") >= 2u &&
        strstr(actual_text, "call __fnwrap_ping1(0)") != NULL &&
        strstr(actual_text, "call __fnwrap_ping2(0)") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-VOID-FNVAL-STMT-REASSIGN-BIND-RETURN-ACTUAL mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_closure_statement_reassign_bind_return_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int pick(int c)(){ int x=3; int y=5; int f()=closure [x] int (){ return x; }; int g()=closure [y] int (){ return y; }; return ((putint(0), c) ? f : g); }\n"
            "int wrap()(){ int h()=(0, pick(1)); int g()=h; int p()=g; p=h; return p; }\n"
            "int main(){ return wrap()(); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call pick(") != NULL &&
        strstr(actual_text, "store_local g$ftag.6, tmp.") != NULL &&
        strstr(actual_text, "store_local g$closurecap$0.7, tmp.") != NULL &&
        lower_ir_test_count_fragment_occurrences(actual_text, "store_local p$ftag.8, tmp.") >= 2u &&
        lower_ir_test_count_fragment_occurrences(actual_text, "store_local p$closurecap$0.9, tmp.") >= 2u &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-CLOSURE-STMT-REASSIGN-BIND-RETURN mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_closure_statement_reassign_bind_return_actual_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        lower_ir_test_count_fragment_occurrences(actual_text, "store_local p$closurecap$0.9, tmp.") >= 2u &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-CLOSURE-STMT-REASSIGN-BIND-RETURN-ACTUAL mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_closure_statement_reassign_bind_return_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "void pick(int c)(){ int x=3; int y=5; void f()=closure [x] void (){ putint(x); return; }; void g()=closure [y] void (){ putint(y); return; }; return ((putint(0), c) ? f : g); }\n"
            "void wrap()(){ void h()=(0, pick(1)); void g()=h; void p()=g; p=h; return p; }\n"
            "int main(){ wrap()(); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call pick(") != NULL &&
        strstr(actual_text, "store_local g$ftag.6, tmp.") != NULL &&
        strstr(actual_text, "store_local g$closurecap$0.7, tmp.") != NULL &&
        lower_ir_test_count_fragment_occurrences(actual_text, "store_local p$ftag.8, tmp.") >= 2u &&
        lower_ir_test_count_fragment_occurrences(actual_text, "store_local p$closurecap$0.9, tmp.") >= 2u &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-VOID-CLOSURE-STMT-REASSIGN-BIND-RETURN mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_closure_statement_reassign_bind_return_actual_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        lower_ir_test_count_fragment_occurrences(actual_text, "store_local p$closurecap$0.9, tmp.") >= 2u &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-VOID-CLOSURE-STMT-REASSIGN-BIND-RETURN-ACTUAL mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_assignment_result_returned_closure_reassignment_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int make(int x)(int){ return closure [x] int (int y){ return x+y; }; }\n"
            "int main(){ int h(int)=make(3); int g(int)=make(5); h=(g=make(4)); return h(5); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call make(") != NULL &&
        strstr(actual_text, "store_local g$closurecap$0.2,") != NULL &&
        strstr(actual_text, "store_local h$closurecap$0.0,") != NULL &&
        strstr(actual_text, "store_local h$ftag.1,") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_make__retclosure_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-ASSIGNMENT-RESULT-RETURNED-CLOSURE-REASSIGN mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_lowers_dynamic_returned_closure_backed_function_value_parameter_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int id(int f(int))(int) { return f; }\n"
            "int main(){ int c=1; int x=3; int y=5; int f(int)=closure [x] int (int z) { return x + z; }; int g(int)=closure [y] int (int z) { return y + z; }; if(c) f=g; return id(f)(4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare id(__ret0.0, f.1)\n") != NULL &&
        strstr(actual_text, "store_local c.0, 1\n") != NULL &&
        strstr(actual_text, "store_local x.1, 3\n") != NULL &&
        strstr(actual_text, "store_local y.2, 5\n") != NULL &&
        strstr(actual_text, "store_local f$closurecap$0.3, ") != NULL &&
        strstr(actual_text, "store_local g$closurecap$0.5, ") != NULL &&
        strstr(actual_text, "store_local f$ftag.4, 2\n") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "call main__closure_g_131166(") != NULL &&
        strstr(actual_text, "func main__closure_g_131166(y.0, z.1) {\n") != NULL &&
        strstr(actual_text, "call id(") == NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL &&
        strstr(actual_text, "__retclosure_declslot") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-LOWER-DYNAMIC-RETURNED-CLOSURE-BACKED-FUNCTION-VALUE-PARAM-IMM mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_lowers_dynamic_returned_two_arg_closure_backed_function_value_parameter_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int id(int f(int,int))(int,int) { return f; }\n"
            "int main(){ int c=1; int x=3; int y=5; int f(int,int)=closure [x] int (int a, int b) { return x + a + b; }; int g(int,int)=closure [y] int (int a, int b) { return y + a + b; }; if(c) f=g; return id(f)(3, 2); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare id(__ret0.0, f.1)\n") != NULL &&
        strstr(actual_text, "store_local c.0, 1\n") != NULL &&
        strstr(actual_text, "store_local x.1, 3\n") != NULL &&
        strstr(actual_text, "store_local y.2, 5\n") != NULL &&
        strstr(actual_text, "store_local f$closurecap$0.3, ") != NULL &&
        strstr(actual_text, "store_local g$closurecap$0.5, ") != NULL &&
        strstr(actual_text, "store_local f$ftag.4, 2\n") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "call main__closure_g_131181(") != NULL &&
        strstr(actual_text, "func main__closure_g_131181(y.0, a.1, b.2) {\n") != NULL &&
        strstr(actual_text, "call id(") == NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL &&
        strstr(actual_text, "__retclosure_declslot") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-LOWER-DYNAMIC-RETURNED-TWO-ARG-CLOSURE-BACKED-FUNCTION-VALUE-PARAM-IMM mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_lowers_dynamic_returned_zero_arg_closure_backed_function_value_parameter_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int id(int f())() { return f; }\n"
            "int main(){ int c=1; int x=3; int y=5; int f()=closure [x] int () { return x; }; int g()=closure [y] int () { return y; }; if(c) f=g; return id(f)(); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare id(__ret0.0, f.1)\n") != NULL &&
        strstr(actual_text, "store_local c.0, 1\n") != NULL &&
        strstr(actual_text, "store_local f$closurecap$0.3, ") != NULL &&
        strstr(actual_text, "store_local g$closurecap$0.5, ") != NULL &&
        strstr(actual_text, "store_local f$ftag.4, 2\n") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "call main__closure_g_131154(") != NULL &&
        strstr(actual_text, "call id(") == NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL &&
        strstr(actual_text, "__retclosure_declslot") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-LOWER-DYNAMIC-RETURNED-ZERO-ARG-CLOSURE-BACKED-FUNCTION-VALUE-PARAM-IMM mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_lowers_dynamic_returned_zero_arg_void_closure_backed_function_value_parameter_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "void id(void f())() { return f; }\n"
            "int main(){ int c=1; int x=3; int y=5; void f()=closure [x] void () { putint(x); return; }; void g()=closure [y] void () { putint(y); return; }; if(c) f=g; id(f)(); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare id(__ret0.0, f.1)\n") != NULL &&
        strstr(actual_text, "store_local c.0, 1\n") != NULL &&
        strstr(actual_text, "store_local f$closurecap$0.3, ") != NULL &&
        strstr(actual_text, "store_local g$closurecap$0.5, ") != NULL &&
        strstr(actual_text, "store_local f$ftag.4, 2\n") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "call main__closure_g_131165(") != NULL &&
        strstr(actual_text, "call id(") == NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL &&
        strstr(actual_text, "__retclosure_declslot") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-LOWER-DYNAMIC-RETURNED-ZERO-ARG-VOID-CLOSURE-BACKED-FUNCTION-VALUE-PARAM-IMM mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_lowers_dynamic_returned_closure_backed_function_value_parameter_bind_and_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int id(int f(int))(int) { return f; }\n"
            "int main(){ int c=1; int x=3; int y=5; int f(int)=closure [x] int (int z) { return x + z; }; int g(int)=closure [y] int (int z) { return y + z; }; if(c) f=g; int h(int)=id(f); return h(4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare id(__ret0.0, f.1)\n") != NULL &&
        strstr(actual_text, "store_local c.0, 1\n") != NULL &&
        strstr(actual_text, "store_local f$closurecap$0.3, ") != NULL &&
        strstr(actual_text, "store_local g$closurecap$0.5, ") != NULL &&
        strstr(actual_text, "store_local f$ftag.4, 2\n") != NULL &&
        strstr(actual_text, "store_local h$ftag.7, ") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "call main__closure_g_131166(") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL &&
        strstr(actual_text, "func main__closure_f_131112(x.0, z.1) {\n") != NULL &&
        strstr(actual_text, "func main__closure_g_131166(y.0, z.1) {\n") != NULL &&
        strstr(actual_text, "__retclosure_declslot") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-LOWER-DYNAMIC-RETURNED-CLOSURE-BACKED-FUNCTION-VALUE-PARAM-BIND mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_ternary_closure_actual_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int apply(int f(int), int x){ return f(x); }\n"
            "int main(){ int c=1; int x=3; int y=5; int f(int)=closure [x] int (int z) { return x + z; }; int g(int)=closure [y] int (int z) { return y + z; }; return apply(((putint(0), c) ? f : g), 4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call putint(0)\n") != NULL &&
        strstr(actual_text, "store_local __ternary_fn_argtag.") != NULL &&
        strstr(actual_text, "store_local __ternary_fn_argcap_0.") != NULL &&
        strstr(actual_text, "call main__closure_f_") != NULL &&
        strstr(actual_text, "call main__closure_g_") != NULL &&
        strstr(actual_text, "func main__closure_f_") != NULL &&
        strstr(actual_text, "func main__closure_g_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-TERNARY-CLOSURE-ACTUAL-ARGUMENT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_comma_wrapped_closure_forward_into_function_value_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int apply(int f(int), int x){ return f(x); }\n"
            "int main(){ int y=3; int f(int)=closure [y] int (int z) { return y + z; }; return apply((f, f), 4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply(f.0, x.1)\n") != NULL &&
        strstr(actual_text, "store_local f$closurecap$0.1, tmp.") != NULL &&
        strstr(actual_text, "call main__closure_f_") != NULL &&
        strstr(actual_text, "func main__closure_f_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-COMMA-WRAPPED-CLOSURE-FORWARD mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_assignment_result_closure_forward_into_function_value_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int apply(int f(int), int x){ return f(x); }\n"
            "int main(){ int y=3; int f(int)=closure [y] int (int z) { return y + z; }; return apply((f = f), 4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply(f.0, x.1)\n") != NULL &&
        strstr(actual_text, "store_local f$closurecap$0.1, tmp.") != NULL &&
        strstr(actual_text, "store_local f$ftag.2, 1\n") != NULL &&
        strstr(actual_text, "call main__closure_f_") != NULL &&
        strstr(actual_text, "func main__closure_f_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-ASSIGNMENT-RESULT-CLOSURE-FORWARD mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_comma_wrapped_closure_forward_into_function_value_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;
    const char *scan = NULL;
    size_t putint_count = 0u;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "call putint(0)\n") != NULL &&
        putint_count == 1u &&
        strstr(actual_text, "call main__closure_f_") != NULL &&
        strstr(actual_text, "func main__closure_f_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-COMMA-WRAPPED-CLOSURE-FORWARD mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_assignment_result_closure_forward_into_function_value_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;
    const char *scan = NULL;
    size_t putint_count = 0u;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "call putint(0)\n") != NULL &&
        putint_count == 1u &&
        strstr(actual_text, "store_local h$ftag.7, 2\n") != NULL &&
        strstr(actual_text, "store_local f$ftag.4, tmp.") != NULL &&
        strstr(actual_text, "call main__closure_f_") != NULL &&
        strstr(actual_text, "call main__closure_g_") != NULL &&
        strstr(actual_text, "func main__closure_g_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-ASSIGNMENT-RESULT-CLOSURE-FORWARD mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_comma_wrapped_zero_arg_closure_forward_into_function_value_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;
    const char *scan = NULL;
    size_t wrapped_putint_count = 0u;

    if (!lower_extension_source_to_lower_ir_text(
            "int apply0(int f()){ return f(); }\n"
            "int main(){ int c=1; int x=7; int y=9; int f()=closure [x] int () { return x; }; int g()=closure [y] int () { return y; }; int h()=f; if((putint(0), c)) h=g; return apply0((0, h)); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    scan = actual_text;
    while (scan && (scan = strstr(scan, "call putint(0)\n")) != NULL) {
        ++wrapped_putint_count;
        scan += strlen("call putint(0)\n");
    }

    ok = actual_text &&
        wrapped_putint_count == 1u &&
        strstr(actual_text, "declare apply0(f.0)\n") != NULL &&
        strstr(actual_text, "store_local h$ftag.7, 2\n") != NULL &&
        strstr(actual_text, "call main__closure_f_") != NULL &&
        strstr(actual_text, "call main__closure_g_") != NULL &&
        strstr(actual_text, "func main__closure_g_") != NULL &&
        strstr(actual_text, "ret tmp.2\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-COMMA-WRAPPED-ZERO-CLOSURE-FORWARD mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_assignment_result_zero_arg_closure_forward_into_function_value_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;
    const char *scan = NULL;
    size_t wrapped_putint_count = 0u;

    if (!lower_extension_source_to_lower_ir_text(
            "int apply0(int f()){ return f(); }\n"
            "int main(){ int c=1; int x=7; int y=9; int f()=closure [x] int () { return x; }; int g()=closure [y] int () { return y; }; int h()=f; if((putint(0), c)) h=g; return apply0((f=h)); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    scan = actual_text;
    while (scan && (scan = strstr(scan, "call putint(0)\n")) != NULL) {
        ++wrapped_putint_count;
        scan += strlen("call putint(0)\n");
    }

    ok = actual_text &&
        wrapped_putint_count == 1u &&
        strstr(actual_text, "declare apply0(f.0)\n") != NULL &&
        strstr(actual_text, "store_local h$ftag.7, tmp.") != NULL &&
        strstr(actual_text, "store_local f$ftag.4, tmp.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "ret tmp.2\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-ASSIGNMENT-RESULT-ZERO-CLOSURE-FORWARD mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_comma_wrapped_zero_arg_void_closure_forward_into_function_value_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;
    const char *scan = NULL;
    size_t wrapped_putint_count = 0u;

    if (!lower_extension_source_to_lower_ir_text(
            "void apply0(void f()){ f(); }\n"
            "int main(){ int c=1; int x=7; int y=9; void f()=closure [x] void () { putint(x); return; }; void g()=closure [y] void () { putint(y); return; }; void h()=f; if((putint(0), c)) h=g; return (apply0((0, h)), 0); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    scan = actual_text;
    while (scan && (scan = strstr(scan, "call putint(0)\n")) != NULL) {
        ++wrapped_putint_count;
        scan += strlen("call putint(0)\n");
    }

    ok = actual_text &&
        wrapped_putint_count == 1u &&
        strstr(actual_text, "declare apply0(f.0)\n") != NULL &&
        strstr(actual_text, "store_local h$ftag.7, 2\n") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "ret 0\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-COMMA-WRAPPED-ZERO-VOID-CLOSURE-FORWARD mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_assignment_result_zero_arg_void_closure_forward_into_function_value_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;
    const char *scan = NULL;
    size_t wrapped_putint_count = 0u;

    if (!lower_extension_source_to_lower_ir_text(
            "void apply0(void f()){ f(); }\n"
            "int main(){ int c=1; int x=7; int y=9; void f()=closure [x] void () { putint(x); return; }; void g()=closure [y] void () { putint(y); return; }; void h()=f; if((putint(0), c)) h=g; return (apply0((f=h)), 0); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    scan = actual_text;
    while (scan && (scan = strstr(scan, "call putint(0)\n")) != NULL) {
        ++wrapped_putint_count;
        scan += strlen("call putint(0)\n");
    }

    ok = actual_text &&
        wrapped_putint_count == 1u &&
        strstr(actual_text, "declare apply0(f.0)\n") != NULL &&
        strstr(actual_text, "store_local f$closurecap$0.3, tmp.") != NULL &&
        strstr(actual_text, "store_local f$ftag.4, tmp.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "ret 0\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-ASSIGNMENT-RESULT-ZERO-VOID-CLOSURE-FORWARD mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_comma_wrapped_function_value_forwarding_into_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;
    const char *scan = NULL;
    size_t putint_count = 0u;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "tmp.1 = eq tmp.6, 1\n") != NULL &&
        strstr(actual_text, "call __fnwrap_add1(0, 40)\n") != NULL &&
        strstr(actual_text, "call __fnwrap_add2(0, 40)\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-COMMA-WRAPPED-FNVAL-FORWARD mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_assignment_result_function_value_forwarding_into_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;
    const char *scan = NULL;
    size_t putint_count = 0u;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "store_local h$ftag.2, tmp.") != NULL &&
        strstr(actual_text, "tmp.1 = eq tmp.") != NULL &&
        strstr(actual_text, "call __fnwrap_add1(0, 40)\n") != NULL &&
        strstr(actual_text, "call __fnwrap_add2(0, 40)\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-ASSIGNMENT-RESULT-FNVAL-FORWARD mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_comma_wrapped_zero_arg_function_value_forwarding_into_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;
    const char *scan = NULL;
    size_t putint_count = 0u;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "tmp.1 = eq tmp.6, 1\n") != NULL &&
        strstr(actual_text, "call __fnwrap_next1(0)\n") != NULL &&
        strstr(actual_text, "call __fnwrap_next2(0)\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-COMMA-WRAPPED-ZERO-FNVAL-FORWARD mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_assignment_result_zero_arg_function_value_forwarding_into_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;
    const char *scan = NULL;
    size_t putint_count = 0u;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "store_local h$ftag.2, tmp.") != NULL &&
        strstr(actual_text, "tmp.1 = eq tmp.") != NULL &&
        strstr(actual_text, "call __fnwrap_next1(0)\n") != NULL &&
        strstr(actual_text, "call __fnwrap_next2(0)\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-ASSIGNMENT-RESULT-ZERO-FNVAL-FORWARD mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_comma_wrapped_zero_arg_void_function_value_forwarding_into_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;
    const char *scan = NULL;
    size_t putint_count = 0u;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "call __fnwrap_ping1(0)\n") != NULL &&
        strstr(actual_text, "call __fnwrap_ping2(0)\n") != NULL &&
        strstr(actual_text, "ret 0\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-COMMA-WRAPPED-ZERO-VOID-FNVAL-FORWARD mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_assignment_result_zero_arg_void_function_value_forwarding_into_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;
    const char *scan = NULL;
    size_t putint_count = 0u;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "store_local h$ftag.2, tmp.6\n") != NULL &&
        strstr(actual_text, "call __fnwrap_ping1(0)\n") != NULL &&
        strstr(actual_text, "call __fnwrap_ping2(0)\n") != NULL &&
        strstr(actual_text, "ret 0\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-ASSIGNMENT-RESULT-ZERO-VOID-FNVAL-FORWARD mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_returned_call_ternary_function_value_assignment_then_direct_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int add2(int x){ return x+2; }\n"
            "int pick(int c)(int){ int f(int)=add1; int g(int)=add2; return c ? f : g; }\n"
            "int main(){ int c=getint(); int h(int)=add1; h=((putint(0), c) ? pick(0) : pick(1)); return h(40); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        strstr(actual_text, "store_local h$ftag.") != NULL &&
        strstr(actual_text, "eq tmp.") != NULL &&
        strstr(actual_text, "call __fnwrap_add1(0, 40)\n") != NULL &&
        strstr(actual_text, "call __fnwrap_add2(0, 40)\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-RETURNED-CALL-TERNARY-FNVAL-ASSIGN-DIRECT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_returned_call_ternary_closure_assignment_then_direct_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int pick(int x, int c)(int){ int f(int)=closure [x] int (int z){ return x+z; }; int g(int)=closure [x] int (int z){ return x-z; }; return c ? f : g; }\n"
            "int main(){ int c=getint(); int y=9; int h(int)=closure [y] int (int z){ return y+z; }; h=((putint(0), c) ? pick(5,0) : pick(7,1)); return h(4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        strstr(actual_text, "store_local h$ftag.") != NULL &&
        strstr(actual_text, "store_local h$closurecap$0.") != NULL &&
        strstr(actual_text, "eq tmp.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-RETURNED-CALL-TERNARY-CLOSURE-ASSIGN-DIRECT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_lowers_float_transport_signature_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "float g; float id(float x){ return x; } int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "global g.0:float\n") != NULL &&
        strstr(actual_text, "func id(x.0:float) {\n") != NULL &&
        strstr(actual_text, "    tmp.0 = load_local x.0\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-TRANSPORT-SIGNATURE mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_lowers_float_literal_transport_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "float id(float x){ return x; }\nint main(){ float x = 1.25; id(1.25); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func id(x.0:float) {\n") != NULL &&
        strstr(actual_text, "    tmp.0 = load_local x.0\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-LITERAL-TRANSPORT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_lowers_float_global_initializer_transport_from_identifier_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "float g = 1.25;\n"
            "float h = g;\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "global g.0:float\n") != NULL &&
        strstr(actual_text, "global h.1:float\n") != NULL &&
        strstr(actual_text, "    store_global g.0, 1067450368\n") != NULL &&
        strstr(actual_text, "    tmp.0 = load_global g.0\n") != NULL &&
        strstr(actual_text, "    store_global h.1, tmp.0\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-GLOBAL-IDENT-INIT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_lowers_float_global_initializer_transport_from_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "float g = 1.25;\n"
            "float id(float x){ return x; }\n"
            "float h = id(g);\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "global g.0:float\n") != NULL &&
        strstr(actual_text, "global h.1:float\n") != NULL &&
        strstr(actual_text, "func id(x.0:float) {\n") != NULL &&
        strstr(actual_text, "    tmp.1 = load_global g.0\n") != NULL &&
        strstr(actual_text, "    tmp.0 = call id(tmp.1)\n") != NULL &&
        strstr(actual_text, "    store_global h.1, tmp.0\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-GLOBAL-CALL-INIT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_lowers_float_return_transport_from_global_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "float g = 1.25;\n"
            "float get(){ return g; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "global g.0:float\n") != NULL &&
        strstr(actual_text, "func get() {\n") != NULL &&
        strstr(actual_text, "    tmp.0 = load_global g.0\n") != NULL &&
        strstr(actual_text, "    ret tmp.0\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-RETURN-GLOBAL mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_lowers_float_return_transport_from_global_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "float g = 1.25;\n"
            "float id(float x){ return x; }\n"
            "float get(){ return id(g); }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "global g.0:float\n") != NULL &&
        strstr(actual_text, "func id(x.0:float) {\n") != NULL &&
        strstr(actual_text, "func get() {\n") != NULL &&
        strstr(actual_text, "    tmp.1 = load_global g.0\n") != NULL &&
        strstr(actual_text, "    tmp.0 = call id(tmp.1)\n") != NULL &&
        strstr(actual_text, "    ret tmp.0\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-RETURN-GLOBAL-CALL mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_lowers_float_global_call_chain_transport_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "float g = 1.25;\n"
            "float id(float x){ return x; }\n"
            "float wrap(float x){ return id(x); }\n"
            "float getg(){ return wrap(g); }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "global g.0:float\n") != NULL &&
        strstr(actual_text, "func wrap(x.0:float) {\n") != NULL &&
        strstr(actual_text, "    tmp.1 = load_local x.0\n") != NULL &&
        strstr(actual_text, "    tmp.0 = call id(tmp.1)\n") != NULL &&
        strstr(actual_text, "func getg() {\n") != NULL &&
        strstr(actual_text, "    tmp.1 = load_global g.0\n") != NULL &&
        strstr(actual_text, "    tmp.0 = call wrap(tmp.1)\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-GLOBAL-CALL-CHAIN mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_lowers_float_local_call_chain_transport_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "float id(float x){ return x; }\n"
            "float wrap(float x){ return id(x); }\n"
            "float bounce(float x){ float y; y = x; return wrap(y); }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func wrap(x.0:float) {\n") != NULL &&
        strstr(actual_text, "    tmp.1 = load_local x.0\n") != NULL &&
        strstr(actual_text, "    tmp.0 = call id(tmp.1)\n") != NULL &&
        strstr(actual_text, "func bounce(x.0:float) {\n") != NULL &&
        strstr(actual_text, "    tmp.1 = load_local x.0\n") != NULL &&
        strstr(actual_text, "    store_local y.1, tmp.1\n") != NULL &&
        strstr(actual_text, "    tmp.2 = load_local y.1\n") != NULL &&
        strstr(actual_text, "    tmp.0 = call wrap(tmp.2)\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-LOCAL-CALL-CHAIN mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_rejects_global_float_operator_expression_under_extension(void) {
    TokenArray tokens;
    AstProgram ast_program;
    ParserError parse_err;
    SemanticError sema_err;
    SemanticOptions sema_options;
    int ok = 0;

    lexer_init_tokens(&tokens);
    ast_program_init(&ast_program);
    memset(&parse_err, 0, sizeof(parse_err));
    memset(&sema_err, 0, sizeof(sema_err));
    memset(&sema_options, 0, sizeof(sema_options));
    sema_options.allow_extension_features = 1;

    ok = lexer_tokenize("float g = 1.25;\nint main(){ return g + 1; }\n", &tokens) &&
        parser_parse_translation_unit_ast(&tokens, &ast_program, &parse_err) &&
        !semantic_analyze_program_with_options(&ast_program, &sema_options, &sema_err) &&
        strstr(sema_err.message, "SEMA-EXT-035") != NULL;

    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-GLOBAL-OP-SEMANTIC-REJECT mismatch: parse='%s' sema='%s'\n",
            parse_err.message,
            sema_err.message);
    }

    ast_program_free(&ast_program);
    lexer_free_tokens(&tokens);
    return ok;
}

static int test_lower_ir_rejects_global_float_operator_expression_in_initializer_under_extension(void) {
    TokenArray tokens;
    AstProgram ast_program;
    ParserError parse_err;
    SemanticError sema_err;
    SemanticOptions sema_options;
    int ok = 0;

    lexer_init_tokens(&tokens);
    ast_program_init(&ast_program);
    memset(&parse_err, 0, sizeof(parse_err));
    memset(&sema_err, 0, sizeof(sema_err));
    memset(&sema_options, 0, sizeof(sema_options));
    sema_options.allow_extension_features = 1;

    ok = lexer_tokenize("float g = 1.25;\nint h = g + 1;\nint main(){ return 0; }\n", &tokens) &&
        parser_parse_translation_unit_ast(&tokens, &ast_program, &parse_err) &&
        !semantic_analyze_program_with_options(&ast_program, &sema_options, &sema_err) &&
        strstr(sema_err.message, "SEMA-EXT-035") != NULL;

    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-GLOBAL-OP-INIT-SEMANTIC-REJECT mismatch: parse='%s' sema='%s'\n",
            parse_err.message,
            sema_err.message);
    }

    ast_program_free(&ast_program);
    lexer_free_tokens(&tokens);
    return ok;
}

static int test_lower_ir_rejects_global_float_call_result_in_initializer_under_extension(void) {
    TokenArray tokens;
    AstProgram ast_program;
    ParserError parse_err;
    SemanticError sema_err;
    SemanticOptions sema_options;
    int ok = 0;

    lexer_init_tokens(&tokens);
    ast_program_init(&ast_program);
    memset(&parse_err, 0, sizeof(parse_err));
    memset(&sema_err, 0, sizeof(sema_err));
    memset(&sema_options, 0, sizeof(sema_options));
    sema_options.allow_extension_features = 1;

    ok = lexer_tokenize(
            "float g = 1.25;\nfloat id(float x){ return x; }\nint h = id(g);\nint main(){ return 0; }\n",
            &tokens) &&
        parser_parse_translation_unit_ast(&tokens, &ast_program, &parse_err) &&
        !semantic_analyze_program_with_options(&ast_program, &sema_options, &sema_err) &&
        strstr(sema_err.message, "SEMA-TYPE-004") != NULL;

    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-GLOBAL-CALL-INIT-TYPE-REJECT mismatch: parse='%s' sema='%s'\n",
            parse_err.message,
            sema_err.message);
    }

    ast_program_free(&ast_program);
    lexer_free_tokens(&tokens);
    return ok;
}

static int test_lower_ir_lowers_float_assignment_transport_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "float g = 1.25;\n"
            "float id(float x){ return x; }\n"
            "int main(){ float y; y = id(g); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "global g.0:float\n") != NULL &&
        strstr(actual_text, "func __global.init() {\n") != NULL &&
        strstr(actual_text, "    store_global g.0, 1067450368\n") != NULL &&
        strstr(actual_text, "    tmp.2 = load_global g.0\n") != NULL &&
        strstr(actual_text, "    tmp.0 = call id(tmp.2)\n") != NULL &&
        strstr(actual_text, "    store_local y.0, tmp.0\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-ASSIGN-TRANSPORT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_rejects_float_assignment_to_int_under_extension(void) {
    TokenArray tokens;
    AstProgram ast_program;
    ParserError parse_err;
    SemanticError sema_err;
    SemanticOptions sema_options;
    int ok = 0;

    lexer_init_tokens(&tokens);
    ast_program_init(&ast_program);
    memset(&parse_err, 0, sizeof(parse_err));
    memset(&sema_err, 0, sizeof(sema_err));
    memset(&sema_options, 0, sizeof(sema_options));
    sema_options.allow_extension_features = 1;

    ok = lexer_tokenize("float g = 1.25;\nint main(){ int x = 0; x = g; return 0; }\n", &tokens) &&
        parser_parse_translation_unit_ast(&tokens, &ast_program, &parse_err) &&
        !semantic_analyze_program_with_options(&ast_program, &sema_options, &sema_err) &&
        strstr(sema_err.message, "SEMA-TYPE-006") != NULL;

    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-ASSIGN-TYPE-REJECT mismatch: parse='%s' sema='%s'\n",
            parse_err.message,
            sema_err.message);
    }

    ast_program_free(&ast_program);
    lexer_free_tokens(&tokens);
    return ok;
}

static int test_lower_ir_rejects_float_ternary_value_return_to_int_under_extension(void) {
    TokenArray tokens;
    AstProgram ast_program;
    ParserError parse_err;
    SemanticError sema_err;
    SemanticOptions sema_options;
    int ok = 0;

    lexer_init_tokens(&tokens);
    ast_program_init(&ast_program);
    memset(&parse_err, 0, sizeof(parse_err));
    memset(&sema_err, 0, sizeof(sema_err));
    memset(&sema_options, 0, sizeof(sema_options));
    sema_options.allow_extension_features = 1;

    ok = lexer_tokenize(
            "float g = 1.25;\n"
            "float h = 2.5;\n"
            "int bad(){ return g ? h : h; }\n"
            "int main(){ return 0; }\n",
            &tokens) &&
        parser_parse_translation_unit_ast(&tokens, &ast_program, &parse_err) &&
        !semantic_analyze_program_with_options(&ast_program, &sema_options, &sema_err) &&
        strstr(sema_err.message, "SEMA-TYPE-005") != NULL;

    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-TERNARY-VALUE-RETURN-INT-REJECT mismatch: parse='%s' sema='%s'\n",
            parse_err.message,
            sema_err.message);
    }

    ast_program_free(&ast_program);
    lexer_free_tokens(&tokens);
    return ok;
}

static int test_lower_ir_rejects_unary_call_ternary_value_return_to_int_under_extension(void) {
    TokenArray tokens;
    AstProgram ast_program;
    ParserError parse_err;
    SemanticError sema_err;
    SemanticOptions sema_options;
    int ok = 0;

    lexer_init_tokens(&tokens);
    ast_program_init(&ast_program);
    memset(&parse_err, 0, sizeof(parse_err));
    memset(&sema_err, 0, sizeof(sema_err));
    memset(&sema_options, 0, sizeof(sema_options));
    sema_options.allow_extension_features = 1;

    ok = lexer_tokenize(
            "float id(float x){ return x; }\n"
            "int bad(){ return -id(1.0) ? 1.0 : 2.0; }\n"
            "int main(){ return 0; }\n",
            &tokens) &&
        parser_parse_translation_unit_ast(&tokens, &ast_program, &parse_err) &&
        !semantic_analyze_program_with_options(&ast_program, &sema_options, &sema_err) &&
        strstr(sema_err.message, "SEMA-TYPE-005") != NULL;

    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-UNARY-CALL-TERNARY-RETURN-INT-REJECT mismatch: parse='%s' sema='%s'\n",
            parse_err.message,
            sema_err.message);
    }

    ast_program_free(&ast_program);
    lexer_free_tokens(&tokens);
    return ok;
}

static int test_lower_ir_rejects_float_if_condition_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "float g = 1.25;\nint main(){ if(g) return 1; return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "tmp.3 = load_global g.0\n") != NULL &&
        strstr(actual_text, "tmp.0 = and tmp.3, 2147483647\n") != NULL &&
        strstr(actual_text, "tmp.1 = ne tmp.0, 0\n") != NULL &&
        strstr(actual_text, "br tmp.1, bb.1, bb.2\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-IF-COND-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_rejects_float_while_condition_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "float g = 1.25;\nint main(){ while(g) return 1; return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "jmp bb.1\n") != NULL &&
        strstr(actual_text, "tmp.3 = load_global g.0\n") != NULL &&
        strstr(actual_text, "tmp.0 = and tmp.3, 2147483647\n") != NULL &&
        strstr(actual_text, "tmp.1 = ne tmp.0, 0\n") != NULL &&
        strstr(actual_text, "br tmp.1, bb.2, bb.3\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-WHILE-COND-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_rejects_float_for_condition_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "float g = 1.25;\nint main(){ for(;g;) return 1; return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "jmp bb.1\n") != NULL &&
        strstr(actual_text, "tmp.3 = load_global g.0\n") != NULL &&
        strstr(actual_text, "tmp.0 = and tmp.3, 2147483647\n") != NULL &&
        strstr(actual_text, "tmp.1 = ne tmp.0, 0\n") != NULL &&
        strstr(actual_text, "br tmp.1, bb.2, bb.3\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-FOR-COND-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_float_logical_condition_composition_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "float g = 1.25;\n"
            "int main(){ if(!g || (g && 1.25)) return g ? 1 : 0; return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "load_global g.0") != NULL &&
        strstr(actual_text, "2147483647") != NULL &&
        strstr(actual_text, "br tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-LOGICAL-COND-COMPOSE-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_recursive_float_if_condition_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int f(float x, float y, float z){ if((x + y) + z) return 1; return 0; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func f(x.0:float, y.1:float, z.2:float) {\n") != NULL &&
        strstr(actual_text, "call __builtin_fadd32(") != NULL &&
        strstr(actual_text, "and tmp.") != NULL &&
        strstr(actual_text, "2147483647\n") != NULL &&
        strstr(actual_text, "br tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-RECURSIVE-IF-COND-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_recursive_float_while_condition_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int f(float a, float b, float c){ while(-a * (b / c)) return 1; return 0; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func f(a.0:float, b.1:float, c.2:float) {\n") != NULL &&
        strstr(actual_text, "__builtin_fdiv32") != NULL &&
        strstr(actual_text, "__builtin_fmul32") != NULL &&
        strstr(actual_text, "2147483647\n") != NULL &&
        strstr(actual_text, "br tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-RECURSIVE-WHILE-COND-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_recursive_float_for_condition_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int f(float x, float y, float z){ for(;(x + y) + z;) return 1; return 0; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func f(x.0:float, y.1:float, z.2:float) {\n") != NULL &&
        strstr(actual_text, "jmp bb.1\n") != NULL &&
        strstr(actual_text, "call __builtin_fadd32(") != NULL &&
        strstr(actual_text, "2147483647\n") != NULL &&
        strstr(actual_text, "br tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-RECURSIVE-FOR-COND-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_rejects_recursive_float_condition_with_ternary_neighbor_under_extension(void) {
    TokenArray tokens;
    AstProgram ast_program;
    ParserError parse_err;
    SemanticError sema_err;
    SemanticOptions semantic_options;
    int ok = 0;

    lexer_init_tokens(&tokens);
    ast_program_init(&ast_program);
    memset(&parse_err, 0, sizeof(parse_err));
    memset(&sema_err, 0, sizeof(sema_err));
    memset(&semantic_options, 0, sizeof(semantic_options));
    semantic_options.allow_extension_features = 1;
    semantic_options.skip_all_paths_return_check = 1;

    if (lexer_tokenize(
            "float g = 1.25;\n"
            "float h = 2.5;\n"
            "int main(){ if((g ? h : h) + h) return 1; return 0; }\n",
            &tokens) &&
        parser_parse_translation_unit_ast(&tokens, &ast_program, &parse_err) &&
        !semantic_analyze_program_with_options(&ast_program, &semantic_options, &sema_err) &&
        strstr(sema_err.message, "SEMA-EXT-035") != NULL) {
        ok = 1;
    } else {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-RECURSIVE-COND-TERNARY-NEIGHBOR-REJECT mismatch: parse='%s' sema='%s'\n",
            parse_err.message,
            sema_err.message);
    }

    ast_program_free(&ast_program);
    lexer_free_tokens(&tokens);
    return ok;
}

static int test_lower_ir_rejects_float_ternary_value_under_extension(void) {
    TokenArray tokens;
    AstProgram ast_program;
    ParserError parse_err;
    SemanticError sema_err;
    SemanticOptions sema_options;
    int ok = 0;

    lexer_init_tokens(&tokens);
    ast_program_init(&ast_program);
    memset(&parse_err, 0, sizeof(parse_err));
    memset(&sema_err, 0, sizeof(sema_err));
    memset(&sema_options, 0, sizeof(sema_options));
    sema_options.allow_extension_features = 1;

    ok = lexer_tokenize(
            "float g = 1.25;\nfloat h = 1.25;\nint main(){ return g ? h : 0; }\n",
            &tokens) &&
        parser_parse_translation_unit_ast(&tokens, &ast_program, &parse_err) &&
        !semantic_analyze_program_with_options(&ast_program, &sema_options, &sema_err) &&
        strstr(sema_err.message, "SEMA-EXT-035") != NULL;

    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-TERNARY-VALUE-SEMANTIC-REJECT mismatch: parse='%s' sema='%s'\n",
            parse_err.message,
            sema_err.message);
    }

    ast_program_free(&ast_program);
    lexer_free_tokens(&tokens);
    return ok;
}

static int test_lower_ir_accepts_same_type_float_ternary_value_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "ret tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-TERNARY-VALUE-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_float_ternary_value_assignment_to_float_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "store_local y.0") != NULL &&
        strstr(actual_text, "ret 0\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-TERNARY-VALUE-ASSIGN-FLOAT-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_float_ternary_value_initializer_to_float_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "store_global y.2") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-TERNARY-VALUE-INIT-FLOAT-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_chained_float_addition_assignment_to_float_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "float f(float x, float y, float z){ float t; t = (x + y) + z; return t; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func f(x.0:float, y.1:float, z.2:float) {\n") != NULL &&
        strstr(actual_text, "call __builtin_fadd32(") != NULL &&
        strstr(actual_text, "store_local t.3") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-CHAIN-ADD-ASSIGN-FLOAT-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_nested_float_mul_div_assignment_to_float_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "float f(float a, float b, float c){ float t; t = -a * (b / c); return t; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func f(a.0:float, b.1:float, c.2:float) {\n") != NULL &&
        strstr(actual_text, "__builtin_fdiv32") != NULL &&
        strstr(actual_text, "__builtin_fmul32") != NULL &&
        strstr(actual_text, "store_local t.3") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-NESTED-MUL-DIV-ASSIGN-FLOAT-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_chained_float_addition_initializer_to_float_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "float f(float x, float y, float z){ float t = (x + y) + z; return t; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func f(x.0:float, y.1:float, z.2:float) {\n") != NULL &&
        strstr(actual_text, "call __builtin_fadd32(") != NULL &&
        strstr(actual_text, "store_local t.3") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-CHAIN-ADD-INIT-FLOAT-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_nested_float_mul_div_initializer_to_float_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "float f(float a, float b, float c){ float t = -a * (b / c); return t; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func f(a.0:float, b.1:float, c.2:float) {\n") != NULL &&
        strstr(actual_text, "__builtin_fdiv32") != NULL &&
        strstr(actual_text, "__builtin_fmul32") != NULL &&
        strstr(actual_text, "store_local t.3") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-NESTED-MUL-DIV-INIT-FLOAT-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_float_ternary_value_call_argument_to_float_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-TERNARY-VALUE-CALLARG-FLOAT-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_unary_call_float_ternary_value_call_argument_to_float_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-UNARY-CALL-TERNARY-CALLARG-FLOAT-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_chained_float_addition_call_argument_to_float_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "call wrap(tmp.") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-CHAIN-ADD-CALLARG-FLOAT-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_nested_float_mul_div_call_argument_to_float_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "__builtin_fdiv32") != NULL &&
        strstr(actual_text, "__builtin_fmul32") != NULL &&
        strstr(actual_text, "call wrap(tmp.") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-NESTED-MUL-DIV-CALLARG-FLOAT-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_float_helper_wrapped_ternary_call_arithmetic_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "tmp.2 = load_global h.1\n") != NULL &&
        strstr(actual_text, "tmp.1 = call __builtin_fadd32(tmp.0, tmp.2)\n") != NULL &&
        strstr(actual_text, "ret tmp.1\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-HELPER-TERNARY-CALL-ARITH-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_unary_call_helper_wrapped_ternary_call_arithmetic_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "tmp.0 = call pick(tmp.2)\n") != NULL &&
        strstr(actual_text, "tmp.1 = call __builtin_fadd32(tmp.0, tmp.3)\n") != NULL &&
        strstr(actual_text, "ret tmp.1\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-UNARY-HELPER-TERNARY-CALL-ARITH-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_noncapturing_function_value_return_bind_and_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int add2(int x){ return x+2; }\n"
            "int pick(int c)(int) { int f(int)=add1; if(c) f=add2; return f; }\n"
            "int main(){ int g(int)=pick(1); return g(40); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func pick(__ret0.0, c.1) {\n") != NULL &&
        strstr(actual_text, "store_local f$ftag.2, 1\n") != NULL &&
        strstr(actual_text, "store_local f$ftag.2, 2\n") != NULL &&
        strstr(actual_text, "store_indirect tmp.1, tmp.2\n") != NULL &&
        strstr(actual_text, "tmp.0 = addr_local g$ftag.0\n") != NULL &&
        strstr(actual_text, "call pick(tmp.0, 1)\n") != NULL &&
        strstr(actual_text, "load_local g$ftag.0\n") != NULL &&
        strstr(actual_text, "eq tmp.") != NULL &&
        strstr(actual_text, ", 1\n") != NULL &&
        strstr(actual_text, "call __fnwrap_add1(0, 40)\n") != NULL &&
        strstr(actual_text, "call __fnwrap_add2(0, 40)\n") != NULL &&
        strstr(actual_text, "func __fnwrap_add1(__env.0, x.1) {\n") != NULL &&
        strstr(actual_text, "func __fnwrap_add2(__env.0, x.1) {\n") != NULL &&
        strstr(actual_text, "__retfn_declslot") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-FNVAL-RETURN-BIND mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_static_toplevel_noncapturing_function_value_return_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int pick()(int) { return add1; }\n"
            "int main(){ int f(int)=pick(); return f(41); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func pick(__ret0.0) {\n") != NULL &&
        strstr(actual_text, "tmp.0 = load_local __ret0.0\n") != NULL &&
        strstr(actual_text, "store_indirect tmp.0, 1\n") != NULL &&
        strstr(actual_text, "tmp.0 = addr_local f$ftag.0\n") != NULL &&
        strstr(actual_text, "call pick(tmp.0)\n") != NULL &&
        strstr(actual_text, "tmp.2 = mov 0\n") != NULL &&
        strstr(actual_text, "call __fnwrap_add1(0, 41)\n") != NULL &&
        strstr(actual_text, "func __fnwrap_add1(__env.0, x.1) {\n") != NULL &&
        strstr(actual_text, "__retfn_declslot") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-STATIC-TOPLEVEL-FNVAL-RETURN mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_noncapturing_function_value_return_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int add2(int x){ return x+2; }\n"
            "int pick(int c)(int) { int f(int)=add1; if(c) f=add2; return f; }\n"
            "int main(){ return pick(1)(40); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func pick(__ret0.0, c.1) {\n") != NULL &&
        strstr(actual_text, "store_indirect tmp.1, tmp.2\n") != NULL &&
        strstr(actual_text, "tmp.0 = addr_local __retfn_immediate_0.0\n") != NULL &&
        strstr(actual_text, "call pick(tmp.0, 1)\n") != NULL &&
        strstr(actual_text, "tmp.2 = eq tmp.6, 1\n") != NULL &&
        strstr(actual_text, "br tmp.2, bb.1, bb.2\n") != NULL &&
        strstr(actual_text, "tmp.4 = mov 0\n") != NULL &&
        strstr(actual_text, "call __fnwrap_add2(0, 40)\n") != NULL &&
        strstr(actual_text, "call __fnwrap_add1(0, 40)\n") != NULL &&
        strstr(actual_text, "func __fnwrap_add1(__env.0, x.1) {\n") != NULL &&
        strstr(actual_text, "func __fnwrap_add2(__env.0, x.1) {\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-FNVAL-RETURN-IMMEDIATE mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_noncapturing_two_arg_function_value_return_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int add(int x,int y){ return x+y; }\n"
            "int sub(int x,int y){ return x-y; }\n"
            "int pick(int c)(int,int) { int f(int,int)=add; if(c) f=sub; return f; }\n"
            "int main(){ return pick(1)(9, 4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func pick(__ret0.0, c.1) {\n") != NULL &&
        strstr(actual_text, "tmp.0 = addr_local __retfn_immediate_0.0\n") != NULL &&
        strstr(actual_text, "call pick(tmp.0, 1)\n") != NULL &&
        strstr(actual_text, "tmp.2 = eq tmp.6, 1\n") != NULL &&
        strstr(actual_text, "call __fnwrap_add(0, 9, 4)\n") != NULL &&
        strstr(actual_text, "call __fnwrap_sub(0, 9, 4)\n") != NULL &&
        strstr(actual_text, "func __fnwrap_add(__env.0, x.1, y.2) {\n") != NULL &&
        strstr(actual_text, "func __fnwrap_sub(__env.0, x.1, y.2) {\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-TWO-ARG-FNVAL-RETURN-IMMEDIATE mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_wrapped_function_value_return_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int add2(int x){ return x+2; }\n"
            "int pick(int c)(int){ int f(int)=add1; int h(int)=add1; if(c) f=add2; return (h=f); }\n"
            "int main(){ return pick(1)(40); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "store_local h$ftag.3, 1\n") != NULL &&
        strstr(actual_text, "store_local f$ftag.2, 2\n") != NULL &&
        strstr(actual_text, "tmp.1 = load_local f$ftag.2\n") != NULL &&
        strstr(actual_text, "store_local h$ftag.3, tmp.1\n") != NULL &&
        strstr(actual_text, "tmp.2 = load_local __ret0.0\n") != NULL &&
        strstr(actual_text, "tmp.3 = load_local h$ftag.3\n") != NULL &&
        strstr(actual_text, "store_indirect tmp.2, tmp.3\n") != NULL &&
        strstr(actual_text, "call __fnwrap_add1(0, 40)\n") != NULL &&
        strstr(actual_text, "call __fnwrap_add2(0, 40)\n") != NULL &&
        strstr(actual_text, "func __fnwrap_add1(__env.0, x.1) {\n") != NULL &&
        strstr(actual_text, "func __fnwrap_add2(__env.0, x.1) {\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-WRAPPED-FNVAL-RETURN mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_returned_closure_assignment_then_return_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int make(int x)(int) { return closure [x] int (int y) { return x + y; }; }\n"
            "int pick()(int){ int f(int)=make(3); int g(int)=make(5); g=f; return g; }\n"
            "int main(){ return pick()(4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func pick(__ret0.0) {\n") != NULL &&
        strstr(actual_text, "addr_local f$closurecap$0.") != NULL &&
        strstr(actual_text, "call make(tmp.2, 5)\n") != NULL &&
        strstr(actual_text, "load_local f$closurecap$0.") != NULL &&
        strstr(actual_text, "store_local g$closurecap$0.") != NULL &&
        strstr(actual_text, "store_local g$ftag.") != NULL &&
        strstr(actual_text, "load_local __ret0.0\n") != NULL &&
        strstr(actual_text, "load_local g$closurecap$0.") != NULL &&
        strstr(actual_text, "store_indirect tmp.") != NULL &&
        strstr(actual_text, "tmp.0 = addr_local __retclosure_immediate_0.0\n") != NULL &&
        strstr(actual_text, "call pick(tmp.0)\n") != NULL &&
        strstr(actual_text, "call make__retclosure_") != NULL &&
        strstr(actual_text, "__retclosure_declslot") == NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-RETURNED-CLOSURE-ASSIGN-RETURN mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_returned_closure_rebind_then_return_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int pick(int x, int c)(int) { int f(int)=closure [x] int (int y) { return x + y; }; int g(int)=closure [x] int (int y) { return x - y; }; if(c) f=g; return f; }\n"
            "int wrap(int c)(int) { int h(int)=pick(5, c); return h; }\n"
            "int main(){ return wrap(1)(3); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func wrap(__ret0.0, __ret1.1, c.2) {\n") != NULL &&
        strstr(actual_text, "store_local h$ftag.3, tmp.") != NULL &&
        strstr(actual_text, "store_local h$closurecap$0.4, tmp.") != NULL &&
        strstr(actual_text, "call pick(") != NULL &&
        strstr(actual_text, "store_indirect tmp.6, tmp.7\n") != NULL &&
        strstr(actual_text, "store_indirect tmp.8, tmp.9\n") != NULL &&
        strstr(actual_text, "call wrap(") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_65567_1") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_65621_1") != NULL &&
        strstr(actual_text, "__retclosure_declslot_0.") != NULL &&
        strstr(actual_text, "__retclosure_declslot_1.") != NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-RETURNED-CLOSURE-REBIND-RETURN mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_returned_multi_capture_closure_rebind_then_return_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int pick(int x, int y, int c)(int,int) { int f(int,int)=closure [x,y] int (int a,int b){ return x+y+a+b; }; int g(int,int)=closure [x,y] int (int a,int b){ return x+y-a-b; }; if(c) f=g; return f; }\n"
            "int wrap(int c)(int,int) { int h(int,int)=pick(5,7,c); return h; }\n"
            "int main(){ return wrap(1)(3,2); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func wrap(__ret0.0, __ret1.1, __ret2.2, c.3) {\n") != NULL &&
        strstr(actual_text, "store_local h$ftag.4, tmp.") != NULL &&
        strstr(actual_text, "store_local h$closurecap$0.5, tmp.") != NULL &&
        strstr(actual_text, "store_local h$closurecap$1.6, tmp.") != NULL &&
        strstr(actual_text, "call pick(") != NULL &&
        strstr(actual_text, "call wrap(") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_65578_2") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_65645_2") != NULL &&
        strstr(actual_text, "__retclosure_declslot_0.") != NULL &&
        strstr(actual_text, "__retclosure_declslot_1.") != NULL &&
        strstr(actual_text, "__retclosure_declslot_2.") != NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-RETURNED-MULTI-CAPTURE-CLOSURE-REBIND-RETURN mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_returned_zero_arg_void_closure_rebind_then_return_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "void pick(int x, int c)() { void f()=closure [x] void (){ putint(x); return; }; void g()=closure [x] void (){ putint(x+1); return; }; if(c) f=g; return f; }\n"
            "void wrap(int c)() { void h()=pick(5,c); return h; }\n"
            "int main(){ wrap(1)(); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func wrap(__ret0.0, __ret1.1, c.2) {\n") != NULL &&
        strstr(actual_text, "store_local h$ftag.3, tmp.") != NULL &&
        strstr(actual_text, "store_local h$closurecap$0.4, tmp.") != NULL &&
        strstr(actual_text, "call pick(") != NULL &&
        strstr(actual_text, "call wrap(") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_65565_1") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_65617_1") != NULL &&
        strstr(actual_text, "__retclosure_declslot_0.") != NULL &&
        strstr(actual_text, "__retclosure_declslot_1.") != NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-RETURNED-ZERO-ARG-VOID-CLOSURE-REBIND-RETURN mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_returned_closure_passthrough_bind_then_return_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int id(int f(int))(int) { return f; }\n"
            "int pick(int x, int c)(int) { int f(int)=closure [x] int (int y){ return x+y; }; int g(int)=closure [x] int (int y){ return x-y; }; if(c) f=g; return f; }\n"
            "int wrap(int c)(int) { int h(int)=id(pick(5,c)); return h; }\n"
            "int main(){ return wrap(1)(3); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare id(__ret0.0, f.1)\n") != NULL &&
        strstr(actual_text, "func wrap(__ret0.0, __ret1.1, c.2) {\n") != NULL &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call pick(") == 1u &&
        strstr(actual_text, "store_local h$ftag.3, tmp.") != NULL &&
        strstr(actual_text, "store_local h$closurecap$0.4, tmp.") != NULL &&
        strstr(actual_text, "call wrap(") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call id(") == NULL &&
        strstr(actual_text, "__retclosure_declslot_0.") != NULL &&
        strstr(actual_text, "__retclosure_declslot_1.") != NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-RETURNED-CLOSURE-PASSTHROUGH-BIND-RETURN mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_returned_closure_two_hop_passthrough_bind_then_return_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        lower_ir_test_count_fragment_occurrences(actual_text, "call pick(") == 1u &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "store_local h$ftag.3, tmp.") != NULL &&
        strstr(actual_text, "store_local h$closurecap$0.4, tmp.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call id(") == NULL &&
        strstr(actual_text, "call id2(") == NULL &&
        strstr(actual_text, "__retclosure_declslot_0.") != NULL &&
        strstr(actual_text, "__retclosure_declslot_1.") != NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-RETURNED-CLOSURE-TWO-HOP-PASSTHROUGH-BIND-RETURN mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_returned_closure_two_hop_passthrough_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        lower_ir_test_count_fragment_occurrences(actual_text, "call pick(") == 1u &&
        strstr(actual_text, "store_local __closure_env_0.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call id(") == NULL &&
        strstr(actual_text, "call id2(") == NULL &&
        strstr(actual_text, "__retclosure_declslot") == NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-RETURNED-CLOSURE-TWO-HOP-PASSTHROUGH-IMMEDIATE mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_returned_closure_three_hop_passthrough_bind_then_return_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        lower_ir_test_count_fragment_occurrences(actual_text, "call pick(") == 1u &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "store_local h$ftag.3, tmp.") != NULL &&
        strstr(actual_text, "store_local h$closurecap$0.4, tmp.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call id(") == NULL &&
        strstr(actual_text, "call id2(") == NULL &&
        strstr(actual_text, "call id3(") == NULL &&
        strstr(actual_text, "__retclosure_declslot_0.") != NULL &&
        strstr(actual_text, "__retclosure_declslot_1.") != NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-RETURNED-CLOSURE-THREE-HOP-PASSTHROUGH-BIND-RETURN mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_returned_closure_three_hop_passthrough_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        lower_ir_test_count_fragment_occurrences(actual_text, "call pick(") == 1u &&
        strstr(actual_text, "store_local __closure_env_0.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call id(") == NULL &&
        strstr(actual_text, "call id2(") == NULL &&
        strstr(actual_text, "call id3(") == NULL &&
        strstr(actual_text, "__retclosure_declslot") == NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-RETURNED-CLOSURE-THREE-HOP-PASSTHROUGH-IMMEDIATE mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_returned_closure_three_hop_passthrough_actual_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        lower_ir_test_count_fragment_occurrences(actual_text, "call pick(") == 1u &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call apply(") == NULL &&
        strstr(actual_text, "call id(") == NULL &&
        strstr(actual_text, "call id2(") == NULL &&
        strstr(actual_text, "call id3(") == NULL &&
        strstr(actual_text, "__retclosure_declslot") == NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-RETURNED-CLOSURE-THREE-HOP-PASSTHROUGH-ACTUAL mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_returned_closure_local_bounce_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int pick(int x, int c)(int) { int f(int)=closure [x] int (int y){ return x+y; }; int g(int)=closure [x] int (int y){ return x-y; }; if(c) f=g; return f; }\n"
            "int wrap(int c)(int) { int h(int)=pick(5,c); int k(int)=h; return k; }\n"
            "int main(){ return wrap(1)(3); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func wrap(__ret0.0, __ret1.1, c.2) {\n") != NULL &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call pick(") == 1u &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "store_local k$ftag.7, tmp.") != NULL &&
        strstr(actual_text, "store_local k$closurecap$0.8, tmp.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "__retclosure_declslot_0.") != NULL &&
        strstr(actual_text, "__retclosure_declslot_1.") != NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-RETURNED-CLOSURE-LOCAL-BOUNCE-IMMEDIATE mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_returned_closure_local_bounce_actual_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        lower_ir_test_count_fragment_occurrences(actual_text, "call pick(") == 1u &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call apply(") == NULL &&
        strstr(actual_text, "__retclosure_declslot_0.") != NULL &&
        strstr(actual_text, "__retclosure_declslot_1.") != NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-RETURNED-CLOSURE-LOCAL-BOUNCE-ACTUAL mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_returned_closure_producer_ternary_merge_bind_and_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int pick(int x, int c)(int) { int f(int)=closure [x] int (int y) { return x + y; }; int g(int)=closure [x] int (int y) { return x - y; }; if(c) f=g; return f; }\n"
            "int wrap(int c, int d)(int) { int h(int)=pick(5, c); int k(int)=pick(7, c); int m(int)=d ? h : k; return m; }\n"
            "int main(){ int f(int)=wrap(1,0); return f(3); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func wrap(__ret0.0, __ret1.1, c.2, d.3) {\n") != NULL &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "store_local m$ftag.") != NULL &&
        strstr(actual_text, "store_local m$closurecap$0.") != NULL &&
        strstr(actual_text, "store_indirect tmp.18, tmp.19\n") != NULL &&
        strstr(actual_text, "store_indirect tmp.20, tmp.21\n") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL &&
        lower_ir_test_count_fragment_occurrences(actual_text, "__retclosure_declslot_0.") >= 2u &&
        lower_ir_test_count_fragment_occurrences(actual_text, "__retclosure_declslot_1.") >= 2u &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-RETURNED-CLOSURE-PRODUCER-TERNARY-MERGE-BIND mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_returned_closure_producer_ternary_merge_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int pick(int x, int c)(int) { int f(int)=closure [x] int (int y) { return x + y; }; int g(int)=closure [x] int (int y) { return x - y; }; if(c) f=g; return f; }\n"
            "int wrap(int c, int d)(int) { int h(int)=pick(5, c); int k(int)=pick(7, c); int m(int)=d ? h : k; return m; }\n"
            "int main(){ return wrap(1,0)(3); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func wrap(__ret0.0, __ret1.1, c.2, d.3) {\n") != NULL &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "store_local m$ftag.") != NULL &&
        strstr(actual_text, "store_local m$closurecap$0.") != NULL &&
        strstr(actual_text, "__retclosure_immediate_0.") != NULL &&
        strstr(actual_text, "__retclosure_immediate_1.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL &&
        lower_ir_test_count_fragment_occurrences(actual_text, "__retclosure_declslot_0.") >= 2u &&
        lower_ir_test_count_fragment_occurrences(actual_text, "__retclosure_declslot_1.") >= 2u &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-RETURNED-CLOSURE-PRODUCER-TERNARY-MERGE-IMMEDIATE mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_returned_closure_returned_call_ternary_merge_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int id(int f(int))(int){ return f; }\n"
            "int pick(int x, int c)(int){ int f(int)=closure [x] int (int y){ return x+y; }; int g(int)=closure [x] int (int y){ return x-y; }; if(c) f=g; return f; }\n"
            "int main(){ int c=1; int h(int)= c ? id(pick(5,1)) : id(pick(7,0)); return h(3); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare id(__ret0.0, f.1)\n") != NULL &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        strstr(actual_text, "store_local h$ftag.") != NULL &&
        strstr(actual_text, "store_local h$closurecap$0.") != NULL &&
        strstr(actual_text, "__retclosure_ternaryslot_0.") != NULL &&
        strstr(actual_text, "__retclosure_ternaryslot_1.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call id(") == NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-RETURNED-CLOSURE-RETURNED-CALL-TERNARY-MERGE-IMMEDIATE mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_returned_closure_returned_call_ternary_merge_local_bounce_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int id(int f(int))(int){ return f; }\n"
            "int pick(int x, int c)(int){ int f(int)=closure [x] int (int y){ return x+y; }; int g(int)=closure [x] int (int y){ return x-y; }; if(c) f=g; return f; }\n"
            "int main(){ int c=1; int h(int)= c ? id(pick(5,1)) : id(pick(7,0)); int g(int)=h; return g(3); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare id(__ret0.0, f.1)\n") != NULL &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        strstr(actual_text, "store_local h$ftag.") != NULL &&
        strstr(actual_text, "store_local h$closurecap$0.") != NULL &&
        strstr(actual_text, "store_local g$ftag.") != NULL &&
        strstr(actual_text, "store_local g$closurecap$0.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call id(") == NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-RETURNED-CLOSURE-RETURNED-CALL-TERNARY-MERGE-LOCAL-BOUNCE-IMMEDIATE mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_returned_closure_returned_call_ternary_merge_bind_return_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        lower_ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "store_local h$ftag.") != NULL &&
        strstr(actual_text, "store_local h$closurecap$0.") != NULL &&
        strstr(actual_text, "__retclosure_immediate_0.") != NULL &&
        strstr(actual_text, "__retclosure_immediate_1.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call id(") == NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-RETURNED-CLOSURE-RETURNED-CALL-TERNARY-MERGE-BIND-RETURN-IMMEDIATE mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_returned_closure_returned_call_ternary_merge_bind_return_actual_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        lower_ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "__retclosure_argslot_0.") != NULL &&
        strstr(actual_text, "__retclosure_argslot_1.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call apply(") == NULL &&
        strstr(actual_text, "call id(") == NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-RETURNED-CLOSURE-RETURNED-CALL-TERNARY-MERGE-BIND-RETURN-ACTUAL mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_returned_closure_returned_call_ternary_merge_bounce_passthrough_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int id(int f(int))(int){ return f; }\n"
            "int pick(int x, int c)(int){ int f(int)=closure [x] int (int y){ return x+y; }; int g(int)=closure [x] int (int y){ return x-y; }; if(c) f=g; return f; }\n"
            "int main(){ int c=1; int h(int)= c ? id(pick(5,1)) : id(pick(7,0)); int g(int)=h; int p(int)=id(g); return p(3); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare id(__ret0.0, f.1)\n") != NULL &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        strstr(actual_text, "store_local g$ftag.") != NULL &&
        strstr(actual_text, "store_local g$closurecap$0.") != NULL &&
        strstr(actual_text, "store_local p$ftag.") != NULL &&
        strstr(actual_text, "store_local p$closurecap$0.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call id(") == NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-RETURNED-CLOSURE-RETURNED-CALL-TERNARY-MERGE-BOUNCE-PASSTHROUGH-IMMEDIATE mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_returned_closure_returned_call_ternary_merge_bounce_passthrough_bind_return_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        lower_ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "store_local g$ftag.") != NULL &&
        strstr(actual_text, "store_local g$closurecap$0.") != NULL &&
        strstr(actual_text, "store_local p$ftag.") != NULL &&
        strstr(actual_text, "store_local p$closurecap$0.") != NULL &&
        strstr(actual_text, "__retclosure_immediate_0.") != NULL &&
        strstr(actual_text, "__retclosure_immediate_1.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call id(") == NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-RETURNED-CLOSURE-RETURNED-CALL-TERNARY-MERGE-BOUNCE-PASSTHROUGH-BIND-RETURN-IMMEDIATE mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_returned_closure_returned_call_ternary_merge_bounce_passthrough_bind_return_actual_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        lower_ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "__retclosure_argslot_0.") != NULL &&
        strstr(actual_text, "__retclosure_argslot_1.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call apply(") == NULL &&
        strstr(actual_text, "call id(") == NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-RETURNED-CLOSURE-RETURNED-CALL-TERNARY-MERGE-BOUNCE-PASSTHROUGH-BIND-RETURN-ACTUAL mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_returned_closure_returned_call_ternary_merge_deeper_chain_bind_return_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        lower_ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "store_local q$ftag.") != NULL &&
        strstr(actual_text, "store_local q$closurecap$0.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call id(") == NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-RETURNED-CLOSURE-RETURNED-CALL-TERNARY-MERGE-DEEPER-CHAIN-BIND-RETURN-IMMEDIATE mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_returned_closure_producer_ternary_merge_actual_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int apply(int f(int), int x){ return f(x); }\n"
            "int pick(int x, int c)(int) { int f(int)=closure [x] int (int y) { return x + y; }; int g(int)=closure [x] int (int y) { return x - y; }; if(c) f=g; return f; }\n"
            "int wrap(int c, int d)(int) { int h(int)=pick(5, c); int k(int)=pick(7, c); int m(int)=d ? h : k; return m; }\n"
            "int main(){ return apply(wrap(1,0), 3); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply(f.0, x.1)\n") != NULL &&
        strstr(actual_text, "func wrap(__ret0.0, __ret1.1, c.2, d.3) {\n") != NULL &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "store_local m$ftag.") != NULL &&
        strstr(actual_text, "store_local m$closurecap$0.") != NULL &&
        strstr(actual_text, "__retclosure_argslot_0.") != NULL &&
        strstr(actual_text, "__retclosure_argslot_1.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call apply(") == NULL &&
        lower_ir_test_count_fragment_occurrences(actual_text, "__retclosure_declslot_0.") >= 2u &&
        lower_ir_test_count_fragment_occurrences(actual_text, "__retclosure_declslot_1.") >= 2u &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-RETURNED-CLOSURE-PRODUCER-TERNARY-MERGE-ACTUAL mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_returned_closure_producer_ternary_merge_local_bounce_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int pick(int x, int c)(int) { int f(int)=closure [x] int (int y) { return x + y; }; int g(int)=closure [x] int (int y) { return x - y; }; if(c) f=g; return f; }\n"
            "int wrap(int c, int d)(int) { int h(int)=pick(5,c); int k(int)=pick(7,c); int m(int)=d ? h : k; int n(int)=m; return n; }\n"
            "int main(){ return wrap(1,0)(3); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func wrap(__ret0.0, __ret1.1, c.2, d.3) {\n") != NULL &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "store_local m$ftag.") != NULL &&
        strstr(actual_text, "store_local m$closurecap$0.") != NULL &&
        strstr(actual_text, "store_local n$ftag.") != NULL &&
        strstr(actual_text, "store_local n$closurecap$0.") != NULL &&
        strstr(actual_text, "__retclosure_immediate_0.") != NULL &&
        strstr(actual_text, "__retclosure_immediate_1.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-RETURNED-CLOSURE-PRODUCER-TERNARY-MERGE-BOUNCE-IMMEDIATE mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_returned_closure_producer_ternary_merge_local_bounce_actual_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        lower_ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "store_local n$ftag.") != NULL &&
        strstr(actual_text, "store_local n$closurecap$0.") != NULL &&
        strstr(actual_text, "__retclosure_argslot_0.") != NULL &&
        strstr(actual_text, "__retclosure_argslot_1.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call apply(") == NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-RETURNED-CLOSURE-PRODUCER-TERNARY-MERGE-BOUNCE-ACTUAL mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_returned_closure_producer_ternary_merge_local_bounce_passthrough_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        lower_ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "store_local n$ftag.") != NULL &&
        strstr(actual_text, "store_local n$closurecap$0.") != NULL &&
        strstr(actual_text, "store_local p$ftag.") != NULL &&
        strstr(actual_text, "store_local p$closurecap$0.") != NULL &&
        strstr(actual_text, "__retclosure_immediate_0.") != NULL &&
        strstr(actual_text, "__retclosure_immediate_1.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-RETURNED-CLOSURE-PRODUCER-TERNARY-MERGE-BOUNCE-PASSTHROUGH-IMMEDIATE mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_returned_closure_producer_ternary_merge_local_bounce_passthrough_actual_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        lower_ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "store_local p$ftag.") != NULL &&
        strstr(actual_text, "store_local p$closurecap$0.") != NULL &&
        strstr(actual_text, "__retclosure_argslot_0.") != NULL &&
        strstr(actual_text, "__retclosure_argslot_1.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call apply(") == NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-RETURNED-CLOSURE-PRODUCER-TERNARY-MERGE-BOUNCE-PASSTHROUGH-ACTUAL mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_returned_closure_producer_ternary_merge_local_reassign_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int pick(int x, int c)(int) { int f(int)=closure [x] int (int y){ return x+y; }; int g(int)=closure [x] int (int y){ return x-y; }; if(c) f=g; return f; }\n"
            "int wrap(int c, int d)(int) { int h(int)=pick(5,c); int k(int)=pick(7,c); int m(int)=d ? h : k; int n(int)=m; int p(int)=n; p=m; return p; }\n"
            "int main(){ return wrap(1,0)(3); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func wrap(__ret0.0, __ret1.1, c.2, d.3) {\n") != NULL &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "store_local n$ftag.") != NULL &&
        strstr(actual_text, "store_local n$closurecap$0.") != NULL &&
        strstr(actual_text, "store_local p$ftag.") != NULL &&
        strstr(actual_text, "store_local p$closurecap$0.") != NULL &&
        strstr(actual_text, "__retclosure_immediate_0.") != NULL &&
        strstr(actual_text, "__retclosure_immediate_1.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-RETURNED-CLOSURE-PRODUCER-TERNARY-MERGE-REASSIGN-IMMEDIATE mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_returned_closure_producer_ternary_merge_local_reassign_actual_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        lower_ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "store_local p$ftag.") != NULL &&
        strstr(actual_text, "store_local p$closurecap$0.") != NULL &&
        strstr(actual_text, "__retclosure_argslot_0.") != NULL &&
        strstr(actual_text, "__retclosure_argslot_1.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call apply(") == NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-RETURNED-CLOSURE-PRODUCER-TERNARY-MERGE-REASSIGN-ACTUAL mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_returned_closure_producer_ternary_merge_returned_call_reassign_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        lower_ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "store_local m$closurecap$0.") != NULL &&
        strstr(actual_text, "store_local m$ftag.") != NULL &&
        strstr(actual_text, "__retclosure_immediate_0.") != NULL &&
        strstr(actual_text, "__retclosure_immediate_1.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "id__fv_0_") == NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-RETURNED-CLOSURE-PRODUCER-TERNARY-MERGE-RETURNED-CALL-REASSIGN-IMMEDIATE mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_returned_closure_producer_ternary_merge_returned_call_reassign_actual_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        lower_ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "store_local m$closurecap$0.") != NULL &&
        strstr(actual_text, "store_local m$ftag.") != NULL &&
        strstr(actual_text, "__retclosure_argslot_0.") != NULL &&
        strstr(actual_text, "__retclosure_argslot_1.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call apply(") == NULL &&
        strstr(actual_text, "id__fv_0_") == NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-RETURNED-CLOSURE-PRODUCER-TERNARY-MERGE-RETURNED-CALL-REASSIGN-ACTUAL mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int pick(int x, int c)() { int f()=closure [x] int () { return x; }; int g()=closure [x] int () { return x + 1; }; if(c) f=g; return f; }\n"
            "int wrap(int c, int d)() { int h()=pick(5,c); int k()=pick(7,c); int m()=d ? h : k; return m; }\n"
            "int main(){ return wrap(1,0)(); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func wrap(__ret0.0, __ret1.1, c.2, d.3) {\n") != NULL &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "store_local m$ftag.") != NULL &&
        strstr(actual_text, "store_local m$closurecap$0.") != NULL &&
        strstr(actual_text, "__retclosure_immediate_0.") != NULL &&
        strstr(actual_text, "__retclosure_immediate_1.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-RETURNED-ZERO-ARG-CLOSURE-PRODUCER-TERNARY-MERGE-IMMEDIATE mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_actual_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int apply0(int f()){ return f(); }\n"
            "int pick(int x, int c)() { int f()=closure [x] int () { return x; }; int g()=closure [x] int () { return x + 1; }; if(c) f=g; return f; }\n"
            "int wrap(int c, int d)() { int h()=pick(5,c); int k()=pick(7,c); int m()=d ? h : k; return m; }\n"
            "int main(){ return apply0(wrap(1,0)); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply0(f.0)\n") != NULL &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "__retclosure_argslot_0.") != NULL &&
        strstr(actual_text, "__retclosure_argslot_1.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call apply0(") == NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-RETURNED-ZERO-ARG-CLOSURE-PRODUCER-TERNARY-MERGE-ACTUAL mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "void pick(int x, int c)() { void f()=closure [x] void () { putint(x); return; }; void g()=closure [x] void () { putint(x + 1); return; }; if(c) f=g; return f; }\n"
            "void wrap(int c, int d)() { void h()=pick(5,c); void k()=pick(7,c); void m()=d ? h : k; return m; }\n"
            "int main(){ wrap(1,0)(); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare putint(param0.0)\n") != NULL &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "__retclosure_immediate_0.") != NULL &&
        strstr(actual_text, "__retclosure_immediate_1.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-RETURNED-ZERO-ARG-VOID-CLOSURE-PRODUCER-TERNARY-MERGE-IMMEDIATE mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_actual_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "void apply0(void f()){ f(); }\n"
            "void pick(int x, int c)() { void f()=closure [x] void () { putint(x); return; }; void g()=closure [x] void () { putint(x + 1); return; }; if(c) f=g; return f; }\n"
            "void wrap(int c, int d)() { void h()=pick(5,c); void k()=pick(7,c); void m()=d ? h : k; return m; }\n"
            "int main(){ apply0(wrap(1,0)); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply0(f.0)\n") != NULL &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "__retclosure_argslot_0.") != NULL &&
        strstr(actual_text, "__retclosure_argslot_1.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call apply0(") == NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-RETURNED-ZERO-ARG-VOID-CLOSURE-PRODUCER-TERNARY-MERGE-ACTUAL mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_local_bounce_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int pick(int x, int c)() { int f()=closure [x] int () { return x; }; int g()=closure [x] int () { return x + 1; }; if(c) f=g; return f; }\n"
            "int wrap(int c, int d)() { int h()=pick(5,c); int k()=pick(7,c); int m()=d ? h : k; int n()=m; return n; }\n"
            "int main(){ return wrap(1,0)(); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "store_local n$ftag.") != NULL &&
        strstr(actual_text, "store_local n$closurecap$0.") != NULL &&
        strstr(actual_text, "__retclosure_immediate_0.") != NULL &&
        strstr(actual_text, "__retclosure_immediate_1.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-RETURNED-ZERO-ARG-CLOSURE-PRODUCER-TERNARY-MERGE-BOUNCE-IMMEDIATE mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_local_bounce_actual_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        lower_ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "store_local n$ftag.") != NULL &&
        strstr(actual_text, "store_local n$closurecap$0.") != NULL &&
        strstr(actual_text, "__retclosure_argslot_0.") != NULL &&
        strstr(actual_text, "__retclosure_argslot_1.") != NULL &&
        strstr(actual_text, "call apply0(") == NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-RETURNED-ZERO-ARG-CLOSURE-PRODUCER-TERNARY-MERGE-BOUNCE-ACTUAL mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_local_bounce_passthrough_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        lower_ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "store_local p$ftag.") != NULL &&
        strstr(actual_text, "store_local p$closurecap$0.") != NULL &&
        strstr(actual_text, "call id0(") == NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "__retclosure_immediate_0.") != NULL &&
        strstr(actual_text, "__retclosure_immediate_1.") != NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-RETURNED-ZERO-ARG-CLOSURE-PRODUCER-TERNARY-MERGE-BOUNCE-PASSTHROUGH-IMMEDIATE mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_local_bounce_passthrough_actual_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        lower_ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "store_local p$ftag.") != NULL &&
        strstr(actual_text, "store_local p$closurecap$0.") != NULL &&
        strstr(actual_text, "call apply0(") == NULL &&
        strstr(actual_text, "call id0(") == NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "__retclosure_argslot_0.") != NULL &&
        strstr(actual_text, "__retclosure_argslot_1.") != NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-RETURNED-ZERO-ARG-CLOSURE-PRODUCER-TERNARY-MERGE-BOUNCE-PASSTHROUGH-ACTUAL mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_local_reassign_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int pick(int x, int c)() { int f()=closure [x] int () { return x; }; int g()=closure [x] int () { return x + 1; }; if(c) f=g; return f; }\n"
            "int wrap(int c, int d)() { int h()=pick(5,c); int k()=pick(7,c); int m()=d ? h : k; int n()=m; int p()=n; p=m; return p; }\n"
            "int main(){ return wrap(1,0)(); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "store_local p$ftag.") != NULL &&
        strstr(actual_text, "store_local p$closurecap$0.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "__retclosure_immediate_0.") != NULL &&
        strstr(actual_text, "__retclosure_immediate_1.") != NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-RETURNED-ZERO-ARG-CLOSURE-PRODUCER-TERNARY-MERGE-REASSIGN-IMMEDIATE mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_local_reassign_actual_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        lower_ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "store_local p$ftag.") != NULL &&
        strstr(actual_text, "store_local p$closurecap$0.") != NULL &&
        strstr(actual_text, "call apply0(") == NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "__retclosure_argslot_0.") != NULL &&
        strstr(actual_text, "__retclosure_argslot_1.") != NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-RETURNED-ZERO-ARG-CLOSURE-PRODUCER-TERNARY-MERGE-REASSIGN-ACTUAL mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_local_bounce_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "void pick(int x, int c)() { void f()=closure [x] void () { putint(x); return; }; void g()=closure [x] void () { putint(x + 1); return; }; if(c) f=g; return f; }\n"
            "void wrap(int c, int d)() { void h()=pick(5,c); void k()=pick(7,c); void m()=d ? h : k; void n()=m; return n; }\n"
            "int main(){ wrap(1,0)(); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "store_local n$ftag.") != NULL &&
        strstr(actual_text, "store_local n$closurecap$0.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "__retclosure_immediate_0.") != NULL &&
        strstr(actual_text, "__retclosure_immediate_1.") != NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-RETURNED-ZERO-ARG-VOID-CLOSURE-PRODUCER-TERNARY-MERGE-BOUNCE-IMMEDIATE mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_local_bounce_actual_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        lower_ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "store_local n$ftag.") != NULL &&
        strstr(actual_text, "store_local n$closurecap$0.") != NULL &&
        strstr(actual_text, "call apply0(") == NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "__retclosure_argslot_0.") != NULL &&
        strstr(actual_text, "__retclosure_argslot_1.") != NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-RETURNED-ZERO-ARG-VOID-CLOSURE-PRODUCER-TERNARY-MERGE-BOUNCE-ACTUAL mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_local_bounce_passthrough_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        lower_ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "store_local p$ftag.") != NULL &&
        strstr(actual_text, "store_local p$closurecap$0.") != NULL &&
        strstr(actual_text, "call idv(") == NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "__retclosure_immediate_0.") != NULL &&
        strstr(actual_text, "__retclosure_immediate_1.") != NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-RETURNED-ZERO-ARG-VOID-CLOSURE-PRODUCER-TERNARY-MERGE-BOUNCE-PASSTHROUGH-IMMEDIATE mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_local_bounce_passthrough_actual_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        lower_ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "store_local p$ftag.") != NULL &&
        strstr(actual_text, "store_local p$closurecap$0.") != NULL &&
        strstr(actual_text, "call apply0(") == NULL &&
        strstr(actual_text, "call idv(") == NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "__retclosure_argslot_0.") != NULL &&
        strstr(actual_text, "__retclosure_argslot_1.") != NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-RETURNED-ZERO-ARG-VOID-CLOSURE-PRODUCER-TERNARY-MERGE-BOUNCE-PASSTHROUGH-ACTUAL mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_local_reassign_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "void pick(int x, int c)() { void f()=closure [x] void () { putint(x); return; }; void g()=closure [x] void () { putint(x + 1); return; }; if(c) f=g; return f; }\n"
            "void wrap(int c, int d)() { void h()=pick(5,c); void k()=pick(7,c); void m()=d ? h : k; void n()=m; void p()=n; p=m; return p; }\n"
            "int main(){ wrap(1,0)(); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "store_local p$ftag.") != NULL &&
        strstr(actual_text, "store_local p$closurecap$0.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "__retclosure_immediate_0.") != NULL &&
        strstr(actual_text, "__retclosure_immediate_1.") != NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-RETURNED-ZERO-ARG-VOID-CLOSURE-PRODUCER-TERNARY-MERGE-REASSIGN-IMMEDIATE mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_local_reassign_actual_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        lower_ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "store_local p$ftag.") != NULL &&
        strstr(actual_text, "store_local p$closurecap$0.") != NULL &&
        strstr(actual_text, "call apply0(") == NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "__retclosure_argslot_0.") != NULL &&
        strstr(actual_text, "__retclosure_argslot_1.") != NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-RETURNED-ZERO-ARG-VOID-CLOSURE-PRODUCER-TERNARY-MERGE-REASSIGN-ACTUAL mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_returned_call_reassign_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        lower_ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "store_local m$closurecap$0.") != NULL &&
        strstr(actual_text, "store_local m$ftag.") != NULL &&
        strstr(actual_text, "id0__fv_0_") == NULL &&
        strstr(actual_text, "__retclosure_immediate_0.") != NULL &&
        strstr(actual_text, "__retclosure_immediate_1.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-RETURNED-ZERO-ARG-CLOSURE-PRODUCER-TERNARY-MERGE-RETURNED-CALL-REASSIGN-IMMEDIATE mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_returned_call_reassign_actual_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        lower_ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "store_local m$closurecap$0.") != NULL &&
        strstr(actual_text, "store_local m$ftag.") != NULL &&
        strstr(actual_text, "call apply0(") == NULL &&
        strstr(actual_text, "id0__fv_0_") == NULL &&
        strstr(actual_text, "__retclosure_argslot_0.") != NULL &&
        strstr(actual_text, "__retclosure_argslot_1.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-RETURNED-ZERO-ARG-CLOSURE-PRODUCER-TERNARY-MERGE-RETURNED-CALL-REASSIGN-ACTUAL mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_returned_call_reassign_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        lower_ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "store_local m$closurecap$0.") != NULL &&
        strstr(actual_text, "store_local m$ftag.") != NULL &&
        strstr(actual_text, "idv__fv_0_") == NULL &&
        strstr(actual_text, "__retclosure_immediate_0.") != NULL &&
        strstr(actual_text, "__retclosure_immediate_1.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-RETURNED-ZERO-ARG-VOID-CLOSURE-PRODUCER-TERNARY-MERGE-RETURNED-CALL-REASSIGN-IMMEDIATE mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_returned_call_reassign_actual_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        lower_ir_test_count_fragment_occurrences(actual_text, "call pick(") == 2u &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call wrap(") == 1u &&
        strstr(actual_text, "store_local m$closurecap$0.") != NULL &&
        strstr(actual_text, "store_local m$ftag.") != NULL &&
        strstr(actual_text, "call apply0(") == NULL &&
        strstr(actual_text, "idv__fv_0_") == NULL &&
        strstr(actual_text, "__retclosure_argslot_0.") != NULL &&
        strstr(actual_text, "__retclosure_argslot_1.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-RETURNED-ZERO-ARG-VOID-CLOSURE-PRODUCER-TERNARY-MERGE-RETURNED-CALL-REASSIGN-ACTUAL mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_returned_zero_arg_closure_statement_passthrough_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int id0(int f())(){ return f; }\n"
            "int pick(int x, int c)() { int f()=closure [x] int () { return x; }; int g()=closure [x] int () { return x + 1; }; if(c) f=g; return f; }\n"
            "int main(){ int m()=pick(5,1); id0(m); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare id0(__ret0.0, f.1)\n") != NULL &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call pick(") == 1u &&
        strstr(actual_text, "store_local m$ftag.") != NULL &&
        strstr(actual_text, "store_local m$closurecap$0.") != NULL &&
        strstr(actual_text, "call id0(") == NULL &&
        strstr(actual_text, "id0__fv_0_") == NULL &&
        strstr(actual_text, "call_indirect") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-RETURNED-ZERO-ARG-CLOSURE-STATEMENT-PASSTHROUGH-CALL mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_returned_zero_arg_void_closure_statement_passthrough_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "void idv(void f())(){ return f; }\n"
            "void pick(int x, int c)() { void f()=closure [x] void () { putint(x); return; }; void g()=closure [x] void () { putint(x + 1); return; }; if(c) f=g; return f; }\n"
            "int main(){ void m()=pick(5,1); idv(m); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare idv(__ret0.0, f.1)\n") != NULL &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call pick(") == 1u &&
        strstr(actual_text, "store_local m$ftag.") != NULL &&
        strstr(actual_text, "store_local m$closurecap$0.") != NULL &&
        strstr(actual_text, "call idv(") == NULL &&
        strstr(actual_text, "idv__fv_0_") == NULL &&
        strstr(actual_text, "call_indirect") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-RETURNED-ZERO-ARG-VOID-CLOSURE-STATEMENT-PASSTHROUGH-CALL mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_returned_closure_statement_passthrough_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int id(int f(int))(int){ return f; }\n"
            "int pick(int x, int c)(int){ int f(int)=closure [x] int (int y){ return x+y; }; int g(int)=closure [x] int (int y){ return x-y; }; if(c) f=g; return f; }\n"
            "int main(){ int m(int)=pick(5,1); id(m); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare id(__ret0.0, f.1)\n") != NULL &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call pick(") == 1u &&
        strstr(actual_text, "store_local m$ftag.0,") != NULL &&
        strstr(actual_text, "store_local m$closurecap$0.1,") != NULL &&
        strstr(actual_text, "call id(") == NULL &&
        strstr(actual_text, "id__fv_0_") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-RETURNED-CLOSURE-STATEMENT-PASSTHROUGH-CALL mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_static_function_value_capture_inside_closure_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int main(){ int f(int)=add1; int g(int)=closure [f] int (int y){ return f(y); }; return g(4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "store_local g$closurecap$0.") != NULL &&
        strstr(actual_text, "call main__closure_g_") != NULL &&
        strstr(actual_text, "__fv_0_add1(4)\n") != NULL &&
        strstr(actual_text, "call __fnwrap_add1(0, tmp.") != NULL &&
        strstr(actual_text, "call_indirect") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR static function-value capture inside closure mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_function_parameter_capture_inside_closure_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int wrap(int f(int), int x){ int g(int)=closure [f] int (int y){ return f(y); }; return g(x); }\n"
            "int add1(int x){ return x+1; }\n"
            "int main(){ return wrap(add1, 4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "load_local f.0") != NULL &&
        strstr(actual_text, "store_local g$closurecap$0.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_wrap__closure_g_") != NULL &&
        strstr(actual_text, "call wrap__closure_g_") != NULL &&
        strstr(actual_text, "call wrap__fv_0_add1(4)\n") != NULL &&
        strstr(actual_text, "call __fnwrap_add1(0, tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR function-parameter capture inside closure mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_passthrough_ternary_closure_local_initializer_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int id(int f(int))(int){ return f; }\n"
            "int main(){ int c=1; int x=3; int y=5; int f(int)=closure [x] int (int z){ return x+z; }; int g(int)=closure [y] int (int z){ return y+z; }; int h(int)= c ? id(f) : id(g); return h(4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare id(__ret0.0, f.1)\n") != NULL &&
        strstr(actual_text, "store_local h$ftag.") != NULL &&
        strstr(actual_text, "store_local h$closurecap$0.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "call id(") == NULL &&
        strstr(actual_text, "id__fv_0_") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-PASSTHROUGH-TERNARY-CLOSURE-LOCAL-INIT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_passthrough_ternary_noncapturing_function_value_return_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        lower_ir_test_count_fragment_occurrences(actual_text, "call pick(") == 1u &&
        strstr(actual_text, "call __fnwrap_add1(") != NULL &&
        strstr(actual_text, "call __fnwrap_add2(") != NULL &&
        strstr(actual_text, "call id(") == NULL &&
        strstr(actual_text, "id__fv_0_") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-PASSTHROUGH-TERNARY-NONCAPTURING-RETURN-IMMEDIATE mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_passthrough_ternary_noncapturing_function_value_actual_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "store_local __ternary_fn_argtag.") != NULL &&
        strstr(actual_text, "call __fnwrap_add1(") != NULL &&
        strstr(actual_text, "call __fnwrap_add2(") != NULL &&
        strstr(actual_text, "call id(") == NULL &&
        strstr(actual_text, "call apply(") == NULL &&
        strstr(actual_text, "id__fv_0_") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-PASSTHROUGH-TERNARY-NONCAPTURING-ACTUAL mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_passthrough_ternary_closure_function_value_actual_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "store_local __ternary_fn_argtag.") != NULL &&
        strstr(actual_text, "store_local __ternary_fn_argcap_0.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "call id(") == NULL &&
        strstr(actual_text, "call apply(") == NULL &&
        strstr(actual_text, "id__fv_0_") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-PASSTHROUGH-TERNARY-CLOSURE-ACTUAL mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_second_order_dynamic_ternary_function_value_actual_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "call getint()\n") != NULL &&
        strstr(actual_text, "store_local __ternary_fn_argtag.1, 1\n") != NULL &&
        strstr(actual_text, "store_local __ternary_fn_argtag.1, 2\n") != NULL &&
        strstr(actual_text, "pass__fv_0_apply_1_add1") == NULL &&
        strstr(actual_text, "pass__fv_0_apply_twice_1_add1") == NULL &&
        strstr(actual_text, "apply_twice__fv_0_add1") == NULL &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call __fnwrap_add1(0,") == 3u;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR second-order dynamic ternary function value actual argument mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_passthrough_ternary_zero_arg_function_value_actual_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "store_local __ternary_fn_argtag.") != NULL &&
        strstr(actual_text, "call __fnwrap_next1(") != NULL &&
        strstr(actual_text, "call __fnwrap_next2(") != NULL &&
        strstr(actual_text, "call id0(") == NULL &&
        strstr(actual_text, "call apply0(") == NULL &&
        strstr(actual_text, "id0__fv_0_") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-PASSTHROUGH-TERNARY-ZERO-ACTUAL mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_passthrough_ternary_zero_arg_void_function_value_actual_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "store_local __ternary_fn_argtag.") != NULL &&
        strstr(actual_text, "call __fnwrap_ping1(") != NULL &&
        strstr(actual_text, "call __fnwrap_ping2(") != NULL &&
        strstr(actual_text, "call idv(") == NULL &&
        strstr(actual_text, "call apply0(") == NULL &&
        strstr(actual_text, "idv__fv_0_") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-PASSTHROUGH-TERNARY-ZERO-VOID-ACTUAL mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_passthrough_ternary_zero_arg_closure_function_value_actual_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "store_local __ternary_fn_argtag.") != NULL &&
        strstr(actual_text, "store_local __ternary_fn_argcap_0.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "call id0(") == NULL &&
        strstr(actual_text, "call apply0(") == NULL &&
        strstr(actual_text, "apply0__fv_0_") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-PASSTHROUGH-TERNARY-ZERO-CLOSURE-ACTUAL mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_passthrough_ternary_zero_arg_void_closure_function_value_actual_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "store_local __ternary_fn_argtag.") != NULL &&
        strstr(actual_text, "store_local __ternary_fn_argcap_0.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "call idv(") == NULL &&
        strstr(actual_text, "call apply0(") == NULL &&
        strstr(actual_text, "apply0__fv_0_") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-PASSTHROUGH-TERNARY-ZERO-VOID-CLOSURE-ACTUAL mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_ternary_closure_function_value_return_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int pick(int c)(int){ int x=3; int y=5; int f(int)=closure [x] int (int z){ return x+z; }; int g(int)=closure [y] int (int z){ return y+z; }; return ((putint(0), c) ? f : g); }\n"
            "int main(){ return pick(1)(4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "tmp.0 = addr_local __retclosure_immediate_0.0\n") != NULL &&
        strstr(actual_text, "tmp.1 = addr_local __retclosure_immediate_1.1\n") != NULL &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call pick(") == 1u &&
        strstr(actual_text, "store_local __closure_env_0.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "__retclosure_declslot") == NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-TERNARY-CLOSURE-RETURN-IMMEDIATE mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_returned_closure_forwarding_into_function_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "store_indirect tmp.0, tmp.1\n") != NULL &&
        strstr(actual_text, "call make(") != NULL &&
        strstr(actual_text, "store_local f$ftag.1, 1\n") != NULL &&
        strstr(actual_text, "store_local __closure_env_0.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_make__retclosure_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_make__retclosure_") != NULL &&
        strstr(actual_text, "call make__retclosure_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-RETURNED-CLOSURE-FORWARD mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_returned_closure_alias_forwarding_into_function_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "store_local g$ftag.") != NULL &&
        strstr(actual_text, "store_local g$closurecap$0.") != NULL &&
        strstr(actual_text, "call make(") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_make__retclosure_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_make__retclosure_") != NULL &&
        strstr(actual_text, "call make__retclosure_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-RETURNED-CLOSURE-ALIAS-FORWARD mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_returned_single_capture_closure_forwarding_into_function_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "store_indirect tmp.0, tmp.1\n") != NULL &&
        strstr(actual_text, "addr_local f$closurecap$0.") != NULL &&
        strstr(actual_text, "call make(") != NULL &&
        strstr(actual_text, "store_local f$ftag.1, 1\n") != NULL &&
        strstr(actual_text, "load_local f$closurecap$0.") != NULL &&
        strstr(actual_text, "store_local __closure_env_0.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_make__retclosure_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_make__retclosure_") != NULL &&
        strstr(actual_text, "call make__retclosure_") != NULL &&
        strstr(actual_text, "__retclosure_declslot") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-RETURNED-SINGLE-CAPTURE-CLOSURE-FORWARD mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_direct_returned_single_capture_closure_function_value_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int apply(int f(int), int x){ return f(x); }\n"
            "int make(int x)(int) { return closure [x] int (int y) { return x + y; }; }\n"
            "int main(){ return apply(make(3), 4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply(f.0, x.1)\n") != NULL &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call make(") == 1u &&
        strstr(actual_text, "addr_local __retclosure_argslot_0.0\n") != NULL &&
        strstr(actual_text, "store_local __closure_env_0.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_make__retclosure_") != NULL &&
        strstr(actual_text, "__retclosure_declslot") == NULL &&
        strstr(actual_text, "apply__fv_0_make__retclosure_") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DIRECT-RETURNED-SINGLE-CAPTURE-CLOSURE-ARG mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_direct_returned_single_capture_zero_arg_closure_function_value_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int apply0(int f()){ return f(); }\n"
            "int make(int x)() { return closure [x] int () { return x; }; }\n"
            "int main(){ return apply0(make(3)); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply0(f.0)\n") != NULL &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call make(") == 1u &&
        strstr(actual_text, "addr_local __retclosure_argslot_0.0\n") != NULL &&
        strstr(actual_text, "store_local __closure_env_0.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_make__retclosure_") != NULL &&
        strstr(actual_text, "__retclosure_declslot") == NULL &&
        strstr(actual_text, "apply0__fv_0_make__retclosure_") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DIRECT-RETURNED-SINGLE-CAPTURE-ZERO-CLOSURE-ARG mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_direct_returned_single_capture_zero_arg_void_closure_function_value_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "void apply0(void f()){ f(); }\n"
            "void make(int x)() { return closure [x] void () { putint(x); return; }; }\n"
            "int main(){ apply0(make(7)); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply0(f.0)\n") != NULL &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call make(") == 1u &&
        strstr(actual_text, "addr_local __retclosure_argslot_0.0\n") != NULL &&
        strstr(actual_text, "store_local __closure_env_0.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_make__retclosure_") != NULL &&
        strstr(actual_text, "__retclosure_declslot") == NULL &&
        strstr(actual_text, "apply0__fv_0_make__retclosure_") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DIRECT-RETURNED-SINGLE-CAPTURE-ZERO-VOID-CLOSURE-ARG mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_direct_returned_multi_capture_closure_function_value_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int apply(int f(int,int), int a, int b){ return f(a,b); }\n"
            "int make(int x, int y)(int,int) { return closure [x,y] int (int a, int b) { return x + y + a + b; }; }\n"
            "int main(){ return apply(make(3, 4), 5, 6); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply(f.0, a.1, b.2)\n") != NULL &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call make(") == 1u &&
        strstr(actual_text, "addr_local __retclosure_argslot_0.0\n") != NULL &&
        strstr(actual_text, "addr_local __retclosure_argslot_1.1\n") != NULL &&
        strstr(actual_text, "store_local __closure_env_0.") != NULL &&
        strstr(actual_text, "store_local __closure_env_1.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_make__retclosure_") != NULL &&
        strstr(actual_text, "apply__fv_0_make__retclosure_") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DIRECT-RETURNED-MULTI-CAPTURE-CLOSURE-ARG mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_returned_single_capture_closure_alias_forwarding_into_function_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "store_local g$ftag.") != NULL &&
        strstr(actual_text, "call make(") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_make__retclosure_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_make__retclosure_") != NULL &&
        strstr(actual_text, "call make__retclosure_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-RETURNED-SINGLE-CAPTURE-CLOSURE-ALIAS-FORWARD mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_returned_single_capture_zero_arg_closure_forwarding_into_function_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "store_indirect tmp.0, tmp.1\n") != NULL &&
        strstr(actual_text, "addr_local f$closurecap$0.") != NULL &&
        strstr(actual_text, "call make(") != NULL &&
        strstr(actual_text, "store_local f$ftag.1, 1\n") != NULL &&
        strstr(actual_text, "load_local f$closurecap$0.") != NULL &&
        strstr(actual_text, "store_local __closure_env_0.") != NULL &&
        strstr(actual_text, "call make__retclosure_") != NULL &&
        strstr(actual_text, "__retclosure_declslot") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-RETURNED-SINGLE-CAPTURE-ZERO-CLOSURE-FORWARD mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_returned_single_capture_zero_arg_void_closure_forwarding_into_function_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "store_indirect tmp.0, tmp.1\n") != NULL &&
        strstr(actual_text, "addr_local f$closurecap$0.") != NULL &&
        strstr(actual_text, "call make(") != NULL &&
        strstr(actual_text, "store_local f$ftag.1, 1\n") != NULL &&
        strstr(actual_text, "load_local f$closurecap$0.") != NULL &&
        strstr(actual_text, "store_local __closure_env_0.") != NULL &&
        strstr(actual_text, "call putint(") != NULL &&
        strstr(actual_text, "call make__retclosure_") != NULL &&
        strstr(actual_text, "__retclosure_declslot") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-RETURNED-SINGLE-CAPTURE-ZERO-VOID-CLOSURE-FORWARD mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_returned_local_closure_forwarding_into_function_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "store_indirect tmp.4, tmp.5\n") != NULL &&
        strstr(actual_text, "store_indirect tmp.6, tmp.7\n") != NULL &&
        strstr(actual_text, "call pick(tmp.0, tmp.1, 5, 1)\n") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "call pick__closure_f_") != NULL &&
        strstr(actual_text, "call pick__closure_g_") != NULL &&
        strstr(actual_text, "__retclosure_declslot") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-RETURNED-LOCAL-CLOSURE-FORWARD mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_direct_dynamic_returned_local_closure_function_value_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int apply(int f(int), int x){ return f(x); }\n"
            "int pick(int x, int c)(int) { int f(int)=closure [x] int (int y) { return x + y; }; int g(int)=closure [x] int (int y) { return x - y; }; if(c) f=g; return f; }\n"
            "int main(){ return apply(pick(5, 1), 3); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply(f.0, x.1)\n") != NULL &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call pick(") == 1u &&
        strstr(actual_text, "addr_local __retclosure_argslot_0.0\n") != NULL &&
        strstr(actual_text, "addr_local __retclosure_argslot_1.1\n") != NULL &&
        strstr(actual_text, "store_local __closure_env_0.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "apply__fv_0_pick__closure_") == NULL &&
        strstr(actual_text, "__retclosure_declslot") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DIRECT-DYNAMIC-RETURNED-LOCAL-CLOSURE-ARG mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_direct_dynamic_returned_local_zero_arg_closure_function_value_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int apply0(int f()){ return f(); }\n"
            "int pick(int x, int c)(){ int f()=closure [x] int () { return x; }; int g()=closure [x] int () { return x + 1; }; if(c) f=g; return f; }\n"
            "int main(){ return apply0(pick(5, 1)); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply0(f.0)\n") != NULL &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call pick(") == 1u &&
        strstr(actual_text, "addr_local __retclosure_argslot_0.0\n") != NULL &&
        strstr(actual_text, "addr_local __retclosure_argslot_1.1\n") != NULL &&
        strstr(actual_text, "store_local __closure_env_0.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "apply0__fv_0_pick__closure_") == NULL &&
        strstr(actual_text, "__retclosure_declslot") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DIRECT-DYNAMIC-RETURNED-LOCAL-ZERO-CLOSURE-ARG mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_direct_dynamic_returned_local_zero_arg_void_closure_function_value_argument_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "void apply0(void f()){ f(); }\n"
            "void pick(int x, int c)(){ void f()=closure [x] void () { putint(x); return; }; void g()=closure [x] void () { putint(x + 1); return; }; if(c) f=g; return f; }\n"
            "int main(){ apply0(pick(5, 1)); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply0(f.0)\n") != NULL &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call pick(") == 1u &&
        strstr(actual_text, "addr_local __retclosure_argslot_0.0\n") != NULL &&
        strstr(actual_text, "addr_local __retclosure_argslot_1.1\n") != NULL &&
        strstr(actual_text, "store_local __closure_env_0.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "apply0__fv_0_pick__closure_") == NULL &&
        strstr(actual_text, "__retclosure_declslot") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DIRECT-DYNAMIC-RETURNED-LOCAL-ZERO-VOID-CLOSURE-ARG mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_multi_capture_closure_forwarding_into_function_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int apply(int f(int,int), int a, int b){ return f(a,b); }\n"
            "int main(){ int x=3; int y=4; int f(int,int)=closure [x,y] int (int a, int b) { return x + y + a + b; }; return apply(f, 5, 6); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply(f.0, a.1, b.2)\n") != NULL &&
        strstr(actual_text, "store_local f$closurecap$0.") != NULL &&
        strstr(actual_text, "store_local f$closurecap$1.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "call main__closure_f_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-MULTI-CAPTURE-CLOSURE-FORWARD mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_direct_closure_literal_argument_to_function_value_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int apply(int f(int), int x){ return f(x); }\n"
            "int main(){ int y=3; return apply(closure [y] int (int z) { return y + z; }, 4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply(f.0, x.1)\n") != NULL &&
        strstr(actual_text, "store_local __argclosure_cap_0.") != NULL &&
        strstr(actual_text, "store_local __closure_env_0.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_main__closure___argclosure_") != NULL &&
        strstr(actual_text, "call main__closure___argclosure_") != NULL &&
        strstr(actual_text, "apply__fv_") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DIRECT-CLOSURE-LITERAL-ARG mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_zero_arg_direct_closure_literal_argument_to_function_value_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int apply0(int f()){ return f(); }\n"
            "int main(){ int y=3; return apply0(closure [y] int () { return y; }); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply0(f.0)\n") != NULL &&
        strstr(actual_text, "store_local __argclosure_cap_0.") != NULL &&
        strstr(actual_text, "store_local __closure_env_0.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_main__closure___argclosure_") != NULL &&
        strstr(actual_text, "call main__closure___argclosure_") != NULL &&
        strstr(actual_text, "apply0__fv_") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-ZERO-ARG-DIRECT-CLOSURE-LITERAL-ARG mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_void_direct_closure_literal_argument_to_function_value_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "void apply0(void f()){ f(); }\n"
            "int main(){ int y=7; apply0(closure [y] void () { putint(y); return; }); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply0(f.0)\n") != NULL &&
        strstr(actual_text, "store_local __argclosure_cap_0.") != NULL &&
        strstr(actual_text, "store_local __closure_env_0.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_main__closure___argclosure_") != NULL &&
        strstr(actual_text, "call main__closure___argclosure_") != NULL &&
        strstr(actual_text, "apply0__fv_") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-VOID-DIRECT-CLOSURE-LITERAL-ARG mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_multi_capture_direct_closure_literal_argument_to_function_value_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int apply(int f(int,int), int a, int b){ return f(a,b); }\n"
            "int main(){ int x=3; int y=4; return apply(closure [x,y] int (int a, int b) { return x + y + a + b; }, 5, 6); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply(f.0, a.1, b.2)\n") != NULL &&
        strstr(actual_text, "store_local __argclosure_cap_0.") != NULL &&
        strstr(actual_text, "store_local __argclosure_cap_1.") != NULL &&
        strstr(actual_text, "store_local __closure_env_0.") != NULL &&
        strstr(actual_text, "store_local __closure_env_1.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_main__closure___argclosure_") != NULL &&
        strstr(actual_text, "call main__closure___argclosure_") != NULL &&
        strstr(actual_text, "apply__fv_") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-MULTI-CAPTURE-DIRECT-CLOSURE-LITERAL-ARG mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_closure_alias_chain_callable_object_transport_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "store_local g$ftag.3, tmp.") != NULL &&
        strstr(actual_text, "store_local g$closurecap$0.4, tmp.") != NULL &&
        strstr(actual_text, "store_local h$ftag.5, tmp.") != NULL &&
        strstr(actual_text, "store_local h$closurecap$0.6, tmp.") != NULL &&
        strstr(actual_text, "store_local b$ftag.12, tmp.") != NULL &&
        strstr(actual_text, "store_local b$closurecap$0.13, tmp.") != NULL &&
        strstr(actual_text, "store_local q$ftag.19, tmp.") != NULL &&
        strstr(actual_text, "store_local q$closurecap$0.20, tmp.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_main__closure_a_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_main__closure_p_") != NULL &&
        strstr(actual_text, "call apply(") == NULL &&
        strstr(actual_text, "call apply0(") == NULL &&
        strstr(actual_text, "call apply0v(") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-CLOSURE-ALIAS-CHAIN-CALLABLE-OBJECT-TRANSPORT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_closure_direct_local_call_callable_object_transport_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int main(){ int x=3; int y=4; int z=9; int f(int)=closure [x] int (int a) { return x + a; }; int g()=closure [y] int () { return y; }; int h(int,int)=closure [x,y] int (int a, int b) { return x + y + a + b; }; void p()=closure [z] void () { putint(z); return; }; int r1=f(1); int r2=g(); int r3=h(5,6); p(); return r1 + r2 + r3; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "store_local f$closurecap$0.") != NULL &&
        strstr(actual_text, "store_local g$closurecap$0.") != NULL &&
        strstr(actual_text, "store_local h$closurecap$0.") != NULL &&
        strstr(actual_text, "store_local h$closurecap$1.") != NULL &&
        strstr(actual_text, "store_local p$closurecap$0.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_main__closure_h_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_main__closure_p_") != NULL &&
        strstr(actual_text, "call main__closure_f_") != NULL &&
        strstr(actual_text, "call main__closure_g_") != NULL &&
        strstr(actual_text, "call main__closure_h_") != NULL &&
        strstr(actual_text, "call main__closure_p_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-CLOSURE-DIRECT-LOCAL-CALL-CALLABLE-OBJECT-TRANSPORT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_rich_body_closure_direct_local_call_callable_object_transport_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int main(){ int x=3; int y=8; int f(int)=closure [x] int (int a) { putint(x); return x + a; }; int g(int)=closure [y] int (int b) { return (b = b + 1, y + b); }; int r1=f(4); int r2=g(5); return r1 + r2; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "store_local f$closurecap$0.") != NULL &&
        strstr(actual_text, "store_local g$closurecap$0.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "call main__closure_f_") != NULL &&
        strstr(actual_text, "call main__closure_g_") != NULL &&
        strstr(actual_text, "call putint(") != NULL &&
        strstr(actual_text, "store_local b.1, tmp.0\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-RICH-BODY-CLOSURE-DIRECT-LOCAL-CALL-CALLABLE-OBJECT-TRANSPORT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_returned_multi_capture_local_closure_bind_and_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int pick(int x,int y)(int,int) { int f(int,int)=closure [x,y] int (int a, int b) { return x + y + a + b; }; return f; }\n"
            "int main(){ int h(int,int)=pick(3,4); return h(5,6); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func pick(__ret0.0, __ret1.1, x.2, y.3) {\n") != NULL &&
        strstr(actual_text, "store_indirect tmp.2, tmp.3\n") != NULL &&
        strstr(actual_text, "store_indirect tmp.4, tmp.5\n") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_pick__closure_f_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-RETURNED-MULTI-CAPTURE-LOCAL-CLOSURE-BIND-CALL mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_returned_multi_capture_local_closure_forwarding_into_function_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "store_indirect tmp.7, tmp.8\n") != NULL &&
        strstr(actual_text, "store_indirect tmp.9, tmp.10\n") != NULL &&
        strstr(actual_text, "store_indirect tmp.11, tmp.12\n") != NULL &&
        strstr(actual_text, "call pick(tmp.0, tmp.1, tmp.2, 5, 7, 1)\n") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_pick__closure_g_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-RETURNED-MULTI-CAPTURE-LOCAL-CLOSURE-FORWARD mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_direct_dynamic_returned_multi_capture_closure_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int pick(int x, int y, int c)(int,int) { int f(int,int)=closure [x,y] int (int a, int b) { return x + y + a + b; }; int g(int,int)=closure [x,y] int (int a, int b) { return x + y - a - b; }; if(c) f=g; return f; }\n"
            "int main(){ return pick(5, 7, 1)(3, 2); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "tmp.0 = addr_local __retclosure_immediate_0.0\n") != NULL &&
        strstr(actual_text, "tmp.1 = addr_local __retclosure_immediate_1.1\n") != NULL &&
        strstr(actual_text, "tmp.2 = addr_local __retclosure_immediate_2.2\n") != NULL &&
        strstr(actual_text, "call pick(tmp.0, tmp.1, tmp.2, 5, 7, 1)\n") != NULL &&
        strstr(actual_text, "store_local __closure_env_0.") != NULL &&
        strstr(actual_text, "store_local __closure_env_1.") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_pick__closure_f_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_pick__closure_g_") != NULL &&
        strstr(actual_text, "__retclosure_callcap") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DIRECT-DYNAMIC-RETURNED-MULTI-CAPTURE-IMMEDIATE mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_local_function_value_forwarding_into_function_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "store_local g$ftag.1, 1\n") != NULL &&
        strstr(actual_text, "call putint(0)\n") != NULL &&
        strstr(actual_text, "store_local g$ftag.1, 2\n") != NULL &&
        strstr(actual_text, "load_local g$ftag.1\n") != NULL &&
        strstr(actual_text, "call __fnwrap_add1(0, 40)\n") != NULL &&
        strstr(actual_text, "call __fnwrap_add2(0, 40)\n") != NULL &&
        strstr(actual_text, "func __fnwrap_add1(__env.0, x.1) {\n") != NULL &&
        strstr(actual_text, "func __fnwrap_add2(__env.0, x.1) {\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-FNVAL-FORWARD mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_multiple_static_function_valued_parameters_without_specialization_shell_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "call __fnwrap_double1(0, 20)\n") != NULL &&
        strstr(actual_text, "call __fnwrap_add1(0, tmp.") != NULL &&
        strstr(actual_text, "func __fnwrap_double1(__env.0, x.1) {\n") != NULL &&
        strstr(actual_text, "func __fnwrap_add1(__env.0, x.1) {\n") != NULL &&
        strstr(actual_text, "compose__fv_0_add1_1_double1") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-MULTI-STATIC-FNVAL-PARAM-NO-SHELL mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_second_order_function_value_parameter_forwarding_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "call __fnwrap_add1(0, 41)\n") != NULL &&
        strstr(actual_text, "pass__fv_0_apply_1_add1") == NULL &&
        strstr(actual_text, "apply__fv_0_add1") == NULL &&
        strstr(actual_text, "call pass(") == NULL &&
        strstr(actual_text, "call apply(") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR second-order function-value parameter forwarding mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_second_order_closure_function_value_parameter_forwarding_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "call __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "call pass(") == NULL &&
        strstr(actual_text, "pass__fv_0_apply_1_main__closure_f_") == NULL &&
        strstr(actual_text, "apply__fv_0_main__closure_f_") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR second-order closure function-value parameter forwarding mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_second_order_zero_arg_function_value_parameter_forwarding_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "call __fnwrap_next(0)\n") != NULL &&
        strstr(actual_text, "call pass0(") == NULL &&
        strstr(actual_text, "pass0__fv_0_apply0_1_next") == NULL &&
        strstr(actual_text, "apply0__fv_0_next") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR second-order zero-arg function-value parameter forwarding mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_second_order_zero_arg_wrapper_scalar_update_function_value_parameter_forwarding_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "ret 1\n") != NULL &&
        strstr(actual_text, "call pass0(") == NULL &&
        strstr(actual_text, "pass0__fv_0_apply0_1_next") == NULL &&
        strstr(actual_text, "apply0__fv_0_next") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR second-order zero-arg wrapper scalar-update function-value parameter forwarding mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_second_order_returned_zero_arg_wrapper_scalar_update_function_value_parameter_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
            "[lower-ir-reg] FAIL: LOWER-IR second-order returned zero-arg wrapper scalar-update immediate-call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_second_order_returned_zero_arg_wrapper_scalar_update_function_value_parameter_bind_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "store_local g$ftag.0, 1\n") != NULL &&
        strstr(actual_text, "tmp.0 = add 0, 1\n") != NULL &&
        strstr(actual_text, "call idh0(") == NULL &&
        strstr(actual_text, "call pass0(") == NULL &&
        strstr(actual_text, "pass0__fv_0_apply0_1_next") == NULL &&
        strstr(actual_text, "apply0__fv_0_next") == NULL &&
        strstr(actual_text, "ret 1\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR second-order returned zero-arg wrapper scalar-update bind-call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_second_order_zero_arg_closure_function_value_parameter_forwarding_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "call __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "call pass0(") == NULL &&
        strstr(actual_text, "pass0__fv_0_apply0_1_main__closure_f_") == NULL &&
        strstr(actual_text, "apply0__fv_0_main__closure_f_") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR second-order zero-arg closure function-value parameter forwarding mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_second_order_zero_arg_void_function_value_parameter_forwarding_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "call __fnwrap_ping(0)\n") != NULL &&
        strstr(actual_text, "call pass0(") == NULL &&
        strstr(actual_text, "pass0__fv_0_apply0_1_ping") == NULL &&
        strstr(actual_text, "apply0__fv_0_ping") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR second-order zero-arg void function-value parameter forwarding mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_second_order_zero_arg_void_wrapper_scalar_update_function_value_parameter_forwarding_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "call __fnwrap_ping(0)\n") != NULL &&
        strstr(actual_text, "call pass0(") == NULL &&
        strstr(actual_text, "pass0__fv_0_apply0_1_ping") == NULL &&
        strstr(actual_text, "apply0__fv_0_ping") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR second-order zero-arg void wrapper scalar-update function-value parameter forwarding mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_second_order_returned_zero_arg_void_wrapper_scalar_update_function_value_parameter_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "call __fnwrap_ping(0)\n") != NULL &&
        strstr(actual_text, "call idv0(") == NULL &&
        strstr(actual_text, "call pass0(") == NULL &&
        strstr(actual_text, "pass0__fv_0_apply0_1_ping") == NULL &&
        strstr(actual_text, "apply0__fv_0_ping") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR second-order returned zero-arg void wrapper scalar-update immediate-call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_second_order_returned_zero_arg_void_wrapper_scalar_update_function_value_parameter_bind_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "store_local g$ftag.0, 1\n") != NULL &&
        strstr(actual_text, "tmp.0 = add 0, 1\n") != NULL &&
        strstr(actual_text, "call __fnwrap_ping(0)\n") != NULL &&
        strstr(actual_text, "call idv0(") == NULL &&
        strstr(actual_text, "call pass0(") == NULL &&
        strstr(actual_text, "pass0__fv_0_apply0_1_ping") == NULL &&
        strstr(actual_text, "apply0__fv_0_ping") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR second-order returned zero-arg void wrapper scalar-update bind-call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_second_order_zero_arg_void_closure_function_value_parameter_forwarding_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "call __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "call main__closure_f_") != NULL &&
        strstr(actual_text, "call pass0(") == NULL &&
        strstr(actual_text, "apply0__fv_0_main__closure_f_") == NULL &&
        strstr(actual_text, "pass0__fv_0_apply0_1_main__closure_f_") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR second-order zero-arg void closure function-value parameter forwarding mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_second_order_returned_passthrough_dynamic_zero_arg_void_function_value_parameter_forwarding_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "call pickhv(tmp.0, tmp.1)\n") != NULL &&
        strstr(actual_text, "call __fnwrap_ping(0)\n") != NULL &&
        strstr(actual_text, "applyv__fv_0_ping") == NULL &&
        strstr(actual_text, "applyv_twice__fv_0_ping") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR second-order returned passthrough dynamic zero-arg void function-value parameter forwarding mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_second_order_returned_passthrough_dynamic_zero_arg_function_value_parameter_forwarding_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "call pickh0(tmp.0, tmp.1)\n") != NULL &&
        strstr(actual_text, "call __fnwrap_next(0)\n") != NULL &&
        strstr(actual_text, "apply0__fv_0_next") == NULL &&
        strstr(actual_text, "apply0_twice__fv_0_next") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR second-order returned passthrough dynamic zero-arg function-value parameter forwarding mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_second_order_local_function_value_parameter_forwarding_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        (strstr(actual_text, "call __fnwrap_add1(0, 41)\n") != NULL ||
            strstr(actual_text, "tmp.0 = add 41, 1\n") != NULL) &&
        strstr(actual_text, "call pass(") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR second-order local function-value parameter forwarding mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_second_order_local_reassigned_function_value_parameter_forwarding_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        (strstr(actual_text, "call __fnwrap_add1(0, 41)\n") != NULL ||
            strstr(actual_text, "tmp.0 = add 41, 1\n") != NULL) &&
        strstr(actual_text, "call pass(") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR second-order local reassigned function-value parameter forwarding mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_second_order_local_scalar_and_reassigned_function_value_parameter_forwarding_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        (strstr(actual_text, "call __fnwrap_add1(0, 41)\n") != NULL ||
            strstr(actual_text, "tmp.0 = add 41, 1\n") != NULL) &&
        strstr(actual_text, "call pass(") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR second-order local scalar+reassigned function-value parameter forwarding mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_second_order_local_wrapper_scalar_update_function_value_parameter_forwarding_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "store_local h$ftag.0, 1\n") != NULL &&
        strstr(actual_text, "tmp.0 = add 41, 1\n") != NULL &&
        strstr(actual_text, "pass__fv_0_apply_1_add1") == NULL &&
        strstr(actual_text, "apply__fv_0_add1") == NULL &&
        (strstr(actual_text, "call __fnwrap_add1(0, tmp.0)\n") != NULL ||
            strstr(actual_text, "tmp.1 = add tmp.0, 1\n") != NULL) &&
        strstr(actual_text, "call pass(") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR second-order local wrapper scalar-update function-value parameter forwarding mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_second_order_local_wrapper_repeated_scalar_update_function_value_parameter_forwarding_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "store_local h$ftag.0, 1\n") != NULL &&
        strstr(actual_text, "tmp.0 = add 41, 1\n") != NULL &&
        strstr(actual_text, "tmp.1 = add tmp.0, 1\n") != NULL &&
        strstr(actual_text, "pass__fv_0_apply_1_add1") == NULL &&
        strstr(actual_text, "apply__fv_0_add1") == NULL &&
        (strstr(actual_text, "call __fnwrap_add1(0, tmp.1)\n") != NULL ||
            strstr(actual_text, "tmp.2 = add tmp.1, 1\n") != NULL) &&
        strstr(actual_text, "call pass(") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR second-order local wrapper repeated scalar-update function-value parameter forwarding mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_third_order_function_value_parameter_forwarding_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        (strstr(actual_text, "call __fnwrap_add1(0, 41)\n") != NULL ||
            strstr(actual_text, "tmp.0 = add 41, 1\n") != NULL) &&
        strstr(actual_text, "call relay(") == NULL &&
        strstr(actual_text, "call pass(") == NULL &&
        strstr(actual_text, "relay__fv_0_pass_1_apply_2_add1") == NULL &&
        strstr(actual_text, "pass__fv_0_apply_1_add1") == NULL &&
        strstr(actual_text, "apply__fv_0_add1") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR third-order function-value parameter forwarding mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_third_order_zero_arg_wrapper_scalar_update_function_value_parameter_forwarding_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "ret 1\n") != NULL &&
        strstr(actual_text, "call relay0(") == NULL &&
        strstr(actual_text, "call pass0(") == NULL &&
        strstr(actual_text, "relay0__fv_0_pass0_1_apply0_2_next") == NULL &&
        strstr(actual_text, "pass0__fv_0_apply0_1_next") == NULL &&
        strstr(actual_text, "apply0__fv_0_next") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR third-order zero-arg wrapper scalar-update function-value parameter forwarding mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_third_order_wrapper_scalar_update_function_value_parameter_forwarding_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
            "[lower-ir-reg] FAIL: LOWER-IR third-order wrapper scalar-update function-value parameter forwarding mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_third_order_zero_arg_void_wrapper_scalar_update_function_value_parameter_forwarding_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "call __fnwrap_ping(0)\n") != NULL &&
        strstr(actual_text, "call relay0(") == NULL &&
        strstr(actual_text, "call pass0(") == NULL &&
        strstr(actual_text, "relay0__fv_0_pass0_1_apply0_2_ping") == NULL &&
        strstr(actual_text, "pass0__fv_0_apply0_1_ping") == NULL &&
        strstr(actual_text, "apply0__fv_0_ping") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR third-order zero-arg void wrapper scalar-update function-value parameter forwarding mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_third_order_wrapper_repeated_scalar_update_function_value_parameter_forwarding_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
            "[lower-ir-reg] FAIL: LOWER-IR third-order wrapper repeated scalar-update function-value parameter forwarding mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_third_order_returned_function_value_parameter_bind_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "store_local g$ftag.0, 1\n") != NULL &&
        strstr(actual_text, "call __fnwrap_add1(0, 41)\n") != NULL &&
        strstr(actual_text, "pass__fv_0_apply_1_add1") == NULL &&
        strstr(actual_text, "apply__fv_0_add1") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR third-order returned function-value parameter bind mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_third_order_local_function_value_parameter_forwarding_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "store_local g$ftag.0, 1\n") != NULL &&
        strstr(actual_text, "call __fnwrap_add1(0, 41)\n") != NULL &&
        strstr(actual_text, "relay__fv_0_pass_1_apply_2_add1") == NULL &&
        strstr(actual_text, "pass__fv_0_apply_1_add1") == NULL &&
        strstr(actual_text, "apply__fv_0_add1") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR third-order local function-value parameter forwarding mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_second_order_returned_function_value_parameter_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "call __fnwrap_add1(0, 41)\n") != NULL &&
        strstr(actual_text, "apply__fv_0_add1") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR second-order returned function-value parameter immediate-call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_second_order_returned_closure_function_value_parameter_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int apply(int f(int), int x){ return f(x); }\n"
            "int idh(int h(int f(int), int x))(int f(int), int x){ return h; }\n"
            "int main(){ int y=3; int f(int)=closure [y] int (int z){ return y+z; }; return idh(apply)(f, 4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare idh(__ret0.0, h.1)\n") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "call main__closure_f_") != NULL &&
        strstr(actual_text, "call idh(") == NULL &&
        strstr(actual_text, "call apply(") == NULL &&
        strstr(actual_text, "apply__fv_0_main__closure_f_") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR second-order returned closure function-value parameter immediate-call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_second_order_dynamic_returned_function_value_parameter_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "call getint()\n") != NULL &&
        strstr(actual_text, "call pick(tmp.0, tmp.1)\n") != NULL &&
        strstr(actual_text, "tmp.3 = eq tmp.11, 1\n") != NULL &&
        strstr(actual_text, "apply__fv_0_add1") == NULL &&
        strstr(actual_text, "apply_twice__fv_0_add1") == NULL &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call __fnwrap_add1(0, 41)\n") == 2u &&
        strstr(actual_text, "call __fnwrap_add1(0, tmp.8)\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR second-order dynamic returned function-value parameter immediate-call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_second_order_returned_passthrough_dynamic_noncapturing_function_value_parameter_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "call pickh_nc(tmp.0, tmp.1)\n") != NULL &&
        strstr(actual_text, "tmp.3 = eq tmp.11, 1\n") != NULL &&
        strstr(actual_text, "apply__fv_0_add1") == NULL &&
        strstr(actual_text, "apply_twice__fv_0_add1") == NULL &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call __fnwrap_add1(0, 41)\n") == 2u &&
        strstr(actual_text, "call __fnwrap_add1(0, tmp.8)\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR second-order returned passthrough dynamic noncapturing function-value parameter immediate-call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_second_order_returned_passthrough_dynamic_function_value_parameter_immediate_call_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int idh(int h(int f(int), int x))(int f(int), int x){ return h; }\n"
            "int pickh(int base, int c)(int f(int), int x){ int h(int f(int), int x)=closure [base] int (int g(int), int y){ return g(y) + base; }; int k(int f(int), int x)=closure [base] int (int g(int), int y){ return g(g(y)) + base; }; if(c) h=k; return h; }\n"
            "int add1(int x){ return x+1; }\n"
            "int main(){ return idh(pickh(5, getint()))(add1, 41); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call pickh(tmp.0, tmp.1, 5, tmp.2)\n") != NULL &&
        strstr(actual_text, "load_local __retclosure_argcap_0.0\n") != NULL &&
        strstr(actual_text, "call pickh__closure_h_") == NULL &&
        strstr(actual_text, "call pickh__closure_k_") == NULL &&
        strstr(actual_text, "__fv_1_add1") == NULL &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call __fnwrap_add1(0,") == 3u;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR second-order returned passthrough dynamic function-value parameter immediate-call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_second_order_dynamic_local_function_value_parameter_forwarding_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "call getint()\n") != NULL &&
        strstr(actual_text, "pass__fv_0_apply_1_add1") == NULL &&
        strstr(actual_text, "pass__fv_0_apply_twice_1_add1") == NULL &&
        strstr(actual_text, "apply_twice__fv_0_add1") == NULL &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call __fnwrap_add1(0,") == 3u;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR second-order dynamic local function-value parameter forwarding mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_second_order_dynamic_local_closure_function_value_parameter_forwarding_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int apply(int f(int), int x){ return f(x); }\n"
            "int apply_twice(int f(int), int x){ return f(f(x)); }\n"
            "int pass(int h(int f(int), int x), int f(int), int x){ return h(f, x); }\n"
            "int main(){ int c=getint(); int y=3; int f(int)=closure [y] int (int z){ return y+z; }; int h(int f(int), int x)= c ? apply : apply_twice; return pass(h, f, 4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call getint()\n") != NULL &&
        strstr(actual_text, "tmp.2 = eq tmp.") != NULL &&
        strstr(actual_text, "func pass__fv_0_apply_1_main__closure_f_") == NULL &&
        strstr(actual_text, "call __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "call main__closure_f_") != NULL &&
        strstr(actual_text, "call pass(") == NULL &&
        strstr(actual_text, "apply__fv_0_main__closure_f_") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR second-order dynamic local closure function-value parameter forwarding mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_second_order_returned_passthrough_dynamic_local_function_value_parameter_forwarding_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int idh(int h(int f(int), int x))(int f(int), int x){ return h; }\n"
            "int pickh(int base, int c)(int f(int), int x){ int h(int f(int), int x)=closure [base] int (int g(int), int y){ return g(y) + base; }; int k(int f(int), int x)=closure [base] int (int g(int), int y){ return g(g(y)) + base; }; if(c) h=k; return h; }\n"
            "int add1(int x){ return x+1; }\n"
            "int main(){ int g(int f(int), int x)=idh(pickh(5, getint())); return g(add1, 41); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call pickh(tmp.0, tmp.1, 5, tmp.2)\n") != NULL &&
        strstr(actual_text, "store_local g$ftag.0, tmp.") != NULL &&
        strstr(actual_text, "store_local g$closurecap$0.1, tmp.") != NULL &&
        strstr(actual_text, "load_local g$ftag.0\n") != NULL &&
        strstr(actual_text, "call pickh__closure_h_") == NULL &&
        strstr(actual_text, "call pickh__closure_k_") == NULL &&
        strstr(actual_text, "__fv_1_add1") == NULL &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call __fnwrap_add1(0,") == 3u;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR second-order returned passthrough dynamic local function-value parameter forwarding mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_second_order_returned_passthrough_dynamic_local_function_value_parameter_forwarding_with_unary_helper_eval_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int idh(int h(int f(int), int x))(int f(int), int x){ return h; }\n"
            "int pickh(int base, int c)(int f(int), int x){ int h(int f(int), int x)=closure [base] int (int g(int), int y){ return -g(y) + base; }; int k(int f(int), int x)=closure [base] int (int g(int), int y){ return -g(g(y)) + base; }; if(c) h=k; return h; }\n"
            "int add1(int x){ return x+1; }\n"
            "int main(){ return idh(pickh(5, getint()))(add1, 41); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call pickh(tmp.0, tmp.1, 5, tmp.2)\n") != NULL &&
        strstr(actual_text, "call pickh__closure_h_") == NULL &&
        strstr(actual_text, "call pickh__closure_k_") == NULL &&
        strstr(actual_text, "__fv_1_add1") == NULL &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call __fnwrap_add1(0,") == 3u &&
        lower_ir_test_count_fragment_occurrences(actual_text, "sub 0, tmp.") == 2u;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR second-order returned passthrough dynamic local function-value unary helper-eval mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_second_order_returned_passthrough_dynamic_local_function_value_parameter_forwarding_with_compound_update_helper_eval_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int idh(int h(int f(int), int x))(int f(int), int x){ return h; }\n"
            "int pickh(int base, int c)(int f(int), int x){ int h(int f(int), int x)=closure [base] int (int g(int), int y){ int z=y; z+=1; return g(z) + base; }; int k(int f(int), int x)=closure [base] int (int g(int), int y){ int z=y; z+=1; return g(g(z)) + base; }; if(c) h=k; return h; }\n"
            "int add1(int x){ return x+1; }\n"
            "int main(){ return idh(pickh(5, getint()))(add1, 41); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call pickh(tmp.0, tmp.1, 5, tmp.2)\n") != NULL &&
        strstr(actual_text, "call pickh__closure_h_") == NULL &&
        strstr(actual_text, "call pickh__closure_k_") == NULL &&
        strstr(actual_text, "__fv_1_add1") == NULL &&
        lower_ir_test_count_fragment_occurrences(actual_text, "add 41, 1") == 2u &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call __fnwrap_add1(0,") == 3u;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR second-order returned passthrough dynamic local function-value compound-update helper-eval mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_second_order_returned_passthrough_dynamic_local_function_value_parameter_forwarding_with_comma_helper_eval_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int idh(int h(int f(int), int x))(int f(int), int x){ return h; }\n"
            "int pickh(int base, int c)(int f(int), int x){ int h(int f(int), int x)=closure [base] int (int g(int), int y){ return (g(y), g(y) + base); }; int k(int f(int), int x)=closure [base] int (int g(int), int y){ return (g(g(y)), g(g(y)) + base); }; if(c) h=k; return h; }\n"
            "int add1(int x){ return x+1; }\n"
            "int main(){ return idh(pickh(5, getint()))(add1, 41); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call pickh__closure_h_") == NULL &&
        strstr(actual_text, "call pickh__closure_k_") == NULL &&
        strstr(actual_text, "__fv_1_add1") == NULL &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call __fnwrap_add1(0,") == 6u;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR second-order returned passthrough dynamic local function-value comma helper-eval mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_second_order_returned_passthrough_dynamic_local_function_value_parameter_forwarding_with_ternary_helper_eval_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int idh(int h(int f(int), int x))(int f(int), int x){ return h; }\n"
            "int pickh(int base, int c)(int f(int), int x){ int h(int f(int), int x)=closure [base] int (int g(int), int y){ return g(y) ? g(y) + base : base; }; int k(int f(int), int x)=closure [base] int (int g(int), int y){ return g(g(y)) ? g(g(y)) + base : base; }; if(c) h=k; return h; }\n"
            "int add1(int x){ return x+1; }\n"
            "int main(){ return idh(pickh(5, getint()))(add1, 41); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call pickh__closure_h_") == NULL &&
        strstr(actual_text, "call pickh__closure_k_") == NULL &&
        strstr(actual_text, "__fv_1_add1") == NULL &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call __fnwrap_add1(0,") == 6u;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR second-order returned passthrough dynamic local function-value ternary helper-eval mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_second_order_returned_passthrough_dynamic_local_function_value_parameter_forwarding_with_statement_call_prefix_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int idh(int h(int f(int), int x))(int f(int), int x){ return h; }\n"
            "int pickh(int base, int c)(int f(int), int x){ int h(int f(int), int x)=closure [base] int (int g(int), int y){ g(y); return g(y) + base; }; int k(int f(int), int x)=closure [base] int (int g(int), int y){ g(g(y)); return g(g(y)) + base; }; if(c) h=k; return h; }\n"
            "int add1(int x){ return x+1; }\n"
            "int main(){ return idh(pickh(5, getint()))(add1, 41); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call pickh__closure_h_") == NULL &&
        strstr(actual_text, "call pickh__closure_k_") == NULL &&
        strstr(actual_text, "__fv_1_add1") == NULL &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call __fnwrap_add1(0,") == 6u;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR second-order returned passthrough dynamic local function-value statement-call-prefix helper-eval mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_second_order_returned_passthrough_dynamic_local_function_value_parameter_forwarding_with_if_return_helper_eval_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int idh(int h(int f(int), int x))(int f(int), int x){ return h; }\n"
            "int pickh(int base, int c)(int f(int), int x){ int h(int f(int), int x)=closure [base] int (int g(int), int y){ if(g(y)) return g(y) + base; return base; }; int k(int f(int), int x)=closure [base] int (int g(int), int y){ if(g(g(y))) return g(g(y)) + base; return base; }; if(c) h=k; return h; }\n"
            "int add1(int x){ return x+1; }\n"
            "int main(){ return idh(pickh(5, getint()))(add1, 41); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call pickh__closure_h_") == NULL &&
        strstr(actual_text, "call pickh__closure_k_") == NULL &&
        strstr(actual_text, "__fv_1_add1") == NULL &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call __fnwrap_add1(0,") == 6u;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR second-order returned passthrough dynamic local function-value if-return helper-eval mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_second_order_returned_closure_function_value_parameter_forwarding_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "func make__retclosure_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_make__retclosure_") != NULL &&
        strstr(actual_text, "call pass(") == NULL &&
        strstr(actual_text, "pass__fv_0_apply_1_make__retclosure_") == NULL &&
        strstr(actual_text, "apply__fv_0_make__retclosure_") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR second-order returned closure function-value parameter forwarding mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_passthrough_decl_local_function_value_forwarding_without_specialization_shell_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "call __fnwrap_add1(0, 41)\n") != NULL &&
        strstr(actual_text, "func __fnwrap_add1(__env.0, x.1) {\n") != NULL &&
        strstr(actual_text, "wrapper__fv_0_add1") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR passthrough decl-local function-value forwarding mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_parameter_local_function_value_direct_call_without_specialization_shell_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int test(int f(int), int x){ int g(int)=f; return g(x); }\n"
            "int main(){ return test(add1, 41); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare test(f.0, x.1)\n") != NULL &&
        strstr(actual_text, "call __fnwrap_add1(0, 41)\n") != NULL &&
        strstr(actual_text, "func __fnwrap_add1(__env.0, x.1) {\n") != NULL &&
        strstr(actual_text, "test__fv_0_add1") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR parameter-local function-value direct call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_parameter_local_scalar_rebind_function_value_direct_call_without_specialization_shell_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int test(int f(int), int x){ int y=x; return f(y); }\n"
            "int main(){ return test(add1, 41); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare test(f.0, x.1)\n") != NULL &&
        strstr(actual_text, "call __fnwrap_add1(0, 41)\n") != NULL &&
        strstr(actual_text, "store_local y.") == NULL &&
        strstr(actual_text, "test__fv_0_add1") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR parameter-local scalar rebind function-value direct call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_parameter_local_scalar_and_alias_function_value_direct_call_without_specialization_shell_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int test(int f(int), int x){ int y=x; int g(int)=f; return g(y); }\n"
            "int main(){ return test(add1, 41); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare test(f.0, x.1)\n") != NULL &&
        strstr(actual_text, "call __fnwrap_add1(0, 41)\n") != NULL &&
        strstr(actual_text, "store_local y.") == NULL &&
        strstr(actual_text, "store_local g$ftag.") == NULL &&
        strstr(actual_text, "test__fv_0_add1") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR parameter-local scalar+alias function-value direct call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_parameter_local_scalar_update_function_value_direct_call_without_specialization_shell_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int test(int f(int), int x){ int y=x; y=y+1; return f(y); }\n"
            "int main(){ return test(add1, 41); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare test(f.0, x.1)\n") != NULL &&
        strstr(actual_text, "call __fnwrap_add1(0,") != NULL &&
        strstr(actual_text, "test__fv_0_add1") == NULL &&
        strstr(actual_text, "func test__fv_0_add1") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR parameter-local scalar update function-value direct call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_parameter_local_scalar_update_and_alias_function_value_direct_call_without_specialization_shell_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int test(int f(int), int x){ int y=x; y=y+1; int g(int)=f; g=f; return g(y); }\n"
            "int main(){ return test(add1, 41); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare test(f.0, x.1)\n") != NULL &&
        strstr(actual_text, "call __fnwrap_add1(0,") != NULL &&
        strstr(actual_text, "test__fv_0_add1") == NULL &&
        strstr(actual_text, "func test__fv_0_add1") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR parameter-local scalar update+alias function-value direct call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_parameter_local_call_update_function_value_direct_call_without_specialization_shell_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int test(int f(int), int x){ int y=x; y=f(y); return y; }\n"
            "int main(){ return test(add1, 41); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare test(f.0, x.1)\n") != NULL &&
        strstr(actual_text, "call __fnwrap_add1(0,") != NULL &&
        strstr(actual_text, "test__fv_0_add1") == NULL &&
        strstr(actual_text, "func test__fv_0_add1") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR parameter-local call update function-value direct call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_parameter_local_repeated_call_update_function_value_direct_call_without_specialization_shell_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int test(int f(int), int x){ int y=x; y=f(y); y=f(y); return y; }\n"
            "int main(){ return test(add1, 41); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare test(f.0, x.1)\n") != NULL &&
        lower_ir_test_count_fragment_occurrences(actual_text, "call __fnwrap_add1(0,") == 2u &&
        strstr(actual_text, "test__fv_0_add1") == NULL &&
        strstr(actual_text, "func test__fv_0_add1") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR parameter-local repeated call update function-value direct call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_parameter_local_two_callable_update_without_specialization_shell_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "call __fnwrap_double1(0, 20)\n") != NULL &&
        strstr(actual_text, "call __fnwrap_add1(0,") != NULL &&
        strstr(actual_text, "compose__fv_0_add1_1_double1") == NULL &&
        strstr(actual_text, "test__fv_0_add1_1_double1") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR parameter-local two-callable update mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_parameter_local_zero_arg_function_value_direct_call_without_specialization_shell_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int next(){ return 7; }\n"
            "int test(int f()){ int g()=f; return g(); }\n"
            "int main(){ return test(next); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare test(f.0)\n") != NULL &&
        strstr(actual_text, "call __fnwrap_next(0)\n") != NULL &&
        strstr(actual_text, "func __fnwrap_next(__env.0) {\n") != NULL &&
        strstr(actual_text, "test__fv_0_next") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR parameter-local zero-arg function-value direct call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_parameter_local_zero_arg_void_function_value_direct_call_without_specialization_shell_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "void ping(){ putint(7); return; }\n"
            "int test(void f()){ void g()=f; g(); return 0; }\n"
            "int main(){ return test(ping); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare test(f.0)\n") != NULL &&
        strstr(actual_text, "call __fnwrap_ping(0)\n") != NULL &&
        strstr(actual_text, "func __fnwrap_ping(__env.0) {\n") != NULL &&
        strstr(actual_text, "test__fv_0_ping") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR parameter-local zero-arg void function-value direct call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_parameter_local_closure_direct_call_without_specialization_shell_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int test(int f(int), int x){ int g(int)=f; return g(x); }\n"
            "int main(){ int y=3; int f(int)=closure [y] int (int z){ return y+z; }; return test(f, 4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare test(f.0, x.1)\n") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "call main__closure_f_") != NULL &&
        strstr(actual_text, "test__fv_0_main__closure_f_") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR parameter-local closure direct call mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_parameter_local_closure_forward_without_specialization_shell_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "call __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "call main__closure_f_") != NULL &&
        strstr(actual_text, "test__fv_0_main__closure_f_") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR parameter-local closure forwarding mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_parameter_local_zero_arg_void_function_value_forward_without_specialization_shell_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "call __fnwrap_ping(0)\n") != NULL &&
        strstr(actual_text, "func __fnwrap_ping(__env.0) {\n") != NULL &&
        strstr(actual_text, "test__fv_0_ping") == NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR parameter-local zero-arg void function-value forwarding mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_local_zero_arg_function_value_forwarding_into_function_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "store_local g$ftag.1, 1\n") != NULL &&
        strstr(actual_text, "call putint(0)\n") != NULL &&
        strstr(actual_text, "store_local g$ftag.1, 2\n") != NULL &&
        strstr(actual_text, "load_local g$ftag.1\n") != NULL &&
        strstr(actual_text, "call __fnwrap_next1(0)\n") != NULL &&
        strstr(actual_text, "call __fnwrap_next2(0)\n") != NULL &&
        strstr(actual_text, "func __fnwrap_next1(__env.0) {\n") != NULL &&
        strstr(actual_text, "func __fnwrap_next2(__env.0) {\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-ZERO-FNVAL-FORWARD mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_local_zero_arg_void_function_value_forwarding_into_function_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "store_local g$ftag.1, 1\n") != NULL &&
        strstr(actual_text, "call putint(0)\n") != NULL &&
        strstr(actual_text, "store_local g$ftag.1, 2\n") != NULL &&
        strstr(actual_text, "load_local g$ftag.1\n") != NULL &&
        strstr(actual_text, "call __fnwrap_ping1(0)\n") != NULL &&
        strstr(actual_text, "call __fnwrap_ping2(0)\n") != NULL &&
        strstr(actual_text, "func __fnwrap_ping1(__env.0) {\n") != NULL &&
        strstr(actual_text, "func __fnwrap_ping2(__env.0) {\n") != NULL &&
        strstr(actual_text, "ret 0\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-ZERO-VOID-FNVAL-FORWARD mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_runtime_closure_local_forward_into_function_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int apply(int f(int), int x){ return f(x); }\n"
            "int main(){ int c=1; int x=3; int y=5; int f(int)=closure [x] int (int z) { return x + z; }; int g(int)=closure [y] int (int z) { return y + z; }; if((putint(0), c)) f=g; return apply(f, 4); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply(f.0, x.1)\n") != NULL &&
        strstr(actual_text, "call putint(0)\n") != NULL &&
        strstr(actual_text, "store_local f$closurecap$0.3, tmp.") != NULL &&
        strstr(actual_text, "store_local f$ftag.4, 2\n") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_g_") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-RUNTIME-CLOSURE-FORWARD mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_runtime_zero_arg_void_closure_local_forward_into_function_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "void apply0(void f()){ f(); }\n"
            "int main(){ int c=1; int x=7; int y=9; void f()=closure [x] void () { putint(x); return; }; void g()=closure [y] void () { putint(y); return; }; if((putint(0), c)) f=g; apply0(f); return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare apply0(f.0)\n") != NULL &&
        strstr(actual_text, "call putint(0)\n") != NULL &&
        strstr(actual_text, "store_local f$closurecap$0.3, tmp.") != NULL &&
        strstr(actual_text, "store_local f$ftag.4, 2\n") != NULL &&
        strstr(actual_text, "load_local f$ftag.4\n") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "call __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_f_") != NULL &&
        strstr(actual_text, "func __fnwrap_closure_main__closure_g_") != NULL &&
        strstr(actual_text, "ret 0\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-RUNTIME-ZERO-VOID-CLOSURE-FORWARD mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_dynamic_function_value_assignment_then_forward_into_function_parameter_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int add1(int x){ return x+1; }\n"
            "int add2(int x){ return x+2; }\n"
            "int apply(int f(int), int x){ return f(x); }\n"
            "int main(){ int c=1; int f(int)=add1; int h(int)=add1; if((putint(0), c)) f=add2; h=f; return apply(h, 40); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "store_local f$ftag.1, 1\n") != NULL &&
        strstr(actual_text, "store_local h$ftag.2, 1\n") != NULL &&
        strstr(actual_text, "call putint(0)\n") != NULL &&
        strstr(actual_text, "store_local f$ftag.1, 2\n") != NULL &&
        strstr(actual_text, "load_local f$ftag.1\n") != NULL &&
        strstr(actual_text, "store_local h$ftag.2, tmp.") != NULL &&
        strstr(actual_text, "load_local h$ftag.2\n") != NULL &&
        strstr(actual_text, "eq tmp.") != NULL &&
        strstr(actual_text, "call __fnwrap_add1(0, 40)\n") != NULL &&
        strstr(actual_text, "call __fnwrap_add2(0, 40)\n") != NULL &&
        strstr(actual_text, "func __fnwrap_add1(__env.0, x.1) {\n") != NULL &&
        strstr(actual_text, "func __fnwrap_add2(__env.0, x.1) {\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-DYNAMIC-FNVAL-ASSIGN-FORWARD mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_float_helper_wrapped_ternary_call_compare_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-HELPER-TERNARY-CALL-COMPARE-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_unary_call_helper_wrapped_ternary_call_compare_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "tmp.0 = call pick(tmp.8)\n") != NULL &&
        strstr(actual_text, "tmp.1 = eq tmp.4, tmp.7\n") != NULL &&
        strstr(actual_text, "ret tmp.1\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-UNARY-HELPER-TERNARY-CALL-COMPARE-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_float_equality_compare_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int eq(float x, float y){ return x == y; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func eq(x.0:float, y.1:float) {\n") != NULL &&
        strstr(actual_text, "load_local x.0") != NULL &&
        strstr(actual_text, "load_local y.1") != NULL &&
        strstr(actual_text, "and tmp.") != NULL &&
        strstr(actual_text, "ne tmp.") != NULL &&
        strstr(actual_text, "mul tmp.") != NULL &&
        strstr(actual_text, "eq tmp.") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-EQ-COMPARE-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_float_inequality_compare_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int ne(float x, float y){ return x != y; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func ne(x.0:float, y.1:float) {\n") != NULL &&
        strstr(actual_text, "load_local x.0") != NULL &&
        strstr(actual_text, "load_local y.1") != NULL &&
        strstr(actual_text, "and tmp.") != NULL &&
        strstr(actual_text, "ne tmp.") != NULL &&
        strstr(actual_text, "mul tmp.") != NULL &&
        strstr(actual_text, "ne tmp.") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-NE-COMPARE-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_float_relational_compare_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int lt(float x, float y){ return x < y; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func lt(x.0:float, y.1:float) {\n") != NULL &&
        strstr(actual_text, "load_local x.0") != NULL &&
        strstr(actual_text, "load_local y.1") != NULL &&
        strstr(actual_text, "and tmp.") != NULL &&
        strstr(actual_text, "ne tmp.") != NULL &&
        strstr(actual_text, "mul tmp.") != NULL &&
        strstr(actual_text, "shr tmp.") != NULL &&
        strstr(actual_text, "or tmp.") != NULL &&
        strstr(actual_text, "xor tmp.") != NULL &&
        strstr(actual_text, "lt tmp.") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL;

    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-LT-COMPARE-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_chained_float_equality_compare_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-CHAIN-ADD-EQ-COMPARE-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_chained_float_relational_compare_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-CHAIN-ADD-LT-COMPARE-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_nested_muldiv_float_equality_compare_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int eq(float a, float b, float c){ return (-a * (b / c)) == c; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func eq(a.0:float, b.1:float, c.2:float) {\n") != NULL &&
        strstr(actual_text, "__builtin_fdiv32") != NULL &&
        strstr(actual_text, "__builtin_fmul32") != NULL &&
        strstr(actual_text, "eq tmp.") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-NESTED-MUL-DIV-EQ-COMPARE-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_nested_muldiv_float_relational_compare_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int lt(float a, float b, float c){ return (-a * (b / c)) < c; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func lt(a.0:float, b.1:float, c.2:float) {\n") != NULL &&
        strstr(actual_text, "__builtin_fdiv32") != NULL &&
        strstr(actual_text, "__builtin_fmul32") != NULL &&
        strstr(actual_text, "lt tmp.") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-NESTED-MUL-DIV-LT-COMPARE-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_chained_float_inequality_compare_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-CHAIN-ADD-NE-COMPARE-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_chained_float_le_compare_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-CHAIN-ADD-LE-COMPARE-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_nested_muldiv_float_inequality_compare_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int ne(float a, float b, float c){ return (-a * (b / c)) != c; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func ne(a.0:float, b.1:float, c.2:float) {\n") != NULL &&
        strstr(actual_text, "__builtin_fdiv32") != NULL &&
        strstr(actual_text, "__builtin_fmul32") != NULL &&
        strstr(actual_text, "ne tmp.") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-NESTED-MUL-DIV-NE-COMPARE-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_nested_muldiv_float_ge_compare_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int ge(float a, float b, float c){ return (-a * (b / c)) >= c; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func ge(a.0:float, b.1:float, c.2:float) {\n") != NULL &&
        strstr(actual_text, "__builtin_fdiv32") != NULL &&
        strstr(actual_text, "__builtin_fmul32") != NULL &&
        strstr(actual_text, "ge tmp.") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-NESTED-MUL-DIV-GE-COMPARE-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_negative_float_literal_transport_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-NEG-LITERAL-TRANSPORT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_unary_minus_float_identifier_transport_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "float g = 1.25;\n"
            "float neg(){ return -g; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func neg() {\n") != NULL &&
        strstr(actual_text, "load_global g.0") != NULL &&
        strstr(actual_text, "xor tmp.") != NULL &&
        strstr(actual_text, "2147483648") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-NEG-IDENT-TRANSPORT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_negative_zero_float_condition_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-NEG-ZERO-COND-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_negative_float_relational_compare_against_zero_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "and 0, 2147483647\n") != NULL &&
        strstr(actual_text, "mul 0, tmp.") != NULL &&
        strstr(actual_text, "or tmp.") != NULL &&
        strstr(actual_text, "xor tmp.") != NULL &&
        strstr(actual_text, "lt tmp.") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-NEG-LT-ZERO-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_negative_zero_le_zero_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-NEG-ZERO-LE-ZERO-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_float_addition_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "float add(float x, float y){ return x + y; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func add(x.0:float, y.1:float) {\n") != NULL &&
        strstr(actual_text, "load_local x.0") != NULL &&
        strstr(actual_text, "load_local y.1") != NULL &&
        strstr(actual_text, "call __builtin_fadd32(") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL &&
        strstr(actual_text, "declare __builtin_fadd32(param0.0:float, param1.1:float)\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-ADD-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_float_subtraction_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "float sub(float x, float y){ return x - y; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func sub(x.0:float, y.1:float) {\n") != NULL &&
        strstr(actual_text, "load_local x.0") != NULL &&
        strstr(actual_text, "load_local y.1") != NULL &&
        strstr(actual_text, "call __builtin_fsub32(") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL &&
        strstr(actual_text, "declare __builtin_fsub32(param0.0:float, param1.1:float)\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-SUB-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_explicit_int_from_float_conversion_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int conv(float x, float y){ return int(x + y); }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare __builtin_fadd32(param0.0:float, param1.1:float)\n") != NULL &&
        strstr(actual_text, "declare __builtin_f2i32(param0.0:float)\n") != NULL &&
        strstr(actual_text, "call __builtin_fadd32(") != NULL &&
        strstr(actual_text, "call __builtin_f2i32(") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-EXPLICIT-INT-FROM-FLOAT-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_explicit_float_from_int_conversion_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "float conv(int x, int y){ return float(x + y); }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare __builtin_i2f32(param0.0)\n") != NULL &&
        strstr(actual_text, "add tmp.") != NULL &&
        strstr(actual_text, "call __builtin_i2f32(") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-EXPLICIT-FLOAT-FROM-INT-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_explicit_float_condition_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int main(){ if(float(3)) return 1; return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare __builtin_i2f32(param0.0)\n") != NULL &&
        strstr(actual_text, "call __builtin_i2f32(") != NULL &&
        strstr(actual_text, "2147483647") != NULL &&
        strstr(actual_text, "br tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-CONVERT-COND-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_recursive_explicit_float_condition_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int add3(int a, int b, int c){ return (a + b) + c; }\n"
            "int main(){ if(float(add3(1, 2, 3))) return 1; return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func add3(a.0, b.1, c.2) {\n") != NULL &&
        strstr(actual_text, "add tmp.") != NULL &&
        strstr(actual_text, "declare __builtin_i2f32(param0.0)\n") != NULL &&
        strstr(actual_text, "call __builtin_i2f32(") != NULL &&
        strstr(actual_text, "2147483647") != NULL &&
        strstr(actual_text, "br tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-CONVERT-RECURSIVE-COND-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_explicit_float_while_condition_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int main(){ while(float(3)) return 1; return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare __builtin_i2f32(param0.0)\n") != NULL &&
        strstr(actual_text, "jmp bb.1\n") != NULL &&
        strstr(actual_text, "call __builtin_i2f32(") != NULL &&
        strstr(actual_text, "2147483647") != NULL &&
        strstr(actual_text, "br tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-CONVERT-WHILE-COND-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_explicit_float_logical_condition_composition_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int add3(int a, int b, int c){ return (a + b) + c; }\n"
            "int main(){ if(!float(0) || (float(3) && float(add3(1, 2, 3)))) return 1; return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare __builtin_i2f32(param0.0)\n") != NULL &&
        strstr(actual_text, "call __builtin_i2f32(") != NULL &&
        strstr(actual_text, "call add3(1, 2, 3)\n") != NULL &&
        strstr(actual_text, "2147483647") != NULL &&
        strstr(actual_text, "br tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-CONVERT-LOGIC-COND-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_explicit_int_from_float_ternary_bridge_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-EXPLICIT-INT-FROM-FLOAT-TERNARY-BRIDGE-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_explicit_int_from_recursive_float_call_argument_bridge_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-EXPLICIT-INT-FROM-RECURSIVE-CALLARG-BRIDGE-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_explicit_int_from_recursive_float_initializer_bridge_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "store_global x.0") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-EXPLICIT-INT-FROM-RECURSIVE-INIT-BRIDGE-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_explicit_int_from_recursive_float_assignment_bridge_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "float add3(float a, float b, float c){ return (a + b) + c; }\n"
            "int main(){ int x=0; x = int(add3(1.0, 2.0, 3.0)); return x; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call add3(") != NULL &&
        strstr(actual_text, "call __builtin_f2i32(") != NULL &&
        strstr(actual_text, "store_local x.0") != NULL &&
        strstr(actual_text, "load_local x.0") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-EXPLICIT-INT-FROM-RECURSIVE-ASSIGN-BRIDGE-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_explicit_int_from_float_compare_bridge_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "float g = 1.25;\n"
            "float h = 2.5;\n"
            "int main(){ return int(g ? h : h) == 2; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call __builtin_f2i32(") != NULL &&
        strstr(actual_text, "eq tmp.") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-EXPLICIT-INT-FROM-FLOAT-COMPARE-BRIDGE-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_explicit_int_from_recursive_float_arithmetic_bridge_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "float add3(float a, float b, float c){ return (a + b) + c; }\n"
            "int main(){ return int(add3(1.0, 2.0, 3.0)) + 1; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call add3(") != NULL &&
        strstr(actual_text, "call __builtin_f2i32(") != NULL &&
        strstr(actual_text, "add tmp.") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-EXPLICIT-INT-FROM-RECURSIVE-ARITH-BRIDGE-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_explicit_float_from_recursive_int_initializer_bridge_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
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
        strstr(actual_text, "call add3(") != NULL &&
        strstr(actual_text, "call __builtin_i2f32(") != NULL &&
        strstr(actual_text, "store_global z.0") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-EXPLICIT-FLOAT-FROM-RECURSIVE-INIT-BRIDGE-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_explicit_float_from_recursive_int_assignment_bridge_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int add3(int a, int b, int c){ return (a + b) + c; }\n"
            "float mainf(){ float y; y = float(add3(1, 2, 3)); return y; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call add3(") != NULL &&
        strstr(actual_text, "call __builtin_i2f32(") != NULL &&
        strstr(actual_text, "store_local y.0") != NULL &&
        strstr(actual_text, "load_local y.0") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-EXPLICIT-FLOAT-FROM-RECURSIVE-ASSIGN-BRIDGE-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_explicit_float_from_int_compare_bridge_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int add3(int a, int b, int c){ return (a + b) + c; }\n"
            "int main(){ return float(add3(1, 2, 3)) == float(6); }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call __builtin_i2f32(") != NULL &&
        strstr(actual_text, "eq tmp.") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-EXPLICIT-FLOAT-FROM-INT-COMPARE-BRIDGE-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_explicit_float_from_recursive_int_arithmetic_bridge_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "int add3(int a, int b, int c){ return (a + b) + c; }\n"
            "float mainf(){ return float(add3(1, 2, 3)) + 1.25; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "call __builtin_i2f32(") != NULL &&
        strstr(actual_text, "add tmp.1, 1067450368") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-EXPLICIT-FLOAT-FROM-RECURSIVE-ARITH-BRIDGE-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_negative_float_addition_combo_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "float g = 1.25;\n"
            "float add(float y){ return -g + y; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func add(y.0:float) {\n") != NULL &&
        strstr(actual_text, "load_global g.0") != NULL &&
        strstr(actual_text, "xor tmp.") != NULL &&
        strstr(actual_text, "__builtin_fadd32") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-NEG-ADD-COMBO-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_negative_float_subtraction_combo_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "float g = 1.25;\n"
            "float sub(float y){ return y - -g; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func sub(y.0:float) {\n") != NULL &&
        strstr(actual_text, "load_global g.0") != NULL &&
        strstr(actual_text, "xor tmp.") != NULL &&
        strstr(actual_text, "__builtin_fsub32") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-NEG-SUB-COMBO-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_float_multiplication_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "float mul(float x, float y){ return x * y; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare __builtin_fmul32(param0.0:float, param1.1:float)\n") != NULL &&
        strstr(actual_text, "func mul(x.0:float, y.1:float) {\n") != NULL &&
        strstr(actual_text, "load_local x.0") != NULL &&
        strstr(actual_text, "load_local y.1") != NULL &&
        strstr(actual_text, "call __builtin_fmul32(") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-MUL-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_float_division_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "float divv(float x, float y){ return x / y; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "declare __builtin_fdiv32(param0.0:float, param1.1:float)\n") != NULL &&
        strstr(actual_text, "func divv(x.0:float, y.1:float) {\n") != NULL &&
        strstr(actual_text, "load_local x.0") != NULL &&
        strstr(actual_text, "load_local y.1") != NULL &&
        strstr(actual_text, "call __builtin_fdiv32(") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-DIV-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_negative_float_multiplication_combo_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "float g = 1.25;\n"
            "float mul(float y){ return -g * y; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func mul(y.0:float) {\n") != NULL &&
        strstr(actual_text, "load_global g.0") != NULL &&
        strstr(actual_text, "xor tmp.") != NULL &&
        strstr(actual_text, "load_local y.0") != NULL &&
        strstr(actual_text, "__builtin_fmul32") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-NEG-MUL-COMBO-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_negative_float_division_combo_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "float g = 1.25;\n"
            "float divv(float y){ return y / -g; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func divv(y.0:float) {\n") != NULL &&
        strstr(actual_text, "load_global g.0") != NULL &&
        strstr(actual_text, "xor tmp.") != NULL &&
        strstr(actual_text, "load_local y.0") != NULL &&
        strstr(actual_text, "__builtin_fdiv32") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-NEG-DIV-COMBO-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_chained_float_addition_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "float add3(float x, float y, float z){ return (x + y) + z; }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func add3(x.0:float, y.1:float, z.2:float) {\n") != NULL &&
        strstr(actual_text, "call __builtin_fadd32(") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-CHAIN-ADD-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_accepts_nested_float_mul_div_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "float f(float a, float b, float c){ return -a * (b / c); }\n"
            "int main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "func f(a.0:float, b.1:float, c.2:float) {\n") != NULL &&
        strstr(actual_text, "xor tmp.") != NULL &&
        strstr(actual_text, "__builtin_fdiv32") != NULL &&
        strstr(actual_text, "__builtin_fmul32") != NULL &&
        strstr(actual_text, "ret tmp.") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-NESTED-MUL-DIV-ACCEPT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
}

static int test_lower_ir_rejects_mixed_float_int_arithmetic_under_extension(void) {
    TokenArray tokens;
    AstProgram ast_program;
    ParserError parse_err;
    SemanticError sema_err;
    SemanticOptions sema_options;
    int ok = 0;

    lexer_init_tokens(&tokens);
    ast_program_init(&ast_program);
    memset(&parse_err, 0, sizeof(parse_err));
    memset(&sema_err, 0, sizeof(sema_err));
    memset(&sema_options, 0, sizeof(sema_options));
    sema_options.allow_extension_features = 1;

    ok = lexer_tokenize(
            "float add(float x){ return x + 1; }\n"
            "int main(){ return 0; }\n",
            &tokens) &&
        parser_parse_translation_unit_ast(&tokens, &ast_program, &parse_err) &&
        !semantic_analyze_program_with_options(&ast_program, &sema_options, &sema_err) &&
        strstr(sema_err.message, "SEMA-TYPE-008") != NULL;

    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-ARITH-INT-TYPE-REJECT mismatch: parse='%s' sema='%s'\n",
            parse_err.message,
            sema_err.message);
    }

    ast_program_free(&ast_program);
    lexer_free_tokens(&tokens);
    return ok;
}

static int test_lower_ir_rejects_float_literal_int_arithmetic_under_extension(void) {
    TokenArray tokens;
    AstProgram ast_program;
    ParserError parse_err;
    SemanticError sema_err;
    SemanticOptions sema_options;
    int ok = 0;

    lexer_init_tokens(&tokens);
    ast_program_init(&ast_program);
    memset(&parse_err, 0, sizeof(parse_err));
    memset(&sema_err, 0, sizeof(sema_err));
    memset(&sema_options, 0, sizeof(sema_options));
    sema_options.allow_extension_features = 1;

    ok = lexer_tokenize(
            "float add(){ return 1.25 + 1; }\n"
            "int main(){ return 0; }\n",
            &tokens) &&
        parser_parse_translation_unit_ast(&tokens, &ast_program, &parse_err) &&
        !semantic_analyze_program_with_options(&ast_program, &sema_options, &sema_err) &&
        strstr(sema_err.message, "SEMA-TYPE-008") != NULL;

    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-LITERAL-ARITH-INT-TYPE-REJECT mismatch: parse='%s' sema='%s'\n",
            parse_err.message,
            sema_err.message);
    }

    ast_program_free(&ast_program);
    lexer_free_tokens(&tokens);
    return ok;
}

static int test_lower_ir_rejects_float_call_int_arithmetic_under_extension(void) {
    TokenArray tokens;
    AstProgram ast_program;
    ParserError parse_err;
    SemanticError sema_err;
    SemanticOptions sema_options;
    int ok = 0;

    lexer_init_tokens(&tokens);
    ast_program_init(&ast_program);
    memset(&parse_err, 0, sizeof(parse_err));
    memset(&sema_err, 0, sizeof(sema_err));
    memset(&sema_options, 0, sizeof(sema_options));
    sema_options.allow_extension_features = 1;

    ok = lexer_tokenize(
            "float id(float x){ return x; }\n"
            "float add(float x){ return id(x) + 1; }\n"
            "int main(){ return 0; }\n",
            &tokens) &&
        parser_parse_translation_unit_ast(&tokens, &ast_program, &parse_err) &&
        !semantic_analyze_program_with_options(&ast_program, &sema_options, &sema_err) &&
        strstr(sema_err.message, "SEMA-TYPE-008") != NULL;

    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-CALL-ARITH-INT-TYPE-REJECT mismatch: parse='%s' sema='%s'\n",
            parse_err.message,
            sema_err.message);
    }

    ast_program_free(&ast_program);
    lexer_free_tokens(&tokens);
    return ok;
}

static int test_lower_ir_rejects_negative_float_call_int_arithmetic_under_extension(void) {
    TokenArray tokens;
    AstProgram ast_program;
    ParserError parse_err;
    SemanticError sema_err;
    SemanticOptions sema_options;
    int ok = 0;

    lexer_init_tokens(&tokens);
    ast_program_init(&ast_program);
    memset(&parse_err, 0, sizeof(parse_err));
    memset(&sema_err, 0, sizeof(sema_err));
    memset(&sema_options, 0, sizeof(sema_options));
    sema_options.allow_extension_features = 1;

    ok = lexer_tokenize(
            "float id(float x){ return x; }\n"
            "float add(float x){ return -id(x) * 1; }\n"
            "int main(){ return 0; }\n",
            &tokens) &&
        parser_parse_translation_unit_ast(&tokens, &ast_program, &parse_err) &&
        !semantic_analyze_program_with_options(&ast_program, &sema_options, &sema_err) &&
        strstr(sema_err.message, "SEMA-TYPE-008") != NULL;

    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-NEG-CALL-ARITH-INT-TYPE-REJECT mismatch: parse='%s' sema='%s'\n",
            parse_err.message,
            sema_err.message);
    }

    ast_program_free(&ast_program);
    lexer_free_tokens(&tokens);
    return ok;
}

static int test_lower_ir_rejects_global_float_int_condition_under_extension(void) {
    TokenArray tokens;
    AstProgram ast_program;
    ParserError parse_err;
    SemanticError sema_err;
    SemanticOptions sema_options;
    int ok = 0;

    lexer_init_tokens(&tokens);
    ast_program_init(&ast_program);
    memset(&parse_err, 0, sizeof(parse_err));
    memset(&sema_err, 0, sizeof(sema_err));
    memset(&sema_options, 0, sizeof(sema_options));
    sema_options.allow_extension_features = 1;

    ok = lexer_tokenize(
            "float g = 1.25;\n"
            "int main(){ if(g + 1) return 1; return 0; }\n",
            &tokens) &&
        parser_parse_translation_unit_ast(&tokens, &ast_program, &parse_err) &&
        !semantic_analyze_program_with_options(&ast_program, &sema_options, &sema_err) &&
        strstr(sema_err.message, "SEMA-EXT-035") != NULL;

    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-GLOBAL-COND-PLUS-INT-REJECT mismatch: parse='%s' sema='%s'\n",
            parse_err.message,
            sema_err.message);
    }

    ast_program_free(&ast_program);
    lexer_free_tokens(&tokens);
    return ok;
}

static int test_lower_ir_rejects_float_call_int_condition_under_extension(void) {
    TokenArray tokens;
    AstProgram ast_program;
    ParserError parse_err;
    SemanticError sema_err;
    SemanticOptions sema_options;
    int ok = 0;

    lexer_init_tokens(&tokens);
    ast_program_init(&ast_program);
    memset(&parse_err, 0, sizeof(parse_err));
    memset(&sema_err, 0, sizeof(sema_err));
    memset(&sema_options, 0, sizeof(sema_options));
    sema_options.allow_extension_features = 1;

    ok = lexer_tokenize(
            "float id(float x){ return x; }\n"
            "int main(){ if(id(1.0) + 1) return 1; return 0; }\n",
            &tokens) &&
        parser_parse_translation_unit_ast(&tokens, &ast_program, &parse_err) &&
        !semantic_analyze_program_with_options(&ast_program, &sema_options, &sema_err) &&
        strstr(sema_err.message, "SEMA-TYPE-008") != NULL;

    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-CALL-COND-PLUS-INT-REJECT mismatch: parse='%s' sema='%s'\n",
            parse_err.message,
            sema_err.message);
    }

    ast_program_free(&ast_program);
    lexer_free_tokens(&tokens);
    return ok;
}

static int test_lower_ir_rejects_negative_float_call_int_condition_under_extension(void) {
    TokenArray tokens;
    AstProgram ast_program;
    ParserError parse_err;
    SemanticError sema_err;
    SemanticOptions sema_options;
    int ok = 0;

    lexer_init_tokens(&tokens);
    ast_program_init(&ast_program);
    memset(&parse_err, 0, sizeof(parse_err));
    memset(&sema_err, 0, sizeof(sema_err));
    memset(&sema_options, 0, sizeof(sema_options));
    sema_options.allow_extension_features = 1;

    ok = lexer_tokenize(
            "float id(float x){ return x; }\n"
            "int main(){ if(-id(1.0) + 1) return 1; return 0; }\n",
            &tokens) &&
        parser_parse_translation_unit_ast(&tokens, &ast_program, &parse_err) &&
        !semantic_analyze_program_with_options(&ast_program, &sema_options, &sema_err) &&
        strstr(sema_err.message, "SEMA-TYPE-008") != NULL;

    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-NEG-CALL-COND-PLUS-INT-REJECT mismatch: parse='%s' sema='%s'\n",
            parse_err.message,
            sema_err.message);
    }

    ast_program_free(&ast_program);
    lexer_free_tokens(&tokens);
    return ok;
}

static int test_lower_ir_rejects_nested_float_tree_plus_int_condition_under_extension(void) {
    TokenArray tokens;
    AstProgram ast_program;
    ParserError parse_err;
    SemanticError sema_err;
    SemanticOptions sema_options;
    int ok = 0;

    lexer_init_tokens(&tokens);
    ast_program_init(&ast_program);
    memset(&parse_err, 0, sizeof(parse_err));
    memset(&sema_err, 0, sizeof(sema_err));
    memset(&sema_options, 0, sizeof(sema_options));
    sema_options.allow_extension_features = 1;

    ok = lexer_tokenize(
            "int main(float x, float y, float z){ if(((x + y) + z) + 1) return 1; return 0; }\n",
            &tokens) &&
        parser_parse_translation_unit_ast(&tokens, &ast_program, &parse_err) &&
        !semantic_analyze_program_with_options(&ast_program, &sema_options, &sema_err) &&
        strstr(sema_err.message, "SEMA-EXT-035") != NULL;

    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-NESTED-TREE-COND-PLUS-INT-REJECT mismatch: parse='%s' sema='%s'\n",
            parse_err.message,
            sema_err.message);
    }

    ast_program_free(&ast_program);
    lexer_free_tokens(&tokens);
    return ok;
}

static int test_lower_ir_rejects_nested_float_muldiv_plus_int_condition_under_extension(void) {
    TokenArray tokens;
    AstProgram ast_program;
    ParserError parse_err;
    SemanticError sema_err;
    SemanticOptions sema_options;
    int ok = 0;

    lexer_init_tokens(&tokens);
    ast_program_init(&ast_program);
    memset(&parse_err, 0, sizeof(parse_err));
    memset(&sema_err, 0, sizeof(sema_err));
    memset(&sema_options, 0, sizeof(sema_options));
    sema_options.allow_extension_features = 1;

    ok = lexer_tokenize(
            "int main(float a, float b, float c){ if((-a * (b / c)) + 1) return 1; return 0; }\n",
            &tokens) &&
        parser_parse_translation_unit_ast(&tokens, &ast_program, &parse_err) &&
        !semantic_analyze_program_with_options(&ast_program, &sema_options, &sema_err) &&
        strstr(sema_err.message, "SEMA-EXT-035") != NULL;

    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-NESTED-MULDIV-COND-PLUS-INT-REJECT mismatch: parse='%s' sema='%s'\n",
            parse_err.message,
            sema_err.message);
    }

    ast_program_free(&ast_program);
    lexer_free_tokens(&tokens);
    return ok;
}

static int test_lower_ir_rejects_float_compare_against_int_under_extension(void) {
    TokenArray tokens;
    AstProgram ast_program;
    ParserError parse_err;
    SemanticError sema_err;
    SemanticOptions sema_options;
    int ok = 0;

    lexer_init_tokens(&tokens);
    ast_program_init(&ast_program);
    memset(&parse_err, 0, sizeof(parse_err));
    memset(&sema_err, 0, sizeof(sema_err));
    memset(&sema_options, 0, sizeof(sema_options));
    sema_options.allow_extension_features = 1;

    ok = lexer_tokenize(
            "float g = 1.25;\nint main(){ return g == 1; }\n",
            &tokens) &&
        parser_parse_translation_unit_ast(&tokens, &ast_program, &parse_err) &&
        !semantic_analyze_program_with_options(&ast_program, &sema_options, &sema_err) &&
        strstr(sema_err.message, "SEMA-TYPE-007") != NULL;

    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-COMPARE-INT-TYPE-REJECT mismatch: parse='%s' sema='%s'\n",
            parse_err.message,
            sema_err.message);
    }

    ast_program_free(&ast_program);
    lexer_free_tokens(&tokens);
    return ok;
}

static int test_lower_ir_rejects_nested_float_tree_plus_int_under_extension(void) {
    TokenArray tokens;
    AstProgram ast_program;
    ParserError parse_err;
    SemanticError sema_err;
    SemanticOptions sema_options;
    int ok = 0;

    lexer_init_tokens(&tokens);
    ast_program_init(&ast_program);
    memset(&parse_err, 0, sizeof(parse_err));
    memset(&sema_err, 0, sizeof(sema_err));
    memset(&sema_options, 0, sizeof(sema_options));
    sema_options.allow_extension_features = 1;

    ok = lexer_tokenize(
            "float add(float x, float y){ return (x + y) + 1; }\n"
            "int main(){ return 0; }\n",
            &tokens) &&
        parser_parse_translation_unit_ast(&tokens, &ast_program, &parse_err) &&
        !semantic_analyze_program_with_options(&ast_program, &sema_options, &sema_err) &&
        strstr(sema_err.message, "SEMA-EXT-035") != NULL;

    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-NESTED-TREE-PLUS-INT-REJECT mismatch: parse='%s' sema='%s'\n",
            parse_err.message,
            sema_err.message);
    }

    ast_program_free(&ast_program);
    lexer_free_tokens(&tokens);
    return ok;
}

static int test_lower_ir_rejects_nested_float_muldiv_plus_int_under_extension(void) {
    TokenArray tokens;
    AstProgram ast_program;
    ParserError parse_err;
    SemanticError sema_err;
    SemanticOptions sema_options;
    int ok = 0;

    lexer_init_tokens(&tokens);
    ast_program_init(&ast_program);
    memset(&parse_err, 0, sizeof(parse_err));
    memset(&sema_err, 0, sizeof(sema_err));
    memset(&sema_options, 0, sizeof(sema_options));
    sema_options.allow_extension_features = 1;

    ok = lexer_tokenize(
            "float f(float a, float b, float c){ return (-a * (b / c)) + 1; }\n"
            "int main(){ return 0; }\n",
            &tokens) &&
        parser_parse_translation_unit_ast(&tokens, &ast_program, &parse_err) &&
        !semantic_analyze_program_with_options(&ast_program, &sema_options, &sema_err) &&
        strstr(sema_err.message, "SEMA-EXT-035") != NULL;

    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-NESTED-MULDIV-PLUS-INT-REJECT mismatch: parse='%s' sema='%s'\n",
            parse_err.message,
            sema_err.message);
    }

    ast_program_free(&ast_program);
    lexer_free_tokens(&tokens);
    return ok;
}

static int test_lower_ir_rejects_float_ternary_value_plus_int_under_extension(void) {
    TokenArray tokens;
    AstProgram ast_program;
    ParserError parse_err;
    SemanticError sema_err;
    SemanticOptions sema_options;
    int ok = 0;

    lexer_init_tokens(&tokens);
    ast_program_init(&ast_program);
    memset(&parse_err, 0, sizeof(parse_err));
    memset(&sema_err, 0, sizeof(sema_err));
    memset(&sema_options, 0, sizeof(sema_options));
    sema_options.allow_extension_features = 1;

    ok = lexer_tokenize(
            "float g = 1.25;\n"
            "float h = 2.5;\n"
            "int main(){ return (g ? h : h) + 1; }\n",
            &tokens) &&
        parser_parse_translation_unit_ast(&tokens, &ast_program, &parse_err) &&
        !semantic_analyze_program_with_options(&ast_program, &sema_options, &sema_err) &&
        strstr(sema_err.message, "SEMA-EXT-035") != NULL;

    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-TERNARY-VALUE-PLUS-INT-REJECT mismatch: parse='%s' sema='%s'\n",
            parse_err.message,
            sema_err.message);
    }

    ast_program_free(&ast_program);
    lexer_free_tokens(&tokens);
    return ok;
}

static int test_lower_ir_rejects_unary_call_ternary_value_plus_int_under_extension(void) {
    TokenArray tokens;
    AstProgram ast_program;
    ParserError parse_err;
    SemanticError sema_err;
    SemanticOptions sema_options;
    int ok = 0;

    lexer_init_tokens(&tokens);
    ast_program_init(&ast_program);
    memset(&parse_err, 0, sizeof(parse_err));
    memset(&sema_err, 0, sizeof(sema_err));
    memset(&sema_options, 0, sizeof(sema_options));
    sema_options.allow_extension_features = 1;

    ok = lexer_tokenize(
            "float id(float x){ return x; }\n"
            "float add(float x){ return (-id(x) ? x : x) + 1; }\n"
            "int main(){ return 0; }\n",
            &tokens) &&
        parser_parse_translation_unit_ast(&tokens, &ast_program, &parse_err) &&
        !semantic_analyze_program_with_options(&ast_program, &sema_options, &sema_err) &&
        strstr(sema_err.message, "SEMA-EXT-035") != NULL;

    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-UNARY-CALL-TERNARY-PLUS-INT-REJECT mismatch: parse='%s' sema='%s'\n",
            parse_err.message,
            sema_err.message);
    }

    ast_program_free(&ast_program);
    lexer_free_tokens(&tokens);
    return ok;
}

static int test_lower_ir_rejects_float_ternary_value_plus_float_call_argument_under_extension(void) {
    TokenArray tokens;
    AstProgram ast_program;
    ParserError parse_err;
    SemanticError sema_err;
    SemanticOptions sema_options;
    int ok = 0;

    lexer_init_tokens(&tokens);
    ast_program_init(&ast_program);
    memset(&parse_err, 0, sizeof(parse_err));
    memset(&sema_err, 0, sizeof(sema_err));
    memset(&sema_options, 0, sizeof(sema_options));
    sema_options.allow_extension_features = 1;

    ok = lexer_tokenize(
            "float g = 1.25;\n"
            "float h = 2.5;\n"
            "float wrap(float x){ return x; }\n"
            "float get(){ return wrap((g ? h : h) + h); }\n"
            "int main(){ return 0; }\n",
            &tokens) &&
        parser_parse_translation_unit_ast(&tokens, &ast_program, &parse_err) &&
        !semantic_analyze_program_with_options(&ast_program, &sema_options, &sema_err) &&
        strstr(sema_err.message, "SEMA-EXT-035") != NULL;

    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-TERNARY-PLUS-FLOAT-CALLARG-REJECT mismatch: parse='%s' sema='%s'\n",
            parse_err.message,
            sema_err.message);
    }

    ast_program_free(&ast_program);
    lexer_free_tokens(&tokens);
    return ok;
}

static int test_lower_ir_rejects_unary_call_ternary_value_plus_float_call_argument_under_extension(void) {
    TokenArray tokens;
    AstProgram ast_program;
    ParserError parse_err;
    SemanticError sema_err;
    SemanticOptions sema_options;
    int ok = 0;

    lexer_init_tokens(&tokens);
    ast_program_init(&ast_program);
    memset(&parse_err, 0, sizeof(parse_err));
    memset(&sema_err, 0, sizeof(sema_err));
    memset(&sema_options, 0, sizeof(sema_options));
    sema_options.allow_extension_features = 1;

    ok = lexer_tokenize(
            "float id(float x){ return x; }\n"
            "float wrap(float x){ return x; }\n"
            "float f(float x){ return wrap(((-id(x) ? x : x)) + x); }\n"
            "int main(){ return 0; }\n",
            &tokens) &&
        parser_parse_translation_unit_ast(&tokens, &ast_program, &parse_err) &&
        !semantic_analyze_program_with_options(&ast_program, &sema_options, &sema_err) &&
        strstr(sema_err.message, "SEMA-EXT-035") != NULL;

    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-UNARY-CALL-TERNARY-PLUS-FLOAT-CALLARG-REJECT mismatch: parse='%s' sema='%s'\n",
            parse_err.message,
            sema_err.message);
    }

    ast_program_free(&ast_program);
    lexer_free_tokens(&tokens);
    return ok;
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

static int test_lower_ir_lowers_float_literal_runtime_global_initializer_under_extension(void) {
    char *actual_text = NULL;
    int ok = 0;

    if (!lower_extension_source_to_lower_ir_text(
            "float g = 1.25;\nfloat id(float x){ return x; }\nint main(){ return 0; }\n",
            &actual_text)) {
        free(actual_text);
        return 0;
    }

    ok = actual_text &&
        strstr(actual_text, "global g.0:float\n") != NULL &&
        strstr(actual_text, "func __global.init() {\n") != NULL &&
        strstr(actual_text, "store_global g.0, 1067450368\n") != NULL &&
        strstr(actual_text, "call __global.init()") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[lower-ir-reg] FAIL: LOWER-IR-FLOAT-GLOBAL-LITERAL-RUNTIME-INIT mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "(null)");
    }

    free(actual_text);
    return ok;
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

static int test_lower_ir_lowers_runtime_global_array_initializer_with_short_circuit_and_calls(void) {
    return expect_lower_ir_lower_succeeds("LOWER-IR-RUNTIME-GLOBAL-ARRAY-INIT-LOGICAL-CALLS",
        "int seed=2;\n"
        "int next(){seed=seed+3; return seed;}\n"
        "int arr[8]={1, next(), seed+10, 0&&next(), 1||next(), next(), {seed}, {}};\n"
        "int main(){return arr[0]+arr[1]+arr[2]+arr[3]+arr[4]+arr[5]+arr[6]+arr[7];}\n");
}

static int test_lower_ir_lowers_multiple_runtime_global_initializers_after_branchy_helper_growth(void) {
    return expect_lower_ir_lower_succeeds("LOWER-IR-RUNTIME-GLOBAL-MULTI-INIT-AFTER-BRANCHY-HELPER",
        "int t(){return 5;}\n"
        "int z(){return 0;}\n"
        "int a=z()&&t();\n"
        "int b=t()||z();\n"
        "int c[2]={a+7,b+9};\n"
        "int main(){return c[0]+c[1];}\n");
}

static int test_lower_ir_omits_synthetic_zero_result_after_void_call_statement(void) {
    return expect_lowered_source_dump("LOWER-IR-VOID-CALL-STMT-NO-SYNTHETIC-ZERO",
        "void touch(){ putint(1); }\n"
        "int main(){ touch(); return 0; }\n",
        "declare putint(param0.0)\n"
        "\n"
        "func touch() {\n"
        "  bb.0:\n"
        "    call putint(1)\n"
        "    ret\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    call touch()\n"
        "    ret 0\n"
        "}\n");
}

static int test_lower_ir_lowers_1d_array_initializer_mixing_nested_and_scalar_items(void) {
    return expect_lowered_source_dump("LOWER-IR-LOCAL-ARRAY-NESTED-SCALAR-MIX",
        "int main(){ int a[4]={{1,2},3}; return a[0]+a[1]*10+a[2]*100+a[3]*1000; }\n",
        "func main() {\n"
        "  bb.0:\n"
        "    store_local a.0, 1\n"
        "    store_local a.1, 3\n"
        "    store_local a.2, 0\n"
        "    store_local a.3, 0\n"
        "    tmp.0 = addr_local a.0\n"
        "    tmp.1 = mul 0, 4\n"
        "    tmp.2 = add tmp.0, tmp.1\n"
        "    tmp.3 = load_indirect tmp.2\n"
        "    tmp.4 = addr_local a.0\n"
        "    tmp.5 = mul 1, 4\n"
        "    tmp.6 = add tmp.4, tmp.5\n"
        "    tmp.7 = load_indirect tmp.6\n"
        "    tmp.8 = mul tmp.7, 10\n"
        "    tmp.9 = add tmp.3, tmp.8\n"
        "    tmp.10 = addr_local a.0\n"
        "    tmp.11 = mul 2, 4\n"
        "    tmp.12 = add tmp.10, tmp.11\n"
        "    tmp.13 = load_indirect tmp.12\n"
        "    tmp.14 = mul tmp.13, 100\n"
        "    tmp.15 = add tmp.9, tmp.14\n"
        "    tmp.16 = addr_local a.0\n"
        "    tmp.17 = mul 3, 4\n"
        "    tmp.18 = add tmp.16, tmp.17\n"
        "    tmp.19 = load_indirect tmp.18\n"
        "    tmp.20 = mul tmp.19, 1000\n"
        "    tmp.21 = add tmp.15, tmp.20\n"
        "    ret tmp.21\n"
        "}\n");
}

static int test_lower_ir_accepts_2d_array_initializer_mixing_nested_and_scalar_items(void) {
    return expect_lower_ir_lower_succeeds("LOWER-IR-2D-ARRAY-NESTED-SCALAR-MIX",
        "int a[2][3]={{1},2,3,{4,5}};\n"
        "int main(){return a[0][0]+a[0][1]*10+a[0][2]*100+a[1][0]*1000+a[1][1]*10000+a[1][2]*100000;}\n");
}

int main(void) {
    const char *filter = getenv("LOWER_IR_REG_FILTER");
    int ok = 1;

    if (filter && filter[0] != '\0') {
        if (strcmp(filter, "LOWER-IR-RETURNED-MULTI-CAPTURE-LOCAL-CLOSURE-BIND-CALL") == 0) {
            return test_lower_ir_accepts_returned_multi_capture_local_closure_bind_and_call_under_extension() ? 0 : 1;
        }
        if (strcmp(filter, "LOWER-IR-DYNAMIC-RETURNED-MULTI-CAPTURE-LOCAL-CLOSURE-FORWARD") == 0) {
            return test_lower_ir_accepts_dynamic_returned_multi_capture_local_closure_forwarding_into_function_parameter_under_extension() ? 0 : 1;
        }
        if (strcmp(filter, "LOWER-IR-DYNAMIC-RUNTIME-CLOSURE-FORWARD") == 0) {
            return test_lower_ir_accepts_dynamic_runtime_closure_local_forward_into_function_parameter_under_extension() ? 0 : 1;
        }
        if (strcmp(filter, "LOWER-IR-DYNAMIC-RUNTIME-ZERO-VOID-CLOSURE-FORWARD") == 0) {
            return test_lower_ir_accepts_dynamic_runtime_zero_arg_void_closure_local_forward_into_function_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-TRANSPORT-SIGNATURE", filter) != NULL) {
            return test_lower_ir_lowers_float_transport_signature_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-LITERAL-TRANSPORT", filter) != NULL) {
            return test_lower_ir_lowers_float_literal_transport_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-GLOBAL-IDENT-INIT", filter) != NULL) {
            return test_lower_ir_lowers_float_global_initializer_transport_from_identifier_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-GLOBAL-CALL-INIT", filter) != NULL) {
            return test_lower_ir_lowers_float_global_initializer_transport_from_call_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-RETURN-GLOBAL", filter) != NULL) {
            return test_lower_ir_lowers_float_return_transport_from_global_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-RETURN-GLOBAL-CALL", filter) != NULL) {
            return test_lower_ir_lowers_float_return_transport_from_global_call_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-GLOBAL-CALL-CHAIN", filter) != NULL) {
            return test_lower_ir_lowers_float_global_call_chain_transport_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-LOCAL-CALL-CHAIN", filter) != NULL) {
            return test_lower_ir_lowers_float_local_call_chain_transport_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-GLOBAL-OP-SEMANTIC-REJECT", filter) != NULL) {
            return test_lower_ir_rejects_global_float_operator_expression_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-GLOBAL-OP-INIT-SEMANTIC-REJECT", filter) != NULL) {
            return test_lower_ir_rejects_global_float_operator_expression_in_initializer_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-GLOBAL-CALL-INIT-TYPE-REJECT", filter) != NULL) {
            return test_lower_ir_rejects_global_float_call_result_in_initializer_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-ASSIGN-TRANSPORT", filter) != NULL) {
            return test_lower_ir_lowers_float_assignment_transport_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-ASSIGN-TYPE-REJECT", filter) != NULL) {
            return test_lower_ir_rejects_float_assignment_to_int_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-IF-COND-ACCEPT", filter) != NULL) {
            return test_lower_ir_rejects_float_if_condition_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-WHILE-COND-ACCEPT", filter) != NULL) {
            return test_lower_ir_rejects_float_while_condition_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-FOR-COND-ACCEPT", filter) != NULL) {
            return test_lower_ir_rejects_float_for_condition_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-RECURSIVE-IF-COND-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_recursive_float_if_condition_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-RECURSIVE-WHILE-COND-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_recursive_float_while_condition_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-RECURSIVE-FOR-COND-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_recursive_float_for_condition_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-RECURSIVE-COND-TERNARY-NEIGHBOR-REJECT", filter) != NULL) {
            return test_lower_ir_rejects_recursive_float_condition_with_ternary_neighbor_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-LOGICAL-COND-COMPOSE-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_float_logical_condition_composition_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-EQ-COMPARE-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_float_equality_compare_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-NE-COMPARE-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_float_inequality_compare_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-LT-COMPARE-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_float_relational_compare_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-CHAIN-ADD-EQ-COMPARE-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_chained_float_equality_compare_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-CHAIN-ADD-LT-COMPARE-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_chained_float_relational_compare_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-CHAIN-ADD-NE-COMPARE-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_chained_float_inequality_compare_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-CHAIN-ADD-LE-COMPARE-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_chained_float_le_compare_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-NESTED-MUL-DIV-EQ-COMPARE-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_nested_muldiv_float_equality_compare_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-NESTED-MUL-DIV-LT-COMPARE-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_nested_muldiv_float_relational_compare_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-NESTED-MUL-DIV-NE-COMPARE-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_nested_muldiv_float_inequality_compare_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-NESTED-MUL-DIV-GE-COMPARE-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_nested_muldiv_float_ge_compare_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-NEG-LITERAL-TRANSPORT", filter) != NULL) {
            return test_lower_ir_accepts_negative_float_literal_transport_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-NEG-IDENT-TRANSPORT", filter) != NULL) {
            return test_lower_ir_accepts_unary_minus_float_identifier_transport_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-NEG-ZERO-COND-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_negative_zero_float_condition_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-NEG-LT-ZERO-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_negative_float_relational_compare_against_zero_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-NEG-ZERO-LE-ZERO-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_negative_zero_le_zero_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-ADD-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_float_addition_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-SUB-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_float_subtraction_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-EXPLICIT-INT-FROM-FLOAT-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_explicit_int_from_float_conversion_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-EXPLICIT-FLOAT-FROM-INT-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_explicit_float_from_int_conversion_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-CONVERT-COND-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_explicit_float_condition_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-CONVERT-RECURSIVE-COND-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_recursive_explicit_float_condition_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-CONVERT-WHILE-COND-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_explicit_float_while_condition_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-CONVERT-LOGIC-COND-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_explicit_float_logical_condition_composition_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-EXPLICIT-INT-FROM-FLOAT-TERNARY-BRIDGE-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_explicit_int_from_float_ternary_bridge_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-EXPLICIT-INT-FROM-RECURSIVE-CALLARG-BRIDGE-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_explicit_int_from_recursive_float_call_argument_bridge_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-EXPLICIT-INT-FROM-RECURSIVE-INIT-BRIDGE-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_explicit_int_from_recursive_float_initializer_bridge_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-EXPLICIT-INT-FROM-RECURSIVE-ASSIGN-BRIDGE-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_explicit_int_from_recursive_float_assignment_bridge_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-EXPLICIT-INT-FROM-FLOAT-COMPARE-BRIDGE-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_explicit_int_from_float_compare_bridge_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-EXPLICIT-INT-FROM-RECURSIVE-ARITH-BRIDGE-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_explicit_int_from_recursive_float_arithmetic_bridge_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-EXPLICIT-FLOAT-FROM-RECURSIVE-INIT-BRIDGE-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_explicit_float_from_recursive_int_initializer_bridge_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-EXPLICIT-FLOAT-FROM-RECURSIVE-ASSIGN-BRIDGE-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_explicit_float_from_recursive_int_assignment_bridge_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-EXPLICIT-FLOAT-FROM-INT-COMPARE-BRIDGE-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_explicit_float_from_int_compare_bridge_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-EXPLICIT-FLOAT-FROM-RECURSIVE-ARITH-BRIDGE-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_explicit_float_from_recursive_int_arithmetic_bridge_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-NEG-ADD-COMBO-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_negative_float_addition_combo_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-NEG-SUB-COMBO-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_negative_float_subtraction_combo_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-MUL-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_float_multiplication_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-DIV-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_float_division_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-NEG-MUL-COMBO-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_negative_float_multiplication_combo_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-NEG-DIV-COMBO-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_negative_float_division_combo_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-CHAIN-ADD-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_chained_float_addition_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-NESTED-MUL-DIV-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_nested_float_mul_div_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-ARITH-INT-TYPE-REJECT", filter) != NULL) {
            return test_lower_ir_rejects_mixed_float_int_arithmetic_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-LITERAL-ARITH-INT-TYPE-REJECT", filter) != NULL) {
            return test_lower_ir_rejects_float_literal_int_arithmetic_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-CALL-ARITH-INT-TYPE-REJECT", filter) != NULL) {
            return test_lower_ir_rejects_float_call_int_arithmetic_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-NEG-CALL-ARITH-INT-TYPE-REJECT", filter) != NULL) {
            return test_lower_ir_rejects_negative_float_call_int_arithmetic_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-RETURNED-SINGLE-CAPTURE-CLOSURE-FORWARD", filter) != NULL) {
            return test_lower_ir_accepts_returned_single_capture_closure_forwarding_into_function_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-RETURNED-SINGLE-CAPTURE-CLOSURE-ALIAS-FORWARD", filter) != NULL) {
            return test_lower_ir_accepts_returned_single_capture_closure_alias_forwarding_into_function_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-RETURNED-SINGLE-CAPTURE-ZERO-CLOSURE-FORWARD", filter) != NULL) {
            return test_lower_ir_accepts_returned_single_capture_zero_arg_closure_forwarding_into_function_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-RETURNED-SINGLE-CAPTURE-ZERO-VOID-CLOSURE-FORWARD", filter) != NULL) {
            return test_lower_ir_accepts_returned_single_capture_zero_arg_void_closure_forwarding_into_function_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-RETURNED-LOCAL-CLOSURE-FORWARD", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_returned_local_closure_forwarding_into_function_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-RETURNED-MULTI-CAPTURE-LOCAL-CLOSURE-BIND-CALL", filter) != NULL) {
            return test_lower_ir_accepts_returned_multi_capture_local_closure_bind_and_call_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-RETURNED-MULTI-CAPTURE-LOCAL-CLOSURE-FORWARD", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_returned_multi_capture_local_closure_forwarding_into_function_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-RUNTIME-CLOSURE-FORWARD", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_runtime_closure_local_forward_into_function_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-RUNTIME-ZERO-VOID-CLOSURE-FORWARD", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_runtime_zero_arg_void_closure_local_forward_into_function_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-RETURNED-CLOSURE-BACKED-FUNCTION-VALUE-PARAM-BIND", filter) != NULL) {
            return test_lower_ir_lowers_returned_closure_backed_function_value_parameter_bind_and_call_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-RETURNED-CLOSURE-BACKED-FUNCTION-VALUE-PARAM-BIND", filter) != NULL) {
            return test_lower_ir_lowers_dynamic_returned_closure_backed_function_value_parameter_bind_and_call_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-GLOBAL-COND-PLUS-INT-REJECT", filter) != NULL) {
            return test_lower_ir_rejects_global_float_int_condition_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-CALL-COND-PLUS-INT-REJECT", filter) != NULL) {
            return test_lower_ir_rejects_float_call_int_condition_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-NEG-CALL-COND-PLUS-INT-REJECT", filter) != NULL) {
            return test_lower_ir_rejects_negative_float_call_int_condition_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-NESTED-TREE-COND-PLUS-INT-REJECT", filter) != NULL) {
            return test_lower_ir_rejects_nested_float_tree_plus_int_condition_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-NESTED-MULDIV-COND-PLUS-INT-REJECT", filter) != NULL) {
            return test_lower_ir_rejects_nested_float_muldiv_plus_int_condition_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-COMPARE-INT-TYPE-REJECT", filter) != NULL) {
            return test_lower_ir_rejects_float_compare_against_int_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-NESTED-TREE-PLUS-INT-REJECT", filter) != NULL) {
            return test_lower_ir_rejects_nested_float_tree_plus_int_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-NESTED-MULDIV-PLUS-INT-REJECT", filter) != NULL) {
            return test_lower_ir_rejects_nested_float_muldiv_plus_int_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-TERNARY-VALUE-PLUS-INT-REJECT", filter) != NULL) {
            return test_lower_ir_rejects_float_ternary_value_plus_int_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-UNARY-CALL-TERNARY-PLUS-INT-REJECT", filter) != NULL) {
            return test_lower_ir_rejects_unary_call_ternary_value_plus_int_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-TERNARY-PLUS-FLOAT-CALLARG-REJECT", filter) != NULL) {
            return test_lower_ir_rejects_float_ternary_value_plus_float_call_argument_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-UNARY-CALL-TERNARY-PLUS-FLOAT-CALLARG-REJECT", filter) != NULL) {
            return test_lower_ir_rejects_unary_call_ternary_value_plus_float_call_argument_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-TERNARY-VALUE-RETURN-INT-REJECT", filter) != NULL) {
            return test_lower_ir_rejects_float_ternary_value_return_to_int_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-UNARY-CALL-TERNARY-RETURN-INT-REJECT", filter) != NULL) {
            return test_lower_ir_rejects_unary_call_ternary_value_return_to_int_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-TERNARY-VALUE-SEMANTIC-REJECT", filter) != NULL) {
            return test_lower_ir_rejects_float_ternary_value_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-TERNARY-VALUE-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_same_type_float_ternary_value_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-TERNARY-VALUE-ASSIGN-FLOAT-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_float_ternary_value_assignment_to_float_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-TERNARY-VALUE-INIT-FLOAT-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_float_ternary_value_initializer_to_float_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-CHAIN-ADD-ASSIGN-FLOAT-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_chained_float_addition_assignment_to_float_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-NESTED-MUL-DIV-ASSIGN-FLOAT-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_nested_float_mul_div_assignment_to_float_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-CHAIN-ADD-INIT-FLOAT-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_chained_float_addition_initializer_to_float_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-NESTED-MUL-DIV-INIT-FLOAT-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_nested_float_mul_div_initializer_to_float_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-TERNARY-VALUE-CALLARG-FLOAT-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_float_ternary_value_call_argument_to_float_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-UNARY-CALL-TERNARY-CALLARG-FLOAT-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_unary_call_float_ternary_value_call_argument_to_float_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-CHAIN-ADD-CALLARG-FLOAT-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_chained_float_addition_call_argument_to_float_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-NESTED-MUL-DIV-CALLARG-FLOAT-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_nested_float_mul_div_call_argument_to_float_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-HELPER-TERNARY-CALL-ARITH-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_float_helper_wrapped_ternary_call_arithmetic_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-UNARY-HELPER-TERNARY-CALL-ARITH-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_unary_call_helper_wrapped_ternary_call_arithmetic_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-HELPER-TERNARY-CALL-COMPARE-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_float_helper_wrapped_ternary_call_compare_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-UNARY-HELPER-TERNARY-CALL-COMPARE-ACCEPT", filter) != NULL) {
            return test_lower_ir_accepts_unary_call_helper_wrapped_ternary_call_compare_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-FNVAL-RETURN-BIND", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_noncapturing_function_value_return_bind_and_call_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-STATIC-TOPLEVEL-FNVAL-RETURN-BIND", filter) != NULL) {
            return test_lower_ir_accepts_static_toplevel_noncapturing_function_value_return_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-FNVAL-RETURN-IMMEDIATE", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_noncapturing_function_value_return_immediate_call_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-WRAPPED-FNVAL-RETURN", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_wrapped_function_value_return_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-RETURNED-FNVAL-REASSIGN", filter) != NULL) {
            return test_lower_ir_accepts_returned_function_value_reassignment_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-RETURNED-ZERO-FNVAL-REASSIGN", filter) != NULL) {
            return test_lower_ir_accepts_returned_zero_arg_function_value_reassignment_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-RETURNED-CLOSURE-REASSIGN", filter) != NULL) {
            return test_lower_ir_accepts_returned_closure_reassignment_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-RETURNED-ZERO-VOID-CLOSURE-REASSIGN", filter) != NULL) {
            return test_lower_ir_accepts_returned_zero_arg_void_closure_reassignment_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-WRAPPED-FNVAL-REASSIGN", filter) != NULL) {
            return test_lower_ir_accepts_wrapped_function_value_reassignment_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-ASSIGNMENT-RESULT-FNVAL-REASSIGN", filter) != NULL) {
            return test_lower_ir_accepts_assignment_result_function_value_reassignment_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-WRAPPED-RETURNED-FNVAL-REASSIGN", filter) != NULL) {
            return test_lower_ir_accepts_wrapped_returned_function_value_reassignment_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-WRAPPED-RETURNED-FNVAL-LOCAL-INIT", filter) != NULL) {
            return test_lower_ir_accepts_wrapped_returned_function_value_local_initializer_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-WRAPPED-RETURNED-ZERO-FNVAL-LOCAL-INIT", filter) != NULL) {
            return test_lower_ir_accepts_wrapped_returned_zero_arg_function_value_local_initializer_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-ASSIGNMENT-RESULT-RETURNED-CLOSURE-LOCAL-INIT", filter) != NULL) {
            return test_lower_ir_accepts_assignment_result_returned_closure_local_initializer_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-FNVAL-LOCAL-INIT", filter) != NULL) {
            return test_lower_ir_accepts_wrapped_returned_call_ternary_function_value_local_initializer_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-CLOSURE-LOCAL-INIT", filter) != NULL) {
            return test_lower_ir_accepts_wrapped_returned_call_ternary_closure_local_initializer_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-FNVAL-BOUNCE", filter) != NULL) {
            return test_lower_ir_accepts_wrapped_returned_call_ternary_function_value_local_bounce_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-FNVAL-FORWARD", filter) != NULL) {
            return test_lower_ir_accepts_wrapped_returned_call_ternary_function_value_forward_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-CLOSURE-BOUNCE", filter) != NULL) {
            return test_lower_ir_accepts_wrapped_returned_call_ternary_closure_local_bounce_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-CLOSURE-FORWARD", filter) != NULL) {
            return test_lower_ir_accepts_wrapped_returned_call_ternary_closure_forward_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-FNVAL-BIND-RETURN", filter) != NULL) {
            return test_lower_ir_accepts_wrapped_returned_call_ternary_function_value_bind_return_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-FNVAL-BIND-RETURN-ACTUAL", filter) != NULL) {
            return test_lower_ir_accepts_wrapped_returned_call_ternary_function_value_bind_return_actual_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-CLOSURE-BIND-RETURN", filter) != NULL) {
            return test_lower_ir_accepts_wrapped_returned_call_ternary_closure_bind_return_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-CLOSURE-BIND-RETURN-ACTUAL", filter) != NULL) {
            return test_lower_ir_accepts_wrapped_returned_call_ternary_closure_bind_return_actual_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-FNVAL-BIND-RETURN", filter) != NULL) {
            return test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_function_value_bind_return_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-FNVAL-BIND-RETURN-ACTUAL", filter) != NULL) {
            return test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_function_value_bind_return_actual_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-VOID-FNVAL-BIND-RETURN", filter) != NULL) {
            return test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_function_value_bind_return_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-VOID-FNVAL-BIND-RETURN-ACTUAL", filter) != NULL) {
            return test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_function_value_bind_return_actual_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-CLOSURE-BIND-RETURN", filter) != NULL) {
            return test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_closure_bind_return_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-CLOSURE-BIND-RETURN-ACTUAL", filter) != NULL) {
            return test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_closure_bind_return_actual_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-VOID-CLOSURE-BIND-RETURN", filter) != NULL) {
            return test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_closure_bind_return_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-VOID-CLOSURE-BIND-RETURN-ACTUAL", filter) != NULL) {
            return test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_closure_bind_return_actual_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-FNVAL-BOUNCE-PASSTHROUGH-BIND-RETURN", filter) != NULL) {
            return test_lower_ir_accepts_wrapped_returned_call_ternary_function_value_bounce_passthrough_bind_return_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-FNVAL-BOUNCE-PASSTHROUGH-BIND-RETURN-ACTUAL", filter) != NULL) {
            return test_lower_ir_accepts_wrapped_returned_call_ternary_function_value_bounce_passthrough_bind_return_actual_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-CLOSURE-BOUNCE-PASSTHROUGH-BIND-RETURN", filter) != NULL) {
            return test_lower_ir_accepts_wrapped_returned_call_ternary_closure_bounce_passthrough_bind_return_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-CLOSURE-BOUNCE-PASSTHROUGH-BIND-RETURN-ACTUAL", filter) != NULL) {
            return test_lower_ir_accepts_wrapped_returned_call_ternary_closure_bounce_passthrough_bind_return_actual_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-FNVAL-BOUNCE-PASSTHROUGH-BIND-RETURN", filter) != NULL) {
            return test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_function_value_bounce_passthrough_bind_return_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-FNVAL-BOUNCE-PASSTHROUGH-BIND-RETURN-ACTUAL", filter) != NULL) {
            return test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_function_value_bounce_passthrough_bind_return_actual_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-VOID-FNVAL-BOUNCE-PASSTHROUGH-BIND-RETURN", filter) != NULL) {
            return test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_function_value_bounce_passthrough_bind_return_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-VOID-FNVAL-BOUNCE-PASSTHROUGH-BIND-RETURN-ACTUAL", filter) != NULL) {
            return test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_function_value_bounce_passthrough_bind_return_actual_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-CLOSURE-BOUNCE-PASSTHROUGH-BIND-RETURN", filter) != NULL) {
            return test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_closure_bounce_passthrough_bind_return_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-CLOSURE-BOUNCE-PASSTHROUGH-BIND-RETURN-ACTUAL", filter) != NULL) {
            return test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_closure_bounce_passthrough_bind_return_actual_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-VOID-CLOSURE-BOUNCE-PASSTHROUGH-BIND-RETURN", filter) != NULL) {
            return test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_closure_bounce_passthrough_bind_return_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-VOID-CLOSURE-BOUNCE-PASSTHROUGH-BIND-RETURN-ACTUAL", filter) != NULL) {
            return test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_closure_bounce_passthrough_bind_return_actual_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-FNVAL-STMT-REASSIGN-BIND-RETURN", filter) != NULL) {
            return test_lower_ir_accepts_wrapped_returned_call_ternary_function_value_statement_reassign_bind_return_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-FNVAL-STMT-REASSIGN-BIND-RETURN-ACTUAL", filter) != NULL) {
            return test_lower_ir_accepts_wrapped_returned_call_ternary_function_value_statement_reassign_bind_return_actual_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-CLOSURE-STMT-REASSIGN-BIND-RETURN", filter) != NULL) {
            return test_lower_ir_accepts_wrapped_returned_call_ternary_closure_statement_reassign_bind_return_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-CLOSURE-STMT-REASSIGN-BIND-RETURN-ACTUAL", filter) != NULL) {
            return test_lower_ir_accepts_wrapped_returned_call_ternary_closure_statement_reassign_bind_return_actual_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-FNVAL-STMT-REASSIGN-BIND-RETURN", filter) != NULL) {
            return test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_function_value_statement_reassign_bind_return_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-FNVAL-STMT-REASSIGN-BIND-RETURN-ACTUAL", filter) != NULL) {
            return test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_function_value_statement_reassign_bind_return_actual_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-VOID-FNVAL-STMT-REASSIGN-BIND-RETURN", filter) != NULL) {
            return test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_function_value_statement_reassign_bind_return_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-VOID-FNVAL-STMT-REASSIGN-BIND-RETURN-ACTUAL", filter) != NULL) {
            return test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_function_value_statement_reassign_bind_return_actual_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-CLOSURE-STMT-REASSIGN-BIND-RETURN", filter) != NULL) {
            return test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_closure_statement_reassign_bind_return_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-CLOSURE-STMT-REASSIGN-BIND-RETURN-ACTUAL", filter) != NULL) {
            return test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_closure_statement_reassign_bind_return_actual_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-VOID-CLOSURE-STMT-REASSIGN-BIND-RETURN", filter) != NULL) {
            return test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_closure_statement_reassign_bind_return_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-WRAPPED-RETURNED-CALL-TERNARY-ZERO-VOID-CLOSURE-STMT-REASSIGN-BIND-RETURN-ACTUAL", filter) != NULL) {
            return test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_closure_statement_reassign_bind_return_actual_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-ASSIGNMENT-RESULT-RETURNED-CLOSURE-REASSIGN", filter) != NULL) {
            return test_lower_ir_accepts_assignment_result_returned_closure_reassignment_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-RETURNED-CLOSURE-ASSIGN-RETURN", filter) != NULL) {
            return test_lower_ir_accepts_returned_closure_assignment_then_return_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-RETURNED-CLOSURE-FORWARD", filter) != NULL) {
            return test_lower_ir_accepts_returned_closure_forwarding_into_function_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-RETURNED-CLOSURE-ALIAS-FORWARD", filter) != NULL) {
            return test_lower_ir_accepts_returned_closure_alias_forwarding_into_function_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-RETURNED-CLOSURE-PRODUCER-TERNARY-MERGE-BIND", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_returned_closure_producer_ternary_merge_bind_and_call_under_extension()
                ? 0
                : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-RETURNED-CLOSURE-PRODUCER-TERNARY-MERGE-IMMEDIATE", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_returned_closure_producer_ternary_merge_immediate_call_under_extension()
                ? 0
                : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-RETURNED-CLOSURE-RETURNED-CALL-TERNARY-MERGE-IMMEDIATE", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_returned_closure_returned_call_ternary_merge_immediate_call_under_extension()
                ? 0
                : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-RETURNED-CLOSURE-RETURNED-CALL-TERNARY-MERGE-LOCAL-BOUNCE-IMMEDIATE", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_returned_closure_returned_call_ternary_merge_local_bounce_immediate_call_under_extension()
                ? 0
                : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-RETURNED-CLOSURE-RETURNED-CALL-TERNARY-MERGE-BIND-RETURN-IMMEDIATE", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_returned_closure_returned_call_ternary_merge_bind_return_immediate_call_under_extension()
                ? 0
                : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-RETURNED-CLOSURE-RETURNED-CALL-TERNARY-MERGE-BIND-RETURN-ACTUAL", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_returned_closure_returned_call_ternary_merge_bind_return_actual_argument_under_extension()
                ? 0
                : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-RETURNED-CLOSURE-RETURNED-CALL-TERNARY-MERGE-BOUNCE-PASSTHROUGH-IMMEDIATE", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_returned_closure_returned_call_ternary_merge_bounce_passthrough_immediate_call_under_extension()
                ? 0
                : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-RETURNED-CLOSURE-RETURNED-CALL-TERNARY-MERGE-BOUNCE-PASSTHROUGH-BIND-RETURN-IMMEDIATE", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_returned_closure_returned_call_ternary_merge_bounce_passthrough_bind_return_immediate_call_under_extension()
                ? 0
                : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-RETURNED-CLOSURE-RETURNED-CALL-TERNARY-MERGE-BOUNCE-PASSTHROUGH-BIND-RETURN-ACTUAL", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_returned_closure_returned_call_ternary_merge_bounce_passthrough_bind_return_actual_argument_under_extension()
                ? 0
                : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-RETURNED-CLOSURE-RETURNED-CALL-TERNARY-MERGE-DEEPER-CHAIN-BIND-RETURN-IMMEDIATE", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_returned_closure_returned_call_ternary_merge_deeper_chain_bind_return_immediate_call_under_extension()
                ? 0
                : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-RETURNED-CLOSURE-PRODUCER-TERNARY-MERGE-ACTUAL", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_returned_closure_producer_ternary_merge_actual_argument_under_extension()
                ? 0
                : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-RETURNED-CLOSURE-PRODUCER-TERNARY-MERGE-BOUNCE-IMMEDIATE", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_returned_closure_producer_ternary_merge_local_bounce_immediate_call_under_extension()
                ? 0
                : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-RETURNED-CLOSURE-PRODUCER-TERNARY-MERGE-BOUNCE-ACTUAL", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_returned_closure_producer_ternary_merge_local_bounce_actual_argument_under_extension()
                ? 0
                : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-RETURNED-CLOSURE-PRODUCER-TERNARY-MERGE-BOUNCE-PASSTHROUGH-IMMEDIATE", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_returned_closure_producer_ternary_merge_local_bounce_passthrough_immediate_call_under_extension()
                ? 0
                : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-RETURNED-CLOSURE-PRODUCER-TERNARY-MERGE-BOUNCE-PASSTHROUGH-ACTUAL", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_returned_closure_producer_ternary_merge_local_bounce_passthrough_actual_argument_under_extension()
                ? 0
                : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-RETURNED-CLOSURE-PRODUCER-TERNARY-MERGE-REASSIGN-IMMEDIATE", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_returned_closure_producer_ternary_merge_local_reassign_immediate_call_under_extension()
                ? 0
                : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-RETURNED-CLOSURE-PRODUCER-TERNARY-MERGE-REASSIGN-ACTUAL", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_returned_closure_producer_ternary_merge_local_reassign_actual_argument_under_extension()
                ? 0
                : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-RETURNED-CLOSURE-PRODUCER-TERNARY-MERGE-RETURNED-CALL-REASSIGN-IMMEDIATE", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_returned_closure_producer_ternary_merge_returned_call_reassign_immediate_call_under_extension()
                ? 0
                : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-RETURNED-CLOSURE-PRODUCER-TERNARY-MERGE-RETURNED-CALL-REASSIGN-ACTUAL", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_returned_closure_producer_ternary_merge_returned_call_reassign_actual_argument_under_extension()
                ? 0
                : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-RETURNED-ZERO-ARG-CLOSURE-PRODUCER-TERNARY-MERGE-IMMEDIATE", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_immediate_call_under_extension()
                ? 0
                : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-RETURNED-ZERO-ARG-CLOSURE-PRODUCER-TERNARY-MERGE-ACTUAL", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_actual_argument_under_extension()
                ? 0
                : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-RETURNED-ZERO-ARG-VOID-CLOSURE-PRODUCER-TERNARY-MERGE-IMMEDIATE", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_immediate_call_under_extension()
                ? 0
                : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-RETURNED-ZERO-ARG-VOID-CLOSURE-PRODUCER-TERNARY-MERGE-ACTUAL", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_actual_argument_under_extension()
                ? 0
                : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-RETURNED-ZERO-ARG-CLOSURE-PRODUCER-TERNARY-MERGE-BOUNCE-IMMEDIATE", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_local_bounce_immediate_call_under_extension()
                ? 0
                : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-RETURNED-ZERO-ARG-CLOSURE-PRODUCER-TERNARY-MERGE-BOUNCE-ACTUAL", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_local_bounce_actual_argument_under_extension()
                ? 0
                : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-RETURNED-ZERO-ARG-CLOSURE-PRODUCER-TERNARY-MERGE-BOUNCE-PASSTHROUGH-IMMEDIATE", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_local_bounce_passthrough_immediate_call_under_extension()
                ? 0
                : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-RETURNED-ZERO-ARG-CLOSURE-PRODUCER-TERNARY-MERGE-BOUNCE-PASSTHROUGH-ACTUAL", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_local_bounce_passthrough_actual_argument_under_extension()
                ? 0
                : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-RETURNED-ZERO-ARG-CLOSURE-PRODUCER-TERNARY-MERGE-REASSIGN-IMMEDIATE", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_local_reassign_immediate_call_under_extension()
                ? 0
                : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-RETURNED-ZERO-ARG-CLOSURE-PRODUCER-TERNARY-MERGE-REASSIGN-ACTUAL", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_local_reassign_actual_argument_under_extension()
                ? 0
                : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-RETURNED-ZERO-ARG-VOID-CLOSURE-PRODUCER-TERNARY-MERGE-BOUNCE-IMMEDIATE", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_local_bounce_immediate_call_under_extension()
                ? 0
                : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-RETURNED-ZERO-ARG-VOID-CLOSURE-PRODUCER-TERNARY-MERGE-BOUNCE-ACTUAL", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_local_bounce_actual_argument_under_extension()
                ? 0
                : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-RETURNED-ZERO-ARG-VOID-CLOSURE-PRODUCER-TERNARY-MERGE-BOUNCE-PASSTHROUGH-IMMEDIATE", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_local_bounce_passthrough_immediate_call_under_extension()
                ? 0
                : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-RETURNED-ZERO-ARG-VOID-CLOSURE-PRODUCER-TERNARY-MERGE-BOUNCE-PASSTHROUGH-ACTUAL", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_local_bounce_passthrough_actual_argument_under_extension()
                ? 0
                : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-RETURNED-ZERO-ARG-VOID-CLOSURE-PRODUCER-TERNARY-MERGE-REASSIGN-IMMEDIATE", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_local_reassign_immediate_call_under_extension()
                ? 0
                : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-RETURNED-ZERO-ARG-VOID-CLOSURE-PRODUCER-TERNARY-MERGE-REASSIGN-ACTUAL", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_local_reassign_actual_argument_under_extension()
                ? 0
                : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-RETURNED-ZERO-ARG-CLOSURE-PRODUCER-TERNARY-MERGE-RETURNED-CALL-REASSIGN-IMMEDIATE", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_returned_call_reassign_immediate_call_under_extension()
                ? 0
                : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-RETURNED-ZERO-ARG-CLOSURE-PRODUCER-TERNARY-MERGE-RETURNED-CALL-REASSIGN-ACTUAL", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_returned_call_reassign_actual_argument_under_extension()
                ? 0
                : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-RETURNED-ZERO-ARG-VOID-CLOSURE-PRODUCER-TERNARY-MERGE-RETURNED-CALL-REASSIGN-IMMEDIATE", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_returned_call_reassign_immediate_call_under_extension()
                ? 0
                : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-RETURNED-ZERO-ARG-VOID-CLOSURE-PRODUCER-TERNARY-MERGE-RETURNED-CALL-REASSIGN-ACTUAL", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_returned_call_reassign_actual_argument_under_extension()
                ? 0
                : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-RETURNED-ZERO-ARG-CLOSURE-STATEMENT-PASSTHROUGH-CALL", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_returned_zero_arg_closure_statement_passthrough_call_under_extension()
                ? 0
                : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-RETURNED-ZERO-ARG-VOID-CLOSURE-STATEMENT-PASSTHROUGH-CALL", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_returned_zero_arg_void_closure_statement_passthrough_call_under_extension()
                ? 0
                : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-RETURNED-CLOSURE-STATEMENT-PASSTHROUGH-CALL", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_returned_closure_statement_passthrough_call_under_extension()
                ? 0
                : 1;
        }
        if (strstr("LOWER-IR-CLOSURE-CAPTURE-STATIC-FNVAL", filter) != NULL) {
            return test_lower_ir_accepts_static_function_value_capture_inside_closure_under_extension()
                ? 0
                : 1;
        }
        if (strstr("LOWER-IR-CLOSURE-CAPTURE-PARAM-FNVAL", filter) != NULL) {
            return test_lower_ir_accepts_function_parameter_capture_inside_closure_under_extension()
                ? 0
                : 1;
        }
        if (strstr("LOWER-IR-PASSTHROUGH-TERNARY-CLOSURE-LOCAL-INIT", filter) != NULL) {
            return test_lower_ir_accepts_passthrough_ternary_closure_local_initializer_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-PASSTHROUGH-TERNARY-NONCAPTURING-RETURN-IMMEDIATE", filter) != NULL) {
            return test_lower_ir_accepts_passthrough_ternary_noncapturing_function_value_return_immediate_call_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-PASSTHROUGH-TERNARY-NONCAPTURING-ACTUAL", filter) != NULL) {
            return test_lower_ir_accepts_passthrough_ternary_noncapturing_function_value_actual_argument_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-PASSTHROUGH-TERNARY-CLOSURE-ACTUAL", filter) != NULL) {
            return test_lower_ir_accepts_passthrough_ternary_closure_function_value_actual_argument_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-PASSTHROUGH-TERNARY-ZERO-ACTUAL", filter) != NULL) {
            return test_lower_ir_accepts_passthrough_ternary_zero_arg_function_value_actual_argument_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-PASSTHROUGH-TERNARY-ZERO-VOID-ACTUAL", filter) != NULL) {
            return test_lower_ir_accepts_passthrough_ternary_zero_arg_void_function_value_actual_argument_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-PASSTHROUGH-TERNARY-ZERO-CLOSURE-ACTUAL", filter) != NULL) {
            return test_lower_ir_accepts_passthrough_ternary_zero_arg_closure_function_value_actual_argument_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-PASSTHROUGH-TERNARY-ZERO-VOID-CLOSURE-ACTUAL", filter) != NULL) {
            return test_lower_ir_accepts_passthrough_ternary_zero_arg_void_closure_function_value_actual_argument_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-FNVAL-FORWARD", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_local_function_value_forwarding_into_function_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-MULTI-STATIC-FNVAL-PARAM-NO-SHELL", filter) != NULL) {
            return test_lower_ir_accepts_multiple_static_function_valued_parameters_without_specialization_shell_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-PASSTHROUGH-DECL-LOCAL-FNVAL-NO-SHELL", filter) != NULL) {
            return test_lower_ir_accepts_passthrough_decl_local_function_value_forwarding_without_specialization_shell_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-PARAM-LOCAL-FNVAL-DIRECT-NO-SHELL", filter) != NULL) {
            return test_lower_ir_accepts_parameter_local_function_value_direct_call_without_specialization_shell_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-PARAM-LOCAL-ZERO-FNVAL-DIRECT-NO-SHELL", filter) != NULL) {
            return test_lower_ir_accepts_parameter_local_zero_arg_function_value_direct_call_without_specialization_shell_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-PARAM-LOCAL-CLOSURE-DIRECT-NO-SHELL", filter) != NULL) {
            return test_lower_ir_accepts_parameter_local_closure_direct_call_without_specialization_shell_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-PARAM-LOCAL-CLOSURE-FORWARD-NO-SHELL", filter) != NULL) {
            return test_lower_ir_accepts_parameter_local_closure_forward_without_specialization_shell_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-FNVAL-ZERO-FORWARD", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_local_zero_arg_function_value_forwarding_into_function_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-FNVAL-ZERO-VOID-FORWARD", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_local_zero_arg_void_function_value_forwarding_into_function_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-DIRECT-CLOSURE-LITERAL-ARG", filter) != NULL) {
            return test_lower_ir_accepts_direct_closure_literal_argument_to_function_value_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-ZERO-ARG-DIRECT-CLOSURE-LITERAL-ARG", filter) != NULL) {
            return test_lower_ir_accepts_zero_arg_direct_closure_literal_argument_to_function_value_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-VOID-DIRECT-CLOSURE-LITERAL-ARG", filter) != NULL) {
            return test_lower_ir_accepts_void_direct_closure_literal_argument_to_function_value_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-MULTI-CAPTURE-DIRECT-CLOSURE-LITERAL-ARG", filter) != NULL) {
            return test_lower_ir_accepts_multi_capture_direct_closure_literal_argument_to_function_value_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-CLOSURE-ALIAS-CHAIN-CALLABLE-OBJECT-TRANSPORT", filter) != NULL) {
            return test_lower_ir_accepts_closure_alias_chain_callable_object_transport_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-CLOSURE-DIRECT-LOCAL-CALL-CALLABLE-OBJECT-TRANSPORT", filter) != NULL) {
            return test_lower_ir_accepts_closure_direct_local_call_callable_object_transport_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-RETURNED-SINGLE-CAPTURE-CLOSURE-FORWARD", filter) != NULL) {
            return test_lower_ir_accepts_returned_single_capture_closure_forwarding_into_function_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-RETURNED-SINGLE-CAPTURE-CLOSURE-ALIAS-FORWARD", filter) != NULL) {
            return test_lower_ir_accepts_returned_single_capture_closure_alias_forwarding_into_function_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-TERNARY-CLOSURE-ACTUAL-ARGUMENT", filter) != NULL) {
            return test_lower_ir_accepts_ternary_closure_actual_argument_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-COMMA-WRAPPED-CLOSURE-FORWARD", filter) != NULL) {
            return test_lower_ir_accepts_comma_wrapped_closure_forward_into_function_value_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-ASSIGNMENT-RESULT-CLOSURE-FORWARD", filter) != NULL) {
            return test_lower_ir_accepts_assignment_result_closure_forward_into_function_value_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-COMMA-WRAPPED-CLOSURE-FORWARD", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_comma_wrapped_closure_forward_into_function_value_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-ASSIGNMENT-RESULT-CLOSURE-FORWARD", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_assignment_result_closure_forward_into_function_value_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-COMMA-WRAPPED-ZERO-CLOSURE-FORWARD", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_comma_wrapped_zero_arg_closure_forward_into_function_value_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-ASSIGNMENT-RESULT-ZERO-CLOSURE-FORWARD", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_assignment_result_zero_arg_closure_forward_into_function_value_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-COMMA-WRAPPED-ZERO-VOID-CLOSURE-FORWARD", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_comma_wrapped_zero_arg_void_closure_forward_into_function_value_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-ASSIGNMENT-RESULT-ZERO-VOID-CLOSURE-FORWARD", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_assignment_result_zero_arg_void_closure_forward_into_function_value_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-COMMA-WRAPPED-FNVAL-FORWARD", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_comma_wrapped_function_value_forwarding_into_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-ASSIGNMENT-RESULT-FNVAL-FORWARD", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_assignment_result_function_value_forwarding_into_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-COMMA-WRAPPED-ZERO-FNVAL-FORWARD", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_comma_wrapped_zero_arg_function_value_forwarding_into_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-ASSIGNMENT-RESULT-ZERO-FNVAL-FORWARD", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_assignment_result_zero_arg_function_value_forwarding_into_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-COMMA-WRAPPED-ZERO-VOID-FNVAL-FORWARD", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_comma_wrapped_zero_arg_void_function_value_forwarding_into_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-SECOND-ORDER-DYNAMIC-TERNARY-FNVAL-ACTUAL", filter) != NULL) {
            return test_lower_ir_accepts_second_order_dynamic_ternary_function_value_actual_argument_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-DYNAMIC-ASSIGNMENT-RESULT-ZERO-VOID-FNVAL-FORWARD", filter) != NULL) {
            return test_lower_ir_accepts_dynamic_assignment_result_zero_arg_void_function_value_forwarding_into_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-SECOND-ORDER-FNVAL-FORWARD", filter) != NULL) {
            return test_lower_ir_accepts_second_order_function_value_parameter_forwarding_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-SECOND-ORDER-CLOSURE-FNVAL-FORWARD", filter) != NULL) {
            return test_lower_ir_accepts_second_order_closure_function_value_parameter_forwarding_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-SECOND-ORDER-ZERO-FNVAL-FORWARD", filter) != NULL) {
            return test_lower_ir_accepts_second_order_zero_arg_function_value_parameter_forwarding_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-SECOND-ORDER-ZERO-CLOSURE-FNVAL-FORWARD", filter) != NULL) {
            return test_lower_ir_accepts_second_order_zero_arg_closure_function_value_parameter_forwarding_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-SECOND-ORDER-ZERO-VOID-FNVAL-FORWARD", filter) != NULL) {
            return test_lower_ir_accepts_second_order_zero_arg_void_function_value_parameter_forwarding_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-SECOND-ORDER-ZERO-VOID-CLOSURE-FNVAL-FORWARD", filter) != NULL) {
            return test_lower_ir_accepts_second_order_zero_arg_void_closure_function_value_parameter_forwarding_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-SECOND-ORDER-RETURNED-PASSTHROUGH-DYNAMIC-ZERO-VOID-FNVAL-FORWARD", filter) != NULL) {
            return test_lower_ir_accepts_second_order_returned_passthrough_dynamic_zero_arg_void_function_value_parameter_forwarding_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-SECOND-ORDER-RETURNED-PASSTHROUGH-DYNAMIC-ZERO-FNVAL-FORWARD", filter) != NULL) {
            return test_lower_ir_accepts_second_order_returned_passthrough_dynamic_zero_arg_function_value_parameter_forwarding_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-SECOND-ORDER-LOCAL-FNVAL-FORWARD", filter) != NULL) {
            return test_lower_ir_accepts_second_order_local_function_value_parameter_forwarding_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-SECOND-ORDER-LOCAL-REASSIGNED-FNVAL-FORWARD", filter) != NULL) {
            return test_lower_ir_accepts_second_order_local_reassigned_function_value_parameter_forwarding_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-SECOND-ORDER-LOCAL-SCALAR-REASSIGNED-FNVAL-FORWARD", filter) != NULL) {
            return test_lower_ir_accepts_second_order_local_scalar_and_reassigned_function_value_parameter_forwarding_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-SECOND-ORDER-LOCAL-WRAPPER-SCALAR-UPDATE-FNVAL-FORWARD", filter) != NULL) {
            return test_lower_ir_accepts_second_order_local_wrapper_scalar_update_function_value_parameter_forwarding_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-SECOND-ORDER-LOCAL-WRAPPER-REPEATED-SCALAR-UPDATE-FNVAL-FORWARD", filter) != NULL) {
            return test_lower_ir_accepts_second_order_local_wrapper_repeated_scalar_update_function_value_parameter_forwarding_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-THIRD-ORDER-FNVAL-FORWARD", filter) != NULL) {
            return test_lower_ir_accepts_third_order_function_value_parameter_forwarding_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-THIRD-ORDER-WRAPPER-SCALAR-UPDATE-FNVAL-FORWARD", filter) != NULL) {
            return test_lower_ir_accepts_third_order_wrapper_scalar_update_function_value_parameter_forwarding_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-THIRD-ORDER-WRAPPER-REPEATED-SCALAR-UPDATE-FNVAL-FORWARD", filter) != NULL) {
            return test_lower_ir_accepts_third_order_wrapper_repeated_scalar_update_function_value_parameter_forwarding_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-THIRD-ORDER-RETURNED-FNVAL-BIND", filter) != NULL) {
            return test_lower_ir_accepts_third_order_returned_function_value_parameter_bind_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-THIRD-ORDER-LOCAL-FNVAL-FORWARD", filter) != NULL) {
            return test_lower_ir_accepts_third_order_local_function_value_parameter_forwarding_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-SECOND-ORDER-RETURNED-FNVAL-IMM-CALL", filter) != NULL) {
            return test_lower_ir_accepts_second_order_returned_function_value_parameter_immediate_call_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-SECOND-ORDER-RETURNED-CLOSURE-FNVAL-IMM-CALL", filter) != NULL) {
            return test_lower_ir_accepts_second_order_returned_closure_function_value_parameter_immediate_call_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-SECOND-ORDER-DYNAMIC-RETURNED-FNVAL-IMM-CALL", filter) != NULL) {
            return test_lower_ir_accepts_second_order_dynamic_returned_function_value_parameter_immediate_call_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-SECOND-ORDER-RETURNED-PASSTHROUGH-DYNAMIC-FNVAL-IMM-CALL", filter) != NULL) {
            return test_lower_ir_accepts_second_order_returned_passthrough_dynamic_function_value_parameter_immediate_call_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-SECOND-ORDER-DYNAMIC-LOCAL-FNVAL-FORWARD", filter) != NULL) {
            return test_lower_ir_accepts_second_order_dynamic_local_function_value_parameter_forwarding_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-SECOND-ORDER-DYNAMIC-LOCAL-CLOSURE-FNVAL-FORWARD", filter) != NULL) {
            return test_lower_ir_accepts_second_order_dynamic_local_closure_function_value_parameter_forwarding_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-SECOND-ORDER-RETURNED-PASSTHROUGH-DYNAMIC-LOCAL-FNVAL-FORWARD", filter) != NULL) {
            return test_lower_ir_accepts_second_order_returned_passthrough_dynamic_local_function_value_parameter_forwarding_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-SECOND-ORDER-RETURNED-PASSTHROUGH-DYNAMIC-LOCAL-FNVAL-UNARY-EVAL", filter) != NULL) {
            return test_lower_ir_accepts_second_order_returned_passthrough_dynamic_local_function_value_parameter_forwarding_with_unary_helper_eval_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-SECOND-ORDER-RETURNED-PASSTHROUGH-DYNAMIC-LOCAL-FNVAL-COMPOUND-UPDATE-EVAL", filter) != NULL) {
            return test_lower_ir_accepts_second_order_returned_passthrough_dynamic_local_function_value_parameter_forwarding_with_compound_update_helper_eval_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-SECOND-ORDER-RETURNED-PASSTHROUGH-DYNAMIC-LOCAL-FNVAL-COMMA-EVAL", filter) != NULL) {
            return test_lower_ir_accepts_second_order_returned_passthrough_dynamic_local_function_value_parameter_forwarding_with_comma_helper_eval_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-SECOND-ORDER-RETURNED-PASSTHROUGH-DYNAMIC-LOCAL-FNVAL-TERNARY-EVAL", filter) != NULL) {
            return test_lower_ir_accepts_second_order_returned_passthrough_dynamic_local_function_value_parameter_forwarding_with_ternary_helper_eval_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-SECOND-ORDER-RETURNED-PASSTHROUGH-DYNAMIC-LOCAL-FNVAL-STMT-CALL-PREFIX", filter) != NULL) {
            return test_lower_ir_accepts_second_order_returned_passthrough_dynamic_local_function_value_parameter_forwarding_with_statement_call_prefix_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-SECOND-ORDER-RETURNED-PASSTHROUGH-DYNAMIC-LOCAL-FNVAL-IF-RETURN-EVAL", filter) != NULL) {
            return test_lower_ir_accepts_second_order_returned_passthrough_dynamic_local_function_value_parameter_forwarding_with_if_return_helper_eval_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-SECOND-ORDER-RETURNED-CLOSURE-FNVAL-FORWARD", filter) != NULL) {
            return test_lower_ir_accepts_second_order_returned_closure_function_value_parameter_forwarding_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-PARAM-LOCAL-ZERO-VOID-FNVAL-DIRECT-NO-SHELL", filter) != NULL) {
            return test_lower_ir_accepts_parameter_local_zero_arg_void_function_value_direct_call_without_specialization_shell_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-PARAM-LOCAL-ZERO-VOID-FNVAL-FORWARD-NO-SHELL", filter) != NULL) {
            return test_lower_ir_accepts_parameter_local_zero_arg_void_function_value_forward_without_specialization_shell_under_extension() ? 0 : 1;
        }
        if (strstr("LOWER-IR-FLOAT-GLOBAL-LITERAL-RUNTIME-INIT", filter) != NULL) {
            return test_lower_ir_lowers_float_literal_runtime_global_initializer_under_extension() ? 0 : 1;
        }
    }

    ok &= test_lower_ir_builder_rejects_declaration_block_append();
    ok &= test_lower_ir_builder_rejects_temp_allocation_on_declaration();
    ok &= test_lower_ir_builder_rejects_parameter_after_nonparameter_local();
    ok &= test_lower_ir_builder_rejects_instruction_append_after_terminator();
    ok &= test_lower_ir_builder_rejects_terminator_overwrite();
    ok &= test_lower_ir_cfg_analysis_diamond_join_temp();
    ok &= test_lower_ir_phi_placement_diamond_join_temp();
    ok &= test_lower_ir_temp_phi_candidates_diamond_join_temp();
    ok &= test_lower_ir_block_phi_candidate_lists_diamond_join_temp();
    ok &= test_lower_ir_block_successor_phi_use_lists_diamond_join_temp();
    ok &= test_lower_ir_dominator_preorder_diamond_join_temp();
    ok &= test_lower_ir_dominator_walk_diamond_join_temp();
    ok &= test_lower_ir_cfg_analysis_loop_header_join_temp();
    ok &= test_lower_ir_phi_placement_loop_header_join_temp();
    ok &= test_lower_ir_temp_phi_candidates_loop_header_join_temp();
    ok &= test_lower_ir_block_phi_candidate_lists_loop_header_join_temp();
    ok &= test_lower_ir_block_successor_phi_use_lists_loop_header_join_temp();
    ok &= test_lower_ir_temp_phi_candidates_two_carried_temps_loop_header();
    ok &= test_lower_ir_temp_phi_candidates_nested_loop_carried_temps();
    ok &= test_lower_ir_pruned_block_phi_candidate_lists_nested_loop_carried_temps();
    ok &= test_lower_ir_temp_phi_candidates_nested_same_temp_double_loop();
    ok &= test_lower_ir_temp_phi_candidates_nested_same_temp_multi_backedge_inner_loop();
    ok &= test_lower_ir_dominator_preorder_loop_header_join_temp();
    ok &= test_lower_ir_dominator_walk_loop_header_join_temp();
    ok &= test_lower_ir_cfg_analysis_multi_backedge_loop_header_join_temp();
    ok &= test_lower_ir_phi_placement_multi_backedge_loop_header_join_temp();
    ok &= test_lower_ir_temp_phi_candidates_multi_backedge_loop_header_join_temp();
    ok &= test_lower_ir_block_phi_candidate_lists_multi_backedge_loop_header_join_temp();
    ok &= test_lower_ir_block_successor_phi_use_lists_multi_backedge_loop_header_join_temp();
    ok &= test_lower_ir_dominator_preorder_multi_backedge_loop_header_join_temp();
    ok &= test_lower_ir_dominator_walk_multi_backedge_loop_header_join_temp();
    ok &= test_lower_ir_dump_explicit_memory_flow();
    ok &= test_lower_ir_bridges_callable_object_indirect_call();
    ok &= test_lower_ir_bridges_moved_callable_object_indirect_call();
    ok &= test_lower_ir_bridges_callable_object_env_and_shape_projection();
    ok &= test_lower_ir_bridges_callable_object_code_projection();
    ok &= test_lower_ir_lowers_return_parameter_to_explicit_load();
    ok &= test_lower_ir_accepts_multi_capture_direct_closure_literal_argument_to_function_value_parameter_under_extension();
    ok &= test_lower_ir_accepts_closure_alias_chain_callable_object_transport_under_extension();
    ok &= test_lower_ir_accepts_closure_direct_local_call_callable_object_transport_under_extension();
    ok &= test_lower_ir_accepts_rich_body_closure_direct_local_call_callable_object_transport_under_extension();
    ok &= test_lower_ir_accepts_returned_closure_forwarding_into_function_parameter_under_extension();
    ok &= test_lower_ir_accepts_returned_closure_alias_forwarding_into_function_parameter_under_extension();
    ok &= test_lower_ir_lowers_returned_function_value_parameter_bind_and_call_under_extension();
    ok &= test_lower_ir_lowers_dynamic_returned_function_value_parameter_immediate_call_under_extension();
    ok &= test_lower_ir_lowers_returned_closure_backed_function_value_parameter_immediate_call_under_extension();
    ok &= test_lower_ir_lowers_returned_closure_backed_function_value_parameter_bind_and_call_under_extension();
    ok &= test_lower_ir_lowers_dynamic_returned_closure_backed_function_value_parameter_immediate_call_under_extension();
    ok &= test_lower_ir_lowers_dynamic_returned_two_arg_closure_backed_function_value_parameter_immediate_call_under_extension();
    ok &= test_lower_ir_lowers_dynamic_returned_zero_arg_closure_backed_function_value_parameter_immediate_call_under_extension();
    ok &= test_lower_ir_lowers_dynamic_returned_zero_arg_void_closure_backed_function_value_parameter_immediate_call_under_extension();
    ok &= test_lower_ir_lowers_dynamic_returned_closure_backed_function_value_parameter_bind_and_call_under_extension();
    ok &= test_lower_ir_accepts_returned_single_capture_closure_forwarding_into_function_parameter_under_extension();
    ok &= test_lower_ir_accepts_direct_returned_single_capture_closure_function_value_argument_under_extension();
    ok &= test_lower_ir_accepts_direct_returned_single_capture_zero_arg_closure_function_value_argument_under_extension();
    ok &= test_lower_ir_accepts_direct_returned_single_capture_zero_arg_void_closure_function_value_argument_under_extension();
    ok &= test_lower_ir_accepts_direct_returned_multi_capture_closure_function_value_argument_under_extension();
    ok &= test_lower_ir_accepts_returned_single_capture_closure_alias_forwarding_into_function_parameter_under_extension();
    ok &= test_lower_ir_accepts_returned_single_capture_zero_arg_closure_forwarding_into_function_parameter_under_extension();
    ok &= test_lower_ir_accepts_returned_single_capture_zero_arg_void_closure_forwarding_into_function_parameter_under_extension();
    ok &= test_lower_ir_accepts_dynamic_returned_local_closure_forwarding_into_function_parameter_under_extension();
    ok &= test_lower_ir_accepts_direct_dynamic_returned_local_closure_function_value_argument_under_extension();
    ok &= test_lower_ir_accepts_direct_dynamic_returned_local_zero_arg_closure_function_value_argument_under_extension();
    ok &= test_lower_ir_accepts_direct_dynamic_returned_local_zero_arg_void_closure_function_value_argument_under_extension();
    ok &= test_lower_ir_accepts_direct_closure_literal_argument_to_function_value_parameter_under_extension();
    ok &= test_lower_ir_accepts_zero_arg_direct_closure_literal_argument_to_function_value_parameter_under_extension();
    ok &= test_lower_ir_accepts_void_direct_closure_literal_argument_to_function_value_parameter_under_extension();
    ok &= test_lower_ir_accepts_multi_capture_closure_forwarding_into_function_parameter_under_extension();
    ok &= test_lower_ir_accepts_returned_multi_capture_local_closure_bind_and_call_under_extension();
    ok &= test_lower_ir_accepts_dynamic_returned_multi_capture_local_closure_forwarding_into_function_parameter_under_extension();
    ok &= test_lower_ir_accepts_second_order_dynamic_returned_function_value_parameter_immediate_call_under_extension();
    ok &= test_lower_ir_accepts_second_order_returned_passthrough_dynamic_noncapturing_function_value_parameter_immediate_call_under_extension();
    ok &= test_lower_ir_accepts_second_order_returned_passthrough_dynamic_function_value_parameter_immediate_call_under_extension();
    ok &= test_lower_ir_accepts_second_order_local_function_value_parameter_forwarding_under_extension();
    ok &= test_lower_ir_accepts_second_order_local_reassigned_function_value_parameter_forwarding_under_extension();
    ok &= test_lower_ir_accepts_second_order_local_scalar_and_reassigned_function_value_parameter_forwarding_under_extension();
    ok &= test_lower_ir_accepts_second_order_local_wrapper_scalar_update_function_value_parameter_forwarding_under_extension();
    ok &= test_lower_ir_accepts_second_order_local_wrapper_repeated_scalar_update_function_value_parameter_forwarding_under_extension();
    ok &= test_lower_ir_accepts_second_order_zero_arg_wrapper_scalar_update_function_value_parameter_forwarding_under_extension();
    ok &= test_lower_ir_accepts_second_order_zero_arg_void_wrapper_scalar_update_function_value_parameter_forwarding_under_extension();
    ok &= test_lower_ir_accepts_second_order_returned_zero_arg_wrapper_scalar_update_function_value_parameter_immediate_call_under_extension();
    ok &= test_lower_ir_accepts_second_order_returned_zero_arg_wrapper_scalar_update_function_value_parameter_bind_call_under_extension();
    ok &= test_lower_ir_accepts_second_order_returned_zero_arg_void_wrapper_scalar_update_function_value_parameter_immediate_call_under_extension();
    ok &= test_lower_ir_accepts_second_order_returned_zero_arg_void_wrapper_scalar_update_function_value_parameter_bind_call_under_extension();
    ok &= test_lower_ir_accepts_second_order_dynamic_ternary_function_value_actual_argument_under_extension();
    ok &= test_lower_ir_accepts_second_order_closure_function_value_parameter_forwarding_under_extension();
    ok &= test_lower_ir_accepts_third_order_function_value_parameter_forwarding_under_extension();
    ok &= test_lower_ir_accepts_third_order_zero_arg_wrapper_scalar_update_function_value_parameter_forwarding_under_extension();
    ok &= test_lower_ir_accepts_third_order_zero_arg_void_wrapper_scalar_update_function_value_parameter_forwarding_under_extension();
    ok &= test_lower_ir_accepts_third_order_wrapper_scalar_update_function_value_parameter_forwarding_under_extension();
    ok &= test_lower_ir_accepts_third_order_wrapper_repeated_scalar_update_function_value_parameter_forwarding_under_extension();
    ok &= test_lower_ir_accepts_second_order_zero_arg_closure_function_value_parameter_forwarding_under_extension();
    ok &= test_lower_ir_accepts_second_order_zero_arg_void_closure_function_value_parameter_forwarding_under_extension();
    ok &= test_lower_ir_accepts_second_order_returned_closure_function_value_parameter_immediate_call_under_extension();
    ok &= test_lower_ir_accepts_second_order_dynamic_local_closure_function_value_parameter_forwarding_under_extension();
    ok &= test_lower_ir_accepts_second_order_returned_closure_function_value_parameter_forwarding_under_extension();
    ok &= test_lower_ir_accepts_parameter_local_scalar_rebind_function_value_direct_call_without_specialization_shell_under_extension();
    ok &= test_lower_ir_accepts_parameter_local_scalar_and_alias_function_value_direct_call_without_specialization_shell_under_extension();
    ok &= test_lower_ir_accepts_parameter_local_scalar_update_function_value_direct_call_without_specialization_shell_under_extension();
    ok &= test_lower_ir_accepts_parameter_local_scalar_update_and_alias_function_value_direct_call_without_specialization_shell_under_extension();
    ok &= test_lower_ir_accepts_parameter_local_call_update_function_value_direct_call_without_specialization_shell_under_extension();
    ok &= test_lower_ir_accepts_parameter_local_repeated_call_update_function_value_direct_call_without_specialization_shell_under_extension();
    ok &= test_lower_ir_accepts_parameter_local_two_callable_update_without_specialization_shell_under_extension();
    ok &= test_lower_ir_lowers_pair_return_via_hidden_result_slots_under_extension();
    ok &= test_lower_ir_lowers_pair_call_init_via_hidden_result_slots_under_extension();
    ok &= test_lower_ir_lowers_pair_call_result_as_aggregate_call_argument_under_extension();
    ok &= test_lower_ir_lowers_pair_multi_hop_return_chain_under_extension();
    ok &= test_lower_ir_lowers_nested_struct_multi_hop_return_chain_under_extension();
    ok &= test_lower_ir_lowers_float_transport_signature_under_extension();
    ok &= test_lower_ir_lowers_float_literal_transport_under_extension();
    ok &= test_lower_ir_lowers_float_global_initializer_transport_from_identifier_under_extension();
    ok &= test_lower_ir_lowers_float_global_initializer_transport_from_call_under_extension();
    ok &= test_lower_ir_lowers_float_return_transport_from_global_under_extension();
    ok &= test_lower_ir_lowers_float_return_transport_from_global_call_under_extension();
    ok &= test_lower_ir_lowers_float_global_call_chain_transport_under_extension();
    ok &= test_lower_ir_lowers_float_local_call_chain_transport_under_extension();
    ok &= test_lower_ir_rejects_global_float_operator_expression_under_extension();
    ok &= test_lower_ir_rejects_global_float_operator_expression_in_initializer_under_extension();
    ok &= test_lower_ir_rejects_global_float_call_result_in_initializer_under_extension();
    ok &= test_lower_ir_lowers_float_assignment_transport_under_extension();
    ok &= test_lower_ir_rejects_float_assignment_to_int_under_extension();
    ok &= test_lower_ir_rejects_float_if_condition_under_extension();
    ok &= test_lower_ir_rejects_float_while_condition_under_extension();
    ok &= test_lower_ir_rejects_float_for_condition_under_extension();
    ok &= test_lower_ir_accepts_recursive_float_if_condition_under_extension();
    ok &= test_lower_ir_accepts_recursive_float_while_condition_under_extension();
    ok &= test_lower_ir_accepts_recursive_float_for_condition_under_extension();
    ok &= test_lower_ir_rejects_recursive_float_condition_with_ternary_neighbor_under_extension();
    ok &= test_lower_ir_accepts_float_logical_condition_composition_under_extension();
    ok &= test_lower_ir_accepts_float_equality_compare_under_extension();
    ok &= test_lower_ir_accepts_float_inequality_compare_under_extension();
    ok &= test_lower_ir_accepts_float_relational_compare_under_extension();
    ok &= test_lower_ir_accepts_chained_float_equality_compare_under_extension();
    ok &= test_lower_ir_accepts_chained_float_relational_compare_under_extension();
    ok &= test_lower_ir_accepts_chained_float_inequality_compare_under_extension();
    ok &= test_lower_ir_accepts_chained_float_le_compare_under_extension();
    ok &= test_lower_ir_accepts_nested_muldiv_float_equality_compare_under_extension();
    ok &= test_lower_ir_accepts_nested_muldiv_float_relational_compare_under_extension();
    ok &= test_lower_ir_accepts_nested_muldiv_float_inequality_compare_under_extension();
    ok &= test_lower_ir_accepts_nested_muldiv_float_ge_compare_under_extension();
    ok &= test_lower_ir_accepts_negative_float_literal_transport_under_extension();
    ok &= test_lower_ir_accepts_unary_minus_float_identifier_transport_under_extension();
    ok &= test_lower_ir_accepts_negative_zero_float_condition_under_extension();
    ok &= test_lower_ir_accepts_negative_float_relational_compare_against_zero_under_extension();
    ok &= test_lower_ir_accepts_negative_zero_le_zero_under_extension();
    ok &= test_lower_ir_accepts_float_addition_under_extension();
    ok &= test_lower_ir_accepts_float_subtraction_under_extension();
    ok &= test_lower_ir_accepts_negative_float_addition_combo_under_extension();
    ok &= test_lower_ir_accepts_negative_float_subtraction_combo_under_extension();
    ok &= test_lower_ir_accepts_float_multiplication_under_extension();
    ok &= test_lower_ir_accepts_float_division_under_extension();
    ok &= test_lower_ir_accepts_negative_float_multiplication_combo_under_extension();
    ok &= test_lower_ir_accepts_negative_float_division_combo_under_extension();
    ok &= test_lower_ir_accepts_chained_float_addition_under_extension();
    ok &= test_lower_ir_accepts_explicit_int_from_float_conversion_under_extension();
    ok &= test_lower_ir_accepts_explicit_float_from_int_conversion_under_extension();
    ok &= test_lower_ir_accepts_explicit_float_condition_under_extension();
    ok &= test_lower_ir_accepts_recursive_explicit_float_condition_under_extension();
    ok &= test_lower_ir_accepts_explicit_float_while_condition_under_extension();
    ok &= test_lower_ir_accepts_explicit_float_logical_condition_composition_under_extension();
    ok &= test_lower_ir_accepts_explicit_int_from_float_ternary_bridge_under_extension();
    ok &= test_lower_ir_accepts_explicit_int_from_recursive_float_call_argument_bridge_under_extension();
    ok &= test_lower_ir_accepts_explicit_int_from_recursive_float_initializer_bridge_under_extension();
    ok &= test_lower_ir_accepts_explicit_int_from_recursive_float_assignment_bridge_under_extension();
    ok &= test_lower_ir_accepts_explicit_int_from_float_compare_bridge_under_extension();
    ok &= test_lower_ir_accepts_explicit_int_from_recursive_float_arithmetic_bridge_under_extension();
    ok &= test_lower_ir_accepts_explicit_float_from_recursive_int_initializer_bridge_under_extension();
    ok &= test_lower_ir_accepts_explicit_float_from_recursive_int_assignment_bridge_under_extension();
    ok &= test_lower_ir_accepts_explicit_float_from_int_compare_bridge_under_extension();
    ok &= test_lower_ir_accepts_explicit_float_from_recursive_int_arithmetic_bridge_under_extension();
    ok &= test_lower_ir_accepts_same_type_float_ternary_value_under_extension();
    ok &= test_lower_ir_accepts_float_ternary_value_assignment_to_float_under_extension();
    ok &= test_lower_ir_accepts_float_ternary_value_initializer_to_float_under_extension();
    ok &= test_lower_ir_accepts_chained_float_addition_assignment_to_float_under_extension();
    ok &= test_lower_ir_accepts_nested_float_mul_div_assignment_to_float_under_extension();
    ok &= test_lower_ir_accepts_chained_float_addition_initializer_to_float_under_extension();
    ok &= test_lower_ir_accepts_nested_float_mul_div_initializer_to_float_under_extension();
    ok &= test_lower_ir_accepts_float_ternary_value_call_argument_to_float_under_extension();
    ok &= test_lower_ir_accepts_unary_call_float_ternary_value_call_argument_to_float_under_extension();
    ok &= test_lower_ir_accepts_chained_float_addition_call_argument_to_float_under_extension();
    ok &= test_lower_ir_accepts_nested_float_mul_div_call_argument_to_float_under_extension();
    ok &= test_lower_ir_accepts_float_helper_wrapped_ternary_call_arithmetic_under_extension();
    ok &= test_lower_ir_accepts_unary_call_helper_wrapped_ternary_call_arithmetic_under_extension();
    ok &= test_lower_ir_accepts_dynamic_noncapturing_function_value_return_bind_and_call_under_extension();
    ok &= test_lower_ir_accepts_static_toplevel_noncapturing_function_value_return_under_extension();
    ok &= test_lower_ir_accepts_dynamic_noncapturing_function_value_return_immediate_call_under_extension();
    ok &= test_lower_ir_accepts_dynamic_noncapturing_two_arg_function_value_return_immediate_call_under_extension();
    ok &= test_lower_ir_accepts_dynamic_wrapped_function_value_return_under_extension();
    ok &= test_lower_ir_accepts_returned_function_value_reassignment_under_extension();
    ok &= test_lower_ir_accepts_returned_zero_arg_function_value_reassignment_under_extension();
    ok &= test_lower_ir_accepts_returned_closure_reassignment_under_extension();
    ok &= test_lower_ir_accepts_returned_zero_arg_void_closure_reassignment_under_extension();
    ok &= test_lower_ir_accepts_wrapped_function_value_reassignment_under_extension();
    ok &= test_lower_ir_accepts_assignment_result_function_value_reassignment_under_extension();
    ok &= test_lower_ir_accepts_wrapped_returned_function_value_reassignment_under_extension();
    ok &= test_lower_ir_accepts_wrapped_returned_function_value_local_initializer_under_extension();
    ok &= test_lower_ir_accepts_wrapped_returned_zero_arg_function_value_local_initializer_under_extension();
    ok &= test_lower_ir_accepts_assignment_result_returned_closure_local_initializer_under_extension();
    ok &= test_lower_ir_accepts_wrapped_returned_call_ternary_function_value_local_initializer_under_extension();
    ok &= test_lower_ir_accepts_wrapped_returned_call_ternary_closure_local_initializer_under_extension();
    ok &= test_lower_ir_accepts_wrapped_returned_call_ternary_function_value_local_bounce_under_extension();
    ok &= test_lower_ir_accepts_wrapped_returned_call_ternary_function_value_forward_under_extension();
    ok &= test_lower_ir_accepts_wrapped_returned_call_ternary_closure_local_bounce_under_extension();
    ok &= test_lower_ir_accepts_wrapped_returned_call_ternary_closure_forward_under_extension();
    ok &= test_lower_ir_accepts_wrapped_returned_call_ternary_function_value_bind_return_under_extension();
    ok &= test_lower_ir_accepts_wrapped_returned_call_ternary_function_value_bind_return_actual_under_extension();
    ok &= test_lower_ir_accepts_wrapped_returned_call_ternary_closure_bind_return_under_extension();
    ok &= test_lower_ir_accepts_wrapped_returned_call_ternary_closure_bind_return_actual_under_extension();
    ok &= test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_function_value_bind_return_under_extension();
    ok &= test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_function_value_bind_return_actual_under_extension();
    ok &= test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_function_value_bind_return_under_extension();
    ok &= test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_function_value_bind_return_actual_under_extension();
    ok &= test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_closure_bind_return_under_extension();
    ok &= test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_closure_bind_return_actual_under_extension();
    ok &= test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_closure_bind_return_under_extension();
    ok &= test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_closure_bind_return_actual_under_extension();
    ok &= test_lower_ir_accepts_wrapped_returned_call_ternary_function_value_bounce_passthrough_bind_return_under_extension();
    ok &= test_lower_ir_accepts_wrapped_returned_call_ternary_function_value_bounce_passthrough_bind_return_actual_under_extension();
    ok &= test_lower_ir_accepts_wrapped_returned_call_ternary_closure_bounce_passthrough_bind_return_under_extension();
    ok &= test_lower_ir_accepts_wrapped_returned_call_ternary_closure_bounce_passthrough_bind_return_actual_under_extension();
    ok &= test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_function_value_bounce_passthrough_bind_return_under_extension();
    ok &= test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_function_value_bounce_passthrough_bind_return_actual_under_extension();
    ok &= test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_function_value_bounce_passthrough_bind_return_under_extension();
    ok &= test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_function_value_bounce_passthrough_bind_return_actual_under_extension();
    ok &= test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_closure_bounce_passthrough_bind_return_under_extension();
    ok &= test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_closure_bounce_passthrough_bind_return_actual_under_extension();
    ok &= test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_closure_bounce_passthrough_bind_return_under_extension();
    ok &= test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_closure_bounce_passthrough_bind_return_actual_under_extension();
    ok &= test_lower_ir_accepts_wrapped_returned_call_ternary_function_value_statement_reassign_bind_return_under_extension();
    ok &= test_lower_ir_accepts_wrapped_returned_call_ternary_function_value_statement_reassign_bind_return_actual_under_extension();
    ok &= test_lower_ir_accepts_wrapped_returned_call_ternary_closure_statement_reassign_bind_return_under_extension();
    ok &= test_lower_ir_accepts_wrapped_returned_call_ternary_closure_statement_reassign_bind_return_actual_under_extension();
    ok &= test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_function_value_statement_reassign_bind_return_under_extension();
    ok &= test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_function_value_statement_reassign_bind_return_actual_under_extension();
    ok &= test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_function_value_statement_reassign_bind_return_under_extension();
    ok &= test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_function_value_statement_reassign_bind_return_actual_under_extension();
    ok &= test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_closure_statement_reassign_bind_return_under_extension();
    ok &= test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_closure_statement_reassign_bind_return_actual_under_extension();
    ok &= test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_closure_statement_reassign_bind_return_under_extension();
    ok &= test_lower_ir_accepts_wrapped_returned_call_ternary_zero_arg_void_closure_statement_reassign_bind_return_actual_under_extension();
    ok &= test_lower_ir_accepts_assignment_result_returned_closure_reassignment_under_extension();
    ok &= test_lower_ir_accepts_returned_closure_assignment_then_return_under_extension();
    ok &= test_lower_ir_accepts_dynamic_returned_closure_rebind_then_return_under_extension();
    ok &= test_lower_ir_accepts_dynamic_returned_multi_capture_closure_rebind_then_return_under_extension();
    ok &= test_lower_ir_accepts_dynamic_returned_zero_arg_void_closure_rebind_then_return_under_extension();
    ok &= test_lower_ir_accepts_dynamic_returned_closure_passthrough_bind_then_return_under_extension();
    ok &= test_lower_ir_accepts_dynamic_returned_closure_two_hop_passthrough_bind_then_return_under_extension();
    ok &= test_lower_ir_accepts_dynamic_returned_closure_two_hop_passthrough_immediate_call_under_extension();
    ok &= test_lower_ir_accepts_dynamic_returned_closure_three_hop_passthrough_bind_then_return_under_extension();
    ok &= test_lower_ir_accepts_dynamic_returned_closure_three_hop_passthrough_immediate_call_under_extension();
    ok &= test_lower_ir_accepts_dynamic_returned_closure_three_hop_passthrough_actual_argument_under_extension();
    ok &= test_lower_ir_accepts_dynamic_returned_closure_local_bounce_immediate_call_under_extension();
    ok &= test_lower_ir_accepts_dynamic_returned_closure_local_bounce_actual_argument_under_extension();
    ok &= test_lower_ir_accepts_dynamic_returned_closure_producer_ternary_merge_bind_and_call_under_extension();
    ok &= test_lower_ir_accepts_dynamic_returned_closure_producer_ternary_merge_immediate_call_under_extension();
    ok &= test_lower_ir_accepts_dynamic_returned_closure_returned_call_ternary_merge_immediate_call_under_extension();
    ok &= test_lower_ir_accepts_dynamic_returned_closure_returned_call_ternary_merge_local_bounce_immediate_call_under_extension();
    ok &= test_lower_ir_accepts_dynamic_returned_closure_returned_call_ternary_merge_bind_return_immediate_call_under_extension();
    ok &= test_lower_ir_accepts_dynamic_returned_closure_returned_call_ternary_merge_bind_return_actual_argument_under_extension();
    ok &= test_lower_ir_accepts_dynamic_returned_closure_returned_call_ternary_merge_bounce_passthrough_immediate_call_under_extension();
    ok &= test_lower_ir_accepts_dynamic_returned_closure_returned_call_ternary_merge_bounce_passthrough_bind_return_immediate_call_under_extension();
    ok &= test_lower_ir_accepts_dynamic_returned_closure_returned_call_ternary_merge_bounce_passthrough_bind_return_actual_argument_under_extension();
    ok &= test_lower_ir_accepts_dynamic_returned_closure_returned_call_ternary_merge_deeper_chain_bind_return_immediate_call_under_extension();
    ok &= test_lower_ir_accepts_dynamic_returned_closure_producer_ternary_merge_actual_argument_under_extension();
    ok &= test_lower_ir_accepts_dynamic_returned_closure_producer_ternary_merge_local_bounce_immediate_call_under_extension();
    ok &= test_lower_ir_accepts_dynamic_returned_closure_producer_ternary_merge_local_bounce_actual_argument_under_extension();
    ok &= test_lower_ir_accepts_dynamic_returned_closure_producer_ternary_merge_local_bounce_passthrough_immediate_call_under_extension();
    ok &= test_lower_ir_accepts_dynamic_returned_closure_producer_ternary_merge_local_bounce_passthrough_actual_argument_under_extension();
    ok &= test_lower_ir_accepts_dynamic_returned_closure_producer_ternary_merge_local_reassign_immediate_call_under_extension();
    ok &= test_lower_ir_accepts_dynamic_returned_closure_producer_ternary_merge_local_reassign_actual_argument_under_extension();
    ok &= test_lower_ir_accepts_dynamic_returned_closure_producer_ternary_merge_returned_call_reassign_immediate_call_under_extension();
    ok &= test_lower_ir_accepts_dynamic_returned_closure_producer_ternary_merge_returned_call_reassign_actual_argument_under_extension();
    ok &= test_lower_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_immediate_call_under_extension();
    ok &= test_lower_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_actual_argument_under_extension();
    ok &= test_lower_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_immediate_call_under_extension();
    ok &= test_lower_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_actual_argument_under_extension();
    ok &= test_lower_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_local_bounce_immediate_call_under_extension();
    ok &= test_lower_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_local_bounce_actual_argument_under_extension();
    ok &= test_lower_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_local_bounce_passthrough_immediate_call_under_extension();
    ok &= test_lower_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_local_bounce_passthrough_actual_argument_under_extension();
    ok &= test_lower_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_local_reassign_immediate_call_under_extension();
    ok &= test_lower_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_local_reassign_actual_argument_under_extension();
    ok &= test_lower_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_local_bounce_immediate_call_under_extension();
    ok &= test_lower_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_local_bounce_actual_argument_under_extension();
    ok &= test_lower_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_local_bounce_passthrough_immediate_call_under_extension();
    ok &= test_lower_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_local_bounce_passthrough_actual_argument_under_extension();
    ok &= test_lower_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_local_reassign_immediate_call_under_extension();
    ok &= test_lower_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_local_reassign_actual_argument_under_extension();
    ok &= test_lower_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_returned_call_reassign_immediate_call_under_extension();
    ok &= test_lower_ir_accepts_dynamic_returned_zero_arg_closure_producer_ternary_merge_returned_call_reassign_actual_argument_under_extension();
    ok &= test_lower_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_returned_call_reassign_immediate_call_under_extension();
    ok &= test_lower_ir_accepts_dynamic_returned_zero_arg_void_closure_producer_ternary_merge_returned_call_reassign_actual_argument_under_extension();
    ok &= test_lower_ir_accepts_dynamic_returned_zero_arg_closure_statement_passthrough_call_under_extension();
    ok &= test_lower_ir_accepts_dynamic_returned_zero_arg_void_closure_statement_passthrough_call_under_extension();
    ok &= test_lower_ir_accepts_dynamic_returned_closure_statement_passthrough_call_under_extension();
    ok &= test_lower_ir_accepts_static_function_value_capture_inside_closure_under_extension();
    ok &= test_lower_ir_accepts_function_parameter_capture_inside_closure_under_extension();
    ok &= test_lower_ir_accepts_passthrough_ternary_closure_local_initializer_under_extension();
    ok &= test_lower_ir_accepts_passthrough_ternary_noncapturing_function_value_return_immediate_call_under_extension();
    ok &= test_lower_ir_accepts_passthrough_ternary_noncapturing_function_value_actual_argument_under_extension();
    ok &= test_lower_ir_accepts_passthrough_ternary_closure_function_value_actual_argument_under_extension();
    ok &= test_lower_ir_accepts_passthrough_ternary_zero_arg_function_value_actual_argument_under_extension();
    ok &= test_lower_ir_accepts_passthrough_ternary_zero_arg_void_function_value_actual_argument_under_extension();
    ok &= test_lower_ir_accepts_passthrough_ternary_zero_arg_closure_function_value_actual_argument_under_extension();
    ok &= test_lower_ir_accepts_passthrough_ternary_zero_arg_void_closure_function_value_actual_argument_under_extension();
    ok &= test_lower_ir_accepts_ternary_closure_function_value_return_immediate_call_under_extension();
    ok &= test_lower_ir_accepts_direct_dynamic_returned_multi_capture_closure_immediate_call_under_extension();
    ok &= test_lower_ir_accepts_dynamic_local_function_value_forwarding_into_function_parameter_under_extension();
    ok &= test_lower_ir_accepts_multiple_static_function_valued_parameters_without_specialization_shell_under_extension();
    ok &= test_lower_ir_accepts_dynamic_local_zero_arg_function_value_forwarding_into_function_parameter_under_extension();
    ok &= test_lower_ir_accepts_dynamic_local_zero_arg_void_function_value_forwarding_into_function_parameter_under_extension();
    ok &= test_lower_ir_accepts_dynamic_runtime_closure_local_forward_into_function_parameter_under_extension();
    ok &= test_lower_ir_accepts_dynamic_runtime_zero_arg_void_closure_local_forward_into_function_parameter_under_extension();
    ok &= test_lower_ir_accepts_dynamic_function_value_assignment_then_forward_into_function_parameter_under_extension();
    ok &= test_lower_ir_accepts_returned_single_capture_closure_forwarding_into_function_parameter_under_extension();
    ok &= test_lower_ir_accepts_returned_single_capture_closure_alias_forwarding_into_function_parameter_under_extension();
    ok &= test_lower_ir_accepts_ternary_closure_actual_argument_under_extension();
    ok &= test_lower_ir_accepts_comma_wrapped_closure_forward_into_function_value_parameter_under_extension();
    ok &= test_lower_ir_accepts_assignment_result_closure_forward_into_function_value_parameter_under_extension();
    ok &= test_lower_ir_accepts_dynamic_comma_wrapped_closure_forward_into_function_value_parameter_under_extension();
    ok &= test_lower_ir_accepts_dynamic_assignment_result_closure_forward_into_function_value_parameter_under_extension();
    ok &= test_lower_ir_accepts_dynamic_comma_wrapped_zero_arg_closure_forward_into_function_value_parameter_under_extension();
    ok &= test_lower_ir_accepts_dynamic_assignment_result_zero_arg_closure_forward_into_function_value_parameter_under_extension();
    ok &= test_lower_ir_accepts_dynamic_comma_wrapped_zero_arg_void_closure_forward_into_function_value_parameter_under_extension();
    ok &= test_lower_ir_accepts_dynamic_assignment_result_zero_arg_void_closure_forward_into_function_value_parameter_under_extension();
    ok &= test_lower_ir_accepts_dynamic_comma_wrapped_function_value_forwarding_into_parameter_under_extension();
    ok &= test_lower_ir_accepts_dynamic_assignment_result_function_value_forwarding_into_parameter_under_extension();
    ok &= test_lower_ir_accepts_dynamic_comma_wrapped_zero_arg_function_value_forwarding_into_parameter_under_extension();
    ok &= test_lower_ir_accepts_dynamic_assignment_result_zero_arg_function_value_forwarding_into_parameter_under_extension();
    ok &= test_lower_ir_accepts_dynamic_comma_wrapped_zero_arg_void_function_value_forwarding_into_parameter_under_extension();
    ok &= test_lower_ir_accepts_dynamic_assignment_result_zero_arg_void_function_value_forwarding_into_parameter_under_extension();
    ok &= test_lower_ir_accepts_returned_call_ternary_function_value_assignment_then_direct_call_under_extension();
    ok &= test_lower_ir_accepts_returned_call_ternary_closure_assignment_then_direct_call_under_extension();
    ok &= test_lower_ir_accepts_float_helper_wrapped_ternary_call_compare_under_extension();
    ok &= test_lower_ir_accepts_unary_call_helper_wrapped_ternary_call_compare_under_extension();
    ok &= test_lower_ir_accepts_nested_float_mul_div_under_extension();
    ok &= test_lower_ir_rejects_mixed_float_int_arithmetic_under_extension();
    ok &= test_lower_ir_rejects_float_literal_int_arithmetic_under_extension();
    ok &= test_lower_ir_rejects_float_call_int_arithmetic_under_extension();
    ok &= test_lower_ir_rejects_negative_float_call_int_arithmetic_under_extension();
    ok &= test_lower_ir_rejects_global_float_int_condition_under_extension();
    ok &= test_lower_ir_rejects_float_call_int_condition_under_extension();
    ok &= test_lower_ir_rejects_negative_float_call_int_condition_under_extension();
    ok &= test_lower_ir_rejects_nested_float_tree_plus_int_condition_under_extension();
    ok &= test_lower_ir_rejects_nested_float_muldiv_plus_int_condition_under_extension();
    ok &= test_lower_ir_rejects_float_compare_against_int_under_extension();
    ok &= test_lower_ir_rejects_float_ternary_value_under_extension();
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
    ok &= test_lower_ir_lowers_float_literal_runtime_global_initializer_under_extension();
    ok &= test_lower_ir_lowers_runtime_global_startup_with_branchy_main();
    ok &= test_lower_ir_lowers_program_initializer_fallback();
    ok &= test_lower_ir_lowers_runtime_global_array_initializer_with_short_circuit_and_calls();
    ok &= test_lower_ir_lowers_multiple_runtime_global_initializers_after_branchy_helper_growth();
    ok &= test_lower_ir_omits_synthetic_zero_result_after_void_call_statement();
    ok &= test_lower_ir_lowers_1d_array_initializer_mixing_nested_and_scalar_items();
    ok &= test_lower_ir_accepts_2d_array_initializer_mixing_nested_and_scalar_items();

    if (!ok) {
        return 1;
    }

    printf("[lower-ir-reg] PASS\n");
    return 0;
}
