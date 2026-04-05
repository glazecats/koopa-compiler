#include "ast.h"
#include "ir.h"
#include "ir_pass.h"
#include "lexer.h"
#include "parser.h"
#include "semantic.h"

#include <limits.h>
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
        fprintf(stderr, "[ir-pass] FAIL: lexer failed for input\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (!parser_parse_translation_unit_ast(&tokens, &program, &parse_err)) {
        fprintf(stderr,
            "[ir-pass] FAIL: parser failed at %d:%d: %s\n",
            parse_err.line,
            parse_err.column,
            parse_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (!semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
            "[ir-pass] FAIL: semantic failed at %d:%d: %s\n",
            sema_err.line,
            sema_err.column,
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (!ir_lower_program(&program, out_program, &ir_err)) {
        fprintf(stderr,
            "[ir-pass] FAIL: IR lowering failed at %d:%d: %s\n",
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

static int expect_error_fragment(const char *case_id,
    const IrError *error,
    const char *expected_fragment) {
    if (!case_id || !error || !expected_fragment) {
        return 0;
    }

    if (!strstr(error->message, expected_fragment)) {
        fprintf(stderr,
            "[ir-pass] FAIL: %s mismatch\nexpected fragment: %s\nactual: %s\n",
            case_id,
            expected_fragment,
            error->message);
        return 0;
    }

    return 1;
}

static IrBasicBlock *find_first_branch_block_mut(IrProgram *program) {
    size_t function_index;

    if (!program) {
        return NULL;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        IrFunction *function = &program->functions[function_index];
        size_t block_index;

        if (!function->has_body || !function->blocks) {
            continue;
        }

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            IrBasicBlock *block = &function->blocks[block_index];

            if (block->has_terminator && block->terminator.kind == IR_TERM_BRANCH) {
                return block;
            }
        }
    }

    return NULL;
}

static IrValueRef make_immediate_value(long long value) {
    IrValueRef ref;

    ref.kind = IR_VALUE_IMMEDIATE;
    ref.immediate = value;
    ref.id = 0;
    return ref;
}

static IrValueRef make_temp_value(size_t temp_id) {
    IrValueRef ref;

    ref.kind = IR_VALUE_TEMP;
    ref.immediate = 0;
    ref.id = temp_id;
    return ref;
}

static IrValueRef make_local_value(size_t local_id) {
    IrValueRef ref;

    ref.kind = IR_VALUE_LOCAL;
    ref.immediate = 0;
    ref.id = local_id;
    return ref;
}

static int rewrite_as_multi_def_join_value_program(IrProgram *program,
    IrValueRef then_value,
    IrValueRef else_value,
    size_t *out_join_block_id) {
    IrFunction *function;
    IrBasicBlock *branch_block;
    IrBasicBlock *then_block;
    IrBasicBlock *else_block;
    IrBasicBlock *join_block;
    size_t then_target;
    size_t else_target;
    size_t join_target;
    IrInstruction *then_instruction;
    IrInstruction *else_instruction;

    if (!program || program->function_count == 0) {
        return 0;
    }

    function = &program->functions[0];
    branch_block = find_first_branch_block_mut(program);
    if (!function->has_body || !function->blocks || !branch_block
        || branch_block->terminator.kind != IR_TERM_BRANCH) {
        return 0;
    }

    then_target = branch_block->terminator.as.branch.then_target;
    else_target = branch_block->terminator.as.branch.else_target;
    if (then_target >= function->block_count || else_target >= function->block_count) {
        return 0;
    }

    then_block = &function->blocks[then_target];
    else_block = &function->blocks[else_target];
    if (!then_block->has_terminator || !else_block->has_terminator
        || then_block->terminator.kind != IR_TERM_JUMP
        || else_block->terminator.kind != IR_TERM_JUMP
        || then_block->terminator.as.jump_target != else_block->terminator.as.jump_target) {
        return 0;
    }

    join_target = then_block->terminator.as.jump_target;
    if (join_target >= function->block_count) {
        return 0;
    }
    if (then_block->instruction_count != 0 || else_block->instruction_count != 0) {
        return 0;
    }

    join_block = &function->blocks[join_target];
    then_instruction = (IrInstruction *)calloc(1, sizeof(IrInstruction));
    else_instruction = (IrInstruction *)calloc(1, sizeof(IrInstruction));
    if (!then_instruction || !else_instruction) {
        free(then_instruction);
        free(else_instruction);
        return 0;
    }

    then_instruction[0].kind = IR_INSTR_MOV;
    then_instruction[0].result = make_temp_value(0);
    then_instruction[0].as.mov_value = then_value;

    else_instruction[0].kind = IR_INSTR_MOV;
    else_instruction[0].result = make_temp_value(0);
    else_instruction[0].as.mov_value = else_value;

    then_block->instructions = then_instruction;
    then_block->instruction_count = 1;
    then_block->instruction_capacity = 1;
    else_block->instructions = else_instruction;
    else_block->instruction_count = 1;
    else_block->instruction_capacity = 1;

    join_block->terminator.kind = IR_TERM_RETURN;
    join_block->terminator.as.return_value = make_temp_value(0);
    function->next_temp_id = 1;

    if (out_join_block_id) {
        *out_join_block_id = join_target;
    }
    return 1;
}

static int rewrite_as_multi_def_join_temp_program(IrProgram *program, size_t *out_join_block_id) {
    return rewrite_as_multi_def_join_value_program(program,
        make_immediate_value(1),
        make_immediate_value(2),
        out_join_block_id);
}

static int rewrite_branch_successors_to_same_temp_return(IrProgram *program,
    size_t *out_branch_block_id,
    size_t *out_temp_id) {
    IrFunction *function;
    IrBasicBlock *branch_block;
    IrBasicBlock *then_block;
    IrBasicBlock *else_block;
    IrInstruction *new_instructions;
    size_t temp_id;

    if (!program || program->function_count == 0) {
        return 0;
    }

    function = &program->functions[0];
    branch_block = find_first_branch_block_mut(program);
    if (!function->has_body || !function->blocks || !branch_block
        || branch_block->terminator.kind != IR_TERM_BRANCH) {
        return 0;
    }
    if (branch_block->terminator.as.branch.then_target >= function->block_count
        || branch_block->terminator.as.branch.else_target >= function->block_count) {
        return 0;
    }

    then_block = &function->blocks[branch_block->terminator.as.branch.then_target];
    else_block = &function->blocks[branch_block->terminator.as.branch.else_target];
    if (then_block->instruction_count != 0 || else_block->instruction_count != 0
        || !then_block->has_terminator || !else_block->has_terminator
        || then_block->terminator.kind != IR_TERM_RETURN
        || else_block->terminator.kind != IR_TERM_RETURN) {
        return 0;
    }

    temp_id = function->next_temp_id;
    new_instructions = (IrInstruction *)realloc(branch_block->instructions,
        (branch_block->instruction_count + 1) * sizeof(IrInstruction));
    if (!new_instructions) {
        return 0;
    }

    branch_block->instructions = new_instructions;
    branch_block->instructions[branch_block->instruction_count].kind = IR_INSTR_MOV;
    branch_block->instructions[branch_block->instruction_count].result = make_temp_value(temp_id);
    branch_block->instructions[branch_block->instruction_count].as.mov_value = make_immediate_value(7);
    branch_block->instruction_count += 1;
    branch_block->instruction_capacity = branch_block->instruction_count;

    then_block->terminator.as.return_value = make_temp_value(temp_id);
    else_block->terminator.as.return_value = make_temp_value(temp_id);
    function->next_temp_id = temp_id + 1;

    if (out_branch_block_id) {
        *out_branch_block_id = branch_block->id;
    }
    if (out_temp_id) {
        *out_temp_id = temp_id;
    }
    return 1;
}

static int rewrite_branch_successors_to_equivalent_temp_returns(IrProgram *program,
    IrValueRef first_source,
    IrValueRef second_source,
    IrValueRef *out_expected_value) {
    IrFunction *function;
    IrBasicBlock *branch_block;
    IrBasicBlock *then_block;
    IrBasicBlock *else_block;
    IrInstruction *new_instructions;
    size_t first_temp_id;
    size_t second_temp_id;

    if (!program || program->function_count == 0) {
        return 0;
    }

    function = &program->functions[0];
    branch_block = find_first_branch_block_mut(program);
    if (!function->has_body || !function->blocks || !branch_block
        || branch_block->terminator.kind != IR_TERM_BRANCH) {
        return 0;
    }
    if (branch_block->terminator.as.branch.then_target >= function->block_count
        || branch_block->terminator.as.branch.else_target >= function->block_count) {
        return 0;
    }

    then_block = &function->blocks[branch_block->terminator.as.branch.then_target];
    else_block = &function->blocks[branch_block->terminator.as.branch.else_target];
    if (then_block->instruction_count != 0 || else_block->instruction_count != 0
        || !then_block->has_terminator || !else_block->has_terminator
        || then_block->terminator.kind != IR_TERM_RETURN
        || else_block->terminator.kind != IR_TERM_RETURN) {
        return 0;
    }

    first_temp_id = function->next_temp_id;
    second_temp_id = function->next_temp_id + 1;
    new_instructions = (IrInstruction *)realloc(branch_block->instructions,
        (branch_block->instruction_count + 2) * sizeof(IrInstruction));
    if (!new_instructions) {
        return 0;
    }

    branch_block->instructions = new_instructions;
    branch_block->instructions[branch_block->instruction_count].kind = IR_INSTR_MOV;
    branch_block->instructions[branch_block->instruction_count].result = make_temp_value(first_temp_id);
    branch_block->instructions[branch_block->instruction_count].as.mov_value = first_source;
    branch_block->instructions[branch_block->instruction_count + 1].kind = IR_INSTR_MOV;
    branch_block->instructions[branch_block->instruction_count + 1].result = make_temp_value(second_temp_id);
    branch_block->instructions[branch_block->instruction_count + 1].as.mov_value = second_source;
    branch_block->instruction_count += 2;
    branch_block->instruction_capacity = branch_block->instruction_count;

    then_block->terminator.as.return_value = make_temp_value(first_temp_id);
    else_block->terminator.as.return_value = make_temp_value(second_temp_id);
    function->next_temp_id = second_temp_id + 1;

    if (out_expected_value) {
        if (first_source.kind == IR_VALUE_IMMEDIATE && second_source.kind == IR_VALUE_IMMEDIATE
            && first_source.immediate == second_source.immediate) {
            *out_expected_value = first_source;
        } else {
            *out_expected_value = first_source;
        }
    }
    return 1;
}

static int rewrite_as_single_use_call_arg_copy_program(IrProgram *program) {
    IrFunction *function;
    IrBasicBlock *block;
    IrInstruction *new_instructions;
    IrValueRef *args;
    char *callee_name;

    if (!program || program->function_count < 2) {
        return 0;
    }

    function = &program->functions[1];
    if (!function->has_body || !function->blocks || function->block_count == 0) {
        return 0;
    }

    block = &function->blocks[0];
    new_instructions = (IrInstruction *)realloc(block->instructions, 2 * sizeof(IrInstruction));
    if (!new_instructions) {
        return 0;
    }
    args = (IrValueRef *)calloc(1, sizeof(IrValueRef));
    callee_name = (char *)malloc(2);
    if (!args || !callee_name) {
        free(args);
        free(callee_name);
        return 0;
    }
    memcpy(callee_name, "g", 2);

    block->instructions = new_instructions;
    memset(block->instructions, 0, 2 * sizeof(IrInstruction));

    block->instructions[0].kind = IR_INSTR_MOV;
    block->instructions[0].result = make_temp_value(0);
    block->instructions[0].as.mov_value = make_immediate_value(7);

    args[0] = make_temp_value(0);
    block->instructions[1].kind = IR_INSTR_CALL;
    block->instructions[1].result = make_temp_value(1);
    block->instructions[1].as.call.callee_name = callee_name;
    block->instructions[1].as.call.args = args;
    block->instructions[1].as.call.arg_count = 1;

    block->instruction_count = 2;
    block->instruction_capacity = 2;
    block->has_terminator = 1;
    block->terminator.kind = IR_TERM_RETURN;
    block->terminator.as.return_value = make_temp_value(1);
    function->next_temp_id = 2;
    return 1;
}

static int rewrite_branch_successors_to_local_constant_returns(IrProgram *program,
    long long then_value,
    long long else_value) {
    IrFunction *function;
    IrBasicBlock *branch_block;
    IrBasicBlock *then_block;
    IrBasicBlock *else_block;
    IrInstruction *then_instructions;
    IrInstruction *else_instructions;
    size_t then_temp_id;
    size_t else_temp_id;

    if (!program || program->function_count == 0) {
        return 0;
    }

    function = &program->functions[0];
    branch_block = find_first_branch_block_mut(program);
    if (!function->has_body || !function->blocks || !branch_block
        || branch_block->terminator.kind != IR_TERM_BRANCH) {
        return 0;
    }
    if (branch_block->terminator.as.branch.then_target >= function->block_count
        || branch_block->terminator.as.branch.else_target >= function->block_count) {
        return 0;
    }

    then_block = &function->blocks[branch_block->terminator.as.branch.then_target];
    else_block = &function->blocks[branch_block->terminator.as.branch.else_target];
    if (then_block->instruction_count != 0 || else_block->instruction_count != 0
        || !then_block->has_terminator || !else_block->has_terminator
        || then_block->terminator.kind != IR_TERM_RETURN
        || else_block->terminator.kind != IR_TERM_RETURN) {
        return 0;
    }

    then_instructions = (IrInstruction *)calloc(1, sizeof(IrInstruction));
    else_instructions = (IrInstruction *)calloc(1, sizeof(IrInstruction));
    if (!then_instructions || !else_instructions) {
        free(then_instructions);
        free(else_instructions);
        return 0;
    }

    then_temp_id = function->next_temp_id;
    else_temp_id = function->next_temp_id + 1;

    then_instructions[0].kind = IR_INSTR_MOV;
    then_instructions[0].result = make_temp_value(then_temp_id);
    then_instructions[0].as.mov_value = make_immediate_value(then_value);
    then_block->instructions = then_instructions;
    then_block->instruction_count = 1;
    then_block->instruction_capacity = 1;
    then_block->terminator.as.return_value = make_temp_value(then_temp_id);

    else_instructions[0].kind = IR_INSTR_MOV;
    else_instructions[0].result = make_temp_value(else_temp_id);
    else_instructions[0].as.mov_value = make_immediate_value(else_value);
    else_block->instructions = else_instructions;
    else_block->instruction_count = 1;
    else_block->instruction_capacity = 1;
    else_block->terminator.as.return_value = make_temp_value(else_temp_id);

    function->next_temp_id = else_temp_id + 1;
    return 1;
}

typedef int (*IrPassProgramSetupFn)(IrProgram *program);

typedef struct {
    const char *case_id;
    const char *source;
    IrPassProgramSetupFn setup;
} IrPassFixedPointCase;

static int setup_post_dce_same_return_fixed_point_case(IrProgram *program) {
    return rewrite_branch_successors_to_local_constant_returns(program, 9, 9);
}

static int expect_default_pipeline_fixed_point_case(const IrPassFixedPointCase *test_case) {
    IrProgram program;
    IrError pass_error;
    IrError verify_error;
    char *first_dump;
    char *second_dump;

    if (!test_case || !test_case->case_id || !test_case->source) {
        return 0;
    }

    first_dump = NULL;
    second_dump = NULL;
    if (!lower_source_to_ir_program(test_case->source, &program)) {
        return 0;
    }

    if (test_case->setup && !test_case->setup(&program)) {
        fprintf(stderr,
            "[ir-pass] FAIL: %s setup failed\n",
            test_case->case_id);
        ir_program_free(&program);
        return 0;
    }

    if (!ir_verify_program(&program, &verify_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: %s constructed input failed verifier at %d:%d: %s\n",
            test_case->case_id,
            verify_error.line,
            verify_error.column,
            verify_error.message);
        ir_program_free(&program);
        return 0;
    }

    if (!ir_pass_run_default_pipeline(&program, &pass_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: %s first default-pipeline run failed: %s\n",
            test_case->case_id,
            pass_error.message);
        ir_program_free(&program);
        return 0;
    }

    if (!ir_verify_program(&program, &verify_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: %s first default-pipeline output failed verifier at %d:%d: %s\n",
            test_case->case_id,
            verify_error.line,
            verify_error.column,
            verify_error.message);
        ir_program_free(&program);
        return 0;
    }

    if (!ir_dump_program(&program, &first_dump) || !first_dump) {
        fprintf(stderr,
            "[ir-pass] FAIL: %s could not dump first default-pipeline output\n",
            test_case->case_id);
        ir_program_free(&program);
        return 0;
    }

    if (!ir_pass_run_default_pipeline(&program, &pass_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: %s second default-pipeline run failed: %s\n",
            test_case->case_id,
            pass_error.message);
        free(first_dump);
        ir_program_free(&program);
        return 0;
    }

    if (!ir_verify_program(&program, &verify_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: %s second default-pipeline output failed verifier at %d:%d: %s\n",
            test_case->case_id,
            verify_error.line,
            verify_error.column,
            verify_error.message);
        free(first_dump);
        ir_program_free(&program);
        return 0;
    }

    if (!ir_dump_program(&program, &second_dump) || !second_dump) {
        fprintf(stderr,
            "[ir-pass] FAIL: %s could not dump second default-pipeline output\n",
            test_case->case_id);
        free(first_dump);
        ir_program_free(&program);
        return 0;
    }

    if (strcmp(first_dump, second_dump) != 0) {
        fprintf(stderr,
            "[ir-pass] FAIL: %s default pipeline was not at a fixed point after first run\n"
            "after first run:\n%s\n"
            "after second run:\n%s\n",
            test_case->case_id,
            first_dump,
            second_dump);
        free(second_dump);
        free(first_dump);
        ir_program_free(&program);
        return 0;
    }

    free(second_dump);
    free(first_dump);
    ir_program_free(&program);
    return 1;
}

static int resolve_simple_block_return_value_for_test(const IrBasicBlock *block, IrValueRef *out_value) {
    if (!block || !out_value || !block->has_terminator || block->terminator.kind != IR_TERM_RETURN) {
        return 0;
    }

    if (block->instruction_count == 0) {
        *out_value = block->terminator.as.return_value;
        return 1;
    }

    if (block->instruction_count == 1
        && block->instructions[0].kind == IR_INSTR_MOV
        && block->instructions[0].result.kind == IR_VALUE_TEMP
        && block->terminator.as.return_value.kind == IR_VALUE_TEMP
        && block->instructions[0].result.id == block->terminator.as.return_value.id) {
        *out_value = block->instructions[0].as.mov_value;
        return 1;
    }

    return 0;
}

static int is_foldable_binary_op(IrBinaryOp op) {
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
        return 0;
    }
}

static int is_safely_foldable_immediate_binary(IrBinaryOp op, long long lhs, long long rhs) {
    const int shift_width = (int)(sizeof(long long) * 8u);
    long long ignored_value;

    switch (op) {
    case IR_BINARY_ADD:
        return !__builtin_add_overflow(lhs, rhs, &ignored_value);
    case IR_BINARY_SUB:
        return !__builtin_sub_overflow(lhs, rhs, &ignored_value);
    case IR_BINARY_MUL:
        return !__builtin_mul_overflow(lhs, rhs, &ignored_value);
    case IR_BINARY_DIV:
    case IR_BINARY_MOD:
        if (rhs == 0) {
            return 0;
        }
        return !(lhs == LLONG_MIN && rhs == -1);
    case IR_BINARY_SHIFT_LEFT:
        if (rhs < 0 || rhs >= shift_width || lhs < 0) {
            return 0;
        }
        return (unsigned long long)lhs <= ((unsigned long long)LLONG_MAX >> (unsigned long long)rhs);
    case IR_BINARY_SHIFT_RIGHT:
        return rhs >= 0 && rhs < shift_width && lhs >= 0;
    default:
        return is_foldable_binary_op(op);
    }
}

static int is_dead_temp_definition_kind(IrInstructionKind kind) {
    return kind == IR_INSTR_MOV || kind == IR_INSTR_BINARY;
}

static int note_temp_use(IrValueRef value, size_t *use_counts, size_t use_count_len) {
    if (value.kind != IR_VALUE_TEMP) {
        return 1;
    }
    if (!use_counts || value.id >= use_count_len) {
        return 0;
    }

    ++use_counts[value.id];
    return 1;
}

static size_t count_dead_temp_defs(const IrProgram *program) {
    size_t function_index;
    size_t dead_count = 0;

    if (!program) {
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const IrFunction *function = &program->functions[function_index];
        size_t *use_counts;
        size_t block_index;

        if (!function->has_body || function->next_temp_id == 0 || !function->blocks) {
            continue;
        }

        use_counts = (size_t *)calloc(function->next_temp_id, sizeof(size_t));
        if (!use_counts) {
            return 0;
        }

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            const IrBasicBlock *block = &function->blocks[block_index];
            size_t instruction_index;

            if (!block->instructions) {
                continue;
            }

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                const IrInstruction *instruction = &block->instructions[instruction_index];
                size_t arg_index;

                switch (instruction->kind) {
                case IR_INSTR_MOV:
                    if (!note_temp_use(instruction->as.mov_value, use_counts, function->next_temp_id)) {
                        free(use_counts);
                        return 0;
                    }
                    break;
                case IR_INSTR_BINARY:
                    if (!note_temp_use(instruction->as.binary.lhs, use_counts, function->next_temp_id)
                        || !note_temp_use(instruction->as.binary.rhs, use_counts, function->next_temp_id)) {
                        free(use_counts);
                        return 0;
                    }
                    break;
                case IR_INSTR_CALL:
                    for (arg_index = 0; arg_index < instruction->as.call.arg_count; ++arg_index) {
                        if (!note_temp_use(instruction->as.call.args[arg_index],
                                use_counts,
                                function->next_temp_id)) {
                            free(use_counts);
                            return 0;
                        }
                    }
                    break;
                default:
                    break;
                }
            }

            if (!block->has_terminator) {
                continue;
            }
            if (block->terminator.kind == IR_TERM_RETURN) {
                if (!note_temp_use(block->terminator.as.return_value,
                        use_counts,
                        function->next_temp_id)) {
                    free(use_counts);
                    return 0;
                }
            } else if (block->terminator.kind == IR_TERM_BRANCH) {
                if (!note_temp_use(block->terminator.as.branch.condition,
                        use_counts,
                        function->next_temp_id)) {
                    free(use_counts);
                    return 0;
                }
            }
        }

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            const IrBasicBlock *block = &function->blocks[block_index];
            size_t instruction_index;

            if (!block->instructions) {
                continue;
            }

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                const IrInstruction *instruction = &block->instructions[instruction_index];

                if (instruction->result.kind == IR_VALUE_TEMP
                    && instruction->result.id < function->next_temp_id
                    && use_counts[instruction->result.id] == 0
                    && is_dead_temp_definition_kind(instruction->kind)) {
                    ++dead_count;
                }
            }
        }

        free(use_counts);
    }

    return dead_count;
}

static size_t count_total_instructions(const IrProgram *program) {
    size_t function_index;
    size_t instruction_count = 0;

    if (!program) {
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const IrFunction *function = &program->functions[function_index];
        size_t block_index;

        if (!function->has_body || !function->blocks) {
            continue;
        }

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            instruction_count += function->blocks[block_index].instruction_count;
        }
    }

    return instruction_count;
}

static size_t count_call_instructions(const IrProgram *program) {
    size_t function_index;
    size_t call_count = 0;

    if (!program) {
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const IrFunction *function = &program->functions[function_index];
        size_t block_index;

        if (!function->has_body || !function->blocks) {
            continue;
        }

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            const IrBasicBlock *block = &function->blocks[block_index];
            size_t instruction_index;

            if (!block->instructions) {
                continue;
            }

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                if (block->instructions[instruction_index].kind == IR_INSTR_CALL) {
                    ++call_count;
                }
            }
        }
    }

    return call_count;
}

static size_t count_total_blocks(const IrProgram *program) {
    size_t function_index;
    size_t block_count = 0;

    if (!program) {
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const IrFunction *function = &program->functions[function_index];

        if (!function->has_body || !function->blocks) {
            continue;
        }
        block_count += function->block_count;
    }

    return block_count;
}

static size_t count_immediate_branch_terminators(const IrProgram *program) {
    size_t function_index;
    size_t branch_count = 0;

    if (!program) {
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const IrFunction *function = &program->functions[function_index];
        size_t block_index;

        if (!function->has_body || !function->blocks) {
            continue;
        }

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            const IrBasicBlock *block = &function->blocks[block_index];

            if (block->has_terminator
                && block->terminator.kind == IR_TERM_BRANCH
                && block->terminator.as.branch.condition.kind == IR_VALUE_IMMEDIATE) {
                ++branch_count;
            }
        }
    }

    return branch_count;
}

static size_t count_branch_terminators(const IrProgram *program) {
    size_t function_index;
    size_t branch_count = 0;

    if (!program) {
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const IrFunction *function = &program->functions[function_index];
        size_t block_index;

        if (!function->has_body || !function->blocks) {
            continue;
        }

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            if (function->blocks[block_index].has_terminator
                && function->blocks[block_index].terminator.kind == IR_TERM_BRANCH) {
                ++branch_count;
            }
        }
    }

    return branch_count;
}

static size_t count_jump_to_empty_return_blocks(const IrProgram *program) {
    size_t function_index;
    size_t jump_count = 0;

    if (!program) {
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const IrFunction *function = &program->functions[function_index];
        size_t block_index;

        if (!function->has_body || !function->blocks) {
            continue;
        }

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            const IrBasicBlock *block = &function->blocks[block_index];

            if (!block->has_terminator || block->terminator.kind != IR_TERM_JUMP) {
                continue;
            }
            if (block->terminator.as.jump_target >= function->block_count) {
                continue;
            }
            {
                const IrBasicBlock *target = &function->blocks[block->terminator.as.jump_target];

                if (target->instruction_count == 0
                    && target->has_terminator
                    && target->terminator.kind == IR_TERM_RETURN) {
                    ++jump_count;
                }
            }
        }
    }

    return jump_count;
}

static size_t count_temp_copy_propagatable_uses(const IrProgram *program) {
    size_t function_index;
    size_t use_count = 0;

    if (!program) {
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const IrFunction *function = &program->functions[function_index];
        unsigned char *has_mov_temp_def;
        size_t block_index;

        if (!function->has_body || function->next_temp_id == 0 || !function->blocks) {
            continue;
        }

        has_mov_temp_def = (unsigned char *)calloc(function->next_temp_id, sizeof(unsigned char));
        if (!has_mov_temp_def) {
            return 0;
        }

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            const IrBasicBlock *block = &function->blocks[block_index];
            size_t instruction_index;

            if (!block->instructions) {
                continue;
            }

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                const IrInstruction *instruction = &block->instructions[instruction_index];

                if (instruction->kind == IR_INSTR_MOV
                    && instruction->result.kind == IR_VALUE_TEMP
                    && instruction->result.id < function->next_temp_id) {
                    has_mov_temp_def[instruction->result.id] = 1;
                }
            }
        }

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            const IrBasicBlock *block = &function->blocks[block_index];
            size_t instruction_index;

            if (!block->instructions) {
                continue;
            }

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                const IrInstruction *instruction = &block->instructions[instruction_index];
                size_t arg_index;

                switch (instruction->kind) {
                case IR_INSTR_MOV:
                    if (instruction->as.mov_value.kind == IR_VALUE_TEMP
                        && instruction->as.mov_value.id < function->next_temp_id
                        && has_mov_temp_def[instruction->as.mov_value.id]) {
                        ++use_count;
                    }
                    break;
                case IR_INSTR_BINARY:
                    if (instruction->as.binary.lhs.kind == IR_VALUE_TEMP
                        && instruction->as.binary.lhs.id < function->next_temp_id
                        && has_mov_temp_def[instruction->as.binary.lhs.id]) {
                        ++use_count;
                    }
                    if (instruction->as.binary.rhs.kind == IR_VALUE_TEMP
                        && instruction->as.binary.rhs.id < function->next_temp_id
                        && has_mov_temp_def[instruction->as.binary.rhs.id]) {
                        ++use_count;
                    }
                    break;
                case IR_INSTR_CALL:
                    for (arg_index = 0; arg_index < instruction->as.call.arg_count; ++arg_index) {
                        if (instruction->as.call.args[arg_index].kind == IR_VALUE_TEMP
                            && instruction->as.call.args[arg_index].id < function->next_temp_id
                            && has_mov_temp_def[instruction->as.call.args[arg_index].id]) {
                            ++use_count;
                        }
                    }
                    break;
                default:
                    break;
                }
            }

            if (!block->has_terminator) {
                continue;
            }
            if (block->terminator.kind == IR_TERM_RETURN) {
                if (block->terminator.as.return_value.kind == IR_VALUE_TEMP
                    && block->terminator.as.return_value.id < function->next_temp_id
                    && has_mov_temp_def[block->terminator.as.return_value.id]) {
                    ++use_count;
                }
            } else if (block->terminator.kind == IR_TERM_BRANCH) {
                if (block->terminator.as.branch.condition.kind == IR_VALUE_TEMP
                    && block->terminator.as.branch.condition.id < function->next_temp_id
                    && has_mov_temp_def[block->terminator.as.branch.condition.id]) {
                    ++use_count;
                }
            }
        }

        free(has_mov_temp_def);
    }

    return use_count;
}

static size_t count_immediate_binary_with_safety(const IrProgram *program, int count_only_safe) {
    size_t function_index;
    size_t count = 0;

    if (!program) {
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const IrFunction *function = &program->functions[function_index];
        size_t block_index;

        if (!function->has_body || !function->blocks) {
            continue;
        }

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            const IrBasicBlock *block = &function->blocks[block_index];
            size_t instruction_index;

            if (!block->instructions) {
                continue;
            }

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                const IrInstruction *instruction = &block->instructions[instruction_index];

                if (instruction->kind != IR_INSTR_BINARY) {
                    continue;
                }
                if (instruction->as.binary.lhs.kind != IR_VALUE_IMMEDIATE
                    || instruction->as.binary.rhs.kind != IR_VALUE_IMMEDIATE) {
                    continue;
                }
                if (!is_foldable_binary_op(instruction->as.binary.op)) {
                    continue;
                }
                if (count_only_safe) {
                    if (is_safely_foldable_immediate_binary(instruction->as.binary.op,
                            instruction->as.binary.lhs.immediate,
                            instruction->as.binary.rhs.immediate)) {
                        ++count;
                    }
                } else {
                    ++count;
                }
            }
        }
    }

    return count;
}

static int is_identity_simplifiable_binary(const IrInstruction *instruction) {
    if (!instruction || instruction->kind != IR_INSTR_BINARY) {
        return 0;
    }

    switch (instruction->as.binary.op) {
    case IR_BINARY_ADD:
        return (instruction->as.binary.lhs.kind == IR_VALUE_IMMEDIATE
                && instruction->as.binary.lhs.immediate == 0)
            || (instruction->as.binary.rhs.kind == IR_VALUE_IMMEDIATE
                && instruction->as.binary.rhs.immediate == 0);
    case IR_BINARY_SUB:
        return instruction->as.binary.rhs.kind == IR_VALUE_IMMEDIATE
            && instruction->as.binary.rhs.immediate == 0;
    case IR_BINARY_MUL:
        return (instruction->as.binary.lhs.kind == IR_VALUE_IMMEDIATE
                && (instruction->as.binary.lhs.immediate == 0 || instruction->as.binary.lhs.immediate == 1))
            || (instruction->as.binary.rhs.kind == IR_VALUE_IMMEDIATE
                && (instruction->as.binary.rhs.immediate == 0 || instruction->as.binary.rhs.immediate == 1));
    case IR_BINARY_DIV:
        return instruction->as.binary.rhs.kind == IR_VALUE_IMMEDIATE
            && instruction->as.binary.rhs.immediate == 1;
    case IR_BINARY_MOD:
        return instruction->as.binary.rhs.kind == IR_VALUE_IMMEDIATE
            && instruction->as.binary.rhs.immediate == 1;
    case IR_BINARY_BIT_AND:
        return (instruction->as.binary.lhs.kind == IR_VALUE_IMMEDIATE
                && (instruction->as.binary.lhs.immediate == 0 || instruction->as.binary.lhs.immediate == -1))
            || (instruction->as.binary.rhs.kind == IR_VALUE_IMMEDIATE
                && (instruction->as.binary.rhs.immediate == 0 || instruction->as.binary.rhs.immediate == -1));
    case IR_BINARY_BIT_XOR:
    case IR_BINARY_BIT_OR:
        return (instruction->as.binary.lhs.kind == IR_VALUE_IMMEDIATE
                && instruction->as.binary.lhs.immediate == 0)
            || (instruction->as.binary.rhs.kind == IR_VALUE_IMMEDIATE
                && instruction->as.binary.rhs.immediate == 0);
    case IR_BINARY_SHIFT_LEFT:
    case IR_BINARY_SHIFT_RIGHT:
        return instruction->as.binary.rhs.kind == IR_VALUE_IMMEDIATE
            && instruction->as.binary.rhs.immediate == 0;
    default:
        return 0;
    }
}

static int ir_value_ref_equals_for_test(IrValueRef lhs, IrValueRef rhs) {
    if (lhs.kind != rhs.kind) {
        return 0;
    }
    if (lhs.kind == IR_VALUE_IMMEDIATE) {
        return lhs.immediate == rhs.immediate;
    }

    return lhs.id == rhs.id;
}

static int is_same_operand_simplifiable_binary(const IrInstruction *instruction) {
    if (!instruction || instruction->kind != IR_INSTR_BINARY) {
        return 0;
    }
    if (!ir_value_ref_equals_for_test(instruction->as.binary.lhs, instruction->as.binary.rhs)) {
        return 0;
    }

    switch (instruction->as.binary.op) {
    case IR_BINARY_SUB:
    case IR_BINARY_BIT_AND:
    case IR_BINARY_BIT_XOR:
    case IR_BINARY_BIT_OR:
    case IR_BINARY_EQ:
    case IR_BINARY_NE:
    case IR_BINARY_LT:
    case IR_BINARY_LE:
    case IR_BINARY_GT:
    case IR_BINARY_GE:
        return 1;
    default:
        return 0;
    }
}

static size_t count_identity_binary_simplification_opportunities(const IrProgram *program) {
    size_t function_index;
    size_t count = 0;

    if (!program) {
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const IrFunction *function = &program->functions[function_index];
        size_t block_index;

        if (!function->has_body || !function->blocks) {
            continue;
        }

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            const IrBasicBlock *block = &function->blocks[block_index];
            size_t instruction_index;

            if (!block->instructions) {
                continue;
            }

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                if (is_identity_simplifiable_binary(&block->instructions[instruction_index])) {
                    ++count;
                }
            }
        }
    }

    return count;
}

static size_t count_same_operand_binary_simplification_opportunities(const IrProgram *program) {
    size_t function_index;
    size_t count = 0;

    if (!program) {
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const IrFunction *function = &program->functions[function_index];
        size_t block_index;

        if (!function->has_body || !function->blocks) {
            continue;
        }

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            const IrBasicBlock *block = &function->blocks[block_index];
            size_t instruction_index;

            if (!block->instructions) {
                continue;
            }

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                if (is_same_operand_simplifiable_binary(&block->instructions[instruction_index])) {
                    ++count;
                }
            }
        }
    }

    return count;
}

static int test_ir_pass_default_pipeline_folds_safe_immediate_binary_ops(void) {
    IrProgram program;
    IrError pass_error;
    IrError verify_error;
    size_t before_count;
    size_t after_count;

    if (!lower_source_to_ir_program(
            "int f(){return ((8+4)*(6-1))+((8<<2)+(8>>1))+(7%3)+(8/2)+((1<2)^(3==3));}\n",
            &program)) {
        return 0;
    }

    before_count = count_immediate_binary_with_safety(&program, 1);
    if (before_count == 0) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected at least one safely foldable immediate binary instruction\n");
        ir_program_free(&program);
        return 0;
    }

    if (!ir_pass_run_default_pipeline(&program, &pass_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: default pipeline failed: %s\n",
            pass_error.message);
        ir_program_free(&program);
        return 0;
    }

    after_count = count_immediate_binary_with_safety(&program, 1);
    if (after_count >= before_count) {
        fprintf(stderr,
            "[ir-pass] FAIL: fold pass did not reduce safely foldable immediate binaries (before=%zu, after=%zu)\n",
            before_count,
            after_count);
        ir_program_free(&program);
        return 0;
    }

    if (!ir_verify_program(&program, &verify_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: verifier rejected pass output at %d:%d: %s\n",
            verify_error.line,
            verify_error.column,
            verify_error.message);
        ir_program_free(&program);
        return 0;
    }

    ir_program_free(&program);
    return 1;
}

static int test_ir_pass_fold_simplifies_safe_binary_identities(void) {
    IrProgram program;
    IrError pass_error;
    IrError verify_error;
    size_t before_count;
    size_t after_count;

    if (!lower_source_to_ir_program(
            "int f(int a){return ((((a+0)-0)*1)|0)^0; }\n",
            &program)) {
        return 0;
    }

    before_count = count_identity_binary_simplification_opportunities(&program);
    if (before_count == 0) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected at least one identity-style binary simplification opportunity\n");
        ir_program_free(&program);
        return 0;
    }

    if (!ir_pass_fold_immediate_binary(&program, &pass_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: identity simplification fold pass failed: %s\n",
            pass_error.message);
        ir_program_free(&program);
        return 0;
    }

    after_count = count_identity_binary_simplification_opportunities(&program);
    if (after_count != 0) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected fold pass to exhaust identity simplification opportunities, got %zu\n",
            after_count);
        ir_program_free(&program);
        return 0;
    }

    if (!ir_verify_program(&program, &verify_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: verifier rejected identity fold output at %d:%d: %s\n",
            verify_error.line,
            verify_error.column,
            verify_error.message);
        ir_program_free(&program);
        return 0;
    }

    ir_program_free(&program);
    return 1;
}

static int test_ir_pass_fold_simplifies_safe_same_operand_binaries(void) {
    IrProgram program;
    IrError pass_error;
    IrError verify_error;
    size_t before_count;
    size_t after_count;

    if (!lower_source_to_ir_program(
            "int f(int a){return ((a-a)+(a^a))+((a&a)|(a==a));}\n",
            &program)) {
        return 0;
    }

    before_count = count_same_operand_binary_simplification_opportunities(&program);
    if (before_count == 0) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected at least one same-operand binary simplification opportunity\n");
        ir_program_free(&program);
        return 0;
    }

    if (!ir_pass_fold_immediate_binary(&program, &pass_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: same-operand simplification fold pass failed: %s\n",
            pass_error.message);
        ir_program_free(&program);
        return 0;
    }

    after_count = count_same_operand_binary_simplification_opportunities(&program);
    if (after_count != 0) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected fold pass to exhaust same-operand simplification opportunities, got %zu\n",
            after_count);
        ir_program_free(&program);
        return 0;
    }

    if (!ir_verify_program(&program, &verify_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: verifier rejected same-operand fold output at %d:%d: %s\n",
            verify_error.line,
            verify_error.column,
            verify_error.message);
        ir_program_free(&program);
        return 0;
    }

    ir_program_free(&program);
    return 1;
}

static int test_ir_pass_default_pipeline_skips_unsafe_immediate_binary_ops(void) {
    IrProgram program;
    IrError pass_error;
    IrError verify_error;
    size_t before_total;
    size_t before_safe;
    size_t after_total;
    size_t after_safe;

    if (!lower_source_to_ir_program(
            "int f(){return (1/0)+(1<<63);}\n",
            &program)) {
        return 0;
    }

    before_total = count_immediate_binary_with_safety(&program, 0);
    before_safe = count_immediate_binary_with_safety(&program, 1);
    if (before_total == 0 || before_total == before_safe) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected at least one unsafe immediate binary instruction before pass\n");
        ir_program_free(&program);
        return 0;
    }

    if (!ir_pass_run_default_pipeline(&program, &pass_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: default pipeline failed on unsafe-skip case: %s\n",
            pass_error.message);
        ir_program_free(&program);
        return 0;
    }

    after_total = count_immediate_binary_with_safety(&program, 0);
    after_safe = count_immediate_binary_with_safety(&program, 1);
    if (after_safe != 0) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected safe foldable immediate binaries to be exhausted in unsafe-skip case, got %zu\n",
            after_safe);
        ir_program_free(&program);
        return 0;
    }
    if (after_total == 0) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected unsafe immediate binary instructions to remain after pass\n");
        ir_program_free(&program);
        return 0;
    }

    if (!ir_verify_program(&program, &verify_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: verifier rejected unsafe-skip pass output at %d:%d: %s\n",
            verify_error.line,
            verify_error.column,
            verify_error.message);
        ir_program_free(&program);
        return 0;
    }

    ir_program_free(&program);
    return 1;
}

static int test_ir_pass_eliminates_dead_temp_defs_directly(void) {
    IrProgram program;
    IrError pass_error;
    IrError verify_error;
    size_t before_dead;
    size_t after_dead;

    if (!lower_source_to_ir_program(
            "int f(int a){a+1; a+2; return a;}\n",
            &program)) {
        return 0;
    }

    before_dead = count_dead_temp_defs(&program);
    if (before_dead == 0) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected dead temp defs before elimination pass\n");
        ir_program_free(&program);
        return 0;
    }

    if (!ir_pass_eliminate_dead_temp_defs(&program, &pass_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: dead-temp elimination failed: %s\n",
            pass_error.message);
        ir_program_free(&program);
        return 0;
    }

    after_dead = count_dead_temp_defs(&program);
    if (after_dead != 0) {
        fprintf(stderr,
            "[ir-pass] FAIL: dead-temp elimination left %zu dead defs\n",
            after_dead);
        ir_program_free(&program);
        return 0;
    }

    if (!ir_verify_program(&program, &verify_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: verifier rejected dead-temp elimination output at %d:%d: %s\n",
            verify_error.line,
            verify_error.column,
            verify_error.message);
        ir_program_free(&program);
        return 0;
    }

    ir_program_free(&program);
    return 1; 
}

static int test_ir_pass_preserves_dead_result_calls_during_dce(void) {
    IrProgram program;
    IrError pass_error;
    IrError verify_error;
    size_t before_call_count;
    size_t after_call_count;

    if (!lower_source_to_ir_program(
            "int g(){return 1;} int f(){g(); return 0;}\n",
            &program)) {
        return 0;
    }

    before_call_count = count_call_instructions(&program);
    if (before_call_count == 0) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected at least one call instruction before DCE side-effect test\n");
        ir_program_free(&program);
        return 0;
    }

    if (!ir_pass_eliminate_dead_temp_defs(&program, &pass_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: dead-temp elimination failed on dead-result call case: %s\n",
            pass_error.message);
        ir_program_free(&program);
        return 0;
    }

    after_call_count = count_call_instructions(&program);
    if (after_call_count != before_call_count) {
        fprintf(stderr,
            "[ir-pass] FAIL: DCE changed side-effecting call count (before=%zu, after=%zu)\n",
            before_call_count,
            after_call_count);
        ir_program_free(&program);
        return 0;
    }

    if (!ir_verify_program(&program, &verify_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: verifier rejected dead-result call DCE output at %d:%d: %s\n",
            verify_error.line,
            verify_error.column,
            verify_error.message);
        ir_program_free(&program);
        return 0;
    }

    ir_program_free(&program);
    return 1;
}

static int test_ir_pass_propagates_temp_copies_after_fold(void) {
    IrProgram program;
    IrError pass_error;
    IrError verify_error;
    size_t before_use_count;
    size_t after_use_count;

    if (!lower_source_to_ir_program(
            "int f(){return (1+2)+3;}\n",
            &program)) {
        return 0;
    }

    if (!ir_pass_fold_immediate_binary(&program, &pass_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: pre-fold for copy propagation failed: %s\n",
            pass_error.message);
        ir_program_free(&program);
        return 0;
    }

    before_use_count = count_temp_copy_propagatable_uses(&program);
    if (before_use_count == 0) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected temp-copy propagation opportunities after fold\n");
        ir_program_free(&program);
        return 0;
    }

    if (!ir_pass_propagate_temp_copies(&program, &pass_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: temp-copy propagation failed: %s\n",
            pass_error.message);
        ir_program_free(&program);
        return 0;
    }

    after_use_count = count_temp_copy_propagatable_uses(&program);
    if (after_use_count >= before_use_count) {
        fprintf(stderr,
            "[ir-pass] FAIL: temp-copy propagation did not reduce propagatable uses (before=%zu, after=%zu)\n",
            before_use_count,
            after_use_count);
        ir_program_free(&program);
        return 0;
    }

    if (!ir_verify_program(&program, &verify_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: verifier rejected temp-copy propagation output at %d:%d: %s\n",
            verify_error.line,
            verify_error.column,
            verify_error.message);
        ir_program_free(&program);
        return 0;
    }

    ir_program_free(&program);
    return 1;
}

static int test_ir_pass_propagates_single_use_call_arg_copies(void) {
    IrProgram program;
    IrError pass_error;
    IrError verify_error;
    size_t before_use_count;
    size_t after_use_count;
    const IrInstruction *call_instruction;

    if (!lower_source_to_ir_program(
            "int g(int x){return x;} int f(){return 0;}\n",
            &program)) {
        return 0;
    }
    if (!rewrite_as_single_use_call_arg_copy_program(&program)) {
        fprintf(stderr,
            "[ir-pass] FAIL: could not build single-use call-arg copy-propagation case\n");
        ir_program_free(&program);
        return 0;
    }
    if (!ir_verify_program(&program, &verify_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: verifier rejected constructed single-use call-arg case at %d:%d: %s\n",
            verify_error.line,
            verify_error.column,
            verify_error.message);
        ir_program_free(&program);
        return 0;
    }

    before_use_count = count_temp_copy_propagatable_uses(&program);
    if (before_use_count != 1) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected exactly one single-use call-arg copy opportunity before propagation, found %zu\n",
            before_use_count);
        ir_program_free(&program);
        return 0;
    }

    if (!ir_pass_propagate_temp_copies(&program, &pass_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: temp-copy propagation failed on single-use call-arg case: %s\n",
            pass_error.message);
        ir_program_free(&program);
        return 0;
    }

    call_instruction = &program.functions[1].blocks[0].instructions[1];
    if (call_instruction->kind != IR_INSTR_CALL
        || call_instruction->as.call.arg_count != 1
        || call_instruction->as.call.args[0].kind != IR_VALUE_IMMEDIATE
        || call_instruction->as.call.args[0].immediate != 7) {
        fprintf(stderr,
            "[ir-pass] FAIL: temp-copy propagation did not rewrite the single-use call argument to an immediate\n");
        ir_program_free(&program);
        return 0;
    }

    after_use_count = count_temp_copy_propagatable_uses(&program);
    if (after_use_count != 0) {
        fprintf(stderr,
            "[ir-pass] FAIL: temp-copy propagation left copyable uses after single-use call-arg rewrite (after=%zu)\n",
            after_use_count);
        ir_program_free(&program);
        return 0;
    }

    if (!ir_verify_program(&program, &verify_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: verifier rejected single-use call-arg copy output at %d:%d: %s\n",
            verify_error.line,
            verify_error.column,
            verify_error.message);
        ir_program_free(&program);
        return 0;
    }

    ir_program_free(&program);
    return 1;
}

static int test_ir_pass_copy_propagation_skips_multi_def_join_temps(void) {
    IrProgram program;
    IrError pass_error;
    IrError verify_error;
    size_t join_block_id;

    if (!lower_source_to_ir_program(
            "int f(int a){if(a) 1; else 2; return 0;}\n",
            &program)) {
        return 0;
    }
    if (!rewrite_as_multi_def_join_temp_program(&program, &join_block_id)) {
        fprintf(stderr,
            "[ir-pass] FAIL: could not build multi-def join-temp copy-propagation case\n");
        ir_program_free(&program);
        return 0;
    }
    if (!ir_verify_program(&program, &verify_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: verifier rejected constructed multi-def join-temp case at %d:%d: %s\n",
            verify_error.line,
            verify_error.column,
            verify_error.message);
        ir_program_free(&program);
        return 0;
    }

    if (!ir_pass_propagate_temp_copies(&program, &pass_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: temp-copy propagation failed on multi-def join-temp case: %s\n",
            pass_error.message);
        ir_program_free(&program);
        return 0;
    }
    if (program.functions[0].blocks[join_block_id].terminator.as.return_value.kind != IR_VALUE_TEMP) {
        fprintf(stderr,
            "[ir-pass] FAIL: temp-copy propagation incorrectly resolved a multi-def join temp\n");
        ir_program_free(&program);
        return 0;
    }

    if (!ir_verify_program(&program, &verify_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: verifier rejected copy-propagation multi-def output at %d:%d: %s\n",
            verify_error.line,
            verify_error.column,
            verify_error.message);
        ir_program_free(&program);
        return 0;
    }

    ir_program_free(&program);
    return 1;
}

static int test_ir_pass_propagates_temp_constants_across_binary_chains(void) {
    IrProgram program;
    IrError pass_error;
    IrError verify_error;
    size_t before_safe_immediates;
    size_t after_safe_immediates;

    if (!lower_source_to_ir_program(
            "int f(){return ((1+2)+3)+4;}\n",
            &program)) {
        return 0;
    }

    if (!ir_pass_fold_immediate_binary(&program, &pass_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: pre-fold for temp-constant propagation failed: %s\n",
            pass_error.message);
        ir_program_free(&program);
        return 0;
    }

    before_safe_immediates = count_immediate_binary_with_safety(&program, 1);
    if (before_safe_immediates != 0) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected folded temp-constant chain to have no immediate-immediate binaries before constant propagation, got %zu\n",
            before_safe_immediates);
        ir_program_free(&program);
        return 0;
    }

    if (!ir_pass_propagate_temp_constants(&program, &pass_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: temp-constant propagation failed: %s\n",
            pass_error.message);
        ir_program_free(&program);
        return 0;
    }

    after_safe_immediates = count_immediate_binary_with_safety(&program, 1);
    if (after_safe_immediates == 0) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected temp-constant propagation to expose new immediate-immediate binaries\n");
        ir_program_free(&program);
        return 0;
    }

    if (!ir_verify_program(&program, &verify_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: verifier rejected temp-constant propagation output at %d:%d: %s\n",
            verify_error.line,
            verify_error.column,
            verify_error.message);
        ir_program_free(&program);
        return 0;
    }

    ir_program_free(&program);
    return 1;
}

static int test_ir_pass_constant_propagation_skips_multi_def_join_temps(void) {
    IrProgram program;
    IrError pass_error;
    IrError verify_error;
    size_t join_block_id;

    if (!lower_source_to_ir_program(
            "int f(int a){if(a) 1; else 2; return 0;}\n",
            &program)) {
        return 0;
    }
    if (!rewrite_as_multi_def_join_temp_program(&program, &join_block_id)) {
        fprintf(stderr,
            "[ir-pass] FAIL: could not build multi-def join-temp constant-propagation case\n");
        ir_program_free(&program);
        return 0;
    }
    if (!ir_verify_program(&program, &verify_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: verifier rejected constructed multi-def constant case at %d:%d: %s\n",
            verify_error.line,
            verify_error.column,
            verify_error.message);
        ir_program_free(&program);
        return 0;
    }

    if (!ir_pass_propagate_temp_constants(&program, &pass_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: temp-constant propagation failed on multi-def join-temp case: %s\n",
            pass_error.message);
        ir_program_free(&program);
        return 0;
    }
    if (program.functions[0].blocks[join_block_id].terminator.as.return_value.kind != IR_VALUE_TEMP) {
        fprintf(stderr,
            "[ir-pass] FAIL: temp-constant propagation incorrectly resolved a multi-def join temp\n");
        ir_program_free(&program);
        return 0;
    }

    if (!ir_verify_program(&program, &verify_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: verifier rejected constant-propagation multi-def output at %d:%d: %s\n",
            verify_error.line,
            verify_error.column,
            verify_error.message);
        ir_program_free(&program);
        return 0;
    }

    ir_program_free(&program);
    return 1;
}

static int test_ir_pass_simplifies_constant_branches_and_removes_dead_blocks(void) {
    IrProgram program;
    IrError pass_error;
    IrError verify_error;
    IrBasicBlock *branch_block;
    size_t before_immediate_branches;
    size_t before_blocks;
    size_t after_immediate_branches;
    size_t after_blocks;

    if (!lower_source_to_ir_program(
            "int f(int a){if(a) return 1; else return 2;}\n",
            &program)) {
        return 0;
    }

    branch_block = find_first_branch_block_mut(&program);
    if (!branch_block) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected branch block before CFG simplification test\n");
        ir_program_free(&program);
        return 0;
    }
    branch_block->terminator.as.branch.condition.kind = IR_VALUE_IMMEDIATE;
    branch_block->terminator.as.branch.condition.immediate = 1;
    branch_block->terminator.as.branch.condition.id = 0;

    before_immediate_branches = count_immediate_branch_terminators(&program);
    before_blocks = count_total_blocks(&program);
    if (before_immediate_branches == 0 || before_blocks < 2) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected immediate branch and multi-block CFG before simplify pass\n");
        ir_program_free(&program);
        return 0;
    }

    if (!ir_pass_simplify_cfg(&program, &pass_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: CFG simplification failed: %s\n",
            pass_error.message);
        ir_program_free(&program);
        return 0;
    }

    after_immediate_branches = count_immediate_branch_terminators(&program);
    after_blocks = count_total_blocks(&program);
    if (after_immediate_branches != 0) {
        fprintf(stderr,
            "[ir-pass] FAIL: CFG simplification left %zu immediate branch terminators\n",
            after_immediate_branches);
        ir_program_free(&program);
        return 0;
    }
    if (after_blocks >= before_blocks) {
        fprintf(stderr,
            "[ir-pass] FAIL: CFG simplification did not reduce block count (before=%zu, after=%zu)\n",
            before_blocks,
            after_blocks);
        ir_program_free(&program);
        return 0;
    }

    if (!ir_verify_program(&program, &verify_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: verifier rejected CFG simplification output at %d:%d: %s\n",
            verify_error.line,
            verify_error.column,
            verify_error.message);
        ir_program_free(&program);
        return 0;
    }

    ir_program_free(&program);
    return 1;
}

static int test_ir_pass_threads_empty_jump_blocks_and_removes_trampolines(void) {
    IrProgram program;
    IrError pass_error;
    IrError verify_error;
    IrBasicBlock *branch_block;
    IrFunction *function;
    size_t before_blocks;
    size_t after_blocks;

    if (!lower_source_to_ir_program(
            "int f(int a){if(a) return 1; else return 2;}\n",
            &program)) {
        return 0;
    }

    function = &program.functions[0];
    branch_block = find_first_branch_block_mut(&program);
    if (!function->has_body || function->block_count < 3 || !branch_block) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected three-block branch CFG before jump-threading test\n");
        ir_program_free(&program);
        return 0;
    }
    if (function->blocks[1].instruction_count != 0) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected trampoline candidate block to be instruction-free before jump-threading test\n");
        ir_program_free(&program);
        return 0;
    }

    function->blocks[1].terminator.kind = IR_TERM_JUMP;
    function->blocks[1].terminator.as.jump_target = 2;

    before_blocks = count_total_blocks(&program);
    if (count_branch_terminators(&program) == 0 || before_blocks < 3) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected branch plus trampoline block before jump-threading simplify pass\n");
        ir_program_free(&program);
        return 0;
    }

    if (!ir_pass_simplify_cfg(&program, &pass_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: CFG jump-threading simplification failed: %s\n",
            pass_error.message);
        ir_program_free(&program);
        return 0;
    }

    after_blocks = count_total_blocks(&program);
    if (count_branch_terminators(&program) != 0) {
        fprintf(stderr,
            "[ir-pass] FAIL: CFG jump-threading simplification left branch terminators behind\n");
        ir_program_free(&program);
        return 0;
    }
    if (after_blocks >= before_blocks) {
        fprintf(stderr,
            "[ir-pass] FAIL: CFG jump-threading simplification did not reduce block count (before=%zu, after=%zu)\n",
            before_blocks,
            after_blocks);
        ir_program_free(&program);
        return 0;
    }

    if (!ir_verify_program(&program, &verify_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: verifier rejected CFG jump-threading output at %d:%d: %s\n",
            verify_error.line,
            verify_error.column,
            verify_error.message);
        ir_program_free(&program);
        return 0;
    }

    ir_program_free(&program);
    return 1;
}

static int test_ir_pass_folds_jump_to_empty_return_blocks(void) {
    IrProgram program;
    IrError pass_error;
    IrError verify_error;
    IrBasicBlock *branch_block;
    size_t before_blocks;
    size_t after_blocks;

    if (!lower_source_to_ir_program(
            "int f(int a){if(a) return 1; else return 2;}\n",
            &program)) {
        return 0;
    }

    branch_block = find_first_branch_block_mut(&program);
    if (!branch_block) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected branch block before jump-to-return folding test\n");
        ir_program_free(&program);
        return 0;
    }
    branch_block->terminator.as.branch.condition.kind = IR_VALUE_IMMEDIATE;
    branch_block->terminator.as.branch.condition.immediate = 1;
    branch_block->terminator.as.branch.condition.id = 0;

    before_blocks = count_total_blocks(&program);
    if (count_jump_to_empty_return_blocks(&program) != 0 || before_blocks < 2) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected pre-simplify constant-branch CFG without direct jump-to-return folding yet\n");
        ir_program_free(&program);
        return 0;
    }

    if (!ir_pass_simplify_cfg(&program, &pass_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: CFG jump-to-return folding failed: %s\n",
            pass_error.message);
        ir_program_free(&program);
        return 0;
    }

    after_blocks = count_total_blocks(&program);
    if (count_jump_to_empty_return_blocks(&program) != 0) {
        fprintf(stderr,
            "[ir-pass] FAIL: CFG simplification left jump-to-empty-return forwarding blocks behind\n");
        ir_program_free(&program);
        return 0;
    }
    if (after_blocks != 1) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected jump-to-return folding case to collapse to one block, got %zu\n",
            after_blocks);
        ir_program_free(&program);
        return 0;
    }

    if (!ir_verify_program(&program, &verify_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: verifier rejected jump-to-return folded output at %d:%d: %s\n",
            verify_error.line,
            verify_error.column,
            verify_error.message);
        ir_program_free(&program);
        return 0;
    }

    ir_program_free(&program);
    return 1;
}

static int test_ir_pass_folds_branch_to_same_empty_return_value(void) {
    IrProgram program;
    IrError pass_error;
    IrError verify_error;
    size_t before_blocks;
    size_t after_blocks;

    if (!lower_source_to_ir_program(
            "int f(int a){if(a) return 7; else return 7;}\n",
            &program)) {
        return 0;
    }

    before_blocks = count_total_blocks(&program);
    if (count_branch_terminators(&program) == 0 || before_blocks < 3) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected branch diamond before same-return CFG folding test\n");
        ir_program_free(&program);
        return 0;
    }

    if (!ir_pass_simplify_cfg(&program, &pass_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: same-return CFG folding failed: %s\n",
            pass_error.message);
        ir_program_free(&program);
        return 0;
    }

    after_blocks = count_total_blocks(&program);
    if (count_branch_terminators(&program) != 0) {
        fprintf(stderr,
            "[ir-pass] FAIL: same-return CFG folding left branch terminators behind\n");
        ir_program_free(&program);
        return 0;
    }
    if (after_blocks != 1) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected same-return CFG folding case to collapse to one block, got %zu\n",
            after_blocks);
        ir_program_free(&program);
        return 0;
    }

    if (!ir_verify_program(&program, &verify_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: verifier rejected same-return CFG folded output at %d:%d: %s\n",
            verify_error.line,
            verify_error.column,
            verify_error.message);
        ir_program_free(&program);
        return 0;
    }

    ir_program_free(&program);
    return 1;
}

static int test_ir_pass_folds_branch_to_same_empty_temp_return_value(void) {
    IrProgram program;
    IrError pass_error;
    IrError verify_error;
    size_t before_blocks;
    size_t after_blocks;
    size_t branch_block_id;
    size_t temp_id;

    if (!lower_source_to_ir_program(
            "int f(int a){if(a) return 1; else return 2;}\n",
            &program)) {
        return 0;
    }
    if (!rewrite_branch_successors_to_same_temp_return(&program, &branch_block_id, &temp_id)) {
        fprintf(stderr,
            "[ir-pass] FAIL: could not build same-temp-return CFG folding case\n");
        ir_program_free(&program);
        return 0;
    }
    if (!ir_verify_program(&program, &verify_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: verifier rejected constructed same-temp-return CFG case at %d:%d: %s\n",
            verify_error.line,
            verify_error.column,
            verify_error.message);
        ir_program_free(&program);
        return 0;
    }

    before_blocks = count_total_blocks(&program);
    if (count_branch_terminators(&program) == 0 || before_blocks < 3) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected branch diamond before same-temp-return CFG folding test\n");
        ir_program_free(&program);
        return 0;
    }

    if (!ir_pass_simplify_cfg(&program, &pass_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: same-temp-return CFG folding failed: %s\n",
            pass_error.message);
        ir_program_free(&program);
        return 0;
    }

    after_blocks = count_total_blocks(&program);
    if (count_branch_terminators(&program) != 0) {
        fprintf(stderr,
            "[ir-pass] FAIL: same-temp-return CFG folding left branch terminators behind\n");
        ir_program_free(&program);
        return 0;
    }
    if (after_blocks != 1) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected same-temp-return CFG folding case to collapse to one block, got %zu\n",
            after_blocks);
        ir_program_free(&program);
        return 0;
    }
    if (program.functions[0].blocks[0].terminator.kind != IR_TERM_RETURN
        || !ir_value_ref_equals_for_test(program.functions[0].blocks[0].terminator.as.return_value,
            make_immediate_value(7))) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected same-temp-return CFG folding to canonicalize to the shared immediate value\n");
        ir_program_free(&program);
        return 0;
    }

    if (!ir_verify_program(&program, &verify_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: verifier rejected same-temp-return CFG folded output at %d:%d: %s\n",
            verify_error.line,
            verify_error.column,
            verify_error.message);
        ir_program_free(&program);
        return 0;
    }

    ir_program_free(&program);
    return 1;
}

static int test_ir_pass_folds_branch_to_equivalent_constant_temp_returns(void) {
    IrProgram program;
    IrError pass_error;
    IrError verify_error;
    IrValueRef expected_value;

    if (!lower_source_to_ir_program(
            "int f(int a){if(a) return 1; else return 2;}\n",
            &program)) {
        return 0;
    }
    if (!rewrite_branch_successors_to_equivalent_temp_returns(&program,
            make_immediate_value(11),
            make_immediate_value(11),
            &expected_value)) {
        fprintf(stderr,
            "[ir-pass] FAIL: could not build equivalent-constant-temp CFG folding case\n");
        ir_program_free(&program);
        return 0;
    }

    if (!ir_pass_simplify_cfg(&program, &pass_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: equivalent-constant-temp CFG folding failed: %s\n",
            pass_error.message);
        ir_program_free(&program);
        return 0;
    }
    if (program.functions[0].blocks[0].terminator.kind != IR_TERM_RETURN
        || !ir_value_ref_equals_for_test(program.functions[0].blocks[0].terminator.as.return_value,
            expected_value)) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected equivalent-constant-temp CFG folding to canonicalize to the shared immediate value\n");
        ir_program_free(&program);
        return 0;
    }

    if (!ir_verify_program(&program, &verify_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: verifier rejected equivalent-constant-temp CFG folded output at %d:%d: %s\n",
            verify_error.line,
            verify_error.column,
            verify_error.message);
        ir_program_free(&program);
        return 0;
    }

    ir_program_free(&program);
    return 1;
}

static int test_ir_pass_folds_branch_to_equivalent_copy_temp_returns(void) {
    IrProgram program;
    IrError pass_error;
    IrError verify_error;
    IrValueRef expected_value;

    if (!lower_source_to_ir_program(
            "int f(int a){if(a) return 1; else return 2;}\n",
            &program)) {
        return 0;
    }
    expected_value.kind = IR_VALUE_LOCAL;
    expected_value.immediate = 0;
    expected_value.id = 0;
    if (!rewrite_branch_successors_to_equivalent_temp_returns(&program,
            expected_value,
            expected_value,
            NULL)) {
        fprintf(stderr,
            "[ir-pass] FAIL: could not build equivalent-copy-temp CFG folding case\n");
        ir_program_free(&program);
        return 0;
    }

    if (!ir_pass_simplify_cfg(&program, &pass_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: equivalent-copy-temp CFG folding failed: %s\n",
            pass_error.message);
        ir_program_free(&program);
        return 0;
    }
    if (program.functions[0].blocks[0].terminator.kind != IR_TERM_RETURN
        || !ir_value_ref_equals_for_test(program.functions[0].blocks[0].terminator.as.return_value,
            expected_value)) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected equivalent-copy-temp CFG folding to canonicalize to the shared copied value\n");
        ir_program_free(&program);
        return 0;
    }

    if (!ir_verify_program(&program, &verify_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: verifier rejected equivalent-copy-temp CFG folded output at %d:%d: %s\n",
            verify_error.line,
            verify_error.column,
            verify_error.message);
        ir_program_free(&program);
        return 0;
    }

    ir_program_free(&program);
    return 1;
}

static int test_ir_pass_merges_single_predecessor_nonempty_jump_blocks(void) {
    IrProgram program;
    IrError pass_error;
    IrError verify_error;
    IrBasicBlock *branch_block;
    size_t before_blocks;
    size_t after_blocks;
    size_t after_instructions;

    if (!lower_source_to_ir_program(
            "int f(int a){if(a) return 1; return a+1;}\n",
            &program)) {
        return 0;
    }

    branch_block = find_first_branch_block_mut(&program);
    if (!branch_block) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected branch block before single-predecessor merge test\n");
        ir_program_free(&program);
        return 0;
    }
    branch_block->terminator.as.branch.condition.kind = IR_VALUE_IMMEDIATE;
    branch_block->terminator.as.branch.condition.immediate = 0;
    branch_block->terminator.as.branch.condition.id = 0;

    before_blocks = count_total_blocks(&program);
    if (before_blocks < 3) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected at least three blocks before single-predecessor merge test\n");
        ir_program_free(&program);
        return 0;
    }

    if (!ir_pass_simplify_cfg(&program, &pass_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: single-predecessor merge simplification failed: %s\n",
            pass_error.message);
        ir_program_free(&program);
        return 0;
    }

    after_blocks = count_total_blocks(&program);
    after_instructions = count_total_instructions(&program);
    if (after_blocks != 1) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected single-predecessor merge case to collapse to one block, got %zu\n",
            after_blocks);
        ir_program_free(&program);
        return 0;
    }
    if (after_instructions == 0) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected merged nonempty successor instructions to remain after block merge\n");
        ir_program_free(&program);
        return 0;
    }
    if (count_branch_terminators(&program) != 0) {
        fprintf(stderr,
            "[ir-pass] FAIL: single-predecessor merge case left branch terminators behind\n");
        ir_program_free(&program);
        return 0;
    }

    if (!ir_verify_program(&program, &verify_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: verifier rejected single-predecessor merged CFG at %d:%d: %s\n",
            verify_error.line,
            verify_error.column,
            verify_error.message);
        ir_program_free(&program);
        return 0;
    }

    ir_program_free(&program);
    return 1;
}

static int test_ir_pass_default_pipeline_cleans_folded_dead_temps(void) {
    IrProgram program;
    IrError pass_error;
    IrError verify_error;
    size_t before_instruction_count;
    size_t after_instruction_count;

    if (!lower_source_to_ir_program(
            "int f(){1+2; return 0;}\n",
            &program)) {
        return 0;
    }

    before_instruction_count = count_total_instructions(&program);
    if (before_instruction_count == 0) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected at least one instruction before default pipeline cleanup\n");
        ir_program_free(&program);
        return 0;
    }

    if (!ir_pass_run_default_pipeline(&program, &pass_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: default pipeline failed on fold-cleanup case: %s\n",
            pass_error.message);
        ir_program_free(&program);
        return 0;
    }

    after_instruction_count = count_total_instructions(&program);
    if (after_instruction_count >= before_instruction_count) {
        fprintf(stderr,
            "[ir-pass] FAIL: default pipeline did not reduce total instruction count (before=%zu, after=%zu)\n",
            before_instruction_count,
            after_instruction_count);
        ir_program_free(&program);
        return 0;
    }
    if (count_dead_temp_defs(&program) != 0) {
        fprintf(stderr,
            "[ir-pass] FAIL: default pipeline left dead temp defs after fold-cleanup case\n");
        ir_program_free(&program);
        return 0;
    }
    if (count_temp_copy_propagatable_uses(&program) != 0) {
        fprintf(stderr,
            "[ir-pass] FAIL: default pipeline left temp-copy propagation opportunities after fold-cleanup case\n");
        ir_program_free(&program);
        return 0;
    }
    if (after_instruction_count != 0) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected nested fold-cleanup case to collapse to zero instructions, got %zu\n",
            after_instruction_count);
        ir_program_free(&program);
        return 0;
    }

    if (!ir_verify_program(&program, &verify_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: verifier rejected default pipeline cleanup output at %d:%d: %s\n",
            verify_error.line,
            verify_error.column,
            verify_error.message);
        ir_program_free(&program);
        return 0;
    }

    ir_program_free(&program);
    return 1;
}

static int test_ir_pass_default_pipeline_closes_temp_constant_chains(void) {
    IrProgram program;
    IrError pass_error;
    IrError verify_error;
    size_t before_instruction_count;
    size_t after_instruction_count;

    if (!lower_source_to_ir_program(
            "int f(){return ((1+2)+3)+4;}\n",
            &program)) {
        return 0;
    }

    before_instruction_count = count_total_instructions(&program);
    if (before_instruction_count == 0) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected temp-constant chain to produce instructions before default pipeline\n");
        ir_program_free(&program);
        return 0;
    }

    if (!ir_pass_run_default_pipeline(&program, &pass_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: default pipeline failed on temp-constant chain case: %s\n",
            pass_error.message);
        ir_program_free(&program);
        return 0;
    }

    after_instruction_count = count_total_instructions(&program);
    if (after_instruction_count != 0) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected temp-constant chain case to collapse to zero instructions, got %zu\n",
            after_instruction_count);
        ir_program_free(&program);
        return 0;
    }
    if (count_dead_temp_defs(&program) != 0) {
        fprintf(stderr,
            "[ir-pass] FAIL: default pipeline left dead temp defs after temp-constant chain case\n");
        ir_program_free(&program);
        return 0;
    }
    if (count_temp_copy_propagatable_uses(&program) != 0) {
        fprintf(stderr,
            "[ir-pass] FAIL: default pipeline left temp-copy opportunities after temp-constant chain case\n");
        ir_program_free(&program);
        return 0;
    }

    if (!ir_verify_program(&program, &verify_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: verifier rejected temp-constant chain output at %d:%d: %s\n",
            verify_error.line,
            verify_error.column,
            verify_error.message);
        ir_program_free(&program);
        return 0;
    }

    ir_program_free(&program);
    return 1;
}

static int test_ir_pass_default_pipeline_cleans_binary_identity_chains(void) {
    IrProgram program;
    IrError pass_error;
    IrError verify_error;
    size_t before_instruction_count;
    size_t after_instruction_count;

    if (!lower_source_to_ir_program(
            "int f(int a){return ((((a+0)-0)/1)%1);}\n",
            &program)) {
        return 0;
    }

    before_instruction_count = count_total_instructions(&program);
    if (before_instruction_count == 0) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected binary identity chain to produce instructions before default pipeline\n");
        ir_program_free(&program);
        return 0;
    }

    if (!ir_pass_run_default_pipeline(&program, &pass_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: default pipeline failed on binary identity chain case: %s\n",
            pass_error.message);
        ir_program_free(&program);
        return 0;
    }

    after_instruction_count = count_total_instructions(&program);
    if (after_instruction_count != 0) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected binary identity chain case to collapse to zero instructions, got %zu\n",
            after_instruction_count);
        ir_program_free(&program);
        return 0;
    }
    if (count_dead_temp_defs(&program) != 0) {
        fprintf(stderr,
            "[ir-pass] FAIL: default pipeline left dead temp defs after binary identity chain case\n");
        ir_program_free(&program);
        return 0;
    }
    if (count_temp_copy_propagatable_uses(&program) != 0) {
        fprintf(stderr,
            "[ir-pass] FAIL: default pipeline left temp-copy opportunities after binary identity chain case\n");
        ir_program_free(&program);
        return 0;
    }

    if (!ir_verify_program(&program, &verify_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: verifier rejected binary identity chain output at %d:%d: %s\n",
            verify_error.line,
            verify_error.column,
            verify_error.message);
        ir_program_free(&program);
        return 0;
    }

    ir_program_free(&program);
    return 1;
}

static int test_ir_pass_default_pipeline_cleans_same_operand_binary_chains(void) {
    IrProgram program;
    IrError pass_error;
    IrError verify_error;
    size_t before_instruction_count;
    size_t after_instruction_count;

    if (!lower_source_to_ir_program(
            "int f(int a){return (a-a)+(a^a);}\n",
            &program)) {
        return 0;
    }

    before_instruction_count = count_total_instructions(&program);
    if (before_instruction_count == 0) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected same-operand chain to produce instructions before default pipeline\n");
        ir_program_free(&program);
        return 0;
    }

    if (!ir_pass_run_default_pipeline(&program, &pass_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: default pipeline failed on same-operand chain case: %s\n",
            pass_error.message);
        ir_program_free(&program);
        return 0;
    }

    after_instruction_count = count_total_instructions(&program);
    if (after_instruction_count != 0) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected same-operand chain case to collapse to zero instructions, got %zu\n",
            after_instruction_count);
        ir_program_free(&program);
        return 0;
    }
    if (count_dead_temp_defs(&program) != 0) {
        fprintf(stderr,
            "[ir-pass] FAIL: default pipeline left dead temp defs after same-operand chain case\n");
        ir_program_free(&program);
        return 0;
    }
    if (count_temp_copy_propagatable_uses(&program) != 0) {
        fprintf(stderr,
            "[ir-pass] FAIL: default pipeline left temp-copy opportunities after same-operand chain case\n");
        ir_program_free(&program);
        return 0;
    }

    if (!ir_verify_program(&program, &verify_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: verifier rejected same-operand chain output at %d:%d: %s\n",
            verify_error.line,
            verify_error.column,
            verify_error.message);
        ir_program_free(&program);
        return 0;
    }

    ir_program_free(&program);
    return 1;
}

static int test_ir_pass_default_pipeline_preserves_dead_result_calls(void) {
    IrProgram program;
    IrError pass_error;
    IrError verify_error;
    size_t before_instruction_count;
    size_t after_instruction_count;
    size_t before_call_count;
    size_t after_call_count;

    if (!lower_source_to_ir_program(
            "int g(){return 1;} int f(){g()+0; return 0;}\n",
            &program)) {
        return 0;
    }

    before_instruction_count = count_total_instructions(&program);
    before_call_count = count_call_instructions(&program);
    if (before_instruction_count == 0 || before_call_count == 0) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected call-bearing dead-result chain before default pipeline\n");
        ir_program_free(&program);
        return 0;
    }

    if (!ir_pass_run_default_pipeline(&program, &pass_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: default pipeline failed on dead-result call case: %s\n",
            pass_error.message);
        ir_program_free(&program);
        return 0;
    }

    after_instruction_count = count_total_instructions(&program);
    after_call_count = count_call_instructions(&program);
    if (after_call_count != before_call_count) {
        fprintf(stderr,
            "[ir-pass] FAIL: default pipeline changed side-effecting call count (before=%zu, after=%zu)\n",
            before_call_count,
            after_call_count);
        ir_program_free(&program);
        return 0;
    }
    if (after_instruction_count >= before_instruction_count) {
        fprintf(stderr,
            "[ir-pass] FAIL: default pipeline did not clean pure work around dead-result call (before=%zu, after=%zu)\n",
            before_instruction_count,
            after_instruction_count);
        ir_program_free(&program);
        return 0;
    }

    if (!ir_verify_program(&program, &verify_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: verifier rejected dead-result call pipeline output at %d:%d: %s\n",
            verify_error.line,
            verify_error.column,
            verify_error.message);
        ir_program_free(&program);
        return 0;
    }

    ir_program_free(&program);
    return 1;
}

static int test_ir_pass_default_pipeline_preserves_dead_result_calls_through_cfg_cleanup(void) {
    IrProgram program;
    IrError pass_error;
    IrError verify_error;
    IrBasicBlock *branch_block;
    IrFunction *function;
    size_t before_call_count;
    size_t after_call_count;
    size_t after_instruction_count;

    if (!lower_source_to_ir_program(
            "int g(){return 1;} int f(int a){if(a) g()+0; else 0; return 0;}\n",
            &program)) {
        return 0;
    }

    branch_block = find_first_branch_block_mut(&program);
    function = &program.functions[1];
    if (!branch_block) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected branch block before dead-result call CFG cleanup case\n");
        ir_program_free(&program);
        return 0;
    }
    branch_block->terminator.as.branch.condition = make_immediate_value(1);

    before_call_count = count_call_instructions(&program);
    if (before_call_count != 1) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected exactly one call before dead-result call CFG cleanup case, found %zu\n",
            before_call_count);
        ir_program_free(&program);
        return 0;
    }

    if (!ir_pass_run_default_pipeline(&program, &pass_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: default pipeline failed on dead-result call CFG cleanup case: %s\n",
            pass_error.message);
        ir_program_free(&program);
        return 0;
    }

    after_call_count = count_call_instructions(&program);
    after_instruction_count = count_total_instructions(&program);
    if (after_call_count != before_call_count) {
        fprintf(stderr,
            "[ir-pass] FAIL: default pipeline changed call count during CFG cleanup case (before=%zu, after=%zu)\n",
            before_call_count,
            after_call_count);
        ir_program_free(&program);
        return 0;
    }
    if (count_branch_terminators(&program) != 0) {
        fprintf(stderr,
            "[ir-pass] FAIL: default pipeline left branch terminators in dead-result call CFG cleanup case\n");
        ir_program_free(&program);
        return 0;
    }
    if (function->block_count != 1) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected dead-result call CFG cleanup case to collapse f() to one block, got %zu\n",
            function->block_count);
        ir_program_free(&program);
        return 0;
    }
    if (after_instruction_count != after_call_count) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected dead-result call CFG cleanup case to keep only the call instruction, got %zu instructions and %zu calls\n",
            after_instruction_count,
            after_call_count);
        ir_program_free(&program);
        return 0;
    }
    if (count_dead_temp_defs(&program) != 0) {
        fprintf(stderr,
            "[ir-pass] FAIL: default pipeline left dead pure temp defs around dead-result call CFG cleanup case\n");
        ir_program_free(&program);
        return 0;
    }

    if (!ir_verify_program(&program, &verify_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: verifier rejected dead-result call CFG cleanup output at %d:%d: %s\n",
            verify_error.line,
            verify_error.column,
            verify_error.message);
        ir_program_free(&program);
        return 0;
    }

    ir_program_free(&program);
    return 1;
}

static int test_ir_pass_default_pipeline_preserves_multi_def_constant_paths(void) {
    IrProgram program;
    IrError pass_error;
    IrError verify_error;
    IrBasicBlock *branch_block;
    IrBasicBlock *then_block;
    IrBasicBlock *else_block;
    IrValueRef then_value;
    IrValueRef else_value;

    if (!lower_source_to_ir_program(
            "int f(int a){if(a) 1; else 2; return 0;}\n",
            &program)) {
        return 0;
    }
    if (!rewrite_as_multi_def_join_temp_program(&program, NULL)) {
        fprintf(stderr,
            "[ir-pass] FAIL: could not build default-pipeline multi-def constant-path case\n");
        ir_program_free(&program);
        return 0;
    }

    if (!ir_pass_run_default_pipeline(&program, &pass_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: default pipeline failed on multi-def constant-path case: %s\n",
            pass_error.message);
        ir_program_free(&program);
        return 0;
    }

    branch_block = find_first_branch_block_mut(&program);
    if (!branch_block || branch_block->terminator.as.branch.then_target >= program.functions[0].block_count
        || branch_block->terminator.as.branch.else_target >= program.functions[0].block_count) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected branch to remain in default-pipeline multi-def constant-path case\n");
        ir_program_free(&program);
        return 0;
    }
    then_block = &program.functions[0].blocks[branch_block->terminator.as.branch.then_target];
    else_block = &program.functions[0].blocks[branch_block->terminator.as.branch.else_target];
    if (!resolve_simple_block_return_value_for_test(then_block, &then_value)
        || !resolve_simple_block_return_value_for_test(else_block, &else_value)) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected default-pipeline multi-def constant-path case to keep directly returning successor paths\n");
        ir_program_free(&program);
        return 0;
    }
    if (then_value.kind != IR_VALUE_IMMEDIATE || then_value.immediate != 1
        || else_value.kind != IR_VALUE_IMMEDIATE || else_value.immediate != 2) {
        fprintf(stderr,
            "[ir-pass] FAIL: default pipeline changed multi-def constant-path returns (then kind=%d value=%lld, else kind=%d value=%lld)\n",
            (int)then_value.kind,
            then_value.immediate,
            (int)else_value.kind,
            else_value.immediate);
        ir_program_free(&program);
        return 0;
    }

    if (!ir_verify_program(&program, &verify_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: verifier rejected default-pipeline multi-def constant-path output at %d:%d: %s\n",
            verify_error.line,
            verify_error.column,
            verify_error.message);
        ir_program_free(&program);
        return 0;
    }

    ir_program_free(&program);
    return 1;
}

static int test_ir_pass_default_pipeline_preserves_multi_def_copy_paths(void) {
    IrProgram program;
    IrError pass_error;
    IrError verify_error;
    IrBasicBlock *branch_block;
    IrBasicBlock *then_block;
    IrBasicBlock *else_block;
    IrValueRef then_value;
    IrValueRef else_value;

    if (!lower_source_to_ir_program(
            "int f(int a, int b){if(a) 1; else 2; return 0;}\n",
            &program)) {
        return 0;
    }
    if (!rewrite_as_multi_def_join_value_program(&program,
            make_local_value(0),
            make_local_value(1),
            NULL)) {
        fprintf(stderr,
            "[ir-pass] FAIL: could not build default-pipeline multi-def copy-path case\n");
        ir_program_free(&program);
        return 0;
    }

    if (!ir_pass_run_default_pipeline(&program, &pass_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: default pipeline failed on multi-def copy-path case: %s\n",
            pass_error.message);
        ir_program_free(&program);
        return 0;
    }

    branch_block = find_first_branch_block_mut(&program);
    if (!branch_block || branch_block->terminator.as.branch.then_target >= program.functions[0].block_count
        || branch_block->terminator.as.branch.else_target >= program.functions[0].block_count) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected branch to remain in default-pipeline multi-def copy-path case\n");
        ir_program_free(&program);
        return 0;
    }
    then_block = &program.functions[0].blocks[branch_block->terminator.as.branch.then_target];
    else_block = &program.functions[0].blocks[branch_block->terminator.as.branch.else_target];
    if (!resolve_simple_block_return_value_for_test(then_block, &then_value)
        || !resolve_simple_block_return_value_for_test(else_block, &else_value)) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected default-pipeline multi-def copy-path case to keep directly returning successor paths\n");
        ir_program_free(&program);
        return 0;
    }
    if (then_value.kind != IR_VALUE_LOCAL || then_value.id != 0
        || else_value.kind != IR_VALUE_LOCAL || else_value.id != 1) {
        fprintf(stderr,
            "[ir-pass] FAIL: default pipeline changed multi-def copy-path returns (then kind=%d id=%zu, else kind=%d id=%zu)\n",
            (int)then_value.kind,
            then_value.id,
            (int)else_value.kind,
            else_value.id);
        ir_program_free(&program);
        return 0;
    }

    if (!ir_verify_program(&program, &verify_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: verifier rejected default-pipeline multi-def copy-path output at %d:%d: %s\n",
            verify_error.line,
            verify_error.column,
            verify_error.message);
        ir_program_free(&program);
        return 0;
    }

    ir_program_free(&program);
    return 1;
}

static int test_ir_pass_default_pipeline_simplifies_constant_condition_cfg(void) {
    IrProgram program;
    IrError pass_error;
    IrError verify_error;
    IrBasicBlock *branch_block;
    size_t before_blocks;
    size_t after_blocks;

    if (!lower_source_to_ir_program(
            "int f(int a){if(a) return 1; else return 2;}\n",
            &program)) {
        return 0;
    }

    branch_block = find_first_branch_block_mut(&program);
    if (!branch_block) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected branch block before default-pipeline CFG test\n");
        ir_program_free(&program);
        return 0;
    }
    branch_block->terminator.as.branch.condition.kind = IR_VALUE_IMMEDIATE;
    branch_block->terminator.as.branch.condition.immediate = 1;
    branch_block->terminator.as.branch.condition.id = 0;

    before_blocks = count_total_blocks(&program);
    if (before_blocks < 2) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected multi-block constant-condition CFG before default pipeline\n");
        ir_program_free(&program);
        return 0;
    }

    if (!ir_pass_run_default_pipeline(&program, &pass_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: default pipeline failed on constant-condition CFG case: %s\n",
            pass_error.message);
        ir_program_free(&program);
        return 0;
    }

    after_blocks = count_total_blocks(&program);
    if (count_immediate_branch_terminators(&program) != 0) {
        fprintf(stderr,
            "[ir-pass] FAIL: default pipeline left immediate branch terminators in constant-condition CFG case\n");
        ir_program_free(&program);
        return 0;
    }
    if (after_blocks >= before_blocks) {
        fprintf(stderr,
            "[ir-pass] FAIL: default pipeline did not reduce block count for constant-condition CFG case (before=%zu, after=%zu)\n",
            before_blocks,
            after_blocks);
        ir_program_free(&program);
        return 0;
    }

    if (!ir_verify_program(&program, &verify_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: verifier rejected default-pipeline CFG simplification output at %d:%d: %s\n",
            verify_error.line,
            verify_error.column,
            verify_error.message);
        ir_program_free(&program);
        return 0;
    }

    ir_program_free(&program);
    return 1;
}

static int test_ir_pass_default_pipeline_threads_empty_jump_blocks(void) {
    IrProgram program;
    IrError pass_error;
    IrError verify_error;
    IrFunction *function;
    size_t before_blocks;
    size_t after_blocks;

    if (!lower_source_to_ir_program(
            "int f(int a){if(a) return 1; else return 2;}\n",
            &program)) {
        return 0;
    }

    function = &program.functions[0];
    if (!function->has_body || function->block_count < 3) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected three-block branch CFG before default-pipeline jump-threading test\n");
        ir_program_free(&program);
        return 0;
    }
    function->blocks[1].terminator.kind = IR_TERM_JUMP;
    function->blocks[1].terminator.as.jump_target = 2;

    before_blocks = count_total_blocks(&program);
    if (count_branch_terminators(&program) == 0 || before_blocks < 3) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected branch plus trampoline block before default-pipeline jump-threading case\n");
        ir_program_free(&program);
        return 0;
    }

    if (!ir_pass_run_default_pipeline(&program, &pass_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: default pipeline failed on jump-threading CFG case: %s\n",
            pass_error.message);
        ir_program_free(&program);
        return 0;
    }

    after_blocks = count_total_blocks(&program);
    if (count_branch_terminators(&program) != 0) {
        fprintf(stderr,
            "[ir-pass] FAIL: default pipeline left branch terminators in jump-threading CFG case\n");
        ir_program_free(&program);
        return 0;
    }
    if (after_blocks >= before_blocks) {
        fprintf(stderr,
            "[ir-pass] FAIL: default pipeline did not reduce block count for jump-threading CFG case (before=%zu, after=%zu)\n",
            before_blocks,
            after_blocks);
        ir_program_free(&program);
        return 0;
    }

    if (!ir_verify_program(&program, &verify_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: verifier rejected default-pipeline jump-threading output at %d:%d: %s\n",
            verify_error.line,
            verify_error.column,
            verify_error.message);
        ir_program_free(&program);
        return 0;
    }

    ir_program_free(&program);
    return 1;
}

static int test_ir_pass_default_pipeline_folds_jump_to_empty_return_blocks(void) {
    IrProgram program;
    IrError pass_error;
    IrError verify_error;
    IrBasicBlock *branch_block;

    if (!lower_source_to_ir_program(
            "int f(int a){if(a) return 1; else return 2;}\n",
            &program)) {
        return 0;
    }

    branch_block = find_first_branch_block_mut(&program);
    if (!branch_block) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected branch block before default-pipeline jump-to-return test\n");
        ir_program_free(&program);
        return 0;
    }
    branch_block->terminator.as.branch.condition.kind = IR_VALUE_IMMEDIATE;
    branch_block->terminator.as.branch.condition.immediate = 1;
    branch_block->terminator.as.branch.condition.id = 0;

    if (!ir_pass_run_default_pipeline(&program, &pass_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: default pipeline failed on jump-to-return folding case: %s\n",
            pass_error.message);
        ir_program_free(&program);
        return 0;
    }

    if (count_jump_to_empty_return_blocks(&program) != 0) {
        fprintf(stderr,
            "[ir-pass] FAIL: default pipeline left jump-to-empty-return forwarding blocks behind\n");
        ir_program_free(&program);
        return 0;
    }
    if (count_total_blocks(&program) != 1) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected default pipeline jump-to-return case to collapse to one block, got %zu\n",
            count_total_blocks(&program));
        ir_program_free(&program);
        return 0;
    }

    if (!ir_verify_program(&program, &verify_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: verifier rejected default-pipeline jump-to-return output at %d:%d: %s\n",
            verify_error.line,
            verify_error.column,
            verify_error.message);
        ir_program_free(&program);
        return 0;
    }

    ir_program_free(&program);
    return 1;
}

static int test_ir_pass_default_pipeline_folds_branch_to_same_empty_return_value(void) {
    IrProgram program;
    IrError pass_error;
    IrError verify_error;
    size_t before_blocks;
    size_t after_blocks;

    if (!lower_source_to_ir_program(
            "int f(int a){if(a) return 9; else return 9;}\n",
            &program)) {
        return 0;
    }

    before_blocks = count_total_blocks(&program);
    if (count_branch_terminators(&program) == 0 || before_blocks < 3) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected branch diamond before default same-return CFG folding test\n");
        ir_program_free(&program);
        return 0;
    }

    if (!ir_pass_run_default_pipeline(&program, &pass_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: default pipeline failed on same-return CFG folding case: %s\n",
            pass_error.message);
        ir_program_free(&program);
        return 0;
    }

    after_blocks = count_total_blocks(&program);
    if (count_branch_terminators(&program) != 0) {
        fprintf(stderr,
            "[ir-pass] FAIL: default pipeline left branch terminators after same-return CFG folding case\n");
        ir_program_free(&program);
        return 0;
    }
    if (after_blocks != 1) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected default same-return CFG folding case to collapse to one block, got %zu\n",
            after_blocks);
        ir_program_free(&program);
        return 0;
    }

    if (!ir_verify_program(&program, &verify_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: verifier rejected default same-return CFG folded output at %d:%d: %s\n",
            verify_error.line,
            verify_error.column,
            verify_error.message);
        ir_program_free(&program);
        return 0;
    }

    ir_program_free(&program);
    return 1;
}

static int test_ir_pass_default_pipeline_folds_branch_to_same_empty_temp_return_value(void) {
    IrProgram program;
    IrError pass_error;
    IrError verify_error;
    size_t before_blocks;
    size_t after_blocks;
    size_t branch_block_id;
    size_t temp_id;
    (void)branch_block_id;
    (void)temp_id;

    if (!lower_source_to_ir_program(
            "int f(int a){if(a) return 1; else return 2;}\n",
            &program)) {
        return 0;
    }
    if (!rewrite_branch_successors_to_same_temp_return(&program, &branch_block_id, &temp_id)) {
        fprintf(stderr,
            "[ir-pass] FAIL: could not build default same-temp-return CFG folding case\n");
        ir_program_free(&program);
        return 0;
    }

    before_blocks = count_total_blocks(&program);
    if (count_branch_terminators(&program) == 0 || before_blocks < 3) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected branch diamond before default same-temp-return CFG folding test\n");
        ir_program_free(&program);
        return 0;
    }

    if (!ir_pass_run_default_pipeline(&program, &pass_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: default pipeline failed on same-temp-return CFG folding case: %s\n",
            pass_error.message);
        ir_program_free(&program);
        return 0;
    }

    after_blocks = count_total_blocks(&program);
    if (count_branch_terminators(&program) != 0) {
        fprintf(stderr,
            "[ir-pass] FAIL: default pipeline left branch terminators after same-temp-return CFG folding case\n");
        ir_program_free(&program);
        return 0;
    }
    if (after_blocks != 1) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected default same-temp-return CFG folding case to collapse to one block, got %zu\n",
            after_blocks);
        ir_program_free(&program);
        return 0;
    }
    if (count_total_instructions(&program) != 0) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected default same-temp-return CFG folding case to clean the temp-producing mov after copy propagation\n");
        ir_program_free(&program);
        return 0;
    }

    if (!ir_verify_program(&program, &verify_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: verifier rejected default same-temp-return CFG folded output at %d:%d: %s\n",
            verify_error.line,
            verify_error.column,
            verify_error.message);
        ir_program_free(&program);
        return 0;
    }

    ir_program_free(&program);
    return 1;
}

static int test_ir_pass_default_pipeline_revisits_cfg_after_dce_exposes_empty_returns(void) {
    IrProgram program;
    IrError pass_error;
    IrError verify_error;
    size_t before_blocks;
    size_t after_blocks;

    if (!lower_source_to_ir_program(
            "int f(int a){if(a) return 1; else return 2;}\n",
            &program)) {
        return 0;
    }
    if (!rewrite_branch_successors_to_local_constant_returns(&program, 9, 9)) {
        fprintf(stderr,
            "[ir-pass] FAIL: could not build post-DCE same-return CFG cleanup case\n");
        ir_program_free(&program);
        return 0;
    }

    before_blocks = count_total_blocks(&program);
    if (count_branch_terminators(&program) == 0 || before_blocks < 3) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected branch diamond before post-DCE CFG cleanup case\n");
        ir_program_free(&program);
        return 0;
    }

    if (!ir_pass_run_default_pipeline(&program, &pass_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: default pipeline failed on post-DCE CFG cleanup case: %s\n",
            pass_error.message);
        ir_program_free(&program);
        return 0;
    }

    after_blocks = count_total_blocks(&program);
    if (count_branch_terminators(&program) != 0) {
        fprintf(stderr,
            "[ir-pass] FAIL: default pipeline left branch terminators after post-DCE CFG cleanup case\n");
        ir_program_free(&program);
        return 0;
    }
    if (after_blocks != 1) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected post-DCE CFG cleanup case to collapse to one block, got %zu\n",
            after_blocks);
        ir_program_free(&program);
        return 0;
    }
    if (count_total_instructions(&program) != 0) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected post-DCE CFG cleanup case to remove temp-def instructions after revisiting CFG\n");
        ir_program_free(&program);
        return 0;
    }
    if (program.functions[0].blocks[0].terminator.kind != IR_TERM_RETURN
        || program.functions[0].blocks[0].terminator.as.return_value.kind != IR_VALUE_IMMEDIATE
        || program.functions[0].blocks[0].terminator.as.return_value.immediate != 9) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected post-DCE CFG cleanup case to canonicalize to ret 9\n");
        ir_program_free(&program);
        return 0;
    }

    if (!ir_verify_program(&program, &verify_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: verifier rejected post-DCE CFG cleanup output at %d:%d: %s\n",
            verify_error.line,
            verify_error.column,
            verify_error.message);
        ir_program_free(&program);
        return 0;
    }

    ir_program_free(&program);
    return 1;
}

static int test_ir_pass_default_pipeline_merges_single_predecessor_nonempty_jump_blocks(void) {
    IrProgram program;
    IrError pass_error;
    IrError verify_error;
    IrBasicBlock *branch_block;

    if (!lower_source_to_ir_program(
            "int f(int a){if(a) return 1; return a+1;}\n",
            &program)) {
        return 0;
    }

    branch_block = find_first_branch_block_mut(&program);
    if (!branch_block) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected branch block before default-pipeline single-predecessor merge test\n");
        ir_program_free(&program);
        return 0;
    }
    branch_block->terminator.as.branch.condition.kind = IR_VALUE_IMMEDIATE;
    branch_block->terminator.as.branch.condition.immediate = 0;
    branch_block->terminator.as.branch.condition.id = 0;

    if (!ir_pass_run_default_pipeline(&program, &pass_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: default pipeline failed on single-predecessor nonempty merge case: %s\n",
            pass_error.message);
        ir_program_free(&program);
        return 0;
    }

    if (count_total_blocks(&program) != 1) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected default-pipeline single-predecessor nonempty merge case to collapse to one block, got %zu\n",
            count_total_blocks(&program));
        ir_program_free(&program);
        return 0;
    }
    if (count_total_instructions(&program) == 0) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected default-pipeline single-predecessor nonempty merge case to keep successor instructions\n");
        ir_program_free(&program);
        return 0;
    }
    if (count_branch_terminators(&program) != 0) {
        fprintf(stderr,
            "[ir-pass] FAIL: default pipeline left branch terminators in single-predecessor nonempty merge case\n");
        ir_program_free(&program);
        return 0;
    }

    if (!ir_verify_program(&program, &verify_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: verifier rejected default-pipeline single-predecessor merged CFG at %d:%d: %s\n",
            verify_error.line,
            verify_error.column,
            verify_error.message);
        ir_program_free(&program);
        return 0;
    }

    ir_program_free(&program);
    return 1;
}

static int test_ir_pass_default_pipeline_reaches_fixed_point_on_representative_cases(void) {
    static const IrPassFixedPointCase cases[] = {
        {
            "IR-PASS-FIXED-POINT-FOLD-CLEANUP",
            "int f(){return (1+2)+3;}\n",
            NULL,
        },
        {
            "IR-PASS-FIXED-POINT-DEAD-CALL",
            "int g(){return 1;} int f(){g()+0; return 0;}\n",
            NULL,
        },
        {
            "IR-PASS-FIXED-POINT-POST-DCE-CFG",
            "int f(int a){if(a) return 1; else return 2;}\n",
            setup_post_dce_same_return_fixed_point_case,
        },
    };
    size_t case_index;

    for (case_index = 0; case_index < sizeof(cases) / sizeof(cases[0]); ++case_index) {
        if (!expect_default_pipeline_fixed_point_case(&cases[case_index])) {
            return 0;
        }
    }

    return 1;
}

static int test_ir_pass_rejects_invalid_pass_spec(void) {
    IrProgram program;
    IrPassSpec passes[1];
    IrError error;

    ir_program_init(&program);
    passes[0].name = "broken";
    passes[0].run = NULL;

    if (ir_pass_run_pipeline(&program, passes, 1, &error)) {
        fprintf(stderr, "[ir-pass] FAIL: invalid pass spec unexpectedly passed\n");
        ir_program_free(&program);
        return 0;
    }

    if (!expect_error_fragment("IR-PASS-INVALID-SPEC", &error, "IR-PASS-003")) {
        ir_program_free(&program);
        return 0;
    }

    ir_program_free(&program);
    return 1;
}

static int test_ir_pass_rejects_null_pass_table_with_nonzero_count(void) {
    IrProgram program;
    IrError error;

    ir_program_init(&program);

    if (ir_pass_run_pipeline(&program, NULL, 1, &error)) {
        fprintf(stderr, "[ir-pass] FAIL: null pass table unexpectedly passed\n");
        ir_program_free(&program);
        return 0;
    }

    if (!expect_error_fragment("IR-PASS-NULL-TABLE", &error, "IR-PASS-002")) {
        ir_program_free(&program);
        return 0;
    }

    ir_program_free(&program);
    return 1;
}

int main(void) {
    if (!test_ir_pass_default_pipeline_folds_safe_immediate_binary_ops()) {
        return 1;
    }
    if (!test_ir_pass_fold_simplifies_safe_binary_identities()) {
        return 1;
    }
    if (!test_ir_pass_fold_simplifies_safe_same_operand_binaries()) {
        return 1;
    }
    if (!test_ir_pass_default_pipeline_skips_unsafe_immediate_binary_ops()) {
        return 1;
    }
    if (!test_ir_pass_eliminates_dead_temp_defs_directly()) {
        return 1;
    }
    if (!test_ir_pass_preserves_dead_result_calls_during_dce()) {
        return 1;
    }
    if (!test_ir_pass_propagates_temp_copies_after_fold()) {
        return 1;
    }
    if (!test_ir_pass_propagates_single_use_call_arg_copies()) {
        return 1;
    }
    if (!test_ir_pass_copy_propagation_skips_multi_def_join_temps()) {
        return 1;
    }
    if (!test_ir_pass_propagates_temp_constants_across_binary_chains()) {
        return 1;
    }
    if (!test_ir_pass_constant_propagation_skips_multi_def_join_temps()) {
        return 1;
    }
    if (!test_ir_pass_simplifies_constant_branches_and_removes_dead_blocks()) {
        return 1;
    }
    if (!test_ir_pass_threads_empty_jump_blocks_and_removes_trampolines()) {
        return 1;
    }
    if (!test_ir_pass_folds_jump_to_empty_return_blocks()) {
        return 1;
    }
    if (!test_ir_pass_folds_branch_to_same_empty_return_value()) {
        return 1;
    }
    if (!test_ir_pass_folds_branch_to_same_empty_temp_return_value()) {
        return 1;
    }
    if (!test_ir_pass_folds_branch_to_equivalent_constant_temp_returns()) {
        return 1;
    }
    if (!test_ir_pass_folds_branch_to_equivalent_copy_temp_returns()) {
        return 1;
    }
    if (!test_ir_pass_merges_single_predecessor_nonempty_jump_blocks()) {
        return 1;
    }
    if (!test_ir_pass_default_pipeline_cleans_folded_dead_temps()) {
        return 1;
    }
    if (!test_ir_pass_default_pipeline_closes_temp_constant_chains()) {
        return 1;
    }
    if (!test_ir_pass_default_pipeline_cleans_binary_identity_chains()) {
        return 1;
    }
    if (!test_ir_pass_default_pipeline_cleans_same_operand_binary_chains()) {
        return 1;
    }
    if (!test_ir_pass_default_pipeline_preserves_dead_result_calls()) {
        return 1;
    }
    if (!test_ir_pass_default_pipeline_preserves_dead_result_calls_through_cfg_cleanup()) {
        return 1;
    }
    if (!test_ir_pass_default_pipeline_preserves_multi_def_constant_paths()) {
        return 1;
    }
    if (!test_ir_pass_default_pipeline_preserves_multi_def_copy_paths()) {
        return 1;
    }
    if (!test_ir_pass_default_pipeline_simplifies_constant_condition_cfg()) {
        return 1;
    }
    if (!test_ir_pass_default_pipeline_threads_empty_jump_blocks()) {
        return 1;
    }
    if (!test_ir_pass_default_pipeline_folds_jump_to_empty_return_blocks()) {
        return 1;
    }
    if (!test_ir_pass_default_pipeline_folds_branch_to_same_empty_return_value()) {
        return 1;
    }
    if (!test_ir_pass_default_pipeline_folds_branch_to_same_empty_temp_return_value()) {
        return 1;
    }
    if (!test_ir_pass_default_pipeline_revisits_cfg_after_dce_exposes_empty_returns()) {
        return 1;
    }
    if (!test_ir_pass_default_pipeline_merges_single_predecessor_nonempty_jump_blocks()) {
        return 1;
    }
    if (!test_ir_pass_default_pipeline_reaches_fixed_point_on_representative_cases()) {
        return 1;
    }
    if (!test_ir_pass_rejects_invalid_pass_spec()) {
        return 1;
    }
    if (!test_ir_pass_rejects_null_pass_table_with_nonzero_count()) {
        return 1;
    }

    printf("[ir-pass] All IR pass tests passed.\n");
    return 0;
}
