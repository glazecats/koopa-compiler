#include "ir_pass_test_intellisense_prelude.inc"

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

static int test_ir_pass_default_pipeline_preserves_dead_result_division(void) {
    IrProgram program;
    IrError pass_error;
    IrError verify_error;

    if (!lower_source_to_ir_program(
            "int f(int a){a/0; return 0;}\n",
            &program)) {
        return 0;
    }

    if (count_binary_instructions_with_op(&program, IR_BINARY_DIV) != 1) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected dead-result division before default-pipeline preservation case\n");
        ir_program_free(&program);
        return 0;
    }

    if (!ir_pass_run_default_pipeline(&program, &pass_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: default pipeline failed on dead-result division case: %s\n",
            pass_error.message);
        ir_program_free(&program);
        return 0;
    }

    if (count_binary_instructions_with_op(&program, IR_BINARY_DIV) != 1) {
        fprintf(stderr,
            "[ir-pass] FAIL: default pipeline removed dead-result division instruction\n");
        ir_program_free(&program);
        return 0;
    }
    if (count_total_instructions(&program) != 1) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected dead-result division case to keep exactly one binary instruction, got %zu\n",
            count_total_instructions(&program));
        ir_program_free(&program);
        return 0;
    }

    if (!ir_verify_program(&program, &verify_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: verifier rejected dead-result division output at %d:%d: %s\n",
            verify_error.line,
            verify_error.column,
            verify_error.message);
        ir_program_free(&program);
        return 0;
    }

    ir_program_free(&program);
    return 1;
}

static int test_ir_pass_default_pipeline_preserves_division_through_zero_multiply_fold(void) {
    IrProgram program;
    IrError pass_error;
    IrError verify_error;

    if (!lower_source_to_ir_program(
            "int f(int a){return (a/0)*0;}\n",
            &program)) {
        return 0;
    }

    if (count_binary_instructions_with_op(&program, IR_BINARY_DIV) != 1) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected division before zero-multiply fold preservation case\n");
        ir_program_free(&program);
        return 0;
    }

    if (!ir_pass_run_default_pipeline(&program, &pass_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: default pipeline failed on zero-multiply fold preservation case: %s\n",
            pass_error.message);
        ir_program_free(&program);
        return 0;
    }

    if (count_binary_instructions_with_op(&program, IR_BINARY_DIV) != 1) {
        fprintf(stderr,
            "[ir-pass] FAIL: default pipeline erased division through zero-multiply fold case\n");
        ir_program_free(&program);
        return 0;
    }
    if (program.function_count == 0
        || program.functions[0].block_count == 0
        || !program.functions[0].blocks[0].has_terminator
        || program.functions[0].blocks[0].terminator.kind != IR_TERM_RETURN
        || program.functions[0].blocks[0].terminator.as.return_value.kind != IR_VALUE_IMMEDIATE
        || program.functions[0].blocks[0].terminator.as.return_value.immediate != 0) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected zero-multiply fold preservation case to still canonicalize return value to 0\n");
        ir_program_free(&program);
        return 0;
    }

    if (!ir_verify_program(&program, &verify_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: verifier rejected zero-multiply fold preservation output at %d:%d: %s\n",
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

static int test_ir_pass_default_pipeline_canonicalizes_dead_result_call_cfg_cleanup(void) {
    IrProgram program;
    IrError pass_error;
    IrError verify_error;
    IrBasicBlock *branch_block;

    if (!lower_source_to_ir_program(
            "int g(){return 1;} int f(int a){if(a) g()+0; else 0; return 0;}\n",
            &program)) {
        return 0;
    }

    branch_block = find_first_branch_block_mut(&program);
    if (!branch_block) {
        fprintf(stderr,
            "[ir-pass] FAIL: expected branch block before dead-result call canonical-form case\n");
        ir_program_free(&program);
        return 0;
    }
    branch_block->terminator.as.branch.condition = make_immediate_value(1);

    if (!ir_pass_run_default_pipeline(&program, &pass_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: default pipeline failed on dead-result call canonical-form case: %s\n",
            pass_error.message);
        ir_program_free(&program);
        return 0;
    }

    if (!ir_verify_program(&program, &verify_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: verifier rejected dead-result call canonical-form output at %d:%d: %s\n",
            verify_error.line,
            verify_error.column,
            verify_error.message);
        ir_program_free(&program);
        return 0;
    }

    if (!expect_program_dump("IR-PASS-DEAD-CALL-CFG-CANONICAL",
            &program,
            "func g() {\n"
            "  bb.0:\n"
            "    ret 1\n"
            "}\n"
            "\n"
            "func f(a.0) {\n"
            "  bb.0:\n"
            "    tmp.0 = call g()\n"
            "    ret 0\n"
            "}\n")) {
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

static int test_ir_pass_default_pipeline_canonicalizes_multi_def_constant_paths(void) {
    IrProgram program;
    IrError pass_error;
    IrError verify_error;

    if (!lower_source_to_ir_program(
            "int f(int a){if(a) 1; else 2; return 0;}\n",
            &program)) {
        return 0;
    }
    if (!rewrite_as_multi_def_join_temp_program(&program, NULL)) {
        fprintf(stderr,
            "[ir-pass] FAIL: could not build multi-def constant canonical-form case\n");
        ir_program_free(&program);
        return 0;
    }

    if (!ir_pass_run_default_pipeline(&program, &pass_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: default pipeline failed on multi-def constant canonical-form case: %s\n",
            pass_error.message);
        ir_program_free(&program);
        return 0;
    }

    if (!ir_verify_program(&program, &verify_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: verifier rejected multi-def constant canonical-form output at %d:%d: %s\n",
            verify_error.line,
            verify_error.column,
            verify_error.message);
        ir_program_free(&program);
        return 0;
    }

    if (!expect_program_dump("IR-PASS-MULTI-DEF-CONSTANT-CANONICAL",
            &program,
            "func f(a.0) {\n"
            "  bb.0:\n"
            "    br a.0, bb.1, bb.2\n"
            "  bb.1:\n"
            "    tmp.0 = mov 1\n"
            "    ret tmp.0\n"
            "  bb.2:\n"
            "    tmp.0 = mov 2\n"
            "    ret tmp.0\n"
            "}\n")) {
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

static int test_ir_pass_default_pipeline_canonicalizes_post_dce_cfg_cleanup(void) {
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
            "[ir-pass] FAIL: could not build post-DCE canonical-form case\n");
        ir_program_free(&program);
        return 0;
    }

    if (!ir_pass_run_default_pipeline(&program, &pass_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: default pipeline failed on post-DCE canonical-form case: %s\n",
            pass_error.message);
        ir_program_free(&program);
        return 0;
    }

    if (!ir_verify_program(&program, &verify_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: verifier rejected post-DCE canonical-form output at %d:%d: %s\n",
            verify_error.line,
            verify_error.column,
            verify_error.message);
        ir_program_free(&program);
        return 0;
    }

    if (!expect_program_dump("IR-PASS-POST-DCE-CFG-CANONICAL",
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

static int test_ir_pass_default_pipeline_canonicalizes_same_temp_return_fold(void) {
    IrProgram program;
    IrError pass_error;
    IrError verify_error;
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
            "[ir-pass] FAIL: could not build same-temp-return canonical-form case\n");
        ir_program_free(&program);
        return 0;
    }

    if (!ir_pass_run_default_pipeline(&program, &pass_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: default pipeline failed on same-temp-return canonical-form case: %s\n",
            pass_error.message);
        ir_program_free(&program);
        return 0;
    }

    if (!ir_verify_program(&program, &verify_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: verifier rejected same-temp-return canonical-form output at %d:%d: %s\n",
            verify_error.line,
            verify_error.column,
            verify_error.message);
        ir_program_free(&program);
        return 0;
    }

    if (!expect_program_dump("IR-PASS-SAME-TEMP-RETURN-CANONICAL",
            &program,
            "func f(a.0) {\n"
            "  bb.0:\n"
            "    ret 7\n"
            "}\n")) {
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

static int test_ir_pass_default_pipeline_canonicalizes_single_predecessor_merge(void) {
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
            "[ir-pass] FAIL: expected branch block before single-predecessor canonical-form case\n");
        ir_program_free(&program);
        return 0;
    }
    branch_block->terminator.as.branch.condition.kind = IR_VALUE_IMMEDIATE;
    branch_block->terminator.as.branch.condition.immediate = 0;
    branch_block->terminator.as.branch.condition.id = 0;

    if (!ir_pass_run_default_pipeline(&program, &pass_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: default pipeline failed on single-predecessor canonical-form case: %s\n",
            pass_error.message);
        ir_program_free(&program);
        return 0;
    }

    if (!ir_verify_program(&program, &verify_error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: verifier rejected single-predecessor canonical-form output at %d:%d: %s\n",
            verify_error.line,
            verify_error.column,
            verify_error.message);
        ir_program_free(&program);
        return 0;
    }

    if (!expect_program_dump("IR-PASS-SINGLE-PREDECESSOR-MERGE-CANONICAL",
            &program,
            "func f(a.0) {\n"
            "  bb.0:\n"
            "    tmp.0 = add a.0, 1\n"
            "    ret tmp.0\n"
            "}\n")) {
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

static int test_ir_pass_default_pipeline_stepwise_preserves_representative_invariants(void) {
    static const IrPassStepwiseCase cases[] = {
        {
            "IR-PASS-STEPWISE-DEAD-CALL-CFG",
            "int g(){return 1;} int f(int a){if(a) g()+0; else 0; return 0;}\n",
            setup_branch_condition_true_fixed_point_case,
            check_stepwise_call_count_stays_one,
        },
        {
            "IR-PASS-STEPWISE-MULTI-DEF-CONSTANT",
            "int f(int a){if(a) 1; else 2; return 0;}\n",
            setup_multi_def_constant_path_case,
            check_stepwise_branch_distinction_remains,
        },
        {
            "IR-PASS-STEPWISE-MULTI-DEF-COPY",
            "int f(int a, int b){if(a) 1; else 2; return 0;}\n",
            setup_multi_def_copy_path_case,
            check_stepwise_branch_distinction_remains,
        },
    };
    size_t case_index;

    for (case_index = 0; case_index < sizeof(cases) / sizeof(cases[0]); ++case_index) {
        if (!expect_default_pipeline_stepwise_case(&cases[case_index])) {
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

static int ir_pass_test_breaks_entry_terminator(IrProgram *program, IrError *error) {
    IrFunction *function;

    (void)error;

    if (!program || program->function_count == 0) {
        return 0;
    }

    function = &program->functions[0];
    if (!function->has_body || !function->blocks || function->block_count == 0) {
        return 0;
    }

    function->blocks[0].has_terminator = 0;
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

static int test_ir_pass_rejects_verifier_invalid_pipeline_input(void) {
    IrProgram program;
    IrError error;

    if (!lower_source_to_ir_program("int f(){return 0;}\n", &program)) {
        return 0;
    }

    program.functions[0].blocks[0].has_terminator = 0;

    if (ir_pass_run_pipeline(&program, NULL, 0, &error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: verifier-invalid pipeline input unexpectedly passed\n");
        ir_program_free(&program);
        return 0;
    }

    if (!expect_error_fragment("IR-PASS-VERIFY-INPUT", &error, "IR-PASS-034")
        || !expect_error_fragment("IR-PASS-VERIFY-INPUT", &error, "IR-VERIFY-005")) {
        ir_program_free(&program);
        return 0;
    }

    ir_program_free(&program);
    return 1;
}

static int test_ir_pass_rejects_verifier_invalid_pass_output(void) {
    IrProgram program;
    IrPassSpec passes[1];
    IrError error;

    if (!lower_source_to_ir_program("int f(){return 0;}\n", &program)) {
        return 0;
    }

    passes[0].name = "break-entry-terminator";
    passes[0].run = ir_pass_test_breaks_entry_terminator;

    if (ir_pass_run_pipeline(&program, passes, 1, &error)) {
        fprintf(stderr,
            "[ir-pass] FAIL: verifier-invalid pass output unexpectedly passed\n");
        ir_program_free(&program);
        return 0;
    }

    if (!expect_error_fragment("IR-PASS-VERIFY-OUTPUT", &error, "IR-PASS-035")
        || !expect_error_fragment("IR-PASS-VERIFY-OUTPUT", &error, "IR-VERIFY-005")) {
        ir_program_free(&program);
        return 0;
    }

    ir_program_free(&program);
    return 1;
}
