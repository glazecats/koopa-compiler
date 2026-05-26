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
    ParserError parse_err;
    SemanticError sema_err;
    SemanticOptions sema_options;
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
    memset(&sema_options, 0, sizeof(sema_options));
    sema_options.allow_extension_features = 1;

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

    if (!semantic_analyze_program_with_options(&ast_program, &sema_options, &sema_err)) {
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
    ok &= test_lower_ir_lowers_return_parameter_to_explicit_load();
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
