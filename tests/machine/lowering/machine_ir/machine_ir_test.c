#include "machine/ir.h"

#include "ir.h"
#include "lexer.h"
#include "lower_ir.h"
#include "parser.h"
#include "semantic.h"
#include "value_ssa_alloc.h"
#include "value_ssa.h"
#include "value_ssa_pass.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dup_text(const char *text) {
    size_t length;
    char *copy;

    if (!text) {
        return NULL;
    }

    length = strlen(text);
    copy = (char *)malloc(length + 1);
    if (!copy) {
        return NULL;
    }
    memcpy(copy, text, length + 1);
    return copy;
}

static void machine_ir_test_set_build_error(ValueSsaError *error,
    const ParserError *parser_error,
    const SemanticError *semantic_error,
    const IrError *ir_error,
    const LowerIrError *lower_error,
    const char *fallback_message) {
    if (!error || error->message[0] != '\0') {
        return;
    }

    if (lower_error && lower_error->message[0] != '\0') {
        error->line = lower_error->line;
        error->column = lower_error->column;
        snprintf(error->message, sizeof(error->message), "%s", lower_error->message);
        return;
    }
    if (ir_error && ir_error->message[0] != '\0') {
        error->line = ir_error->line;
        error->column = ir_error->column;
        snprintf(error->message, sizeof(error->message), "%s", ir_error->message);
        return;
    }
    if (semantic_error && semantic_error->message[0] != '\0') {
        error->line = semantic_error->line;
        error->column = semantic_error->column;
        snprintf(error->message, sizeof(error->message), "%s", semantic_error->message);
        return;
    }
    if (parser_error && parser_error->message[0] != '\0') {
        error->line = parser_error->line;
        error->column = parser_error->column;
        snprintf(error->message, sizeof(error->message), "%s", parser_error->message);
        return;
    }
    snprintf(error->message,
        sizeof(error->message),
        "%s",
        fallback_message ? fallback_message : "machine-ir source builder failed");
}

static int build_value_ssa_program_from_source_text(const char *source,
    ValueSsaProgram *program,
    ValueSsaError *error) {
    TokenArray tokens;
    AstProgram ast_program;
    ParserError parser_error;
    SemanticError semantic_error;
    SemanticOptions semantic_options;
    IrProgram ir_program;
    IrError ir_error;
    IrLowerOptions ir_options;
    LowerIrProgram lower_program;
    LowerIrError lower_error;
    LowerIrOptions lower_options;
    int ok = 0;

    if (!source || !program) {
        if (error) {
            error->line = 0;
            error->column = 0;
            snprintf(error->message, sizeof(error->message), "machine-ir source builder received invalid input");
        }
        return 0;
    }

    lexer_init_tokens(&tokens);
    ast_program_init(&ast_program);
    ir_program_init(&ir_program);
    lower_ir_program_init(&lower_program);
    value_ssa_program_init(program);
    memset(&parser_error, 0, sizeof(parser_error));
    memset(&semantic_error, 0, sizeof(semantic_error));
    memset(&semantic_options, 0, sizeof(semantic_options));
    memset(&ir_error, 0, sizeof(ir_error));
    memset(&ir_options, 0, sizeof(ir_options));
    memset(&lower_error, 0, sizeof(lower_error));
    memset(&lower_options, 0, sizeof(lower_options));

    semantic_options.skip_all_paths_return_check = 1;
    ir_options.allow_implicit_fallthrough_return = 1;
    lower_options.allow_implicit_fallthrough_return = 1;

    if (!lexer_tokenize(source, &tokens) ||
        !parser_parse_translation_unit_ast(&tokens, &ast_program, &parser_error) ||
        !semantic_analyze_program_with_options(&ast_program, &semantic_options, &semantic_error) ||
        !ir_lower_program(&ast_program, &ir_options, &ir_program, &ir_error) ||
        !lower_ir_lower_from_ir(&ir_program, &lower_options, &lower_program, &lower_error) ||
        !value_ssa_build_default_from_lower_ir(&lower_program, program, error) ||
        !value_ssa_optimize_perf_hotspots(program, error)) {
        machine_ir_test_set_build_error(error,
            &parser_error,
            &semantic_error,
            &ir_error,
            &lower_error,
            "machine-ir source builder failed");
        goto cleanup;
    }

    ok = 1;

cleanup:
    if (!ok) {
        value_ssa_program_free(program);
    }
    lower_ir_program_free(&lower_program);
    ir_program_free(&ir_program);
    ast_program_free(&ast_program);
    lexer_free_tokens(&tokens);
    return ok;
}

static int build_value_ssa_program_from_extension_source_text(const char *source,
    ValueSsaProgram *program,
    ValueSsaError *error) {
    TokenArray tokens;
    AstProgram ast_program;
    ParserError parser_error;
    SemanticError semantic_error;
    SemanticOptions semantic_options;
    IrProgram ir_program;
    IrError ir_error;
    IrLowerOptions ir_options;
    LowerIrProgram lower_program;
    LowerIrError lower_error;
    LowerIrOptions lower_options;
    int ok = 0;

    if (!source || !program) {
        if (error) {
            error->line = 0;
            error->column = 0;
            snprintf(error->message, sizeof(error->message), "machine-ir source builder received invalid input");
        }
        return 0;
    }

    lexer_init_tokens(&tokens);
    ast_program_init(&ast_program);
    ir_program_init(&ir_program);
    lower_ir_program_init(&lower_program);
    value_ssa_program_init(program);
    memset(&parser_error, 0, sizeof(parser_error));
    memset(&semantic_error, 0, sizeof(semantic_error));
    memset(&semantic_options, 0, sizeof(semantic_options));
    memset(&ir_error, 0, sizeof(ir_error));
    memset(&ir_options, 0, sizeof(ir_options));
    memset(&lower_error, 0, sizeof(lower_error));
    memset(&lower_options, 0, sizeof(lower_options));

    semantic_options.allow_extension_features = 1;
    semantic_options.skip_all_paths_return_check = 1;
    ir_options.allow_implicit_fallthrough_return = 1;
    lower_options.allow_implicit_fallthrough_return = 1;

    if (!lexer_tokenize(source, &tokens) ||
        !parser_parse_translation_unit_ast(&tokens, &ast_program, &parser_error) ||
        !semantic_analyze_program_with_options(&ast_program, &semantic_options, &semantic_error) ||
        !ir_lower_program(&ast_program, &ir_options, &ir_program, &ir_error) ||
        !lower_ir_lower_from_ir(&ir_program, &lower_options, &lower_program, &lower_error) ||
        !value_ssa_build_translation_only_from_lower_ir(&lower_program, program, error)) {
        machine_ir_test_set_build_error(error,
            &parser_error,
            &semantic_error,
            &ir_error,
            &lower_error,
            "machine-ir source builder failed");
        goto cleanup;
    }

    ok = 1;

cleanup:
    if (!ok) {
        value_ssa_program_free(program);
    }
    lower_ir_program_free(&lower_program);
    ir_program_free(&ir_program);
    ast_program_free(&ast_program);
    lexer_free_tokens(&tokens);
    return ok;
}

static int build_default_value_ssa_program_from_extension_source_text(const char *source,
    ValueSsaProgram *program,
    ValueSsaError *error) {
    TokenArray tokens;
    AstProgram ast_program;
    ParserError parser_error;
    SemanticError semantic_error;
    SemanticOptions semantic_options;
    IrProgram ir_program;
    IrError ir_error;
    IrLowerOptions ir_options;
    LowerIrProgram lower_program;
    LowerIrError lower_error;
    LowerIrOptions lower_options;
    int ok = 0;

    if (!source || !program) {
        if (error) {
            error->line = 0;
            error->column = 0;
            snprintf(error->message, sizeof(error->message), "machine-ir default source builder received invalid input");
        }
        return 0;
    }

    lexer_init_tokens(&tokens);
    ast_program_init(&ast_program);
    ir_program_init(&ir_program);
    lower_ir_program_init(&lower_program);
    value_ssa_program_init(program);
    memset(&parser_error, 0, sizeof(parser_error));
    memset(&semantic_error, 0, sizeof(semantic_error));
    memset(&semantic_options, 0, sizeof(semantic_options));
    memset(&ir_error, 0, sizeof(ir_error));
    memset(&ir_options, 0, sizeof(ir_options));
    memset(&lower_error, 0, sizeof(lower_error));
    memset(&lower_options, 0, sizeof(lower_options));

    semantic_options.allow_extension_features = 1;
    semantic_options.skip_all_paths_return_check = 1;
    ir_options.allow_implicit_fallthrough_return = 1;
    lower_options.allow_implicit_fallthrough_return = 1;

    if (!lexer_tokenize(source, &tokens) ||
        !parser_parse_translation_unit_ast(&tokens, &ast_program, &parser_error) ||
        !semantic_analyze_program_with_options(&ast_program, &semantic_options, &semantic_error) ||
        !ir_lower_program(&ast_program, &ir_options, &ir_program, &ir_error) ||
        !lower_ir_lower_from_ir(&ir_program, &lower_options, &lower_program, &lower_error) ||
        !value_ssa_build_default_from_lower_ir(&lower_program, program, error)) {
        machine_ir_test_set_build_error(error,
            &parser_error,
            &semantic_error,
            &ir_error,
            &lower_error,
            "machine-ir default source builder failed");
        goto cleanup;
    }

    ok = 1;

cleanup:
    if (!ok) {
        value_ssa_program_free(program);
    }
    lower_ir_program_free(&lower_program);
    ir_program_free(&ir_program);
    ast_program_free(&ast_program);
    lexer_free_tokens(&tokens);
    return ok;
}

static int build_phi_spill_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaPhiInput phi_inputs[2];
    size_t cond_id;
    size_t then_value_id;
    size_t else_value_id;
    size_t join_value_id;
    size_t sum_id;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 1, NULL, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &then_block, error) ||
        !value_ssa_function_append_block(function, NULL, &else_block, error) ||
        !value_ssa_function_append_block(function, NULL, &join_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    cond_id = value_ssa_function_allocate_value(function);
    then_value_id = value_ssa_function_allocate_value(function);
    else_value_id = value_ssa_function_allocate_value(function);
    join_value_id = value_ssa_function_allocate_value(function);
    sum_id = value_ssa_function_allocate_value(function);
    if (cond_id == (size_t)-1 || then_value_id == (size_t)-1 || else_value_id == (size_t)-1 ||
        join_value_id == (size_t)-1 || sum_id == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_id);
    instruction.as.mov_value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_id(cond_id), 1, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(then_value_id);
    instruction.as.mov_value = value_ssa_value_immediate(10);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error) ||
        !value_ssa_block_set_jump(then_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(else_value_id);
    instruction.as.mov_value = value_ssa_value_immediate(20);
    if (!value_ssa_block_append_instruction(else_block, &instruction, error) ||
        !value_ssa_block_set_jump(else_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    phi_inputs[0].predecessor_block_id = 1;
    phi_inputs[0].value = value_ssa_value_id(then_value_id);
    phi_inputs[1].predecessor_block_id = 2;
    phi_inputs[1].value = value_ssa_value_id(else_value_id);
    if (!value_ssa_block_append_phi(join_block, join_value_id, phi_inputs, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(sum_id);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(join_value_id);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_id(sum_id), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_call_memory_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *callee = NULL;
    ValueSsaFunction *main_function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaValueRef call_args[1];
    size_t local_a_id;
    size_t load_a_id;
    size_t call_result_id;
    size_t load_g_id;
    size_t sum_id;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_global(program, "g", NULL, error) ||
        !value_ssa_program_append_function(program, "callee", 1, &callee, error) ||
        !value_ssa_function_append_local(callee, "p", 1, NULL, error) ||
        !value_ssa_function_append_block(callee, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    if (!value_ssa_block_set_return(block, value_ssa_value_immediate(7), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    block = NULL;
    if (!value_ssa_program_append_function(program, "main", 1, &main_function, error) ||
        !value_ssa_function_append_local(main_function, "a", 1, &local_a_id, error) ||
        !value_ssa_function_append_block(main_function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    load_a_id = value_ssa_function_allocate_value(main_function);
    call_result_id = value_ssa_function_allocate_value(main_function);
    load_g_id = value_ssa_function_allocate_value(main_function);
    sum_id = value_ssa_function_allocate_value(main_function);
    if (load_a_id == (size_t)-1 || call_result_id == (size_t)-1 ||
        load_g_id == (size_t)-1 || sum_id == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(load_a_id);
    instruction.as.load_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(call_result_id);
    instruction.as.call.callee_name = "callee";
    call_args[0] = value_ssa_value_id(load_a_id);
    instruction.as.call.args = call_args;
    instruction.as.call.arg_count = 1;
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.has_result = 0;
    instruction.as.store.slot = value_ssa_slot_global(0);
    instruction.as.store.value = value_ssa_value_id(call_result_id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(load_g_id);
    instruction.as.load_slot = value_ssa_slot_global(0);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(sum_id);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(load_g_id);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(sum_id), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int expect_dump(const MachineIrProgram *program, const char *expected_text) {
    char *actual_text = NULL;
    MachineIrError error;
    int ok = 1;

    if (!machine_ir_dump_program(program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: dump failed: %s\n", error.message);
        return 0;
    }
    if (strcmp(actual_text, expected_text) != 0) {
        fprintf(stderr,
            "[machine-ir] FAIL: dump mismatch\nexpected:\n%s\nactual:\n%s\n",
            expected_text,
            actual_text);
        ok = 0;
    }

    free(actual_text);
    return ok;
}

static int test_machine_ir_manual_verify_accepts_valid_program(void) {
    MachineIrProgram program;
    MachineIrFunction *function = NULL;
    MachineIrBasicBlock *block = NULL;
    MachineIrInstruction instruction;
    MachineIrError error;
    int ok;

    machine_ir_program_init(&program);
    if (!machine_ir_program_append_global(&program, "g", NULL, &error) ||
        !machine_ir_program_append_function(&program, "main", 1, &function, &error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &error) ||
        !machine_ir_function_append_block(function, NULL, &block, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: manual setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }

    function->spill_slot_count = 1;
    program.register_bank.register_count = 2;
    program.register_bank.registers = (MachineIrRegisterDesc *)calloc(2, sizeof(MachineIrRegisterDesc));
    if (!program.register_bank.registers) {
        fprintf(stderr, "[machine-ir] FAIL: out of memory allocating manual register bank\n");
        machine_ir_program_free(&program);
        return 0;
    }
    program.register_bank.registers[0].register_id = 0;
    program.register_bank.registers[0].name = dup_text("r0");
    program.register_bank.registers[0].allocatable = 1u;
    program.register_bank.registers[0].caller_clobbered = 1u;
    program.register_bank.registers[1].register_id = 1;
    program.register_bank.registers[1].name = dup_text("r1");
    program.register_bank.registers[1].allocatable = 1u;
    program.register_bank.registers[1].callee_preserved = 1u;

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_immediate(7);
    if (!machine_ir_block_append_instruction(block, &instruction, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: manual append failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_GLOBAL;
    instruction.has_result = 0;
    instruction.as.store.slot = machine_ir_slot_global(0);
    instruction.as.store.value = machine_ir_operand_register(0);
    if (!machine_ir_block_append_instruction(block, &instruction, &error) ||
        !machine_ir_block_set_return(block, machine_ir_operand_register(0), &error)) {
        fprintf(stderr, "[machine-ir] FAIL: manual terminator setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }

    ok = machine_ir_verify_program(&program, &error);
    if (!ok) {
        fprintf(stderr, "[machine-ir] FAIL: valid manual program rejected: %s\n", error.message);
    }

    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_manual_verify_rejects_bad_spill_slot(void) {
    MachineIrProgram program;
    MachineIrFunction *function = NULL;
    MachineIrBasicBlock *block = NULL;
    MachineIrError error;

    machine_ir_program_init(&program);
    if (!machine_ir_program_append_function(&program, "main", 1, &function, &error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, &block, &error) ||
        !machine_ir_block_set_return(block, machine_ir_operand_spill_slot(0), &error)) {
        fprintf(stderr, "[machine-ir] FAIL: invalid-setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }

    if (machine_ir_verify_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: verifier accepted invalid spill-slot return\n");
        machine_ir_program_free(&program);
        return 0;
    }
    if (!strstr(error.message, "spill slot")) {
        fprintf(stderr,
            "[machine-ir] FAIL: unexpected verifier message for invalid spill-slot return: %s\n",
            error.message);
        machine_ir_program_free(&program);
        return 0;
    }

    machine_ir_program_free(&program);
    return 1;
}

static int test_machine_ir_query_surface(void) {
    MachineIrProgram program;
    MachineIrFunction *function = NULL;
    MachineIrBasicBlock *block = NULL;
    MachineIrError error;
    const MachineIrRegisterDesc *reg = NULL;
    const MachineIrFunction *function_view = NULL;
    const MachineIrBasicBlock *block_view = NULL;
    const char *function_name = NULL;
    size_t register_count = 0;
    size_t allocatable_count = 0;
    size_t caller_clobbered_count = 0;
    size_t callee_preserved_count = 0;
    size_t global_count = 0;
    size_t function_count = 0;
    size_t function_index = (size_t)-1;
    size_t parameter_count = 0;
    size_t local_count = 0;
    size_t block_count = 0;
    size_t spill_slot_count = 0;
    size_t block_id = (size_t)-1;
    size_t phi_count = 0;
    size_t instruction_count = 0;
    int has_body = 0;
    int has_terminator = 0;
    MachineIrTerminatorKind terminator_kind = MACHINE_IR_TERM_JUMP;

    machine_ir_program_init(&program);
    if (!machine_ir_program_append_function(&program, "main", 1, &function, &error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &error) ||
        !machine_ir_function_append_block(function, NULL, &block, &error) ||
        !machine_ir_block_set_return(block, machine_ir_operand_immediate(0), &error)) {
        fprintf(stderr, "[machine-ir] FAIL: query-surface setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }

    function->spill_slot_count = 2;
    program.register_bank.register_count = 2;
    program.register_bank.registers = (MachineIrRegisterDesc *)calloc(2, sizeof(MachineIrRegisterDesc));
    if (!program.register_bank.registers) {
        fprintf(stderr, "[machine-ir] FAIL: query-surface register allocation failed\n");
        machine_ir_program_free(&program);
        return 0;
    }
    program.register_bank.registers[0].register_id = 0;
    program.register_bank.registers[0].name = dup_text("r0");
    program.register_bank.registers[0].allocatable = 1u;
    program.register_bank.registers[0].caller_clobbered = 1u;
    program.register_bank.registers[1].register_id = 1;
    program.register_bank.registers[1].name = dup_text("r1");
    program.register_bank.registers[1].allocatable = 1u;
    program.register_bank.registers[1].callee_preserved = 1u;

    if (!machine_ir_register_bank_get_summary(&program.register_bank,
            &register_count,
            &allocatable_count,
            &caller_clobbered_count,
            &callee_preserved_count) ||
        register_count != 2 || allocatable_count != 2 || caller_clobbered_count != 1 ||
        callee_preserved_count != 1) {
        fprintf(stderr, "[machine-ir] FAIL: register-bank summary query mismatch\n");
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_register_bank_find_register_by_name(&program.register_bank, "r1", &function_index, &reg) ||
        function_index != 1 || !reg || reg->register_id != 1 || !reg->callee_preserved) {
        fprintf(stderr, "[machine-ir] FAIL: register-bank name lookup mismatch\n");
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_program_get_summary(&program, &register_count, &global_count, &function_count) ||
        register_count != 2 || global_count != 0 || function_count != 1) {
        fprintf(stderr, "[machine-ir] FAIL: program summary query mismatch\n");
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_program_get_function_by_name(&program, "main", &function_index, &function_view) ||
        function_index != 0 || !function_view) {
        fprintf(stderr, "[machine-ir] FAIL: program function lookup mismatch\n");
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_function_get_summary(function_view,
            &function_name,
            &has_body,
            &parameter_count,
            &local_count,
            &block_count,
            &spill_slot_count) ||
        !function_name || strcmp(function_name, "main") != 0 || !has_body || parameter_count != 1 ||
        local_count != 1 || block_count != 1 || spill_slot_count != 2) {
        fprintf(stderr, "[machine-ir] FAIL: function summary query mismatch\n");
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_function_get_block(function_view, 0, &block_view) || !block_view) {
        fprintf(stderr, "[machine-ir] FAIL: block lookup mismatch\n");
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_basic_block_get_summary(block_view,
            &block_id,
            &phi_count,
            &instruction_count,
            &has_terminator,
            &terminator_kind) ||
        block_id != 0 || phi_count != 0 || instruction_count != 0 || !has_terminator ||
        terminator_kind != MACHINE_IR_TERM_RETURN) {
        fprintf(stderr, "[machine-ir] FAIL: block summary query mismatch\n");
        machine_ir_program_free(&program);
        return 0;
    }

    machine_ir_program_free(&program);
    return 1;
}

static int test_machine_ir_lower_from_value_ssa_dump(void) {
    ValueSsaProgram program;
    ValueSsaMachineRegisterBank bank;
    MachineIrProgram machine_program;
    ValueSsaError value_error;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_machine_register_bank_init(&bank);
    machine_ir_program_init(&machine_program);

    if (!build_phi_spill_program(&program, &value_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: value-ssa setup failed: %s\n",
            value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!value_ssa_build_flat_machine_register_bank(2, &bank, &value_error) ||
        !machine_ir_allocate_lower_program_from_value_ssa(&program, 2, &bank, &machine_program, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: machine lowering failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!machine_ir_dump_program(&machine_program, &actual_text, &machine_error)) {
        fprintf(stderr, "[machine-ir] FAIL: allocate+lower dump failed: %s\n", machine_error.message);
        ok = 0;
        goto cleanup;
    }
    if (!strstr(actual_text, "machine_ir\nregister-bank:\n") ||
        !strstr(actual_text, "function main params=1 locals=1 spills=") ||
        !strstr(actual_text, "phi ") ||
        !strstr(actual_text, "br reg.") ||
        !strstr(actual_text, "ret reg.") ||
        strstr(actual_text, "ssa.")) {
        fprintf(stderr,
            "[machine-ir] FAIL: allocate+lower smoke dump missing expected machine-ir shape\nactual:\n%s\n",
            actual_text);
        ok = 0;
        goto cleanup;
    }

    if (!machine_ir_verify_program(&machine_program, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: lowered machine program rejected: %s\n",
            machine_error.message);
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_program_free(&machine_program);
    value_ssa_machine_register_bank_free(&bank);
    value_ssa_program_free(&program);
    return ok;
}

static int prepare_manual_phi_spill_result(ValueSsaProgramAllocationResult *result,
    const ValueSsaProgram *program) {
    ValueSsaAllocationResult *function_result;
    size_t value_count;
    size_t value_index;

    if (!result || !program || program->function_count != 1) {
        return 0;
    }

    value_ssa_program_allocation_result_free(result);
    result->function_count = 1;
    result->function_results = (ValueSsaAllocationResult *)calloc(1, sizeof(ValueSsaAllocationResult));
    if (!result->function_results) {
        return 0;
    }

    function_result = &result->function_results[0];
    value_ssa_allocation_result_init(function_result);
    value_count = program->functions[0].next_value_id;
    function_result->value_count = value_count;
    function_result->color_budget = 2;
    function_result->spill_slot_count = 1;
    function_result->colors = (size_t *)malloc(value_count * sizeof(size_t));
    function_result->has_color = (unsigned char *)calloc(value_count, sizeof(unsigned char));
    function_result->spill_flags = (unsigned char *)calloc(value_count, sizeof(unsigned char));
    function_result->spill_intended_flags = (unsigned char *)calloc(value_count, sizeof(unsigned char));
    function_result->spill_confirmed_flags = (unsigned char *)calloc(value_count, sizeof(unsigned char));
    function_result->spill_recovered_flags = (unsigned char *)calloc(value_count, sizeof(unsigned char));
    function_result->spill_recovery_orders = (size_t *)malloc(value_count * sizeof(size_t));
    function_result->spill_recovery_phase_ids = (size_t *)malloc(value_count * sizeof(size_t));
    function_result->spill_recovery_phase_member_orders = (size_t *)malloc(value_count * sizeof(size_t));
    function_result->spill_recovery_phase_member_counts = (size_t *)malloc(value_count * sizeof(size_t));
    function_result->spill_recovery_phase_entries =
        (ValueSsaAllocatorRetryFamilyAgendaItem *)calloc(value_count, sizeof(ValueSsaAllocatorRetryFamilyAgendaItem));
    function_result->spill_priorities = (size_t *)calloc(value_count, sizeof(size_t));
    function_result->spill_slots = (size_t *)malloc(value_count * sizeof(size_t));
    if (!function_result->colors || !function_result->has_color || !function_result->spill_flags ||
        !function_result->spill_intended_flags || !function_result->spill_confirmed_flags ||
        !function_result->spill_recovered_flags || !function_result->spill_recovery_orders ||
        !function_result->spill_recovery_phase_ids || !function_result->spill_recovery_phase_member_orders ||
        !function_result->spill_recovery_phase_member_counts || !function_result->spill_recovery_phase_entries ||
        !function_result->spill_priorities || !function_result->spill_slots) {
        value_ssa_program_allocation_result_free(result);
        return 0;
    }

    for (value_index = 0; value_index < value_count; ++value_index) {
        function_result->colors[value_index] = (size_t)-1;
        function_result->spill_recovery_orders[value_index] = (size_t)-1;
        function_result->spill_recovery_phase_ids[value_index] = (size_t)-1;
        function_result->spill_recovery_phase_member_orders[value_index] = (size_t)-1;
        function_result->spill_recovery_phase_member_counts[value_index] = (size_t)-1;
        function_result->spill_slots[value_index] = (size_t)-1;
    }

    function_result->has_color[0] = 1;
    function_result->colors[0] = 0;
    function_result->has_color[1] = 1;
    function_result->colors[1] = 1;
    function_result->spill_flags[2] = 1;
    function_result->spill_confirmed_flags[2] = 1;
    function_result->spill_slots[2] = 0;
    function_result->has_color[3] = 1;
    function_result->colors[3] = 0;
    function_result->has_color[4] = 1;
    function_result->colors[4] = 1;
    return 1;
}

static int test_machine_ir_lower_from_manual_machine_view_dump(void) {
    ValueSsaProgram program;
    ValueSsaProgramAllocationResult result;
    ValueSsaProgramAllocationLayout layout;
    ValueSsaMachineRegisterBank bank;
    ValueSsaProgramMachineAllocationView machine_view;
    ValueSsaError value_error;
    MachineIrProgram machine_program;
    MachineIrError machine_error;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_program_allocation_result_init(&result);
    value_ssa_program_allocation_layout_init(&layout);
    value_ssa_machine_register_bank_init(&bank);
    value_ssa_program_machine_allocation_view_init(&machine_view);
    machine_ir_program_init(&machine_program);

    if (!build_phi_spill_program(&program, &value_error) ||
        !prepare_manual_phi_spill_result(&result, &program) ||
        !value_ssa_build_program_allocation_layout(&result, &layout, &value_error) ||
        !value_ssa_program_allocation_layout_attach_program_context(&layout, &program, &value_error) ||
        !value_ssa_build_flat_machine_register_bank(2, &bank, &value_error) ||
        !value_ssa_build_program_machine_allocation_view(&layout, &bank, &machine_view, &value_error) ||
        !machine_ir_lower_program_from_value_ssa(&program, &machine_view, &bank, &machine_program, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: manual machine-view lowering failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    ok = expect_dump(&machine_program,
        "machine_ir\n"
        "register-bank:\n"
        "  reg.0 name=r0 alloc=1 caller-clobbered=1 callee-preserved=0\n"
        "  reg.1 name=r1 alloc=1 caller-clobbered=1 callee-preserved=0\n"
        "globals:\n"
        "function main params=1 locals=1 spills=1\n"
        "  bb.0:\n"
        "    reg.0(r0) = mov 1\n"
        "    br reg.0(r0), bb.1, bb.2\n"
        "  bb.1:\n"
        "    reg.1(r1) = mov 10\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    spill.0 = mov 20\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    phi reg.0(r0) = [bb.1: reg.1(r1)], [bb.2: spill.0]\n"
        "    reg.1(r1) = add reg.0(r0), 1\n"
        "    ret reg.1(r1)\n");

cleanup:
    machine_ir_program_free(&machine_program);
    value_ssa_program_machine_allocation_view_free(&machine_view);
    value_ssa_machine_register_bank_free(&bank);
    value_ssa_program_allocation_layout_free(&layout);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_direct_dump_entrypoints(void) {
    ValueSsaProgram program;
    ValueSsaProgramAllocationResult result;
    ValueSsaProgramAllocationLayout layout;
    ValueSsaProgramMachineAllocationView machine_view;
    ValueSsaMachineRegisterBank bank;
    ValueSsaError value_error;
    MachineIrError machine_error;
    char *direct_text = NULL;
    char *flat_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_program_allocation_result_init(&result);
    value_ssa_program_allocation_layout_init(&layout);
    value_ssa_program_machine_allocation_view_init(&machine_view);
    value_ssa_machine_register_bank_init(&bank);

    if (!build_phi_spill_program(&program, &value_error) ||
        !prepare_manual_phi_spill_result(&result, &program) ||
        !value_ssa_build_program_allocation_layout(&result, &layout, &value_error) ||
        !value_ssa_program_allocation_layout_attach_program_context(&layout, &program, &value_error) ||
        !value_ssa_build_flat_machine_register_bank(2, &bank, &value_error) ||
        !value_ssa_build_program_machine_allocation_view(&layout, &bank, &machine_view, &value_error) ||
        !machine_ir_lower_program_from_value_ssa_dump(&program, &machine_view, &bank, &direct_text, &machine_error) ||
        !machine_ir_allocate_lower_program_from_value_ssa_flat_dump(&program, 2, 2, &flat_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: direct-dump entrypoint setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(direct_text, "spill.0 = mov 20") ||
        !strstr(direct_text, "phi reg.0(r0) = [bb.1: reg.1(r1)], [bb.2: spill.0]")) {
        fprintf(stderr,
            "[machine-ir] FAIL: direct dump entrypoint lost manual machine-view shape\nactual:\n%s\n",
            direct_text);
        ok = 0;
        goto cleanup;
    }
    if (!strstr(flat_text, "machine_ir\nregister-bank:\n") ||
        !strstr(flat_text, "function main params=1 locals=1 spills=") ||
        strstr(flat_text, "ssa.")) {
        fprintf(stderr,
            "[machine-ir] FAIL: flat direct dump entrypoint did not produce machine-ir text\nactual:\n%s\n",
            flat_text);
        ok = 0;
    }

cleanup:
    free(flat_text);
    free(direct_text);
    value_ssa_machine_register_bank_free(&bank);
    value_ssa_program_machine_allocation_view_free(&machine_view);
    value_ssa_program_allocation_layout_free(&layout);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_allocate_and_rewrite_entrypoints(void) {
    ValueSsaProgram program;
    ValueSsaError value_error;
    ValueSsaMachineRegisterBank bank;
    MachineIrProgram machine_program;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_machine_register_bank_init(&bank);
    machine_ir_program_init(&machine_program);

    if (!build_phi_spill_program(&program, &value_error) ||
        !value_ssa_build_flat_machine_register_bank(2, &bank, &value_error) ||
        !machine_ir_allocate_and_rewrite_program_single_block_spills(
            &program, 2, &bank, &machine_program, &machine_error) ||
        !machine_ir_dump_program(&machine_program, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: allocate+rewrite machine-ir entrypoint failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "machine_ir\nregister-bank:\n") ||
        !strstr(actual_text, "function main params=1 locals=") ||
        !strstr(actual_text, "ret reg.") ||
        strstr(actual_text, "ssa.")) {
        fprintf(stderr,
            "[machine-ir] FAIL: allocate+rewrite machine-ir dump missing expected shape\nactual:\n%s\n",
            actual_text);
        ok = 0;
        goto cleanup;
    }

    free(actual_text);
    actual_text = NULL;
    if (!machine_ir_allocate_and_rewrite_program_single_block_spills_flat_dump(
            &program, 2, 2, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: flat allocate+rewrite machine-ir dump failed: %s\n",
            machine_error.message);
        ok = 0;
        goto cleanup;
    }
    if (!strstr(actual_text, "machine_ir\nregister-bank:\n") ||
        !strstr(actual_text, "function main params=1 locals=") ||
        strstr(actual_text, "ssa.")) {
        fprintf(stderr,
            "[machine-ir] FAIL: flat allocate+rewrite machine-ir dump missing expected shape\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_program_free(&machine_program);
    value_ssa_machine_register_bank_free(&bank);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_allocate_and_rewrite_report_surface(void) {
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    const MachineIrProgram *report_program = NULL;
    const MachineIrFunction *report_function = NULL;
    const size_t *phi_functions = NULL;
    const size_t *call_functions = NULL;
    const size_t *spill_functions = NULL;
    const size_t *register_only_functions = NULL;
    MachineIrError machine_error;
    char *actual_text = NULL;
    size_t allocation_rounds = 0;
    size_t rewrite_rounds = 0;
    size_t register_count = 0;
    size_t global_count = 0;
    size_t function_count = 0;
    size_t function_index = (size_t)-1;
    size_t phi_function_count = 0;
    size_t call_function_count = 0;
    size_t spill_function_count = 0;
    size_t register_only_function_count = 0;
    size_t phi_count = 0;
    size_t call_count = 0;
    size_t spill_slot_count = 0;
    size_t load_local_count = 0;
    size_t store_local_count = 0;
    size_t load_global_count = 0;
    size_t store_global_count = 0;
    size_t memory_op_count = 0;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);

    if (!build_phi_spill_program(&program, &value_error) ||
        !machine_ir_build_allocate_and_rewrite_program_single_block_spills_flat_report(
            &program, 2, 2, &report, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: allocate+rewrite report build failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!machine_ir_allocate_rewrite_report_get_summary(&report,
            &allocation_rounds,
            &rewrite_rounds,
            &register_count,
            &global_count,
            &function_count) ||
        allocation_rounds == 0 || register_count != 2 || global_count != 0 || function_count != 1) {
        fprintf(stderr, "[machine-ir] FAIL: allocate+rewrite report summary mismatch\n");
        ok = 0;
        goto cleanup;
    }
    if (!machine_ir_allocate_rewrite_report_get_program(&report, &report_program) || !report_program) {
        fprintf(stderr, "[machine-ir] FAIL: allocate+rewrite report program query failed\n");
        ok = 0;
        goto cleanup;
    }
    if (!machine_ir_allocate_rewrite_report_get_functions_with_phi(&report, &phi_function_count, &phi_functions) ||
        !machine_ir_allocate_rewrite_report_get_functions_with_call(&report, &call_function_count, &call_functions) ||
        !machine_ir_allocate_rewrite_report_get_functions_with_spills(&report, &spill_function_count, &spill_functions) ||
        !machine_ir_allocate_rewrite_report_get_functions_register_only(
            &report, &register_only_function_count, &register_only_functions) ||
        phi_function_count != 1 || phi_functions[0] != 0 ||
        call_function_count != 0 ||
        spill_function_count != 0 ||
        register_only_function_count != 1 || register_only_functions[0] != 0) {
        fprintf(stderr, "[machine-ir] FAIL: allocate+rewrite report function-filter queries mismatch\n");
        ok = 0;
        goto cleanup;
    }
    if (!machine_ir_allocate_rewrite_report_get_function_by_name(&report, "main", &function_index, &report_function) ||
        function_index != 0 || !report_function) {
        fprintf(stderr, "[machine-ir] FAIL: allocate+rewrite report function lookup failed\n");
        ok = 0;
        goto cleanup;
    }
    if (!machine_ir_allocate_rewrite_report_get_function_shape_summary(&report,
            0,
            &phi_count,
            &call_count,
            &spill_slot_count,
            &load_local_count,
            &store_local_count,
            &load_global_count,
            &store_global_count,
            &memory_op_count) ||
        phi_count != 1 || call_count != 0 || spill_slot_count != 0 ||
        load_local_count != 0 || store_local_count != 0 ||
        load_global_count != 0 || store_global_count != 0 || memory_op_count != 0) {
        fprintf(stderr, "[machine-ir] FAIL: allocate+rewrite report function-shape summary mismatch\n");
        ok = 0;
        goto cleanup;
    }
    if (!machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: allocate+rewrite report dump failed: %s\n",
            machine_error.message);
        ok = 0;
        goto cleanup;
    }
    if (!strstr(actual_text, "allocate+rewrite machine-ir-report allocation_rounds=") ||
        !strstr(actual_text, "phi_funcs=1 call_funcs=0 spill_funcs=0 memory_funcs=0 register_only_funcs=1") ||
        !strstr(actual_text, "functions-with-phi: 0") ||
        !strstr(actual_text, "functions-with-call:") ||
        !strstr(actual_text, "functions-with-spills:") ||
        !strstr(actual_text, "functions-with-memory-ops:") ||
        !strstr(actual_text, "functions-register-only: 0") ||
        !strstr(actual_text,
            "function-shapes:\n  fn.0 main blocks=4 phi=1 call=0 spills=0 load_local=0 store_local=0 load_global=0 store_global=0 memory_ops=0 blocks_with_phi=1 blocks_with_call=0 blocks_with_memory_ops=0 branch_blocks=1 jump_blocks=2 return_blocks=1\n") ||
        !strstr(actual_text,
            "bb.3 phi=1 instr=1 call=0 load_local=0 store_local=0 load_global=0 store_global=0 memory_ops=0 term_ret=1 term_jmp=0 term_br=0") ||
        !strstr(actual_text, "rewrite_rounds=") ||
        !strstr(actual_text, "machine_ir\nregister-bank:\n") ||
        strstr(actual_text, "ssa.")) {
        fprintf(stderr,
            "[machine-ir] FAIL: allocate+rewrite report dump missing expected shape\nactual:\n%s\n",
            actual_text);
        ok = 0;
        goto cleanup;
    }

    free(actual_text);
    actual_text = NULL;
    if (!machine_ir_build_allocate_and_rewrite_program_single_block_spills_flat_report_dump(
            &program, 2, 2, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: allocate+rewrite direct report dump failed: %s\n",
            machine_error.message);
        ok = 0;
        goto cleanup;
    }
    if (!strstr(actual_text, "allocate+rewrite machine-ir-report allocation_rounds=") ||
        !strstr(actual_text, "machine_ir\nregister-bank:\n")) {
        fprintf(stderr,
            "[machine-ir] FAIL: allocate+rewrite direct report dump missing expected shape\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_allocate_and_rewrite_report_memory_shape_surface(void) {
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    const size_t *call_functions = NULL;
    const size_t *memory_functions = NULL;
    const MachineIrFunctionShapeSummary *shape = NULL;
    const MachineIrBlockShapeSummary *block_shape = NULL;
    MachineIrError machine_error;
    char *actual_text = NULL;
    size_t call_function_count = 0;
    size_t memory_function_count = 0;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);

    if (!build_call_memory_program(&program, &value_error) ||
        !machine_ir_build_allocate_and_rewrite_program_single_block_spills_flat_report(
            &program, 2, 2, &report, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: memory-shape report build failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!machine_ir_allocate_rewrite_report_get_functions_with_call(
            &report, &call_function_count, &call_functions) ||
        call_function_count != 1 || call_functions[0] != 1) {
        fprintf(stderr, "[machine-ir] FAIL: memory-shape call-function filter mismatch\n");
        ok = 0;
        goto cleanup;
    }
    if (!machine_ir_allocate_rewrite_report_get_functions_with_memory_ops(
            &report, &memory_function_count, &memory_functions) ||
        memory_function_count != 1 || memory_functions[0] != 1) {
        fprintf(stderr, "[machine-ir] FAIL: memory-shape memory-function filter mismatch\n");
        ok = 0;
        goto cleanup;
    }
    if (!machine_ir_allocate_rewrite_report_get_function_shape_summary_artifact(&report, 1, &shape) ||
        !shape ||
        shape->block_count != 1 ||
        shape->phi_count != 0 ||
        shape->call_count != 1 ||
        shape->spill_slot_count != 0 ||
        shape->load_local_count != 1 ||
        shape->store_local_count != 0 ||
        shape->load_global_count != 1 ||
        shape->store_global_count != 1 ||
        shape->memory_op_count != 3 ||
        shape->blocks_with_phi_count != 0 ||
        shape->blocks_with_call_count != 1 ||
        shape->blocks_with_memory_ops_count != 1 ||
        shape->return_block_count != 1) {
        fprintf(stderr, "[machine-ir] FAIL: memory-shape function summary mismatch\n");
        ok = 0;
        goto cleanup;
    }
    if (!machine_ir_allocate_rewrite_report_get_block_shape_summary(&report, 1, 0, &block_shape) ||
        !block_shape ||
        block_shape->block_id != 0 ||
        block_shape->phi_count != 0 ||
        block_shape->instruction_count != 5 ||
        block_shape->call_count != 1 ||
        block_shape->load_local_count != 1 ||
        block_shape->store_local_count != 0 ||
        block_shape->load_global_count != 1 ||
        block_shape->store_global_count != 1 ||
        block_shape->memory_op_count != 3 ||
        !block_shape->has_return_terminator ||
        block_shape->has_jump_terminator ||
        block_shape->has_branch_terminator) {
        fprintf(stderr, "[machine-ir] FAIL: memory-shape block summary mismatch\n");
        ok = 0;
        goto cleanup;
    }
    if (!machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: memory-shape report dump failed: %s\n",
            machine_error.message);
        ok = 0;
        goto cleanup;
    }
    if (!strstr(actual_text, "call_funcs=1") ||
        !strstr(actual_text, "memory_funcs=1") ||
        !strstr(actual_text, "functions-with-call: 1") ||
        !strstr(actual_text, "functions-with-memory-ops: 1") ||
        !strstr(actual_text,
            "fn.1 main blocks=1 phi=0 call=1 spills=0 load_local=1 store_local=0 load_global=1 store_global=1 memory_ops=3") ||
        !strstr(actual_text,
            "bb.0 phi=0 instr=5 call=1 load_local=1 store_local=0 load_global=1 store_global=1 memory_ops=3 term_ret=1 term_jmp=0 term_br=0")) {
        fprintf(stderr,
            "[machine-ir] FAIL: memory-shape report dump mismatch\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_allocate_and_rewrite_reallocates_functions_after_post_rewrite_canonicalize(void) {
    static const char *source =
        "int g0=1,g1=2,g2=3,g3=4,g4=5,g5=6;\n"
        "int norm(int x){ while(x<0)x=x+10009; while(x>=10009)x=x-10009; return x; }\n"
        "int pred(int x){ return (norm(x + g0 + g1 + 7) % 3) != 1; }\n"
        "int side(int x){ if (((x + g2) & 1) == 0) g2 = norm(g2 + x + 13); else g3 = norm(g3 + x + g2 + 17); return norm(g2 + g3 + x); }\n"
        "int wide0(int a0,int a1,int a2,int a3,int a4,int a5,int a6,int a7,int a8,int a9,int a10,int a11){\n"
        "  int t = norm(a0+a1+a2+a3+a4+a5+a6+a7+a8+a9+a10+a11+g0+g1+g2+29);\n"
        "  if ((pred(t) && side(a0 + a5)) || pred(a11)) t = norm(t + g2 + g3); else t = norm(t + g4 + g5);\n"
        "  return t;\n"
        "}\n"
        "int wide1(int a0,int a1,int a2,int a3,int a4,int a5,int a6,int a7,int a8,int a9,int a10,int a11){\n"
        "  return wide0(a11,a10,a9,a8,a7,a6,a5,a4,a3,a2,a1,a0);\n"
        "}\n"
        "int main(){ return wide1(1,2,3,4,5,6,7,8,9,10,11,12); }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);

    if (!build_value_ssa_program_from_source_text(source, &program, &value_error) ||
        !machine_ir_build_allocate_and_rewrite_program_single_block_spills_flat_report(
            &program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: post-rewrite canonicalize reallocate witness setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function wide0 params=12") ||
        !strstr(actual_text, "  bb.1:\n") ||
        !strstr(actual_text, "    reg.0(r0) = load global.2\n") ||
        !strstr(actual_text, "    reg.0(r0) = load global.3\n") ||
        !strstr(actual_text, "    reg.0(r0) = call norm(reg.0(r0))\n") ||
        !strstr(actual_text, "    store local.12, reg.0(r0)\n") ||
        strstr(actual_text,
            "    reg.0(r0) = add reg.0(r0), reg.0(r0)\n")) {
        fprintf(stderr,
            "[machine-ir] FAIL: post-rewrite canonicalize witness kept stale allocation mapping\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_translation_only_preserves_float_metadata(void) {
    ValueSsaProgram program;
    ValueSsaGlobal *global = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    size_t param_local_id = 0u;
    size_t loaded_value_id = 0u;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!value_ssa_program_append_global(&program, "g", &global, &value_error) ||
        !global ||
        !value_ssa_program_append_function(&program, "id", 1, &function, &value_error) ||
        !function ||
        !value_ssa_function_append_local(function, "x", 1, &param_local_id, &value_error) ||
        !value_ssa_function_append_block(function, NULL, &block, &value_error)) {
        fprintf(stderr, "[machine-ir] FAIL: float translation-only program setup failed: %s\n", value_error.message);
        ok = 0;
        goto cleanup;
    }
    global->value_type = AST_FUNCTION_RETURN_FLOAT;
    function->locals[param_local_id].value_type = AST_FUNCTION_RETURN_FLOAT;
    loaded_value_id = value_ssa_function_allocate_value(function);
    if (loaded_value_id == (size_t)-1) {
        fprintf(stderr, "[machine-ir] FAIL: float translation-only value allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(loaded_value_id);
    instruction.as.load_slot = value_ssa_slot_local(param_local_id);
    if (!value_ssa_block_append_instruction(block, &instruction, &value_error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(loaded_value_id), &value_error) ||
        !machine_ir_build_translation_only_report(
            &program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: float translation-only setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "global.0 g init=0 runtime-init=0:float") ||
        !strstr(actual_text, "function id params=1 locals=1 spills=0") ||
        !strstr(actual_text, "param local.0:float name=x")) {
        fprintf(stderr,
            "[machine-ir] FAIL: float translation-only dump missing metadata\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_translation_only_lowers_extension_float_transport(void) {
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_value_ssa_program_from_extension_source_text(
            "float id(float x){ return x; } int main(){ float x = 1.25; id(1.25); return 0; }\n",
            &program,
            &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: extension float translation-only setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "param local.0:float name=x") ||
        !strstr(actual_text, "store local.0, 1067450368") ||
        !strstr(actual_text, "call id(1067450368)")) {
        fprintf(stderr,
            "[machine-ir] FAIL: extension float translation-only dump missing literal transport\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_translation_only_lowers_extension_float_global_literal_runtime_init(void) {
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_value_ssa_program_from_extension_source_text(
            "float g = 1.25;\nfloat id(float x){ return x; }\nint main(){ return 0; }\n",
            &program,
            &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: extension float global literal runtime-init setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "global.0 g init=0 runtime-init=1:float") ||
        !strstr(actual_text, "function __global.init params=0 locals=0 spills=0") ||
        !strstr(actual_text, "store global.0, 1067450368")) {
        fprintf(stderr,
            "[machine-ir] FAIL: extension float global literal runtime-init dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_rejects_global_float_operator_expression_under_extension(void) {
    ValueSsaProgram program;
    ValueSsaError value_error;

    value_ssa_program_init(&program);
    memset(&value_error, 0, sizeof(value_error));

    if (build_value_ssa_program_from_extension_source_text(
            "float g = 1.25;\nint main(){ return g + 1; }\n",
            &program,
            &value_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-GLOBAL-OP-SEMANTIC-REJECT should have failed\n");
        value_ssa_program_free(&program);
        return 0;
    }
    if (strstr(value_error.message, "SEMA-EXT-035") == NULL) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-GLOBAL-OP-SEMANTIC-REJECT mismatch: %s\n",
            value_error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    value_ssa_program_free(&program);
    return 1;
}

static int test_machine_ir_rejects_global_float_operator_expression_in_initializer_under_extension(void) {
    ValueSsaProgram program;
    ValueSsaError value_error;

    value_ssa_program_init(&program);
    memset(&value_error, 0, sizeof(value_error));

    if (build_value_ssa_program_from_extension_source_text(
            "float g = 1.25;\nint h = g + 1;\nint main(){ return 0; }\n",
            &program,
            &value_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-GLOBAL-OP-INIT-REJECT should have failed\n");
        value_ssa_program_free(&program);
        return 0;
    }
    if (strstr(value_error.message, "SEMA-EXT-035") == NULL) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-GLOBAL-OP-INIT-REJECT mismatch: %s\n",
            value_error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    value_ssa_program_free(&program);
    return 1;
}

static int test_machine_ir_rejects_global_float_call_result_in_initializer_under_extension(void) {
    ValueSsaProgram program;
    ValueSsaError value_error;

    value_ssa_program_init(&program);
    memset(&value_error, 0, sizeof(value_error));

    if (build_value_ssa_program_from_extension_source_text(
            "float g = 1.25;\nfloat id(float x){ return x; }\nint h = id(g);\nint main(){ return 0; }\n",
            &program,
            &value_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-GLOBAL-CALL-INIT-TYPE-REJECT should have failed\n");
        value_ssa_program_free(&program);
        return 0;
    }
    if (strstr(value_error.message, "SEMA-TYPE-004") == NULL) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-GLOBAL-CALL-INIT-TYPE-REJECT mismatch: %s\n",
            value_error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    value_ssa_program_free(&program);
    return 1;
}

static int test_machine_ir_rejects_float_assignment_to_int_under_extension(void) {
    ValueSsaProgram program;
    ValueSsaError value_error;

    value_ssa_program_init(&program);
    memset(&value_error, 0, sizeof(value_error));

    if (build_value_ssa_program_from_extension_source_text(
            "float g = 1.25;\nint main(){ int x = 0; x = g; return 0; }\n",
            &program,
            &value_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-ASSIGN-TYPE-REJECT should have failed\n");
        value_ssa_program_free(&program);
        return 0;
    }
    if (strstr(value_error.message, "SEMA-TYPE-006") == NULL) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-ASSIGN-TYPE-REJECT mismatch: %s\n",
            value_error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    value_ssa_program_free(&program);
    return 1;
}

static int test_machine_ir_rejects_float_ternary_value_return_to_int_under_extension(void) {
    ValueSsaProgram program;
    ValueSsaError value_error;

    value_ssa_program_init(&program);
    memset(&value_error, 0, sizeof(value_error));

    if (build_value_ssa_program_from_extension_source_text(
            "float g = 1.25;\n"
            "float h = 2.5;\n"
            "int bad(){ return g ? h : h; }\n"
            "int main(){ return 0; }\n",
            &program,
            &value_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-TERNARY-VALUE-RETURN-INT-REJECT should have failed\n");
        value_ssa_program_free(&program);
        return 0;
    }
    if (strstr(value_error.message, "SEMA-TYPE-005") == NULL) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-TERNARY-VALUE-RETURN-INT-REJECT mismatch: %s\n",
            value_error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    value_ssa_program_free(&program);
    return 1;
}

static int test_machine_ir_rejects_unary_call_ternary_value_return_to_int_under_extension(void) {
    ValueSsaProgram program;
    ValueSsaError value_error;

    value_ssa_program_init(&program);
    memset(&value_error, 0, sizeof(value_error));

    if (build_value_ssa_program_from_extension_source_text(
            "float id(float x){ return x; }\n"
            "int bad(){ return -id(1.0) ? 1.0 : 2.0; }\n"
            "int main(){ return 0; }\n",
            &program,
            &value_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-UNARY-CALL-TERNARY-RETURN-INT-REJECT should have failed\n");
        value_ssa_program_free(&program);
        return 0;
    }
    if (strstr(value_error.message, "SEMA-TYPE-005") == NULL) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-UNARY-CALL-TERNARY-RETURN-INT-REJECT mismatch: %s\n",
            value_error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    value_ssa_program_free(&program);
    return 1;
}

static int test_machine_ir_accepts_float_if_condition_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "int main(){ if(g) return 1; return 0; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_default_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-IF-COND-ACCEPT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "global.0 g init=0 runtime-init=1:float") ||
        !strstr(actual_text, "function main params=0 locals=0 spills=0") ||
        !strstr(actual_text, "ret 1")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-IF-COND-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_accepts_float_while_condition_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "int main(){ while(g) return 1; return 0; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_default_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-WHILE-COND-ACCEPT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "global.0 g init=0 runtime-init=1:float") ||
        !strstr(actual_text, "function main params=0 locals=0 spills=0") ||
        !strstr(actual_text, "ret 1")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-WHILE-COND-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_accepts_float_for_condition_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "int main(){ for(;g;) return 1; return 0; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_default_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-FOR-COND-ACCEPT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "global.0 g init=0 runtime-init=1:float") ||
        !strstr(actual_text, "function main params=0 locals=0 spills=0") ||
        !strstr(actual_text, "ret 1")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-FOR-COND-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_accepts_float_logical_condition_composition_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "int main(){ if(!g || (g && 1.25)) return g ? 1 : 0; return 0; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_default_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-LOGICAL-COND-COMPOSE-ACCEPT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "global.0 g init=0 runtime-init=1:float") ||
        !strstr(actual_text, "function main params=0 locals=0 spills=0") ||
        !strstr(actual_text, "load global.0") ||
        !strstr(actual_text, "and reg.0(r0), 2147483647") ||
        !strstr(actual_text, "br reg.0(r0), bb.2, bb.3") ||
        !strstr(actual_text, "ret reg.0(r0)")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-LOGICAL-COND-COMPOSE-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_accepts_recursive_float_if_condition_under_extension(void) {
    static const char *source =
        "int f(float x, float y, float z){ if((x + y) + z) return 1; return 0; }\n"
        "int main(){ return 0; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-RECURSIVE-IF-COND-ACCEPT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function f params=3 locals=3 spills=0") ||
        !strstr(actual_text, "call __builtin_fadd32") ||
        !strstr(actual_text, "and") ||
        !strstr(actual_text, "2147483647") ||
        !strstr(actual_text, "br reg.")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-RECURSIVE-IF-COND-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_accepts_recursive_explicit_float_condition_under_extension(void) {
    static const char *source =
        "int add3(int a, int b, int c){ return (a + b) + c; }\n"
        "int main(){ if(float(add3(1, 2, 3))) return 1; return 0; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-CONVERT-RECURSIVE-COND-ACCEPT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function add3 params=3 locals=3 spills=0") ||
        !strstr(actual_text, "call __builtin_i2f32") ||
        !strstr(actual_text, "2147483647") ||
        !strstr(actual_text, "br reg.")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-CONVERT-RECURSIVE-COND-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_accepts_explicit_float_while_condition_under_extension(void) {
    static const char *source =
        "int main(){ while(float(3)) return 1; return 0; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-CONVERT-WHILE-COND-ACCEPT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "call __builtin_i2f32") ||
        !strstr(actual_text, "2147483647") ||
        !strstr(actual_text, "jmp bb.1") ||
        !strstr(actual_text, "br reg.")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-CONVERT-WHILE-COND-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_accepts_explicit_float_logical_condition_composition_under_extension(void) {
    static const char *source =
        "int add3(int a, int b, int c){ return (a + b) + c; }\n"
        "int main(){ if(!float(0) || (float(3) && float(add3(1, 2, 3)))) return 1; return 0; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-CONVERT-LOGIC-COND-ACCEPT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function add3 params=3 locals=3 spills=0") ||
        !strstr(actual_text, "call __builtin_i2f32") ||
        !strstr(actual_text, "call add3") ||
        !strstr(actual_text, "2147483647") ||
        !strstr(actual_text, "br reg.")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-CONVERT-LOGIC-COND-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_rejects_recursive_float_condition_with_ternary_neighbor_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "int main(){ if((g ? h : h) + h) return 1; return 0; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;

    value_ssa_program_init(&program);
    memset(&value_error, 0, sizeof(value_error));

    if (build_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        strstr(value_error.message, "SEMA-EXT-035") == NULL) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-RECURSIVE-COND-TERNARY-NEIGHBOR-REJECT mismatch: %s\n",
            value_error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    value_ssa_program_free(&program);
    return 1;
}

static int test_machine_ir_accepts_float_equality_compare_under_extension(void) {
    static const char *source =
        "int eq(float x, float y){ return x == y; }\n"
        "int main(){ return 0; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_default_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-EQ-COMPARE-ACCEPT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function eq params=2 locals=2 spills=0") ||
        !strstr(actual_text, "load local.0") ||
        !strstr(actual_text, "load local.1") ||
        !strstr(actual_text, "and") ||
        !strstr(actual_text, " ne ") ||
        !strstr(actual_text, "mul") ||
        !strstr(actual_text, " eq ") ||
        !strstr(actual_text, "ret reg.")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-EQ-COMPARE-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_accepts_float_relational_compare_under_extension(void) {
    static const char *source =
        "int lt(float x, float y){ return x < y; }\n"
        "int main(){ return 0; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_default_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-LT-COMPARE-ACCEPT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function lt params=2 locals=2 spills=0") ||
        !strstr(actual_text, "load local.0") ||
        !strstr(actual_text, "load local.1") ||
        !strstr(actual_text, "and") ||
        !strstr(actual_text, " ne ") ||
        !strstr(actual_text, "mul") ||
        !strstr(actual_text, "shr") ||
        !strstr(actual_text, "or") ||
        !strstr(actual_text, "xor") ||
        !strstr(actual_text, " lt ") ||
        !strstr(actual_text, "ret reg.")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-LT-COMPARE-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_accepts_chained_float_equality_compare_under_extension(void) {
    static const char *source =
        "int eq(float x, float y, float z){ return ((x + y) + z) == z; }\n"
        "int main(){ return 0; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_default_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-CHAIN-ADD-EQ-COMPARE-ACCEPT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function eq params=3") ||
        !strstr(actual_text, "call __builtin_fadd32(") ||
        !strstr(actual_text, " eq ") ||
        !strstr(actual_text, "ret reg.")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-CHAIN-ADD-EQ-COMPARE-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_accepts_chained_float_relational_compare_under_extension(void) {
    static const char *source =
        "int lt(float x, float y, float z){ return ((x + y) + z) < z; }\n"
        "int main(){ return 0; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_default_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-CHAIN-ADD-LT-COMPARE-ACCEPT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function lt params=3") ||
        !strstr(actual_text, "call __builtin_fadd32(") ||
        !strstr(actual_text, " lt ") ||
        !strstr(actual_text, "ret reg.")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-CHAIN-ADD-LT-COMPARE-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_accepts_nested_muldiv_float_equality_compare_under_extension(void) {
    static const char *source =
        "int eq(float a, float b, float c){ return (-a * (b / c)) == c; }\n"
        "int main(){ return 0; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_default_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-NESTED-MUL-DIV-EQ-COMPARE-ACCEPT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function eq params=3") ||
        !strstr(actual_text, "call __builtin_fdiv32(") ||
        !strstr(actual_text, "call __builtin_fmul32(") ||
        !strstr(actual_text, " eq ") ||
        !strstr(actual_text, "ret reg.")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-NESTED-MUL-DIV-EQ-COMPARE-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_accepts_nested_muldiv_float_relational_compare_under_extension(void) {
    static const char *source =
        "int lt(float a, float b, float c){ return (-a * (b / c)) < c; }\n"
        "int main(){ return 0; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_default_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-NESTED-MUL-DIV-LT-COMPARE-ACCEPT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function lt params=3") ||
        !strstr(actual_text, "call __builtin_fdiv32(") ||
        !strstr(actual_text, "call __builtin_fmul32(") ||
        !strstr(actual_text, " lt ") ||
        !strstr(actual_text, "ret reg.")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-NESTED-MUL-DIV-LT-COMPARE-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_accepts_chained_float_inequality_compare_under_extension(void) {
    static const char *source =
        "int ne(float x, float y, float z){ return ((x + y) + z) != z; }\n"
        "int main(){ return 0; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_default_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-CHAIN-ADD-NE-COMPARE-ACCEPT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function ne params=3") ||
        !strstr(actual_text, "call __builtin_fadd32(") ||
        !strstr(actual_text, " ne ") ||
        !strstr(actual_text, "ret reg.")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-CHAIN-ADD-NE-COMPARE-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_accepts_chained_float_le_compare_under_extension(void) {
    static const char *source =
        "int le(float x, float y, float z){ return ((x + y) + z) <= z; }\n"
        "int main(){ return 0; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_default_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-CHAIN-ADD-LE-COMPARE-ACCEPT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function le params=3") ||
        !strstr(actual_text, "call __builtin_fadd32(") ||
        !strstr(actual_text, " le ") ||
        !strstr(actual_text, "ret reg.")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-CHAIN-ADD-LE-COMPARE-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_accepts_nested_muldiv_float_inequality_compare_under_extension(void) {
    static const char *source =
        "int ne(float a, float b, float c){ return (-a * (b / c)) != c; }\n"
        "int main(){ return 0; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_default_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-NESTED-MUL-DIV-NE-COMPARE-ACCEPT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function ne params=3") ||
        !strstr(actual_text, "call __builtin_fdiv32(") ||
        !strstr(actual_text, "call __builtin_fmul32(") ||
        !strstr(actual_text, " ne ") ||
        !strstr(actual_text, "ret reg.")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-NESTED-MUL-DIV-NE-COMPARE-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_accepts_nested_muldiv_float_ge_compare_under_extension(void) {
    static const char *source =
        "int ge(float a, float b, float c){ return (-a * (b / c)) >= c; }\n"
        "int main(){ return 0; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_default_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-NESTED-MUL-DIV-GE-COMPARE-ACCEPT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function ge params=3") ||
        !strstr(actual_text, "call __builtin_fdiv32(") ||
        !strstr(actual_text, "call __builtin_fmul32(") ||
        !strstr(actual_text, " ge ") ||
        !strstr(actual_text, "ret reg.")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-NESTED-MUL-DIV-GE-COMPARE-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_accepts_signed_zero_float_equality_under_extension(void) {
    static const char *source =
        "int z(){ return 0.0 == -0.0; }\n"
        "int main(){ return 0; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_default_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-SIGNED-ZERO-EQ-ACCEPT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function z params=0 locals=0 spills=0") ||
        !strstr(actual_text, "ret 1")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-SIGNED-ZERO-EQ-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_accepts_negative_float_relational_compare_under_extension(void) {
    static const char *source =
        "int lt(){ return -1.25 < 0.0; }\n"
        "int main(){ return 0; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_default_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-NEG-LT-ZERO-ACCEPT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function lt params=0 locals=0 spills=0") ||
        !strstr(actual_text, "ret 0")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-NEG-LT-ZERO-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_accepts_float_addition_under_extension(void) {
    static const char *source =
        "float add(float x, float y){ return x + y; }\n"
        "int main(){ return 0; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_default_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-ADD-ACCEPT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function add params=2 locals=2 spills=0") ||
        !strstr(actual_text, "load local.0") ||
        !strstr(actual_text, "load local.1") ||
        !strstr(actual_text, "call __builtin_fadd32(") ||
        !strstr(actual_text, "ret reg.")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-ADD-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_accepts_float_subtraction_under_extension(void) {
    static const char *source =
        "float sub(float x, float y){ return x - y; }\n"
        "int main(){ return 0; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_default_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-SUB-ACCEPT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function sub params=2 locals=2 spills=0") ||
        !strstr(actual_text, "load local.0") ||
        !strstr(actual_text, "load local.1") ||
        !strstr(actual_text, "call __builtin_fsub32(") ||
        !strstr(actual_text, "ret reg.")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-SUB-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_accepts_negative_float_addition_combo_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float add(float y){ return -g + y; }\n"
        "int main(){ return 0; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_default_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-NEG-ADD-COMBO-ACCEPT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function add params=1 locals=1 spills=0") ||
        !strstr(actual_text, "function __builtin_fadd32 params=0 locals=0 spills=0") ||
        !strstr(actual_text, "load global.0") ||
        !strstr(actual_text, "load local.0") ||
        !strstr(actual_text, "xor reg.") ||
        !strstr(actual_text, "call __builtin_fadd32(") ||
        !strstr(actual_text, "ret reg.")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-NEG-ADD-COMBO-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_accepts_negative_float_subtraction_combo_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float sub(float y){ return y - -g; }\n"
        "int main(){ return 0; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_default_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-NEG-SUB-COMBO-ACCEPT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function sub params=1 locals=1 spills=0") ||
        !strstr(actual_text, "function __builtin_fsub32 params=0 locals=0 spills=0") ||
        !strstr(actual_text, "load global.0") ||
        !strstr(actual_text, "load local.0") ||
        !strstr(actual_text, "xor reg.") ||
        !strstr(actual_text, "call __builtin_fsub32(") ||
        !strstr(actual_text, "ret reg.")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-NEG-SUB-COMBO-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_accepts_float_multiplication_under_extension(void) {
    static const char *source =
        "float mul(float x, float y){ return x * y; }\n"
        "int main(){ return 0; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_default_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-MUL-ACCEPT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function mul params=2 locals=2 spills=0") ||
        !strstr(actual_text, "call __builtin_fmul32(") ||
        !strstr(actual_text, "ret reg.")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-MUL-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_accepts_float_division_under_extension(void) {
    static const char *source =
        "float divv(float x, float y){ return x / y; }\n"
        "int main(){ return 0; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_default_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-DIV-ACCEPT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function divv params=2 locals=2 spills=0") ||
        !strstr(actual_text, "call __builtin_fdiv32(") ||
        !strstr(actual_text, "ret reg.")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-DIV-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_accepts_negative_float_multiplication_combo_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float mul(float y){ return -g * y; }\n"
        "int main(){ return 0; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_default_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-NEG-MUL-COMBO-ACCEPT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function mul params=1 locals=1 spills=0") ||
        !strstr(actual_text, "load global.0") ||
        !strstr(actual_text, "xor reg.") ||
        !strstr(actual_text, "call __builtin_fmul32(") ||
        !strstr(actual_text, "ret reg.")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-NEG-MUL-COMBO-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_accepts_negative_float_division_combo_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float divv(float y){ return y / -g; }\n"
        "int main(){ return 0; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_default_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-NEG-DIV-COMBO-ACCEPT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function divv params=1 locals=1 spills=0") ||
        !strstr(actual_text, "load global.0") ||
        !strstr(actual_text, "xor reg.") ||
        !strstr(actual_text, "call __builtin_fdiv32(") ||
        !strstr(actual_text, "ret reg.")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-NEG-DIV-COMBO-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_accepts_chained_float_addition_under_extension(void) {
    static const char *source =
        "float add3(float x, float y, float z){ return (x + y) + z; }\n"
        "int main(){ return 0; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    const char *first_add = NULL;
    const char *second_add = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_default_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-CHAIN-ADD-ACCEPT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    first_add = actual_text ? strstr(actual_text, "call __builtin_fadd32(") : NULL;
    second_add = first_add ? strstr(first_add + 1, "call __builtin_fadd32(") : NULL;
    if (!strstr(actual_text, "function add3 params=3") ||
        !first_add ||
        !second_add ||
        !strstr(actual_text, "ret reg.")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-CHAIN-ADD-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_accepts_float_helper_wrapped_ternary_call_arithmetic_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "float pick(){ return g ? h : h; }\n"
        "float get(){ return pick() + h; }\n"
        "int main(){ return 0; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_default_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-HELPER-TERNARY-CALL-ARITH-ACCEPT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function pick params=0 locals=0 spills=0") ||
        !strstr(actual_text, "function get params=0 locals=0 spills=0") ||
        !strstr(actual_text, "reg.1(r1) = call pick()") ||
        !strstr(actual_text, "reg.0(r0) = load global.1") ||
        !strstr(actual_text, "call __builtin_fadd32(reg.1(r1), reg.0(r0))") ||
        !strstr(actual_text, "ret reg.0(r0)")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-HELPER-TERNARY-CALL-ARITH-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_accepts_unary_call_helper_wrapped_ternary_call_arithmetic_under_extension(void) {
    static const char *source =
        "float id(float x){ return x; }\n"
        "float pick(float x){ return -id(x) ? x : x; }\n"
        "float f(float x){ return pick(x) + x; }\n"
        "int main(){ return 0; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_default_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-UNARY-HELPER-TERNARY-CALL-ARITH-ACCEPT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function pick params=1 locals=1 spills=0") ||
        !strstr(actual_text, "function f params=1 locals=1 spills=1") ||
        !strstr(actual_text, "spill.0 = load local.0") ||
        !strstr(actual_text, "reg.0(r0) = call pick(spill.0)") ||
        !strstr(actual_text, "call __builtin_fadd32(reg.0(r0), spill.0)") ||
        !strstr(actual_text, "ret reg.0(r0)")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-UNARY-HELPER-TERNARY-CALL-ARITH-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_accepts_float_helper_wrapped_ternary_call_compare_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "float pick(){ return g ? h : h; }\n"
        "int eq(){ return pick() == h; }\n"
        "int main(){ return 0; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_default_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-HELPER-TERNARY-CALL-COMPARE-ACCEPT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function pick params=0 locals=0 spills=0") ||
        !strstr(actual_text, "function eq params=0 locals=0 spills=0") ||
        !strstr(actual_text, "reg.1(r1) = call pick()") ||
        !strstr(actual_text, "reg.0(r0) = eq reg.2(r2), reg.0(r0)") ||
        !strstr(actual_text, "ret reg.0(r0)")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-HELPER-TERNARY-CALL-COMPARE-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_accepts_unary_call_helper_wrapped_ternary_call_compare_under_extension(void) {
    static const char *source =
        "float id(float x){ return x; }\n"
        "float pick(float x){ return -id(x) ? x : x; }\n"
        "int eq(float x){ return pick(x) == x; }\n"
        "int main(){ return 0; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_default_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-UNARY-HELPER-TERNARY-CALL-COMPARE-ACCEPT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function pick params=1 locals=1 spills=0") ||
        !strstr(actual_text, "function eq params=1 locals=1 spills=1") ||
        !strstr(actual_text, "spill.0 = load local.0") ||
        !strstr(actual_text, "reg.1(r1) = call pick(spill.0)") ||
        !strstr(actual_text, "reg.1(r1) = mul reg.1(r1), reg.0(r0)") ||
        !strstr(actual_text, "reg.0(r0) = mul spill.0, reg.0(r0)") ||
        !strstr(actual_text, "reg.0(r0) = eq reg.1(r1), reg.0(r0)") ||
        !strstr(actual_text, "ret reg.0(r0)")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-UNARY-HELPER-TERNARY-CALL-COMPARE-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_accepts_explicit_int_from_float_conversion_under_extension(void) {
    static const char *source =
        "int conv(float x, float y){ return int(x + y); }\n"
        "int main(){ return 0; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_default_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-TO-INT-CONVERT-ACCEPT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function conv params=2") ||
        !strstr(actual_text, "call __builtin_fadd32(") ||
        !strstr(actual_text, "call __builtin_f2i32(") ||
        !strstr(actual_text, "ret reg.")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-TO-INT-CONVERT-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_accepts_explicit_float_from_int_conversion_under_extension(void) {
    static const char *source =
        "float conv(int x, int y){ return float(x + y); }\n"
        "int main(){ return 0; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_default_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-INT-TO-FLOAT-CONVERT-ACCEPT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function conv params=2") ||
        !strstr(actual_text, " add ") ||
        !strstr(actual_text, "call __builtin_i2f32(") ||
        !strstr(actual_text, "ret reg.")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-INT-TO-FLOAT-CONVERT-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_accepts_explicit_float_from_recursive_int_initializer_bridge_under_extension(void) {
    static const char *source =
        "int add3(int a, int b, int c){ return (a + b) + c; }\n"
        "float z = float(add3(1, 2, 3));\n"
        "int main(){ return 0; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_default_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-INT-TO-FLOAT-RECURSIVE-INIT-BRIDGE-ACCEPT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "global.0 z") ||
        !strstr(actual_text, "function __global.init") ||
        !strstr(actual_text, "call __builtin_i2f32(6)") ||
        !strstr(actual_text, "store global.0")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-INT-TO-FLOAT-RECURSIVE-INIT-BRIDGE-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_accepts_explicit_float_from_recursive_int_assignment_bridge_under_extension(void) {
    static const char *source =
        "int add3(int a, int b, int c){ return (a + b) + c; }\n"
        "float mainf(){ float y; y = float(add3(1, 2, 3)); return y; }\n"
        "int main(){ return 0; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_default_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-INT-TO-FLOAT-RECURSIVE-ASSIGN-BRIDGE-ACCEPT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function mainf params=0") ||
        !strstr(actual_text, "call __builtin_i2f32(6)") ||
        !strstr(actual_text, "store local.0") ||
        !strstr(actual_text, "load local.0")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-INT-TO-FLOAT-RECURSIVE-ASSIGN-BRIDGE-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_accepts_explicit_float_from_int_compare_bridge_under_extension(void) {
    static const char *source =
        "int add3(int a, int b, int c){ return (a + b) + c; }\n"
        "int main(){ return float(add3(1, 2, 3)) == float(6); }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_default_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-INT-TO-FLOAT-COMPARE-BRIDGE-ACCEPT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "call __builtin_i2f32(6)") ||
        !strstr(actual_text, "2147483647") ||
        !strstr(actual_text, " eq ") ||
        !strstr(actual_text, "ret reg.")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-INT-TO-FLOAT-COMPARE-BRIDGE-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_accepts_explicit_float_from_recursive_int_arithmetic_bridge_under_extension(void) {
    static const char *source =
        "int add3(int a, int b, int c){ return (a + b) + c; }\n"
        "float mainf(){ return float(add3(1, 2, 3)) + 1.25; }\n"
        "int main(){ return 0; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_default_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-INT-TO-FLOAT-RECURSIVE-ARITH-BRIDGE-ACCEPT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function mainf params=0") ||
        !strstr(actual_text, "call __builtin_i2f32(") ||
        !strstr(actual_text, " add ") ||
        !strstr(actual_text, "ret reg.")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-INT-TO-FLOAT-RECURSIVE-ARITH-BRIDGE-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_accepts_explicit_int_from_float_ternary_bridge_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "int main(){ return int(g ? h : h); }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_default_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-TO-INT-TERNARY-BRIDGE-ACCEPT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function main params=0") ||
        !strstr(actual_text, "store global.0, 1067450368") ||
        !strstr(actual_text, "store global.1, 1075838976") ||
        !strstr(actual_text, "call __builtin_f2i32(1075838976)") ||
        !strstr(actual_text, "ret reg.")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-TO-INT-TERNARY-BRIDGE-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_accepts_explicit_int_from_recursive_float_call_argument_bridge_under_extension(void) {
    static const char *source =
        "int sink(int x){ return x; }\n"
        "float add3(float a, float b, float c){ return (a + b) + c; }\n"
        "int main(){ return sink(int(add3(1.0, 2.0, 3.0))); }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_default_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-TO-INT-RECURSIVE-CALLARG-BRIDGE-ACCEPT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function add3 params=3") ||
        !strstr(actual_text, "call __builtin_fadd32(") ||
        !strstr(actual_text, "call __builtin_f2i32(") ||
        !strstr(actual_text, "function sink params=1") ||
        !strstr(actual_text, "ret reg.")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-TO-INT-RECURSIVE-CALLARG-BRIDGE-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_accepts_explicit_int_from_recursive_float_assignment_bridge_under_extension(void) {
    static const char *source =
        "float add3(float a, float b, float c){ return (a + b) + c; }\n"
        "int main(){ int x=0; x = int(add3(1.0, 2.0, 3.0)); return x; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_default_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-TO-INT-RECURSIVE-ASSIGN-BRIDGE-ACCEPT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function add3 params=3") ||
        !strstr(actual_text, "call __builtin_f2i32(") ||
        !strstr(actual_text, "store local.0") ||
        !strstr(actual_text, "load local.0") ||
        !strstr(actual_text, "ret reg.")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-TO-INT-RECURSIVE-ASSIGN-BRIDGE-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_accepts_explicit_int_from_float_compare_bridge_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "int main(){ return int(g ? h : h) == 2; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_default_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-TO-INT-COMPARE-BRIDGE-ACCEPT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "call __builtin_f2i32(") ||
        !strstr(actual_text, " eq ") ||
        !strstr(actual_text, "ret reg.")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-TO-INT-COMPARE-BRIDGE-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_accepts_explicit_int_from_recursive_float_arithmetic_bridge_under_extension(void) {
    static const char *source =
        "float add3(float a, float b, float c){ return (a + b) + c; }\n"
        "int main(){ return int(add3(1.0, 2.0, 3.0)) + 1; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_default_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-TO-INT-RECURSIVE-ARITH-BRIDGE-ACCEPT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function add3 params=3") ||
        !strstr(actual_text, "call __builtin_f2i32(") ||
        !strstr(actual_text, " add ") ||
        !strstr(actual_text, "ret reg.")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-TO-INT-RECURSIVE-ARITH-BRIDGE-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_accepts_nested_float_mul_div_under_extension(void) {
    static const char *source =
        "float f(float a, float b, float c){ return -a * (b / c); }\n"
        "int main(){ return 0; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_default_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-NESTED-MUL-DIV-ACCEPT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function f params=3") ||
        !strstr(actual_text, "call __builtin_fdiv32(") ||
        !strstr(actual_text, "call __builtin_fmul32(") ||
        !strstr(actual_text, "ret reg.")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-NESTED-MUL-DIV-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_rejects_mixed_float_int_arithmetic_under_extension(void) {
    ValueSsaProgram program;
    ValueSsaError value_error;

    value_ssa_program_init(&program);
    memset(&value_error, 0, sizeof(value_error));

    if (build_value_ssa_program_from_extension_source_text(
            "float add(float x){ return x + 1; }\n"
            "int main(){ return 0; }\n",
            &program,
            &value_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-ARITH-INT-TYPE-REJECT should have failed\n");
        value_ssa_program_free(&program);
        return 0;
    }
    if (strstr(value_error.message, "SEMA-TYPE-008") == NULL) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-ARITH-INT-TYPE-REJECT mismatch: %s\n",
            value_error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    value_ssa_program_free(&program);
    return 1;
}

static int test_machine_ir_rejects_float_call_int_arithmetic_under_extension(void) {
    ValueSsaProgram program;
    ValueSsaError value_error;

    value_ssa_program_init(&program);
    memset(&value_error, 0, sizeof(value_error));

    if (build_value_ssa_program_from_extension_source_text(
            "float id(float x){ return x; }\n"
            "float add(float x){ return id(x) + 1; }\n"
            "int main(){ return 0; }\n",
            &program,
            &value_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-CALL-ARITH-INT-TYPE-REJECT should have failed\n");
        value_ssa_program_free(&program);
        return 0;
    }
    if (strstr(value_error.message, "SEMA-TYPE-008") == NULL) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-CALL-ARITH-INT-TYPE-REJECT mismatch: %s\n",
            value_error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    value_ssa_program_free(&program);
    return 1;
}

static int test_machine_ir_rejects_float_literal_int_arithmetic_under_extension(void) {
    ValueSsaProgram program;
    ValueSsaError value_error;

    value_ssa_program_init(&program);
    memset(&value_error, 0, sizeof(value_error));

    if (build_value_ssa_program_from_extension_source_text(
            "float add(){ return 1.25 + 1; }\n"
            "int main(){ return 0; }\n",
            &program,
            &value_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-LITERAL-ARITH-INT-TYPE-REJECT should have failed\n");
        value_ssa_program_free(&program);
        return 0;
    }
    if (strstr(value_error.message, "SEMA-TYPE-008") == NULL) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-LITERAL-ARITH-INT-TYPE-REJECT mismatch: %s\n",
            value_error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    value_ssa_program_free(&program);
    return 1;
}

static int test_machine_ir_rejects_negative_float_call_int_arithmetic_under_extension(void) {
    ValueSsaProgram program;
    ValueSsaError value_error;

    value_ssa_program_init(&program);
    memset(&value_error, 0, sizeof(value_error));

    if (build_value_ssa_program_from_extension_source_text(
            "float id(float x){ return x; }\n"
            "float add(float x){ return -id(x) * 1; }\n"
            "int main(){ return 0; }\n",
            &program,
            &value_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-NEG-CALL-ARITH-INT-TYPE-REJECT should have failed\n");
        value_ssa_program_free(&program);
        return 0;
    }
    if (strstr(value_error.message, "SEMA-TYPE-008") == NULL) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-NEG-CALL-ARITH-INT-TYPE-REJECT mismatch: %s\n",
            value_error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    value_ssa_program_free(&program);
    return 1;
}

static int test_machine_ir_rejects_global_float_int_condition_under_extension(void) {
    ValueSsaProgram program;
    ValueSsaError value_error;

    value_ssa_program_init(&program);
    memset(&value_error, 0, sizeof(value_error));

    if (build_value_ssa_program_from_extension_source_text(
            "float g = 1.25;\n"
            "int main(){ if(g + 1) return 1; return 0; }\n",
            &program,
            &value_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-GLOBAL-COND-PLUS-INT-REJECT should have failed\n");
        value_ssa_program_free(&program);
        return 0;
    }
    if (strstr(value_error.message, "SEMA-EXT-035") == NULL) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-GLOBAL-COND-PLUS-INT-REJECT mismatch: %s\n",
            value_error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    value_ssa_program_free(&program);
    return 1;
}

static int test_machine_ir_rejects_float_call_int_condition_under_extension(void) {
    ValueSsaProgram program;
    ValueSsaError value_error;

    value_ssa_program_init(&program);
    memset(&value_error, 0, sizeof(value_error));

    if (build_value_ssa_program_from_extension_source_text(
            "float id(float x){ return x; }\n"
            "int main(){ if(id(1.0) + 1) return 1; return 0; }\n",
            &program,
            &value_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-CALL-COND-PLUS-INT-REJECT should have failed\n");
        value_ssa_program_free(&program);
        return 0;
    }
    if (strstr(value_error.message, "SEMA-TYPE-008") == NULL) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-CALL-COND-PLUS-INT-REJECT mismatch: %s\n",
            value_error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    value_ssa_program_free(&program);
    return 1;
}

static int test_machine_ir_rejects_negative_float_call_int_condition_under_extension(void) {
    ValueSsaProgram program;
    ValueSsaError value_error;

    value_ssa_program_init(&program);
    memset(&value_error, 0, sizeof(value_error));

    if (build_value_ssa_program_from_extension_source_text(
            "float id(float x){ return x; }\n"
            "int main(){ if(-id(1.0) + 1) return 1; return 0; }\n",
            &program,
            &value_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-NEG-CALL-COND-PLUS-INT-REJECT should have failed\n");
        value_ssa_program_free(&program);
        return 0;
    }
    if (strstr(value_error.message, "SEMA-TYPE-008") == NULL) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-NEG-CALL-COND-PLUS-INT-REJECT mismatch: %s\n",
            value_error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    value_ssa_program_free(&program);
    return 1;
}

static int test_machine_ir_rejects_nested_float_tree_plus_int_condition_under_extension(void) {
    ValueSsaProgram program;
    ValueSsaError value_error;

    value_ssa_program_init(&program);
    memset(&value_error, 0, sizeof(value_error));

    if (build_value_ssa_program_from_extension_source_text(
            "int main(float x, float y, float z){ if(((x + y) + z) + 1) return 1; return 0; }\n",
            &program,
            &value_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-NESTED-TREE-COND-PLUS-INT-REJECT should have failed\n");
        value_ssa_program_free(&program);
        return 0;
    }
    if (strstr(value_error.message, "SEMA-EXT-035") == NULL) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-NESTED-TREE-COND-PLUS-INT-REJECT mismatch: %s\n",
            value_error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    value_ssa_program_free(&program);
    return 1;
}

static int test_machine_ir_rejects_nested_float_muldiv_plus_int_condition_under_extension(void) {
    ValueSsaProgram program;
    ValueSsaError value_error;

    value_ssa_program_init(&program);
    memset(&value_error, 0, sizeof(value_error));

    if (build_value_ssa_program_from_extension_source_text(
            "int main(float a, float b, float c){ if((-a * (b / c)) + 1) return 1; return 0; }\n",
            &program,
            &value_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-NESTED-MULDIV-COND-PLUS-INT-REJECT should have failed\n");
        value_ssa_program_free(&program);
        return 0;
    }
    if (strstr(value_error.message, "SEMA-EXT-035") == NULL) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-NESTED-MULDIV-COND-PLUS-INT-REJECT mismatch: %s\n",
            value_error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    value_ssa_program_free(&program);
    return 1;
}

static int test_machine_ir_rejects_nested_float_tree_plus_int_under_extension(void) {
    ValueSsaProgram program;
    ValueSsaError value_error;

    value_ssa_program_init(&program);
    memset(&value_error, 0, sizeof(value_error));

    if (build_value_ssa_program_from_extension_source_text(
            "float add(float x, float y){ return (x + y) + 1; }\n"
            "int main(){ return 0; }\n",
            &program,
            &value_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-NESTED-TREE-PLUS-INT-REJECT should have failed\n");
        value_ssa_program_free(&program);
        return 0;
    }
    if (strstr(value_error.message, "SEMA-EXT-035") == NULL) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-NESTED-TREE-PLUS-INT-REJECT mismatch: %s\n",
            value_error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    value_ssa_program_free(&program);
    return 1;
}

static int test_machine_ir_rejects_nested_float_muldiv_plus_int_under_extension(void) {
    ValueSsaProgram program;
    ValueSsaError value_error;

    value_ssa_program_init(&program);
    memset(&value_error, 0, sizeof(value_error));

    if (build_value_ssa_program_from_extension_source_text(
            "float f(float a, float b, float c){ return (-a * (b / c)) + 1; }\n"
            "int main(){ return 0; }\n",
            &program,
            &value_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-NESTED-MULDIV-PLUS-INT-REJECT should have failed\n");
        value_ssa_program_free(&program);
        return 0;
    }
    if (strstr(value_error.message, "SEMA-EXT-035") == NULL) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-NESTED-MULDIV-PLUS-INT-REJECT mismatch: %s\n",
            value_error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    value_ssa_program_free(&program);
    return 1;
}

static int test_machine_ir_rejects_float_ternary_value_plus_int_under_extension(void) {
    ValueSsaProgram program;
    ValueSsaError value_error;

    value_ssa_program_init(&program);
    memset(&value_error, 0, sizeof(value_error));

    if (build_value_ssa_program_from_extension_source_text(
            "float g = 1.25;\n"
            "float h = 2.5;\n"
            "int main(){ return (g ? h : h) + 1; }\n",
            &program,
            &value_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-TERNARY-VALUE-PLUS-INT-REJECT should have failed\n");
        value_ssa_program_free(&program);
        return 0;
    }
    if (strstr(value_error.message, "SEMA-EXT-035") == NULL) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-TERNARY-VALUE-PLUS-INT-REJECT mismatch: %s\n",
            value_error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    value_ssa_program_free(&program);
    return 1;
}

static int test_machine_ir_rejects_unary_call_ternary_value_plus_int_under_extension(void) {
    ValueSsaProgram program;
    ValueSsaError value_error;

    value_ssa_program_init(&program);
    memset(&value_error, 0, sizeof(value_error));

    if (build_value_ssa_program_from_extension_source_text(
            "float id(float x){ return x; }\n"
            "float add(float x){ return (-id(x) ? x : x) + 1; }\n"
            "int main(){ return 0; }\n",
            &program,
            &value_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-UNARY-CALL-TERNARY-PLUS-INT-REJECT should have failed\n");
        value_ssa_program_free(&program);
        return 0;
    }
    if (strstr(value_error.message, "SEMA-EXT-035") == NULL) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-UNARY-CALL-TERNARY-PLUS-INT-REJECT mismatch: %s\n",
            value_error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    value_ssa_program_free(&program);
    return 1;
}

static int test_machine_ir_rejects_float_ternary_value_plus_float_call_argument_under_extension(void) {
    ValueSsaProgram program;
    ValueSsaError value_error;

    value_ssa_program_init(&program);
    memset(&value_error, 0, sizeof(value_error));

    if (build_value_ssa_program_from_extension_source_text(
            "float g = 1.25;\n"
            "float h = 2.5;\n"
            "float wrap(float x){ return x; }\n"
            "float get(){ return wrap((g ? h : h) + h); }\n"
            "int main(){ return 0; }\n",
            &program,
            &value_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-TERNARY-PLUS-FLOAT-CALLARG-REJECT should have failed\n");
        value_ssa_program_free(&program);
        return 0;
    }
    if (strstr(value_error.message, "SEMA-EXT-035") == NULL) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-TERNARY-PLUS-FLOAT-CALLARG-REJECT mismatch: %s\n",
            value_error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    value_ssa_program_free(&program);
    return 1;
}

static int test_machine_ir_rejects_unary_call_ternary_value_plus_float_call_argument_under_extension(void) {
    ValueSsaProgram program;
    ValueSsaError value_error;

    value_ssa_program_init(&program);
    memset(&value_error, 0, sizeof(value_error));

    if (build_value_ssa_program_from_extension_source_text(
            "float id(float x){ return x; }\n"
            "float wrap(float x){ return x; }\n"
            "float f(float x){ return wrap(((-id(x) ? x : x)) + x); }\n"
            "int main(){ return 0; }\n",
            &program,
            &value_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-UNARY-CALL-TERNARY-PLUS-FLOAT-CALLARG-REJECT should have failed\n");
        value_ssa_program_free(&program);
        return 0;
    }
    if (strstr(value_error.message, "SEMA-EXT-035") == NULL) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-UNARY-CALL-TERNARY-PLUS-FLOAT-CALLARG-REJECT mismatch: %s\n",
            value_error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    value_ssa_program_free(&program);
    return 1;
}

static int test_machine_ir_rejects_float_ternary_value_assignment_to_int_under_extension(void) {
    ValueSsaProgram program;
    ValueSsaError value_error;

    value_ssa_program_init(&program);
    memset(&value_error, 0, sizeof(value_error));

    if (build_value_ssa_program_from_extension_source_text(
            "float g = 1.25;\n"
            "float h = 2.5;\n"
            "int main(){ int x=0; x = (g ? h : h); return x; }\n",
            &program,
            &value_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-TERNARY-VALUE-ASSIGN-INT-REJECT should have failed\n");
        value_ssa_program_free(&program);
        return 0;
    }
    if (strstr(value_error.message, "SEMA-TYPE-006") == NULL) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-TERNARY-VALUE-ASSIGN-INT-REJECT mismatch: %s\n",
            value_error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    value_ssa_program_free(&program);
    return 1;
}

static int test_machine_ir_rejects_unary_call_ternary_value_assignment_to_int_under_extension(void) {
    ValueSsaProgram program;
    ValueSsaError value_error;

    value_ssa_program_init(&program);
    memset(&value_error, 0, sizeof(value_error));

    if (build_value_ssa_program_from_extension_source_text(
            "float id(float x){ return x; }\n"
            "int main(){ int y=0; y = (-id(1.0) ? 1.0 : 2.0); return y; }\n",
            &program,
            &value_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-UNARY-CALL-TERNARY-ASSIGN-INT-REJECT should have failed\n");
        value_ssa_program_free(&program);
        return 0;
    }
    if (strstr(value_error.message, "SEMA-TYPE-006") == NULL) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-UNARY-CALL-TERNARY-ASSIGN-INT-REJECT mismatch: %s\n",
            value_error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    value_ssa_program_free(&program);
    return 1;
}

static int test_machine_ir_rejects_recursive_float_call_result_in_initializer_under_extension(void) {
    ValueSsaProgram program;
    ValueSsaError value_error;

    value_ssa_program_init(&program);
    memset(&value_error, 0, sizeof(value_error));

    if (build_value_ssa_program_from_extension_source_text(
            "float add3(float a, float b, float c){ return (a + b) + c; }\n"
            "int x = add3(1.0, 2.0, 3.0);\n"
            "int main(){ return 0; }\n",
            &program,
            &value_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-RECURSIVE-CALL-INIT-INT-REJECT should have failed\n");
        value_ssa_program_free(&program);
        return 0;
    }
    if (strstr(value_error.message, "SEMA-TYPE-004") == NULL) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-RECURSIVE-CALL-INIT-INT-REJECT mismatch: %s\n",
            value_error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    value_ssa_program_free(&program);
    return 1;
}

static int test_machine_ir_rejects_recursive_float_call_argument_to_int_under_extension(void) {
    ValueSsaProgram program;
    ValueSsaError value_error;

    value_ssa_program_init(&program);
    memset(&value_error, 0, sizeof(value_error));

    if (build_value_ssa_program_from_extension_source_text(
            "int sink(int x){ return x; }\n"
            "float add3(float a, float b, float c){ return (a + b) + c; }\n"
            "int main(){ return sink(add3(1.0, 2.0, 3.0)); }\n",
            &program,
            &value_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-RECURSIVE-CALL-CALLARG-INT-REJECT should have failed\n");
        value_ssa_program_free(&program);
        return 0;
    }
    if (strstr(value_error.message, "SEMA-TYPE-003") == NULL) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-RECURSIVE-CALL-CALLARG-INT-REJECT mismatch: %s\n",
            value_error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    value_ssa_program_free(&program);
    return 1;
}

static int test_machine_ir_rejects_float_ternary_value_call_argument_to_int_under_extension(void) {
    ValueSsaProgram program;
    ValueSsaError value_error;

    value_ssa_program_init(&program);
    memset(&value_error, 0, sizeof(value_error));

    if (build_value_ssa_program_from_extension_source_text(
            "int sink(int x){ return x; }\n"
            "float g = 1.25;\n"
            "float h = 2.5;\n"
            "int main(){ return sink((g ? h : h)); }\n",
            &program,
            &value_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-TERNARY-VALUE-CALLARG-INT-REJECT should have failed\n");
        value_ssa_program_free(&program);
        return 0;
    }
    if (strstr(value_error.message, "SEMA-TYPE-003") == NULL) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-TERNARY-VALUE-CALLARG-INT-REJECT mismatch: %s\n",
            value_error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    value_ssa_program_free(&program);
    return 1;
}

static int test_machine_ir_rejects_unary_call_ternary_value_call_argument_to_int_under_extension(void) {
    ValueSsaProgram program;
    ValueSsaError value_error;

    value_ssa_program_init(&program);
    memset(&value_error, 0, sizeof(value_error));

    if (build_value_ssa_program_from_extension_source_text(
            "int sink(int x){ return x; }\n"
            "float id(float x){ return x; }\n"
            "int main(){ return sink((-id(1.0) ? 1.0 : 2.0)); }\n",
            &program,
            &value_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-UNARY-CALL-TERNARY-CALLARG-INT-REJECT should have failed\n");
        value_ssa_program_free(&program);
        return 0;
    }
    if (strstr(value_error.message, "SEMA-TYPE-003") == NULL) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-UNARY-CALL-TERNARY-CALLARG-INT-REJECT mismatch: %s\n",
            value_error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    value_ssa_program_free(&program);
    return 1;
}

static int test_machine_ir_rejects_float_ternary_value_initializer_to_int_under_extension(void) {
    ValueSsaProgram program;
    ValueSsaError value_error;

    value_ssa_program_init(&program);
    memset(&value_error, 0, sizeof(value_error));

    if (build_value_ssa_program_from_extension_source_text(
            "float g = 1.25;\n"
            "float h = 2.5;\n"
            "int x = (g ? h : h);\n"
            "int main(){ return 0; }\n",
            &program,
            &value_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-TERNARY-VALUE-INIT-INT-REJECT should have failed\n");
        value_ssa_program_free(&program);
        return 0;
    }
    if (strstr(value_error.message, "SEMA-TYPE-004") == NULL) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-TERNARY-VALUE-INIT-INT-REJECT mismatch: %s\n",
            value_error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    value_ssa_program_free(&program);
    return 1;
}

static int test_machine_ir_rejects_unary_call_ternary_value_initializer_to_int_under_extension(void) {
    ValueSsaProgram program;
    ValueSsaError value_error;

    value_ssa_program_init(&program);
    memset(&value_error, 0, sizeof(value_error));

    if (build_value_ssa_program_from_extension_source_text(
            "float id(float x){ return x; }\n"
            "int y = (-id(1.0) ? 1.0 : 2.0);\n"
            "int main(){ return 0; }\n",
            &program,
            &value_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-UNARY-CALL-TERNARY-INIT-INT-REJECT should have failed\n");
        value_ssa_program_free(&program);
        return 0;
    }
    if (strstr(value_error.message, "SEMA-TYPE-004") == NULL) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-UNARY-CALL-TERNARY-INIT-INT-REJECT mismatch: %s\n",
            value_error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    value_ssa_program_free(&program);
    return 1;
}

static int test_machine_ir_accepts_float_ternary_value_return_to_float_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float h = 1.25;\n"
        "float mainf(){ return g ? h : h; }\n"
        "int main(){ return 0; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_default_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-TERNARY-VALUE-RETURN-FLOAT-ACCEPT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "global.0 g init=0 runtime-init=1:float") ||
        !strstr(actual_text, "global.1 h init=0 runtime-init=1:float") ||
        !strstr(actual_text, "function mainf params=0 locals=0 spills=0") ||
        !strstr(actual_text, "reg.0(r0) = load global.1") ||
        !strstr(actual_text, "ret reg.0(r0)")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-TERNARY-VALUE-RETURN-FLOAT-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_accepts_float_ternary_value_assignment_to_float_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "float mainf(){ float y; y = (g ? h : h); return y; }\n"
        "int main(){ return 0; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_default_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-TERNARY-VALUE-ASSIGN-FLOAT-ACCEPT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "global.0 g init=0 runtime-init=1:float") ||
        !strstr(actual_text, "global.1 h init=0 runtime-init=1:float") ||
        !strstr(actual_text, "function mainf params=0 locals=1 spills=0") ||
        !strstr(actual_text, "reg.0(r0) = load global.1") ||
        !strstr(actual_text, "ret reg.0(r0)")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-TERNARY-VALUE-ASSIGN-FLOAT-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_accepts_float_ternary_value_initializer_to_float_under_extension(void) {
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_default_value_ssa_program_from_extension_source_text(
            "float g = 1.25;\n"
            "float h = 2.5;\n"
            "float y = (g ? h : h);\n"
            "int main(){ return 0; }\n",
            &program,
            &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-TERNARY-VALUE-INIT-FLOAT-ACCEPT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "global.2 y") ||
        !strstr(actual_text, "function __global.init") ||
        !strstr(actual_text, "store global.2")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-TERNARY-VALUE-INIT-FLOAT-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_accepts_chained_float_addition_assignment_to_float_under_extension(void) {
    static const char *source =
        "float f(float x, float y, float z){ float t; t = (x + y) + z; return t; }\n"
        "int main(){ return 0; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_default_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-CHAIN-ADD-ASSIGN-FLOAT-ACCEPT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function f params=3") ||
        !strstr(actual_text, "call __builtin_fadd32(") ||
        !strstr(actual_text, "ret reg.")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-CHAIN-ADD-ASSIGN-FLOAT-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_accepts_nested_float_mul_div_assignment_to_float_under_extension(void) {
    static const char *source =
        "float f(float a, float b, float c){ float t; t = -a * (b / c); return t; }\n"
        "int main(){ return 0; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_default_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-NESTED-MUL-DIV-ASSIGN-FLOAT-ACCEPT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function f params=3") ||
        !strstr(actual_text, "call __builtin_fdiv32(") ||
        !strstr(actual_text, "call __builtin_fmul32(") ||
        !strstr(actual_text, "ret reg.")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-NESTED-MUL-DIV-ASSIGN-FLOAT-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_accepts_chained_float_addition_initializer_to_float_under_extension(void) {
    static const char *source =
        "float f(float x, float y, float z){ float t = (x + y) + z; return t; }\n"
        "int main(){ return 0; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_default_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-CHAIN-ADD-INIT-FLOAT-ACCEPT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function f params=3") ||
        !strstr(actual_text, "call __builtin_fadd32(") ||
        !strstr(actual_text, "ret reg.")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-CHAIN-ADD-INIT-FLOAT-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_accepts_nested_float_mul_div_initializer_to_float_under_extension(void) {
    static const char *source =
        "float f(float a, float b, float c){ float t = -a * (b / c); return t; }\n"
        "int main(){ return 0; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_default_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-NESTED-MUL-DIV-INIT-FLOAT-ACCEPT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function f params=3") ||
        !strstr(actual_text, "call __builtin_fdiv32(") ||
        !strstr(actual_text, "call __builtin_fmul32(") ||
        !strstr(actual_text, "ret reg.")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-NESTED-MUL-DIV-INIT-FLOAT-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_accepts_float_ternary_value_call_argument_to_float_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "float id(float x){ return x; }\n"
        "float wrap(float x){ return id(x); }\n"
        "float get(){ return wrap((g ? h : h)); }\n"
        "int main(){ return 0; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_default_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-TERNARY-VALUE-CALLARG-FLOAT-ACCEPT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function get params=0 locals=0 spills=0") ||
        !strstr(actual_text, "reg.0(r0) = load global.0") ||
        !strstr(actual_text, "br reg.0(r0), bb.1, bb.2") ||
        !strstr(actual_text, "bb.1:\n    reg.0(r0) = load global.1\n    jmp bb.3") ||
        !strstr(actual_text, "bb.2:\n    reg.0(r0) = load global.1\n    jmp bb.3") ||
        !strstr(actual_text, "ret reg.0(r0)")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-TERNARY-VALUE-CALLARG-FLOAT-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_accepts_unary_call_float_ternary_value_call_argument_to_float_under_extension(void) {
    static const char *source =
        "float id(float x){ return x; }\n"
        "float wrap(float x){ return id(x); }\n"
        "float get(){ return wrap((-id(1.0) ? 1.0 : 2.0)); }\n"
        "int main(){ return 0; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_default_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-UNARY-CALL-TERNARY-CALLARG-FLOAT-ACCEPT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function get params=0 locals=0 spills=0") ||
        !strstr(actual_text, "bb.0:\n    jmp bb.1") ||
        !strstr(actual_text, "bb.1:\n    ret 1065353216")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-UNARY-CALL-TERNARY-CALLARG-FLOAT-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_accepts_chained_float_addition_call_argument_to_float_under_extension(void) {
    static const char *source =
        "float wrap(float x){ return x; }\n"
        "float get(float x, float y, float z){ return wrap((x + y) + z); }\n"
        "int main(){ return 0; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_default_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-CHAIN-ADD-CALLARG-FLOAT-ACCEPT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function get params=3") ||
        !strstr(actual_text, "call __builtin_fadd32(") ||
        !strstr(actual_text, "ret reg.")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-CHAIN-ADD-CALLARG-FLOAT-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_accepts_nested_float_mul_div_call_argument_to_float_under_extension(void) {
    static const char *source =
        "float wrap(float x){ return x; }\n"
        "float f(float a, float b, float c){ return wrap(-a * (b / c)); }\n"
        "int main(){ return 0; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_default_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-NESTED-MUL-DIV-CALLARG-FLOAT-ACCEPT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function f params=3") ||
        !strstr(actual_text, "call __builtin_fdiv32(") ||
        !strstr(actual_text, "call __builtin_fmul32(") ||
        !strstr(actual_text, "ret reg.")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-NESTED-MUL-DIV-CALLARG-FLOAT-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_rejects_float_ternary_value_compare_against_int_under_extension(void) {
    ValueSsaProgram program;
    ValueSsaError value_error;

    value_ssa_program_init(&program);
    memset(&value_error, 0, sizeof(value_error));

    if (build_value_ssa_program_from_extension_source_text(
            "float g = 1.25;\n"
            "float h = 2.5;\n"
            "int main(){ return (g ? h : h) == 0; }\n",
            &program,
            &value_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-TERNARY-VALUE-COMPARE-INT-REJECT should have failed\n");
        value_ssa_program_free(&program);
        return 0;
    }
    if (strstr(value_error.message, "SEMA-TYPE-007") == NULL) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-TERNARY-VALUE-COMPARE-INT-REJECT mismatch: %s\n",
            value_error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    value_ssa_program_free(&program);
    return 1;
}

static int test_machine_ir_rejects_unary_call_ternary_value_compare_against_int_under_extension(void) {
    ValueSsaProgram program;
    ValueSsaError value_error;

    value_ssa_program_init(&program);
    memset(&value_error, 0, sizeof(value_error));

    if (build_value_ssa_program_from_extension_source_text(
            "float id(float x){ return x; }\n"
            "int main(){ return (-id(1.0) ? 1.0 : 2.0) == 0; }\n",
            &program,
            &value_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-UNARY-CALL-TERNARY-COMPARE-INT-REJECT should have failed\n");
        value_ssa_program_free(&program);
        return 0;
    }
    if (strstr(value_error.message, "SEMA-TYPE-007") == NULL) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-UNARY-CALL-TERNARY-COMPARE-INT-REJECT mismatch: %s\n",
            value_error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    value_ssa_program_free(&program);
    return 1;
}

static int test_machine_ir_rejects_float_ternary_value_compare_against_float_under_extension(void) {
    ValueSsaProgram program;
    ValueSsaError value_error;

    value_ssa_program_init(&program);
    memset(&value_error, 0, sizeof(value_error));

    if (build_value_ssa_program_from_extension_source_text(
            "float g = 1.25;\n"
            "float h = 2.5;\n"
            "int eq(){ return (g ? h : h) == h; }\n"
            "int main(){ return 0; }\n",
            &program,
            &value_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-TERNARY-VALUE-COMPARE-FLOAT-REJECT should have failed\n");
        value_ssa_program_free(&program);
        return 0;
    }
    if (strstr(value_error.message, "SEMA-EXT-035") == NULL) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-TERNARY-VALUE-COMPARE-FLOAT-REJECT mismatch: %s\n",
            value_error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    value_ssa_program_free(&program);
    return 1;
}

static int test_machine_ir_rejects_unary_call_ternary_value_compare_against_float_under_extension(void) {
    ValueSsaProgram program;
    ValueSsaError value_error;

    value_ssa_program_init(&program);
    memset(&value_error, 0, sizeof(value_error));

    if (build_value_ssa_program_from_extension_source_text(
            "float id(float x){ return x; }\n"
            "int eq(float x){ return (-id(x) ? x : x) == x; }\n"
            "int main(){ return 0; }\n",
            &program,
            &value_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-UNARY-CALL-TERNARY-COMPARE-FLOAT-REJECT should have failed\n");
        value_ssa_program_free(&program);
        return 0;
    }
    if (strstr(value_error.message, "SEMA-EXT-035") == NULL) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-UNARY-CALL-TERNARY-COMPARE-FLOAT-REJECT mismatch: %s\n",
            value_error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    value_ssa_program_free(&program);
    return 1;
}

static int test_machine_ir_translation_only_float_report_query_surface(void) {
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    const MachineIrProgram *report_program = NULL;
    const MachineIrFunction *report_function = NULL;
    const MachineIrFunctionShapeSummary *shape = NULL;
    const MachineIrBlockShapeSummary *block_shape = NULL;
    size_t allocation_rounds = 0u;
    size_t rewrite_rounds = 0u;
    size_t register_count = 0u;
    size_t global_count = 0u;
    size_t function_count = 0u;
    size_t function_index = (size_t)-1;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_value_ssa_program_from_extension_source_text(
            "float g = 1.25;\nfloat id(float x){ return x; }\nint main(){ float x = 1.25; id(1.25); return 0; }\n",
            &program,
            &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: float report query setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!machine_ir_allocate_rewrite_report_get_summary(
            &report,
            &allocation_rounds,
            &rewrite_rounds,
            &register_count,
            &global_count,
            &function_count) ||
        allocation_rounds != 1u ||
        rewrite_rounds != 0u ||
        register_count != 8u ||
        global_count != 1u ||
        function_count != 3u) {
        fprintf(stderr, "[machine-ir] FAIL: float report summary query mismatch\n");
        ok = 0;
        goto cleanup;
    }

    if (!machine_ir_allocate_rewrite_report_get_program(&report, &report_program) ||
        !report_program ||
        report_program->global_count < 1u ||
        report_program->globals[0].value_type != AST_FUNCTION_RETURN_FLOAT ||
        !machine_ir_allocate_rewrite_report_get_function_by_name(&report, "id", &function_index, &report_function) ||
        function_index == (size_t)-1 ||
        !report_function ||
        report_function->parameter_count != 1u ||
        report_function->local_count < 1u ||
        report_function->locals[0].value_type != AST_FUNCTION_RETURN_FLOAT) {
        fprintf(stderr, "[machine-ir] FAIL: float report program/function query mismatch\n");
        ok = 0;
        goto cleanup;
    }

    if (!machine_ir_allocate_rewrite_report_get_function_shape_summary_artifact(&report, function_index, &shape) ||
        !shape ||
        shape->block_count != 1u ||
        shape->spill_slot_count != 0u ||
        !machine_ir_allocate_rewrite_report_get_block_shape_summary(&report, function_index, 0, &block_shape) ||
        !block_shape ||
        block_shape->block_id != 0u ||
        !block_shape->has_return_terminator) {
        fprintf(stderr, "[machine-ir] FAIL: float report shape query mismatch\n");
        ok = 0;
    }

cleanup:
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_translation_only_float_global_literal_report_query_surface(void) {
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    const MachineIrProgram *report_program = NULL;
    const MachineIrFunction *global_init_function = NULL;
    const MachineIrFunctionShapeSummary *shape = NULL;
    const MachineIrBlockShapeSummary *block_shape = NULL;
    size_t allocation_rounds = 0u;
    size_t rewrite_rounds = 0u;
    size_t register_count = 0u;
    size_t global_count = 0u;
    size_t function_count = 0u;
    size_t function_index = (size_t)-1;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_value_ssa_program_from_extension_source_text(
            "float g = 1.25;\nfloat id(float x){ return x; }\nint main(){ return 0; }\n",
            &program,
            &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: float global literal report query setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!machine_ir_allocate_rewrite_report_get_summary(
            &report,
            &allocation_rounds,
            &rewrite_rounds,
            &register_count,
            &global_count,
            &function_count) ||
        allocation_rounds != 1u ||
        rewrite_rounds != 0u ||
        global_count != 1u ||
        function_count != 3u ||
        !machine_ir_allocate_rewrite_report_get_program(&report, &report_program) ||
        !report_program ||
        report_program->global_count < 1u ||
        report_program->globals[0].value_type != AST_FUNCTION_RETURN_FLOAT ||
        !report_program->globals[0].has_runtime_initializer ||
        !machine_ir_allocate_rewrite_report_get_function_by_name(
            &report, "__global.init", &function_index, &global_init_function) ||
        !global_init_function ||
        !machine_ir_allocate_rewrite_report_get_function_shape_summary_artifact(&report, function_index, &shape) ||
        !shape ||
        shape->block_count != 1u ||
        shape->store_global_count == 0u ||
        !machine_ir_allocate_rewrite_report_get_block_shape_summary(&report, function_index, 0, &block_shape) ||
        !block_shape ||
        !block_shape->has_return_terminator ||
        block_shape->store_global_count == 0u) {
        fprintf(stderr, "[machine-ir] FAIL: float global literal report query mismatch\n");
        ok = 0;
    }

cleanup:
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_default_pipeline_preserves_live_extension_float_assignment_transport(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float id(float x){ return x; }\n"
        "float mainf(){ float y; y = id(g); return y; }\n"
        "int main(){ return 0; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_default_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-ASSIGN-LIVE-DEFAULT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "global.0 g init=0 runtime-init=1:float") ||
        !strstr(actual_text, "function mainf params=0 locals=1 spills=0") ||
        !strstr(actual_text, "reg.0(r0) = load global.0") ||
        !strstr(actual_text, "ret reg.0(r0)")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-ASSIGN-LIVE-DEFAULT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_default_pipeline_preserves_float_return_transport_from_global(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float get(){ return g; }\n"
        "int main(){ return 0; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_default_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-RETURN-GLOBAL-LIVE-DEFAULT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "global.0 g init=0 runtime-init=1:float") ||
        !strstr(actual_text, "function get params=0 locals=0 spills=0") ||
        !strstr(actual_text, "reg.0(r0) = load global.0") ||
        !strstr(actual_text, "ret reg.0(r0)")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-RETURN-GLOBAL-LIVE-DEFAULT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_default_pipeline_preserves_float_return_transport_from_global_call(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float id(float x){ return x; }\n"
        "float get(){ return id(g); }\n"
        "int main(){ return 0; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_default_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-RETURN-GLOBAL-CALL-LIVE-DEFAULT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "global.0 g init=0 runtime-init=1:float") ||
        !strstr(actual_text, "function id params=1 locals=1 spills=0") ||
        !strstr(actual_text, "function get params=0 locals=0 spills=0") ||
        !strstr(actual_text, "reg.0(r0) = load global.0") ||
        !strstr(actual_text, "ret reg.0(r0)")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-RETURN-GLOBAL-CALL-LIVE-DEFAULT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_default_pipeline_preserves_float_global_identifier_runtime_init(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float h = g;\n"
        "int main(){ return 0; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_default_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-GLOBAL-IDENT-INIT-DEFAULT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "global.0 g init=0 runtime-init=1:float") ||
        !strstr(actual_text, "global.1 h init=0 runtime-init=1:float") ||
        !strstr(actual_text, "store global.0, 1067450368") ||
        !strstr(actual_text, "store global.1, 1067450368")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-GLOBAL-IDENT-INIT-DEFAULT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_default_pipeline_preserves_float_global_call_runtime_init(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float id(float x){ return x; }\n"
        "float h = id(g);\n"
        "int main(){ return 0; }\n";
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));

    if (!build_default_value_ssa_program_from_extension_source_text(source, &program, &value_error) ||
        !machine_ir_build_translation_only_report(&program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-GLOBAL-CALL-INIT-DEFAULT setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "global.0 g init=0 runtime-init=1:float") ||
        !strstr(actual_text, "global.1 h init=0 runtime-init=1:float") ||
        !strstr(actual_text, "function id params=1 locals=1 spills=0") ||
        !strstr(actual_text, "store global.0, 1067450368") ||
        !strstr(actual_text, "store global.1, 1067450368")) {
        fprintf(stderr,
            "[machine-ir] FAIL: MACHINE-IR-FLOAT-GLOBAL-CALL-INIT-DEFAULT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_allocate_and_rewrite_keeps_rewritten_many_args_spill_shape_stable(void) {
    static const char *source =
        "int g0=4973,g1=1981,g2=8998,g3=6006,g4=3014,g5=22;\n"
        "int norm(int x){ while(x<0)x=x+10009; while(x>=10009)x=x-10009; return x; }\n"
        "int pred(int x){ return (norm(x + g0 + g1 + 7) % 3) != 1; }\n"
        "int side(int x){ if (((x + g2) & 1) == 0) g2 = norm(g2 + x + 13); else g3 = norm(g3 + x + g2 + 17); return norm(g2 + g3 + x); }\n"
        "int wide0(int a0,int a1,int a2,int a3,int a4,int a5,int a6,int a7,int a8,int a9,int a10,int a11){\n"
        "  int t = norm(a0+a1+a2+a3+a4+a5+a6+a7+a8+a9+a10+a11+g0+g1+g2+29);\n"
        "  if ((pred(t) && side(a0 + a5)) || pred(a11)) t = norm(t + g2 + g3); else t = norm(t + g4 + g5);\n"
        "  return t;\n"
        "}\n"
        "int wide1(int a0,int a1,int a2,int a3,int a4,int a5,int a6,int a7,int a8,int a9,int a10,int a11){\n"
        "  if (pred(wide0(a11,a10,a9,a8,a7,a6,a5,a4,a3,a2,a1,a0))) return 1;\n"
        "  return 0;\n"
        "}\n"
        "int main(){ return wide1(2131,9151,6162,3173,184,7204,2149,9169,6180,3191,202,7222); }\n";
    MachineIrAllocateRewriteReport report;
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);

    if (!build_value_ssa_program_from_source_text(source, &program, &value_error) ||
        !machine_ir_build_allocate_and_rewrite_program_single_block_spills_flat_program_only_report(
            &program, 8, 8, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: many-args spill-shape witness setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function wide1 params=12 locals=16 spills=4") ||
        !strstr(actual_text,
            "    spill.0 = load local.11\n"
            "    spill.1 = load local.10\n"
            "    spill.2 = load local.9\n"
            "    spill.3 = load local.8\n") ||
        !strstr(actual_text,
            "    reg.7(r7) = load local.7\n"
            "    reg.6(r6) = load local.6\n"
            "    reg.5(r5) = load local.5\n"
            "    reg.4(r4) = load local.4\n") ||
        !strstr(actual_text,
            "    reg.0(r0) = call wide0(spill.0, spill.1, spill.2, spill.3, reg.7(r7), reg.6(r6), reg.5(r5), reg.4(r4), reg.3(r3), reg.2(r2), reg.1(r1), reg.0(r0))\n") ||
        strstr(actual_text,
            "    reg.0(r0) = call wide0(reg.7(r7), reg.6(r6), reg.5(r5), reg.4(r4), reg.7(r7), reg.6(r6), reg.5(r5), reg.4(r4), reg.3(r3), reg.2(r2), reg.1(r1), reg.0(r0))\n")) {
        fprintf(stderr,
            "[machine-ir] FAIL: many-args spill-shape witness regressed\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_allocate_and_rewrite_protects_branch_condition_from_live_out_alias(void) {
    static const char *source =
        "int main(){\n"
        "  int a = 0;\n"
        "  int i = 0;\n"
        "  while (i < 5) {\n"
        "    a = a + 1;\n"
        "    i = i + 1;\n"
        "  }\n"
        "  return a;\n"
        "}\n";
    MachineIrAllocateRewriteReport report;
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrError machine_error;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);

    if (!build_value_ssa_program_from_source_text(source, &program, &value_error) ||
        !machine_ir_build_allocate_and_rewrite_program_single_block_spills_flat_program_only_report(
            &program, 1, 1, &report, &machine_error) ||
        !machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: branch-condition-protect witness setup failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if ((!strstr(actual_text,
             "  bb.1:\n"
             "    phi spill.0 = [bb.0: 0], [bb.2: spill.0]\n"
             "    phi spill.2 = [bb.0: 0], [bb.2: reg.0(r0)]\n"
             "    spill.1 = lt spill.0, 5\n"
             "    br spill.1, bb.2, bb.3\n") &&
            !strstr(actual_text,
                "  bb.1:\n"
                "    reg.0(r0) = load local.1\n"
                "    reg.0(r0) = lt reg.0(r0), 5\n"
                "    br reg.0(r0), bb.2, bb.3\n") &&
            !strstr(actual_text,
                "  bb.1:\n"
                "    spill.0 = load local.1\n"
                "    reg.0(r0) = lt spill.0, 5\n"
                "    br reg.0(r0), bb.2, bb.3\n")) ||
        !strstr(actual_text,
            "  bb.2:\n"
            "    reg.0(r0) = load local.0\n"
            "    reg.0(r0) = add reg.0(r0), 1\n"
            "    store local.0, reg.0(r0)\n")) {
        fprintf(stderr,
            "[machine-ir] FAIL: branch condition was not protected from live-out aliasing\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int build_machine_ir_phi_cycle_program(MachineIrProgram *program, MachineIrError *error) {
    MachineIrFunction *function = NULL;
    MachineIrBasicBlock *entry = NULL;
    MachineIrBasicBlock *left = NULL;
    MachineIrBasicBlock *right = NULL;
    MachineIrBasicBlock *join = NULL;
    MachineIrPhi *phi = NULL;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 2;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(2, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;
    program->register_bank.registers[1].register_id = 1;
    program->register_bank.registers[1].name = dup_text("r1");
    program->register_bank.registers[1].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, &entry, error) ||
        !machine_ir_function_append_block(function, NULL, &left, error) ||
        !machine_ir_function_append_block(function, NULL, &right, error) ||
        !machine_ir_function_append_block(function, NULL, &join, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    function->spill_slot_count = 1;

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_immediate(1);
    if (!machine_ir_block_append_instruction(entry, &instruction, error) ||
        !machine_ir_block_set_branch(entry, machine_ir_operand_register(0), 1, 2, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_immediate(10);
    if (!machine_ir_block_append_instruction(left, &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }
    instruction.result = machine_ir_operand_register(1);
    instruction.as.mov_value = machine_ir_operand_immediate(20);
    if (!machine_ir_block_append_instruction(left, &instruction, error) ||
        !machine_ir_block_set_jump(left, 3, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_immediate(30);
    if (!machine_ir_block_append_instruction(right, &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }
    instruction.result = machine_ir_operand_register(1);
    instruction.as.mov_value = machine_ir_operand_immediate(40);
    if (!machine_ir_block_append_instruction(right, &instruction, error) ||
        !machine_ir_block_set_jump(right, 3, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    if (!machine_ir_block_append_phi(join, machine_ir_operand_register(0), NULL, &phi, error) || !phi ||
        !machine_ir_phi_append_input(phi, 1, machine_ir_operand_register(1), error) ||
        !machine_ir_phi_append_input(phi, 2, machine_ir_operand_register(1), error)) {
        machine_ir_program_free(program);
        return 0;
    }
    if (!machine_ir_block_append_phi(join, machine_ir_operand_register(1), NULL, &phi, error) || !phi ||
        !machine_ir_phi_append_input(phi, 1, machine_ir_operand_register(0), error) ||
        !machine_ir_phi_append_input(phi, 2, machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }
    if (!machine_ir_block_set_return(join, machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int test_machine_ir_phi_elimination_dump(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_phi_cycle_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: phi-elim setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_eliminate_phi_nodes_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: phi-elim dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (strstr(actual_text, "phi ") ||
        !strstr(actual_text, "spill.1 = mov reg.1(r1)") ||
        !strstr(actual_text, "reg.0(r0) = mov spill.1") ||
        !strstr(actual_text, "reg.1(r1) = mov reg.0(r0)") ||
        !strstr(actual_text, "bb.1:\n    reg.0(r0) = mov 10\n    reg.1(r1) = mov 20\n") ||
        !strstr(actual_text, "bb.2:\n    reg.0(r0) = mov 30\n    reg.1(r1) = mov 40\n") ||
        !strstr(actual_text, "jmp bb.3")) {
        fprintf(stderr,
            "[machine-ir] FAIL: phi-elim dump missing expected split/cycle shape\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_phi_eliminated_report_surface(void) {
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    const size_t *phi_functions = NULL;
    const MachineIrFunctionShapeSummary *shape = NULL;
    const MachineIrBlockShapeSummary *block_shape = NULL;
    MachineIrError machine_error;
    char *actual_text = NULL;
    size_t phi_function_count = 0;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);

    if (!build_phi_spill_program(&program, &value_error) ||
        !machine_ir_build_allocate_and_rewrite_program_single_block_spills_phi_eliminated_flat_report(
            &program, 2, 2, &report, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: phi-eliminated report build failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!machine_ir_allocate_rewrite_report_get_functions_with_phi(
            &report, &phi_function_count, &phi_functions) ||
        phi_function_count != 0) {
        fprintf(stderr, "[machine-ir] FAIL: phi-eliminated report still reports phi-bearing functions\n");
        ok = 0;
        goto cleanup;
    }
    if (!machine_ir_allocate_rewrite_report_get_function_shape_summary_artifact(&report, 0, &shape) ||
        !shape || shape->phi_count != 0 || shape->blocks_with_phi_count != 0) {
        fprintf(stderr, "[machine-ir] FAIL: phi-eliminated function summary still reports phis\n");
        ok = 0;
        goto cleanup;
    }
    if (!machine_ir_allocate_rewrite_report_get_block_shape_summary(&report, 0, 3, &block_shape) ||
        !block_shape || block_shape->phi_count != 0) {
        fprintf(stderr, "[machine-ir] FAIL: phi-eliminated block summary still reports phis\n");
        ok = 0;
        goto cleanup;
    }
    if (!machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: phi-eliminated report dump failed: %s\n",
            machine_error.message);
        ok = 0;
        goto cleanup;
    }
    if (strstr(actual_text, "phi reg.") ||
        !strstr(actual_text, "phi_funcs=0") ||
        !strstr(actual_text, "functions-with-phi:") ||
        !strstr(actual_text, "fn.0 main blocks=4 phi=0")) {
        fprintf(stderr,
            "[machine-ir] FAIL: phi-eliminated report dump mismatch\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int build_machine_ir_trivial_phi_cleanup_program(MachineIrProgram *program, MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t left_id;
    size_t right_id;
    size_t join_id;
    size_t exit_id;
    MachineIrPhi *phi = NULL;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &left_id, NULL, error) ||
        !machine_ir_function_append_block(function, &right_id, NULL, error) ||
        !machine_ir_function_append_block(function, &join_id, NULL, error) ||
        !machine_ir_function_append_block(function, &exit_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_immediate(1);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !machine_ir_block_set_branch(&function->blocks[entry_id], machine_ir_operand_register(0), 1, 2, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    instruction.as.mov_value = machine_ir_operand_immediate(10);
    if (!machine_ir_block_append_instruction(&function->blocks[left_id], &instruction, error) ||
        !machine_ir_block_set_jump(&function->blocks[left_id], 3, error) ||
        !machine_ir_block_append_instruction(&function->blocks[right_id], &instruction, error) ||
        !machine_ir_block_set_jump(&function->blocks[right_id], 3, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    if (!machine_ir_block_append_phi(&function->blocks[join_id], machine_ir_operand_register(0), NULL, &phi, error) || !phi ||
        !machine_ir_phi_append_input(phi, 1, machine_ir_operand_register(0), error) ||
        !machine_ir_phi_append_input(phi, 2, machine_ir_operand_register(0), error) ||
        !machine_ir_block_set_jump(&function->blocks[join_id], 4, error) ||
        !machine_ir_block_set_return(&function->blocks[exit_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int test_machine_ir_cleanup_after_phi_elimination_dump(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_trivial_phi_cleanup_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: post-phi cleanup setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: post-phi cleanup dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (strstr(actual_text, "phi ") ||
        strstr(actual_text, "reg.0(r0) = mov reg.0(r0)") ||
        strstr(actual_text, "bb.1:\n") ||
        !strstr(actual_text, "bb.0:\n    ret 10\n")) {
        fprintf(stderr,
            "[machine-ir] FAIL: post-phi cleanup dump missing expected cleaned shape\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_canonicalize_program_dump_matches_legacy_cleanup_dump(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *canonical_text = NULL;
    char *legacy_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_trivial_phi_cleanup_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: canonicalize setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_canonicalize_program_dump(&program, &canonical_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: canonicalize dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &legacy_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: legacy cleanup dump failed: %s\n", error.message);
        free(canonical_text);
        machine_ir_program_free(&program);
        return 0;
    }
    if (strcmp(canonical_text, legacy_text) != 0) {
        fprintf(stderr,
            "[machine-ir] FAIL: canonicalize dump should match legacy cleanup dump\ncanonical:\n%s\nlegacy:\n%s\n",
            canonical_text,
            legacy_text);
        ok = 0;
    }

    free(canonical_text);
    free(legacy_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleaned_report_surface(void) {
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    const size_t *phi_functions = NULL;
    const MachineIrFunctionShapeSummary *shape = NULL;
    MachineIrError machine_error;
    char *actual_text = NULL;
    size_t phi_function_count = 0;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);

    if (!build_phi_spill_program(&program, &value_error) ||
        !machine_ir_build_allocate_and_rewrite_program_single_block_spills_cleaned_flat_report(
            &program, 2, 2, &report, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: cleaned report build failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!machine_ir_allocate_rewrite_report_get_functions_with_phi(
            &report, &phi_function_count, &phi_functions) ||
        phi_function_count != 0) {
        fprintf(stderr, "[machine-ir] FAIL: cleaned report still reports phi-bearing functions\n");
        ok = 0;
        goto cleanup;
    }
    if (!machine_ir_allocate_rewrite_report_get_function_shape_summary_artifact(&report, 0, &shape) ||
        !shape ||
        shape->phi_count != 0 ||
        shape->blocks_with_phi_count != 0 ||
        shape->block_count != 1) {
        fprintf(stderr, "[machine-ir] FAIL: cleaned report summary mismatch\n");
        ok = 0;
        goto cleanup;
    }
    if (!machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: cleaned report dump failed: %s\n",
            machine_error.message);
        ok = 0;
        goto cleanup;
    }
    if (strstr(actual_text, "phi reg.") ||
        !strstr(actual_text, "phi_funcs=0") ||
        !strstr(actual_text, "fn.0 main blocks=1 phi=0")) {
        fprintf(stderr,
            "[machine-ir] FAIL: cleaned report dump mismatch\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_machine_ir_canonicalized_report_surface(void) {
    ValueSsaProgram program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    const size_t *phi_functions = NULL;
    const MachineIrFunctionShapeSummary *shape = NULL;
    MachineIrError machine_error;
    char *actual_text = NULL;
    size_t phi_function_count = 0;
    int ok = 1;

    value_ssa_program_init(&program);
    machine_ir_allocate_rewrite_report_init(&report);

    if (!build_phi_spill_program(&program, &value_error) ||
        !machine_ir_build_allocate_and_rewrite_program_single_block_spills_canonicalized_flat_report(
            &program, 2, 2, &report, &machine_error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: canonicalized report build failed: %s\n",
            machine_error.message[0] ? machine_error.message : value_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!machine_ir_allocate_rewrite_report_get_functions_with_phi(
            &report, &phi_function_count, &phi_functions) ||
        phi_function_count != 0) {
        fprintf(stderr, "[machine-ir] FAIL: canonicalized report still reports phi-bearing functions\n");
        ok = 0;
        goto cleanup;
    }
    if (!machine_ir_allocate_rewrite_report_get_function_shape_summary_artifact(&report, 0, &shape) ||
        !shape ||
        shape->phi_count != 0 ||
        shape->blocks_with_phi_count != 0 ||
        shape->block_count != 1) {
        fprintf(stderr, "[machine-ir] FAIL: canonicalized report shape summary mismatch\n");
        ok = 0;
        goto cleanup;
    }
    if (!machine_ir_dump_allocate_rewrite_report(&report, &actual_text, &machine_error)) {
        fprintf(stderr, "[machine-ir] FAIL: canonicalized report dump failed: %s\n", machine_error.message);
        ok = 0;
        goto cleanup;
    }
    if (strstr(actual_text, "phi ") != NULL) {
        fprintf(stderr, "[machine-ir] FAIL: canonicalized report dump still contains phi\n");
        ok = 0;
        goto cleanup;
    }
    if (!strstr(actual_text, "phi_funcs=0") ||
        !strstr(actual_text, "fn.0 main blocks=1 phi=0")) {
        fprintf(stderr, "[machine-ir] FAIL: canonicalized report dump summary mismatch\nactual:\n%s\n", actual_text);
        ok = 0;
        goto cleanup;
    }

cleanup:
    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int build_machine_ir_same_target_branch_cleanup_program(MachineIrProgram *program, MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t join_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &join_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_immediate(1);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !machine_ir_block_set_branch(&function->blocks[entry_id], machine_ir_operand_register(0), 1, 1, error) ||
        !machine_ir_block_set_return(&function->blocks[join_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_immediate_branch_cleanup_program(MachineIrProgram *program, MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t then_id;
    size_t else_id;
    size_t exit_id;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &then_id, NULL, error) ||
        !machine_ir_function_append_block(function, &else_id, NULL, error) ||
        !machine_ir_function_append_block(function, &exit_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    if (!machine_ir_block_set_branch(&function->blocks[entry_id], machine_ir_operand_immediate(0), 1, 2, error) ||
        !machine_ir_block_set_jump(&function->blocks[then_id], 3, error) ||
        !machine_ir_block_set_jump(&function->blocks[else_id], 3, error) ||
        !machine_ir_block_set_return(&function->blocks[exit_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int test_machine_ir_cleanup_simplifies_same_target_branch(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_same_target_branch_cleanup_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: same-target cleanup setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: same-target cleanup dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (strstr(actual_text, "br ") ||
        strstr(actual_text, "bb.1:\n") ||
        !strstr(actual_text, "bb.0:\n    ret 1\n")) {
        fprintf(stderr,
            "[machine-ir] FAIL: same-target branch cleanup mismatch\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_simplifies_immediate_branch_and_compacts(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_immediate_branch_cleanup_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: immediate-branch cleanup setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: immediate-branch cleanup dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (strstr(actual_text, "br ") ||
        strstr(actual_text, "bb.1:\n") ||
        !strstr(actual_text, "bb.0:\n    ret reg.0(r0)\n")) {
        fprintf(stderr,
            "[machine-ir] FAIL: immediate-branch cleanup/compaction mismatch\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int build_machine_ir_jump_thread_program(MachineIrProgram *program, MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t middle_id;
    size_t exit_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &middle_id, NULL, error) ||
        !machine_ir_function_append_block(function, &exit_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_immediate(1);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !machine_ir_block_set_branch(&function->blocks[entry_id], machine_ir_operand_register(0), 1, 1, error) ||
        !machine_ir_block_set_jump(&function->blocks[middle_id], 2, error) ||
        !machine_ir_block_set_return(&function->blocks[exit_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_linear_merge_program(MachineIrProgram *program, MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t middle_id;
    size_t exit_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 2;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(2, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;
    program->register_bank.registers[1].register_id = 1;
    program->register_bank.registers[1].name = dup_text("r1");
    program->register_bank.registers[1].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &middle_id, NULL, error) ||
        !machine_ir_function_append_block(function, &exit_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_immediate(7);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !machine_ir_block_set_jump(&function->blocks[entry_id], 1, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    instruction.result = machine_ir_operand_register(1);
    instruction.as.mov_value = machine_ir_operand_register(0);
    if (!machine_ir_block_append_instruction(&function->blocks[middle_id], &instruction, error) ||
        !machine_ir_block_set_jump(&function->blocks[middle_id], 2, error) ||
        !machine_ir_block_set_return(&function->blocks[exit_id], machine_ir_operand_register(1), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int test_machine_ir_cleanup_threads_jump_targets(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_jump_thread_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: jump-thread cleanup setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: jump-thread cleanup dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (strstr(actual_text, "bb.2:\n") ||
        strstr(actual_text, "br ") ||
        strstr(actual_text, "bb.1:\n") ||
        !strstr(actual_text, "bb.0:\n    ret 1\n")) {
        fprintf(stderr,
            "[machine-ir] FAIL: jump-thread cleanup mismatch\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_merges_linear_jump_blocks(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_linear_merge_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: linear-merge cleanup setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: linear-merge cleanup dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (strstr(actual_text, "bb.2:\n") ||
        !strstr(actual_text, "bb.0:\n    ret 7\n")) {
        fprintf(stderr,
            "[machine-ir] FAIL: linear-merge cleanup mismatch\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int build_machine_ir_local_copy_cleanup_program(MachineIrProgram *program, MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 2;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(2, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;
    program->register_bank.registers[1].register_id = 1;
    program->register_bank.registers[1].name = dup_text("r1");
    program->register_bank.registers[1].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_immediate(7);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    instruction.result = machine_ir_operand_register(1);
    instruction.as.mov_value = machine_ir_operand_register(0);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(1);
    instruction.as.binary.rhs = machine_ir_operand_immediate(1);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[entry_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_dead_move_cleanup_program(MachineIrProgram *program, MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 2;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(2, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;
    program->register_bank.registers[1].register_id = 1;
    program->register_bank.registers[1].name = dup_text("r1");
    program->register_bank.registers[1].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_immediate(5);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    instruction.result = machine_ir_operand_register(1);
    instruction.as.mov_value = machine_ir_operand_register(0);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[entry_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int test_machine_ir_cleanup_propagates_local_copies(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_local_copy_cleanup_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: local-copy cleanup setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: local-copy cleanup dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (strstr(actual_text, "reg.1(r1) = mov reg.0(r0)") ||
        !strstr(actual_text,
            "bb.0:\n"
            "    ret 8\n")) {
        fprintf(stderr,
            "[machine-ir] FAIL: local-copy cleanup propagation mismatch\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_eliminates_dead_moves(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_dead_move_cleanup_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: dead-move cleanup setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: dead-move cleanup dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (strstr(actual_text, "reg.1(r1) = mov reg.0(r0)") ||
        !strstr(actual_text,
            "bb.0:\n"
            "    ret 5\n")) {
        fprintf(stderr,
            "[machine-ir] FAIL: dead-move cleanup mismatch\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int build_machine_ir_indirect_load_address_use_program(MachineIrProgram *program, MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 4;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(4, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    for (size_t i = 0; i < 4; ++i) {
        program->register_bank.registers[i].register_id = i;
        program->register_bank.registers[i].name = dup_text(i == 0 ? "r0" : i == 1 ? "r1" : i == 2 ? "r2" : "r3");
        program->register_bank.registers[i].allocatable = 1u;
    }

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 0, NULL, error) ||
        !machine_ir_function_append_local(function, "b", 0, NULL, error) ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_LOCAL;
    instruction.as.store.slot = machine_ir_slot_local(0);
    instruction.as.store.value = machine_ir_operand_immediate(7);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }
    instruction.as.store.slot = machine_ir_slot_local(1);
    instruction.as.store.value = machine_ir_operand_immediate(9);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_ADDR_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.addr_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1);
    instruction.as.mov_value = machine_ir_operand_immediate(4);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(2);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_register(1);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_INDIRECT;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(3);
    instruction.as.load_indirect_addr = machine_ir_operand_register(2);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[entry_id], machine_ir_operand_register(3), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int test_machine_ir_cleanup_preserves_indirect_load_address_chain(void) {
    MachineIrProgram program;
    MachineIrError error;
    MachineIrBasicBlock *block = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    memset(&error, 0, sizeof(error));
    if (!build_machine_ir_indirect_load_address_use_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: indirect-load-address cleanup setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: indirect-load-address cleanup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }

    block = &program.functions[0].blocks[0];
    if (block->instruction_count < 5 ||
        block->instructions[3].kind != MACHINE_IR_INSTR_BINARY ||
        block->instructions[3].result.kind != MACHINE_IR_OPERAND_REGISTER ||
        block->instructions[3].result.machine_register_id != 2u ||
        block->instructions[4].kind != MACHINE_IR_INSTR_LOAD_INDIRECT ||
        block->instructions[4].as.load_indirect_addr.kind != MACHINE_IR_OPERAND_REGISTER ||
        block->instructions[4].as.load_indirect_addr.machine_register_id != 2u) {
        fprintf(stderr,
            "[machine-ir] FAIL: indirect load address chain should stay live through cleanup\n");
        ok = 0;
    }

    machine_ir_program_free(&program);
    return ok;
}

static int build_machine_ir_indirect_store_use_program(MachineIrProgram *program, MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 4;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(4, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    for (size_t i = 0; i < 4; ++i) {
        program->register_bank.registers[i].register_id = i;
        program->register_bank.registers[i].name = dup_text(i == 0 ? "r0" : i == 1 ? "r1" : i == 2 ? "r2" : "r3");
        program->register_bank.registers[i].allocatable = 1u;
    }

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_local(function, "src", 0, NULL, error) ||
        !machine_ir_function_append_local(function, "dst0", 0, NULL, error) ||
        !machine_ir_function_append_local(function, "dst1", 0, NULL, error) ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_LOCAL;
    instruction.as.store.slot = machine_ir_slot_local(0);
    instruction.as.store.value = machine_ir_operand_immediate(11);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_ADDR_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1);
    instruction.as.addr_slot = machine_ir_slot_local(1);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(2);
    instruction.as.mov_value = machine_ir_operand_immediate(4);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(3);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(1);
    instruction.as.binary.rhs = machine_ir_operand_register(2);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_INDIRECT;
    instruction.as.store_indirect.addr = machine_ir_operand_register(3);
    instruction.as.store_indirect.value = machine_ir_operand_register(0);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[entry_id], machine_ir_operand_immediate(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int test_machine_ir_cleanup_preserves_indirect_store_inputs(void) {
    MachineIrProgram program;
    MachineIrError error;
    MachineIrBasicBlock *block = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    memset(&error, 0, sizeof(error));
    if (!build_machine_ir_indirect_store_use_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: indirect-store cleanup setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: indirect-store cleanup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }

    block = &program.functions[0].blocks[0];
    if (block->instruction_count < 5 ||
        !block->instructions[1].has_result ||
        block->instructions[1].result.kind != MACHINE_IR_OPERAND_REGISTER ||
        block->instructions[1].result.machine_register_id != 0u ||
        block->instructions[3].kind != MACHINE_IR_INSTR_BINARY ||
        block->instructions[3].result.kind != MACHINE_IR_OPERAND_REGISTER ||
        block->instructions[3].result.machine_register_id != 3u ||
        block->instructions[4].kind != MACHINE_IR_INSTR_STORE_INDIRECT ||
        block->instructions[4].as.store_indirect.addr.kind != MACHINE_IR_OPERAND_REGISTER ||
        block->instructions[4].as.store_indirect.addr.machine_register_id != 3u ||
        block->instructions[4].as.store_indirect.value.kind != MACHINE_IR_OPERAND_REGISTER ||
        block->instructions[4].as.store_indirect.value.machine_register_id != 0u) {
        fprintf(stderr,
            "[machine-ir] FAIL: indirect store address/value inputs should stay live through cleanup\n");
        ok = 0;
    }

    machine_ir_program_free(&program);
    return ok;
}

static int build_machine_ir_cross_block_indirect_load_use_program(MachineIrProgram *program, MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t tail_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 4;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(4, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    for (size_t i = 0; i < 4; ++i) {
        program->register_bank.registers[i].register_id = i;
        program->register_bank.registers[i].name = dup_text(i == 0 ? "r0" : i == 1 ? "r1" : i == 2 ? "r2" : "r3");
        program->register_bank.registers[i].allocatable = 1u;
    }

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 0, NULL, error) ||
        !machine_ir_function_append_local(function, "b", 0, NULL, error) ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &tail_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_LOCAL;
    instruction.as.store.slot = machine_ir_slot_local(0);
    instruction.as.store.value = machine_ir_operand_immediate(7);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }
    instruction.as.store.slot = machine_ir_slot_local(1);
    instruction.as.store.value = machine_ir_operand_immediate(9);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_ADDR_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.addr_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1);
    instruction.as.mov_value = machine_ir_operand_immediate(4);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !machine_ir_block_set_jump(&function->blocks[entry_id], tail_id, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(2);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_register(1);
    if (!machine_ir_block_append_instruction(&function->blocks[tail_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_INDIRECT;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(3);
    instruction.as.load_indirect_addr = machine_ir_operand_register(2);
    if (!machine_ir_block_append_instruction(&function->blocks[tail_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[tail_id], machine_ir_operand_register(3), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int test_machine_ir_cleanup_preserves_cross_block_indirect_load_address_chain(void) {
    MachineIrProgram program;
    MachineIrError error;
    int ok = 1;
    int saw_binary_addr = 0;
    int saw_indirect_load = 0;

    machine_ir_program_init(&program);
    memset(&error, 0, sizeof(error));
    if (!build_machine_ir_cross_block_indirect_load_use_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: cross-block indirect-load setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: cross-block indirect-load cleanup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }

    for (size_t block_index = 0; block_index < program.functions[0].block_count; ++block_index) {
        MachineIrBasicBlock *block = &program.functions[0].blocks[block_index];

        for (size_t instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
            MachineIrInstruction *instruction = &block->instructions[instruction_index];

            if (instruction->kind == MACHINE_IR_INSTR_BINARY &&
                instruction->has_result &&
                instruction->result.kind == MACHINE_IR_OPERAND_REGISTER &&
                instruction->result.machine_register_id == 2u) {
                saw_binary_addr = 1;
            }
            if (instruction->kind == MACHINE_IR_INSTR_LOAD_INDIRECT &&
                instruction->as.load_indirect_addr.kind == MACHINE_IR_OPERAND_REGISTER &&
                instruction->as.load_indirect_addr.machine_register_id == 2u) {
                saw_indirect_load = 1;
            }
        }
    }

    if (!saw_binary_addr || !saw_indirect_load) {
        fprintf(stderr,
            "[machine-ir] FAIL: cross-block indirect load address chain should stay live through cleanup\n");
        ok = 0;
    }

    machine_ir_program_free(&program);
    return ok;
}

static int build_machine_ir_cross_block_indirect_store_use_program(MachineIrProgram *program, MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t tail_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 4;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(4, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    for (size_t i = 0; i < 4; ++i) {
        program->register_bank.registers[i].register_id = i;
        program->register_bank.registers[i].name = dup_text(i == 0 ? "r0" : i == 1 ? "r1" : i == 2 ? "r2" : "r3");
        program->register_bank.registers[i].allocatable = 1u;
    }

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_local(function, "src", 0, NULL, error) ||
        !machine_ir_function_append_local(function, "dst0", 0, NULL, error) ||
        !machine_ir_function_append_local(function, "dst1", 0, NULL, error) ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &tail_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_LOCAL;
    instruction.as.store.slot = machine_ir_slot_local(0);
    instruction.as.store.value = machine_ir_operand_immediate(11);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_ADDR_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1);
    instruction.as.addr_slot = machine_ir_slot_local(1);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(2);
    instruction.as.mov_value = machine_ir_operand_immediate(4);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !machine_ir_block_set_jump(&function->blocks[entry_id], tail_id, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(3);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(1);
    instruction.as.binary.rhs = machine_ir_operand_register(2);
    if (!machine_ir_block_append_instruction(&function->blocks[tail_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_INDIRECT;
    instruction.as.store_indirect.addr = machine_ir_operand_register(3);
    instruction.as.store_indirect.value = machine_ir_operand_register(0);
    if (!machine_ir_block_append_instruction(&function->blocks[tail_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[tail_id], machine_ir_operand_immediate(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int test_machine_ir_cleanup_preserves_cross_block_indirect_store_inputs(void) {
    MachineIrProgram program;
    MachineIrError error;
    int ok = 1;
    int saw_value_def_reg0 = 0;
    int saw_binary_addr = 0;
    int saw_store_indirect = 0;
    int store_uses_loaded_reg = 0;
    int store_uses_folded_immediate = 0;

    machine_ir_program_init(&program);
    memset(&error, 0, sizeof(error));
    if (!build_machine_ir_cross_block_indirect_store_use_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: cross-block indirect-store setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: cross-block indirect-store cleanup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }

    for (size_t block_index = 0; block_index < program.functions[0].block_count; ++block_index) {
        MachineIrBasicBlock *block = &program.functions[0].blocks[block_index];

        for (size_t instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
            MachineIrInstruction *instruction = &block->instructions[instruction_index];

            if (((instruction->kind == MACHINE_IR_INSTR_LOAD_LOCAL) ||
                    (instruction->kind == MACHINE_IR_INSTR_MOV &&
                        instruction->as.mov_value.kind == MACHINE_IR_OPERAND_IMMEDIATE &&
                        instruction->as.mov_value.immediate == 11)) &&
                instruction->has_result &&
                instruction->result.kind == MACHINE_IR_OPERAND_REGISTER &&
                instruction->result.machine_register_id == 0u) {
                saw_value_def_reg0 = 1;
            }
            if (instruction->kind == MACHINE_IR_INSTR_BINARY &&
                instruction->has_result &&
                instruction->result.kind == MACHINE_IR_OPERAND_REGISTER &&
                instruction->result.machine_register_id == 3u) {
                saw_binary_addr = 1;
            }
            if (instruction->kind == MACHINE_IR_INSTR_STORE_INDIRECT &&
                instruction->as.store_indirect.addr.kind == MACHINE_IR_OPERAND_REGISTER &&
                instruction->as.store_indirect.addr.machine_register_id == 3u) {
                saw_store_indirect = 1;
                if (instruction->as.store_indirect.value.kind == MACHINE_IR_OPERAND_REGISTER &&
                    instruction->as.store_indirect.value.machine_register_id == 0u) {
                    store_uses_loaded_reg = 1;
                }
                if (instruction->as.store_indirect.value.kind == MACHINE_IR_OPERAND_IMMEDIATE &&
                    instruction->as.store_indirect.value.immediate == 11) {
                    store_uses_folded_immediate = 1;
                }
            }
        }
    }

    if (!saw_binary_addr ||
        !saw_store_indirect ||
        !(store_uses_folded_immediate || (store_uses_loaded_reg && saw_value_def_reg0))) {
        fprintf(stderr,
            "[machine-ir] FAIL: cross-block indirect store address/value inputs should stay live through cleanup\n");
        ok = 0;
    }

    machine_ir_program_free(&program);
    return ok;
}

static int build_machine_ir_straight_line_cross_block_copy_program(MachineIrProgram *program, MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t body_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 2;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(2, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;
    program->register_bank.registers[1].register_id = 1;
    program->register_bank.registers[1].name = dup_text("r1");
    program->register_bank.registers[1].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &body_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_immediate(9);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    instruction.result = machine_ir_operand_register(1);
    instruction.as.mov_value = machine_ir_operand_register(0);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !machine_ir_block_set_jump(&function->blocks[entry_id], 1, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(1);
    instruction.as.binary.rhs = machine_ir_operand_immediate(2);
    if (!machine_ir_block_append_instruction(&function->blocks[body_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[body_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_join_stops_cross_block_copy_program(MachineIrProgram *program, MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t left_id;
    size_t right_id;
    size_t join_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 2;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(2, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;
    program->register_bank.registers[1].register_id = 1;
    program->register_bank.registers[1].name = dup_text("r1");
    program->register_bank.registers[1].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &left_id, NULL, error) ||
        !machine_ir_function_append_block(function, &right_id, NULL, error) ||
        !machine_ir_function_append_block(function, &join_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1);
    instruction.as.mov_value = machine_ir_operand_immediate(4);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    if (!machine_ir_block_set_branch(&function->blocks[entry_id], machine_ir_operand_register(0), 1, 2, error) ||
        !machine_ir_block_set_jump(&function->blocks[left_id], 3, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    instruction.result = machine_ir_operand_register(1);
    instruction.as.mov_value = machine_ir_operand_immediate(99);
    if (!machine_ir_block_append_instruction(&function->blocks[right_id], &instruction, error) ||
        !machine_ir_block_set_jump(&function->blocks[right_id], 3, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(1);
    instruction.as.binary.rhs = machine_ir_operand_immediate(3);
    if (!machine_ir_block_append_instruction(&function->blocks[join_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[join_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_join_agrees_cross_block_copy_program(MachineIrProgram *program, MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t left_id;
    size_t right_id;
    size_t join_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 2;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(2, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;
    program->register_bank.registers[1].register_id = 1;
    program->register_bank.registers[1].name = dup_text("r1");
    program->register_bank.registers[1].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &left_id, NULL, error) ||
        !machine_ir_function_append_block(function, &right_id, NULL, error) ||
        !machine_ir_function_append_block(function, &join_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1);
    instruction.as.mov_value = machine_ir_operand_immediate(4);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !machine_ir_block_set_branch(&function->blocks[entry_id], machine_ir_operand_register(0), 1, 2, error) ||
        !machine_ir_block_set_jump(&function->blocks[left_id], 3, error) ||
        !machine_ir_block_set_jump(&function->blocks[right_id], 3, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(1);
    instruction.as.binary.rhs = machine_ir_operand_immediate(3);
    if (!machine_ir_block_append_instruction(&function->blocks[join_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[join_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int test_machine_ir_cleanup_propagates_copies_across_single_predecessor_chain(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_straight_line_cross_block_copy_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: cross-block copy cleanup setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: cross-block copy cleanup dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (strstr(actual_text, "reg.1(r1) = mov reg.0(r0)") ||
        !strstr(actual_text,
            "bb.0:\n"
            "    ret 11\n")) {
        fprintf(stderr,
            "[machine-ir] FAIL: cross-block copy cleanup propagation mismatch\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_stops_copy_propagation_at_join(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_join_stops_cross_block_copy_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: join-stop copy cleanup setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: join-stop copy cleanup dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!strstr(actual_text, "bb.1:\n    ret 7\n") ||
        !strstr(actual_text, "bb.2:\n    ret 102\n") ||
        strstr(actual_text, "ret 7\n  bb.2:\n    ret 7\n")) {
        fprintf(stderr,
            "[machine-ir] FAIL: join-stop copy cleanup should stay conservative at join\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_propagates_agreed_copy_through_join(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_join_agrees_cross_block_copy_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: join-agree copy cleanup setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: join-agree copy cleanup dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!strstr(actual_text, "bb.0:\n    ret 7\n") ||
        strstr(actual_text, "reg.0(r0) = add reg.1(r1), 3")) {
        fprintf(stderr,
            "[machine-ir] FAIL: join-agree copy cleanup should propagate matching incoming fact\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int build_machine_ir_jump_return_tail_program(MachineIrProgram *program, MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t exit_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &exit_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_immediate(11);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !machine_ir_block_set_jump(&function->blocks[entry_id], 1, error) ||
        !machine_ir_block_set_return(&function->blocks[exit_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_same_return_branch_tail_program(MachineIrProgram *program, MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t left_id;
    size_t right_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &left_id, NULL, error) ||
        !machine_ir_function_append_block(function, &right_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_immediate(13);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !machine_ir_block_set_branch(&function->blocks[entry_id], machine_ir_operand_register(0), 1, 2, error) ||
        !machine_ir_block_set_return(&function->blocks[left_id], machine_ir_operand_register(0), error) ||
        !machine_ir_block_set_return(&function->blocks[right_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int test_machine_ir_cleanup_folds_jump_to_return_tail(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_jump_return_tail_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: jump-return-tail setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: jump-return-tail dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (strstr(actual_text, "jmp ") ||
        strstr(actual_text, "bb.1:\n") ||
        !strstr(actual_text, "bb.0:\n    ret 11\n")) {
        fprintf(stderr,
            "[machine-ir] FAIL: jump-return-tail cleanup mismatch\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_folds_same_return_branch_tail(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_same_return_branch_tail_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: same-return-branch-tail setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: same-return-branch-tail dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (strstr(actual_text, "br ") ||
        strstr(actual_text, "bb.1:\n") ||
        !strstr(actual_text, "bb.0:\n    ret 13\n")) {
        fprintf(stderr,
            "[machine-ir] FAIL: same-return-branch-tail cleanup mismatch\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int build_machine_ir_jump_thin_return_tail_program(MachineIrProgram *program, MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t tail_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &tail_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    if (!machine_ir_block_set_jump(&function->blocks[entry_id], 1, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_immediate(21);
    if (!machine_ir_block_append_instruction(&function->blocks[tail_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[tail_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_same_thin_return_branch_tail_program(MachineIrProgram *program, MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t left_id;
    size_t right_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &left_id, NULL, error) ||
        !machine_ir_function_append_block(function, &right_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    if (!machine_ir_block_set_branch(&function->blocks[entry_id], machine_ir_operand_register(0), 1, 2, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_immediate(34);
    if (!machine_ir_block_append_instruction(&function->blocks[left_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[left_id], machine_ir_operand_register(0), error) ||
        !machine_ir_block_append_instruction(&function->blocks[right_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[right_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int test_machine_ir_cleanup_folds_jump_to_thin_return_tail(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_jump_thin_return_tail_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: jump-thin-return-tail setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: jump-thin-return-tail dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (strstr(actual_text, "jmp ") ||
        strstr(actual_text, "bb.1:\n") ||
        !strstr(actual_text, "bb.0:\n    ret 21\n")) {
        fprintf(stderr,
            "[machine-ir] FAIL: jump-thin-return-tail cleanup mismatch\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_folds_same_thin_return_branch_tail(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_same_thin_return_branch_tail_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: same-thin-return-branch-tail setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: same-thin-return-branch-tail dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (strstr(actual_text, "br ") ||
        strstr(actual_text, "bb.1:\n") ||
        !strstr(actual_text, "bb.0:\n    ret 34\n")) {
        fprintf(stderr,
            "[machine-ir] FAIL: same-thin-return-branch-tail cleanup mismatch\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int build_machine_ir_jump_thin_jump_wrapper_program(MachineIrProgram *program, MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t wrapper_id;
    size_t exit_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &wrapper_id, NULL, error) ||
        !machine_ir_function_append_block(function, &exit_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    if (!machine_ir_block_set_jump(&function->blocks[entry_id], 1, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_immediate(55);
    if (!machine_ir_block_append_instruction(&function->blocks[wrapper_id], &instruction, error) ||
        !machine_ir_block_set_jump(&function->blocks[wrapper_id], 2, error) ||
        !machine_ir_block_set_return(&function->blocks[exit_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_jump_thin_jump_wrapper_to_thin_return_program(MachineIrProgram *program,
    MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t wrapper_id;
    size_t tail_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &wrapper_id, NULL, error) ||
        !machine_ir_function_append_block(function, &tail_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    if (!machine_ir_block_set_jump(&function->blocks[entry_id], 1, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_immediate(89);
    if (!machine_ir_block_append_instruction(&function->blocks[wrapper_id], &instruction, error) ||
        !machine_ir_block_set_jump(&function->blocks[wrapper_id], 2, error) ||
        !machine_ir_block_append_instruction(&function->blocks[tail_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[tail_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int test_machine_ir_cleanup_folds_jump_to_thin_jump_wrapper(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_jump_thin_jump_wrapper_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: jump-thin-jump-wrapper setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: jump-thin-jump-wrapper dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (strstr(actual_text, "jmp ") ||
        strstr(actual_text, "bb.1:\n") ||
        !strstr(actual_text, "bb.0:\n    ret 55\n")) {
        fprintf(stderr,
            "[machine-ir] FAIL: jump-thin-jump-wrapper cleanup mismatch\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_folds_jump_to_thin_jump_wrapper_then_thin_return(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_jump_thin_jump_wrapper_to_thin_return_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: jump-thin-wrapper-to-thin-return setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: jump-thin-wrapper-to-thin-return dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (strstr(actual_text, "jmp ") ||
        strstr(actual_text, "bb.1:\n") ||
        !strstr(actual_text, "bb.0:\n    ret 89\n")) {
        fprintf(stderr,
            "[machine-ir] FAIL: jump-thin-wrapper-to-thin-return cleanup mismatch\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int build_machine_ir_call_clobber_kills_copy_program(MachineIrProgram *program, MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    MachineIrInstruction instruction;
    MachineIrOperand call_args[1];

    machine_ir_program_init(program);
    program->register_bank.register_count = 2;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(2, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;
    program->register_bank.registers[0].caller_clobbered = 1u;
    program->register_bank.registers[1].register_id = 1;
    program->register_bank.registers[1].name = dup_text("r1");
    program->register_bank.registers[1].allocatable = 1u;
    program->register_bank.registers[1].caller_clobbered = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1);
    instruction.as.mov_value = machine_ir_operand_immediate(41);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = dup_text("callee");
    if (!instruction.as.call.callee_name) {
        machine_ir_program_free(program);
        return 0;
    }
    call_args[0] = machine_ir_operand_register(1);
    instruction.as.call.args = call_args;
    instruction.as.call.arg_count = 1;
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        free(instruction.as.call.callee_name);
        machine_ir_program_free(program);
        return 0;
    }
    free(instruction.as.call.callee_name);

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(1);
    instruction.as.binary.rhs = machine_ir_operand_immediate(1);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[entry_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_call_preserves_copy_program(MachineIrProgram *program, MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    MachineIrInstruction instruction;
    MachineIrOperand call_args[1];

    machine_ir_program_init(program);
    program->register_bank.register_count = 2;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(2, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;
    program->register_bank.registers[0].caller_clobbered = 1u;
    program->register_bank.registers[1].register_id = 1;
    program->register_bank.registers[1].name = dup_text("r1");
    program->register_bank.registers[1].allocatable = 1u;
    program->register_bank.registers[1].callee_preserved = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1);
    instruction.as.mov_value = machine_ir_operand_immediate(41);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = dup_text("callee");
    if (!instruction.as.call.callee_name) {
        machine_ir_program_free(program);
        return 0;
    }
    call_args[0] = machine_ir_operand_register(1);
    instruction.as.call.args = call_args;
    instruction.as.call.arg_count = 1;
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        free(instruction.as.call.callee_name);
        machine_ir_program_free(program);
        return 0;
    }
    free(instruction.as.call.callee_name);

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(1);
    instruction.as.binary.rhs = machine_ir_operand_immediate(1);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[entry_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int test_machine_ir_cleanup_respects_caller_clobber_kills(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_call_clobber_kills_copy_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: caller-clobber copy cleanup setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: caller-clobber copy cleanup dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!strstr(actual_text, "reg.0(r0) = add reg.1(r1), 1") ||
        strstr(actual_text, "reg.0(r0) = add 41, 1")) {
        fprintf(stderr,
            "[machine-ir] FAIL: caller-clobbered register should not propagate through call\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_keeps_callee_preserved_copy_facts(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_call_preserves_copy_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: callee-preserved copy cleanup setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: callee-preserved copy cleanup dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!strstr(actual_text, "bb.0:\n    reg.0(r0) = call callee(41)\n    ret 42\n") ||
        strstr(actual_text, "reg.0(r0) = add reg.1(r1), 1")) {
        fprintf(stderr,
            "[machine-ir] FAIL: callee-preserved register should keep copy fact through call\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int build_machine_ir_identical_branch_successor_program(MachineIrProgram *program, MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t left_id;
    size_t right_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &left_id, NULL, error) ||
        !machine_ir_function_append_block(function, &right_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    if (!machine_ir_block_set_branch(&function->blocks[entry_id], machine_ir_operand_register(0), 1, 2, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_immediate(77);
    if (!machine_ir_block_append_instruction(&function->blocks[left_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[left_id], machine_ir_operand_register(0), error) ||
        !machine_ir_block_append_instruction(&function->blocks[right_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[right_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_nonidentical_branch_successor_program(MachineIrProgram *program, MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t left_id;
    size_t right_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &left_id, NULL, error) ||
        !machine_ir_function_append_block(function, &right_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    if (!machine_ir_block_set_branch(&function->blocks[entry_id], machine_ir_operand_register(0), 1, 2, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_immediate(77);
    if (!machine_ir_block_append_instruction(&function->blocks[left_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[left_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    instruction.as.mov_value = machine_ir_operand_immediate(78);
    if (!machine_ir_block_append_instruction(&function->blocks[right_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[right_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int test_machine_ir_cleanup_folds_identical_branch_successors(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_identical_branch_successor_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: identical-successor cleanup setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: identical-successor cleanup dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (strstr(actual_text, "br ") ||
        !strstr(actual_text, "bb.0:\n    ret 77\n")) {
        fprintf(stderr,
            "[machine-ir] FAIL: identical branch successor cleanup mismatch\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_keeps_nonidentical_branch_successors(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_nonidentical_branch_successor_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: nonidentical-successor cleanup setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: nonidentical-successor cleanup dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!strstr(actual_text, "br reg.0(r0), bb.1, bb.2") ||
        strstr(actual_text, "bb.0:\n    ret 77\n")) {
        fprintf(stderr,
            "[machine-ir] FAIL: nonidentical branch successors should not fold\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int build_machine_ir_same_wrapper_branch_successor_program(MachineIrProgram *program, MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t left_id;
    size_t right_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &left_id, NULL, error) ||
        !machine_ir_function_append_block(function, &right_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    if (!machine_ir_block_set_branch(&function->blocks[entry_id], machine_ir_operand_register(0), 1, 2, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_immediate(91);
    if (!machine_ir_block_append_instruction(&function->blocks[left_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[left_id], machine_ir_operand_register(0), error) ||
        !machine_ir_block_append_instruction(&function->blocks[right_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[right_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_nonmatching_wrapper_branch_successor_program(MachineIrProgram *program,
    MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t left_id;
    size_t right_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &left_id, NULL, error) ||
        !machine_ir_function_append_block(function, &right_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    if (!machine_ir_block_set_branch(&function->blocks[entry_id], machine_ir_operand_register(0), 1, 2, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_immediate(91);
    if (!machine_ir_block_append_instruction(&function->blocks[left_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[left_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    instruction.as.mov_value = machine_ir_operand_immediate(92);
    if (!machine_ir_block_append_instruction(&function->blocks[right_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[right_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_threaded_identical_branch_successor_program(MachineIrProgram *program,
    MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t left_jump_id;
    size_t right_jump_id;
    size_t left_id;
    size_t right_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &left_jump_id, NULL, error) ||
        !machine_ir_function_append_block(function, &right_jump_id, NULL, error) ||
        !machine_ir_function_append_block(function, &left_id, NULL, error) ||
        !machine_ir_function_append_block(function, &right_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    if (!machine_ir_block_set_branch(&function->blocks[entry_id], machine_ir_operand_register(0), 1, 2, error) ||
        !machine_ir_block_set_jump(&function->blocks[left_jump_id], 3, error) ||
        !machine_ir_block_set_jump(&function->blocks[right_jump_id], 4, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_immediate(77);
    if (!machine_ir_block_append_instruction(&function->blocks[left_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[left_id], machine_ir_operand_register(0), error) ||
        !machine_ir_block_append_instruction(&function->blocks[right_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[right_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_equivalent_call_wrapper_branch_successor_program(MachineIrProgram *program,
    MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t left_id;
    size_t right_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 2;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(2, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;
    program->register_bank.registers[0].caller_clobbered = 1u;
    program->register_bank.registers[1].register_id = 1;
    program->register_bank.registers[1].name = dup_text("r1");
    program->register_bank.registers[1].allocatable = 1u;
    program->register_bank.registers[1].caller_clobbered = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &left_id, NULL, error) ||
        !machine_ir_function_append_block(function, &right_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    if (!machine_ir_block_set_branch(&function->blocks[entry_id], machine_ir_operand_register(0), 1, 2, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = dup_text("foo");
    instruction.as.call.args = NULL;
    instruction.as.call.arg_count = 0;
    if (!instruction.as.call.callee_name) {
        machine_ir_program_free(program);
        return 0;
    }
    if (!machine_ir_block_append_instruction(&function->blocks[left_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[left_id], machine_ir_operand_register(0), error)) {
        free(instruction.as.call.callee_name);
        machine_ir_program_free(program);
        return 0;
    }
    free(instruction.as.call.callee_name);

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1);
    instruction.as.call.callee_name = dup_text("foo");
    instruction.as.call.args = NULL;
    instruction.as.call.arg_count = 0;
    if (!instruction.as.call.callee_name) {
        machine_ir_program_free(program);
        return 0;
    }
    if (!machine_ir_block_append_instruction(&function->blocks[right_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[right_id], machine_ir_operand_register(1), error)) {
        free(instruction.as.call.callee_name);
        machine_ir_program_free(program);
        return 0;
    }
    free(instruction.as.call.callee_name);

    return 1;
}

static int build_machine_ir_threaded_equivalent_call_wrapper_branch_successor_program(MachineIrProgram *program,
    MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t left_jump_id;
    size_t right_jump_id;
    size_t left_id;
    size_t right_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 2;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(2, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;
    program->register_bank.registers[0].caller_clobbered = 1u;
    program->register_bank.registers[1].register_id = 1;
    program->register_bank.registers[1].name = dup_text("r1");
    program->register_bank.registers[1].allocatable = 1u;
    program->register_bank.registers[1].caller_clobbered = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &left_jump_id, NULL, error) ||
        !machine_ir_function_append_block(function, &right_jump_id, NULL, error) ||
        !machine_ir_function_append_block(function, &left_id, NULL, error) ||
        !machine_ir_function_append_block(function, &right_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    if (!machine_ir_block_set_branch(&function->blocks[entry_id], machine_ir_operand_register(0), 1, 2, error) ||
        !machine_ir_block_set_jump(&function->blocks[left_jump_id], 3, error) ||
        !machine_ir_block_set_jump(&function->blocks[right_jump_id], 4, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = dup_text("foo");
    instruction.as.call.args = NULL;
    instruction.as.call.arg_count = 0;
    if (!instruction.as.call.callee_name) {
        machine_ir_program_free(program);
        return 0;
    }
    if (!machine_ir_block_append_instruction(&function->blocks[left_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[left_id], machine_ir_operand_register(0), error)) {
        free(instruction.as.call.callee_name);
        machine_ir_program_free(program);
        return 0;
    }
    free(instruction.as.call.callee_name);

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1);
    instruction.as.call.callee_name = dup_text("foo");
    instruction.as.call.args = NULL;
    instruction.as.call.arg_count = 0;
    if (!instruction.as.call.callee_name) {
        machine_ir_program_free(program);
        return 0;
    }
    if (!machine_ir_block_append_instruction(&function->blocks[right_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[right_id], machine_ir_operand_register(1), error)) {
        free(instruction.as.call.callee_name);
        machine_ir_program_free(program);
        return 0;
    }
    free(instruction.as.call.callee_name);

    return 1;
}

static int test_machine_ir_cleanup_folds_same_single_instruction_branch_successors(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_same_wrapper_branch_successor_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: same-wrapper-branch cleanup setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: same-wrapper-branch cleanup dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (strstr(actual_text, "br ") ||
        !strstr(actual_text, "bb.0:\n    ret 91\n")) {
        fprintf(stderr,
            "[machine-ir] FAIL: same single-instruction branch successor cleanup mismatch\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_folds_threaded_identical_branch_successors(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_threaded_identical_branch_successor_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: threaded-identical-branch setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: threaded-identical-branch dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (strstr(actual_text, "br ") ||
        !strstr(actual_text, "bb.0:\n    ret 77\n")) {
        fprintf(stderr,
            "[machine-ir] FAIL: threaded identical branch successors should collapse\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_folds_equivalent_call_wrapper_branch_successors(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_equivalent_call_wrapper_branch_successor_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: equivalent-call-wrapper-branch setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: equivalent-call-wrapper-branch dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (strstr(actual_text, "br ") ||
        !strstr(actual_text, "bb.0:\n    reg.0(r0) = call foo()\n    ret reg.0(r0)\n")) {
        fprintf(stderr,
            "[machine-ir] FAIL: equivalent call-wrapper branch successors should collapse\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_folds_threaded_equivalent_call_wrapper_branch_successors(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_threaded_equivalent_call_wrapper_branch_successor_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: threaded-equivalent-call-wrapper-branch setup failed: %s\n",
            error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: threaded-equivalent-call-wrapper-branch dump failed: %s\n",
            error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (strstr(actual_text, "br ") ||
        !strstr(actual_text, "bb.0:\n    reg.0(r0) = call foo()\n    ret reg.0(r0)\n")) {
        fprintf(stderr,
            "[machine-ir] FAIL: threaded equivalent call-wrapper branch successors should collapse\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int build_machine_ir_equivalent_inlineable_return_branch_program(MachineIrProgram *program,
    MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t left_id;
    size_t right_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 2;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(2, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;
    program->register_bank.registers[1].register_id = 1;
    program->register_bank.registers[1].name = dup_text("r1");
    program->register_bank.registers[1].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &left_id, NULL, error) ||
        !machine_ir_function_append_block(function, &right_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    if (!machine_ir_block_set_branch(&function->blocks[entry_id], machine_ir_operand_register(0), 1, 2, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_immediate(30);
    instruction.as.binary.rhs = machine_ir_operand_immediate(2);
    if (!machine_ir_block_append_instruction(&function->blocks[left_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[left_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    instruction.result = machine_ir_operand_register(1);
    if (!machine_ir_block_append_instruction(&function->blocks[right_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[right_id], machine_ir_operand_register(1), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_nonequivalent_inlineable_return_branch_program(MachineIrProgram *program,
    MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t left_id;
    size_t right_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 2;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(2, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;
    program->register_bank.registers[1].register_id = 1;
    program->register_bank.registers[1].name = dup_text("r1");
    program->register_bank.registers[1].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &left_id, NULL, error) ||
        !machine_ir_function_append_block(function, &right_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    if (!machine_ir_block_set_branch(&function->blocks[entry_id], machine_ir_operand_register(0), 1, 2, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_immediate(30);
    instruction.as.binary.rhs = machine_ir_operand_immediate(2);
    if (!machine_ir_block_append_instruction(&function->blocks[left_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[left_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    instruction.result = machine_ir_operand_register(1);
    instruction.as.binary.rhs = machine_ir_operand_immediate(3);
    if (!machine_ir_block_append_instruction(&function->blocks[right_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[right_id], machine_ir_operand_register(1), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int test_machine_ir_cleanup_folds_equivalent_inlineable_return_branch_successors(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_equivalent_inlineable_return_branch_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: equivalent-inlineable-return-branch setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: equivalent-inlineable-return-branch dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (strstr(actual_text, "br ") ||
        !strstr(actual_text, "bb.0:\n    ret 32\n")) {
        fprintf(stderr,
            "[machine-ir] FAIL: equivalent inlineable return branch cleanup mismatch\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_keeps_nonequivalent_inlineable_return_branch_successors(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_nonequivalent_inlineable_return_branch_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: nonequivalent-inlineable-return-branch setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: nonequivalent-inlineable-return-branch dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!strstr(actual_text, "br reg.0(r0), bb.1, bb.2") ||
        strstr(actual_text, "bb.0:\n    ret 32\n")) {
        fprintf(stderr,
            "[machine-ir] FAIL: nonequivalent inlineable return branch successors should not fold\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int build_machine_ir_empty_branch_wrapper_program(MachineIrProgram *program, MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t wrapper_id;
    size_t left_id;
    size_t right_id;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &wrapper_id, NULL, error) ||
        !machine_ir_function_append_block(function, &left_id, NULL, error) ||
        !machine_ir_function_append_block(function, &right_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    if (!machine_ir_block_set_jump(&function->blocks[entry_id], 1, error) ||
        !machine_ir_block_set_branch(&function->blocks[wrapper_id], machine_ir_operand_register(0), 2, 3, error) ||
        !machine_ir_block_set_return(&function->blocks[left_id], machine_ir_operand_immediate(1), error) ||
        !machine_ir_block_set_return(&function->blocks[right_id], machine_ir_operand_immediate(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_thin_branch_wrapper_program(MachineIrProgram *program, MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t wrapper_id;
    size_t left_id;
    size_t right_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &wrapper_id, NULL, error) ||
        !machine_ir_function_append_block(function, &left_id, NULL, error) ||
        !machine_ir_function_append_block(function, &right_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    if (!machine_ir_block_set_jump(&function->blocks[entry_id], 1, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_immediate(5);
    if (!machine_ir_block_append_instruction(&function->blocks[wrapper_id], &instruction, error) ||
        !machine_ir_block_set_branch(&function->blocks[wrapper_id], machine_ir_operand_register(0), 2, 3, error) ||
        !machine_ir_block_set_return(&function->blocks[left_id], machine_ir_operand_immediate(1), error) ||
        !machine_ir_block_set_return(&function->blocks[right_id], machine_ir_operand_immediate(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_inlineable_branch_wrapper_tail_program(MachineIrProgram *program, MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t wrapper_id;
    size_t true_id;
    size_t false_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 2;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(2, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;
    program->register_bank.registers[1].register_id = 1;
    program->register_bank.registers[1].name = dup_text("r1");
    program->register_bank.registers[1].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &wrapper_id, NULL, error) ||
        !machine_ir_function_append_block(function, &true_id, NULL, error) ||
        !machine_ir_function_append_block(function, &false_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_register(1);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !machine_ir_block_set_jump(&function->blocks[entry_id], 1, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(1);
    if (!machine_ir_block_append_instruction(&function->blocks[wrapper_id], &instruction, error) ||
        !machine_ir_block_set_branch(&function->blocks[wrapper_id], machine_ir_operand_register(0), 2, 3, error) ||
        !machine_ir_block_set_return(&function->blocks[true_id], machine_ir_operand_immediate(1), error) ||
        !machine_ir_block_set_return(&function->blocks[false_id], machine_ir_operand_immediate(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int test_machine_ir_cleanup_folds_empty_branch_wrapper_tail(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_empty_branch_wrapper_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: empty-branch-wrapper setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: empty-branch-wrapper dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!strstr(actual_text, "bb.0:\n    br reg.0(r0), bb.1, bb.2\n") ||
        strstr(actual_text, "bb.3:\n")) {
        fprintf(stderr,
            "[machine-ir] FAIL: empty branch-wrapper cleanup mismatch\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int build_machine_ir_equivalent_branch_wrapper_successor_program(MachineIrProgram *program,
    MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t left_id;
    size_t right_id;
    size_t exit_true_id;
    size_t exit_false_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 2;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(2, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;
    program->register_bank.registers[1].register_id = 1;
    program->register_bank.registers[1].name = dup_text("r1");
    program->register_bank.registers[1].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &left_id, NULL, error) ||
        !machine_ir_function_append_block(function, &right_id, NULL, error) ||
        !machine_ir_function_append_block(function, &exit_true_id, NULL, error) ||
        !machine_ir_function_append_block(function, &exit_false_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    if (!machine_ir_block_set_branch(&function->blocks[entry_id], machine_ir_operand_register(0), 1, 2, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_immediate(10);
    instruction.as.binary.rhs = machine_ir_operand_immediate(2);
    if (!machine_ir_block_append_instruction(&function->blocks[left_id], &instruction, error) ||
        !machine_ir_block_set_branch(&function->blocks[left_id], machine_ir_operand_register(0), 3, 4, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    instruction.result = machine_ir_operand_register(1);
    if (!machine_ir_block_append_instruction(&function->blocks[right_id], &instruction, error) ||
        !machine_ir_block_set_branch(&function->blocks[right_id], machine_ir_operand_register(1), 3, 4, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    if (!machine_ir_block_set_return(&function->blocks[exit_true_id], machine_ir_operand_immediate(1), error) ||
        !machine_ir_block_set_return(&function->blocks[exit_false_id], machine_ir_operand_immediate(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_threaded_equivalent_branch_wrapper_successor_program(MachineIrProgram *program,
    MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t left_jump_id;
    size_t right_jump_id;
    size_t left_id;
    size_t right_id;
    size_t exit_true_id;
    size_t exit_false_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 2;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(2, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;
    program->register_bank.registers[1].register_id = 1;
    program->register_bank.registers[1].name = dup_text("r1");
    program->register_bank.registers[1].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &left_jump_id, NULL, error) ||
        !machine_ir_function_append_block(function, &right_jump_id, NULL, error) ||
        !machine_ir_function_append_block(function, &left_id, NULL, error) ||
        !machine_ir_function_append_block(function, &right_id, NULL, error) ||
        !machine_ir_function_append_block(function, &exit_true_id, NULL, error) ||
        !machine_ir_function_append_block(function, &exit_false_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    if (!machine_ir_block_set_branch(&function->blocks[entry_id], machine_ir_operand_register(0), 1, 2, error) ||
        !machine_ir_block_set_jump(&function->blocks[left_jump_id], 3, error) ||
        !machine_ir_block_set_jump(&function->blocks[right_jump_id], 4, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_immediate(10);
    instruction.as.binary.rhs = machine_ir_operand_immediate(2);
    if (!machine_ir_block_append_instruction(&function->blocks[left_id], &instruction, error) ||
        !machine_ir_block_set_branch(&function->blocks[left_id], machine_ir_operand_register(0), 5, 6, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    instruction.result = machine_ir_operand_register(1);
    if (!machine_ir_block_append_instruction(&function->blocks[right_id], &instruction, error) ||
        !machine_ir_block_set_branch(&function->blocks[right_id], machine_ir_operand_register(1), 5, 6, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    if (!machine_ir_block_set_return(&function->blocks[exit_true_id], machine_ir_operand_immediate(1), error) ||
        !machine_ir_block_set_return(&function->blocks[exit_false_id], machine_ir_operand_immediate(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_nonequivalent_branch_wrapper_successor_program(MachineIrProgram *program,
    MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t left_id;
    size_t right_id;
    size_t exit_true_id;
    size_t exit_false_id;
    size_t alt_true_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 2;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(2, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;
    program->register_bank.registers[1].register_id = 1;
    program->register_bank.registers[1].name = dup_text("r1");
    program->register_bank.registers[1].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &left_id, NULL, error) ||
        !machine_ir_function_append_block(function, &right_id, NULL, error) ||
        !machine_ir_function_append_block(function, &exit_true_id, NULL, error) ||
        !machine_ir_function_append_block(function, &exit_false_id, NULL, error) ||
        !machine_ir_function_append_block(function, &alt_true_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    if (!machine_ir_block_set_branch(&function->blocks[entry_id], machine_ir_operand_register(0), 1, 2, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_immediate(10);
    instruction.as.binary.rhs = machine_ir_operand_immediate(2);
    if (!machine_ir_block_append_instruction(&function->blocks[left_id], &instruction, error) ||
        !machine_ir_block_set_branch(&function->blocks[left_id], machine_ir_operand_register(0), 3, 4, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    instruction.result = machine_ir_operand_register(1);
    instruction.as.binary.rhs = machine_ir_operand_immediate(3);
    if (!machine_ir_block_append_instruction(&function->blocks[right_id], &instruction, error) ||
        !machine_ir_block_set_branch(&function->blocks[right_id], machine_ir_operand_register(1), 5, 4, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    if (!machine_ir_block_set_return(&function->blocks[exit_true_id], machine_ir_operand_immediate(1), error) ||
        !machine_ir_block_set_return(&function->blocks[exit_false_id], machine_ir_operand_immediate(0), error) ||
        !machine_ir_block_set_return(&function->blocks[alt_true_id], machine_ir_operand_immediate(2), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int test_machine_ir_cleanup_folds_equivalent_inlineable_branch_successors(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_equivalent_branch_wrapper_successor_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: equivalent-branch-wrapper setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: equivalent-branch-wrapper dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!strstr(actual_text, "bb.0:\n    ret 1\n") ||
        strstr(actual_text, "bb.3:\n")) {
        fprintf(stderr,
            "[machine-ir] FAIL: equivalent inlineable branch successors cleanup mismatch\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_folds_threaded_equivalent_inlineable_branch_successors(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_threaded_equivalent_branch_wrapper_successor_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: threaded-equivalent-branch-wrapper setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: threaded-equivalent-branch-wrapper dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!strstr(actual_text, "bb.0:\n    ret 1\n") ||
        strstr(actual_text, "bb.3:\n") ||
        strstr(actual_text, "bb.4:\n")) {
        fprintf(stderr,
            "[machine-ir] FAIL: threaded equivalent inlineable branch successors should collapse\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_keeps_nonequivalent_inlineable_branch_successors(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_nonequivalent_branch_wrapper_successor_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: nonequivalent-branch-wrapper setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: nonequivalent-branch-wrapper dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!strstr(actual_text, "br reg.0(r0), bb.1, bb.2") ||
        strstr(actual_text, "bb.0:\n    br 12, bb.1, bb.2\n")) {
        fprintf(stderr,
            "[machine-ir] FAIL: nonequivalent inlineable branch successors should not fold\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_folds_thin_branch_wrapper_tail(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_thin_branch_wrapper_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: thin-branch-wrapper setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: thin-branch-wrapper dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!strstr(actual_text, "bb.0:\n    ret 1\n") ||
        strstr(actual_text, "bb.3:\n")) {
        fprintf(stderr,
            "[machine-ir] FAIL: thin branch-wrapper cleanup mismatch\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_folds_inlineable_branch_wrapper_tail(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_inlineable_branch_wrapper_tail_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: inlineable-branch-wrapper-tail setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: inlineable-branch-wrapper-tail dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!strstr(actual_text, "bb.0:\n    reg.0(r0) = add reg.1(r1), 1\n    br reg.0(r0), bb.1, bb.2\n") ||
        strstr(actual_text, "bb.3:\n")) {
        fprintf(stderr,
            "[machine-ir] FAIL: inlineable branch-wrapper tail should fold into predecessor\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int build_machine_ir_jump_binary_return_wrapper_program(MachineIrProgram *program, MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t tail_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &tail_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    if (!machine_ir_block_set_jump(&function->blocks[entry_id], 1, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_immediate(12);
    instruction.as.binary.rhs = machine_ir_operand_immediate(8);
    if (!machine_ir_block_append_instruction(&function->blocks[tail_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[tail_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_threaded_thin_return_wrapper_program(MachineIrProgram *program, MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t jump_id;
    size_t tail_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &jump_id, NULL, error) ||
        !machine_ir_function_append_block(function, &tail_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    if (!machine_ir_block_set_jump(&function->blocks[entry_id], 1, error) ||
        !machine_ir_block_set_jump(&function->blocks[jump_id], 2, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_immediate(21);
    if (!machine_ir_block_append_instruction(&function->blocks[tail_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[tail_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_threaded_inlineable_return_wrapper_program(MachineIrProgram *program,
    MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t jump_id;
    size_t tail_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &jump_id, NULL, error) ||
        !machine_ir_function_append_block(function, &tail_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    if (!machine_ir_block_set_jump(&function->blocks[entry_id], 1, error) ||
        !machine_ir_block_set_jump(&function->blocks[jump_id], 2, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_immediate(12);
    instruction.as.binary.rhs = machine_ir_operand_immediate(8);
    if (!machine_ir_block_append_instruction(&function->blocks[tail_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[tail_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_jump_call_return_wrapper_program(MachineIrProgram *program, MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t tail_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;
    program->register_bank.registers[0].caller_clobbered = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &tail_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    if (!machine_ir_block_set_jump(&function->blocks[entry_id], 1, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = dup_text("foo");
    instruction.as.call.args = NULL;
    instruction.as.call.arg_count = 0;
    if (!instruction.as.call.callee_name) {
        machine_ir_program_free(program);
        return 0;
    }
    if (!machine_ir_block_append_instruction(&function->blocks[tail_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[tail_id], machine_ir_operand_register(0), error)) {
        free(instruction.as.call.callee_name);
        machine_ir_program_free(program);
        return 0;
    }
    free(instruction.as.call.callee_name);

    return 1;
}

static int build_machine_ir_jump_call_branch_wrapper_program(MachineIrProgram *program, MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t tail_id;
    size_t true_id;
    size_t false_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;
    program->register_bank.registers[0].caller_clobbered = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &tail_id, NULL, error) ||
        !machine_ir_function_append_block(function, &true_id, NULL, error) ||
        !machine_ir_function_append_block(function, &false_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    if (!machine_ir_block_set_jump(&function->blocks[entry_id], 1, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = dup_text("pred");
    instruction.as.call.args = NULL;
    instruction.as.call.arg_count = 0;
    if (!instruction.as.call.callee_name) {
        machine_ir_program_free(program);
        return 0;
    }
    if (!machine_ir_block_append_instruction(&function->blocks[tail_id], &instruction, error) ||
        !machine_ir_block_set_branch(&function->blocks[tail_id], machine_ir_operand_register(0), 2, 3, error)) {
        free(instruction.as.call.callee_name);
        machine_ir_program_free(program);
        return 0;
    }
    free(instruction.as.call.callee_name);

    if (!machine_ir_block_set_return(&function->blocks[true_id], machine_ir_operand_immediate(1), error) ||
        !machine_ir_block_set_return(&function->blocks[false_id], machine_ir_operand_immediate(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_threaded_call_return_wrapper_program(MachineIrProgram *program, MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t jump_id;
    size_t wrapper_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;
    program->register_bank.registers[0].caller_clobbered = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &jump_id, NULL, error) ||
        !machine_ir_function_append_block(function, &wrapper_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    if (!machine_ir_block_set_jump(&function->blocks[entry_id], 1, error) ||
        !machine_ir_block_set_jump(&function->blocks[jump_id], 2, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = dup_text("foo");
    instruction.as.call.args = NULL;
    instruction.as.call.arg_count = 0;
    if (!instruction.as.call.callee_name) {
        machine_ir_program_free(program);
        return 0;
    }
    if (!machine_ir_block_append_instruction(&function->blocks[wrapper_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[wrapper_id], machine_ir_operand_register(0), error)) {
        free(instruction.as.call.callee_name);
        machine_ir_program_free(program);
        return 0;
    }
    free(instruction.as.call.callee_name);

    return 1;
}

static int build_machine_ir_threaded_call_branch_wrapper_program(MachineIrProgram *program, MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t jump_id;
    size_t wrapper_id;
    size_t true_id;
    size_t false_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;
    program->register_bank.registers[0].caller_clobbered = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &jump_id, NULL, error) ||
        !machine_ir_function_append_block(function, &wrapper_id, NULL, error) ||
        !machine_ir_function_append_block(function, &true_id, NULL, error) ||
        !machine_ir_function_append_block(function, &false_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    if (!machine_ir_block_set_jump(&function->blocks[entry_id], 1, error) ||
        !machine_ir_block_set_jump(&function->blocks[jump_id], 2, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = dup_text("pred");
    instruction.as.call.args = NULL;
    instruction.as.call.arg_count = 0;
    if (!instruction.as.call.callee_name) {
        machine_ir_program_free(program);
        return 0;
    }
    if (!machine_ir_block_append_instruction(&function->blocks[wrapper_id], &instruction, error) ||
        !machine_ir_block_set_branch(&function->blocks[wrapper_id], machine_ir_operand_register(0), 3, 4, error)) {
        free(instruction.as.call.callee_name);
        machine_ir_program_free(program);
        return 0;
    }
    free(instruction.as.call.callee_name);

    if (!machine_ir_block_set_return(&function->blocks[true_id], machine_ir_operand_immediate(1), error) ||
        !machine_ir_block_set_return(&function->blocks[false_id], machine_ir_operand_immediate(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_jump_dangerous_binary_return_wrapper_program(MachineIrProgram *program,
    MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t tail_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &tail_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    if (!machine_ir_block_set_jump(&function->blocks[entry_id], 1, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_DIV;
    instruction.as.binary.lhs = machine_ir_operand_immediate(12);
    instruction.as.binary.rhs = machine_ir_operand_immediate(3);
    if (!machine_ir_block_append_instruction(&function->blocks[tail_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[tail_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int test_machine_ir_cleanup_folds_jump_to_binary_return_wrapper(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_jump_binary_return_wrapper_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: jump-binary-return-wrapper setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: jump-binary-return-wrapper dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (strstr(actual_text, "jmp ") ||
        !strstr(actual_text, "bb.0:\n    ret 20\n")) {
        fprintf(stderr,
            "[machine-ir] FAIL: jump-binary-return-wrapper cleanup mismatch\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_folds_threaded_thin_return_wrapper(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_threaded_thin_return_wrapper_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: threaded-thin-return-wrapper setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: threaded-thin-return-wrapper dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (strstr(actual_text, "jmp bb.1") ||
        !strstr(actual_text, "bb.0:\n    ret 21\n")) {
        fprintf(stderr,
            "[machine-ir] FAIL: threaded thin return wrapper should fold through jump chain\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_folds_threaded_inlineable_return_wrapper(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_threaded_inlineable_return_wrapper_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: threaded-inlineable-return-wrapper setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: threaded-inlineable-return-wrapper dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (strstr(actual_text, "jmp bb.1") ||
        !strstr(actual_text, "bb.0:\n    ret 20\n")) {
        fprintf(stderr,
            "[machine-ir] FAIL: threaded inlineable return wrapper should fold through jump chain\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_folds_jump_to_call_return_wrapper(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_jump_call_return_wrapper_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: jump-call-return-wrapper setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: jump-call-return-wrapper dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (strstr(actual_text, "jmp ") ||
        !strstr(actual_text, "bb.0:\n    reg.0(r0) = call foo()\n    ret reg.0(r0)\n")) {
        fprintf(stderr,
            "[machine-ir] FAIL: jump-call-return-wrapper cleanup mismatch\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_folds_jump_to_call_branch_wrapper(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_jump_call_branch_wrapper_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: jump-call-branch-wrapper setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: jump-call-branch-wrapper dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (strstr(actual_text, "jmp bb.1") ||
        !strstr(actual_text, "bb.0:\n    reg.0(r0) = call pred()\n    br reg.0(r0), bb.1, bb.2\n") ||
        !strstr(actual_text, "bb.1:\n    ret 1\n") ||
        !strstr(actual_text, "bb.2:\n    ret 0\n")) {
        fprintf(stderr,
            "[machine-ir] FAIL: jump-call-branch-wrapper cleanup mismatch\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_folds_threaded_call_return_wrapper(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_threaded_call_return_wrapper_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: threaded-call-return-wrapper setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: threaded-call-return-wrapper dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (strstr(actual_text, "jmp bb.1") ||
        !strstr(actual_text, "bb.0:\n    reg.0(r0) = call foo()\n    ret reg.0(r0)\n")) {
        fprintf(stderr,
            "[machine-ir] FAIL: threaded call-return wrapper should fold through jump chain\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_folds_threaded_call_branch_wrapper(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_threaded_call_branch_wrapper_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: threaded-call-branch-wrapper setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: threaded-call-branch-wrapper dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (strstr(actual_text, "jmp bb.1") ||
        !strstr(actual_text, "bb.0:\n    reg.0(r0) = call pred()\n    br reg.0(r0), bb.1, bb.2\n") ||
        !strstr(actual_text, "bb.1:\n    ret 1\n") ||
        !strstr(actual_text, "bb.2:\n    ret 0\n")) {
        fprintf(stderr,
            "[machine-ir] FAIL: threaded call-branch wrapper should fold through jump chain\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_keeps_dangerous_binary_return_wrapper(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_jump_dangerous_binary_return_wrapper_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: dangerous-binary-return-wrapper setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: dangerous-binary-return-wrapper dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!strstr(actual_text, "bb.0:\n    reg.0(r0) = div 12, 3\n    ret reg.0(r0)\n")) {
        fprintf(stderr,
            "[machine-ir] FAIL: dangerous binary return wrapper should stay conservative\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_keeps_nonmatching_single_instruction_branch_successors(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_nonmatching_wrapper_branch_successor_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: nonmatching-wrapper-branch cleanup setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: nonmatching-wrapper-branch cleanup dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!strstr(actual_text, "br reg.0(r0), bb.1, bb.2") ||
        strstr(actual_text, "bb.0:\n    ret 91\n")) {
        fprintf(stderr,
            "[machine-ir] FAIL: nonmatching single-instruction branch successors should not fold\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int build_machine_ir_value_fold_program(MachineIrProgram *program, MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_immediate(2);
    instruction.as.binary.rhs = machine_ir_operand_immediate(3);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[entry_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_value_identity_program(MachineIrProgram *program, MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_MUL;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(1);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[entry_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_overflow_value_fold_program(MachineIrProgram *program, MachineIrError *error) {
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_immediate(2147483647LL);
    instruction.as.binary.rhs = machine_ir_operand_immediate(1);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_value_fold_tail_chain_program(MachineIrProgram *program, MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t tail_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &tail_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_immediate(8);
    instruction.as.binary.rhs = machine_ir_operand_immediate(5);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !machine_ir_block_set_jump(&function->blocks[entry_id], 1, error) ||
        !machine_ir_block_set_return(&function->blocks[tail_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int test_machine_ir_cleanup_folds_machine_value_constants(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_value_fold_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: machine-value fold setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: machine-value fold dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!strstr(actual_text, "bb.0:\n    ret 5\n")) {
        fprintf(stderr,
            "[machine-ir] FAIL: machine-value constant fold cleanup mismatch\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_wraps_machine_value_overflow_constants(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_overflow_value_fold_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: machine-value overflow fold setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: machine-value overflow fold dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!strstr(actual_text, "bb.0:\n    ret -2147483648\n")) {
        fprintf(stderr,
            "[machine-ir] FAIL: machine-value overflow fold cleanup mismatch\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_folds_machine_value_identities(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_value_identity_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: machine-value identity setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: machine-value identity dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (strstr(actual_text, "mul ") ||
        strstr(actual_text, "mov reg.0(r0)") ||
        !strstr(actual_text, "bb.0:\n    ret reg.0(r0)\n")) {
        fprintf(stderr,
            "[machine-ir] FAIL: machine-value identity cleanup mismatch\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_chain_folds_value_then_tail(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_value_fold_tail_chain_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: machine-value-tail-chain setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: machine-value-tail-chain dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (strstr(actual_text, "jmp ") ||
        !strstr(actual_text, "bb.0:\n    ret 13\n")) {
        fprintf(stderr,
            "[machine-ir] FAIL: machine-value then tail cleanup chain mismatch\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int build_machine_ir_dead_binary_cleanup_program(MachineIrProgram *program, MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_immediate(1);
    instruction.as.binary.rhs = machine_ir_operand_immediate(2);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[entry_id], machine_ir_operand_immediate(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_dead_load_cleanup_program(MachineIrProgram *program, MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t local_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 0, &local_id, error) ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(local_id);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[entry_id], machine_ir_operand_immediate(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_effectful_defs_survive_cleanup_program(MachineIrProgram *program, MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t local_id;
    MachineIrInstruction instruction;
    MachineIrOperand call_args[1];

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 0, &local_id, error) ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = dup_text("callee");
    if (!instruction.as.call.callee_name) {
        machine_ir_program_free(program);
        return 0;
    }
    call_args[0] = machine_ir_operand_immediate(1);
    instruction.as.call.args = call_args;
    instruction.as.call.arg_count = 1;
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        free(instruction.as.call.callee_name);
        machine_ir_program_free(program);
        return 0;
    }
    free(instruction.as.call.callee_name);

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_LOCAL;
    instruction.has_result = 0;
    instruction.as.store.slot = machine_ir_slot_local(local_id);
    instruction.as.store.value = machine_ir_operand_immediate(7);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[entry_id], machine_ir_operand_immediate(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_slot_forward_local_program(MachineIrProgram *program, MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t local_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 0, &local_id, error) ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_LOCAL;
    instruction.as.store.slot = machine_ir_slot_local(local_id);
    instruction.as.store.value = machine_ir_operand_immediate(7);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(local_id);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[entry_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_slot_forward_fold_program(MachineIrProgram *program, MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t local_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 0, &local_id, error) ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_LOCAL;
    instruction.as.store.slot = machine_ir_slot_local(local_id);
    instruction.as.store.value = machine_ir_operand_immediate(7);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(local_id);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(5);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[entry_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_redundant_store_local_program(MachineIrProgram *program, MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t local_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 0, &local_id, error) ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_LOCAL;
    instruction.as.store.slot = machine_ir_slot_local(local_id);
    instruction.as.store.value = machine_ir_operand_immediate(7);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[entry_id], machine_ir_operand_immediate(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_global_fact_killed_by_call_program(MachineIrProgram *program, MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_global(program, "g", NULL, error) ||
        !machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = machine_ir_slot_global(0);
    instruction.as.store.value = machine_ir_operand_immediate(7);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = dup_text("touch");
    instruction.as.call.args = NULL;
    instruction.as.call.arg_count = 0;
    if (!instruction.as.call.callee_name) {
        machine_ir_program_free(program);
        return 0;
    }
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        free(instruction.as.call.callee_name);
        machine_ir_program_free(program);
        return 0;
    }
    free(instruction.as.call.callee_name);

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_global(0);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[entry_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_straight_line_cross_block_slot_forward_program(MachineIrProgram *program,
    MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t next_id;
    size_t local_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 0, &local_id, error) ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &next_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_LOCAL;
    instruction.as.store.slot = machine_ir_slot_local(local_id);
    instruction.as.store.value = machine_ir_operand_immediate(7);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !machine_ir_block_set_jump(&function->blocks[entry_id], next_id, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(local_id);
    if (!machine_ir_block_append_instruction(&function->blocks[next_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[next_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_join_agree_slot_forward_program(MachineIrProgram *program,
    MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t left_id;
    size_t right_id;
    size_t join_id;
    size_t local_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 0, &local_id, error) ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &left_id, NULL, error) ||
        !machine_ir_function_append_block(function, &right_id, NULL, error) ||
        !machine_ir_function_append_block(function, &join_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = dup_text("cond");
    instruction.as.call.args = NULL;
    instruction.as.call.arg_count = 0;
    if (!instruction.as.call.callee_name) {
        machine_ir_program_free(program);
        return 0;
    }
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        free(instruction.as.call.callee_name);
        machine_ir_program_free(program);
        return 0;
    }
    free(instruction.as.call.callee_name);

    if (!machine_ir_block_set_branch(&function->blocks[entry_id], machine_ir_operand_register(0), left_id, right_id, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_immediate(1);
    if (!machine_ir_block_append_instruction(&function->blocks[left_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_LOCAL;
    instruction.as.store.slot = machine_ir_slot_local(local_id);
    instruction.as.store.value = machine_ir_operand_immediate(7);
    if (!machine_ir_block_append_instruction(&function->blocks[left_id], &instruction, error) ||
        !machine_ir_block_set_jump(&function->blocks[left_id], join_id, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_immediate(2);
    if (!machine_ir_block_append_instruction(&function->blocks[right_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_LOCAL;
    instruction.as.store.slot = machine_ir_slot_local(local_id);
    instruction.as.store.value = machine_ir_operand_immediate(7);
    if (!machine_ir_block_append_instruction(&function->blocks[right_id], &instruction, error) ||
        !machine_ir_block_set_jump(&function->blocks[right_id], join_id, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(local_id);
    if (!machine_ir_block_append_instruction(&function->blocks[join_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[join_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_slot_value_invalidated_by_register_redef_program(MachineIrProgram *program,
    MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t local_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 0, &local_id, error) ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_LOCAL;
    instruction.as.store.slot = machine_ir_slot_local(local_id);
    instruction.as.store.value = machine_ir_operand_immediate(7);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_immediate(7);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    instruction.as.mov_value = machine_ir_operand_immediate(9);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_LOCAL;
    instruction.as.store.slot = machine_ir_slot_local(local_id);
    instruction.as.store.value = machine_ir_operand_register(0);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(local_id);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[entry_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_dead_store_same_block_overwrite_program(MachineIrProgram *program,
    MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t local_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 0, &local_id, error) ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_LOCAL;
    instruction.as.store.slot = machine_ir_slot_local(local_id);
    instruction.as.store.value = machine_ir_operand_immediate(1);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }
    instruction.as.store.value = machine_ir_operand_immediate(2);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(local_id);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[entry_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_dead_store_cross_block_overwrite_program(MachineIrProgram *program,
    MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t next_id;
    size_t local_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 0, &local_id, error) ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &next_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_LOCAL;
    instruction.as.store.slot = machine_ir_slot_local(local_id);
    instruction.as.store.value = machine_ir_operand_immediate(1);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !machine_ir_block_set_jump(&function->blocks[entry_id], next_id, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    instruction.as.store.value = machine_ir_operand_immediate(2);
    if (!machine_ir_block_append_instruction(&function->blocks[next_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(local_id);
    if (!machine_ir_block_append_instruction(&function->blocks[next_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[next_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_dead_store_join_observed_program(MachineIrProgram *program,
    MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t left_id;
    size_t right_id;
    size_t join_id;
    size_t local_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 0, &local_id, error) ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &left_id, NULL, error) ||
        !machine_ir_function_append_block(function, &right_id, NULL, error) ||
        !machine_ir_function_append_block(function, &join_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = dup_text("cond");
    instruction.as.call.args = NULL;
    instruction.as.call.arg_count = 0;
    if (!instruction.as.call.callee_name) {
        machine_ir_program_free(program);
        return 0;
    }
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        free(instruction.as.call.callee_name);
        machine_ir_program_free(program);
        return 0;
    }
    free(instruction.as.call.callee_name);

    if (!machine_ir_block_set_branch(&function->blocks[entry_id], machine_ir_operand_register(0), left_id, right_id, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_LOCAL;
    instruction.as.store.slot = machine_ir_slot_local(local_id);
    instruction.as.store.value = machine_ir_operand_immediate(1);
    if (!machine_ir_block_append_instruction(&function->blocks[left_id], &instruction, error) ||
        !machine_ir_block_set_jump(&function->blocks[left_id], join_id, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    instruction.as.store.value = machine_ir_operand_immediate(2);
    if (!machine_ir_block_append_instruction(&function->blocks[right_id], &instruction, error) ||
        !machine_ir_block_set_jump(&function->blocks[right_id], join_id, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(local_id);
    if (!machine_ir_block_append_instruction(&function->blocks[join_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[join_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_dead_global_store_preserved_by_call_program(MachineIrProgram *program,
    MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_global(program, "g", NULL, error) ||
        !machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = machine_ir_slot_global(0);
    instruction.as.store.value = machine_ir_operand_immediate(1);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = dup_text("touch");
    instruction.as.call.args = NULL;
    instruction.as.call.arg_count = 0;
    if (!instruction.as.call.callee_name) {
        machine_ir_program_free(program);
        return 0;
    }
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        free(instruction.as.call.callee_name);
        machine_ir_program_free(program);
        return 0;
    }
    free(instruction.as.call.callee_name);

    if (!machine_ir_block_set_return(&function->blocks[entry_id], machine_ir_operand_immediate(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_dead_global_store_preserved_at_return_program(MachineIrProgram *program,
    MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_global(program, "g", NULL, error) ||
        !machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = machine_ir_slot_global(0);
    instruction.as.store.value = machine_ir_operand_immediate(1);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[entry_id], machine_ir_operand_immediate(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int test_machine_ir_cleanup_eliminates_dead_safe_binaries(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_dead_binary_cleanup_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: dead-binary cleanup setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: dead-binary cleanup dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (strstr(actual_text, "add ") ||
        !strstr(actual_text, "bb.0:\n    ret 0\n")) {
        fprintf(stderr,
            "[machine-ir] FAIL: dead safe binary cleanup mismatch\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_eliminates_dead_loads(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_dead_load_cleanup_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: dead-load cleanup setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: dead-load cleanup dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (strstr(actual_text, "load_local") ||
        !strstr(actual_text, "bb.0:\n    ret 0\n")) {
        fprintf(stderr,
            "[machine-ir] FAIL: dead load cleanup mismatch\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_preserves_effectful_defs(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_effectful_defs_survive_cleanup_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: effectful-def cleanup setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: effectful-def cleanup dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!strstr(actual_text, "call callee(1)") ||
        strstr(actual_text, "store local.0, 7") != NULL) {
        fprintf(stderr,
            "[machine-ir] FAIL: call should survive while dead local store may disappear\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_forwards_known_local_loads(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_slot_forward_local_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: slot-forward local setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: slot-forward local cleanup dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (strstr(actual_text, "load_local") ||
        !strstr(actual_text, "ret 7")) {
        fprintf(stderr,
            "[machine-ir] FAIL: known local load should forward through cleanup\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_forwards_slot_values_into_value_folding(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_slot_forward_fold_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: slot-forward fold setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: slot-forward fold cleanup dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (strstr(actual_text, "load_local") ||
        strstr(actual_text, " add ") ||
        !strstr(actual_text, "ret 12")) {
        fprintf(stderr,
            "[machine-ir] FAIL: slot forwarding should feed later value folding\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_eliminates_redundant_local_stores(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_redundant_store_local_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: redundant-store local setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: redundant-store local cleanup dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (strstr(actual_text, "store local.0, 7") != NULL ||
        !strstr(actual_text, "bb.0:\n    ret 0\n")) {
        fprintf(stderr,
            "[machine-ir] FAIL: redundant local store should be eliminated\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_call_kills_known_global_slot_values(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_global_fact_killed_by_call_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: global-call-kill setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: global-call-kill cleanup dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!strstr(actual_text, "call touch()") ||
        !strstr(actual_text, "reg.0(r0) = load global.0") ||
        strstr(actual_text, "ret 7")) {
        fprintf(stderr,
            "[machine-ir] FAIL: call should kill known global slot facts\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_forwards_known_slot_values_across_straight_line_blocks(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_straight_line_cross_block_slot_forward_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: cross-block slot-forward setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: cross-block slot-forward cleanup dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (strstr(actual_text, "load_local") ||
        strstr(actual_text, "store local.0, 7") != NULL ||
        !strstr(actual_text, "ret 7")) {
        fprintf(stderr,
            "[machine-ir] FAIL: straight-line successor should inherit known slot fact\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_forwards_must_agree_join_slot_values(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_join_agree_slot_forward_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: join-agree slot-forward setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: join-agree slot-forward cleanup dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (strstr(actual_text, "load_local") ||
        !strstr(actual_text, "call cond()") ||
        strstr(actual_text, "store local.0, 7") != NULL ||
        !strstr(actual_text, "ret 7")) {
        fprintf(stderr,
            "[machine-ir] FAIL: must-agree join should forward known slot fact\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_invalidates_slot_fact_on_register_redefinition(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_slot_value_invalidated_by_register_redef_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: register-redef slot invalidation setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: register-redef slot invalidation dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (strstr(actual_text, "ret 7") != NULL ||
        strstr(actual_text, "load_local") != NULL ||
        !strstr(actual_text, "ret 9")) {
        fprintf(stderr,
            "[machine-ir] FAIL: register redefinition should invalidate old slot fact\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_eliminates_dead_store_overwritten_in_same_block(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_dead_store_same_block_overwrite_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: same-block dead-store setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: same-block dead-store cleanup dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (strstr(actual_text, "store local.0, 1") != NULL ||
        !strstr(actual_text, "ret 2")) {
        fprintf(stderr,
            "[machine-ir] FAIL: overwritten same-block store should be eliminated\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_eliminates_dead_store_overwritten_across_straight_line_blocks(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_dead_store_cross_block_overwrite_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: cross-block dead-store setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: cross-block dead-store cleanup dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (strstr(actual_text, "store local.0, 1") != NULL ||
        !strstr(actual_text, "ret 2")) {
        fprintf(stderr,
            "[machine-ir] FAIL: overwritten straight-line predecessor store should be eliminated\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_keeps_store_observed_at_join(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_dead_store_join_observed_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: join-observed store setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: join-observed store cleanup dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!strstr(actual_text, "call cond()") ||
        !strstr(actual_text, "bb.1:\n    ret 1\n") ||
        !strstr(actual_text, "bb.2:\n    ret 2\n")) {
        fprintf(stderr,
            "[machine-ir] FAIL: join-observed slot values should survive as path results\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_keeps_global_store_observed_by_call(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_dead_global_store_preserved_by_call_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: global-call-observed store setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: global-call-observed store cleanup dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!strstr(actual_text, "store global.0, 1") ||
        !strstr(actual_text, "call touch()")) {
        fprintf(stderr,
            "[machine-ir] FAIL: global store before call should be preserved as observed\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_keeps_global_store_observed_at_return(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_dead_global_store_preserved_at_return_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: global-return-observed store setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: global-return-observed store cleanup dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!strstr(actual_text, "store global.0, 1") ||
        !strstr(actual_text, "ret 0")) {
        fprintf(stderr,
            "[machine-ir] FAIL: global store before return should be preserved as externally observed\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int build_machine_ir_branch_side_cross_block_copy_program_impl(MachineIrProgram *program,
    MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t left_id;
    size_t right_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 2;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(2, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;
    program->register_bank.registers[1].register_id = 1;
    program->register_bank.registers[1].name = dup_text("r1");
    program->register_bank.registers[1].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &left_id, NULL, error) ||
        !machine_ir_function_append_block(function, &right_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1);
    instruction.as.mov_value = machine_ir_operand_immediate(6);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !machine_ir_block_set_branch(&function->blocks[entry_id], machine_ir_operand_register(0), 1, 2, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(1);
    instruction.as.binary.rhs = machine_ir_operand_immediate(5);
    if (!machine_ir_block_append_instruction(&function->blocks[left_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[left_id], machine_ir_operand_register(0), error) ||
        !machine_ir_block_set_return(&function->blocks[right_id], machine_ir_operand_immediate(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_branch_side_unique_successor_slot_program(MachineIrProgram *program,
    MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t left_id;
    size_t right_id;
    size_t left_tail_id;
    size_t local_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 0, &local_id, error) ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &left_id, NULL, error) ||
        !machine_ir_function_append_block(function, &right_id, NULL, error) ||
        !machine_ir_function_append_block(function, &left_tail_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = dup_text("cond");
    instruction.as.call.args = NULL;
    instruction.as.call.arg_count = 0;
    if (!instruction.as.call.callee_name) {
        machine_ir_program_free(program);
        return 0;
    }
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        free(instruction.as.call.callee_name);
        machine_ir_program_free(program);
        return 0;
    }
    free(instruction.as.call.callee_name);

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_LOCAL;
    instruction.as.store.slot = machine_ir_slot_local(local_id);
    instruction.as.store.value = machine_ir_operand_immediate(7);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !machine_ir_block_set_branch(&function->blocks[entry_id], machine_ir_operand_register(0), left_id, right_id, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    if (!machine_ir_block_set_jump(&function->blocks[left_id], left_tail_id, error) ||
        !machine_ir_block_set_return(&function->blocks[right_id], machine_ir_operand_immediate(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(local_id);
    if (!machine_ir_block_append_instruction(&function->blocks[left_tail_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[left_tail_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_slot_fact_follows_copy_chain_program(MachineIrProgram *program,
    MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t local_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 2;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(2, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;
    program->register_bank.registers[1].register_id = 1;
    program->register_bank.registers[1].name = dup_text("r1");
    program->register_bank.registers[1].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 0, &local_id, error) ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1);
    instruction.as.mov_value = machine_ir_operand_immediate(7);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_register(1);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_LOCAL;
    instruction.as.store.slot = machine_ir_slot_local(local_id);
    instruction.as.store.value = machine_ir_operand_register(0);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(local_id);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[entry_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_known_slot_return_wrapper_program(MachineIrProgram *program,
    MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t wrapper_id;
    size_t local_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 0, &local_id, error) ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &wrapper_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_LOCAL;
    instruction.as.store.slot = machine_ir_slot_local(local_id);
    instruction.as.store.value = machine_ir_operand_immediate(7);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !machine_ir_block_set_jump(&function->blocks[entry_id], wrapper_id, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(local_id);
    if (!machine_ir_block_append_instruction(&function->blocks[wrapper_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[wrapper_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_branch_side_known_slot_return_wrappers_program(MachineIrProgram *program,
    MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t left_id;
    size_t right_id;
    size_t left_wrapper_id;
    size_t right_wrapper_id;
    size_t local_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 0, &local_id, error) ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &left_id, NULL, error) ||
        !machine_ir_function_append_block(function, &right_id, NULL, error) ||
        !machine_ir_function_append_block(function, &left_wrapper_id, NULL, error) ||
        !machine_ir_function_append_block(function, &right_wrapper_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = dup_text("cond");
    instruction.as.call.args = NULL;
    instruction.as.call.arg_count = 0;
    if (!instruction.as.call.callee_name) {
        machine_ir_program_free(program);
        return 0;
    }
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        free(instruction.as.call.callee_name);
        machine_ir_program_free(program);
        return 0;
    }
    free(instruction.as.call.callee_name);

    if (!machine_ir_block_set_branch(&function->blocks[entry_id], machine_ir_operand_register(0), left_id, right_id, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_LOCAL;
    instruction.as.store.slot = machine_ir_slot_local(local_id);
    instruction.as.store.value = machine_ir_operand_immediate(1);
    if (!machine_ir_block_append_instruction(&function->blocks[left_id], &instruction, error) ||
        !machine_ir_block_set_jump(&function->blocks[left_id], left_wrapper_id, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    instruction.as.store.value = machine_ir_operand_immediate(2);
    if (!machine_ir_block_append_instruction(&function->blocks[right_id], &instruction, error) ||
        !machine_ir_block_set_jump(&function->blocks[right_id], right_wrapper_id, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(local_id);
    if (!machine_ir_block_append_instruction(&function->blocks[left_wrapper_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[left_wrapper_id], machine_ir_operand_register(0), error) ||
        !machine_ir_block_append_instruction(&function->blocks[right_wrapper_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[right_wrapper_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_known_slot_branch_wrapper_program(MachineIrProgram *program,
    MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t wrapper_id;
    size_t true_id;
    size_t false_id;
    size_t local_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 0, &local_id, error) ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &wrapper_id, NULL, error) ||
        !machine_ir_function_append_block(function, &true_id, NULL, error) ||
        !machine_ir_function_append_block(function, &false_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_LOCAL;
    instruction.as.store.slot = machine_ir_slot_local(local_id);
    instruction.as.store.value = machine_ir_operand_immediate(1);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !machine_ir_block_set_jump(&function->blocks[entry_id], wrapper_id, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(local_id);
    if (!machine_ir_block_append_instruction(&function->blocks[wrapper_id], &instruction, error) ||
        !machine_ir_block_set_branch(&function->blocks[wrapper_id], machine_ir_operand_register(0), true_id, false_id, error) ||
        !machine_ir_block_set_return(&function->blocks[true_id], machine_ir_operand_immediate(1), error) ||
        !machine_ir_block_set_return(&function->blocks[false_id], machine_ir_operand_immediate(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_branch_side_known_slot_branch_wrappers_program(MachineIrProgram *program,
    MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t left_id;
    size_t right_id;
    size_t left_wrapper_id;
    size_t right_wrapper_id;
    size_t true_id;
    size_t false_id;
    size_t local_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 0, &local_id, error) ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &left_id, NULL, error) ||
        !machine_ir_function_append_block(function, &right_id, NULL, error) ||
        !machine_ir_function_append_block(function, &left_wrapper_id, NULL, error) ||
        !machine_ir_function_append_block(function, &right_wrapper_id, NULL, error) ||
        !machine_ir_function_append_block(function, &true_id, NULL, error) ||
        !machine_ir_function_append_block(function, &false_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = dup_text("cond");
    instruction.as.call.args = NULL;
    instruction.as.call.arg_count = 0;
    if (!instruction.as.call.callee_name) {
        machine_ir_program_free(program);
        return 0;
    }
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        free(instruction.as.call.callee_name);
        machine_ir_program_free(program);
        return 0;
    }
    free(instruction.as.call.callee_name);

    if (!machine_ir_block_set_branch(&function->blocks[entry_id], machine_ir_operand_register(0), left_id, right_id, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_LOCAL;
    instruction.as.store.slot = machine_ir_slot_local(local_id);
    instruction.as.store.value = machine_ir_operand_immediate(1);
    if (!machine_ir_block_append_instruction(&function->blocks[left_id], &instruction, error) ||
        !machine_ir_block_set_jump(&function->blocks[left_id], left_wrapper_id, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    instruction.as.store.value = machine_ir_operand_immediate(0);
    if (!machine_ir_block_append_instruction(&function->blocks[right_id], &instruction, error) ||
        !machine_ir_block_set_jump(&function->blocks[right_id], right_wrapper_id, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(local_id);
    if (!machine_ir_block_append_instruction(&function->blocks[left_wrapper_id], &instruction, error) ||
        !machine_ir_block_set_branch(&function->blocks[left_wrapper_id], machine_ir_operand_register(0), true_id, false_id, error) ||
        !machine_ir_block_append_instruction(&function->blocks[right_wrapper_id], &instruction, error) ||
        !machine_ir_block_set_branch(&function->blocks[right_wrapper_id], machine_ir_operand_register(0), true_id, false_id, error) ||
        !machine_ir_block_set_return(&function->blocks[true_id], machine_ir_operand_immediate(1), error) ||
        !machine_ir_block_set_return(&function->blocks[false_id], machine_ir_operand_immediate(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_shared_known_slot_return_successors_program(MachineIrProgram *program,
    MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t left_id;
    size_t right_id;
    size_t left_ret_id;
    size_t right_ret_id;
    size_t local_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 0, &local_id, error) ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &left_id, NULL, error) ||
        !machine_ir_function_append_block(function, &right_id, NULL, error) ||
        !machine_ir_function_append_block(function, &left_ret_id, NULL, error) ||
        !machine_ir_function_append_block(function, &right_ret_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = dup_text("cond");
    instruction.as.call.args = NULL;
    instruction.as.call.arg_count = 0;
    if (!instruction.as.call.callee_name) {
        machine_ir_program_free(program);
        return 0;
    }
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        free(instruction.as.call.callee_name);
        machine_ir_program_free(program);
        return 0;
    }
    free(instruction.as.call.callee_name);

    if (!machine_ir_block_set_branch(&function->blocks[entry_id], machine_ir_operand_register(0), left_id, right_id, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_LOCAL;
    instruction.as.store.slot = machine_ir_slot_local(local_id);
    instruction.as.store.value = machine_ir_operand_immediate(7);
    if (!machine_ir_block_append_instruction(&function->blocks[left_id], &instruction, error) ||
        !machine_ir_block_set_jump(&function->blocks[left_id], left_ret_id, error) ||
        !machine_ir_block_append_instruction(&function->blocks[right_id], &instruction, error) ||
        !machine_ir_block_set_jump(&function->blocks[right_id], right_ret_id, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(local_id);
    if (!machine_ir_block_append_instruction(&function->blocks[left_ret_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[left_ret_id], machine_ir_operand_register(0), error) ||
        !machine_ir_block_append_instruction(&function->blocks[right_ret_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[right_ret_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_shared_known_slot_branch_successors_program(MachineIrProgram *program,
    MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t left_id;
    size_t right_id;
    size_t left_br_id;
    size_t right_br_id;
    size_t true_id;
    size_t false_id;
    size_t local_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 0, &local_id, error) ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &left_id, NULL, error) ||
        !machine_ir_function_append_block(function, &right_id, NULL, error) ||
        !machine_ir_function_append_block(function, &left_br_id, NULL, error) ||
        !machine_ir_function_append_block(function, &right_br_id, NULL, error) ||
        !machine_ir_function_append_block(function, &true_id, NULL, error) ||
        !machine_ir_function_append_block(function, &false_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = dup_text("cond");
    instruction.as.call.args = NULL;
    instruction.as.call.arg_count = 0;
    if (!instruction.as.call.callee_name) {
        machine_ir_program_free(program);
        return 0;
    }
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        free(instruction.as.call.callee_name);
        machine_ir_program_free(program);
        return 0;
    }
    free(instruction.as.call.callee_name);

    if (!machine_ir_block_set_branch(&function->blocks[entry_id], machine_ir_operand_register(0), left_id, right_id, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_LOCAL;
    instruction.as.store.slot = machine_ir_slot_local(local_id);
    instruction.as.store.value = machine_ir_operand_immediate(1);
    if (!machine_ir_block_append_instruction(&function->blocks[left_id], &instruction, error) ||
        !machine_ir_block_set_jump(&function->blocks[left_id], left_br_id, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    if (!machine_ir_block_append_instruction(&function->blocks[right_id], &instruction, error) ||
        !machine_ir_block_set_jump(&function->blocks[right_id], right_br_id, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(local_id);
    if (!machine_ir_block_append_instruction(&function->blocks[left_br_id], &instruction, error) ||
        !machine_ir_block_set_branch(&function->blocks[left_br_id], machine_ir_operand_register(0), true_id, false_id, error) ||
        !machine_ir_block_append_instruction(&function->blocks[right_br_id], &instruction, error) ||
        !machine_ir_block_set_branch(&function->blocks[right_br_id], machine_ir_operand_register(0), true_id, false_id, error) ||
        !machine_ir_block_set_return(&function->blocks[true_id], machine_ir_operand_immediate(1), error) ||
        !machine_ir_block_set_return(&function->blocks[false_id], machine_ir_operand_immediate(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_inlineable_known_slot_return_wrapper_program(MachineIrProgram *program,
    MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t wrapper_id;
    size_t local_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 0, &local_id, error) ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &wrapper_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_LOCAL;
    instruction.as.store.slot = machine_ir_slot_local(local_id);
    instruction.as.store.value = machine_ir_operand_immediate(7);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !machine_ir_block_set_jump(&function->blocks[entry_id], wrapper_id, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(local_id);
    if (!machine_ir_block_append_instruction(&function->blocks[wrapper_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(5);
    if (!machine_ir_block_append_instruction(&function->blocks[wrapper_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[wrapper_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_nonempty_known_slot_return_tail_program(MachineIrProgram *program,
    MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t wrapper_id;
    size_t local_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    program->global_count = 1;
    program->global_capacity = 1;
    program->globals = (MachineIrGlobal *)calloc(1, sizeof(MachineIrGlobal));
    if (!program->register_bank.registers || !program->globals) {
        machine_ir_program_free(program);
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;
    program->globals[0].name = dup_text("g");
    if (!program->register_bank.registers[0].name || !program->globals[0].name) {
        machine_ir_program_free(program);
        return 0;
    }

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 0, &local_id, error) ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &wrapper_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(local_id);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(1);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = machine_ir_slot_global(0);
    instruction.as.store.value = machine_ir_operand_register(0);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !machine_ir_block_set_jump(&function->blocks[entry_id], wrapper_id, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_global(0);
    if (!machine_ir_block_append_instruction(&function->blocks[wrapper_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(5);
    if (!machine_ir_block_append_instruction(&function->blocks[wrapper_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[wrapper_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_inlineable_known_slot_branch_wrapper_program(MachineIrProgram *program,
    MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t wrapper_id;
    size_t true_id;
    size_t false_id;
    size_t local_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 0, &local_id, error) ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &wrapper_id, NULL, error) ||
        !machine_ir_function_append_block(function, &true_id, NULL, error) ||
        !machine_ir_function_append_block(function, &false_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_LOCAL;
    instruction.as.store.slot = machine_ir_slot_local(local_id);
    instruction.as.store.value = machine_ir_operand_immediate(1);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !machine_ir_block_set_jump(&function->blocks[entry_id], wrapper_id, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(local_id);
    if (!machine_ir_block_append_instruction(&function->blocks[wrapper_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_BIT_XOR;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(1);
    if (!machine_ir_block_append_instruction(&function->blocks[wrapper_id], &instruction, error) ||
        !machine_ir_block_set_branch(&function->blocks[wrapper_id], machine_ir_operand_register(0), true_id, false_id, error) ||
        !machine_ir_block_set_return(&function->blocks[true_id], machine_ir_operand_immediate(1), error) ||
        !machine_ir_block_set_return(&function->blocks[false_id], machine_ir_operand_immediate(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_nonempty_known_slot_branch_tail_with_local_store_program(MachineIrProgram *program,
    MachineIrError *error) {
    MachineIrFunction *helper = NULL;
    MachineIrFunction *main_function = NULL;
    size_t helper_entry_id;
    size_t entry_id;
    size_t wrapper_id;
    size_t true_id;
    size_t false_id;
    size_t cond_local_id;
    size_t value_local_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    program->global_count = 1;
    program->global_capacity = 1;
    program->globals = (MachineIrGlobal *)calloc(1, sizeof(MachineIrGlobal));
    if (!program->register_bank.registers || !program->globals) {
        machine_ir_program_free(program);
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;
    program->globals[0].name = dup_text("g");
    if (!program->register_bank.registers[0].name || !program->globals[0].name) {
        machine_ir_program_free(program);
        return 0;
    }

    if (!machine_ir_program_append_function(program, "mut", 1, &helper, error) ||
        !helper ||
        !machine_ir_function_append_local(helper, "x", 1, NULL, error) ||
        !machine_ir_function_append_block(helper, &helper_entry_id, NULL, error) ||
        !machine_ir_program_append_function(program, "main", 1, &main_function, error) ||
        !main_function ||
        !machine_ir_function_append_local(main_function, "cond", 0, &cond_local_id, error) ||
        !machine_ir_function_append_local(main_function, "value", 0, &value_local_id, error) ||
        !machine_ir_function_append_block(main_function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(main_function, &wrapper_id, NULL, error) ||
        !machine_ir_function_append_block(main_function, &true_id, NULL, error) ||
        !machine_ir_function_append_block(main_function, &false_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0u);
    if (!machine_ir_block_append_instruction(&helper->blocks[helper_entry_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = machine_ir_slot_global(0u);
    instruction.as.store.value = machine_ir_operand_register(0);
    if (!machine_ir_block_append_instruction(&helper->blocks[helper_entry_id], &instruction, error) ||
        !machine_ir_block_set_return(&helper->blocks[helper_entry_id], machine_ir_operand_immediate(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_global(0u);
    if (!machine_ir_block_append_instruction(&main_function->blocks[entry_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_LOCAL;
    instruction.as.store.slot = machine_ir_slot_local(value_local_id);
    instruction.as.store.value = machine_ir_operand_register(0);
    if (!machine_ir_block_append_instruction(&main_function->blocks[entry_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_LOCAL;
    instruction.as.store.slot = machine_ir_slot_local(cond_local_id);
    instruction.as.store.value = machine_ir_operand_immediate(1);
    if (!machine_ir_block_append_instruction(&main_function->blocks[entry_id], &instruction, error) ||
        !machine_ir_block_set_jump(&main_function->blocks[entry_id], wrapper_id, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(cond_local_id);
    if (!machine_ir_block_append_instruction(&main_function->blocks[wrapper_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_BIT_XOR;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(1);
    if (!machine_ir_block_append_instruction(&main_function->blocks[wrapper_id], &instruction, error) ||
        !machine_ir_block_set_branch(&main_function->blocks[wrapper_id], machine_ir_operand_register(0), true_id, false_id, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.as.call.callee_name = dup_text("mut");
    instruction.as.call.arg_count = 1u;
    instruction.as.call.args = (MachineIrOperand *)calloc(1u, sizeof(MachineIrOperand));
    if (!instruction.as.call.callee_name || !instruction.as.call.args) {
        free(instruction.as.call.callee_name);
        free(instruction.as.call.args);
        machine_ir_program_free(program);
        return 0;
    }
    instruction.as.call.args[0] = machine_ir_operand_immediate(7);
    if (!machine_ir_block_append_instruction(&main_function->blocks[true_id], &instruction, error)) {
        free(instruction.as.call.callee_name);
        free(instruction.as.call.args);
        machine_ir_program_free(program);
        return 0;
    }
    free(instruction.as.call.callee_name);
    free(instruction.as.call.args);
    instruction.as.call.callee_name = NULL;
    instruction.as.call.args = NULL;

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(value_local_id);
    if (!machine_ir_block_append_instruction(&main_function->blocks[true_id], &instruction, error) ||
        !machine_ir_block_set_return(&main_function->blocks[true_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.as.call.callee_name = dup_text("mut");
    instruction.as.call.arg_count = 1u;
    instruction.as.call.args = (MachineIrOperand *)calloc(1u, sizeof(MachineIrOperand));
    if (!instruction.as.call.callee_name || !instruction.as.call.args) {
        free(instruction.as.call.callee_name);
        free(instruction.as.call.args);
        machine_ir_program_free(program);
        return 0;
    }
    instruction.as.call.args[0] = machine_ir_operand_immediate(9);
    if (!machine_ir_block_append_instruction(&main_function->blocks[false_id], &instruction, error)) {
        free(instruction.as.call.callee_name);
        free(instruction.as.call.args);
        machine_ir_program_free(program);
        return 0;
    }
    free(instruction.as.call.callee_name);
    free(instruction.as.call.args);
    instruction.as.call.callee_name = NULL;
    instruction.as.call.args = NULL;

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(value_local_id);
    if (!machine_ir_block_append_instruction(&main_function->blocks[false_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(5);
    if (!machine_ir_block_append_instruction(&main_function->blocks[false_id], &instruction, error) ||
        !machine_ir_block_set_return(&main_function->blocks[false_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_shared_inlineable_known_slot_return_successors_program(MachineIrProgram *program,
    MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t left_id;
    size_t right_id;
    size_t left_ret_id;
    size_t right_ret_id;
    size_t local_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 0, &local_id, error) ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &left_id, NULL, error) ||
        !machine_ir_function_append_block(function, &right_id, NULL, error) ||
        !machine_ir_function_append_block(function, &left_ret_id, NULL, error) ||
        !machine_ir_function_append_block(function, &right_ret_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = dup_text("cond");
    instruction.as.call.args = NULL;
    instruction.as.call.arg_count = 0;
    if (!instruction.as.call.callee_name) {
        machine_ir_program_free(program);
        return 0;
    }
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        free(instruction.as.call.callee_name);
        machine_ir_program_free(program);
        return 0;
    }
    free(instruction.as.call.callee_name);

    if (!machine_ir_block_set_branch(&function->blocks[entry_id], machine_ir_operand_register(0), left_id, right_id, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_LOCAL;
    instruction.as.store.slot = machine_ir_slot_local(local_id);
    instruction.as.store.value = machine_ir_operand_immediate(7);
    if (!machine_ir_block_append_instruction(&function->blocks[left_id], &instruction, error) ||
        !machine_ir_block_set_jump(&function->blocks[left_id], left_ret_id, error) ||
        !machine_ir_block_append_instruction(&function->blocks[right_id], &instruction, error) ||
        !machine_ir_block_set_jump(&function->blocks[right_id], right_ret_id, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(local_id);
    if (!machine_ir_block_append_instruction(&function->blocks[left_ret_id], &instruction, error) ||
        !machine_ir_block_append_instruction(&function->blocks[right_ret_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(5);
    if (!machine_ir_block_append_instruction(&function->blocks[left_ret_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[left_ret_id], machine_ir_operand_register(0), error) ||
        !machine_ir_block_append_instruction(&function->blocks[right_ret_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[right_ret_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_shared_inlineable_known_slot_branch_successors_program(MachineIrProgram *program,
    MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t left_id;
    size_t right_id;
    size_t left_br_id;
    size_t right_br_id;
    size_t true_id;
    size_t false_id;
    size_t local_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 0, &local_id, error) ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &left_id, NULL, error) ||
        !machine_ir_function_append_block(function, &right_id, NULL, error) ||
        !machine_ir_function_append_block(function, &left_br_id, NULL, error) ||
        !machine_ir_function_append_block(function, &right_br_id, NULL, error) ||
        !machine_ir_function_append_block(function, &true_id, NULL, error) ||
        !machine_ir_function_append_block(function, &false_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = dup_text("cond");
    instruction.as.call.args = NULL;
    instruction.as.call.arg_count = 0;
    if (!instruction.as.call.callee_name) {
        machine_ir_program_free(program);
        return 0;
    }
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        free(instruction.as.call.callee_name);
        machine_ir_program_free(program);
        return 0;
    }
    free(instruction.as.call.callee_name);

    if (!machine_ir_block_set_branch(&function->blocks[entry_id], machine_ir_operand_register(0), left_id, right_id, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_LOCAL;
    instruction.as.store.slot = machine_ir_slot_local(local_id);
    instruction.as.store.value = machine_ir_operand_immediate(1);
    if (!machine_ir_block_append_instruction(&function->blocks[left_id], &instruction, error) ||
        !machine_ir_block_set_jump(&function->blocks[left_id], left_br_id, error) ||
        !machine_ir_block_append_instruction(&function->blocks[right_id], &instruction, error) ||
        !machine_ir_block_set_jump(&function->blocks[right_id], right_br_id, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(local_id);
    if (!machine_ir_block_append_instruction(&function->blocks[left_br_id], &instruction, error) ||
        !machine_ir_block_append_instruction(&function->blocks[right_br_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_BIT_XOR;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(1);
    if (!machine_ir_block_append_instruction(&function->blocks[left_br_id], &instruction, error) ||
        !machine_ir_block_set_branch(&function->blocks[left_br_id], machine_ir_operand_register(0), true_id, false_id, error) ||
        !machine_ir_block_append_instruction(&function->blocks[right_br_id], &instruction, error) ||
        !machine_ir_block_set_branch(&function->blocks[right_br_id], machine_ir_operand_register(0), true_id, false_id, error) ||
        !machine_ir_block_set_return(&function->blocks[true_id], machine_ir_operand_immediate(1), error) ||
        !machine_ir_block_set_return(&function->blocks[false_id], machine_ir_operand_immediate(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_shared_threaded_inlineable_known_slot_return_successors_program(MachineIrProgram *program,
    MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t left_id;
    size_t right_id;
    size_t left_jump_id;
    size_t right_jump_id;
    size_t left_ret_id;
    size_t right_ret_id;
    size_t local_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 0, &local_id, error) ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &left_id, NULL, error) ||
        !machine_ir_function_append_block(function, &right_id, NULL, error) ||
        !machine_ir_function_append_block(function, &left_jump_id, NULL, error) ||
        !machine_ir_function_append_block(function, &right_jump_id, NULL, error) ||
        !machine_ir_function_append_block(function, &left_ret_id, NULL, error) ||
        !machine_ir_function_append_block(function, &right_ret_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = dup_text("cond");
    instruction.as.call.args = NULL;
    instruction.as.call.arg_count = 0;
    if (!instruction.as.call.callee_name) {
        machine_ir_program_free(program);
        return 0;
    }
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        free(instruction.as.call.callee_name);
        machine_ir_program_free(program);
        return 0;
    }
    free(instruction.as.call.callee_name);

    if (!machine_ir_block_set_branch(&function->blocks[entry_id], machine_ir_operand_register(0), left_id, right_id, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_LOCAL;
    instruction.as.store.slot = machine_ir_slot_local(local_id);
    instruction.as.store.value = machine_ir_operand_immediate(7);
    if (!machine_ir_block_append_instruction(&function->blocks[left_id], &instruction, error) ||
        !machine_ir_block_set_jump(&function->blocks[left_id], left_jump_id, error) ||
        !machine_ir_block_append_instruction(&function->blocks[right_id], &instruction, error) ||
        !machine_ir_block_set_jump(&function->blocks[right_id], right_jump_id, error) ||
        !machine_ir_block_set_jump(&function->blocks[left_jump_id], left_ret_id, error) ||
        !machine_ir_block_set_jump(&function->blocks[right_jump_id], right_ret_id, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(local_id);
    if (!machine_ir_block_append_instruction(&function->blocks[left_ret_id], &instruction, error) ||
        !machine_ir_block_append_instruction(&function->blocks[right_ret_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(5);
    if (!machine_ir_block_append_instruction(&function->blocks[left_ret_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[left_ret_id], machine_ir_operand_register(0), error) ||
        !machine_ir_block_append_instruction(&function->blocks[right_ret_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[right_ret_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_shared_threaded_inlineable_known_slot_branch_successors_program(MachineIrProgram *program,
    MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t left_id;
    size_t right_id;
    size_t left_jump_id;
    size_t right_jump_id;
    size_t left_br_id;
    size_t right_br_id;
    size_t true_id;
    size_t false_id;
    size_t local_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 0, &local_id, error) ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &left_id, NULL, error) ||
        !machine_ir_function_append_block(function, &right_id, NULL, error) ||
        !machine_ir_function_append_block(function, &left_jump_id, NULL, error) ||
        !machine_ir_function_append_block(function, &right_jump_id, NULL, error) ||
        !machine_ir_function_append_block(function, &left_br_id, NULL, error) ||
        !machine_ir_function_append_block(function, &right_br_id, NULL, error) ||
        !machine_ir_function_append_block(function, &true_id, NULL, error) ||
        !machine_ir_function_append_block(function, &false_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = dup_text("cond");
    instruction.as.call.args = NULL;
    instruction.as.call.arg_count = 0;
    if (!instruction.as.call.callee_name) {
        machine_ir_program_free(program);
        return 0;
    }
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        free(instruction.as.call.callee_name);
        machine_ir_program_free(program);
        return 0;
    }
    free(instruction.as.call.callee_name);

    if (!machine_ir_block_set_branch(&function->blocks[entry_id], machine_ir_operand_register(0), left_id, right_id, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_LOCAL;
    instruction.as.store.slot = machine_ir_slot_local(local_id);
    instruction.as.store.value = machine_ir_operand_immediate(1);
    if (!machine_ir_block_append_instruction(&function->blocks[left_id], &instruction, error) ||
        !machine_ir_block_set_jump(&function->blocks[left_id], left_jump_id, error) ||
        !machine_ir_block_append_instruction(&function->blocks[right_id], &instruction, error) ||
        !machine_ir_block_set_jump(&function->blocks[right_id], right_jump_id, error) ||
        !machine_ir_block_set_jump(&function->blocks[left_jump_id], left_br_id, error) ||
        !machine_ir_block_set_jump(&function->blocks[right_jump_id], right_br_id, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(local_id);
    if (!machine_ir_block_append_instruction(&function->blocks[left_br_id], &instruction, error) ||
        !machine_ir_block_append_instruction(&function->blocks[right_br_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_BIT_XOR;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(1);
    if (!machine_ir_block_append_instruction(&function->blocks[left_br_id], &instruction, error) ||
        !machine_ir_block_set_branch(&function->blocks[left_br_id], machine_ir_operand_register(0), true_id, false_id, error) ||
        !machine_ir_block_append_instruction(&function->blocks[right_br_id], &instruction, error) ||
        !machine_ir_block_set_branch(&function->blocks[right_br_id], machine_ir_operand_register(0), true_id, false_id, error) ||
        !machine_ir_block_set_return(&function->blocks[true_id], machine_ir_operand_immediate(1), error) ||
        !machine_ir_block_set_return(&function->blocks[false_id], machine_ir_operand_immediate(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_semantically_equivalent_shared_inlineable_known_slot_return_successors_program(
    MachineIrProgram *program,
    MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t left_id;
    size_t right_id;
    size_t left_ret_id;
    size_t right_ret_id;
    size_t local_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 2;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(2, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;
    program->register_bank.registers[1].register_id = 1;
    program->register_bank.registers[1].name = dup_text("r1");
    program->register_bank.registers[1].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 0, &local_id, error) ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &left_id, NULL, error) ||
        !machine_ir_function_append_block(function, &right_id, NULL, error) ||
        !machine_ir_function_append_block(function, &left_ret_id, NULL, error) ||
        !machine_ir_function_append_block(function, &right_ret_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = dup_text("cond");
    instruction.as.call.args = NULL;
    instruction.as.call.arg_count = 0;
    if (!instruction.as.call.callee_name) {
        machine_ir_program_free(program);
        return 0;
    }
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        free(instruction.as.call.callee_name);
        machine_ir_program_free(program);
        return 0;
    }
    free(instruction.as.call.callee_name);

    if (!machine_ir_block_set_branch(&function->blocks[entry_id], machine_ir_operand_register(0), left_id, right_id, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_LOCAL;
    instruction.as.store.slot = machine_ir_slot_local(local_id);
    instruction.as.store.value = machine_ir_operand_immediate(7);
    if (!machine_ir_block_append_instruction(&function->blocks[left_id], &instruction, error) ||
        !machine_ir_block_set_jump(&function->blocks[left_id], left_ret_id, error) ||
        !machine_ir_block_append_instruction(&function->blocks[right_id], &instruction, error) ||
        !machine_ir_block_set_jump(&function->blocks[right_id], right_ret_id, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(local_id);
    if (!machine_ir_block_append_instruction(&function->blocks[left_ret_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    instruction.result = machine_ir_operand_register(0);
    if (!machine_ir_block_append_instruction(&function->blocks[right_ret_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(5);
    if (!machine_ir_block_append_instruction(&function->blocks[left_ret_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[left_ret_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    instruction.result = machine_ir_operand_register(1);
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    if (!machine_ir_block_append_instruction(&function->blocks[right_ret_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[right_ret_id], machine_ir_operand_register(1), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_semantically_equivalent_shared_inlineable_known_slot_branch_successors_program(
    MachineIrProgram *program,
    MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t left_id;
    size_t right_id;
    size_t left_br_id;
    size_t right_br_id;
    size_t true_id;
    size_t false_id;
    size_t local_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 2;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(2, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;
    program->register_bank.registers[1].register_id = 1;
    program->register_bank.registers[1].name = dup_text("r1");
    program->register_bank.registers[1].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 0, &local_id, error) ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &left_id, NULL, error) ||
        !machine_ir_function_append_block(function, &right_id, NULL, error) ||
        !machine_ir_function_append_block(function, &left_br_id, NULL, error) ||
        !machine_ir_function_append_block(function, &right_br_id, NULL, error) ||
        !machine_ir_function_append_block(function, &true_id, NULL, error) ||
        !machine_ir_function_append_block(function, &false_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = dup_text("cond");
    instruction.as.call.args = NULL;
    instruction.as.call.arg_count = 0;
    if (!instruction.as.call.callee_name) {
        machine_ir_program_free(program);
        return 0;
    }
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        free(instruction.as.call.callee_name);
        machine_ir_program_free(program);
        return 0;
    }
    free(instruction.as.call.callee_name);

    if (!machine_ir_block_set_branch(&function->blocks[entry_id], machine_ir_operand_register(0), left_id, right_id, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_LOCAL;
    instruction.as.store.slot = machine_ir_slot_local(local_id);
    instruction.as.store.value = machine_ir_operand_immediate(1);
    if (!machine_ir_block_append_instruction(&function->blocks[left_id], &instruction, error) ||
        !machine_ir_block_set_jump(&function->blocks[left_id], left_br_id, error) ||
        !machine_ir_block_append_instruction(&function->blocks[right_id], &instruction, error) ||
        !machine_ir_block_set_jump(&function->blocks[right_id], right_br_id, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(local_id);
    if (!machine_ir_block_append_instruction(&function->blocks[left_br_id], &instruction, error) ||
        !machine_ir_block_append_instruction(&function->blocks[right_br_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_BIT_XOR;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(1);
    if (!machine_ir_block_append_instruction(&function->blocks[left_br_id], &instruction, error) ||
        !machine_ir_block_set_branch(&function->blocks[left_br_id], machine_ir_operand_register(0), true_id, false_id, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    instruction.result = machine_ir_operand_register(1);
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    if (!machine_ir_block_append_instruction(&function->blocks[right_br_id], &instruction, error) ||
        !machine_ir_block_set_branch(&function->blocks[right_br_id], machine_ir_operand_register(1), true_id, false_id, error) ||
        !machine_ir_block_set_return(&function->blocks[true_id], machine_ir_operand_immediate(1), error) ||
        !machine_ir_block_set_return(&function->blocks[false_id], machine_ir_operand_immediate(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_constraint_mismatched_equivalent_inlineable_return_branch_program(
    MachineIrProgram *program,
    MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t left_id;
    size_t right_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 2;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(2, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;
    program->register_bank.registers[0].caller_clobbered = 1u;
    program->register_bank.registers[0].callee_preserved = 0u;
    program->register_bank.registers[1].register_id = 1;
    program->register_bank.registers[1].name = dup_text("r1");
    program->register_bank.registers[1].allocatable = 1u;
    program->register_bank.registers[1].caller_clobbered = 0u;
    program->register_bank.registers[1].callee_preserved = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &left_id, NULL, error) ||
        !machine_ir_function_append_block(function, &right_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = dup_text("cond");
    instruction.as.call.args = NULL;
    instruction.as.call.arg_count = 0;
    if (!instruction.as.call.callee_name) {
        machine_ir_program_free(program);
        return 0;
    }
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        free(instruction.as.call.callee_name);
        machine_ir_program_free(program);
        return 0;
    }
    free(instruction.as.call.callee_name);

    if (!machine_ir_block_set_branch(&function->blocks[entry_id], machine_ir_operand_register(0), 1, 2, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(2);
    if (!machine_ir_block_append_instruction(&function->blocks[left_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[left_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    instruction.result = machine_ir_operand_register(1);
    if (!machine_ir_block_append_instruction(&function->blocks[right_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[right_id], machine_ir_operand_register(1), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_internal_call_preserves_unrelated_global_wrapper_program(MachineIrProgram *program,
    MachineIrError *error) {
    MachineIrFunction *reader = NULL;
    MachineIrFunction *main_fn = NULL;
    size_t reader_entry_id;
    size_t main_entry_id;
    size_t wrapper_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_global(program, "g0", NULL, error) ||
        !machine_ir_program_append_global(program, "g1", NULL, error) ||
        !machine_ir_program_append_function(program, "reader", 1, &reader, error) ||
        !reader ||
        !machine_ir_function_append_block(reader, &reader_entry_id, NULL, error) ||
        !machine_ir_program_append_function(program, "main", 1, &main_fn, error) ||
        !main_fn ||
        !machine_ir_function_append_block(main_fn, &main_entry_id, NULL, error) ||
        !machine_ir_function_append_block(main_fn, &wrapper_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_global(0);
    if (!machine_ir_block_append_instruction(&reader->blocks[reader_entry_id], &instruction, error) ||
        !machine_ir_block_set_return(&reader->blocks[reader_entry_id], machine_ir_operand_immediate(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = machine_ir_slot_global(1);
    instruction.as.store.value = machine_ir_operand_immediate(7);
    if (!machine_ir_block_append_instruction(&main_fn->blocks[main_entry_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = dup_text("reader");
    instruction.as.call.args = NULL;
    instruction.as.call.arg_count = 0;
    if (!instruction.as.call.callee_name) {
        machine_ir_program_free(program);
        return 0;
    }
    if (!machine_ir_block_append_instruction(&main_fn->blocks[main_entry_id], &instruction, error) ||
        !machine_ir_block_set_jump(&main_fn->blocks[main_entry_id], wrapper_id, error)) {
        free(instruction.as.call.callee_name);
        machine_ir_program_free(program);
        return 0;
    }
    free(instruction.as.call.callee_name);

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_global(1);
    if (!machine_ir_block_append_instruction(&main_fn->blocks[wrapper_id], &instruction, error) ||
        !machine_ir_block_set_return(&main_fn->blocks[wrapper_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_internal_call_preserves_unrelated_global_inlineable_wrapper_program(
    MachineIrProgram *program,
    MachineIrError *error) {
    MachineIrFunction *reader = NULL;
    MachineIrFunction *main_fn = NULL;
    size_t reader_entry_id;
    size_t main_entry_id;
    size_t wrapper_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_global(program, "g0", NULL, error) ||
        !machine_ir_program_append_global(program, "g1", NULL, error) ||
        !machine_ir_program_append_function(program, "reader", 1, &reader, error) ||
        !reader ||
        !machine_ir_function_append_block(reader, &reader_entry_id, NULL, error) ||
        !machine_ir_program_append_function(program, "main", 1, &main_fn, error) ||
        !main_fn ||
        !machine_ir_function_append_block(main_fn, &main_entry_id, NULL, error) ||
        !machine_ir_function_append_block(main_fn, &wrapper_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_global(0);
    if (!machine_ir_block_append_instruction(&reader->blocks[reader_entry_id], &instruction, error) ||
        !machine_ir_block_set_return(&reader->blocks[reader_entry_id], machine_ir_operand_immediate(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = machine_ir_slot_global(1);
    instruction.as.store.value = machine_ir_operand_immediate(7);
    if (!machine_ir_block_append_instruction(&main_fn->blocks[main_entry_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = dup_text("reader");
    instruction.as.call.args = NULL;
    instruction.as.call.arg_count = 0;
    if (!instruction.as.call.callee_name) {
        machine_ir_program_free(program);
        return 0;
    }
    if (!machine_ir_block_append_instruction(&main_fn->blocks[main_entry_id], &instruction, error) ||
        !machine_ir_block_set_jump(&main_fn->blocks[main_entry_id], wrapper_id, error)) {
        free(instruction.as.call.callee_name);
        machine_ir_program_free(program);
        return 0;
    }
    free(instruction.as.call.callee_name);

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_global(1);
    if (!machine_ir_block_append_instruction(&main_fn->blocks[wrapper_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(5);
    if (!machine_ir_block_append_instruction(&main_fn->blocks[wrapper_id], &instruction, error) ||
        !machine_ir_block_set_return(&main_fn->blocks[wrapper_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_internal_call_written_global_blocks_wrapper_program(MachineIrProgram *program,
    MachineIrError *error) {
    MachineIrFunction *writer = NULL;
    MachineIrFunction *main_fn = NULL;
    size_t writer_entry_id;
    size_t main_entry_id;
    size_t wrapper_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_global(program, "g0", NULL, error) ||
        !machine_ir_program_append_function(program, "writer", 1, &writer, error) ||
        !writer ||
        !machine_ir_function_append_block(writer, &writer_entry_id, NULL, error) ||
        !machine_ir_program_append_function(program, "main", 1, &main_fn, error) ||
        !main_fn ||
        !machine_ir_function_append_block(main_fn, &main_entry_id, NULL, error) ||
        !machine_ir_function_append_block(main_fn, &wrapper_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = machine_ir_slot_global(0);
    instruction.as.store.value = machine_ir_operand_immediate(9);
    if (!machine_ir_block_append_instruction(&writer->blocks[writer_entry_id], &instruction, error) ||
        !machine_ir_block_set_return(&writer->blocks[writer_entry_id], machine_ir_operand_immediate(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    instruction.as.store.value = machine_ir_operand_immediate(7);
    if (!machine_ir_block_append_instruction(&main_fn->blocks[main_entry_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = dup_text("writer");
    instruction.as.call.args = NULL;
    instruction.as.call.arg_count = 0;
    if (!instruction.as.call.callee_name) {
        machine_ir_program_free(program);
        return 0;
    }
    if (!machine_ir_block_append_instruction(&main_fn->blocks[main_entry_id], &instruction, error) ||
        !machine_ir_block_set_jump(&main_fn->blocks[main_entry_id], wrapper_id, error)) {
        free(instruction.as.call.callee_name);
        machine_ir_program_free(program);
        return 0;
    }
    free(instruction.as.call.callee_name);

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_global(0);
    if (!machine_ir_block_append_instruction(&main_fn->blocks[wrapper_id], &instruction, error) ||
        !machine_ir_block_set_return(&main_fn->blocks[wrapper_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}


static int build_machine_ir_threaded_known_slot_return_wrapper_program(MachineIrProgram *program,
    MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t jump_id;
    size_t wrapper_id;
    size_t local_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 0, &local_id, error) ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &jump_id, NULL, error) ||
        !machine_ir_function_append_block(function, &wrapper_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_LOCAL;
    instruction.as.store.slot = machine_ir_slot_local(local_id);
    instruction.as.store.value = machine_ir_operand_immediate(7);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !machine_ir_block_set_jump(&function->blocks[entry_id], jump_id, error) ||
        !machine_ir_block_set_jump(&function->blocks[jump_id], wrapper_id, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(local_id);
    if (!machine_ir_block_append_instruction(&function->blocks[wrapper_id], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[wrapper_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_threaded_known_slot_branch_wrapper_program(MachineIrProgram *program,
    MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t jump_id;
    size_t wrapper_id;
    size_t true_id;
    size_t false_id;
    size_t local_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 0, &local_id, error) ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &jump_id, NULL, error) ||
        !machine_ir_function_append_block(function, &wrapper_id, NULL, error) ||
        !machine_ir_function_append_block(function, &true_id, NULL, error) ||
        !machine_ir_function_append_block(function, &false_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_LOCAL;
    instruction.as.store.slot = machine_ir_slot_local(local_id);
    instruction.as.store.value = machine_ir_operand_immediate(1);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !machine_ir_block_set_jump(&function->blocks[entry_id], jump_id, error) ||
        !machine_ir_block_set_jump(&function->blocks[jump_id], wrapper_id, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(local_id);
    if (!machine_ir_block_append_instruction(&function->blocks[wrapper_id], &instruction, error) ||
        !machine_ir_block_set_branch(&function->blocks[wrapper_id], machine_ir_operand_register(0), true_id, false_id, error) ||
        !machine_ir_block_set_return(&function->blocks[true_id], machine_ir_operand_immediate(1), error) ||
        !machine_ir_block_set_return(&function->blocks[false_id], machine_ir_operand_immediate(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_internal_call_preserves_unrelated_global_fact_program(MachineIrProgram *program,
    MachineIrError *error) {
    MachineIrFunction *reader = NULL;
    MachineIrFunction *main_fn = NULL;
    size_t reader_entry_id;
    size_t main_entry_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_global(program, "g0", NULL, error) ||
        !machine_ir_program_append_global(program, "g1", NULL, error) ||
        !machine_ir_program_append_function(program, "reader", 1, &reader, error) ||
        !reader ||
        !machine_ir_function_append_block(reader, &reader_entry_id, NULL, error) ||
        !machine_ir_program_append_function(program, "main", 1, &main_fn, error) ||
        !main_fn ||
        !machine_ir_function_append_block(main_fn, &main_entry_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_global(0);
    if (!machine_ir_block_append_instruction(&reader->blocks[reader_entry_id], &instruction, error) ||
        !machine_ir_block_set_return(&reader->blocks[reader_entry_id], machine_ir_operand_immediate(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = machine_ir_slot_global(1);
    instruction.as.store.value = machine_ir_operand_immediate(7);
    if (!machine_ir_block_append_instruction(&main_fn->blocks[main_entry_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = dup_text("reader");
    instruction.as.call.args = NULL;
    instruction.as.call.arg_count = 0;
    if (!instruction.as.call.callee_name) {
        machine_ir_program_free(program);
        return 0;
    }
    if (!machine_ir_block_append_instruction(&main_fn->blocks[main_entry_id], &instruction, error)) {
        free(instruction.as.call.callee_name);
        machine_ir_program_free(program);
        return 0;
    }
    free(instruction.as.call.callee_name);

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_global(1);
    if (!machine_ir_block_append_instruction(&main_fn->blocks[main_entry_id], &instruction, error) ||
        !machine_ir_block_set_return(&main_fn->blocks[main_entry_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_internal_call_does_not_observe_unrelated_global_store_program(MachineIrProgram *program,
    MachineIrError *error) {
    MachineIrFunction *writer = NULL;
    MachineIrFunction *main_fn = NULL;
    size_t writer_entry_id;
    size_t main_entry_id;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_global(program, "g0", NULL, error) ||
        !machine_ir_program_append_global(program, "g1", NULL, error) ||
        !machine_ir_program_append_function(program, "writer", 1, &writer, error) ||
        !writer ||
        !machine_ir_function_append_block(writer, &writer_entry_id, NULL, error) ||
        !machine_ir_program_append_function(program, "main", 1, &main_fn, error) ||
        !main_fn ||
        !machine_ir_function_append_block(main_fn, &main_entry_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = machine_ir_slot_global(0);
    instruction.as.store.value = machine_ir_operand_immediate(1);
    if (!machine_ir_block_append_instruction(&writer->blocks[writer_entry_id], &instruction, error) ||
        !machine_ir_block_set_return(&writer->blocks[writer_entry_id], machine_ir_operand_immediate(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = machine_ir_slot_global(1);
    instruction.as.store.value = machine_ir_operand_immediate(7);
    if (!machine_ir_block_append_instruction(&main_fn->blocks[main_entry_id], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = dup_text("writer");
    instruction.as.call.args = NULL;
    instruction.as.call.arg_count = 0;
    if (!instruction.as.call.callee_name) {
        machine_ir_program_free(program);
        return 0;
    }
    if (!machine_ir_block_append_instruction(&main_fn->blocks[main_entry_id], &instruction, error)) {
        free(instruction.as.call.callee_name);
        machine_ir_program_free(program);
        return 0;
    }
    free(instruction.as.call.callee_name);

    if (!machine_ir_block_set_return(&main_fn->blocks[main_entry_id], machine_ir_operand_immediate(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int test_machine_ir_cleanup_propagates_copies_into_unique_branch_successor(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_branch_side_cross_block_copy_program_impl(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: branch-side copy cleanup setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: branch-side copy cleanup dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!strstr(actual_text, "bb.1:\n    ret 11\n") ||
        strstr(actual_text, "reg.0(r0) = add reg.1(r1), 5")) {
        fprintf(stderr,
            "[machine-ir] FAIL: branch-side copy cleanup should propagate into unique successor\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_propagates_slot_values_into_unique_branch_successor(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_branch_side_unique_successor_slot_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: branch-side slot cleanup setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: branch-side slot cleanup dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!strstr(actual_text, "call cond()") ||
        !strstr(actual_text, "bb.1:\n    ret 7\n") ||
        strstr(actual_text, "load_local") != NULL) {
        fprintf(stderr,
            "[machine-ir] FAIL: branch-side slot cleanup should propagate into unique successor\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_slot_fact_follows_copy_chain(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_slot_fact_follows_copy_chain_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: slot-copy-chain cleanup setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: slot-copy-chain cleanup dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (strstr(actual_text, "load_local") != NULL ||
        strstr(actual_text, "store local.0") != NULL ||
        !strstr(actual_text, "ret 7")) {
        fprintf(stderr,
            "[machine-ir] FAIL: slot facts should follow copy chains through cleanup\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_folds_known_slot_return_wrapper(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_known_slot_return_wrapper_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: known-slot-return-wrapper setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: known-slot-return-wrapper dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (strstr(actual_text, "load_local") != NULL ||
        !strstr(actual_text, "ret 7")) {
        fprintf(stderr,
            "[machine-ir] FAIL: known-slot return wrapper should fold to direct return\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_folds_branch_side_known_slot_return_wrappers(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_branch_side_known_slot_return_wrappers_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: branch-side-known-slot-wrapper setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: branch-side-known-slot-wrapper dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!strstr(actual_text, "call cond()") ||
        !strstr(actual_text, "bb.1:\n    ret 1\n") ||
        !strstr(actual_text, "bb.2:\n    ret 2\n") ||
        strstr(actual_text, "load_local") != NULL) {
        fprintf(stderr,
            "[machine-ir] FAIL: branch-side known-slot wrappers should fold per predecessor\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_folds_known_slot_branch_wrapper(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_known_slot_branch_wrapper_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: known-slot-branch-wrapper setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: known-slot-branch-wrapper dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (strstr(actual_text, "load_local") != NULL ||
        !strstr(actual_text, "ret 1")) {
        fprintf(stderr,
            "[machine-ir] FAIL: known-slot branch wrapper should fold to direct branch\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_folds_branch_side_known_slot_branch_wrappers(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_branch_side_known_slot_branch_wrappers_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: branch-side-known-slot-branch-wrapper setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: branch-side-known-slot-branch-wrapper dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!strstr(actual_text, "call cond()") ||
        !strstr(actual_text, "bb.1:\n    ret 1\n") ||
        !strstr(actual_text, "bb.2:\n    ret 0\n") ||
        strstr(actual_text, "load_local") != NULL) {
        fprintf(stderr,
            "[machine-ir] FAIL: branch-side known-slot branch wrappers should fold per predecessor\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_folds_shared_known_slot_return_successors(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_shared_known_slot_return_successors_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: shared-known-slot-ret-successors setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: shared-known-slot-ret-successors dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!strstr(actual_text, "call cond()") ||
        strstr(actual_text, "load_local") != NULL ||
        !strstr(actual_text, "ret 7")) {
        fprintf(stderr,
            "[machine-ir] FAIL: shared known-slot return successors should collapse to direct return\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_folds_shared_known_slot_branch_successors(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_shared_known_slot_branch_successors_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: shared-known-slot-branch-successors setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: shared-known-slot-branch-successors dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!strstr(actual_text, "call cond()") ||
        strstr(actual_text, "load_local") != NULL ||
        !strstr(actual_text, "ret 1")) {
        fprintf(stderr,
            "[machine-ir] FAIL: shared known-slot branch successors should collapse to direct return\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_folds_inlineable_known_slot_return_wrapper(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_inlineable_known_slot_return_wrapper_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: inlineable-known-slot-ret-wrapper setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: inlineable-known-slot-ret-wrapper dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (strstr(actual_text, "load_local") != NULL ||
        strstr(actual_text, " add ") != NULL ||
        !strstr(actual_text, "ret 12")) {
        fprintf(stderr,
            "[machine-ir] FAIL: inlineable known-slot return wrapper should fold through value op\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_keeps_nonempty_known_slot_return_tail_body(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_nonempty_known_slot_return_tail_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: nonempty-known-slot-ret-tail setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: nonempty-known-slot-ret-tail dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (strstr(actual_text, "store global.0") == NULL ||
        strstr(actual_text, "reg.0(r0) = add reg.0(r0), 5") == NULL ||
        strstr(actual_text, "ret reg.0(r0)") == NULL) {
        fprintf(stderr,
            "[machine-ir] FAIL: nonempty known-slot return tail should keep predecessor body intact\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_folds_inlineable_known_slot_branch_wrapper(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_inlineable_known_slot_branch_wrapper_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: inlineable-known-slot-branch-wrapper setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: inlineable-known-slot-branch-wrapper dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (strstr(actual_text, "load_local") != NULL ||
        strstr(actual_text, " xor ") != NULL ||
        !strstr(actual_text, "ret 0")) {
        fprintf(stderr,
            "[machine-ir] FAIL: inlineable known-slot branch wrapper should fold through value op\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_keeps_nonempty_known_slot_branch_tail_local_store(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_nonempty_known_slot_branch_tail_with_local_store_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: nonempty-known-slot-branch-tail-local-store setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: nonempty-known-slot-branch-tail-local-store dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (strstr(actual_text, "store local.1, reg.0") == NULL) {
        fprintf(stderr,
            "[machine-ir] FAIL: nonempty known-slot branch tail should keep predecessor local store body\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_folds_shared_inlineable_known_slot_return_successors(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_shared_inlineable_known_slot_return_successors_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: shared-inlineable-known-slot-ret-successors setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: shared-inlineable-known-slot-ret-successors dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!strstr(actual_text, "call cond()") ||
        strstr(actual_text, "load_local") != NULL ||
        strstr(actual_text, " add ") != NULL ||
        !strstr(actual_text, "ret 12")) {
        fprintf(stderr,
            "[machine-ir] FAIL: shared inlineable known-slot return successors should collapse through value op\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_folds_shared_inlineable_known_slot_branch_successors(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_shared_inlineable_known_slot_branch_successors_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: shared-inlineable-known-slot-branch-successors setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: shared-inlineable-known-slot-branch-successors dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!strstr(actual_text, "call cond()") ||
        strstr(actual_text, "load_local") != NULL ||
        strstr(actual_text, " xor ") != NULL ||
        !strstr(actual_text, "ret 0")) {
        fprintf(stderr,
            "[machine-ir] FAIL: shared inlineable known-slot branch successors should collapse through value op\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_folds_shared_threaded_inlineable_known_slot_return_successors(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_shared_threaded_inlineable_known_slot_return_successors_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: shared-threaded-inlineable-known-slot-ret-successors setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: shared-threaded-inlineable-known-slot-ret-successors dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!strstr(actual_text, "call cond()") ||
        strstr(actual_text, "load_local") != NULL ||
        strstr(actual_text, " add ") != NULL ||
        !strstr(actual_text, "ret 12")) {
        fprintf(stderr,
            "[machine-ir] FAIL: shared threaded inlineable known-slot return successors should collapse\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_folds_shared_threaded_inlineable_known_slot_branch_successors(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_shared_threaded_inlineable_known_slot_branch_successors_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: shared-threaded-inlineable-known-slot-branch-successors setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: shared-threaded-inlineable-known-slot-branch-successors dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!strstr(actual_text, "call cond()") ||
        strstr(actual_text, "load_local") != NULL ||
        strstr(actual_text, " xor ") != NULL ||
        !strstr(actual_text, "ret 0")) {
        fprintf(stderr,
            "[machine-ir] FAIL: shared threaded inlineable known-slot branch successors should collapse\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_folds_semantically_equivalent_shared_inlineable_known_slot_return_successors(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_semantically_equivalent_shared_inlineable_known_slot_return_successors_program(
            &program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: semantically-equivalent-shared-inlineable-ret setup failed: %s\n",
            error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: semantically-equivalent-shared-inlineable-ret dump failed: %s\n",
            error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!strstr(actual_text, "call cond()") ||
        strstr(actual_text, "load_local") != NULL ||
        strstr(actual_text, " add ") != NULL ||
        !strstr(actual_text, "ret 12")) {
        fprintf(stderr,
            "[machine-ir] FAIL: semantically equivalent shared inlineable return successors should collapse\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_folds_semantically_equivalent_shared_inlineable_known_slot_branch_successors(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_semantically_equivalent_shared_inlineable_known_slot_branch_successors_program(
            &program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: semantically-equivalent-shared-inlineable-branch setup failed: %s\n",
            error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: semantically-equivalent-shared-inlineable-branch dump failed: %s\n",
            error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!strstr(actual_text, "call cond()") ||
        strstr(actual_text, "load_local") != NULL ||
        strstr(actual_text, " xor ") != NULL ||
        !strstr(actual_text, "ret 0")) {
        fprintf(stderr,
            "[machine-ir] FAIL: semantically equivalent shared inlineable branch successors should collapse\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_keeps_constraint_mismatched_equivalent_inlineable_return_branch_successors(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_constraint_mismatched_equivalent_inlineable_return_branch_program(
            &program, &error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: constraint-mismatched-inlineable-return-branch setup failed: %s\n",
            error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr,
            "[machine-ir] FAIL: constraint-mismatched-inlineable-return-branch dump failed: %s\n",
            error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!strstr(actual_text, "reg.0(r0) = call cond()") ||
        !strstr(actual_text, "br reg.0(r0), bb.1, bb.2") ||
        !strstr(actual_text, "bb.1:\n    reg.0(r0) = add reg.0(r0), 2\n    ret reg.0(r0)\n") ||
        !strstr(actual_text, "bb.2:\n    reg.1(r1) = add reg.0(r0), 2\n    ret reg.1(r1)\n") ||
        strstr(actual_text, "bb.0:\n    reg.0(r0) = add reg.0(r0), 2\n    ret reg.0(r0)\n")) {
        fprintf(stderr,
            "[machine-ir] FAIL: constraint-mismatched inlineable return branch successors should not collapse\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_internal_call_preserves_unrelated_global_return_wrapper(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_internal_call_preserves_unrelated_global_wrapper_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: internal-call-unrelated-global-ret-wrapper setup failed: %s\n",
            error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: internal-call-unrelated-global-ret-wrapper dump failed: %s\n",
            error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!strstr(actual_text, "call reader()") ||
        strstr(actual_text, "load global.1") != NULL ||
        !strstr(actual_text, "ret 7")) {
        fprintf(stderr,
            "[machine-ir] FAIL: internal call should preserve unrelated global fact through return wrapper\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_internal_call_preserves_unrelated_global_inlineable_wrapper(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_internal_call_preserves_unrelated_global_inlineable_wrapper_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: internal-call-unrelated-global-inlineable-wrapper setup failed: %s\n",
            error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: internal-call-unrelated-global-inlineable-wrapper dump failed: %s\n",
            error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (strstr(actual_text, "load global.1") != NULL ||
        strstr(actual_text, " add ") != NULL ||
        !strstr(actual_text, "ret 12")) {
        fprintf(stderr,
            "[machine-ir] FAIL: internal call should preserve unrelated global fact through inlineable wrapper\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_internal_call_written_global_blocks_wrapper_fold(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_internal_call_written_global_blocks_wrapper_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: internal-call-written-global-wrapper setup failed: %s\n",
            error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: internal-call-written-global-wrapper dump failed: %s\n",
            error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!strstr(actual_text, "call writer()") ||
        !strstr(actual_text, "load global.0") ||
        strstr(actual_text, "ret 7")) {
        fprintf(stderr,
            "[machine-ir] FAIL: internal call writing same global should block wrapper folding\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}


static int test_machine_ir_cleanup_folds_threaded_known_slot_return_wrapper(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_threaded_known_slot_return_wrapper_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: threaded-known-slot-ret-wrapper setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: threaded-known-slot-ret-wrapper dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (strstr(actual_text, "load_local") != NULL ||
        !strstr(actual_text, "ret 7")) {
        fprintf(stderr,
            "[machine-ir] FAIL: threaded known-slot return wrapper should fold through jump chain\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_folds_threaded_known_slot_branch_wrapper(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_threaded_known_slot_branch_wrapper_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: threaded-known-slot-branch-wrapper setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: threaded-known-slot-branch-wrapper dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (strstr(actual_text, "load_local") != NULL ||
        !strstr(actual_text, "ret 1")) {
        fprintf(stderr,
            "[machine-ir] FAIL: threaded known-slot branch wrapper should fold through jump chain\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_internal_call_preserves_unrelated_global_fact(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_internal_call_preserves_unrelated_global_fact_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: internal-call-global-preserve setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: internal-call-global-preserve dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!strstr(actual_text, "call reader()") ||
        !strstr(actual_text, "function main") ||
        strstr(actual_text, "load global.1") != NULL ||
        !strstr(actual_text, "ret 7")) {
        fprintf(stderr,
            "[machine-ir] FAIL: internal call should preserve unrelated global known fact\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int test_machine_ir_cleanup_internal_call_still_preserves_final_global_store(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_internal_call_does_not_observe_unrelated_global_store_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: internal-call-final-global setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_cleanup_after_phi_elimination_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: internal-call-final-global dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!strstr(actual_text, "store global.1, 7") ||
        !strstr(actual_text, "call writer()")) {
        fprintf(stderr,
            "[machine-ir] FAIL: final global store should stay externally observable even across unrelated internal call\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int build_machine_ir_critical_edge_phi_program(MachineIrProgram *program, MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t left_id;
    size_t right_id;
    size_t other_id;
    size_t join_id;
    size_t exit_id;
    MachineIrPhi *phi = NULL;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &left_id, NULL, error) ||
        !machine_ir_function_append_block(function, &right_id, NULL, error) ||
        !machine_ir_function_append_block(function, &other_id, NULL, error) ||
        !machine_ir_function_append_block(function, &join_id, NULL, error) ||
        !machine_ir_function_append_block(function, &exit_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_immediate(1);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !machine_ir_block_set_branch(&function->blocks[entry_id], machine_ir_operand_register(0), 1, 2, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    instruction.as.mov_value = machine_ir_operand_immediate(10);
    if (!machine_ir_block_append_instruction(&function->blocks[left_id], &instruction, error) ||
        !machine_ir_block_set_branch(&function->blocks[left_id], machine_ir_operand_register(0), 4, 3, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    instruction.as.mov_value = machine_ir_operand_immediate(20);
    if (!machine_ir_block_append_instruction(&function->blocks[right_id], &instruction, error) ||
        !machine_ir_block_set_jump(&function->blocks[right_id], 4, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    instruction.as.mov_value = machine_ir_operand_immediate(30);
    if (!machine_ir_block_append_instruction(&function->blocks[other_id], &instruction, error) ||
        !machine_ir_block_set_jump(&function->blocks[other_id], 5, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    if (!machine_ir_block_append_phi(&function->blocks[join_id], machine_ir_operand_register(0), NULL, &phi, error) || !phi ||
        !machine_ir_phi_append_input(phi, 1, machine_ir_operand_register(0), error) ||
        !machine_ir_phi_append_input(phi, 2, machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }
    if (!machine_ir_block_set_jump(&function->blocks[join_id], 5, error) ||
        !machine_ir_block_set_return(&function->blocks[exit_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int test_machine_ir_phi_elimination_critical_edge_dump(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_critical_edge_phi_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: critical-edge phi-elim setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_eliminate_phi_nodes_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: critical-edge phi-elim dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (strstr(actual_text, "phi ") ||
        strstr(actual_text, "bb.6:") ||
        !strstr(actual_text, "bb.1:\n    reg.0(r0) = mov 10\n    br reg.0(r0), bb.4, bb.3") ||
        !strstr(actual_text, "bb.2:\n    reg.0(r0) = mov 20\n    jmp bb.4") ||
        !strstr(actual_text, "bb.4:\n    jmp bb.5")) {
        fprintf(stderr,
            "[machine-ir] FAIL: critical-edge phi-elim dump missing expected split-edge shape\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

static int build_machine_ir_trivial_critical_phi_program(MachineIrProgram *program, MachineIrError *error) {
    MachineIrFunction *function = NULL;
    size_t entry_id;
    size_t left_id;
    size_t right_id;
    size_t other_id;
    size_t join_id;
    size_t exit_id;
    MachineIrPhi *phi = NULL;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_block(function, &entry_id, NULL, error) ||
        !machine_ir_function_append_block(function, &left_id, NULL, error) ||
        !machine_ir_function_append_block(function, &right_id, NULL, error) ||
        !machine_ir_function_append_block(function, &other_id, NULL, error) ||
        !machine_ir_function_append_block(function, &join_id, NULL, error) ||
        !machine_ir_function_append_block(function, &exit_id, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_immediate(1);
    if (!machine_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !machine_ir_block_set_branch(&function->blocks[entry_id], machine_ir_operand_register(0), 1, 2, error)) {
        machine_ir_program_free(program);
        return 0;
    }
    if (!machine_ir_block_set_branch(&function->blocks[left_id], machine_ir_operand_register(0), 4, 3, error) ||
        !machine_ir_block_set_jump(&function->blocks[right_id], 4, error) ||
        !machine_ir_block_set_jump(&function->blocks[other_id], 5, error)) {
        machine_ir_program_free(program);
        return 0;
    }
    if (!machine_ir_block_append_phi(&function->blocks[join_id], machine_ir_operand_register(0), NULL, &phi, error) || !phi ||
        !machine_ir_phi_append_input(phi, 1, machine_ir_operand_register(0), error) ||
        !machine_ir_phi_append_input(phi, 2, machine_ir_operand_register(0), error) ||
        !machine_ir_block_set_jump(&function->blocks[join_id], 5, error) ||
        !machine_ir_block_set_return(&function->blocks[exit_id], machine_ir_operand_register(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int test_machine_ir_phi_elimination_skips_trivial_critical_edge_split(void) {
    MachineIrProgram program;
    MachineIrError error;
    char *actual_text = NULL;
    int ok = 1;

    machine_ir_program_init(&program);
    if (!build_machine_ir_trivial_critical_phi_program(&program, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: trivial-critical setup failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (!machine_ir_eliminate_phi_nodes_dump(&program, &actual_text, &error)) {
        fprintf(stderr, "[machine-ir] FAIL: trivial-critical phi-elim dump failed: %s\n", error.message);
        machine_ir_program_free(&program);
        return 0;
    }
    if (strstr(actual_text, "phi ") ||
        strstr(actual_text, "bb.6:") ||
        !strstr(actual_text, "bb.1:\n    br reg.0(r0), bb.4, bb.3") ||
        !strstr(actual_text, "bb.4:\n    jmp bb.5")) {
        fprintf(stderr,
            "[machine-ir] FAIL: trivial-critical phi-elim should avoid unnecessary split block\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&program);
    return ok;
}

int main(void) {
    const char *filter = getenv("MACHINE_IR_REG_FILTER");

    if (filter && filter[0] != '\0') {
        if (strstr("MACHINE-IR-FLOAT-TRANSPORT", filter) != NULL) {
            return test_machine_ir_translation_only_preserves_float_metadata() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-SOURCE-TRANSPORT", filter) != NULL) {
            return test_machine_ir_translation_only_lowers_extension_float_transport() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-GLOBAL-LITERAL-RUNTIME-INIT", filter) != NULL) {
            return test_machine_ir_translation_only_lowers_extension_float_global_literal_runtime_init() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-REPORT-QUERY", filter) != NULL) {
            return test_machine_ir_translation_only_float_report_query_surface() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-GLOBAL-LITERAL-REPORT-QUERY", filter) != NULL) {
            return test_machine_ir_translation_only_float_global_literal_report_query_surface() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-ASSIGN-LIVE-DEFAULT", filter) != NULL) {
            return test_machine_ir_default_pipeline_preserves_live_extension_float_assignment_transport() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-RETURN-GLOBAL-LIVE-DEFAULT", filter) != NULL) {
            return test_machine_ir_default_pipeline_preserves_float_return_transport_from_global() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-RETURN-GLOBAL-CALL-LIVE-DEFAULT", filter) != NULL) {
            return test_machine_ir_default_pipeline_preserves_float_return_transport_from_global_call() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-GLOBAL-IDENT-INIT-DEFAULT", filter) != NULL) {
            return test_machine_ir_default_pipeline_preserves_float_global_identifier_runtime_init() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-GLOBAL-CALL-INIT-DEFAULT", filter) != NULL) {
            return test_machine_ir_default_pipeline_preserves_float_global_call_runtime_init() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-GLOBAL-OP-REJECT", filter) != NULL) {
            return test_machine_ir_rejects_global_float_operator_expression_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-GLOBAL-OP-INIT-REJECT", filter) != NULL) {
            return test_machine_ir_rejects_global_float_operator_expression_in_initializer_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-GLOBAL-CALL-INIT-TYPE-REJECT", filter) != NULL) {
            return test_machine_ir_rejects_global_float_call_result_in_initializer_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-ASSIGN-TYPE-REJECT", filter) != NULL) {
            return test_machine_ir_rejects_float_assignment_to_int_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-TERNARY-VALUE-RETURN-INT-REJECT", filter) != NULL) {
            return test_machine_ir_rejects_float_ternary_value_return_to_int_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-UNARY-CALL-TERNARY-RETURN-INT-REJECT", filter) != NULL) {
            return test_machine_ir_rejects_unary_call_ternary_value_return_to_int_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-IF-COND-ACCEPT", filter) != NULL) {
            return test_machine_ir_accepts_float_if_condition_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-WHILE-COND-ACCEPT", filter) != NULL) {
            return test_machine_ir_accepts_float_while_condition_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-FOR-COND-ACCEPT", filter) != NULL) {
            return test_machine_ir_accepts_float_for_condition_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-RECURSIVE-IF-COND-ACCEPT", filter) != NULL) {
            return test_machine_ir_accepts_recursive_float_if_condition_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-CONVERT-RECURSIVE-COND-ACCEPT", filter) != NULL) {
            return test_machine_ir_accepts_recursive_explicit_float_condition_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-CONVERT-WHILE-COND-ACCEPT", filter) != NULL) {
            return test_machine_ir_accepts_explicit_float_while_condition_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-CONVERT-LOGIC-COND-ACCEPT", filter) != NULL) {
            return test_machine_ir_accepts_explicit_float_logical_condition_composition_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-RECURSIVE-COND-TERNARY-NEIGHBOR-REJECT", filter) != NULL) {
            return test_machine_ir_rejects_recursive_float_condition_with_ternary_neighbor_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-LOGICAL-COND-COMPOSE-ACCEPT", filter) != NULL) {
            return test_machine_ir_accepts_float_logical_condition_composition_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-EQ-COMPARE-ACCEPT", filter) != NULL) {
            return test_machine_ir_accepts_float_equality_compare_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-LT-COMPARE-ACCEPT", filter) != NULL) {
            return test_machine_ir_accepts_float_relational_compare_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-CHAIN-ADD-EQ-COMPARE-ACCEPT", filter) != NULL) {
            return test_machine_ir_accepts_chained_float_equality_compare_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-CHAIN-ADD-LT-COMPARE-ACCEPT", filter) != NULL) {
            return test_machine_ir_accepts_chained_float_relational_compare_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-CHAIN-ADD-NE-COMPARE-ACCEPT", filter) != NULL) {
            return test_machine_ir_accepts_chained_float_inequality_compare_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-CHAIN-ADD-LE-COMPARE-ACCEPT", filter) != NULL) {
            return test_machine_ir_accepts_chained_float_le_compare_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-NESTED-MUL-DIV-EQ-COMPARE-ACCEPT", filter) != NULL) {
            return test_machine_ir_accepts_nested_muldiv_float_equality_compare_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-NESTED-MUL-DIV-LT-COMPARE-ACCEPT", filter) != NULL) {
            return test_machine_ir_accepts_nested_muldiv_float_relational_compare_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-NESTED-MUL-DIV-NE-COMPARE-ACCEPT", filter) != NULL) {
            return test_machine_ir_accepts_nested_muldiv_float_inequality_compare_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-NESTED-MUL-DIV-GE-COMPARE-ACCEPT", filter) != NULL) {
            return test_machine_ir_accepts_nested_muldiv_float_ge_compare_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-SIGNED-ZERO-EQ-ACCEPT", filter) != NULL) {
            return test_machine_ir_accepts_signed_zero_float_equality_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-NEG-LT-ZERO-ACCEPT", filter) != NULL) {
            return test_machine_ir_accepts_negative_float_relational_compare_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-ADD-ACCEPT", filter) != NULL) {
            return test_machine_ir_accepts_float_addition_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-SUB-ACCEPT", filter) != NULL) {
            return test_machine_ir_accepts_float_subtraction_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-NEG-ADD-COMBO-ACCEPT", filter) != NULL) {
            return test_machine_ir_accepts_negative_float_addition_combo_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-NEG-SUB-COMBO-ACCEPT", filter) != NULL) {
            return test_machine_ir_accepts_negative_float_subtraction_combo_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-MUL-ACCEPT", filter) != NULL) {
            return test_machine_ir_accepts_float_multiplication_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-DIV-ACCEPT", filter) != NULL) {
            return test_machine_ir_accepts_float_division_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-NEG-MUL-COMBO-ACCEPT", filter) != NULL) {
            return test_machine_ir_accepts_negative_float_multiplication_combo_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-NEG-DIV-COMBO-ACCEPT", filter) != NULL) {
            return test_machine_ir_accepts_negative_float_division_combo_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-CHAIN-ADD-ACCEPT", filter) != NULL) {
            return test_machine_ir_accepts_chained_float_addition_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-HELPER-TERNARY-CALL-ARITH-ACCEPT", filter) != NULL) {
            return test_machine_ir_accepts_float_helper_wrapped_ternary_call_arithmetic_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-UNARY-HELPER-TERNARY-CALL-ARITH-ACCEPT", filter) != NULL) {
            return test_machine_ir_accepts_unary_call_helper_wrapped_ternary_call_arithmetic_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-HELPER-TERNARY-CALL-COMPARE-ACCEPT", filter) != NULL) {
            return test_machine_ir_accepts_float_helper_wrapped_ternary_call_compare_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-UNARY-HELPER-TERNARY-CALL-COMPARE-ACCEPT", filter) != NULL) {
            return test_machine_ir_accepts_unary_call_helper_wrapped_ternary_call_compare_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-TO-INT-CONVERT-ACCEPT", filter) != NULL) {
            return test_machine_ir_accepts_explicit_int_from_float_conversion_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-INT-TO-FLOAT-CONVERT-ACCEPT", filter) != NULL) {
            return test_machine_ir_accepts_explicit_float_from_int_conversion_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-INT-TO-FLOAT-RECURSIVE-INIT-BRIDGE-ACCEPT", filter) != NULL) {
            return test_machine_ir_accepts_explicit_float_from_recursive_int_initializer_bridge_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-INT-TO-FLOAT-RECURSIVE-ASSIGN-BRIDGE-ACCEPT", filter) != NULL) {
            return test_machine_ir_accepts_explicit_float_from_recursive_int_assignment_bridge_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-INT-TO-FLOAT-COMPARE-BRIDGE-ACCEPT", filter) != NULL) {
            return test_machine_ir_accepts_explicit_float_from_int_compare_bridge_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-INT-TO-FLOAT-RECURSIVE-ARITH-BRIDGE-ACCEPT", filter) != NULL) {
            return test_machine_ir_accepts_explicit_float_from_recursive_int_arithmetic_bridge_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-TO-INT-TERNARY-BRIDGE-ACCEPT", filter) != NULL) {
            return test_machine_ir_accepts_explicit_int_from_float_ternary_bridge_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-TO-INT-RECURSIVE-CALLARG-BRIDGE-ACCEPT", filter) != NULL) {
            return test_machine_ir_accepts_explicit_int_from_recursive_float_call_argument_bridge_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-TO-INT-RECURSIVE-ASSIGN-BRIDGE-ACCEPT", filter) != NULL) {
            return test_machine_ir_accepts_explicit_int_from_recursive_float_assignment_bridge_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-TO-INT-COMPARE-BRIDGE-ACCEPT", filter) != NULL) {
            return test_machine_ir_accepts_explicit_int_from_float_compare_bridge_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-TO-INT-RECURSIVE-ARITH-BRIDGE-ACCEPT", filter) != NULL) {
            return test_machine_ir_accepts_explicit_int_from_recursive_float_arithmetic_bridge_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-NESTED-MUL-DIV-ACCEPT", filter) != NULL) {
            return test_machine_ir_accepts_nested_float_mul_div_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-ARITH-INT-TYPE-REJECT", filter) != NULL) {
            return test_machine_ir_rejects_mixed_float_int_arithmetic_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-CALL-ARITH-INT-TYPE-REJECT", filter) != NULL) {
            return test_machine_ir_rejects_float_call_int_arithmetic_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-LITERAL-ARITH-INT-TYPE-REJECT", filter) != NULL) {
            return test_machine_ir_rejects_float_literal_int_arithmetic_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-NEG-CALL-ARITH-INT-TYPE-REJECT", filter) != NULL) {
            return test_machine_ir_rejects_negative_float_call_int_arithmetic_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-GLOBAL-COND-PLUS-INT-REJECT", filter) != NULL) {
            return test_machine_ir_rejects_global_float_int_condition_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-CALL-COND-PLUS-INT-REJECT", filter) != NULL) {
            return test_machine_ir_rejects_float_call_int_condition_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-NEG-CALL-COND-PLUS-INT-REJECT", filter) != NULL) {
            return test_machine_ir_rejects_negative_float_call_int_condition_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-NESTED-TREE-COND-PLUS-INT-REJECT", filter) != NULL) {
            return test_machine_ir_rejects_nested_float_tree_plus_int_condition_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-NESTED-MULDIV-COND-PLUS-INT-REJECT", filter) != NULL) {
            return test_machine_ir_rejects_nested_float_muldiv_plus_int_condition_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-NESTED-TREE-PLUS-INT-REJECT", filter) != NULL) {
            return test_machine_ir_rejects_nested_float_tree_plus_int_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-NESTED-MULDIV-PLUS-INT-REJECT", filter) != NULL) {
            return test_machine_ir_rejects_nested_float_muldiv_plus_int_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-TERNARY-VALUE-PLUS-INT-REJECT", filter) != NULL) {
            return test_machine_ir_rejects_float_ternary_value_plus_int_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-UNARY-CALL-TERNARY-PLUS-INT-REJECT", filter) != NULL) {
            return test_machine_ir_rejects_unary_call_ternary_value_plus_int_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-TERNARY-PLUS-FLOAT-CALLARG-REJECT", filter) != NULL) {
            return test_machine_ir_rejects_float_ternary_value_plus_float_call_argument_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-UNARY-CALL-TERNARY-PLUS-FLOAT-CALLARG-REJECT", filter) != NULL) {
            return test_machine_ir_rejects_unary_call_ternary_value_plus_float_call_argument_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-TERNARY-VALUE-ASSIGN-INT-REJECT", filter) != NULL) {
            return test_machine_ir_rejects_float_ternary_value_assignment_to_int_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-UNARY-CALL-TERNARY-ASSIGN-INT-REJECT", filter) != NULL) {
            return test_machine_ir_rejects_unary_call_ternary_value_assignment_to_int_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-RECURSIVE-CALL-INIT-INT-REJECT", filter) != NULL) {
            return test_machine_ir_rejects_recursive_float_call_result_in_initializer_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-RECURSIVE-CALL-CALLARG-INT-REJECT", filter) != NULL) {
            return test_machine_ir_rejects_recursive_float_call_argument_to_int_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-TERNARY-VALUE-CALLARG-INT-REJECT", filter) != NULL) {
            return test_machine_ir_rejects_float_ternary_value_call_argument_to_int_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-UNARY-CALL-TERNARY-CALLARG-INT-REJECT", filter) != NULL) {
            return test_machine_ir_rejects_unary_call_ternary_value_call_argument_to_int_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-TERNARY-VALUE-CALLARG-FLOAT-ACCEPT", filter) != NULL) {
            return test_machine_ir_accepts_float_ternary_value_call_argument_to_float_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-UNARY-CALL-TERNARY-CALLARG-FLOAT-ACCEPT", filter) != NULL) {
            return test_machine_ir_accepts_unary_call_float_ternary_value_call_argument_to_float_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-CHAIN-ADD-CALLARG-FLOAT-ACCEPT", filter) != NULL) {
            return test_machine_ir_accepts_chained_float_addition_call_argument_to_float_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-NESTED-MUL-DIV-CALLARG-FLOAT-ACCEPT", filter) != NULL) {
            return test_machine_ir_accepts_nested_float_mul_div_call_argument_to_float_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-TERNARY-VALUE-INIT-INT-REJECT", filter) != NULL) {
            return test_machine_ir_rejects_float_ternary_value_initializer_to_int_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-UNARY-CALL-TERNARY-INIT-INT-REJECT", filter) != NULL) {
            return test_machine_ir_rejects_unary_call_ternary_value_initializer_to_int_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-TERNARY-VALUE-RETURN-FLOAT-ACCEPT", filter) != NULL) {
            return test_machine_ir_accepts_float_ternary_value_return_to_float_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-TERNARY-VALUE-ASSIGN-FLOAT-ACCEPT", filter) != NULL) {
            return test_machine_ir_accepts_float_ternary_value_assignment_to_float_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-TERNARY-VALUE-INIT-FLOAT-ACCEPT", filter) != NULL) {
            return test_machine_ir_accepts_float_ternary_value_initializer_to_float_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-CHAIN-ADD-ASSIGN-FLOAT-ACCEPT", filter) != NULL) {
            return test_machine_ir_accepts_chained_float_addition_assignment_to_float_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-NESTED-MUL-DIV-ASSIGN-FLOAT-ACCEPT", filter) != NULL) {
            return test_machine_ir_accepts_nested_float_mul_div_assignment_to_float_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-CHAIN-ADD-INIT-FLOAT-ACCEPT", filter) != NULL) {
            return test_machine_ir_accepts_chained_float_addition_initializer_to_float_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-NESTED-MUL-DIV-INIT-FLOAT-ACCEPT", filter) != NULL) {
            return test_machine_ir_accepts_nested_float_mul_div_initializer_to_float_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-TERNARY-VALUE-COMPARE-INT-REJECT", filter) != NULL) {
            return test_machine_ir_rejects_float_ternary_value_compare_against_int_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-UNARY-CALL-TERNARY-COMPARE-INT-REJECT", filter) != NULL) {
            return test_machine_ir_rejects_unary_call_ternary_value_compare_against_int_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-TERNARY-VALUE-COMPARE-FLOAT-REJECT", filter) != NULL) {
            return test_machine_ir_rejects_float_ternary_value_compare_against_float_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-IR-FLOAT-UNARY-CALL-TERNARY-COMPARE-FLOAT-REJECT", filter) != NULL) {
            return test_machine_ir_rejects_unary_call_ternary_value_compare_against_float_under_extension() ? 0 : 1;
        }
    }

    if (!test_machine_ir_manual_verify_accepts_valid_program()) {
        return 1;
    }
    if (!test_machine_ir_manual_verify_rejects_bad_spill_slot()) {
        return 1;
    }
    if (!test_machine_ir_query_surface()) {
        return 1;
    }
    if (!test_machine_ir_lower_from_value_ssa_dump()) {
        return 1;
    }
    if (!test_machine_ir_lower_from_manual_machine_view_dump()) {
        return 1;
    }
    if (!test_machine_ir_direct_dump_entrypoints()) {
        return 1;
    }
    if (!test_machine_ir_allocate_and_rewrite_entrypoints()) {
        return 1;
    }
    if (!test_machine_ir_allocate_and_rewrite_report_surface()) {
        return 1;
    }
    if (!test_machine_ir_allocate_and_rewrite_report_memory_shape_surface()) {
        return 1;
    }
    if (!test_machine_ir_allocate_and_rewrite_reallocates_functions_after_post_rewrite_canonicalize()) {
        return 1;
    }
    if (!test_machine_ir_translation_only_preserves_float_metadata()) {
        return 1;
    }
    if (!test_machine_ir_translation_only_lowers_extension_float_transport()) {
        return 1;
    }
    if (!test_machine_ir_translation_only_lowers_extension_float_global_literal_runtime_init()) {
        return 1;
    }
    if (!test_machine_ir_translation_only_float_report_query_surface()) {
        return 1;
    }
    if (!test_machine_ir_translation_only_float_global_literal_report_query_surface()) {
        return 1;
    }
    if (!test_machine_ir_default_pipeline_preserves_live_extension_float_assignment_transport()) {
        return 1;
    }
    if (!test_machine_ir_default_pipeline_preserves_float_return_transport_from_global()) {
        return 1;
    }
    if (!test_machine_ir_default_pipeline_preserves_float_return_transport_from_global_call()) {
        return 1;
    }
    if (!test_machine_ir_default_pipeline_preserves_float_global_identifier_runtime_init()) {
        return 1;
    }
    if (!test_machine_ir_default_pipeline_preserves_float_global_call_runtime_init()) {
        return 1;
    }
    if (!test_machine_ir_rejects_global_float_operator_expression_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_rejects_global_float_operator_expression_in_initializer_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_rejects_global_float_call_result_in_initializer_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_rejects_float_assignment_to_int_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_accepts_float_if_condition_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_accepts_float_while_condition_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_accepts_float_for_condition_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_accepts_recursive_float_if_condition_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_accepts_recursive_explicit_float_condition_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_accepts_explicit_float_while_condition_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_accepts_explicit_float_logical_condition_composition_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_rejects_recursive_float_condition_with_ternary_neighbor_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_accepts_float_logical_condition_composition_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_accepts_float_equality_compare_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_accepts_float_relational_compare_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_accepts_chained_float_equality_compare_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_accepts_chained_float_relational_compare_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_accepts_chained_float_inequality_compare_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_accepts_chained_float_le_compare_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_accepts_nested_muldiv_float_equality_compare_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_accepts_nested_muldiv_float_relational_compare_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_accepts_nested_muldiv_float_inequality_compare_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_accepts_nested_muldiv_float_ge_compare_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_accepts_signed_zero_float_equality_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_accepts_negative_float_relational_compare_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_accepts_float_addition_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_accepts_float_subtraction_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_accepts_negative_float_addition_combo_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_accepts_negative_float_subtraction_combo_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_accepts_float_multiplication_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_accepts_float_division_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_accepts_negative_float_multiplication_combo_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_accepts_negative_float_division_combo_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_accepts_chained_float_addition_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_accepts_float_helper_wrapped_ternary_call_arithmetic_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_accepts_unary_call_helper_wrapped_ternary_call_arithmetic_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_accepts_float_helper_wrapped_ternary_call_compare_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_accepts_unary_call_helper_wrapped_ternary_call_compare_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_accepts_explicit_int_from_float_conversion_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_accepts_explicit_float_from_int_conversion_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_accepts_explicit_float_from_recursive_int_initializer_bridge_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_accepts_explicit_float_from_recursive_int_assignment_bridge_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_accepts_explicit_float_from_int_compare_bridge_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_accepts_explicit_float_from_recursive_int_arithmetic_bridge_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_accepts_explicit_int_from_float_ternary_bridge_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_accepts_explicit_int_from_recursive_float_call_argument_bridge_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_accepts_explicit_int_from_recursive_float_assignment_bridge_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_accepts_explicit_int_from_float_compare_bridge_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_accepts_explicit_int_from_recursive_float_arithmetic_bridge_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_accepts_float_ternary_value_call_argument_to_float_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_accepts_unary_call_float_ternary_value_call_argument_to_float_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_accepts_chained_float_addition_call_argument_to_float_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_accepts_nested_float_mul_div_call_argument_to_float_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_accepts_nested_float_mul_div_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_rejects_mixed_float_int_arithmetic_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_rejects_float_call_int_arithmetic_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_rejects_float_literal_int_arithmetic_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_rejects_negative_float_call_int_arithmetic_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_rejects_global_float_int_condition_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_rejects_float_call_int_condition_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_rejects_negative_float_call_int_condition_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_rejects_nested_float_tree_plus_int_condition_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_rejects_nested_float_muldiv_plus_int_condition_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_rejects_float_ternary_value_plus_float_call_argument_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_rejects_unary_call_ternary_value_plus_float_call_argument_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_rejects_float_ternary_value_compare_against_float_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_rejects_unary_call_ternary_value_compare_against_float_under_extension()) {
        return 1;
    }
    if (!test_machine_ir_allocate_and_rewrite_keeps_rewritten_many_args_spill_shape_stable()) {
        return 1;
    }
    if (!test_machine_ir_allocate_and_rewrite_protects_branch_condition_from_live_out_alias()) {
        return 1;
    }
    if (!test_machine_ir_phi_elimination_dump()) {
        return 1;
    }
    if (!test_machine_ir_phi_eliminated_report_surface()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_after_phi_elimination_dump()) {
        return 1;
    }
    if (!test_machine_ir_canonicalize_program_dump_matches_legacy_cleanup_dump()) {
        return 1;
    }
    if (!test_machine_ir_cleaned_report_surface()) {
        return 1;
    }
    if (!test_machine_ir_canonicalized_report_surface()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_simplifies_same_target_branch()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_simplifies_immediate_branch_and_compacts()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_threads_jump_targets()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_merges_linear_jump_blocks()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_propagates_local_copies()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_eliminates_dead_moves()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_preserves_indirect_load_address_chain()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_preserves_indirect_store_inputs()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_preserves_cross_block_indirect_load_address_chain()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_preserves_cross_block_indirect_store_inputs()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_propagates_copies_across_single_predecessor_chain()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_stops_copy_propagation_at_join()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_propagates_agreed_copy_through_join()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_folds_jump_to_return_tail()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_folds_same_return_branch_tail()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_folds_jump_to_thin_return_tail()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_folds_same_thin_return_branch_tail()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_folds_jump_to_thin_jump_wrapper()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_folds_jump_to_thin_jump_wrapper_then_thin_return()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_respects_caller_clobber_kills()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_keeps_callee_preserved_copy_facts()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_folds_identical_branch_successors()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_keeps_nonidentical_branch_successors()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_folds_same_single_instruction_branch_successors()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_folds_threaded_identical_branch_successors()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_folds_equivalent_call_wrapper_branch_successors()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_folds_threaded_equivalent_call_wrapper_branch_successors()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_folds_equivalent_inlineable_return_branch_successors()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_keeps_nonequivalent_inlineable_return_branch_successors()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_folds_empty_branch_wrapper_tail()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_folds_equivalent_inlineable_branch_successors()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_folds_threaded_equivalent_inlineable_branch_successors()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_keeps_nonequivalent_inlineable_branch_successors()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_folds_thin_branch_wrapper_tail()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_folds_inlineable_branch_wrapper_tail()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_folds_jump_to_binary_return_wrapper()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_folds_threaded_thin_return_wrapper()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_folds_threaded_inlineable_return_wrapper()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_folds_jump_to_call_return_wrapper()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_folds_jump_to_call_branch_wrapper()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_folds_threaded_call_return_wrapper()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_folds_threaded_call_branch_wrapper()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_keeps_dangerous_binary_return_wrapper()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_keeps_nonmatching_single_instruction_branch_successors()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_folds_machine_value_constants()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_wraps_machine_value_overflow_constants()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_folds_machine_value_identities()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_chain_folds_value_then_tail()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_eliminates_dead_safe_binaries()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_eliminates_dead_loads()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_preserves_effectful_defs()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_forwards_known_local_loads()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_forwards_slot_values_into_value_folding()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_eliminates_redundant_local_stores()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_call_kills_known_global_slot_values()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_forwards_known_slot_values_across_straight_line_blocks()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_forwards_must_agree_join_slot_values()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_invalidates_slot_fact_on_register_redefinition()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_eliminates_dead_store_overwritten_in_same_block()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_eliminates_dead_store_overwritten_across_straight_line_blocks()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_keeps_store_observed_at_join()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_keeps_global_store_observed_by_call()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_keeps_global_store_observed_at_return()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_propagates_copies_into_unique_branch_successor()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_propagates_slot_values_into_unique_branch_successor()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_slot_fact_follows_copy_chain()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_internal_call_preserves_unrelated_global_fact()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_internal_call_still_preserves_final_global_store()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_folds_known_slot_return_wrapper()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_folds_branch_side_known_slot_return_wrappers()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_folds_known_slot_branch_wrapper()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_folds_branch_side_known_slot_branch_wrappers()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_folds_shared_known_slot_return_successors()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_folds_shared_known_slot_branch_successors()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_folds_inlineable_known_slot_return_wrapper()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_keeps_nonempty_known_slot_return_tail_body()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_folds_inlineable_known_slot_branch_wrapper()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_keeps_nonempty_known_slot_branch_tail_local_store()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_folds_shared_inlineable_known_slot_return_successors()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_folds_shared_inlineable_known_slot_branch_successors()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_folds_threaded_known_slot_return_wrapper()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_folds_threaded_known_slot_branch_wrapper()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_folds_shared_threaded_inlineable_known_slot_return_successors()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_folds_shared_threaded_inlineable_known_slot_branch_successors()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_folds_semantically_equivalent_shared_inlineable_known_slot_return_successors()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_keeps_constraint_mismatched_equivalent_inlineable_return_branch_successors()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_folds_semantically_equivalent_shared_inlineable_known_slot_branch_successors()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_internal_call_preserves_unrelated_global_return_wrapper()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_internal_call_preserves_unrelated_global_inlineable_wrapper()) {
        return 1;
    }
    if (!test_machine_ir_cleanup_internal_call_written_global_blocks_wrapper_fold()) {
        return 1;
    }
    if (!test_machine_ir_phi_elimination_critical_edge_dump()) {
        return 1;
    }
    if (!test_machine_ir_phi_elimination_skips_trivial_critical_edge_split()) {
        return 1;
    }
    printf("[machine-ir] PASS\n");
    return 0;
}
