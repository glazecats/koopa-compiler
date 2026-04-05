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

static size_t count_call_instructions(const IrProgram *program);
static size_t count_branch_terminators(const IrProgram *program);

static int expect_program_dump(const char *case_id,
    const IrProgram *program,
    const char *expected_text) {
    char *actual_text = NULL;
    int ok;

    if (!case_id || !program || !expected_text) {
        return 0;
    }

    if (!ir_dump_program(program, &actual_text) || !actual_text) {
        fprintf(stderr,
            "[ir-pass] FAIL: %s could not dump IR\n",
            case_id);
        free(actual_text);
        return 0;
    }

    ok = strcmp(actual_text, expected_text) == 0;
    if (!ok) {
        fprintf(stderr,
            "[ir-pass] FAIL: %s IR mismatch\nexpected:\n%s\nactual:\n%s\n",
            case_id,
            expected_text,
            actual_text);
    }

    free(actual_text);
    return ok;
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

static int rewrite_branch_successors_to_local_value_returns(IrProgram *program,
    IrValueRef then_value,
    IrValueRef else_value) {
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
    then_instructions[0].as.mov_value = then_value;
    then_block->instructions = then_instructions;
    then_block->instruction_count = 1;
    then_block->instruction_capacity = 1;
    then_block->terminator.as.return_value = make_temp_value(then_temp_id);

    else_instructions[0].kind = IR_INSTR_MOV;
    else_instructions[0].result = make_temp_value(else_temp_id);
    else_instructions[0].as.mov_value = else_value;
    else_block->instructions = else_instructions;
    else_block->instruction_count = 1;
    else_block->instruction_capacity = 1;
    else_block->terminator.as.return_value = make_temp_value(else_temp_id);

    function->next_temp_id = else_temp_id + 1;
    return 1;
}

static int rewrite_branch_successors_to_local_constant_returns(IrProgram *program,
    long long then_value,
    long long else_value) {
    return rewrite_branch_successors_to_local_value_returns(program,
        make_immediate_value(then_value),
        make_immediate_value(else_value));
}

typedef int (*IrPassProgramSetupFn)(IrProgram *program);
typedef int (*IrPassProgramStepCheckFn)(const char *case_id,
    const IrProgram *program,
    const char *pass_name,
    size_t pass_index);

typedef struct {
    const char *case_id;
    const char *source;
    IrPassProgramSetupFn setup;
} IrPassFixedPointCase;

typedef struct {
    const char *case_id;
    const char *source;
    IrPassProgramSetupFn setup;
    IrPassProgramStepCheckFn step_check;
} IrPassStepwiseCase;

static int setup_post_dce_same_return_fixed_point_case(IrProgram *program) {
    return rewrite_branch_successors_to_local_constant_returns(program, 9, 9);
}

static int setup_branch_condition_true_fixed_point_case(IrProgram *program) {
    IrBasicBlock *branch_block;

    branch_block = find_first_branch_block_mut(program);
    if (!branch_block) {
        return 0;
    }

    branch_block->terminator.as.branch.condition = make_immediate_value(1);
    return 1;
}

static int setup_multi_def_constant_path_case(IrProgram *program) {
    return rewrite_as_multi_def_join_temp_program(program, NULL);
}

static int setup_multi_def_copy_path_case(IrProgram *program) {
    return rewrite_as_multi_def_join_value_program(program,
        make_local_value(0),
        make_local_value(1),
        NULL);
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

static int expect_default_pipeline_stepwise_case(const IrPassStepwiseCase *test_case) {
    static const IrPassSpec k_default_passes[] = {
        {"fold-immediate-binary", ir_pass_fold_immediate_binary},
        {"propagate-temp-constants", ir_pass_propagate_temp_constants},
        {"propagate-temp-copies", ir_pass_propagate_temp_copies},
        {"fold-immediate-binary", ir_pass_fold_immediate_binary},
        {"propagate-temp-constants", ir_pass_propagate_temp_constants},
        {"propagate-temp-copies", ir_pass_propagate_temp_copies},
        {"simplify-cfg", ir_pass_simplify_cfg},
        {"eliminate-dead-temp-defs", ir_pass_eliminate_dead_temp_defs},
        {"simplify-cfg", ir_pass_simplify_cfg},
        {"eliminate-dead-temp-defs", ir_pass_eliminate_dead_temp_defs},
    };
    IrProgram program;
    IrError pass_error;
    IrError verify_error;
    size_t pass_index;

    if (!test_case || !test_case->case_id || !test_case->source) {
        return 0;
    }

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

    for (pass_index = 0; pass_index < sizeof(k_default_passes) / sizeof(k_default_passes[0]); ++pass_index) {
        if (!k_default_passes[pass_index].run(&program, &pass_error)) {
            fprintf(stderr,
                "[ir-pass] FAIL: %s step %zu (%s) failed: %s\n",
                test_case->case_id,
                pass_index,
                k_default_passes[pass_index].name,
                pass_error.message);
            ir_program_free(&program);
            return 0;
        }

        if (!ir_verify_program(&program, &verify_error)) {
            fprintf(stderr,
                "[ir-pass] FAIL: %s step %zu (%s) violated verifier at %d:%d: %s\n",
                test_case->case_id,
                pass_index,
                k_default_passes[pass_index].name,
                verify_error.line,
                verify_error.column,
                verify_error.message);
            ir_program_free(&program);
            return 0;
        }

        if (test_case->step_check
            && !test_case->step_check(test_case->case_id,
                &program,
                k_default_passes[pass_index].name,
                pass_index)) {
            ir_program_free(&program);
            return 0;
        }
    }

    ir_program_free(&program);
    return 1;
}

static int check_stepwise_call_count_stays_one(const char *case_id,
    const IrProgram *program,
    const char *pass_name,
    size_t pass_index) {
    size_t call_count;

    if (!case_id || !program || !pass_name) {
        return 0;
    }

    call_count = count_call_instructions(program);
    if (call_count != 1) {
        fprintf(stderr,
            "[ir-pass] FAIL: %s step %zu (%s) changed side-effecting call count to %zu\n",
            case_id,
            pass_index,
            pass_name,
            call_count);
        return 0;
    }

    return 1;
}

static int check_stepwise_branch_distinction_remains(const char *case_id,
    const IrProgram *program,
    const char *pass_name,
    size_t pass_index) {
    size_t branch_count;

    if (!case_id || !program || !pass_name) {
        return 0;
    }

    branch_count = count_branch_terminators(program);
    if (branch_count == 0) {
        fprintf(stderr,
            "[ir-pass] FAIL: %s step %zu (%s) collapsed branch distinction unexpectedly\n",
            case_id,
            pass_index,
            pass_name);
        return 0;
    }

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

static int binary_op_requires_runtime_validation_for_test(IrBinaryOp op) {
    switch (op) {
    case IR_BINARY_DIV:
    case IR_BINARY_MOD:
    case IR_BINARY_SHIFT_LEFT:
    case IR_BINARY_SHIFT_RIGHT:
        return 1;
    default:
        return 0;
    }
}

static int instruction_has_side_effects_for_test(const IrInstruction *instruction) {
    if (!instruction) {
        return 1;
    }

    switch (instruction->kind) {
    case IR_INSTR_MOV:
        return 0;
    case IR_INSTR_BINARY:
        return binary_op_requires_runtime_validation_for_test(instruction->as.binary.op);
    case IR_INSTR_CALL:
    default:
        return 1;
    }
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
                    && !instruction_has_side_effects_for_test(instruction)) {
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

static size_t count_binary_instructions_with_op(const IrProgram *program, IrBinaryOp op) {
    size_t function_index;
    size_t binary_count = 0;

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

                if (instruction->kind == IR_INSTR_BINARY && instruction->as.binary.op == op) {
                    ++binary_count;
                }
            }
        }
    }

    return binary_count;
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
        return 0;
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

/* Split fragment inventory (keep this list in sync with includes below):
 * - ir_pass_test_direct.inc
 * - ir_pass_test_pipeline.inc
 */
#include "ir_pass_test_direct.inc"
#include "ir_pass_test_pipeline.inc"

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
    if (!test_ir_pass_folds_branch_to_same_local_constant_return_value()) {
        return 1;
    }
    if (!test_ir_pass_folds_branch_to_same_local_copy_return_value()) {
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
    if (!test_ir_pass_preserves_dead_result_runtime_checked_binaries_during_dce()) {
        return 1;
    }
    if (!test_ir_pass_default_pipeline_preserves_dead_result_calls()) {
        return 1;
    }
    if (!test_ir_pass_default_pipeline_preserves_dead_result_division()) {
        return 1;
    }
    if (!test_ir_pass_default_pipeline_preserves_division_through_zero_multiply_fold()) {
        return 1;
    }
    if (!test_ir_pass_default_pipeline_preserves_dead_result_calls_through_cfg_cleanup()) {
        return 1;
    }
    if (!test_ir_pass_default_pipeline_canonicalizes_dead_result_call_cfg_cleanup()) {
        return 1;
    }
    if (!test_ir_pass_default_pipeline_preserves_multi_def_constant_paths()) {
        return 1;
    }
    if (!test_ir_pass_default_pipeline_canonicalizes_multi_def_constant_paths()) {
        return 1;
    }
    if (!test_ir_pass_default_pipeline_preserves_multi_def_copy_paths()) {
        return 1;
    }
    if (!test_ir_pass_default_pipeline_canonicalizes_post_dce_cfg_cleanup()) {
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
    if (!test_ir_pass_default_pipeline_canonicalizes_same_temp_return_fold()) {
        return 1;
    }
    if (!test_ir_pass_default_pipeline_revisits_cfg_after_dce_exposes_empty_returns()) {
        return 1;
    }
    if (!test_ir_pass_default_pipeline_merges_single_predecessor_nonempty_jump_blocks()) {
        return 1;
    }
    if (!test_ir_pass_default_pipeline_canonicalizes_single_predecessor_merge()) {
        return 1;
    }
    if (!test_ir_pass_default_pipeline_reaches_fixed_point_on_representative_cases()) {
        return 1;
    }
    if (!test_ir_pass_default_pipeline_stepwise_preserves_representative_invariants()) {
        return 1;
    }
    if (!test_ir_pass_rejects_invalid_pass_spec()) {
        return 1;
    }
    if (!test_ir_pass_rejects_null_pass_table_with_nonzero_count()) {
        return 1;
    }
    if (!test_ir_pass_rejects_verifier_invalid_pipeline_input()) {
        return 1;
    }
    if (!test_ir_pass_rejects_verifier_invalid_pass_output()) {
        return 1;
    }

    printf("[ir-pass] All IR pass tests passed.\n");
    return 0;
}
