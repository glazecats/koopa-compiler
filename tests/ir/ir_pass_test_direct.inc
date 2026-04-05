#include "ir_pass_test_intellisense_prelude.inc"

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

static int test_ir_pass_preserves_dead_result_runtime_checked_binaries_during_dce(void) {
    IrProgram program;
    IrError pass_error;
    IrError verify_error;
    size_t before_instruction_count;
    size_t after_instruction_count;

    if (!lower_source_to_ir_program(
            "int f(int a){a/0; a<<a; return 0;}\n",
            &program)) {
        return 0;
    }

    before_instruction_count = count_total_instructions(&program);
    if (count_binary_instructions_with_op(&program, IR_BINARY_DIV) != 1
        || count_binary_instructions_with_op(&program, IR_BINARY_SHIFT_LEFT) != 1) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected dead-result runtime-checked binaries before DCE regression case\n");
        ir_program_free(&program);
        return 0;
    }

    if (!ir_pass_eliminate_dead_temp_defs(&program, &pass_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: dead-temp elimination failed on runtime-checked binary case: %s\n",
            pass_error.message);
        ir_program_free(&program);
        return 0;
    }

    after_instruction_count = count_total_instructions(&program);
    if (after_instruction_count != before_instruction_count) {
        fprintf(stderr,
            "[ir-pass] FAIL: DCE changed instruction count for runtime-checked binary case (before=%zu, after=%zu)\n",
            before_instruction_count,
            after_instruction_count);
        ir_program_free(&program);
        return 0;
    }
    if (count_binary_instructions_with_op(&program, IR_BINARY_DIV) != 1
        || count_binary_instructions_with_op(&program, IR_BINARY_SHIFT_LEFT) != 1) {
        fprintf(stderr,
            "[ir-pass] FAIL: DCE removed runtime-checked binaries with dead results\n");
        ir_program_free(&program);
        return 0;
    }

    if (!ir_verify_program(&program, &verify_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: verifier rejected runtime-checked binary DCE output at %d:%d: %s\n",
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

static int test_ir_pass_folds_branch_to_same_local_constant_return_value(void) {
    IrProgram program;
    IrError pass_error;
    IrError verify_error;

    if (!lower_source_to_ir_program(
            "int f(int a){if(a) return 1; else return 2;}\n",
            &program)) {
        return 0;
    }
    if (!rewrite_branch_successors_to_local_constant_returns(&program, 9, 9)) {
        fprintf(stderr,
            "[ir-pass] FAIL: could not build same-local-constant-return CFG folding case\n");
        ir_program_free(&program);
        return 0;
    }

    if (!ir_pass_simplify_cfg(&program, &pass_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: same-local-constant-return CFG folding failed: %s\n",
            pass_error.message);
        ir_program_free(&program);
        return 0;
    }

    if (!ir_verify_program(&program, &verify_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: verifier rejected same-local-constant-return CFG folded output at %d:%d: %s\n",
            verify_error.line,
            verify_error.column,
            verify_error.message);
        ir_program_free(&program);
        return 0;
    }

    if (!expect_program_dump("IR-PASS-SAME-LOCAL-CONSTANT-RETURN",
            &program,
            "func f(a.0) {\n"
            "  bb.0:\n"
            "    ret 9\n"
            "}\n")) {
        ir_program_free(&program);
        return 0;
    }

    ir_program_free(&program);
    return 1;
}

static int test_ir_pass_folds_branch_to_same_local_copy_return_value(void) {
    IrProgram program;
    IrError pass_error;
    IrError verify_error;

    if (!lower_source_to_ir_program(
            "int f(int a){if(a) return 1; else return 2;}\n",
            &program)) {
        return 0;
    }
    if (!rewrite_branch_successors_to_local_value_returns(&program,
            make_local_value(0),
            make_local_value(0))) {
        fprintf(stderr,
            "[ir-pass] FAIL: could not build same-local-copy-return CFG folding case\n");
        ir_program_free(&program);
        return 0;
    }

    if (!ir_pass_simplify_cfg(&program, &pass_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: same-local-copy-return CFG folding failed: %s\n",
            pass_error.message);
        ir_program_free(&program);
        return 0;
    }

    if (!ir_verify_program(&program, &verify_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: verifier rejected same-local-copy-return CFG folded output at %d:%d: %s\n",
            verify_error.line,
            verify_error.column,
            verify_error.message);
        ir_program_free(&program);
        return 0;
    }

    if (!expect_program_dump("IR-PASS-SAME-LOCAL-COPY-RETURN",
            &program,
            "func f(a.0) {\n"
            "  bb.0:\n"
            "    ret a.0\n"
            "}\n")) {
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
