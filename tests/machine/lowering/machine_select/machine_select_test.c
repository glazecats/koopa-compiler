#include "machine/select.h"

#include "machine/ir.h"
#include "value_ssa.h"
#include "value_ssa_pass.h"
#include "lexer.h"
#include "parser.h"
#include "semantic.h"
#include "ir.h"
#include "lower_ir.h"

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

static void machine_select_test_set_build_error(MachineIrError *error,
    const ParserError *parser_error,
    const SemanticError *semantic_error,
    const IrError *ir_error,
    const LowerIrError *lower_error,
    const ValueSsaError *value_error,
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
    if (value_error && value_error->message[0] != '\0') {
        error->line = value_error->line;
        error->column = value_error->column;
        snprintf(error->message, sizeof(error->message), "%s", value_error->message);
        return;
    }
    snprintf(error->message,
        sizeof(error->message),
        "%s",
        fallback_message ? fallback_message : "machine-select source builder failed");
}

static int expect_dump(const MachineSelectProgram *program, const char *expected_text) {
    char *actual_text = NULL;
    MachineSelectError error;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!machine_select_dump_program(program, &actual_text, &error)) {
        fprintf(stderr, "[machine-select] FAIL: dump failed: %s\n", error.message);
        return 0;
    }
    if (!actual_text || strcmp(actual_text, expected_text) != 0) {
        fprintf(stderr, "[machine-select] FAIL: dump mismatch\nactual:\n%s\n", actual_text ? actual_text : "<null>");
        ok = 0;
    }
    free(actual_text);
    return ok;
}

static int expect_dump_contains_all(const MachineSelectProgram *program,
    const char *case_id,
    const char *const *fragments,
    size_t fragment_count) {
    char *actual_text = NULL;
    MachineSelectError error;
    size_t index;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!machine_select_dump_program(program, &actual_text, &error)) {
        fprintf(stderr, "[machine-select] FAIL: %s dump failed: %s\n", case_id, error.message);
        return 0;
    }

    for (index = 0; index < fragment_count; ++index) {
        if (!fragments[index] || !strstr(actual_text, fragments[index])) {
            fprintf(stderr,
                "[machine-select] FAIL: %s dump missing fragment:\n%s\nactual:\n%s\n",
                case_id,
                fragments[index] ? fragments[index] : "<null>",
                actual_text ? actual_text : "<null>");
            ok = 0;
            break;
        }
    }

    free(actual_text);
    return ok;
}

static int expect_report_dump(const MachineSelectLowerReport *report, const char *expected_text) {
    char *actual_text = NULL;
    MachineSelectError error;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!machine_select_dump_lower_report_artifact(report, &actual_text, &error)) {
        fprintf(stderr, "[machine-select] FAIL: report dump failed: %s\n", error.message);
        return 0;
    }
    if (!actual_text || strcmp(actual_text, expected_text) != 0) {
        fprintf(stderr, "[machine-select] FAIL: report dump mismatch\nactual:\n%s\n", actual_text ? actual_text : "<null>");
        ok = 0;
    }
    free(actual_text);
    return ok;
}

static int build_machine_ir_smoke_program(MachineIrProgram *program, MachineIrError *error) {
    MachineIrFunction *function = NULL;
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

    if (!machine_ir_program_append_global(program, "g", NULL, error) ||
        !machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, error) ||
        !machine_ir_function_append_block(function, NULL, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(1);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = machine_ir_slot_global(0);
    instruction.as.store.value = machine_ir_operand_register(1);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_register(1), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_program_from_extension_source_text(const char *source,
    MachineIrProgram *program,
    MachineIrError *error) {
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
    ValueSsaProgram value_program;
    ValueSsaError value_error;
    MachineIrAllocateRewriteReport report;
    const MachineIrProgram *report_program = NULL;
    int ok = 0;

    if (!source || !program) {
        if (error) {
            error->line = 0;
            error->column = 0;
            snprintf(error->message, sizeof(error->message), "invalid machine-ir extension source builder contract");
        }
        return 0;
    }

    lexer_init_tokens(&tokens);
    ast_program_init(&ast_program);
    ir_program_init(&ir_program);
    lower_ir_program_init(&lower_program);
    value_ssa_program_init(&value_program);
    machine_ir_program_init(program);
    machine_ir_allocate_rewrite_report_init(&report);
    memset(&parser_error, 0, sizeof(parser_error));
    memset(&semantic_error, 0, sizeof(semantic_error));
    memset(&semantic_options, 0, sizeof(semantic_options));
    memset(&ir_error, 0, sizeof(ir_error));
    memset(&ir_options, 0, sizeof(ir_options));
    memset(&lower_error, 0, sizeof(lower_error));
    memset(&lower_options, 0, sizeof(lower_options));
    memset(&value_error, 0, sizeof(value_error));

    semantic_options.allow_extension_features = 1;
    semantic_options.skip_all_paths_return_check = 1;
    ir_options.allow_implicit_fallthrough_return = 1;
    lower_options.allow_implicit_fallthrough_return = 1;

    if (!lexer_tokenize(source, &tokens) ||
        !parser_parse_translation_unit_ast(&tokens, &ast_program, &parser_error) ||
        !semantic_analyze_program_with_options(&ast_program, &semantic_options, &semantic_error) ||
        !ir_lower_program(&ast_program, &ir_options, &ir_program, &ir_error) ||
        !lower_ir_lower_from_ir(&ir_program, &lower_options, &lower_program, &lower_error) ||
        !value_ssa_build_translation_only_from_lower_ir(&lower_program, &value_program, &value_error) ||
        !machine_ir_build_translation_only_report(&value_program, 8, 8, &report, error) ||
        !machine_ir_allocate_rewrite_report_get_program(&report, &report_program) ||
        !report_program ||
        !machine_ir_clone_program(report_program, program, error)) {
        machine_select_test_set_build_error(error,
            &parser_error,
            &semantic_error,
            &ir_error,
            &lower_error,
            &value_error,
            "invalid machine-ir extension source builder contract");
        goto cleanup;
    }

    ok = 1;

cleanup:
    if (!ok) {
        machine_ir_program_free(program);
    }
    machine_ir_allocate_rewrite_report_free(&report);
    value_ssa_program_free(&value_program);
    lower_ir_program_free(&lower_program);
    ir_program_free(&ir_program);
    ast_program_free(&ast_program);
    lexer_free_tokens(&tokens);
    return ok;
}

static int build_machine_ir_report_from_extension_source_text(const char *source,
    MachineIrAllocateRewriteReport *report,
    MachineIrError *error) {
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
    ValueSsaProgram value_program;
    ValueSsaError value_error;
    int ok = 0;

    if (!source || !report) {
        if (error) {
            error->line = 0;
            error->column = 0;
            snprintf(error->message, sizeof(error->message), "invalid machine-ir extension report builder contract");
        }
        return 0;
    }

    lexer_init_tokens(&tokens);
    ast_program_init(&ast_program);
    ir_program_init(&ir_program);
    lower_ir_program_init(&lower_program);
    value_ssa_program_init(&value_program);
    machine_ir_allocate_rewrite_report_init(report);
    memset(&parser_error, 0, sizeof(parser_error));
    memset(&semantic_error, 0, sizeof(semantic_error));
    memset(&semantic_options, 0, sizeof(semantic_options));
    memset(&ir_error, 0, sizeof(ir_error));
    memset(&ir_options, 0, sizeof(ir_options));
    memset(&lower_error, 0, sizeof(lower_error));
    memset(&lower_options, 0, sizeof(lower_options));
    memset(&value_error, 0, sizeof(value_error));

    semantic_options.allow_extension_features = 1;
    semantic_options.skip_all_paths_return_check = 1;
    ir_options.allow_implicit_fallthrough_return = 1;
    lower_options.allow_implicit_fallthrough_return = 1;

    if (!lexer_tokenize(source, &tokens) ||
        !parser_parse_translation_unit_ast(&tokens, &ast_program, &parser_error) ||
        !semantic_analyze_program_with_options(&ast_program, &semantic_options, &semantic_error) ||
        !ir_lower_program(&ast_program, &ir_options, &ir_program, &ir_error) ||
        !lower_ir_lower_from_ir(&ir_program, &lower_options, &lower_program, &lower_error) ||
        !value_ssa_build_translation_only_from_lower_ir(&lower_program, &value_program, &value_error) ||
        !machine_ir_build_translation_only_report(&value_program, 8, 8, report, error)) {
        machine_select_test_set_build_error(error,
            &parser_error,
            &semantic_error,
            &ir_error,
            &lower_error,
            &value_error,
            "invalid machine-ir extension report builder contract");
        goto cleanup;
    }

    ok = 1;

cleanup:
    if (!ok) {
        machine_ir_allocate_rewrite_report_free(report);
    }
    value_ssa_program_free(&value_program);
    lower_ir_program_free(&lower_program);
    ir_program_free(&ir_program);
    ast_program_free(&ast_program);
    lexer_free_tokens(&tokens);
    return ok;
}

static int build_machine_ir_report_from_default_extension_source_text(const char *source,
    MachineIrAllocateRewriteReport *report,
    MachineIrError *error) {
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
    ValueSsaProgram value_program;
    ValueSsaError value_error;
    int ok = 0;

    if (!source || !report) {
        if (error) {
            error->line = 0;
            error->column = 0;
            snprintf(error->message, sizeof(error->message), "invalid default machine-ir extension report builder contract");
        }
        return 0;
    }

    lexer_init_tokens(&tokens);
    ast_program_init(&ast_program);
    ir_program_init(&ir_program);
    lower_ir_program_init(&lower_program);
    value_ssa_program_init(&value_program);
    machine_ir_allocate_rewrite_report_init(report);
    memset(&parser_error, 0, sizeof(parser_error));
    memset(&semantic_error, 0, sizeof(semantic_error));
    memset(&semantic_options, 0, sizeof(semantic_options));
    memset(&ir_error, 0, sizeof(ir_error));
    memset(&ir_options, 0, sizeof(ir_options));
    memset(&lower_error, 0, sizeof(lower_error));
    memset(&lower_options, 0, sizeof(lower_options));
    memset(&value_error, 0, sizeof(value_error));

    semantic_options.allow_extension_features = 1;
    semantic_options.skip_all_paths_return_check = 1;
    ir_options.allow_implicit_fallthrough_return = 1;
    lower_options.allow_implicit_fallthrough_return = 1;

    if (!lexer_tokenize(source, &tokens) ||
        !parser_parse_translation_unit_ast(&tokens, &ast_program, &parser_error) ||
        !semantic_analyze_program_with_options(&ast_program, &semantic_options, &semantic_error) ||
        !ir_lower_program(&ast_program, &ir_options, &ir_program, &ir_error) ||
        !lower_ir_lower_from_ir(&ir_program, &lower_options, &lower_program, &lower_error) ||
        !value_ssa_build_default_from_lower_ir(&lower_program, &value_program, &value_error) ||
        !machine_ir_build_translation_only_report(&value_program, 8, 8, report, error)) {
        machine_select_test_set_build_error(error,
            &parser_error,
            &semantic_error,
            &ir_error,
            &lower_error,
            &value_error,
            "invalid default machine-ir extension report builder contract");
        goto cleanup;
    }

    ok = 1;

cleanup:
    if (!ok) {
        machine_ir_allocate_rewrite_report_free(report);
    }
    value_ssa_program_free(&value_program);
    lower_ir_program_free(&lower_program);
    ir_program_free(&ir_program);
    ast_program_free(&ast_program);
    lexer_free_tokens(&tokens);
    return ok;
}

static int test_machine_select_lower_machine_ir_smoke(void) {
    MachineIrProgram machine_program;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    static const char *const fragments[] = {
        "machine_select\n",
        "function main params=1 locals=1 spills=0\n",
        "store_global global.0, ",
    };
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    if (!build_machine_ir_smoke_program(&machine_program, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: machine-ir smoke setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    if (!machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: lower from machine-ir failed: %s\n", select_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump_contains_all(
        &select_program,
        "machine-ir-smoke",
        fragments,
        sizeof(fragments) / sizeof(fragments[0]));

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_rejects_phi_input(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineSelectProgram select_program;
    MachineIrError machine_error;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_block_append_phi(&function->blocks[0], machine_ir_operand_register(0), NULL, NULL, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_register(0), &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: phi setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    if (machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: lowering should reject phi-bearing machine_ir input\n");
        ok = 0;
    }

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_rejects_load_store_slot_kind_mismatch(void) {
    MachineIrProgram machine_program;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectProgram mutated_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);
    machine_select_program_init(&mutated_program);

    if (!build_machine_ir_smoke_program(&machine_program, &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: slot-kind-mismatch setup failed: %s\n", select_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&mutated_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    if (!machine_select_clone_program(&select_program, &mutated_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: load-slot mismatch clone failed: %s\n", select_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&mutated_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    mutated_program.functions[0].blocks[0].ops[0].as.load_slot = machine_select_slot_global(0);
    memset(&select_error, 0, sizeof(select_error));
    if (machine_select_verify_program(&mutated_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: verifier accepted load-local/global-slot mismatch\n");
        ok = 0;
    }

    machine_select_program_free(&mutated_program);
    machine_select_program_init(&mutated_program);
    if (!machine_select_clone_program(&select_program, &mutated_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: store-slot mismatch clone failed: %s\n", select_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&mutated_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    mutated_program.functions[0].blocks[0].ops[2].as.store.slot = machine_select_slot_local(0);
    memset(&select_error, 0, sizeof(select_error));
    if (machine_select_verify_program(&mutated_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: verifier accepted store-global/local-slot mismatch\n");
        ok = 0;
    }

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&mutated_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_rejects_missing_backing_tables(void) {
    MachineIrProgram machine_program;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectProgram mutated_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);
    machine_select_program_init(&mutated_program);

    if (!build_machine_ir_smoke_program(&machine_program, &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: missing-table setup failed: %s\n", select_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&mutated_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    if (!machine_select_clone_program(&select_program, &mutated_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: missing-local-table clone failed: %s\n", select_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&mutated_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    free(mutated_program.functions[0].locals);
    mutated_program.functions[0].locals = NULL;
    memset(&select_error, 0, sizeof(select_error));
    if (machine_select_verify_program(&mutated_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: verifier accepted missing local table\n");
        ok = 0;
    }

    machine_select_program_free(&mutated_program);
    machine_select_program_init(&mutated_program);
    if (!machine_select_clone_program(&select_program, &mutated_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: missing-op-table clone failed: %s\n", select_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&mutated_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    free(mutated_program.functions[0].blocks[0].ops);
    mutated_program.functions[0].blocks[0].ops = NULL;
    memset(&select_error, 0, sizeof(select_error));
    if (machine_select_verify_program(&mutated_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: verifier accepted missing op table\n");
        ok = 0;
    }

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&mutated_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_query_dump_rejects_missing_backing_tables(void) {
    MachineIrProgram machine_program;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectProgram mutated_program;
    MachineSelectError select_error;
    const MachineSelectFunction *function_view = NULL;
    const MachineSelectBasicBlock *block_view = NULL;
    MachineSelectFunctionSummary summary;
    char *dump_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    memset(&summary, 0, sizeof(summary));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);
    machine_select_program_init(&mutated_program);

    if (!build_machine_ir_smoke_program(&machine_program, &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: query/dump missing-table setup failed: %s\n", select_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&mutated_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    if (!machine_select_clone_program(&select_program, &mutated_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: query missing function-table clone failed: %s\n", select_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&mutated_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    free(mutated_program.functions);
    mutated_program.functions = NULL;
    memset(&select_error, 0, sizeof(select_error));
    if (machine_select_program_get_function_by_name(&mutated_program, "main", NULL, &function_view) ||
        machine_select_dump_program(&mutated_program, &dump_text, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: function-table query/dump should fail on missing table\n");
        ok = 0;
    }
    free(dump_text);
    dump_text = NULL;

    machine_select_program_free(&mutated_program);
    machine_select_program_init(&mutated_program);
    if (!machine_select_clone_program(&select_program, &mutated_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: query missing block/op-table clone failed: %s\n", select_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&mutated_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    free(mutated_program.functions[0].blocks[0].ops);
    mutated_program.functions[0].blocks[0].ops = NULL;
    memset(&select_error, 0, sizeof(select_error));
    if (machine_select_program_get_block_by_function_name_and_block_id(
            &mutated_program, "main", 0, NULL, NULL, &function_view, &block_view) &&
        machine_select_function_compute_summary(&mutated_program.functions[0], &summary)) {
        fprintf(stderr, "[machine-select] FAIL: block/op-table queries should not succeed on missing ops\n");
        ok = 0;
    }
    if (machine_select_dump_program(&mutated_program, &dump_text, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: dump should fail on missing op table\n");
        ok = 0;
    }
    free(dump_text);

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&mutated_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_dump_rejects_missing_call_args(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectProgram mutated_program;
    MachineSelectError select_error;
    char *dump_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);
    machine_select_program_init(&mutated_program);

    machine_program.register_bank.register_count = 1u;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1u, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0u;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    if (!machine_program.register_bank.registers[0].name) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&mutated_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "x", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: missing-call-args setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&mutated_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.load_slot = machine_ir_slot_local(0u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: missing-call-args setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&mutated_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.call.callee_name = "main";
    instruction.as.call.arg_count = 1u;
    instruction.as.call.args = (MachineIrOperand *)calloc(1u, sizeof(MachineIrOperand));
    if (!instruction.as.call.args) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&mutated_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    instruction.as.call.args[0] = machine_ir_operand_register(0u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_register(0u), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: missing-call-args lowering failed: %s\n", select_error.message);
        free(instruction.as.call.args);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&mutated_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    free(instruction.as.call.args);

    if (!machine_select_clone_program(&select_program, &mutated_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: missing-call-args clone failed: %s\n", select_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&mutated_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    free(mutated_program.functions[0].blocks[0].ops[1].as.call.args);
    mutated_program.functions[0].blocks[0].ops[1].as.call.args = NULL;
    memset(&select_error, 0, sizeof(select_error));
    if (machine_select_dump_program(&mutated_program, &dump_text, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: dump should fail on missing call args\n");
        ok = 0;
    }
    free(dump_text);

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&mutated_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_summary_surface(void) {
    MachineIrProgram machine_program;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    const MachineSelectFunction *function = NULL;
    const MachineSelectFunction *named_function = NULL;
    const MachineSelectBasicBlock *block = NULL;
    MachineSelectFunctionSummary summary;
    MachineSelectFunctionSummary named_summary;
    MachineSelectFunctionSummary artifact_summary;
    MachineSelectTerminatorKind terminator_kind = MACHINE_SELECT_TERM_JUMP;
    size_t register_count = 0;
    size_t global_count = 0;
    size_t function_count = 0;
    size_t function_index = (size_t)-1;
    const char *name = NULL;
    size_t block_count = 0;
    size_t block_id = (size_t)-1;
    size_t op_count = 0;
    size_t spill_slot_count = 0;
    int has_terminator = 0;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    if (!build_machine_ir_smoke_program(&machine_program, &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: summary setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    if (!machine_select_program_get_summary(&select_program, &register_count, &global_count, &function_count) ||
        register_count != 2 || global_count != 1 || function_count != 1) {
        fprintf(stderr, "[machine-select] FAIL: program summary mismatch\n");
        ok = 0;
    }
    if (!machine_select_program_get_function_by_name(&select_program, "main", &function_index, &named_function) ||
        function_index != 0 || !named_function) {
        fprintf(stderr, "[machine-select] FAIL: program function-by-name mismatch\n");
        ok = 0;
    }
    if (!machine_select_program_get_function_artifact(&select_program, 0, &function, &artifact_summary) ||
        !function ||
        artifact_summary.op_count != 4 ||
        artifact_summary.load_local_count != 1 ||
        artifact_summary.store_global_count != 1 ||
        artifact_summary.return_count != 1) {
        fprintf(stderr, "[machine-select] FAIL: program function artifact mismatch\n");
        ok = 0;
    }
    if (!machine_select_program_get_function_summary_by_name(&select_program, "main", &function_index, &named_summary) ||
        function_index != 0 ||
        named_summary.op_count != 4 ||
        named_summary.load_local_count != 1 ||
        named_summary.store_global_count != 1 ||
        named_summary.return_count != 1) {
        fprintf(stderr, "[machine-select] FAIL: program function-summary-by-name mismatch\n");
        ok = 0;
    }
    if (!machine_select_program_get_function_artifact_by_name(
            &select_program, "main", &function_index, &named_function, &named_summary) ||
        function_index != 0 ||
        !named_function ||
        named_summary.op_count != 4 ||
        named_summary.load_local_count != 1 ||
        named_summary.store_global_count != 1 ||
        named_summary.return_count != 1) {
        fprintf(stderr, "[machine-select] FAIL: program function-artifact-by-name mismatch\n");
        ok = 0;
    }
    if (!machine_select_program_get_function(&select_program, 0, &function) ||
        !machine_select_function_get_summary(function, &name, NULL, NULL, NULL, &block_count, &spill_slot_count) ||
        !name || strcmp(name, "main") != 0 || block_count != 1 || spill_slot_count != 0) {
        fprintf(stderr, "[machine-select] FAIL: function summary mismatch\n");
        ok = 0;
    }
    if (!machine_select_function_get_block(function, 0, &block) ||
        !block ||
        !machine_select_basic_block_get_summary(block, &block_id, &op_count, &has_terminator, &terminator_kind) ||
        block_id != 0 || op_count != 4 || !has_terminator || terminator_kind != MACHINE_SELECT_TERM_RETURN) {
        fprintf(stderr, "[machine-select] FAIL: block summary mismatch\n");
        ok = 0;
    }
    if (!machine_select_function_compute_summary(function, &summary) ||
        summary.op_count != 4 ||
        summary.call_count != 0 ||
        summary.load_local_count != 1 ||
        summary.store_global_count != 1 ||
        summary.return_count != 1 ||
        summary.return_imm_count != 0 ||
        summary.return_spill_count != 0) {
        fprintf(stderr, "[machine-select] FAIL: computed function summary mismatch\n");
        ok = 0;
    }

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_reuses_duplicate_addr_local_spill_roots_in_block(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineSelectProgram select_program;
    MachineIrError machine_error;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 0, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: duplicate addr_local setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    function->spill_slot_count = 2;

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_ADDR_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_spill_slot(0);
    instruction.as.addr_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: first addr_local append failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_ADDR_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_spill_slot(1);
    instruction.as.addr_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_spill_slot(1), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: duplicate addr_local lowering failed: %s\n", select_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=0 locals=1 spills=2\n"
        "  bb.0:\n"
        "    spill.1 = addr_local local.0\n"
        "    retspill spill.1\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_forwards_self_copy_until_last_visible_use(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineSelectProgram select_program;
    MachineIrError machine_error;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 2;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(2, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    machine_program.register_bank.registers[1].register_id = 1;
    machine_program.register_bank.registers[1].name = dup_text("r1");
    machine_program.register_bank.registers[1].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 0, NULL, &machine_error) ||
        !machine_ir_function_append_local(function, "b", 0, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: self-copy forwarding setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_ADDR_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.addr_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: self-copy forwarding setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_register(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: self-copy forwarding setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1);
    instruction.as.load_slot = machine_ir_slot_local(1);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: self-copy forwarding setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_INDIRECT;
    instruction.as.store_indirect.addr = machine_ir_operand_register(0);
    instruction.as.store_indirect.value = machine_ir_operand_register(1);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: self-copy forwarding setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_ADDR_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.addr_slot = machine_ir_slot_local(1);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_register(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: self-copy forwarding lowering failed: %s\n", select_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=0 locals=2 spills=0\n"
        "  bb.0:\n"
        "    reg.0 = addr_local local.0\n"
        "    reg.1 = load_local local.1\n"
        "    store_indirect reg.0, reg.1\n"
        "    reg.0 = addr_local local.1\n"
        "    ret reg.0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_removes_redundant_same_result_addr_local_redefs(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineSelectProgram select_program;
    MachineIrError machine_error;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 2;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(2, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    machine_program.register_bank.registers[1].register_id = 1;
    machine_program.register_bank.registers[1].name = dup_text("r1");
    machine_program.register_bank.registers[1].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 0, NULL, &machine_error) ||
        !machine_ir_function_append_local(function, "b", 0, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: redundant-addr-redef setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_ADDR_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.addr_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: redundant-addr-redef setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1);
    instruction.as.load_slot = machine_ir_slot_local(1);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: redundant-addr-redef setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_INDIRECT;
    instruction.as.store_indirect.addr = machine_ir_operand_register(0);
    instruction.as.store_indirect.value = machine_ir_operand_register(1);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: redundant-addr-redef setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_ADDR_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.addr_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_register(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: redundant-addr-redef lowering failed: %s\n", select_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=0 locals=2 spills=0\n"
        "  bb.0:\n"
        "    reg.0 = addr_local local.0\n"
        "    reg.1 = load_local local.1\n"
        "    store_indirect reg.0, reg.1\n"
        "    ret reg.0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_removes_copy_self_noop(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineSelectProgram select_program;
    MachineIrError machine_error;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 0, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: copy-self-noop setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_ADDR_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.addr_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: copy-self-noop setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_register(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_register(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: copy-self-noop lowering failed: %s\n", select_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=0 locals=1 spills=0\n"
        "  bb.0:\n"
        "    reg.0 = addr_local local.0\n"
        "    ret reg.0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_folds_addr_local_copy_pair_into_final_result(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineSelectProgram select_program;
    MachineIrError machine_error;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 2;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(2, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    machine_program.register_bank.registers[1].register_id = 1;
    machine_program.register_bank.registers[1].name = dup_text("r1");
    machine_program.register_bank.registers[1].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 0, NULL, &machine_error) ||
        !machine_ir_function_append_local(function, "b", 0, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: addr-copy-pair setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_ADDR_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.addr_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: addr-copy-pair setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1);
    instruction.as.mov_value = machine_ir_operand_register(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: addr-copy-pair setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(1);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: addr-copy-pair setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_INDIRECT;
    instruction.as.store_indirect.addr = machine_ir_operand_register(1);
    instruction.as.store_indirect.value = machine_ir_operand_register(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_register(1), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: addr-copy-pair lowering failed: %s\n", select_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=0 locals=2 spills=0\n"
        "  bb.0:\n"
        "    reg.1 = addr_local local.0\n"
        "    reg.0 = load_local local.1\n"
        "    store_indirect reg.1, reg.0\n"
        "    ret reg.1\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_folds_pure_producer_copy_pair_into_final_result(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineSelectProgram select_program;
    MachineIrError machine_error;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 2;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(2, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    machine_program.register_bank.registers[1].register_id = 1;
    machine_program.register_bank.registers[1].name = dup_text("r1");
    machine_program.register_bank.registers[1].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: pure-producer-copy setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_immediate(3);
    instruction.as.binary.rhs = machine_ir_operand_immediate(4);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: pure-producer-copy setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1);
    instruction.as.mov_value = machine_ir_operand_register(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_register(1), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: pure-producer-copy lowering failed: %s\n", select_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=0 locals=0 spills=0\n"
        "  bb.0:\n"
        "    reti 7\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_keeps_parameter_pointer_copy_when_original_value_is_used_in_successor(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 3u;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(3u, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    for (size_t i = 0; i < 3u; ++i) {
        machine_program.register_bank.registers[i].register_id = i;
        machine_program.register_bank.registers[i].name = dup_text(i == 0 ? "r0" : (i == 1 ? "r1" : "r2"));
        machine_program.register_bank.registers[i].allocatable = 1u;
        if (!machine_program.register_bank.registers[i].name) {
            machine_ir_program_free(&machine_program);
            machine_select_program_free(&select_program);
            return 0;
        }
    }

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "arr", 1, NULL, &machine_error) ||
        !machine_ir_function_append_local(function, "tmp", 0, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(2u);
    instruction.as.load_slot = machine_ir_slot_local(0u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.mov_value = machine_ir_operand_register(2u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_INDIRECT;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.load_indirect_addr = machine_ir_operand_register(0u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[0], 1u, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(2u);
    instruction.as.binary.rhs = machine_ir_operand_immediate(4);
    if (!machine_ir_block_append_instruction(&function->blocks[1], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_INDIRECT;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.load_indirect_addr = machine_ir_operand_register(0u);
    if (!machine_ir_block_append_instruction(&function->blocks[1], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_register(0u), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=1 locals=2 spills=0\n"
        "  bb.0:\n"
        "    reg.2 = load_local local.0\n"
        "    reg.0 = copy reg.2\n"
        "    reg.0 = load_indirect reg.0\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    reg.0 = alui.0 reg.2, 4\n"
        "    reg.0 = load_indirect reg.0\n"
        "    ret reg.0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_forwards_compare_inputs_through_adjacent_copy(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 2u;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(2u, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0u;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    machine_program.register_bank.registers[1].register_id = 1u;
    machine_program.register_bank.registers[1].name = dup_text("r1");
    machine_program.register_bank.registers[1].allocatable = 1u;
    if (!machine_program.register_bank.registers[0].name ||
        !machine_program.register_bank.registers[1].name) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.load_slot = machine_ir_slot_local(0u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1u);
    instruction.as.mov_value = machine_ir_operand_register(0u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1u);
    instruction.as.binary.op = MACHINE_IR_BINARY_LT;
    instruction.as.binary.lhs = machine_ir_operand_register(1u);
    instruction.as.binary.rhs = machine_ir_operand_immediate(4);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(1u), 1u, 2u, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_immediate(1), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=1 locals=1 spills=0\n"
        "  bb.0:\n"
        "    reg.0 = load_local local.0\n"
        "    cmpbri.12 reg.0, 4, bb.1, bb.2\n"
        "  bb.1:\n"
        "    reti 1\n"
        "  bb.2:\n"
        "    reti 0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_fuses_branch_with_adjacent_compare_after_copy_forward(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 2u;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(2u, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0u;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    machine_program.register_bank.registers[1].register_id = 1u;
    machine_program.register_bank.registers[1].name = dup_text("r1");
    machine_program.register_bank.registers[1].allocatable = 1u;
    if (!machine_program.register_bank.registers[0].name ||
        !machine_program.register_bank.registers[1].name) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.load_slot = machine_ir_slot_local(0u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1u);
    instruction.as.mov_value = machine_ir_operand_register(0u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1u);
    instruction.as.binary.op = MACHINE_IR_BINARY_LT;
    instruction.as.binary.lhs = machine_ir_operand_register(1u);
    instruction.as.binary.rhs = machine_ir_operand_immediate(4);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(1u), 1u, 2u, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_immediate(1), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=1 locals=1 spills=0\n"
        "  bb.0:\n"
        "    reg.0 = load_local local.0\n"
        "    cmpbri.12 reg.0, 4, bb.1, bb.2\n"
        "  bb.1:\n"
        "    reti 1\n"
        "  bb.2:\n"
        "    reti 0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_distinguishes_cmp_ops(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 2;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(2, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    machine_program.register_bank.registers[1].register_id = 1;
    machine_program.register_bank.registers[1].name = dup_text("r1");
    machine_program.register_bank.registers[1].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_local(function, "b", 1, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: cmp setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: cmp setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1);
    instruction.as.load_slot = machine_ir_slot_local(1);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: cmp setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_EQ;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_register(1);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_register(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: cmp lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=2 locals=2 spills=0\n"
        "  bb.0:\n"
        "    reg.0 = load_local local.0\n"
        "    reg.1 = load_local local.1\n"
        "    reg.0 = cmp.10 reg.0, reg.1\n"
        "    ret reg.0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_distinguishes_alu_immediate_ops(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: alui setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: alui setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(9);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_register(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: alui lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=1 locals=1 spills=0\n"
        "  bb.0:\n"
        "    reg.0 = load_local local.0\n"
        "    reg.0 = alui.0 reg.0, 9\n"
        "    ret reg.0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_distinguishes_cmp_immediate_ops(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: cmpi setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: cmpi setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_EQ;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(4);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_register(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: cmpi lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=1 locals=1 spills=0\n"
        "  bb.0:\n"
        "    reg.0 = load_local local.0\n"
        "    reg.0 = cmpi.10 reg.0, 4\n"
        "    ret reg.0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_normalizes_commutative_immediate_to_rhs(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: commutative-imm setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: commutative-imm setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_immediate(7);
    instruction.as.binary.rhs = machine_ir_operand_register(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_register(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: commutative-imm lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=1 locals=1 spills=0\n"
        "  bb.0:\n"
        "    reg.0 = load_local local.0\n"
        "    reg.0 = alui.0 reg.0, 7\n"
        "    ret reg.0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_keeps_noncommutative_lhs_immediate_out_of_imm_family(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: noncommutative-imm setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: noncommutative-imm setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_SUB;
    instruction.as.binary.lhs = machine_ir_operand_immediate(7);
    instruction.as.binary.rhs = machine_ir_operand_register(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_register(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: noncommutative-imm lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=1 locals=1 spills=0\n"
        "  bb.0:\n"
        "    reg.0 = load_local local.0\n"
        "    reg.0 = alu.1 7, reg.0\n"
        "    ret reg.0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_materializes_constant_binary_results(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    MachineSelectFunctionSummary summary;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 2;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(2, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    machine_program.register_bank.registers[1].register_id = 1;
    machine_program.register_bank.registers[1].name = dup_text("r1");
    machine_program.register_bank.registers[1].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: constant-binary setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_immediate(10);
    instruction.as.binary.rhs = machine_ir_operand_immediate(32);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: constant-binary setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1);
    instruction.as.binary.op = MACHINE_IR_BINARY_EQ;
    instruction.as.binary.lhs = machine_ir_operand_immediate(7);
    instruction.as.binary.rhs = machine_ir_operand_immediate(7);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_register(1), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: constant-binary lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=0 locals=0 spills=0\n"
        "  bb.0:\n"
        "    reti 1\n");

    if (!machine_select_function_compute_summary(&select_program.functions[0], &summary) ||
        summary.materialize_imm_count != 0 ||
        summary.alu_count != 0 ||
        summary.alu_imm_count != 0 ||
        summary.cmp_count != 0 ||
        summary.cmp_imm_count != 0 ||
        summary.return_count != 0 ||
        summary.return_imm_count != 1) {
        fprintf(stderr, "[machine-select] FAIL: constant-binary summary mismatch\n");
        ok = 0;
    }

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_materializes_overflowing_constant_binary_results(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: overflowing-constant setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_immediate(2147483647LL);
    instruction.as.binary.rhs = machine_ir_operand_immediate(1);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_register(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: overflowing-constant lowering failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=0 locals=0 spills=0\n"
        "  bb.0:\n"
        "    reg.0 = alu.0 2147483647, 1\n"
        "    ret reg.0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_cleanup_removes_shadowed_trivial_defs(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: shadowed-trivial-def setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_immediate(5);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: shadowed-trivial-def setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_immediate(6);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_register(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: shadowed-trivial-def lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=0 locals=0 spills=0\n"
        "  bb.0:\n"
        "    reti 6\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_canonicalized_bridge(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: canonicalized bridge setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_immediate(7);
    if (!machine_ir_block_append_instruction(&function->blocks[1], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_register(0), &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[0], 1, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: canonicalized bridge setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    if (!machine_select_lower_canonicalized_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: canonicalized bridge lowering failed: %s\n", select_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=0 locals=0 spills=0\n"
        "  bb.0:\n"
        "    reti 7\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_canonicalized_conservative_bridge(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: conservative canonicalized bridge setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_immediate(7);
    if (!machine_ir_block_append_instruction(&function->blocks[1], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_register(0), &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[0], 1, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: conservative canonicalized bridge setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    if (!machine_select_lower_canonicalized_program_from_machine_ir_conservative(
            &machine_program, &select_program, &select_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: conservative canonicalized bridge lowering failed: %s\n",
            select_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=0 locals=0 spills=0\n"
        "  bb.0:\n"
        "    reti 7\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_float_metadata_roundtrip(void) {
    MachineIrProgram machine_program;
    MachineSelectProgram select_program;
    MachineIrError machine_error;
    MachineSelectError select_error;
    char *actual_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    if (!build_machine_ir_smoke_program(&machine_program, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: float metadata setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    machine_program.globals[0].value_type = AST_FUNCTION_RETURN_FLOAT;
    machine_program.functions[0].locals[0].value_type = AST_FUNCTION_RETURN_FLOAT;

    if (!machine_select_lower_program_from_machine_ir_conservative(
            &machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: conservative lower failed: %s\n", select_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    if (!machine_select_dump_program(&select_program, &actual_text, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: float metadata dump failed: %s\n", select_error.message);
        ok = 0;
        goto cleanup;
    }

    if (select_program.globals[0].value_type != AST_FUNCTION_RETURN_FLOAT ||
        select_program.functions[0].locals[0].value_type != AST_FUNCTION_RETURN_FLOAT ||
        !strstr(actual_text, "param local.0:float name=a")) {
        fprintf(stderr,
            "[machine-select] FAIL: float metadata was not preserved\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_lowers_extension_float_transport(void) {
    MachineIrProgram machine_program;
    MachineSelectProgram select_program;
    MachineIrError machine_error;
    MachineSelectError select_error;
    char *actual_text = NULL;
    MachineSelectLowerReport report;
    char *report_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);
    machine_select_lower_report_init(&report);

    if (!build_machine_ir_program_from_extension_source_text(
            "float id(float x){ return x; } int main(){ float x = 1.25; id(1.25); return 0; }\n",
            &machine_program,
            &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: extension float setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    if (!machine_select_lower_program_from_machine_ir_conservative(
            &machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: extension float conservative lower failed: %s\n", select_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    if (!machine_select_dump_program(&select_program, &actual_text, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: extension float dump failed: %s\n", select_error.message);
        ok = 0;
        goto cleanup;
    }
    if (!machine_select_build_report_from_machine_ir_program(&machine_program, &report, &select_error) ||
        !machine_select_dump_lower_report_artifact(&report, &report_text, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: extension float report dump failed: %s\n", select_error.message);
        ok = 0;
        goto cleanup;
    }

    if (select_program.function_count < 2u ||
        select_program.functions[0].parameter_count < 1u ||
        select_program.functions[0].locals[0].value_type != AST_FUNCTION_RETURN_FLOAT ||
        strcmp(select_program.functions[0].name, "id") != 0 ||
        !strstr(actual_text, "param local.0:float name=x") ||
        !strstr(actual_text, "store_locali local.0, 1067450368") ||
        !strstr(actual_text, "calli id(1067450368)") ||
        !strstr(report_text, "functions=2") ||
        !strstr(report_text, "target_policy preview_reg_cap=8") ||
        !strstr(report_text, "function_summaries:")) {
        fprintf(stderr,
            "[machine-select] FAIL: extension float metadata was not preserved\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    free(report_text);
    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    machine_select_lower_report_free(&report);
    return ok;
}

static int test_machine_select_lowers_extension_float_global_literal_runtime_init(void) {
    MachineIrProgram machine_program;
    MachineSelectProgram select_program;
    MachineIrError machine_error;
    MachineSelectError select_error;
    char *actual_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    if (!build_machine_ir_program_from_extension_source_text(
            "float g = 1.25;\nfloat id(float x){ return x; }\nint main(){ return 0; }\n",
            &machine_program,
            &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: extension float global literal setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    if (!machine_select_lower_program_from_machine_ir_conservative(
            &machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: extension float global literal conservative lower failed: %s\n", select_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    if (!machine_select_dump_program(&select_program, &actual_text, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: extension float global literal dump failed: %s\n", select_error.message);
        ok = 0;
        goto cleanup;
    }

    if (select_program.global_count < 1u ||
        select_program.globals[0].value_type != AST_FUNCTION_RETURN_FLOAT ||
        !strstr(actual_text, "function __global.init params=0 locals=0 spills=0") ||
        !strstr(actual_text, "store_globali global.0, 1067450368")) {
        fprintf(stderr,
            "[machine-select] FAIL: extension float global literal runtime-init mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_builds_report_from_extension_float_machine_ir_report(void) {
    MachineIrAllocateRewriteReport machine_report;
    MachineIrError machine_error;
    MachineSelectLowerReport select_report;
    MachineSelectError select_error;
    char *actual_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_select_lower_report_init(&select_report);

    if (!build_machine_ir_report_from_extension_source_text(
            "float id(float x){ return x; } int main(){ float x = 1.25; id(1.25); return 0; }\n",
            &machine_report,
            &machine_error) ||
        !machine_select_build_report_from_machine_ir_report(&machine_report, &select_report, &select_error) ||
        !machine_select_dump_lower_report_artifact(&select_report, &actual_text, &select_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: extension float report bridge setup failed: %s\n",
            select_error.message[0] ? select_error.message : machine_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "functions=2") ||
        !strstr(actual_text, "fn.0 id blocks=1") ||
        !strstr(actual_text, "param local.0:float name=x") ||
        !strstr(actual_text, "store_locali local.0, 1067450368") ||
        !strstr(actual_text, "calli id(1067450368)")) {
        fprintf(stderr,
            "[machine-select] FAIL: extension float report bridge dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_select_lower_report_free(&select_report);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    return ok;
}

static int test_machine_select_builds_report_from_extension_float_global_literal_machine_ir_report(void) {
    MachineIrAllocateRewriteReport machine_report;
    MachineIrError machine_error;
    MachineSelectLowerReport select_report;
    MachineSelectError select_error;
    char *actual_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_select_lower_report_init(&select_report);

    if (!build_machine_ir_report_from_extension_source_text(
            "float g = 1.25;\nfloat id(float x){ return x; }\nint main(){ return 0; }\n",
            &machine_report,
            &machine_error) ||
        !machine_select_build_report_from_machine_ir_report(&machine_report, &select_report, &select_error) ||
        !machine_select_dump_lower_report_artifact(&select_report, &actual_text, &select_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: extension float global literal report bridge setup failed: %s\n",
            select_error.message[0] ? select_error.message : machine_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "functions=3") ||
        !strstr(actual_text, "function __global.init params=0 locals=0 spills=0") ||
        !strstr(actual_text, "store_globali global.0, 1067450368")) {
        fprintf(stderr,
            "[machine-select] FAIL: extension float global literal report bridge dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_select_lower_report_free(&select_report);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    return ok;
}

static int test_machine_select_float_report_query_surface(void) {
    MachineIrAllocateRewriteReport machine_report;
    MachineIrError machine_error;
    MachineSelectLowerReport select_report;
    MachineSelectError select_error;
    const MachineSelectProgram *report_program = NULL;
    const MachineSelectFunction *id_function = NULL;
    const MachineSelectFunction *main_function = NULL;
    const MachineSelectFunctionSummary *id_summary = NULL;
    const MachineSelectFunctionSummary *main_summary = NULL;
    char *actual_text = NULL;
    size_t register_count = 0;
    size_t global_count = 0;
    size_t function_count = 0;
    size_t function_index = 0;
    size_t main_function_index = 0;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_select_lower_report_init(&select_report);

    if (!build_machine_ir_report_from_extension_source_text(
            "float id(float x){ return x; } int main(){ float x = 1.25; id(1.25); return 0; }\n",
            &machine_report,
            &machine_error) ||
        !machine_select_build_report_from_machine_ir_report(&machine_report, &select_report, &select_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: float report query setup failed: %s\n",
            select_error.message[0] ? select_error.message : machine_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!machine_select_lower_report_get_summary(&select_report,
            &register_count,
            &global_count,
            &function_count,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL) ||
        register_count < 1u ||
        global_count != 0u ||
        function_count < 2u ||
        !machine_select_lower_report_get_program(&select_report, &report_program) ||
        !report_program ||
        !machine_select_lower_report_get_function_by_name(&select_report, "id", &function_index, &id_function) ||
        !id_function ||
        !machine_select_lower_report_get_function_summary_artifact(&select_report, function_index, &id_summary) ||
        !id_summary ||
        !machine_select_lower_report_get_function_by_name(&select_report, "main", &main_function_index, &main_function) ||
        !main_function ||
        !machine_select_lower_report_get_function_summary_artifact(&select_report, main_function_index, &main_summary) ||
        !main_summary ||
        id_summary->load_local_count == 0u ||
        id_summary->block_count != 1u ||
        main_summary->call_imm_count == 0u ||
        !machine_select_dump_lower_report_artifact(&select_report, &actual_text, &select_error) ||
        !strstr(actual_text, "param local.0:float name=x") ||
        !strstr(actual_text, "calli id(1067450368)")) {
        fprintf(stderr,
            "[machine-select] FAIL: float report query mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_select_lower_report_free(&select_report);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    return ok;
}

static int test_machine_select_float_global_literal_report_query_surface(void) {
    MachineIrAllocateRewriteReport machine_report;
    MachineIrError machine_error;
    MachineSelectLowerReport select_report;
    MachineSelectError select_error;
    const MachineSelectProgram *report_program = NULL;
    const MachineSelectFunction *global_init_function = NULL;
    const MachineSelectFunctionSummary *global_init_summary = NULL;
    char *actual_text = NULL;
    size_t register_count = 0;
    size_t global_count = 0;
    size_t function_count = 0;
    size_t function_index = 0;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_select_lower_report_init(&select_report);

    if (!build_machine_ir_report_from_extension_source_text(
            "float g = 1.25;\nfloat id(float x){ return x; }\nint main(){ return 0; }\n",
            &machine_report,
            &machine_error) ||
        !machine_select_build_report_from_machine_ir_report(&machine_report, &select_report, &select_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: float global literal report query setup failed: %s\n",
            select_error.message[0] ? select_error.message : machine_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!machine_select_lower_report_get_summary(&select_report,
            &register_count,
            &global_count,
            &function_count,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL) ||
        global_count != 1u ||
        function_count < 3u ||
        !machine_select_lower_report_get_program(&select_report, &report_program) ||
        !report_program ||
        report_program->global_count < 1u ||
        report_program->globals[0].value_type != AST_FUNCTION_RETURN_FLOAT ||
        !machine_select_lower_report_get_function_by_name(
            &select_report, "__global.init", &function_index, &global_init_function) ||
        !global_init_function ||
        !machine_select_lower_report_get_function_summary_artifact(
            &select_report, function_index, &global_init_summary) ||
        !global_init_summary ||
        global_init_summary->store_global_imm_count == 0u ||
        !machine_select_dump_lower_report_artifact(&select_report, &actual_text, &select_error) ||
        !strstr(actual_text, "function __global.init params=0 locals=0 spills=0") ||
        !strstr(actual_text, "store_globali global.0, 1067450368")) {
        fprintf(stderr,
            "[machine-select] FAIL: float global literal report query mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_select_lower_report_free(&select_report);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    return ok;
}

static int test_machine_select_default_pipeline_preserves_live_extension_float_assignment_transport(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float id(float x){ return x; }\n"
        "float mainf(){ float y; y = id(g); return y; }\n"
        "int main(){ return 0; }\n";
    MachineIrAllocateRewriteReport machine_report;
    MachineIrError machine_error;
    MachineSelectLowerReport select_report;
    MachineSelectError select_error;
    char *actual_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_select_lower_report_init(&select_report);

    if (!build_machine_ir_report_from_default_extension_source_text(source, &machine_report, &machine_error) ||
        !machine_select_build_report_from_machine_ir_report(&machine_report, &select_report, &select_error) ||
        !machine_select_dump_lower_report_artifact(&select_report, &actual_text, &select_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-ASSIGN-LIVE-DEFAULT setup failed: %s\n",
            select_error.message[0] ? select_error.message : machine_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "functions=4") ||
        !strstr(actual_text, "fn.2 mainf blocks=1") ||
        !strstr(actual_text, "load_global=1") ||
        !strstr(actual_text, "function mainf params=0 locals=1 spills=0") ||
        !strstr(actual_text, "reg.0 = load_global global.0") ||
        !strstr(actual_text, "ret reg.0")) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-ASSIGN-LIVE-DEFAULT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_select_lower_report_free(&select_report);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    return ok;
}

static int test_machine_select_default_pipeline_preserves_float_return_transport_from_global(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float get(){ return g; }\n"
        "int main(){ return 0; }\n";
    MachineIrAllocateRewriteReport machine_report;
    MachineIrError machine_error;
    MachineSelectLowerReport select_report;
    MachineSelectError select_error;
    char *actual_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_select_lower_report_init(&select_report);

    if (!build_machine_ir_report_from_default_extension_source_text(source, &machine_report, &machine_error) ||
        !machine_select_build_report_from_machine_ir_report(&machine_report, &select_report, &select_error) ||
        !machine_select_dump_lower_report_artifact(&select_report, &actual_text, &select_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-RETURN-GLOBAL-LIVE-DEFAULT setup failed: %s\n",
            select_error.message[0] ? select_error.message : machine_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "functions=3") ||
        !strstr(actual_text, "fn.1 get blocks=1") ||
        !strstr(actual_text, "load_global=1") ||
        !strstr(actual_text, "function get params=0 locals=0 spills=0") ||
        !strstr(actual_text, "reg.0 = load_global global.0") ||
        !strstr(actual_text, "ret reg.0")) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-RETURN-GLOBAL-LIVE-DEFAULT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_select_lower_report_free(&select_report);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    return ok;
}

static int test_machine_select_default_pipeline_preserves_float_return_transport_from_global_call(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float id(float x){ return x; }\n"
        "float get(){ return id(g); }\n"
        "int main(){ return 0; }\n";
    MachineIrAllocateRewriteReport machine_report;
    MachineIrError machine_error;
    MachineSelectLowerReport select_report;
    MachineSelectError select_error;
    char *actual_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_select_lower_report_init(&select_report);

    if (!build_machine_ir_report_from_default_extension_source_text(source, &machine_report, &machine_error) ||
        !machine_select_build_report_from_machine_ir_report(&machine_report, &select_report, &select_error) ||
        !machine_select_dump_lower_report_artifact(&select_report, &actual_text, &select_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-RETURN-GLOBAL-CALL-LIVE-DEFAULT setup failed: %s\n",
            select_error.message[0] ? select_error.message : machine_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "functions=4") ||
        !strstr(actual_text, "fn.1 id blocks=1") ||
        !strstr(actual_text, "fn.2 get blocks=1") ||
        !strstr(actual_text, "load_global=1") ||
        !strstr(actual_text, "function get params=0 locals=0 spills=0") ||
        !strstr(actual_text, "reg.0 = load_global global.0") ||
        !strstr(actual_text, "ret reg.0")) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-RETURN-GLOBAL-CALL-LIVE-DEFAULT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_select_lower_report_free(&select_report);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    return ok;
}

static int test_machine_select_default_pipeline_preserves_float_global_identifier_runtime_init(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float h = g;\n"
        "int main(){ return 0; }\n";
    MachineIrAllocateRewriteReport machine_report;
    MachineIrError machine_error;
    MachineSelectLowerReport select_report;
    MachineSelectError select_error;
    char *actual_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_select_lower_report_init(&select_report);

    if (!build_machine_ir_report_from_default_extension_source_text(source, &machine_report, &machine_error) ||
        !machine_select_build_report_from_machine_ir_report(&machine_report, &select_report, &select_error) ||
        !machine_select_dump_lower_report_artifact(&select_report, &actual_text, &select_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-GLOBAL-IDENT-INIT-DEFAULT setup failed: %s\n",
            select_error.message[0] ? select_error.message : machine_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "functions=2") ||
        !strstr(actual_text, "store_globali global.0, 1067450368") ||
        !strstr(actual_text, "store_globali global.1, 1067450368")) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-GLOBAL-IDENT-INIT-DEFAULT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_select_lower_report_free(&select_report);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    return ok;
}

static int test_machine_select_default_pipeline_preserves_float_global_call_runtime_init(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float id(float x){ return x; }\n"
        "float h = id(g);\n"
        "int main(){ return 0; }\n";
    MachineIrAllocateRewriteReport machine_report;
    MachineIrError machine_error;
    MachineSelectLowerReport select_report;
    MachineSelectError select_error;
    char *actual_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_select_lower_report_init(&select_report);

    if (!build_machine_ir_report_from_default_extension_source_text(source, &machine_report, &machine_error) ||
        !machine_select_build_report_from_machine_ir_report(&machine_report, &select_report, &select_error) ||
        !machine_select_dump_lower_report_artifact(&select_report, &actual_text, &select_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-GLOBAL-CALL-INIT-DEFAULT setup failed: %s\n",
            select_error.message[0] ? select_error.message : machine_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "functions=3") ||
        !strstr(actual_text, "fn.1 id blocks=1") ||
        !strstr(actual_text, "store_globali global.0, 1067450368") ||
        !strstr(actual_text, "store_globali global.1, 1067450368")) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-GLOBAL-CALL-INIT-DEFAULT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_select_lower_report_free(&select_report);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    return ok;
}

static int test_machine_select_rejects_global_float_operator_expression_under_extension(void) {
    MachineIrProgram machine_program;
    MachineIrError machine_error;
    MachineSelectProgram select_program;

    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);
    memset(&machine_error, 0, sizeof(machine_error));

    if (build_machine_ir_program_from_extension_source_text(
            "float g = 1.25;\nint main(){ return g + 1; }\n",
            &machine_program,
            &machine_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-GLOBAL-OP-REJECT should have failed\n");
        machine_select_program_free(&select_program);
        machine_ir_program_free(&machine_program);
        return 0;
    }
    if (strstr(machine_error.message, "SEMA-EXT-035") == NULL) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-GLOBAL-OP-REJECT mismatch: %s\n",
            machine_error.message);
        machine_select_program_free(&select_program);
        machine_ir_program_free(&machine_program);
        return 0;
    }

    machine_select_program_free(&select_program);
    machine_ir_program_free(&machine_program);
    return 1;
}

static int test_machine_select_rejects_global_float_operator_expression_in_initializer_under_extension(void) {
    MachineIrProgram machine_program;
    MachineIrError machine_error;
    MachineSelectProgram select_program;

    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);
    memset(&machine_error, 0, sizeof(machine_error));

    if (build_machine_ir_program_from_extension_source_text(
            "float g = 1.25;\nint h = g + 1;\nint main(){ return 0; }\n",
            &machine_program,
            &machine_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-GLOBAL-OP-INIT-REJECT should have failed\n");
        machine_select_program_free(&select_program);
        machine_ir_program_free(&machine_program);
        return 0;
    }
    if (strstr(machine_error.message, "SEMA-EXT-035") == NULL) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-GLOBAL-OP-INIT-REJECT mismatch: %s\n",
            machine_error.message);
        machine_select_program_free(&select_program);
        machine_ir_program_free(&machine_program);
        return 0;
    }

    machine_select_program_free(&select_program);
    machine_ir_program_free(&machine_program);
    return 1;
}

static int test_machine_select_rejects_global_float_call_result_in_initializer_under_extension(void) {
    MachineIrProgram machine_program;
    MachineIrError machine_error;
    MachineSelectProgram select_program;

    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);
    memset(&machine_error, 0, sizeof(machine_error));

    if (build_machine_ir_program_from_extension_source_text(
            "float g = 1.25;\nfloat id(float x){ return x; }\nint h = id(g);\nint main(){ return 0; }\n",
            &machine_program,
            &machine_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-GLOBAL-CALL-INIT-TYPE-REJECT should have failed\n");
        machine_select_program_free(&select_program);
        machine_ir_program_free(&machine_program);
        return 0;
    }
    if (strstr(machine_error.message, "SEMA-TYPE-004") == NULL) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-GLOBAL-CALL-INIT-TYPE-REJECT mismatch: %s\n",
            machine_error.message);
        machine_select_program_free(&select_program);
        machine_ir_program_free(&machine_program);
        return 0;
    }

    machine_select_program_free(&select_program);
    machine_ir_program_free(&machine_program);
    return 1;
}

static int test_machine_select_rejects_float_assignment_to_int_under_extension(void) {
    MachineIrProgram machine_program;
    MachineIrError machine_error;
    MachineSelectProgram select_program;

    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);
    memset(&machine_error, 0, sizeof(machine_error));

    if (build_machine_ir_program_from_extension_source_text(
            "float g = 1.25;\nint main(){ int x = 0; x = g; return 0; }\n",
            &machine_program,
            &machine_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-ASSIGN-TYPE-REJECT should have failed\n");
        machine_select_program_free(&select_program);
        machine_ir_program_free(&machine_program);
        return 0;
    }
    if (strstr(machine_error.message, "SEMA-TYPE-006") == NULL) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-ASSIGN-TYPE-REJECT mismatch: %s\n",
            machine_error.message);
        machine_select_program_free(&select_program);
        machine_ir_program_free(&machine_program);
        return 0;
    }

    machine_select_program_free(&select_program);
    machine_ir_program_free(&machine_program);
    return 1;
}

static int test_machine_select_rejects_float_ternary_value_return_to_int_under_extension(void) {
    MachineIrProgram machine_program;
    MachineIrError machine_error;
    MachineSelectProgram select_program;

    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);
    memset(&machine_error, 0, sizeof(machine_error));

    if (build_machine_ir_program_from_extension_source_text(
            "float g = 1.25;\n"
            "float h = 2.5;\n"
            "int bad(){ return g ? h : h; }\n"
            "int main(){ return 0; }\n",
            &machine_program,
            &machine_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-TERNARY-VALUE-RETURN-INT-REJECT should have failed\n");
        machine_select_program_free(&select_program);
        machine_ir_program_free(&machine_program);
        return 0;
    }
    if (strstr(machine_error.message, "SEMA-TYPE-005") == NULL) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-TERNARY-VALUE-RETURN-INT-REJECT mismatch: %s\n",
            machine_error.message);
        machine_select_program_free(&select_program);
        machine_ir_program_free(&machine_program);
        return 0;
    }

    machine_select_program_free(&select_program);
    machine_ir_program_free(&machine_program);
    return 1;
}

static int test_machine_select_rejects_unary_call_ternary_value_return_to_int_under_extension(void) {
    MachineIrProgram machine_program;
    MachineIrError machine_error;
    MachineSelectProgram select_program;

    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);
    memset(&machine_error, 0, sizeof(machine_error));

    if (build_machine_ir_program_from_extension_source_text(
            "float id(float x){ return x; }\n"
            "int bad(){ return -id(1.0) ? 1.0 : 2.0; }\n"
            "int main(){ return 0; }\n",
            &machine_program,
            &machine_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-UNARY-CALL-TERNARY-RETURN-INT-REJECT should have failed\n");
        machine_select_program_free(&select_program);
        machine_ir_program_free(&machine_program);
        return 0;
    }
    if (strstr(machine_error.message, "SEMA-TYPE-005") == NULL) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-UNARY-CALL-TERNARY-RETURN-INT-REJECT mismatch: %s\n",
            machine_error.message);
        machine_select_program_free(&select_program);
        machine_ir_program_free(&machine_program);
        return 0;
    }

    machine_select_program_free(&select_program);
    machine_ir_program_free(&machine_program);
    return 1;
}

static int test_machine_select_accepts_float_if_condition_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "int main(){ if(g) return 1; return 0; }\n";
    MachineIrAllocateRewriteReport machine_report;
    MachineIrError machine_error;
    MachineSelectLowerReport select_report;
    MachineSelectError select_error;
    char *actual_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_select_lower_report_init(&select_report);

    if (!build_machine_ir_report_from_default_extension_source_text(source, &machine_report, &machine_error) ||
        !machine_select_build_report_from_machine_ir_report(&machine_report, &select_report, &select_error) ||
        !machine_select_dump_lower_report_artifact(&select_report, &actual_text, &select_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-IF-COND-ACCEPT setup failed: %s\n",
            select_error.message[0] ? select_error.message : machine_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "functions=2") ||
        !strstr(actual_text, "function main params=0 locals=0 spills=0") ||
        !strstr(actual_text, "reti 1")) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-IF-COND-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_select_lower_report_free(&select_report);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    return ok;
}

static int test_machine_select_accepts_float_while_condition_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "int main(){ while(g) return 1; return 0; }\n";
    MachineIrAllocateRewriteReport machine_report;
    MachineIrError machine_error;
    MachineSelectLowerReport select_report;
    MachineSelectError select_error;
    char *actual_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_select_lower_report_init(&select_report);

    if (!build_machine_ir_report_from_default_extension_source_text(source, &machine_report, &machine_error) ||
        !machine_select_build_report_from_machine_ir_report(&machine_report, &select_report, &select_error) ||
        !machine_select_dump_lower_report_artifact(&select_report, &actual_text, &select_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-WHILE-COND-ACCEPT setup failed: %s\n",
            select_error.message[0] ? select_error.message : machine_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "functions=2") ||
        !strstr(actual_text, "function main params=0 locals=0 spills=0") ||
        !strstr(actual_text, "reti 1")) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-WHILE-COND-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_select_lower_report_free(&select_report);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    return ok;
}

static int test_machine_select_accepts_float_for_condition_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "int main(){ for(;g;) return 1; return 0; }\n";
    MachineIrAllocateRewriteReport machine_report;
    MachineIrError machine_error;
    MachineSelectLowerReport select_report;
    MachineSelectError select_error;
    char *actual_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_select_lower_report_init(&select_report);

    if (!build_machine_ir_report_from_default_extension_source_text(source, &machine_report, &machine_error) ||
        !machine_select_build_report_from_machine_ir_report(&machine_report, &select_report, &select_error) ||
        !machine_select_dump_lower_report_artifact(&select_report, &actual_text, &select_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-FOR-COND-ACCEPT setup failed: %s\n",
            select_error.message[0] ? select_error.message : machine_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "functions=2") ||
        !strstr(actual_text, "function main params=0 locals=0 spills=0") ||
        !strstr(actual_text, "reti 1")) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-FOR-COND-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_select_lower_report_free(&select_report);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    return ok;
}

static int test_machine_select_accepts_float_logical_condition_composition_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "int main(){ if(!g || (g && 1.25)) return g ? 1 : 0; return 0; }\n";
    MachineIrAllocateRewriteReport machine_report;
    MachineIrError machine_error;
    MachineSelectLowerReport select_report;
    MachineSelectError select_error;
    char *actual_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_select_lower_report_init(&select_report);

    if (!build_machine_ir_report_from_default_extension_source_text(source, &machine_report, &machine_error) ||
        !machine_select_build_report_from_machine_ir_report(&machine_report, &select_report, &select_error) ||
        !machine_select_dump_lower_report_artifact(&select_report, &actual_text, &select_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-LOGICAL-COND-COMPOSE-ACCEPT setup failed: %s\n",
            select_error.message[0] ? select_error.message : machine_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "functions=2") ||
        !strstr(actual_text, "function main params=0 locals=0 spills=0") ||
        !strstr(actual_text, "reg.0 = load_global global.0") ||
        !strstr(actual_text, "cmpbri.11 reg.0, 0, bb.2, bb.3") ||
        !strstr(actual_text, "ret reg.0")) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-LOGICAL-COND-COMPOSE-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_select_lower_report_free(&select_report);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    return ok;
}

static int test_machine_select_accepts_float_equality_compare_under_extension(void) {
    static const char *source =
        "int eq(float x, float y){ return x == y; }\n"
        "int main(){ return 0; }\n";
    MachineIrAllocateRewriteReport machine_report;
    MachineIrError machine_error;
    MachineSelectLowerReport select_report;
    MachineSelectError select_error;
    char *actual_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_select_lower_report_init(&select_report);

    if (!build_machine_ir_report_from_default_extension_source_text(source, &machine_report, &machine_error) ||
        !machine_select_build_report_from_machine_ir_report(&machine_report, &select_report, &select_error) ||
        !machine_select_dump_lower_report_artifact(&select_report, &actual_text, &select_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-EQ-COMPARE-ACCEPT setup failed: %s\n",
            select_error.message[0] ? select_error.message : machine_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function eq params=2 locals=2 spills=0") ||
        !strstr(actual_text, "load_local local.0") ||
        !strstr(actual_text, "load_local local.1") ||
        !strstr(actual_text, "alui.5") ||
        !strstr(actual_text, "cmpi.11") ||
        !strstr(actual_text, "alu.2") ||
        !strstr(actual_text, "cmp.10") ||
        !strstr(actual_text, "ret reg.")) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-EQ-COMPARE-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_select_lower_report_free(&select_report);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    return ok;
}

static int test_machine_select_accepts_float_relational_compare_under_extension(void) {
    static const char *source =
        "int lt(float x, float y){ return x < y; }\n"
        "int main(){ return 0; }\n";
    MachineIrAllocateRewriteReport machine_report;
    MachineIrError machine_error;
    MachineSelectLowerReport select_report;
    MachineSelectError select_error;
    char *actual_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_select_lower_report_init(&select_report);

    if (!build_machine_ir_report_from_default_extension_source_text(source, &machine_report, &machine_error) ||
        !machine_select_build_report_from_machine_ir_report(&machine_report, &select_report, &select_error) ||
        !machine_select_dump_lower_report_artifact(&select_report, &actual_text, &select_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-LT-COMPARE-ACCEPT setup failed: %s\n",
            select_error.message[0] ? select_error.message : machine_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function lt params=2 locals=2 spills=0") ||
        !strstr(actual_text, "load_local local.0") ||
        !strstr(actual_text, "load_local local.1") ||
        !strstr(actual_text, "alui.5") ||
        !strstr(actual_text, "cmpi.11") ||
        !strstr(actual_text, "alui.9") ||
        !strstr(actual_text, "alui.7") ||
        !strstr(actual_text, "alu.6") ||
        !strstr(actual_text, "cmp.12") ||
        !strstr(actual_text, "ret reg.")) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-LT-COMPARE-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_select_lower_report_free(&select_report);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    return ok;
}

static int test_machine_select_accepts_signed_zero_float_equality_under_extension(void) {
    static const char *source =
        "int z(){ return 0.0 == -0.0; }\n"
        "int main(){ return 0; }\n";
    MachineIrAllocateRewriteReport machine_report;
    MachineIrError machine_error;
    MachineSelectLowerReport select_report;
    MachineSelectError select_error;
    char *actual_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_select_lower_report_init(&select_report);

    if (!build_machine_ir_report_from_default_extension_source_text(source, &machine_report, &machine_error) ||
        !machine_select_build_report_from_machine_ir_report(&machine_report, &select_report, &select_error) ||
        !machine_select_dump_lower_report_artifact(&select_report, &actual_text, &select_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-SIGNED-ZERO-EQ-ACCEPT setup failed: %s\n",
            select_error.message[0] ? select_error.message : machine_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function z params=0 locals=0 spills=0") ||
        !strstr(actual_text, "reti 1")) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-SIGNED-ZERO-EQ-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_select_lower_report_free(&select_report);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    return ok;
}

static int test_machine_select_accepts_negative_float_relational_compare_under_extension(void) {
    static const char *source =
        "int lt(){ return -1.25 < 0.0; }\n"
        "int main(){ return 0; }\n";
    MachineIrAllocateRewriteReport machine_report;
    MachineIrError machine_error;
    MachineSelectLowerReport select_report;
    MachineSelectError select_error;
    char *actual_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_select_lower_report_init(&select_report);

    if (!build_machine_ir_report_from_default_extension_source_text(source, &machine_report, &machine_error) ||
        !machine_select_build_report_from_machine_ir_report(&machine_report, &select_report, &select_error) ||
        !machine_select_dump_lower_report_artifact(&select_report, &actual_text, &select_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-NEG-LT-ZERO-ACCEPT setup failed: %s\n",
            select_error.message[0] ? select_error.message : machine_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function lt params=0 locals=0 spills=0") ||
        !strstr(actual_text, "reti 0")) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-NEG-LT-ZERO-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_select_lower_report_free(&select_report);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    return ok;
}

static int test_machine_select_accepts_float_addition_under_extension(void) {
    static const char *source =
        "float add(float x, float y){ return x + y; }\n"
        "int main(){ return 0; }\n";
    MachineIrAllocateRewriteReport machine_report;
    MachineIrError machine_error;
    MachineSelectLowerReport select_report;
    MachineSelectError select_error;
    char *actual_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_select_lower_report_init(&select_report);

    if (!build_machine_ir_report_from_default_extension_source_text(source, &machine_report, &machine_error) ||
        !machine_select_build_report_from_machine_ir_report(&machine_report, &select_report, &select_error) ||
        !machine_select_dump_lower_report_artifact(&select_report, &actual_text, &select_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-ADD-ACCEPT setup failed: %s\n",
            select_error.message[0] ? select_error.message : machine_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function add params=2 locals=2 spills=0") ||
        !strstr(actual_text, "load_local local.0") ||
        !strstr(actual_text, "load_local local.1") ||
        !strstr(actual_text, "call __builtin_fadd32(") ||
        !strstr(actual_text, "ret reg.")) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-ADD-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_select_lower_report_free(&select_report);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    return ok;
}

static int test_machine_select_accepts_float_subtraction_under_extension(void) {
    static const char *source =
        "float sub(float x, float y){ return x - y; }\n"
        "int main(){ return 0; }\n";
    MachineIrAllocateRewriteReport machine_report;
    MachineIrError machine_error;
    MachineSelectLowerReport select_report;
    MachineSelectError select_error;
    char *actual_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_select_lower_report_init(&select_report);

    if (!build_machine_ir_report_from_default_extension_source_text(source, &machine_report, &machine_error) ||
        !machine_select_build_report_from_machine_ir_report(&machine_report, &select_report, &select_error) ||
        !machine_select_dump_lower_report_artifact(&select_report, &actual_text, &select_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-SUB-ACCEPT setup failed: %s\n",
            select_error.message[0] ? select_error.message : machine_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function sub params=2 locals=2 spills=0") ||
        !strstr(actual_text, "load_local local.0") ||
        !strstr(actual_text, "load_local local.1") ||
        !strstr(actual_text, "call __builtin_fsub32(") ||
        !strstr(actual_text, "ret reg.")) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-SUB-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_select_lower_report_free(&select_report);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    return ok;
}

static int test_machine_select_accepts_negative_float_addition_combo_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float add(float y){ return -g + y; }\n"
        "int main(){ return 0; }\n";
    MachineIrAllocateRewriteReport machine_report;
    MachineIrError machine_error;
    MachineSelectLowerReport select_report;
    MachineSelectError select_error;
    char *actual_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_select_lower_report_init(&select_report);

    if (!build_machine_ir_report_from_default_extension_source_text(source, &machine_report, &machine_error) ||
        !machine_select_build_report_from_machine_ir_report(&machine_report, &select_report, &select_error) ||
        !machine_select_dump_lower_report_artifact(&select_report, &actual_text, &select_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-NEG-ADD-COMBO-ACCEPT setup failed: %s\n",
            select_error.message[0] ? select_error.message : machine_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function add params=1 locals=1 spills=0") ||
        !strstr(actual_text, "function __builtin_fadd32 params=0 locals=0 spills=0") ||
        !strstr(actual_text, "load_global global.0") ||
        !strstr(actual_text, "load_local local.0") ||
        !strstr(actual_text, "alui.6") ||
        !strstr(actual_text, "call __builtin_fadd32(") ||
        !strstr(actual_text, "ret reg.")) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-NEG-ADD-COMBO-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_select_lower_report_free(&select_report);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    return ok;
}

static int test_machine_select_accepts_negative_float_subtraction_combo_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float sub(float y){ return y - -g; }\n"
        "int main(){ return 0; }\n";
    MachineIrAllocateRewriteReport machine_report;
    MachineIrError machine_error;
    MachineSelectLowerReport select_report;
    MachineSelectError select_error;
    char *actual_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_select_lower_report_init(&select_report);

    if (!build_machine_ir_report_from_default_extension_source_text(source, &machine_report, &machine_error) ||
        !machine_select_build_report_from_machine_ir_report(&machine_report, &select_report, &select_error) ||
        !machine_select_dump_lower_report_artifact(&select_report, &actual_text, &select_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-NEG-SUB-COMBO-ACCEPT setup failed: %s\n",
            select_error.message[0] ? select_error.message : machine_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function sub params=1 locals=1 spills=0") ||
        !strstr(actual_text, "function __builtin_fsub32 params=0 locals=0 spills=0") ||
        !strstr(actual_text, "load_global global.0") ||
        !strstr(actual_text, "load_local local.0") ||
        !strstr(actual_text, "alui.6") ||
        !strstr(actual_text, "call __builtin_fsub32(") ||
        !strstr(actual_text, "ret reg.")) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-NEG-SUB-COMBO-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_select_lower_report_free(&select_report);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    return ok;
}

static int test_machine_select_accepts_float_multiplication_under_extension(void) {
    static const char *source =
        "float mul(float x, float y){ return x * y; }\n"
        "int main(){ return 0; }\n";
    MachineIrAllocateRewriteReport machine_report;
    MachineIrError machine_error;
    MachineSelectLowerReport select_report;
    MachineSelectError select_error;
    char *actual_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_select_lower_report_init(&select_report);

    if (!build_machine_ir_report_from_default_extension_source_text(source, &machine_report, &machine_error) ||
        !machine_select_build_report_from_machine_ir_report(&machine_report, &select_report, &select_error) ||
        !machine_select_dump_lower_report_artifact(&select_report, &actual_text, &select_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-MUL-ACCEPT setup failed: %s\n",
            select_error.message[0] ? select_error.message : machine_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function mul params=2 locals=2 spills=0") ||
        !strstr(actual_text, "call __builtin_fmul32(") ||
        !strstr(actual_text, "ret reg.")) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-MUL-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_select_lower_report_free(&select_report);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    return ok;
}

static int test_machine_select_accepts_float_division_under_extension(void) {
    static const char *source =
        "float divv(float x, float y){ return x / y; }\n"
        "int main(){ return 0; }\n";
    MachineIrAllocateRewriteReport machine_report;
    MachineIrError machine_error;
    MachineSelectLowerReport select_report;
    MachineSelectError select_error;
    char *actual_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_select_lower_report_init(&select_report);

    if (!build_machine_ir_report_from_default_extension_source_text(source, &machine_report, &machine_error) ||
        !machine_select_build_report_from_machine_ir_report(&machine_report, &select_report, &select_error) ||
        !machine_select_dump_lower_report_artifact(&select_report, &actual_text, &select_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-DIV-ACCEPT setup failed: %s\n",
            select_error.message[0] ? select_error.message : machine_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function divv params=2 locals=2 spills=0") ||
        !strstr(actual_text, "call __builtin_fdiv32(") ||
        !strstr(actual_text, "ret reg.")) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-DIV-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_select_lower_report_free(&select_report);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    return ok;
}

static int test_machine_select_accepts_negative_float_multiplication_combo_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float mul(float y){ return -g * y; }\n"
        "int main(){ return 0; }\n";
    MachineIrAllocateRewriteReport machine_report;
    MachineIrError machine_error;
    MachineSelectLowerReport select_report;
    MachineSelectError select_error;
    char *actual_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_select_lower_report_init(&select_report);

    if (!build_machine_ir_report_from_default_extension_source_text(source, &machine_report, &machine_error) ||
        !machine_select_build_report_from_machine_ir_report(&machine_report, &select_report, &select_error) ||
        !machine_select_dump_lower_report_artifact(&select_report, &actual_text, &select_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-NEG-MUL-COMBO-ACCEPT setup failed: %s\n",
            select_error.message[0] ? select_error.message : machine_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function mul params=1 locals=1 spills=0") ||
        !strstr(actual_text, "load_global global.0") ||
        !strstr(actual_text, "alui.6 reg.") ||
        !strstr(actual_text, "call __builtin_fmul32(") ||
        !strstr(actual_text, "ret reg.")) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-NEG-MUL-COMBO-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_select_lower_report_free(&select_report);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    return ok;
}

static int test_machine_select_accepts_negative_float_division_combo_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float divv(float y){ return y / -g; }\n"
        "int main(){ return 0; }\n";
    MachineIrAllocateRewriteReport machine_report;
    MachineIrError machine_error;
    MachineSelectLowerReport select_report;
    MachineSelectError select_error;
    char *actual_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_select_lower_report_init(&select_report);

    if (!build_machine_ir_report_from_default_extension_source_text(source, &machine_report, &machine_error) ||
        !machine_select_build_report_from_machine_ir_report(&machine_report, &select_report, &select_error) ||
        !machine_select_dump_lower_report_artifact(&select_report, &actual_text, &select_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-NEG-DIV-COMBO-ACCEPT setup failed: %s\n",
            select_error.message[0] ? select_error.message : machine_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function divv params=1 locals=1 spills=0") ||
        !strstr(actual_text, "load_global global.0") ||
        !strstr(actual_text, "alui.6 reg.") ||
        !strstr(actual_text, "call __builtin_fdiv32(") ||
        !strstr(actual_text, "ret reg.")) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-NEG-DIV-COMBO-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_select_lower_report_free(&select_report);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    return ok;
}

static int test_machine_select_accepts_chained_float_addition_under_extension(void) {
    static const char *source =
        "float add3(float x, float y, float z){ return (x + y) + z; }\n"
        "int main(){ return 0; }\n";
    MachineIrAllocateRewriteReport machine_report;
    MachineIrError machine_error;
    MachineSelectLowerReport select_report;
    MachineSelectError select_error;
    char *actual_text = NULL;
    const char *first_add = NULL;
    const char *second_add = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_select_lower_report_init(&select_report);

    if (!build_machine_ir_report_from_default_extension_source_text(source, &machine_report, &machine_error) ||
        !machine_select_build_report_from_machine_ir_report(&machine_report, &select_report, &select_error) ||
        !machine_select_dump_lower_report_artifact(&select_report, &actual_text, &select_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-CHAIN-ADD-ACCEPT setup failed: %s\n",
            select_error.message[0] ? select_error.message : machine_error.message);
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
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-CHAIN-ADD-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_select_lower_report_free(&select_report);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    return ok;
}

static int test_machine_select_accepts_explicit_int_from_float_conversion_under_extension(void) {
    static const char *source =
        "int conv(float x, float y){ return int(x + y); }\n"
        "int main(){ return 0; }\n";
    MachineIrAllocateRewriteReport machine_report;
    MachineIrError machine_error;
    MachineSelectLowerReport select_report;
    MachineSelectError select_error;
    char *actual_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_select_lower_report_init(&select_report);

    if (!build_machine_ir_report_from_default_extension_source_text(source, &machine_report, &machine_error) ||
        !machine_select_build_report_from_machine_ir_report(&machine_report, &select_report, &select_error) ||
        !machine_select_dump_lower_report_artifact(&select_report, &actual_text, &select_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-TO-INT-CONVERT-ACCEPT setup failed: %s\n",
            select_error.message[0] ? select_error.message : machine_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function conv params=2") ||
        !strstr(actual_text, "call __builtin_fadd32(") ||
        !strstr(actual_text, "call __builtin_f2i32(") ||
        !strstr(actual_text, "ret reg.")) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-TO-INT-CONVERT-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_select_lower_report_free(&select_report);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    return ok;
}

static int test_machine_select_accepts_explicit_float_from_int_conversion_under_extension(void) {
    static const char *source =
        "float conv(int x, int y){ return float(x + y); }\n"
        "int main(){ return 0; }\n";
    MachineIrAllocateRewriteReport machine_report;
    MachineIrError machine_error;
    MachineSelectLowerReport select_report;
    MachineSelectError select_error;
    char *actual_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_select_lower_report_init(&select_report);

    if (!build_machine_ir_report_from_default_extension_source_text(source, &machine_report, &machine_error) ||
        !machine_select_build_report_from_machine_ir_report(&machine_report, &select_report, &select_error) ||
        !machine_select_dump_lower_report_artifact(&select_report, &actual_text, &select_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-INT-TO-FLOAT-CONVERT-ACCEPT setup failed: %s\n",
            select_error.message[0] ? select_error.message : machine_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function conv params=2") ||
        !strstr(actual_text, "alu.0") ||
        !strstr(actual_text, "call __builtin_i2f32(") ||
        !strstr(actual_text, "ret reg.")) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-INT-TO-FLOAT-CONVERT-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_select_lower_report_free(&select_report);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    return ok;
}

static int test_machine_select_accepts_explicit_float_from_recursive_int_initializer_bridge_under_extension(void) {
    static const char *source =
        "int add3(int a, int b, int c){ return (a + b) + c; }\n"
        "float z = float(add3(1, 2, 3));\n"
        "int main(){ return 0; }\n";
    MachineIrAllocateRewriteReport machine_report;
    MachineIrError machine_error;
    MachineSelectLowerReport select_report;
    MachineSelectError select_error;
    char *actual_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_select_lower_report_init(&select_report);

    if (!build_machine_ir_report_from_default_extension_source_text(source, &machine_report, &machine_error) ||
        !machine_select_build_report_from_machine_ir_report(&machine_report, &select_report, &select_error) ||
        !machine_select_dump_lower_report_artifact(&select_report, &actual_text, &select_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-INT-TO-FLOAT-RECURSIVE-INIT-BRIDGE-ACCEPT setup failed: %s\n",
            select_error.message[0] ? select_error.message : machine_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function __global.init params=0") ||
        (!strstr(actual_text, "calli __builtin_i2f32(6)") &&
            !strstr(actual_text, "call __builtin_i2f32(6)")) ||
        !strstr(actual_text, "store_global")) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-INT-TO-FLOAT-RECURSIVE-INIT-BRIDGE-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_select_lower_report_free(&select_report);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    return ok;
}

static int test_machine_select_accepts_explicit_float_from_recursive_int_assignment_bridge_under_extension(void) {
    static const char *source =
        "int add3(int a, int b, int c){ return (a + b) + c; }\n"
        "float mainf(){ float y; y = float(add3(1, 2, 3)); return y; }\n"
        "int main(){ return 0; }\n";
    MachineIrAllocateRewriteReport machine_report;
    MachineIrError machine_error;
    MachineSelectLowerReport select_report;
    MachineSelectError select_error;
    char *actual_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_select_lower_report_init(&select_report);

    if (!build_machine_ir_report_from_default_extension_source_text(source, &machine_report, &machine_error) ||
        !machine_select_build_report_from_machine_ir_report(&machine_report, &select_report, &select_error) ||
        !machine_select_dump_lower_report_artifact(&select_report, &actual_text, &select_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-INT-TO-FLOAT-RECURSIVE-ASSIGN-BRIDGE-ACCEPT setup failed: %s\n",
            select_error.message[0] ? select_error.message : machine_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function mainf params=0") ||
        (!strstr(actual_text, "call __builtin_i2f32(6)") &&
            !strstr(actual_text, "calli __builtin_i2f32(6)")) ||
        !strstr(actual_text, "store_local local.0") ||
        !strstr(actual_text, "load_local local.0")) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-INT-TO-FLOAT-RECURSIVE-ASSIGN-BRIDGE-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_select_lower_report_free(&select_report);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    return ok;
}

static int test_machine_select_accepts_explicit_float_from_int_compare_bridge_under_extension(void) {
    static const char *source =
        "int add3(int a, int b, int c){ return (a + b) + c; }\n"
        "int main(){ return float(add3(1, 2, 3)) == float(6); }\n";
    MachineIrAllocateRewriteReport machine_report;
    MachineIrError machine_error;
    MachineSelectLowerReport select_report;
    MachineSelectError select_error;
    char *actual_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_select_lower_report_init(&select_report);

    if (!build_machine_ir_report_from_default_extension_source_text(source, &machine_report, &machine_error) ||
        !machine_select_build_report_from_machine_ir_report(&machine_report, &select_report, &select_error) ||
        !machine_select_dump_lower_report_artifact(&select_report, &actual_text, &select_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-INT-TO-FLOAT-COMPARE-BRIDGE-ACCEPT setup failed: %s\n",
            select_error.message[0] ? select_error.message : machine_error.message);
        ok = 0;
        goto cleanup;
    }

    if ((!strstr(actual_text, "call __builtin_i2f32(6)") &&
            !strstr(actual_text, "calli __builtin_i2f32(6)")) ||
        !strstr(actual_text, "ret reg.")) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-INT-TO-FLOAT-COMPARE-BRIDGE-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_select_lower_report_free(&select_report);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    return ok;
}

static int test_machine_select_accepts_explicit_float_from_recursive_int_arithmetic_bridge_under_extension(void) {
    static const char *source =
        "int add3(int a, int b, int c){ return (a + b) + c; }\n"
        "float mainf(){ return float(add3(1, 2, 3)) + 1.25; }\n"
        "int main(){ return 0; }\n";
    MachineIrAllocateRewriteReport machine_report;
    MachineIrError machine_error;
    MachineSelectLowerReport select_report;
    MachineSelectError select_error;
    char *actual_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_select_lower_report_init(&select_report);

    if (!build_machine_ir_report_from_default_extension_source_text(source, &machine_report, &machine_error) ||
        !machine_select_build_report_from_machine_ir_report(&machine_report, &select_report, &select_error) ||
        !machine_select_dump_lower_report_artifact(&select_report, &actual_text, &select_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-INT-TO-FLOAT-RECURSIVE-ARITH-BRIDGE-ACCEPT setup failed: %s\n",
            select_error.message[0] ? select_error.message : machine_error.message);
        ok = 0;
        goto cleanup;
    }

    if ((!strstr(actual_text, "call __builtin_i2f32(") &&
            !strstr(actual_text, "calli __builtin_i2f32(")) ||
        !strstr(actual_text, "alui") ||
        !strstr(actual_text, "ret reg.")) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-INT-TO-FLOAT-RECURSIVE-ARITH-BRIDGE-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_select_lower_report_free(&select_report);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    return ok;
}

static int test_machine_select_accepts_explicit_int_from_float_ternary_bridge_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "int main(){ return int(g ? h : h); }\n";
    MachineIrAllocateRewriteReport machine_report;
    MachineIrError machine_error;
    MachineSelectLowerReport select_report;
    MachineSelectError select_error;
    char *actual_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_select_lower_report_init(&select_report);

    if (!build_machine_ir_report_from_default_extension_source_text(source, &machine_report, &machine_error) ||
        !machine_select_build_report_from_machine_ir_report(&machine_report, &select_report, &select_error) ||
        !machine_select_dump_lower_report_artifact(&select_report, &actual_text, &select_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-TO-INT-TERNARY-BRIDGE-ACCEPT setup failed: %s\n",
            select_error.message[0] ? select_error.message : machine_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function main params=0") ||
        !strstr(actual_text, "store_globali global.0, 1067450368") ||
        !strstr(actual_text, "store_globali global.1, 1075838976") ||
        !strstr(actual_text, "calli __builtin_f2i32(1075838976)") ||
        !strstr(actual_text, "ret reg.")) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-TO-INT-TERNARY-BRIDGE-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_select_lower_report_free(&select_report);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    return ok;
}

static int test_machine_select_accepts_explicit_int_from_recursive_float_call_argument_bridge_under_extension(void) {
    static const char *source =
        "int sink(int x){ return x; }\n"
        "float add3(float a, float b, float c){ return (a + b) + c; }\n"
        "int main(){ return sink(int(add3(1.0, 2.0, 3.0))); }\n";
    MachineIrAllocateRewriteReport machine_report;
    MachineIrError machine_error;
    MachineSelectLowerReport select_report;
    MachineSelectError select_error;
    char *actual_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_select_lower_report_init(&select_report);

    if (!build_machine_ir_report_from_default_extension_source_text(source, &machine_report, &machine_error) ||
        !machine_select_build_report_from_machine_ir_report(&machine_report, &select_report, &select_error) ||
        !machine_select_dump_lower_report_artifact(&select_report, &actual_text, &select_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-TO-INT-RECURSIVE-CALLARG-BRIDGE-ACCEPT setup failed: %s\n",
            select_error.message[0] ? select_error.message : machine_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function add3 params=3") ||
        !strstr(actual_text, "call __builtin_fadd32(") ||
        !strstr(actual_text, "call __builtin_f2i32(") ||
        !strstr(actual_text, "function sink params=1") ||
        !strstr(actual_text, "ret reg.")) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-TO-INT-RECURSIVE-CALLARG-BRIDGE-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_select_lower_report_free(&select_report);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    return ok;
}

static int test_machine_select_accepts_explicit_int_from_recursive_float_assignment_bridge_under_extension(void) {
    static const char *source =
        "float add3(float a, float b, float c){ return (a + b) + c; }\n"
        "int main(){ int x=0; x = int(add3(1.0, 2.0, 3.0)); return x; }\n";
    MachineIrAllocateRewriteReport machine_report;
    MachineIrError machine_error;
    MachineSelectLowerReport select_report;
    MachineSelectError select_error;
    char *actual_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_select_lower_report_init(&select_report);

    if (!build_machine_ir_report_from_default_extension_source_text(source, &machine_report, &machine_error) ||
        !machine_select_build_report_from_machine_ir_report(&machine_report, &select_report, &select_error) ||
        !machine_select_dump_lower_report_artifact(&select_report, &actual_text, &select_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-TO-INT-RECURSIVE-ASSIGN-BRIDGE-ACCEPT setup failed: %s\n",
            select_error.message[0] ? select_error.message : machine_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function add3 params=3") ||
        !strstr(actual_text, "call __builtin_f2i32(") ||
        !strstr(actual_text, "store_local local.0") ||
        !strstr(actual_text, "load_local local.0") ||
        !strstr(actual_text, "ret reg.")) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-TO-INT-RECURSIVE-ASSIGN-BRIDGE-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_select_lower_report_free(&select_report);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    return ok;
}

static int test_machine_select_accepts_explicit_int_from_float_compare_bridge_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "int main(){ return int(g ? h : h) == 2; }\n";
    MachineIrAllocateRewriteReport machine_report;
    MachineIrError machine_error;
    MachineSelectLowerReport select_report;
    MachineSelectError select_error;
    char *actual_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_select_lower_report_init(&select_report);

    if (!build_machine_ir_report_from_default_extension_source_text(source, &machine_report, &machine_error) ||
        !machine_select_build_report_from_machine_ir_report(&machine_report, &select_report, &select_error) ||
        !machine_select_dump_lower_report_artifact(&select_report, &actual_text, &select_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-TO-INT-COMPARE-BRIDGE-ACCEPT setup failed: %s\n",
            select_error.message[0] ? select_error.message : machine_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "calli __builtin_f2i32(") &&
        !strstr(actual_text, "call __builtin_f2i32(")) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-TO-INT-COMPARE-BRIDGE-ACCEPT missing conversion helper\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }
    if ((!strstr(actual_text, "cmpbri") &&
            !strstr(actual_text, " sltiu ") &&
            !strstr(actual_text, " xori ")) ||
        !strstr(actual_text, "ret")) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-TO-INT-COMPARE-BRIDGE-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_select_lower_report_free(&select_report);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    return ok;
}

static int test_machine_select_accepts_explicit_int_from_recursive_float_arithmetic_bridge_under_extension(void) {
    static const char *source =
        "float add3(float a, float b, float c){ return (a + b) + c; }\n"
        "int main(){ return int(add3(1.0, 2.0, 3.0)) + 1; }\n";
    MachineIrAllocateRewriteReport machine_report;
    MachineIrError machine_error;
    MachineSelectLowerReport select_report;
    MachineSelectError select_error;
    char *actual_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_select_lower_report_init(&select_report);

    if (!build_machine_ir_report_from_default_extension_source_text(source, &machine_report, &machine_error) ||
        !machine_select_build_report_from_machine_ir_report(&machine_report, &select_report, &select_error) ||
        !machine_select_dump_lower_report_artifact(&select_report, &actual_text, &select_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-TO-INT-RECURSIVE-ARITH-BRIDGE-ACCEPT setup failed: %s\n",
            select_error.message[0] ? select_error.message : machine_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function add3 params=3") ||
        !strstr(actual_text, "call __builtin_f2i32(") ||
        !strstr(actual_text, "alui") ||
        !strstr(actual_text, "ret reg.")) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-TO-INT-RECURSIVE-ARITH-BRIDGE-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_select_lower_report_free(&select_report);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    return ok;
}

static int test_machine_select_accepts_nested_float_mul_div_under_extension(void) {
    static const char *source =
        "float f(float a, float b, float c){ return -a * (b / c); }\n"
        "int main(){ return 0; }\n";
    MachineIrAllocateRewriteReport machine_report;
    MachineIrError machine_error;
    MachineSelectLowerReport select_report;
    MachineSelectError select_error;
    char *actual_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_select_lower_report_init(&select_report);

    if (!build_machine_ir_report_from_default_extension_source_text(source, &machine_report, &machine_error) ||
        !machine_select_build_report_from_machine_ir_report(&machine_report, &select_report, &select_error) ||
        !machine_select_dump_lower_report_artifact(&select_report, &actual_text, &select_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-NESTED-MUL-DIV-ACCEPT setup failed: %s\n",
            select_error.message[0] ? select_error.message : machine_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function f params=3") ||
        !strstr(actual_text, "call __builtin_fdiv32(") ||
        !strstr(actual_text, "call __builtin_fmul32(") ||
        !strstr(actual_text, "ret reg.")) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-NESTED-MUL-DIV-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_select_lower_report_free(&select_report);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    return ok;
}

static int test_machine_select_rejects_mixed_float_int_arithmetic_under_extension(void) {
    MachineIrProgram machine_program;
    MachineIrError machine_error;
    MachineSelectProgram select_program;

    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);
    memset(&machine_error, 0, sizeof(machine_error));

    if (build_machine_ir_program_from_extension_source_text(
            "float add(float x){ return x + 1; }\n"
            "int main(){ return 0; }\n",
            &machine_program,
            &machine_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-ARITH-INT-TYPE-REJECT should have failed\n");
        machine_select_program_free(&select_program);
        machine_ir_program_free(&machine_program);
        return 0;
    }
    if (strstr(machine_error.message, "SEMA-TYPE-008") == NULL) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-ARITH-INT-TYPE-REJECT mismatch: %s\n",
            machine_error.message);
        machine_select_program_free(&select_program);
        machine_ir_program_free(&machine_program);
        return 0;
    }

    machine_select_program_free(&select_program);
    machine_ir_program_free(&machine_program);
    return 1;
}

static int test_machine_select_rejects_float_call_int_arithmetic_under_extension(void) {
    MachineIrProgram machine_program;
    MachineIrError machine_error;
    MachineSelectProgram select_program;

    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);
    memset(&machine_error, 0, sizeof(machine_error));

    if (build_machine_ir_program_from_extension_source_text(
            "float id(float x){ return x; }\n"
            "float add(float x){ return id(x) + 1; }\n"
            "int main(){ return 0; }\n",
            &machine_program,
            &machine_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-CALL-ARITH-INT-TYPE-REJECT should have failed\n");
        machine_select_program_free(&select_program);
        machine_ir_program_free(&machine_program);
        return 0;
    }
    if (strstr(machine_error.message, "SEMA-TYPE-008") == NULL) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-CALL-ARITH-INT-TYPE-REJECT mismatch: %s\n",
            machine_error.message);
        machine_select_program_free(&select_program);
        machine_ir_program_free(&machine_program);
        return 0;
    }

    machine_select_program_free(&select_program);
    machine_ir_program_free(&machine_program);
    return 1;
}

static int test_machine_select_rejects_float_literal_int_arithmetic_under_extension(void) {
    MachineIrProgram machine_program;
    MachineIrError machine_error;
    MachineSelectProgram select_program;

    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);
    memset(&machine_error, 0, sizeof(machine_error));

    if (build_machine_ir_program_from_extension_source_text(
            "float add(){ return 1.25 + 1; }\n"
            "int main(){ return 0; }\n",
            &machine_program,
            &machine_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-LITERAL-ARITH-INT-TYPE-REJECT should have failed\n");
        machine_select_program_free(&select_program);
        machine_ir_program_free(&machine_program);
        return 0;
    }
    if (strstr(machine_error.message, "SEMA-TYPE-008") == NULL) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-LITERAL-ARITH-INT-TYPE-REJECT mismatch: %s\n",
            machine_error.message);
        machine_select_program_free(&select_program);
        machine_ir_program_free(&machine_program);
        return 0;
    }

    machine_select_program_free(&select_program);
    machine_ir_program_free(&machine_program);
    return 1;
}

static int test_machine_select_rejects_negative_float_call_int_arithmetic_under_extension(void) {
    MachineIrProgram machine_program;
    MachineIrError machine_error;
    MachineSelectProgram select_program;

    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);
    memset(&machine_error, 0, sizeof(machine_error));

    if (build_machine_ir_program_from_extension_source_text(
            "float id(float x){ return x; }\n"
            "float add(float x){ return -id(x) * 1; }\n"
            "int main(){ return 0; }\n",
            &machine_program,
            &machine_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-NEG-CALL-ARITH-INT-TYPE-REJECT should have failed\n");
        machine_select_program_free(&select_program);
        machine_ir_program_free(&machine_program);
        return 0;
    }
    if (strstr(machine_error.message, "SEMA-TYPE-008") == NULL) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-NEG-CALL-ARITH-INT-TYPE-REJECT mismatch: %s\n",
            machine_error.message);
        machine_select_program_free(&select_program);
        machine_ir_program_free(&machine_program);
        return 0;
    }

    machine_select_program_free(&select_program);
    machine_ir_program_free(&machine_program);
    return 1;
}

static int test_machine_select_rejects_nested_float_tree_plus_int_under_extension(void) {
    MachineIrProgram machine_program;
    MachineIrError machine_error;
    MachineSelectProgram select_program;

    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);
    memset(&machine_error, 0, sizeof(machine_error));

    if (build_machine_ir_program_from_extension_source_text(
            "float add(float x, float y){ return (x + y) + 1; }\n"
            "int main(){ return 0; }\n",
            &machine_program,
            &machine_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-NESTED-TREE-PLUS-INT-REJECT should have failed\n");
        machine_select_program_free(&select_program);
        machine_ir_program_free(&machine_program);
        return 0;
    }
    if (strstr(machine_error.message, "SEMA-EXT-035") == NULL) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-NESTED-TREE-PLUS-INT-REJECT mismatch: %s\n",
            machine_error.message);
        machine_select_program_free(&select_program);
        machine_ir_program_free(&machine_program);
        return 0;
    }

    machine_select_program_free(&select_program);
    machine_ir_program_free(&machine_program);
    return 1;
}

static int test_machine_select_rejects_nested_float_muldiv_plus_int_under_extension(void) {
    MachineIrProgram machine_program;
    MachineIrError machine_error;
    MachineSelectProgram select_program;

    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);
    memset(&machine_error, 0, sizeof(machine_error));

    if (build_machine_ir_program_from_extension_source_text(
            "float f(float a, float b, float c){ return (-a * (b / c)) + 1; }\n"
            "int main(){ return 0; }\n",
            &machine_program,
            &machine_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-NESTED-MULDIV-PLUS-INT-REJECT should have failed\n");
        machine_select_program_free(&select_program);
        machine_ir_program_free(&machine_program);
        return 0;
    }
    if (strstr(machine_error.message, "SEMA-EXT-035") == NULL) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-NESTED-MULDIV-PLUS-INT-REJECT mismatch: %s\n",
            machine_error.message);
        machine_select_program_free(&select_program);
        machine_ir_program_free(&machine_program);
        return 0;
    }

    machine_select_program_free(&select_program);
    machine_ir_program_free(&machine_program);
    return 1;
}

static int test_machine_select_rejects_float_ternary_value_plus_int_under_extension(void) {
    MachineIrProgram machine_program;
    MachineIrError machine_error;
    MachineSelectProgram select_program;

    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);
    memset(&machine_error, 0, sizeof(machine_error));

    if (build_machine_ir_program_from_extension_source_text(
            "float g = 1.25;\n"
            "float h = 2.5;\n"
            "int main(){ return (g ? h : h) + 1; }\n",
            &machine_program,
            &machine_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-TERNARY-VALUE-PLUS-INT-REJECT should have failed\n");
        machine_select_program_free(&select_program);
        machine_ir_program_free(&machine_program);
        return 0;
    }
    if (strstr(machine_error.message, "SEMA-EXT-035") == NULL) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-TERNARY-VALUE-PLUS-INT-REJECT mismatch: %s\n",
            machine_error.message);
        machine_select_program_free(&select_program);
        machine_ir_program_free(&machine_program);
        return 0;
    }

    machine_select_program_free(&select_program);
    machine_ir_program_free(&machine_program);
    return 1;
}

static int test_machine_select_rejects_unary_call_ternary_value_plus_int_under_extension(void) {
    MachineIrProgram machine_program;
    MachineIrError machine_error;
    MachineSelectProgram select_program;

    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);
    memset(&machine_error, 0, sizeof(machine_error));

    if (build_machine_ir_program_from_extension_source_text(
            "float id(float x){ return x; }\n"
            "float add(float x){ return (-id(x) ? x : x) + 1; }\n"
            "int main(){ return 0; }\n",
            &machine_program,
            &machine_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-UNARY-CALL-TERNARY-PLUS-INT-REJECT should have failed\n");
        machine_select_program_free(&select_program);
        machine_ir_program_free(&machine_program);
        return 0;
    }
    if (strstr(machine_error.message, "SEMA-EXT-035") == NULL) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-UNARY-CALL-TERNARY-PLUS-INT-REJECT mismatch: %s\n",
            machine_error.message);
        machine_select_program_free(&select_program);
        machine_ir_program_free(&machine_program);
        return 0;
    }

    machine_select_program_free(&select_program);
    machine_ir_program_free(&machine_program);
    return 1;
}

static int test_machine_select_rejects_float_ternary_value_plus_float_call_argument_under_extension(void) {
    MachineIrProgram machine_program;
    MachineIrError machine_error;
    MachineSelectProgram select_program;

    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);
    memset(&machine_error, 0, sizeof(machine_error));

    if (build_machine_ir_program_from_extension_source_text(
            "float g = 1.25;\n"
            "float h = 2.5;\n"
            "float wrap(float x){ return x; }\n"
            "float get(){ return wrap((g ? h : h) + h); }\n"
            "int main(){ return 0; }\n",
            &machine_program,
            &machine_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-TERNARY-PLUS-FLOAT-CALLARG-REJECT should have failed\n");
        machine_select_program_free(&select_program);
        machine_ir_program_free(&machine_program);
        return 0;
    }
    if (strstr(machine_error.message, "SEMA-EXT-035") == NULL) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-TERNARY-PLUS-FLOAT-CALLARG-REJECT mismatch: %s\n",
            machine_error.message);
        machine_select_program_free(&select_program);
        machine_ir_program_free(&machine_program);
        return 0;
    }

    machine_select_program_free(&select_program);
    machine_ir_program_free(&machine_program);
    return 1;
}

static int test_machine_select_rejects_unary_call_ternary_value_plus_float_call_argument_under_extension(void) {
    MachineIrProgram machine_program;
    MachineIrError machine_error;
    MachineSelectProgram select_program;

    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);
    memset(&machine_error, 0, sizeof(machine_error));

    if (build_machine_ir_program_from_extension_source_text(
            "float id(float x){ return x; }\n"
            "float wrap(float x){ return x; }\n"
            "float f(float x){ return wrap(((-id(x) ? x : x)) + x); }\n"
            "int main(){ return 0; }\n",
            &machine_program,
            &machine_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-UNARY-CALL-TERNARY-PLUS-FLOAT-CALLARG-REJECT should have failed\n");
        machine_select_program_free(&select_program);
        machine_ir_program_free(&machine_program);
        return 0;
    }
    if (strstr(machine_error.message, "SEMA-EXT-035") == NULL) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-UNARY-CALL-TERNARY-PLUS-FLOAT-CALLARG-REJECT mismatch: %s\n",
            machine_error.message);
        machine_select_program_free(&select_program);
        machine_ir_program_free(&machine_program);
        return 0;
    }

    machine_select_program_free(&select_program);
    machine_ir_program_free(&machine_program);
    return 1;
}

static int test_machine_select_rejects_float_ternary_value_assignment_to_int_under_extension(void) {
    MachineIrProgram machine_program;
    MachineIrError machine_error;
    MachineSelectProgram select_program;

    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);
    memset(&machine_error, 0, sizeof(machine_error));

    if (build_machine_ir_program_from_extension_source_text(
            "float g = 1.25;\n"
            "float h = 2.5;\n"
            "int main(){ int x=0; x = (g ? h : h); return x; }\n",
            &machine_program,
            &machine_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-TERNARY-VALUE-ASSIGN-INT-REJECT should have failed\n");
        machine_select_program_free(&select_program);
        machine_ir_program_free(&machine_program);
        return 0;
    }
    if (strstr(machine_error.message, "SEMA-TYPE-006") == NULL) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-TERNARY-VALUE-ASSIGN-INT-REJECT mismatch: %s\n",
            machine_error.message);
        machine_select_program_free(&select_program);
        machine_ir_program_free(&machine_program);
        return 0;
    }

    machine_select_program_free(&select_program);
    machine_ir_program_free(&machine_program);
    return 1;
}

static int test_machine_select_rejects_unary_call_ternary_value_assignment_to_int_under_extension(void) {
    MachineIrProgram machine_program;
    MachineIrError machine_error;
    MachineSelectProgram select_program;

    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);
    memset(&machine_error, 0, sizeof(machine_error));

    if (build_machine_ir_program_from_extension_source_text(
            "float id(float x){ return x; }\n"
            "int main(){ int y=0; y = (-id(1.0) ? 1.0 : 2.0); return y; }\n",
            &machine_program,
            &machine_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-UNARY-CALL-TERNARY-ASSIGN-INT-REJECT should have failed\n");
        machine_select_program_free(&select_program);
        machine_ir_program_free(&machine_program);
        return 0;
    }
    if (strstr(machine_error.message, "SEMA-TYPE-006") == NULL) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-UNARY-CALL-TERNARY-ASSIGN-INT-REJECT mismatch: %s\n",
            machine_error.message);
        machine_select_program_free(&select_program);
        machine_ir_program_free(&machine_program);
        return 0;
    }

    machine_select_program_free(&select_program);
    machine_ir_program_free(&machine_program);
    return 1;
}

static int test_machine_select_rejects_recursive_float_call_result_in_initializer_under_extension(void) {
    MachineIrProgram machine_program;
    MachineIrError machine_error;
    MachineSelectProgram select_program;

    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);
    memset(&machine_error, 0, sizeof(machine_error));

    if (build_machine_ir_program_from_extension_source_text(
            "float add3(float a, float b, float c){ return (a + b) + c; }\n"
            "int x = add3(1.0, 2.0, 3.0);\n"
            "int main(){ return 0; }\n",
            &machine_program,
            &machine_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-RECURSIVE-CALL-INIT-INT-REJECT should have failed\n");
        machine_select_program_free(&select_program);
        machine_ir_program_free(&machine_program);
        return 0;
    }
    if (strstr(machine_error.message, "SEMA-TYPE-004") == NULL) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-RECURSIVE-CALL-INIT-INT-REJECT mismatch: %s\n",
            machine_error.message);
        machine_select_program_free(&select_program);
        machine_ir_program_free(&machine_program);
        return 0;
    }

    machine_select_program_free(&select_program);
    machine_ir_program_free(&machine_program);
    return 1;
}

static int test_machine_select_rejects_recursive_float_call_argument_to_int_under_extension(void) {
    MachineIrProgram machine_program;
    MachineIrError machine_error;
    MachineSelectProgram select_program;

    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);
    memset(&machine_error, 0, sizeof(machine_error));

    if (build_machine_ir_program_from_extension_source_text(
            "int sink(int x){ return x; }\n"
            "float add3(float a, float b, float c){ return (a + b) + c; }\n"
            "int main(){ return sink(add3(1.0, 2.0, 3.0)); }\n",
            &machine_program,
            &machine_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-RECURSIVE-CALL-CALLARG-INT-REJECT should have failed\n");
        machine_select_program_free(&select_program);
        machine_ir_program_free(&machine_program);
        return 0;
    }
    if (strstr(machine_error.message, "SEMA-TYPE-003") == NULL) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-RECURSIVE-CALL-CALLARG-INT-REJECT mismatch: %s\n",
            machine_error.message);
        machine_select_program_free(&select_program);
        machine_ir_program_free(&machine_program);
        return 0;
    }

    machine_select_program_free(&select_program);
    machine_ir_program_free(&machine_program);
    return 1;
}

static int test_machine_select_rejects_float_ternary_value_call_argument_to_int_under_extension(void) {
    MachineIrProgram machine_program;
    MachineIrError machine_error;
    MachineSelectProgram select_program;

    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);
    memset(&machine_error, 0, sizeof(machine_error));

    if (build_machine_ir_program_from_extension_source_text(
            "int sink(int x){ return x; }\n"
            "float g = 1.25;\n"
            "float h = 2.5;\n"
            "int main(){ return sink((g ? h : h)); }\n",
            &machine_program,
            &machine_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-TERNARY-VALUE-CALLARG-INT-REJECT should have failed\n");
        machine_select_program_free(&select_program);
        machine_ir_program_free(&machine_program);
        return 0;
    }
    if (strstr(machine_error.message, "SEMA-TYPE-003") == NULL) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-TERNARY-VALUE-CALLARG-INT-REJECT mismatch: %s\n",
            machine_error.message);
        machine_select_program_free(&select_program);
        machine_ir_program_free(&machine_program);
        return 0;
    }

    machine_select_program_free(&select_program);
    machine_ir_program_free(&machine_program);
    return 1;
}

static int test_machine_select_rejects_unary_call_ternary_value_call_argument_to_int_under_extension(void) {
    MachineIrProgram machine_program;
    MachineIrError machine_error;
    MachineSelectProgram select_program;

    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);
    memset(&machine_error, 0, sizeof(machine_error));

    if (build_machine_ir_program_from_extension_source_text(
            "int sink(int x){ return x; }\n"
            "float id(float x){ return x; }\n"
            "int main(){ return sink((-id(1.0) ? 1.0 : 2.0)); }\n",
            &machine_program,
            &machine_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-UNARY-CALL-TERNARY-CALLARG-INT-REJECT should have failed\n");
        machine_select_program_free(&select_program);
        machine_ir_program_free(&machine_program);
        return 0;
    }
    if (strstr(machine_error.message, "SEMA-TYPE-003") == NULL) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-UNARY-CALL-TERNARY-CALLARG-INT-REJECT mismatch: %s\n",
            machine_error.message);
        machine_select_program_free(&select_program);
        machine_ir_program_free(&machine_program);
        return 0;
    }

    machine_select_program_free(&select_program);
    machine_ir_program_free(&machine_program);
    return 1;
}

static int test_machine_select_rejects_float_ternary_value_initializer_to_int_under_extension(void) {
    MachineIrProgram machine_program;
    MachineIrError machine_error;
    MachineSelectProgram select_program;

    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);
    memset(&machine_error, 0, sizeof(machine_error));

    if (build_machine_ir_program_from_extension_source_text(
            "float g = 1.25;\n"
            "float h = 2.5;\n"
            "int x = (g ? h : h);\n"
            "int main(){ return 0; }\n",
            &machine_program,
            &machine_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-TERNARY-VALUE-INIT-INT-REJECT should have failed\n");
        machine_select_program_free(&select_program);
        machine_ir_program_free(&machine_program);
        return 0;
    }
    if (strstr(machine_error.message, "SEMA-TYPE-004") == NULL) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-TERNARY-VALUE-INIT-INT-REJECT mismatch: %s\n",
            machine_error.message);
        machine_select_program_free(&select_program);
        machine_ir_program_free(&machine_program);
        return 0;
    }

    machine_select_program_free(&select_program);
    machine_ir_program_free(&machine_program);
    return 1;
}

static int test_machine_select_rejects_unary_call_ternary_value_initializer_to_int_under_extension(void) {
    MachineIrProgram machine_program;
    MachineIrError machine_error;
    MachineSelectProgram select_program;

    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);
    memset(&machine_error, 0, sizeof(machine_error));

    if (build_machine_ir_program_from_extension_source_text(
            "float id(float x){ return x; }\n"
            "int y = (-id(1.0) ? 1.0 : 2.0);\n"
            "int main(){ return 0; }\n",
            &machine_program,
            &machine_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-UNARY-CALL-TERNARY-INIT-INT-REJECT should have failed\n");
        machine_select_program_free(&select_program);
        machine_ir_program_free(&machine_program);
        return 0;
    }
    if (strstr(machine_error.message, "SEMA-TYPE-004") == NULL) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-UNARY-CALL-TERNARY-INIT-INT-REJECT mismatch: %s\n",
            machine_error.message);
        machine_select_program_free(&select_program);
        machine_ir_program_free(&machine_program);
        return 0;
    }

    machine_select_program_free(&select_program);
    machine_ir_program_free(&machine_program);
    return 1;
}

static int test_machine_select_accepts_float_ternary_value_return_to_float_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float h = 1.25;\n"
        "float mainf(){ return g ? h : h; }\n"
        "int main(){ return 0; }\n";
    MachineIrAllocateRewriteReport machine_report;
    MachineIrError machine_error;
    MachineSelectLowerReport select_report;
    MachineSelectError select_error;
    char *actual_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_select_lower_report_init(&select_report);

    if (!build_machine_ir_report_from_default_extension_source_text(source, &machine_report, &machine_error) ||
        !machine_select_build_report_from_machine_ir_report(&machine_report, &select_report, &select_error) ||
        !machine_select_dump_lower_report_artifact(&select_report, &actual_text, &select_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-TERNARY-VALUE-RETURN-FLOAT-ACCEPT setup failed: %s\n",
            select_error.message[0] ? select_error.message : machine_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "functions=3") ||
        !strstr(actual_text, "fn.1 mainf blocks=4") ||
        !strstr(actual_text, "load_global=3") ||
        !strstr(actual_text, "function mainf params=0 locals=0 spills=0") ||
        !strstr(actual_text, "cmpbri.11 reg.0, 0, bb.1, bb.2") ||
        !strstr(actual_text, "bb.1:\n    reg.0 = load_global global.1\n    jmp bb.3") ||
        !strstr(actual_text, "bb.2:\n    reg.0 = load_global global.1\n    jmp bb.3") ||
        !strstr(actual_text, "ret reg.0")) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-TERNARY-VALUE-RETURN-FLOAT-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_select_lower_report_free(&select_report);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    return ok;
}

static int test_machine_select_accepts_float_ternary_value_assignment_to_float_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "float mainf(){ float y; y = (g ? h : h); return y; }\n"
        "int main(){ return 0; }\n";
    MachineIrAllocateRewriteReport machine_report;
    MachineIrError machine_error;
    MachineSelectLowerReport select_report;
    MachineSelectError select_error;
    char *actual_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_select_lower_report_init(&select_report);

    if (!build_machine_ir_report_from_default_extension_source_text(source, &machine_report, &machine_error) ||
        !machine_select_build_report_from_machine_ir_report(&machine_report, &select_report, &select_error) ||
        !machine_select_dump_lower_report_artifact(&select_report, &actual_text, &select_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-TERNARY-VALUE-ASSIGN-FLOAT-ACCEPT setup failed: %s\n",
            select_error.message[0] ? select_error.message : machine_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "functions=3") ||
        !strstr(actual_text, "fn.1 mainf blocks=4") ||
        !strstr(actual_text, "load_global=3") ||
        !strstr(actual_text, "function mainf params=0 locals=1 spills=0") ||
        !strstr(actual_text, "cmpbri.11 reg.0, 0, bb.1, bb.2") ||
        !strstr(actual_text, "bb.1:\n    reg.0 = load_global global.1\n    jmp bb.3") ||
        !strstr(actual_text, "bb.2:\n    reg.0 = load_global global.1\n    jmp bb.3") ||
        !strstr(actual_text, "store_local local.0, reg.0") ||
        !strstr(actual_text, "reg.0 = load_local local.0") ||
        !strstr(actual_text, "ret reg.0")) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-TERNARY-VALUE-ASSIGN-FLOAT-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_select_lower_report_free(&select_report);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    return ok;
}

static int test_machine_select_accepts_float_ternary_value_initializer_to_float_under_extension(void) {
    MachineIrAllocateRewriteReport machine_report;
    MachineIrError machine_error;
    MachineSelectLowerReport select_report;
    MachineSelectError select_error;
    char *actual_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_select_lower_report_init(&select_report);

    if (!build_machine_ir_report_from_default_extension_source_text(
            "float g = 1.25;\n"
            "float h = 2.5;\n"
            "float y = (g ? h : h);\n"
            "int main(){ return 0; }\n",
            &machine_report,
            &machine_error) ||
        !machine_select_build_report_from_machine_ir_report(&machine_report, &select_report, &select_error) ||
        !machine_select_dump_lower_report_artifact(&select_report, &actual_text, &select_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-TERNARY-VALUE-INIT-FLOAT-ACCEPT setup failed: %s\n",
            select_error.message[0] ? select_error.message : machine_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "function __global.init params=0") ||
        !strstr(actual_text, "store_global")) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-TERNARY-VALUE-INIT-FLOAT-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_select_lower_report_free(&select_report);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    return ok;
}

static int test_machine_select_accepts_float_ternary_value_call_argument_to_float_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "float id(float x){ return x; }\n"
        "float wrap(float x){ return id(x); }\n"
        "float get(){ return wrap((g ? h : h)); }\n"
        "int main(){ return 0; }\n";
    MachineIrAllocateRewriteReport machine_report;
    MachineIrError machine_error;
    MachineSelectLowerReport select_report;
    MachineSelectError select_error;
    char *actual_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_select_lower_report_init(&select_report);

    if (!build_machine_ir_report_from_default_extension_source_text(source, &machine_report, &machine_error) ||
        !machine_select_build_report_from_machine_ir_report(&machine_report, &select_report, &select_error) ||
        !machine_select_dump_lower_report_artifact(&select_report, &actual_text, &select_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-TERNARY-VALUE-CALLARG-FLOAT-ACCEPT setup failed: %s\n",
            select_error.message[0] ? select_error.message : machine_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "fn.3 get blocks=4") ||
        !strstr(actual_text, "load_global=3") ||
        !strstr(actual_text, "cmpbri.11 reg.0, 0, bb.1, bb.2") ||
        !strstr(actual_text, "bb.1:\n    reg.0 = load_global global.1\n    jmp bb.3") ||
        !strstr(actual_text, "bb.2:\n    reg.0 = load_global global.1\n    jmp bb.3") ||
        !strstr(actual_text, "bb.3:\n    ret reg.0")) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-TERNARY-VALUE-CALLARG-FLOAT-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_select_lower_report_free(&select_report);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    return ok;
}

static int test_machine_select_accepts_unary_call_float_ternary_value_call_argument_to_float_under_extension(void) {
    static const char *source =
        "float id(float x){ return x; }\n"
        "float wrap(float x){ return id(x); }\n"
        "float get(){ return wrap((-id(1.0) ? 1.0 : 2.0)); }\n"
        "int main(){ return 0; }\n";
    MachineIrAllocateRewriteReport machine_report;
    MachineIrError machine_error;
    MachineSelectLowerReport select_report;
    MachineSelectError select_error;
    char *actual_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_select_lower_report_init(&select_report);

    if (!build_machine_ir_report_from_default_extension_source_text(source, &machine_report, &machine_error) ||
        !machine_select_build_report_from_machine_ir_report(&machine_report, &select_report, &select_error) ||
        !machine_select_dump_lower_report_artifact(&select_report, &actual_text, &select_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-UNARY-CALL-TERNARY-CALLARG-FLOAT-ACCEPT setup failed: %s\n",
            select_error.message[0] ? select_error.message : machine_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!strstr(actual_text, "fn.2 get blocks=2") ||
        !strstr(actual_text, "bb.0:\n    jmp bb.1") ||
        !strstr(actual_text, "bb.1:\n    reti 1065353216")) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-UNARY-CALL-TERNARY-CALLARG-FLOAT-ACCEPT dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    machine_select_lower_report_free(&select_report);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    return ok;
}

static int test_machine_select_rejects_float_ternary_value_compare_against_int_under_extension(void) {
    MachineIrProgram machine_program;
    MachineIrError machine_error;
    MachineSelectProgram select_program;

    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);
    memset(&machine_error, 0, sizeof(machine_error));

    if (build_machine_ir_program_from_extension_source_text(
            "float g = 1.25;\n"
            "float h = 2.5;\n"
            "int main(){ return (g ? h : h) == 0; }\n",
            &machine_program,
            &machine_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-TERNARY-VALUE-COMPARE-INT-REJECT should have failed\n");
        machine_select_program_free(&select_program);
        machine_ir_program_free(&machine_program);
        return 0;
    }
    if (strstr(machine_error.message, "SEMA-TYPE-007") == NULL) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-TERNARY-VALUE-COMPARE-INT-REJECT mismatch: %s\n",
            machine_error.message);
        machine_select_program_free(&select_program);
        machine_ir_program_free(&machine_program);
        return 0;
    }

    machine_select_program_free(&select_program);
    machine_ir_program_free(&machine_program);
    return 1;
}

static int test_machine_select_rejects_unary_call_ternary_value_compare_against_int_under_extension(void) {
    MachineIrProgram machine_program;
    MachineIrError machine_error;
    MachineSelectProgram select_program;

    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);
    memset(&machine_error, 0, sizeof(machine_error));

    if (build_machine_ir_program_from_extension_source_text(
            "float id(float x){ return x; }\n"
            "int main(){ return (-id(1.0) ? 1.0 : 2.0) == 0; }\n",
            &machine_program,
            &machine_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-UNARY-CALL-TERNARY-COMPARE-INT-REJECT should have failed\n");
        machine_select_program_free(&select_program);
        machine_ir_program_free(&machine_program);
        return 0;
    }
    if (strstr(machine_error.message, "SEMA-TYPE-007") == NULL) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-UNARY-CALL-TERNARY-COMPARE-INT-REJECT mismatch: %s\n",
            machine_error.message);
        machine_select_program_free(&select_program);
        machine_ir_program_free(&machine_program);
        return 0;
    }

    machine_select_program_free(&select_program);
    machine_ir_program_free(&machine_program);
    return 1;
}

static int test_machine_select_rejects_float_ternary_value_compare_against_float_under_extension(void) {
    MachineIrProgram machine_program;
    MachineIrError machine_error;
    MachineSelectProgram select_program;

    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);
    memset(&machine_error, 0, sizeof(machine_error));

    if (build_machine_ir_program_from_extension_source_text(
            "float g = 1.25;\n"
            "float h = 2.5;\n"
            "int eq(){ return (g ? h : h) == h; }\n"
            "int main(){ return 0; }\n",
            &machine_program,
            &machine_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-TERNARY-VALUE-COMPARE-FLOAT-REJECT should have failed\n");
        machine_select_program_free(&select_program);
        machine_ir_program_free(&machine_program);
        return 0;
    }
    if (strstr(machine_error.message, "SEMA-EXT-035") == NULL) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-TERNARY-VALUE-COMPARE-FLOAT-REJECT mismatch: %s\n",
            machine_error.message);
        machine_select_program_free(&select_program);
        machine_ir_program_free(&machine_program);
        return 0;
    }

    machine_select_program_free(&select_program);
    machine_ir_program_free(&machine_program);
    return 1;
}

static int test_machine_select_rejects_unary_call_ternary_value_compare_against_float_under_extension(void) {
    MachineIrProgram machine_program;
    MachineIrError machine_error;
    MachineSelectProgram select_program;

    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);
    memset(&machine_error, 0, sizeof(machine_error));

    if (build_machine_ir_program_from_extension_source_text(
            "float id(float x){ return x; }\n"
            "int eq(float x){ return (-id(x) ? x : x) == x; }\n"
            "int main(){ return 0; }\n",
            &machine_program,
            &machine_error)) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-UNARY-CALL-TERNARY-COMPARE-FLOAT-REJECT should have failed\n");
        machine_select_program_free(&select_program);
        machine_ir_program_free(&machine_program);
        return 0;
    }
    if (strstr(machine_error.message, "SEMA-EXT-035") == NULL) {
        fprintf(stderr,
            "[machine-select] FAIL: MACHINE-SELECT-FLOAT-UNARY-CALL-TERNARY-COMPARE-FLOAT-REJECT mismatch: %s\n",
            machine_error.message);
        machine_select_program_free(&select_program);
        machine_ir_program_free(&machine_program);
        return 0;
    }

    machine_select_program_free(&select_program);
    machine_ir_program_free(&machine_program);
    return 1;
}

static int test_machine_select_conservative_no_phi_report_bridge_rejects_phi_input(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrPhi *phis = NULL;
    MachineIrPhiInput *phi_inputs = NULL;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_ir_allocate_rewrite_report_init(&report);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: conservative no-phi report setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_ir_allocate_rewrite_report_free(&report);
        machine_select_program_free(&select_program);
        return 0;
    }

    phis = (MachineIrPhi *)calloc(1u, sizeof(MachineIrPhi));
    phi_inputs = (MachineIrPhiInput *)calloc(1u, sizeof(MachineIrPhiInput));
    if (!phis || !phi_inputs) {
        fprintf(stderr, "[machine-select] FAIL: conservative no-phi report setup ran out of memory\n");
        free(phis);
        free(phi_inputs);
        machine_ir_program_free(&machine_program);
        machine_ir_allocate_rewrite_report_free(&report);
        machine_select_program_free(&select_program);
        return 0;
    }

    phis[0].result = machine_ir_operand_register(0);
    phis[0].inputs = phi_inputs;
    phis[0].input_count = 1u;
    phis[0].input_capacity = 1u;
    phi_inputs[0].predecessor_block_id = 0u;
    phi_inputs[0].value = machine_ir_operand_immediate(7);
    function->blocks[0].phis = phis;
    function->blocks[0].phi_count = 1u;
    function->blocks[0].phi_capacity = 1u;

    report.program = machine_program;

    if (machine_select_build_program_from_machine_ir_report_conservative_no_phi(
            &report, &select_program, &select_error) ||
        strstr(select_error.message, "MACHINE-SELECT-425") == NULL) {
        fprintf(stderr, "[machine-select] FAIL: conservative no-phi report bridge should reject phi input\n");
        ok = 0;
    }

    report.program.functions = NULL;
    report.program.function_count = 0u;
    report.program.function_capacity = 0u;
    report.program.register_bank.registers = NULL;
    report.program.register_bank.register_count = 0u;
    machine_ir_program_free(&machine_program);
    machine_ir_allocate_rewrite_report_free(&report);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_materializes_immediates(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: imm setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_immediate(42);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_register(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: imm lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=0 locals=0 spills=0\n"
        "  bb.0:\n"
        "    reti 42\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_distinguishes_immediate_store_families(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    MachineSelectFunctionSummary summary;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    if (!machine_ir_program_append_global(&machine_program, "g", NULL, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: immediate-store setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_LOCAL;
    instruction.as.store.slot = machine_ir_slot_local(0);
    instruction.as.store.value = machine_ir_operand_immediate(7);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: immediate-store setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = machine_ir_slot_global(0);
    instruction.as.store.value = machine_ir_operand_immediate(9);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_immediate(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: immediate-store lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=1 locals=1 spills=0\n"
        "  bb.0:\n"
        "    store_locali local.0, 7\n"
        "    store_globali global.0, 9\n"
        "    reti 0\n");

    if (!machine_select_function_compute_summary(&select_program.functions[0], &summary) ||
        summary.store_local_count != 0 ||
        summary.store_local_imm_count != 1 ||
        summary.store_global_count != 0 ||
        summary.store_global_imm_count != 1) {
        fprintf(stderr, "[machine-select] FAIL: immediate-store summary mismatch\n");
        ok = 0;
    }

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_cleanup_forwards_tail_copy_into_last_store(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    static const char *const fragments[] = {
        "function main params=1 locals=1 spills=0\n",
        "store_global global.0, ",
        "reti 0\n",
    };
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    if (!machine_ir_program_append_global(&machine_program, "g", NULL, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    machine_program.register_bank.register_count = 2;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(2, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    machine_program.register_bank.registers[1].register_id = 1;
    machine_program.register_bank.registers[1].name = dup_text("r1");
    machine_program.register_bank.registers[1].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: tail-store-copy setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: tail-store-copy setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1);
    instruction.as.mov_value = machine_ir_operand_register(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: tail-store-copy setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = machine_ir_slot_global(0);
    instruction.as.store.value = machine_ir_operand_register(1);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_immediate(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: tail-store-copy lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump_contains_all(
        &select_program,
        "tail-store-copy",
        fragments,
        sizeof(fragments) / sizeof(fragments[0]));

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_cleanup_forwards_tail_imm_into_last_alu(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 3;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(3, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    machine_program.register_bank.registers[1].register_id = 1;
    machine_program.register_bank.registers[1].name = dup_text("r1");
    machine_program.register_bank.registers[1].allocatable = 1u;
    machine_program.register_bank.registers[2].register_id = 2;
    machine_program.register_bank.registers[2].name = dup_text("r2");
    machine_program.register_bank.registers[2].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: tail-alu-imm setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: tail-alu-imm setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1);
    instruction.as.mov_value = machine_ir_operand_immediate(9);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: tail-alu-imm setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(2);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_register(1);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_register(2), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: tail-alu-imm lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=1 locals=1 spills=0\n"
        "  bb.0:\n"
        "    reg.0 = load_local local.0\n"
        "    reg.2 = alui.0 reg.0, 9\n"
        "    reg.0 = copy reg.2\n"
        "    ret reg.0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_cleanup_forwards_tail_imm_into_last_call(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: tail-call-imm setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_immediate(5);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: tail-call-imm setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 0;
    instruction.as.call.callee_name = "sink";
    instruction.as.call.args = &instruction.result;
    instruction.as.call.arg_count = 1;
    instruction.as.call.args[0] = machine_ir_operand_register(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_immediate(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: tail-call-imm lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=0 locals=0 spills=0\n"
        "  bb.0:\n"
        "    call_voidi sink(5)\n"
        "    reti 0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_cleanup_forwards_multihop_copy_chain_into_store(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    static const char *const fragments[] = {
        "function main params=1 locals=1 spills=0\n",
        "store_global global.0, ",
        "reti 0\n",
    };
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    if (!machine_ir_program_append_global(&machine_program, "g", NULL, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    machine_program.register_bank.register_count = 3;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(3, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    machine_program.register_bank.registers[1].register_id = 1;
    machine_program.register_bank.registers[1].name = dup_text("r1");
    machine_program.register_bank.registers[1].allocatable = 1u;
    machine_program.register_bank.registers[2].register_id = 2;
    machine_program.register_bank.registers[2].name = dup_text("r2");
    machine_program.register_bank.registers[2].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: multihop-store setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: multihop-store setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1);
    instruction.as.mov_value = machine_ir_operand_register(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: multihop-store setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(2);
    instruction.as.mov_value = machine_ir_operand_register(1);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: multihop-store setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = machine_ir_slot_global(0);
    instruction.as.store.value = machine_ir_operand_register(2);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_immediate(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: multihop-store lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump_contains_all(
        &select_program,
        "multihop-store",
        fragments,
        sizeof(fragments) / sizeof(fragments[0]));

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_cleanup_forwards_multihop_imm_chain_into_call(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrOperand call_args[1];
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 3;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(3, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    machine_program.register_bank.registers[1].register_id = 1;
    machine_program.register_bank.registers[1].name = dup_text("r1");
    machine_program.register_bank.registers[1].allocatable = 1u;
    machine_program.register_bank.registers[2].register_id = 2;
    machine_program.register_bank.registers[2].name = dup_text("r2");
    machine_program.register_bank.registers[2].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: multihop-call setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_immediate(5);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: multihop-call setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1);
    instruction.as.mov_value = machine_ir_operand_register(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: multihop-call setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(2);
    instruction.as.mov_value = machine_ir_operand_register(1);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: multihop-call setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 0;
    instruction.as.call.callee_name = "sink";
    call_args[0] = machine_ir_operand_register(2);
    instruction.as.call.args = call_args;
    instruction.as.call.arg_count = 1;
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_immediate(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: multihop-call lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=0 locals=0 spills=0\n"
        "  bb.0:\n"
        "    call_voidi sink(5)\n"
        "    reti 0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_cleanup_propagates_cross_block_copy_into_unique_successor(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    static const char *const fragments[] = {
        "function main params=1 locals=1 spills=0\n",
        "jmp bb.1\n",
        "store_global global.0, ",
        "reti 0\n",
    };
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    if (!machine_ir_program_append_global(&machine_program, "g", NULL, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    machine_program.register_bank.register_count = 2;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(2, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    machine_program.register_bank.registers[1].register_id = 1;
    machine_program.register_bank.registers[1].name = dup_text("r1");
    machine_program.register_bank.registers[1].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: cross-block-copy setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: cross-block-copy setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1);
    instruction.as.mov_value = machine_ir_operand_register(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[0], 1, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: cross-block-copy setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = machine_ir_slot_global(0);
    instruction.as.store.value = machine_ir_operand_register(1);
    if (!machine_ir_block_append_instruction(&function->blocks[1], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_immediate(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: cross-block-copy lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump_contains_all(
        &select_program,
        "cross-block-copy",
        fragments,
        sizeof(fragments) / sizeof(fragments[0]));

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_cleanup_propagates_cross_block_spill_alias_into_unique_successor(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: cross-block-spill setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    function->spill_slot_count = 1;

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: cross-block-spill setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_spill_slot(0);
    instruction.as.mov_value = machine_ir_operand_register(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[0], 1, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: cross-block-spill setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    if (!machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_spill_slot(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: cross-block-spill lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=1 locals=1 spills=1\n"
        "  bb.0:\n"
        "    reg.0 = load_local local.0\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    ret reg.0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_cleanup_propagates_must_agree_immediate_at_join(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    if (!machine_ir_program_append_global(&machine_program, "g", NULL, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    machine_program.register_bank.register_count = 2;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(2, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    machine_program.register_bank.registers[1].register_id = 1;
    machine_program.register_bank.registers[1].name = dup_text("r1");
    machine_program.register_bank.registers[1].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: join-agree setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 1, 2, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: join-agree setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1);
    instruction.as.mov_value = machine_ir_operand_immediate(5);
    if (!machine_ir_block_append_instruction(&function->blocks[1], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[1], 3, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: join-agree setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1);
    instruction.as.mov_value = machine_ir_operand_immediate(5);
    if (!machine_ir_block_append_instruction(&function->blocks[2], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[2], 3, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: join-agree setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = machine_ir_slot_global(0);
    instruction.as.store.value = machine_ir_operand_register(1);
    if (!machine_ir_block_append_instruction(&function->blocks[3], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[3], machine_ir_operand_immediate(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: join-agree lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=1 locals=1 spills=0\n"
        "  bb.0:\n"
        "    reg.0 = load_local local.0\n"
        "    br reg.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    store_globali global.0, 5\n"
        "    reti 0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_cleanup_propagates_must_agree_spill_immediate_at_join(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: spill-join-agree setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    function->spill_slot_count = 1;

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 1, 2, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: spill-join-agree setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_spill_slot(0);
    instruction.as.mov_value = machine_ir_operand_immediate(5);
    if (!machine_ir_block_append_instruction(&function->blocks[1], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[1], 3, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: spill-join-agree setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_spill_slot(0);
    instruction.as.mov_value = machine_ir_operand_immediate(5);
    if (!machine_ir_block_append_instruction(&function->blocks[2], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[2], 3, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: spill-join-agree setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    if (!machine_ir_block_set_return(&function->blocks[3], machine_ir_operand_spill_slot(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: spill-join-agree lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=1 locals=1 spills=1\n"
        "  bb.0:\n"
        "    reg.0 = load_local local.0\n"
        "    br reg.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    reti 5\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_cleanup_keeps_disagreeing_join_values(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    static const char *const fragments[] = {
        "function main params=1 locals=1 spills=0\n",
        "br reg.0, bb.1, bb.2\n",
        "reg.1 = imm 5\n",
        "reg.1 = imm 6\n",
        "store_global global.0, reg.1\n",
        "reti 0\n",
    };
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    if (!machine_ir_program_append_global(&machine_program, "g", NULL, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    machine_program.register_bank.register_count = 2;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(2, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    machine_program.register_bank.registers[1].register_id = 1;
    machine_program.register_bank.registers[1].name = dup_text("r1");
    machine_program.register_bank.registers[1].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: join-disagree setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 1, 2, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: join-disagree setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1);
    instruction.as.mov_value = machine_ir_operand_immediate(5);
    if (!machine_ir_block_append_instruction(&function->blocks[1], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[1], 3, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: join-disagree setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1);
    instruction.as.mov_value = machine_ir_operand_immediate(6);
    if (!machine_ir_block_append_instruction(&function->blocks[2], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[2], 3, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: join-disagree setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = machine_ir_slot_global(0);
    instruction.as.store.value = machine_ir_operand_register(1);
    if (!machine_ir_block_append_instruction(&function->blocks[3], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[3], machine_ir_operand_immediate(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: join-disagree lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump_contains_all(
        &select_program,
        "join-disagree",
        fragments,
        sizeof(fragments) / sizeof(fragments[0]));

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_cleanup_keeps_disagreeing_spill_join_values(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: spill-join-disagree setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    function->spill_slot_count = 1;

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 1, 2, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: spill-join-disagree setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_spill_slot(0);
    instruction.as.mov_value = machine_ir_operand_immediate(5);
    if (!machine_ir_block_append_instruction(&function->blocks[1], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[1], 3, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: spill-join-disagree setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_spill_slot(0);
    instruction.as.mov_value = machine_ir_operand_immediate(6);
    if (!machine_ir_block_append_instruction(&function->blocks[2], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[2], 3, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: spill-join-disagree setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    if (!machine_ir_block_set_return(&function->blocks[3], machine_ir_operand_spill_slot(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: spill-join-disagree lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=1 locals=1 spills=1\n"
        "  bb.0:\n"
        "    reg.0 = load_local local.0\n"
        "    br reg.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    spill.0 = imm 5\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    spill.0 = imm 6\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    retspill spill.0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_cleanup_does_not_propagate_caller_clobbered_copy_through_call(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 2;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(2, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    machine_program.register_bank.registers[0].caller_clobbered = 1u;
    machine_program.register_bank.registers[1].register_id = 1;
    machine_program.register_bank.registers[1].name = dup_text("r1");
    machine_program.register_bank.registers[1].allocatable = 1u;
    machine_program.register_bank.registers[1].caller_clobbered = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: caller-clobber setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1);
    instruction.as.mov_value = machine_ir_operand_immediate(41);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: caller-clobber setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = "callee";
    instruction.as.call.args = NULL;
    instruction.as.call.arg_count = 0;
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: caller-clobber setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(1);
    instruction.as.binary.rhs = machine_ir_operand_immediate(1);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_register(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: caller-clobber lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=0 locals=0 spills=0\n"
        "  bb.0:\n"
        "    reg.1 = imm 41\n"
        "    reg.0 = call callee()\n"
        "    reg.0 = alui.0 reg.1, 1\n"
        "    ret reg.0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_cleanup_keeps_callee_preserved_copy_through_call(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 2;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(2, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    machine_program.register_bank.registers[0].caller_clobbered = 1u;
    machine_program.register_bank.registers[1].register_id = 1;
    machine_program.register_bank.registers[1].name = dup_text("r1");
    machine_program.register_bank.registers[1].allocatable = 1u;
    machine_program.register_bank.registers[1].caller_clobbered = 0u;
    machine_program.register_bank.registers[1].callee_preserved = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: callee-preserved setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1);
    instruction.as.mov_value = machine_ir_operand_immediate(41);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: callee-preserved setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = "callee";
    instruction.as.call.args = NULL;
    instruction.as.call.arg_count = 0;
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: callee-preserved setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(1);
    instruction.as.binary.rhs = machine_ir_operand_immediate(1);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_register(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: callee-preserved lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=0 locals=0 spills=0\n"
        "  bb.0:\n"
        "    reg.0 = call callee()\n"
        "    reti 42\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_cleanup_keeps_spill_alias_through_call(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    machine_program.register_bank.registers[0].caller_clobbered = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: spill-alias setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    function->spill_slot_count = 1;

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_spill_slot(0);
    instruction.as.mov_value = machine_ir_operand_immediate(41);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: spill-alias setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = "callee";
    instruction.as.call.args = NULL;
    instruction.as.call.arg_count = 0;
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: spill-alias setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_spill_slot(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(1);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_register(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: spill-alias lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=0 locals=0 spills=1\n"
        "  bb.0:\n"
        "    reg.0 = call callee()\n"
        "    reti 42\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_builds_from_machine_ir_report(void) {
    ValueSsaProgram ssa_program;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t value_id;
    ValueSsaError ssa_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&ssa_error, 0, sizeof(ssa_error));
    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    value_ssa_program_init(&ssa_program);
    machine_ir_allocate_rewrite_report_init(&report);
    machine_select_program_init(&select_program);

    if (!value_ssa_program_append_function(&ssa_program, "main", 1, &function, &ssa_error) ||
        !function ||
        !value_ssa_function_append_block(function, NULL, &block, &ssa_error)) {
        fprintf(stderr, "[machine-select] FAIL: report bridge setup failed: %s\n", ssa_error.message);
        value_ssa_program_free(&ssa_program);
        machine_ir_allocate_rewrite_report_free(&report);
        machine_select_program_free(&select_program);
        return 0;
    }
    value_id = value_ssa_function_allocate_value(function);
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value_id);
    instruction.as.mov_value = value_ssa_value_immediate(5);
    if (!value_ssa_block_append_instruction(block, &instruction, &ssa_error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(value_id), &ssa_error) ||
        !machine_ir_build_allocate_and_rewrite_program_single_block_spills_canonicalized_flat_report(
            &ssa_program, 1, 1, &report, &machine_error) ||
        !machine_select_build_program_from_machine_ir_report(&report, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: report bridge setup failed\n");
        value_ssa_program_free(&ssa_program);
        machine_ir_allocate_rewrite_report_free(&report);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=0 locals=0 spills=0\n"
        "  bb.0:\n"
        "    reti 5\n");

    value_ssa_program_free(&ssa_program);
    machine_ir_allocate_rewrite_report_free(&report);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_lowers_compare_branch_terminator(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 2;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(2, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    machine_program.register_bank.registers[1].register_id = 1;
    machine_program.register_bank.registers[1].name = dup_text("r1");
    machine_program.register_bank.registers[1].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_local(function, "b", 1, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: cmp-branch setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: cmp-branch setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1);
    instruction.as.load_slot = machine_ir_slot_local(1);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: cmp-branch setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_EQ;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_register(1);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 1, 2, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_immediate(1), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(0), &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: cmp-branch setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    if (!machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: cmp-branch lowering failed: %s\n", select_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=2 locals=2 spills=0\n"
        "  bb.0:\n"
        "    reg.0 = load_local local.0\n"
        "    reg.1 = load_local local.1\n"
        "    cmpbr.10 reg.0, reg.1, bb.1, bb.2\n"
        "  bb.1:\n"
        "    reti 1\n"
        "  bb.2:\n"
        "    reti 0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_lowers_compare_branch_immediate_terminator(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: cmpbri setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: cmpbri setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_EQ;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(4);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 1, 2, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_immediate(1), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(0), &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: cmpbri setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    if (!machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: cmpbri lowering failed: %s\n", select_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=1 locals=1 spills=0\n"
        "  bb.0:\n"
        "    reg.0 = load_local local.0\n"
        "    cmpbri.10 reg.0, 4, bb.1, bb.2\n"
        "  bb.1:\n"
        "    reti 1\n"
        "  bb.2:\n"
        "    reti 0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_normalizes_compare_branch_lhs_immediate(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: normalized-cmpbri setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: normalized-cmpbri setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_LT;
    instruction.as.binary.lhs = machine_ir_operand_immediate(4);
    instruction.as.binary.rhs = machine_ir_operand_register(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 1, 2, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_immediate(1), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: normalized-cmpbri lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=1 locals=1 spills=0\n"
        "  bb.0:\n"
        "    reg.0 = load_local local.0\n"
        "    cmpbri.14 reg.0, 4, bb.1, bb.2\n"
        "  bb.1:\n"
        "    reti 1\n"
        "  bb.2:\n"
        "    reti 0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_terminator_query_surface(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectLowerReport report;
    MachineSelectError select_error;
    const MachineSelectFunction *function_view = NULL;
    const MachineSelectFunction *report_function_view = NULL;
    const MachineSelectBasicBlock *block_view = NULL;
    const MachineSelectBasicBlock *report_block_view = NULL;
    const MachineSelectBlockShapeSummary *block_summary = NULL;
    const MachineSelectBlockShapeSummary *artifact_block_summary = NULL;
    MachineSelectBlockShapeSummary raw_block_summary;
    MachineSelectTerminatorSummary terminator_summary;
    MachineSelectTerminatorSummary artifact_terminator_summary;
    size_t function_index = 0;
    size_t block_index = 0;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);
    machine_select_lower_report_init(&report);

    machine_program.register_bank.register_count = 2;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(2, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    machine_program.register_bank.registers[1].register_id = 1;
    machine_program.register_bank.registers[1].name = dup_text("r1");
    machine_program.register_bank.registers[1].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_local(function, "b", 1, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: terminator-query setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        machine_select_lower_report_free(&report);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: terminator-query setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        machine_select_lower_report_free(&report);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1);
    instruction.as.load_slot = machine_ir_slot_local(1);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: terminator-query setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        machine_select_lower_report_free(&report);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_EQ;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_register(1);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 1, 2, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_immediate(1), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error) ||
        !machine_select_build_report_from_program(&select_program, &report, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: terminator-query lowering/build failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        machine_select_lower_report_free(&report);
        return 0;
    }

    if (!machine_select_program_get_function_by_name(&select_program, "main", NULL, &function_view) ||
        !function_view ||
        !machine_select_program_get_block_by_function_name_and_block_id(
            &select_program, "main", 0, &function_index, &block_index, &function_view, &block_view) ||
        function_index != 0 ||
        block_index != 0 ||
        !function_view ||
        !block_view ||
        !machine_select_program_get_block_artifact_by_function_name_and_block_id(
            &select_program,
            "main",
            0,
            &function_index,
            &block_index,
            &function_view,
            &block_view,
            &raw_block_summary,
            &artifact_terminator_summary) ||
        function_index != 0 ||
        block_index != 0 ||
        raw_block_summary.block_id != 0 ||
        raw_block_summary.op_count != 2 ||
        !raw_block_summary.has_compare_branch_terminator ||
        artifact_terminator_summary.kind != MACHINE_SELECT_TERM_COMPARE_BRANCH ||
        artifact_terminator_summary.primary_target_block_id != 1 ||
        artifact_terminator_summary.secondary_target_block_id != 2 ||
        !machine_select_program_get_block_summary_by_function_name_and_block_id(
            &select_program, "main", 0, &function_index, &block_index, &raw_block_summary) ||
        function_index != 0 ||
        block_index != 0 ||
        raw_block_summary.block_id != 0 ||
        raw_block_summary.op_count != 2 ||
        !raw_block_summary.has_compare_branch_terminator ||
        !machine_select_program_get_block_terminator_summary_by_function_name_and_block_id(
            &select_program, "main", 0, &function_index, &block_index, &terminator_summary) ||
        function_index != 0 ||
        block_index != 0 ||
        !machine_select_function_get_block(function_view, 0, &block_view) ||
        !block_view ||
        terminator_summary.kind != MACHINE_SELECT_TERM_COMPARE_BRANCH ||
        !terminator_summary.has_primary_target ||
        terminator_summary.primary_target_block_id != 1 ||
        !terminator_summary.has_secondary_target ||
        terminator_summary.secondary_target_block_id != 2 ||
        !terminator_summary.has_compare ||
        terminator_summary.compare_op != MACHINE_IR_BINARY_EQ ||
        terminator_summary.compare_lhs.kind != MACHINE_SELECT_OPERAND_REGISTER ||
        terminator_summary.compare_lhs.machine_register_id != 0 ||
        terminator_summary.compare_rhs.kind != MACHINE_SELECT_OPERAND_REGISTER ||
        terminator_summary.compare_rhs.machine_register_id != 1) {
        fprintf(stderr, "[machine-select] FAIL: raw terminator summary mismatch\n");
        ok = 0;
    }

    if (!machine_select_lower_report_get_block_summary_artifact_by_function_name_and_block_id(
            &report, "main", 0, &function_index, &block_index, &block_summary) ||
        function_index != 0 ||
        block_index != 0 ||
        !block_summary ||
        block_summary->block_id != 0 ||
        !machine_select_lower_report_get_block_artifact_by_function_name_and_block_id(
            &report,
            "main",
            0,
            &function_index,
            &block_index,
            &report_function_view,
            &report_block_view,
            &artifact_block_summary,
            &artifact_terminator_summary) ||
        function_index != 0 ||
        block_index != 0 ||
        !report_function_view ||
        !report_block_view ||
        !artifact_block_summary ||
        artifact_block_summary->block_id != 0 ||
        artifact_terminator_summary.kind != MACHINE_SELECT_TERM_COMPARE_BRANCH ||
        artifact_terminator_summary.primary_target_block_id != 1 ||
        artifact_terminator_summary.secondary_target_block_id != 2 ||
        !machine_select_lower_report_get_block_by_function_name_and_block_id(
            &report, "main", 0, &function_index, &block_index, &report_function_view, &report_block_view) ||
        function_index != 0 ||
        block_index != 0 ||
        !report_function_view ||
        !report_block_view ||
        report_block_view->id != 0 ||
        !block_summary->has_compare_branch_terminator ||
        !machine_select_lower_report_get_block_terminator_summary(&report, 0, 0, &terminator_summary) ||
        terminator_summary.kind != MACHINE_SELECT_TERM_COMPARE_BRANCH ||
        terminator_summary.primary_target_block_id != 1 ||
        terminator_summary.secondary_target_block_id != 2 ||
        !machine_select_lower_report_get_block_terminator_summary_by_id(
            &report, 0, 1, &block_index, &terminator_summary) ||
        block_index != 1 ||
        terminator_summary.kind != MACHINE_SELECT_TERM_RETURN_IMM ||
        !terminator_summary.has_return_value ||
        terminator_summary.return_value.kind != MACHINE_SELECT_OPERAND_IMMEDIATE ||
        terminator_summary.return_value.immediate != 1 ||
        !machine_select_lower_report_get_block_terminator_summary_by_function_name_and_block_id(
            &report, "main", 2, &function_index, &block_index, &terminator_summary) ||
        function_index != 0 ||
        block_index != 2 ||
        terminator_summary.kind != MACHINE_SELECT_TERM_RETURN_IMM ||
        terminator_summary.return_value.immediate != 0) {
        fprintf(stderr, "[machine-select] FAIL: report terminator query mismatch\n");
        ok = 0;
    }

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    machine_select_lower_report_free(&report);
    return ok;
}

static int test_machine_select_folds_constant_branches_to_jump(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    MachineSelectFunctionSummary summary;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_immediate(0), 1, 2, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_immediate(1), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: const-branch lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=0 locals=0 spills=0\n"
        "  bb.0:\n"
        "    jmp bb.2\n"
        "  bb.1:\n"
        "    reti 1\n"
        "  bb.2:\n"
        "    reti 0\n");

    if (!machine_select_function_compute_summary(&select_program.functions[0], &summary) ||
        summary.plain_branch_count != 0 ||
        summary.compare_branch_count != 0 ||
        summary.compare_branch_imm_count != 0) {
        fprintf(stderr, "[machine-select] FAIL: const-branch summary mismatch\n");
        ok = 0;
    }

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_folds_normalized_constant_branches_to_jump(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_immediate(4294967296LL), 1, 2, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_immediate(1), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: normalized-const-branch lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=0 locals=0 spills=0\n"
        "  bb.0:\n"
        "    jmp bb.2\n"
        "  bb.1:\n"
        "    reti 1\n"
        "  bb.2:\n"
        "    reti 0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_folds_constant_compare_branches_to_jump(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    MachineSelectFunctionSummary summary;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: constant-cmp-branch setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_LT;
    instruction.as.binary.lhs = machine_ir_operand_immediate(1);
    instruction.as.binary.rhs = machine_ir_operand_immediate(2);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 1, 2, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_immediate(7), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(9), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: constant-cmp-branch lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=0 locals=0 spills=0\n"
        "  bb.0:\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    reti 7\n"
        "  bb.2:\n"
        "    reti 9\n");

    if (!machine_select_function_compute_summary(&select_program.functions[0], &summary) ||
        summary.materialize_imm_count != 0 ||
        summary.compare_branch_count != 0 ||
        summary.compare_branch_imm_count != 0 ||
        summary.plain_branch_count != 0) {
        fprintf(stderr, "[machine-select] FAIL: constant-cmp-branch summary mismatch\n");
        ok = 0;
    }

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_folds_materialized_boolean_branches_to_jump(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    MachineSelectFunctionSummary summary;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: materialized-branch setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_immediate(1);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 1, 2, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_immediate(11), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(22), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: materialized-branch lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=0 locals=0 spills=0\n"
        "  bb.0:\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    reti 11\n"
        "  bb.2:\n"
        "    reti 22\n");

    if (!machine_select_function_compute_summary(&select_program.functions[0], &summary) ||
        summary.materialize_imm_count != 0 ||
        summary.compare_branch_count != 0 ||
        summary.compare_branch_imm_count != 0 ||
        summary.plain_branch_count != 0) {
        fprintf(stderr, "[machine-select] FAIL: materialized-branch summary mismatch\n");
        ok = 0;
    }

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_folds_normalized_materialized_boolean_branches_to_jump(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: normalized-materialized-branch setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_immediate(4294967296LL);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 1, 2, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_immediate(11), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(22), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: normalized-materialized-branch lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=0 locals=0 spills=0\n"
        "  bb.0:\n"
        "    jmp bb.2\n"
        "  bb.1:\n"
        "    reti 11\n"
        "  bb.2:\n"
        "    reti 22\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_keeps_compare_result_when_live_out_of_block(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 2;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(2, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    machine_program.register_bank.registers[1].register_id = 1;
    machine_program.register_bank.registers[1].name = dup_text("r1");
    machine_program.register_bank.registers[1].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: live-out-cmp setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: live-out-cmp setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1);
    instruction.as.binary.op = MACHINE_IR_BINARY_LT;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(4);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(1), 1, 2, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[1], machine_ir_operand_register(1), 3, 2, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(0), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[3], machine_ir_operand_immediate(1), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: live-out-cmp lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=1 locals=1 spills=0\n"
        "  bb.0:\n"
        "    reg.0 = load_local local.0\n"
        "    reg.1 = cmpi.12 reg.0, 4\n"
        "    br reg.1, bb.1, bb.2\n"
        "  bb.1:\n"
        "    br reg.1, bb.3, bb.2\n"
        "  bb.2:\n"
        "    reti 0\n"
        "  bb.3:\n"
        "    reti 1\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_keeps_compare_result_when_live_out_via_store_indirect(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 3;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(3, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    machine_program.register_bank.registers[1].register_id = 1;
    machine_program.register_bank.registers[1].name = dup_text("r1");
    machine_program.register_bank.registers[1].allocatable = 1u;
    machine_program.register_bank.registers[2].register_id = 2;
    machine_program.register_bank.registers[2].name = dup_text("r2");
    machine_program.register_bank.registers[2].allocatable = 1u;

    if (!machine_ir_program_append_global(&machine_program, "g", NULL, &machine_error) ||
        !machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: live-out-indirect setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_ADDR_GLOBAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(2);
    instruction.as.addr_slot = machine_ir_slot_global(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: live-out-indirect addr setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: live-out-indirect load setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1);
    instruction.as.binary.op = MACHINE_IR_BINARY_LT;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(4);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(1), 1, 2, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: live-out-indirect branch setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_INDIRECT;
    instruction.as.store_indirect.addr = machine_ir_operand_register(2);
    instruction.as.store_indirect.value = machine_ir_operand_register(1);
    if (!machine_ir_block_append_instruction(&function->blocks[1], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_immediate(1), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: live-out-indirect lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=1 locals=1 spills=0\n"
        "  bb.0:\n"
        "    reg.2 = addr_global global.0\n"
        "    reg.0 = load_local local.0\n"
        "    reg.1 = cmpi.12 reg.0, 4\n"
        "    br reg.1, bb.1, bb.2\n"
        "  bb.1:\n"
        "    store_indirect reg.2, reg.1\n"
        "    reti 1\n"
        "  bb.2:\n"
        "    reti 0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_cleanup_fuses_branch_with_adjacent_compare_after_selected_copy_forward(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 2;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(2, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    machine_program.register_bank.registers[1].register_id = 1;
    machine_program.register_bank.registers[1].name = dup_text("r1");
    machine_program.register_bank.registers[1].allocatable = 1u;
    if (!machine_program.register_bank.registers[0].name ||
        !machine_program.register_bank.registers[1].name) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: selected-adjacent-cmp setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: selected-adjacent-cmp setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1);
    instruction.as.mov_value = machine_ir_operand_register(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: selected-adjacent-cmp setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_LT;
    instruction.as.binary.lhs = machine_ir_operand_register(1);
    instruction.as.binary.rhs = machine_ir_operand_immediate(4);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 1, 2, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_immediate(1), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: selected-adjacent-cmp lowering failed: %s\n", select_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=1 locals=1 spills=0\n"
        "  bb.0:\n"
        "    reg.0 = load_local local.0\n"
        "    cmpbri.12 reg.0, 4, bb.1, bb.2\n"
        "  bb.1:\n"
        "    reti 1\n"
        "  bb.2:\n"
        "    reti 0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_cleanup_removes_dead_load_before_folded_branch(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    MachineSelectFunctionSummary summary;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 2;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(2, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    machine_program.register_bank.registers[1].register_id = 1;
    machine_program.register_bank.registers[1].name = dup_text("r1");
    machine_program.register_bank.registers[1].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: dead-load-before-branch setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: dead-load-before-branch setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1);
    instruction.as.mov_value = machine_ir_operand_immediate(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(1), 1, 2, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_immediate(1), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(2), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: dead-load-before-branch lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=1 locals=1 spills=0\n"
        "  bb.0:\n"
        "    jmp bb.2\n"
        "  bb.1:\n"
        "    reti 1\n"
        "  bb.2:\n"
        "    reti 2\n");

    if (!machine_select_function_compute_summary(&select_program.functions[0], &summary) ||
        summary.op_count != 0 ||
        summary.load_local_count != 0 ||
        summary.materialize_imm_count != 0 ||
        summary.plain_branch_count != 0) {
        fprintf(stderr, "[machine-select] FAIL: dead-load-before-branch summary mismatch\n");
        ok = 0;
    }

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_cleanup_propagates_multiconsumer_immediate(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrOperand call_args[1];
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    if (!machine_ir_program_append_global(&machine_program, "g", NULL, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: multiconsumer-imm setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_immediate(5);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: multiconsumer-imm setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 0;
    instruction.as.call.callee_name = "sink";
    call_args[0] = machine_ir_operand_register(0);
    instruction.as.call.args = call_args;
    instruction.as.call.arg_count = 1;
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: multiconsumer-imm setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = machine_ir_slot_global(0);
    instruction.as.store.value = machine_ir_operand_register(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_immediate(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: multiconsumer-imm lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=0 locals=0 spills=0\n"
        "  bb.0:\n"
        "    call_voidi sink(5)\n"
        "    store_globali global.0, 5\n"
        "    reti 0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_cleanup_normalizes_folded_branch_immediate(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 2;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(2, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    machine_program.register_bank.registers[1].register_id = 1;
    machine_program.register_bank.registers[1].name = dup_text("r1");
    machine_program.register_bank.registers[1].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: normalized-folded-branch cleanup setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_immediate(4294967296LL);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: normalized-folded-branch cleanup setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1);
    instruction.as.mov_value = machine_ir_operand_register(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(1), 1, 2, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_immediate(1), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(2), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: normalized-folded-branch cleanup lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=0 locals=0 spills=0\n"
        "  bb.0:\n"
        "    jmp bb.2\n"
        "  bb.1:\n"
        "    reti 1\n"
        "  bb.2:\n"
        "    reti 2\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_cleanup_removes_dead_cross_block_register_def_redefined_in_successor(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: dead-cross-block-reg setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_immediate(5);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[0], 1, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: dead-cross-block-reg setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_immediate(7);
    if (!machine_ir_block_append_instruction(&function->blocks[1], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_register(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: dead-cross-block-reg lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=0 locals=0 spills=0\n"
        "  bb.0:\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    reti 7\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_cleanup_removes_dead_cross_block_spill_def_redefined_in_successor(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: dead-cross-block-spill setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    function->spill_slot_count = 1;

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_spill_slot(0);
    instruction.as.mov_value = machine_ir_operand_immediate(5);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[0], 1, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: dead-cross-block-spill setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_spill_slot(0);
    instruction.as.mov_value = machine_ir_operand_immediate(7);
    if (!machine_ir_block_append_instruction(&function->blocks[1], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_spill_slot(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: dead-cross-block-spill lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=0 locals=0 spills=1\n"
        "  bb.0:\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    reti 7\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_cleanup_keeps_cross_block_register_def_when_only_one_successor_path_redefines(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 2;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(2, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    machine_program.register_bank.registers[1].register_id = 1;
    machine_program.register_bank.registers[1].name = dup_text("r1");
    machine_program.register_bank.registers[1].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: partial-redef-reg setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_immediate(5);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[0], 1, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: partial-redef-reg setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[1], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[1], machine_ir_operand_register(1), 2, 3, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: partial-redef-reg setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_immediate(7);
    if (!machine_ir_block_append_instruction(&function->blocks[2], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[2], 4, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[3], 4, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[4], machine_ir_operand_register(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: partial-redef-reg lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=1 locals=1 spills=0\n"
        "  bb.0:\n"
        "    reg.0 = imm 5\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    reg.1 = load_local local.0\n"
        "    br reg.1, bb.2, bb.3\n"
        "  bb.2:\n"
        "    reg.0 = imm 7\n"
        "    jmp bb.4\n"
        "  bb.3:\n"
        "    jmp bb.4\n"
        "  bb.4:\n"
        "    ret reg.0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_cleanup_keeps_cross_block_spill_def_when_only_one_successor_path_redefines(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: partial-redef-spill setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    function->spill_slot_count = 1;

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_spill_slot(0);
    instruction.as.mov_value = machine_ir_operand_immediate(5);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[0], 1, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: partial-redef-spill setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[1], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[1], machine_ir_operand_register(0), 2, 3, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: partial-redef-spill setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_spill_slot(0);
    instruction.as.mov_value = machine_ir_operand_immediate(7);
    if (!machine_ir_block_append_instruction(&function->blocks[2], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[2], 4, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[3], 4, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[4], machine_ir_operand_spill_slot(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: partial-redef-spill lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=1 locals=1 spills=1\n"
        "  bb.0:\n"
        "    spill.0 = imm 5\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    reg.0 = load_local local.0\n"
        "    br reg.0, bb.2, bb.3\n"
        "  bb.2:\n"
        "    spill.0 = imm 7\n"
        "    jmp bb.4\n"
        "  bb.3:\n"
        "    jmp bb.4\n"
        "  bb.4:\n"
        "    retspill spill.0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_cleanup_keeps_register_def_live_across_partial_call_clobber_path(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 2;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(2, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    machine_program.register_bank.registers[0].caller_clobbered = 1u;
    machine_program.register_bank.registers[1].register_id = 1;
    machine_program.register_bank.registers[1].name = dup_text("r1");
    machine_program.register_bank.registers[1].allocatable = 1u;
    machine_program.register_bank.registers[1].caller_clobbered = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: partial-call-clobber-reg setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1);
    instruction.as.mov_value = machine_ir_operand_immediate(41);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[0], 1, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: partial-call-clobber-reg setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[1], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[1], machine_ir_operand_register(0), 2, 3, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: partial-call-clobber-reg setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = "callee";
    instruction.as.call.args = NULL;
    instruction.as.call.arg_count = 0;
    if (!machine_ir_block_append_instruction(&function->blocks[2], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[2], 4, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[3], 4, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: partial-call-clobber-reg setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(1);
    instruction.as.binary.rhs = machine_ir_operand_immediate(1);
    if (!machine_ir_block_append_instruction(&function->blocks[4], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[4], machine_ir_operand_register(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: partial-call-clobber-reg lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=1 locals=1 spills=0\n"
        "  bb.0:\n"
        "    reg.1 = imm 41\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    reg.0 = load_local local.0\n"
        "    br reg.0, bb.2, bb.3\n"
        "  bb.2:\n"
        "    reg.0 = call callee()\n"
        "    jmp bb.4\n"
        "  bb.3:\n"
        "    jmp bb.4\n"
        "  bb.4:\n"
        "    reg.0 = alui.0 reg.1, 1\n"
        "    ret reg.0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_cleanup_keeps_spill_alias_through_partial_call_path(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    machine_program.register_bank.registers[0].caller_clobbered = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: partial-call-spill setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    function->spill_slot_count = 1;

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_spill_slot(0);
    instruction.as.mov_value = machine_ir_operand_immediate(41);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[0], 1, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: partial-call-spill setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[1], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[1], machine_ir_operand_register(0), 2, 3, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: partial-call-spill setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = "callee";
    instruction.as.call.args = NULL;
    instruction.as.call.arg_count = 0;
    if (!machine_ir_block_append_instruction(&function->blocks[2], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[2], 4, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[3], 4, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[4], machine_ir_operand_spill_slot(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: partial-call-spill lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=1 locals=1 spills=1\n"
        "  bb.0:\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    reg.0 = load_local local.0\n"
        "    br reg.0, bb.2, bb.3\n"
        "  bb.2:\n"
        "    reg.0 = call callee()\n"
        "    jmp bb.4\n"
        "  bb.3:\n"
        "    jmp bb.4\n"
        "  bb.4:\n"
        "    reti 41\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_cleanup_removes_register_def_when_successor_paths_call_or_redefine_before_use(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 2;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(2, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    machine_program.register_bank.registers[0].caller_clobbered = 1u;
    machine_program.register_bank.registers[1].register_id = 1;
    machine_program.register_bank.registers[1].name = dup_text("r1");
    machine_program.register_bank.registers[1].allocatable = 1u;
    machine_program.register_bank.registers[1].caller_clobbered = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: mixed-kill-reg setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1);
    instruction.as.mov_value = machine_ir_operand_immediate(41);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[0], 1, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: mixed-kill-reg setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[1], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[1], machine_ir_operand_register(0), 2, 3, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: mixed-kill-reg setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = "callee";
    instruction.as.call.args = NULL;
    instruction.as.call.arg_count = 0;
    if (!machine_ir_block_append_instruction(&function->blocks[2], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[2], 4, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: mixed-kill-reg setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1);
    instruction.as.mov_value = machine_ir_operand_immediate(7);
    if (!machine_ir_block_append_instruction(&function->blocks[3], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[3], 4, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: mixed-kill-reg setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(1);
    instruction.as.binary.rhs = machine_ir_operand_immediate(1);
    if (!machine_ir_block_append_instruction(&function->blocks[4], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[4], machine_ir_operand_register(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: mixed-kill-reg lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=1 locals=1 spills=0\n"
        "  bb.0:\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    reg.0 = load_local local.0\n"
        "    br reg.0, bb.2, bb.3\n"
        "  bb.2:\n"
        "    reg.0 = call callee()\n"
        "    jmp bb.4\n"
        "  bb.3:\n"
        "    reg.1 = imm 7\n"
        "    jmp bb.4\n"
        "  bb.4:\n"
        "    reg.0 = alui.0 reg.1, 1\n"
        "    ret reg.0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_cleanup_keeps_spill_def_live_when_successor_paths_call_or_redefine_before_use(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    machine_program.register_bank.registers[0].caller_clobbered = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: mixed-kill-spill setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    function->spill_slot_count = 1;

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_spill_slot(0);
    instruction.as.mov_value = machine_ir_operand_immediate(41);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[0], 1, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: mixed-kill-spill setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[1], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[1], machine_ir_operand_register(0), 2, 3, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: mixed-kill-spill setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = "callee";
    instruction.as.call.args = NULL;
    instruction.as.call.arg_count = 0;
    if (!machine_ir_block_append_instruction(&function->blocks[2], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[2], 4, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: mixed-kill-spill setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_spill_slot(0);
    instruction.as.mov_value = machine_ir_operand_immediate(7);
    if (!machine_ir_block_append_instruction(&function->blocks[3], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[3], 4, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[4], machine_ir_operand_spill_slot(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: mixed-kill-spill lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=1 locals=1 spills=1\n"
        "  bb.0:\n"
        "    spill.0 = imm 41\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    reg.0 = load_local local.0\n"
        "    br reg.0, bb.2, bb.3\n"
        "  bb.2:\n"
        "    reg.0 = call callee()\n"
        "    jmp bb.4\n"
        "  bb.3:\n"
        "    spill.0 = imm 7\n"
        "    jmp bb.4\n"
        "  bb.4:\n"
        "    retspill spill.0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_cleanup_keeps_spill_def_when_register_def_is_dead_in_same_predecessor(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    machine_program.register_bank.registers[0].caller_clobbered = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: dead-reg-live-spill setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    function->spill_slot_count = 1;

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_immediate(5);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: dead-reg-live-spill setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_spill_slot(0);
    instruction.as.mov_value = machine_ir_operand_immediate(41);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[0], 1, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: dead-reg-live-spill setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[1], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[1], machine_ir_operand_register(0), 2, 3, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: dead-reg-live-spill setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = "callee";
    instruction.as.call.args = NULL;
    instruction.as.call.arg_count = 0;
    if (!machine_ir_block_append_instruction(&function->blocks[2], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[2], 4, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[3], 4, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: dead-reg-live-spill setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    if (!machine_ir_block_set_return(&function->blocks[4], machine_ir_operand_spill_slot(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: dead-reg-live-spill lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=1 locals=1 spills=1\n"
        "  bb.0:\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    reg.0 = load_local local.0\n"
        "    br reg.0, bb.2, bb.3\n"
        "  bb.2:\n"
        "    reg.0 = call callee()\n"
        "    jmp bb.4\n"
        "  bb.3:\n"
        "    jmp bb.4\n"
        "  bb.4:\n"
        "    reti 41\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_cleanup_keeps_spill_live_while_dropping_dead_register_in_same_predecessor(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    machine_program.register_bank.registers[0].caller_clobbered = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: dead-reg-live-spill-mixed setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    function->spill_slot_count = 1;

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_immediate(5);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: dead-reg-live-spill-mixed setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_spill_slot(0);
    instruction.as.mov_value = machine_ir_operand_immediate(41);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[0], 1, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: dead-reg-live-spill-mixed setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[1], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[1], machine_ir_operand_register(0), 2, 3, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: dead-reg-live-spill-mixed setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = "callee";
    instruction.as.call.args = NULL;
    instruction.as.call.arg_count = 0;
    if (!machine_ir_block_append_instruction(&function->blocks[2], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[2], 4, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: dead-reg-live-spill-mixed setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_spill_slot(0);
    instruction.as.mov_value = machine_ir_operand_immediate(7);
    if (!machine_ir_block_append_instruction(&function->blocks[3], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[3], 4, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[4], machine_ir_operand_spill_slot(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: dead-reg-live-spill-mixed lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=1 locals=1 spills=1\n"
        "  bb.0:\n"
        "    spill.0 = imm 41\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    reg.0 = load_local local.0\n"
        "    br reg.0, bb.2, bb.3\n"
        "  bb.2:\n"
        "    reg.0 = call callee()\n"
        "    jmp bb.4\n"
        "  bb.3:\n"
        "    spill.0 = imm 7\n"
        "    jmp bb.4\n"
        "  bb.4:\n"
        "    retspill spill.0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_lowers_value_call_with_immediate_arg_family(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrOperand call_args[1];
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    MachineSelectFunctionSummary summary;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: call-imm-family setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = "f";
    call_args[0] = machine_ir_operand_immediate(7);
    instruction.as.call.args = call_args;
    instruction.as.call.arg_count = 1;
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_register(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: call-imm-family lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=0 locals=0 spills=0\n"
        "  bb.0:\n"
        "    reg.0 = calli f(7)\n"
        "    ret reg.0\n");

    if (!machine_select_function_compute_summary(&select_program.functions[0], &summary) ||
        summary.call_count != 0 ||
        summary.call_imm_count != 1 ||
        summary.call_void_count != 0 ||
        summary.call_void_imm_count != 0 ||
        summary.return_count != 1 ||
        summary.return_imm_count != 0 ||
        summary.return_spill_count != 0) {
        fprintf(stderr, "[machine-select] FAIL: call-imm-family summary mismatch\n");
        ok = 0;
    }

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_lowers_spill_result_call_families(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrOperand call_args[1];
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    MachineSelectFunctionSummary summary;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: spill-call-family setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    function->spill_slot_count = 1;

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_spill_slot(0);
    instruction.as.call.callee_name = "g";
    call_args[0] = machine_ir_operand_immediate(9);
    instruction.as.call.args = call_args;
    instruction.as.call.arg_count = 1;
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_spill_slot(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: spill-call-family lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=0 locals=0 spills=1\n"
        "  bb.0:\n"
        "    spill.0 = calli_spill g(9)\n"
        "    retspill spill.0\n");

    if (!machine_select_function_compute_summary(&select_program.functions[0], &summary) ||
        summary.call_count != 0 ||
        summary.call_imm_count != 0 ||
        summary.call_spill_count != 0 ||
        summary.call_imm_spill_count != 1 ||
        summary.return_count != 0 ||
        summary.return_imm_count != 0 ||
        summary.return_spill_count != 1) {
        fprintf(stderr, "[machine-select] FAIL: spill-call-family summary mismatch\n");
        ok = 0;
    }

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_distinguishes_void_calls(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: void-call setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 0;
    instruction.as.call.callee_name = "sink";
    instruction.as.call.args = NULL;
    instruction.as.call.arg_count = 0;
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_void_return(&function->blocks[0], &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: void-call lowering failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=0 locals=0 spills=0\n"
        "  bb.0:\n"
        "    call_void sink()\n"
        "    ret\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_builds_report_artifact(void) {
    MachineIrProgram machine_program;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectLowerReport report;
    MachineSelectError select_error;
    MachineSelectTargetPolicySummary program_policy_summary;
    const MachineSelectTargetPolicySummary *report_policy_summary = NULL;
    const MachineSelectFunctionSummary *summary = NULL;
    char *actual_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    memset(&program_policy_summary, 0, sizeof(program_policy_summary));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);
    machine_select_lower_report_init(&report);

    if (!build_machine_ir_smoke_program(&machine_program, &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error) ||
        !machine_select_build_report_from_program(&select_program, &report, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: report artifact build failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        machine_select_lower_report_free(&report);
        return 0;
    }

    if (!machine_select_lower_report_get_function_summary_artifact(&report, 0, &summary) ||
        !machine_select_program_get_target_policy_summary(&select_program, &program_policy_summary) ||
        !machine_select_lower_report_get_target_policy_summary_artifact(&report, &report_policy_summary) ||
        !machine_select_verify_current_riscv32_preview_compatibility(&select_program, &select_error) ||
        !machine_select_lower_report_verify_current_riscv32_preview_compatibility(&report, &select_error) ||
        !machine_select_verify_current_riscv32_preview_bytes_compatibility(&select_program, &select_error) ||
        !machine_select_lower_report_verify_current_riscv32_preview_bytes_compatibility(&report, &select_error) ||
        !summary ||
        program_policy_summary.current_riscv32_preview_logical_register_cap != 8u ||
        !program_policy_summary.supports_early_immediate_legalization ||
        !program_policy_summary.supports_compare_branch_fusion ||
        !program_policy_summary.preserves_spill_operands_for_later_materialization ||
        !program_policy_summary.preserves_global_slot_ops_for_later_address_formation ||
        !report_policy_summary ||
        report_policy_summary->current_riscv32_preview_logical_register_cap != 8u ||
        !report_policy_summary->supports_early_immediate_legalization ||
        !report_policy_summary->supports_compare_branch_fusion ||
        !report_policy_summary->preserves_spill_operands_for_later_materialization ||
        !report_policy_summary->preserves_global_slot_ops_for_later_address_formation ||
        report.function_summary_count != 1 ||
        summary->op_count != 4 ||
        summary->call_count != 0 ||
        summary->load_local_count != 1 ||
        summary->alu_imm_count != 1 ||
        summary->alu_count != 0 ||
        summary->materialize_imm_count != 0 ||
        summary->blocks_with_calls_count != 0 ||
        summary->blocks_with_memory_ops_count != 1 ||
        summary->branch_block_count != 0 ||
        summary->jump_block_count != 0 ||
        summary->return_block_count != 1 ||
        !machine_select_build_report_from_program_dump(&select_program, &actual_text, &select_error) ||
        !actual_text ||
        !strstr(actual_text, "machine_select report registers=2 globals=1 functions=1")) {
        fprintf(stderr, "[machine-select] FAIL: report artifact summary mismatch\n");
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    machine_select_lower_report_free(&report);
    return ok;
}

static int test_machine_select_rejects_riscv32_preview_incompatible_register_bank(void) {
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;
    size_t register_index;

    memset(&select_error, 0, sizeof(select_error));
    machine_select_program_init(&select_program);

    select_program.register_bank.register_count = 9u;
    select_program.register_bank.registers =
        (MachineSelectRegisterDesc *)calloc(9u, sizeof(MachineSelectRegisterDesc));
    if (!select_program.register_bank.registers) {
        return 0;
    }
    for (register_index = 0u; register_index < 9u; ++register_index) {
        select_program.register_bank.registers[register_index].register_id = register_index;
        select_program.register_bank.registers[register_index].name = dup_text("r");
        select_program.register_bank.registers[register_index].allocatable = 1u;
        if (!select_program.register_bank.registers[register_index].name) {
            machine_select_program_free(&select_program);
            return 0;
        }
    }

    if (machine_select_verify_current_riscv32_preview_compatibility(&select_program, &select_error) ||
        strstr(select_error.message, "MACHINE-SELECT-420") == NULL) {
        fprintf(stderr,
            "[machine-select] FAIL: oversized riscv32-preview register bank was not rejected: %s\n",
            select_error.message);
        ok = 0;
    }

    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_rejects_riscv32_preview_bytes_incompatible_branch_range(void) {
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    size_t op_index;
    int ok = 1;

    memset(&select_error, 0, sizeof(select_error));
    machine_select_program_init(&select_program);

    select_program.register_bank.register_count = 1u;
    select_program.register_bank.registers =
        (MachineSelectRegisterDesc *)calloc(1u, sizeof(MachineSelectRegisterDesc));
    select_program.function_count = 1u;
    select_program.function_capacity = 1u;
    select_program.functions = (MachineSelectFunction *)calloc(1u, sizeof(MachineSelectFunction));
    if (!select_program.register_bank.registers || !select_program.functions) {
        machine_select_program_free(&select_program);
        return 0;
    }
    select_program.register_bank.registers[0].register_id = 0u;
    select_program.register_bank.registers[0].name = dup_text("r0");
    select_program.register_bank.registers[0].allocatable = 1u;
    if (!select_program.register_bank.registers[0].name) {
        machine_select_program_free(&select_program);
        return 0;
    }

    select_program.functions[0].name = dup_text("main");
    select_program.functions[0].has_body = 1;
    select_program.functions[0].block_count = 3u;
    select_program.functions[0].block_capacity = 3u;
    select_program.functions[0].blocks = (MachineSelectBasicBlock *)calloc(3u, sizeof(MachineSelectBasicBlock));
    if (!select_program.functions[0].name || !select_program.functions[0].blocks) {
        machine_select_program_free(&select_program);
        return 0;
    }

    select_program.functions[0].blocks[0].id = 0u;
    select_program.functions[0].blocks[0].has_terminator = 1;
    select_program.functions[0].blocks[0].terminator.kind = MACHINE_SELECT_TERM_BRANCH;
    select_program.functions[0].blocks[0].terminator.as.branch.condition = machine_select_operand_register(0u);
    select_program.functions[0].blocks[0].terminator.as.branch.then_target = 2u;
    select_program.functions[0].blocks[0].terminator.as.branch.else_target = 1u;

    select_program.functions[0].blocks[1].id = 1u;
    select_program.functions[0].blocks[1].op_count = 513u;
    select_program.functions[0].blocks[1].op_capacity = 513u;
    select_program.functions[0].blocks[1].ops = (MachineSelectOp *)calloc(513u, sizeof(MachineSelectOp));
    select_program.functions[0].blocks[1].has_terminator = 1;
    select_program.functions[0].blocks[1].terminator.kind = MACHINE_SELECT_TERM_RETURN_IMM;
    select_program.functions[0].blocks[1].terminator.as.return_value = machine_select_operand_immediate(0u);
    if (!select_program.functions[0].blocks[1].ops) {
        machine_select_program_free(&select_program);
        return 0;
    }
    for (op_index = 0u; op_index < 513u; ++op_index) {
        select_program.functions[0].blocks[1].ops[op_index].kind = MACHINE_SELECT_OP_MATERIALIZE_IMM;
        select_program.functions[0].blocks[1].ops[op_index].has_result = 1;
        select_program.functions[0].blocks[1].ops[op_index].result = machine_select_operand_register(0u);
        select_program.functions[0].blocks[1].ops[op_index].as.copy_value = machine_select_operand_immediate(5000);
    }

    select_program.functions[0].blocks[2].id = 2u;
    select_program.functions[0].blocks[2].has_terminator = 1;
    select_program.functions[0].blocks[2].terminator.kind = MACHINE_SELECT_TERM_RETURN_IMM;
    select_program.functions[0].blocks[2].terminator.as.return_value = machine_select_operand_immediate(0u);

    if (machine_select_verify_current_riscv32_preview_bytes_compatibility(&select_program, &select_error) ||
        strstr(select_error.message, "MACHINE-SELECT-423") == NULL ||
        strstr(select_error.message, "MACHINE-LAYOUT-140") == NULL ||
        strstr(select_error.message, "MACHINE-EMIT-140") == NULL ||
        strstr(select_error.message, "MACHINE-ENCODE-125") == NULL ||
        strstr(select_error.message, "MACHINE-BYTES-344") == NULL) {
        fprintf(stderr,
            "[machine-select] FAIL: preview bytes-compatibility branch-range reject mismatch: %s\n",
            select_error.message);
        ok = 0;
    }

    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_report_refresh_surface(void) {
    MachineIrProgram machine_program;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectLowerReport report;
    MachineSelectError select_error;
    const MachineSelectFunctionSummary *summary = NULL;
    char *actual_text = NULL;
    size_t total_block_summary_count = 0;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);
    machine_select_lower_report_init(&report);

    if (!build_machine_ir_smoke_program(&machine_program, &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error) ||
        !machine_select_build_report_from_program(&select_program, &report, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: report refresh setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        machine_select_lower_report_free(&report);
        return 0;
    }

    report.program.functions[0].blocks[0].terminator.kind = MACHINE_SELECT_TERM_RETURN_IMM;
    report.program.functions[0].blocks[0].terminator.as.return_value = machine_select_operand_immediate(99);
    if (!machine_select_lower_report_refresh(&report, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: report refresh failed: %s\n", select_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        machine_select_lower_report_free(&report);
        return 0;
    }

    if (!machine_select_lower_report_get_summary(&report,
            NULL,
            NULL,
            NULL,
            &total_block_summary_count,
            NULL,
            NULL,
            NULL,
            NULL) ||
        total_block_summary_count != 1 ||
        !machine_select_lower_report_get_function_summary_artifact(&report, 0, &summary) ||
        !summary ||
        summary->return_count != 0 ||
        summary->return_imm_count != 1 ||
        !machine_select_dump_lower_report_artifact(&report, &actual_text, &select_error) ||
        !actual_text ||
        !strstr(actual_text, "reti=1") ||
        !strstr(actual_text, "reti 99")) {
        fprintf(stderr, "[machine-select] FAIL: report refresh surface mismatch\n");
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    machine_select_lower_report_free(&report);
    return ok;
}

static int test_machine_select_report_query_surface(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectLowerReport report;
    MachineSelectError select_error;
    const MachineSelectProgram *report_program = NULL;
    const MachineSelectFunction *report_function = NULL;
    const MachineSelectFunction *filtered_function = NULL;
    const MachineSelectFunction *artifact_function = NULL;
    const MachineSelectFunctionSummary *summary = NULL;
    const MachineSelectFunctionSummary *named_summary = NULL;
    const MachineSelectFunctionSummary *filtered_summary = NULL;
    const MachineSelectFunctionSummary *artifact_summary_ptr = NULL;
    const MachineSelectBlockShapeSummary *block_summary = NULL;
    const MachineSelectBlockShapeSummary *block_summary_by_id = NULL;
    const size_t *indices = NULL;
    size_t register_count = 0;
    size_t global_count = 0;
    size_t function_count = 0;
    size_t functions_with_calls = 0;
    size_t functions_with_spills = 0;
    size_t functions_with_memory_ops = 0;
    size_t functions_with_branches = 0;
    size_t function_index = 0;
    size_t filtered_index = 0;
    size_t block_index = 0;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_lower_report_init(&report);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_global(&machine_program, "g", NULL, &machine_error) ||
        !machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: report query setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_lower_report_free(&report);
        return 0;
    }
    function->spill_slot_count = 1;

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_global(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: report query setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_lower_report_free(&report);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_spill_slot(0);
    instruction.as.call.callee_name = dup_text("sink");
    if (!instruction.as.call.callee_name ||
        !machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 0, 0, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: report query setup failed: %s\n", machine_error.message);
        free(instruction.as.call.callee_name);
        machine_ir_program_free(&machine_program);
        machine_select_lower_report_free(&report);
        return 0;
    }

    if (!machine_select_build_report_from_machine_ir_program(&machine_program, &report, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: report query build failed: %s\n", select_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_lower_report_free(&report);
        return 0;
    }

    if (!machine_select_lower_report_get_summary(&report,
            &register_count,
            &global_count,
            &function_count,
            NULL,
            &functions_with_calls,
            &functions_with_spills,
            &functions_with_memory_ops,
            &functions_with_branches) ||
        register_count != 1 || global_count != 1 || function_count != 1 ||
        functions_with_calls != 1 || functions_with_spills != 1 ||
        functions_with_memory_ops != 1 || functions_with_branches != 1) {
        fprintf(stderr, "[machine-select] FAIL: report summary query mismatch\n");
        ok = 0;
    }

    if (!machine_select_lower_report_get_program(&report, &report_program) ||
        !report_program ||
        !machine_select_lower_report_get_function_artifact(&report, 0, &artifact_function, &artifact_summary_ptr) ||
        !artifact_function ||
        !artifact_summary_ptr ||
        artifact_summary_ptr->call_spill_count != 1 ||
        !machine_select_lower_report_get_function_by_name(&report, "main", &function_index, &report_function) ||
        function_index != 0 || !report_function ||
        !machine_select_lower_report_get_function_artifact_by_name(
            &report, "main", &function_index, &artifact_function, &artifact_summary_ptr) ||
        function_index != 0 ||
        !artifact_function ||
        !artifact_summary_ptr ||
        artifact_summary_ptr->call_spill_count != 1 ||
        !machine_select_lower_report_get_function_summary_artifact(&report, 0, &summary) ||
        !machine_select_lower_report_get_function_summary_artifact_by_name(
            &report, "main", &function_index, &named_summary) ||
        function_index != 0 || !named_summary ||
        !machine_select_lower_report_get_block_summary_artifact(&report, 0, 0, &block_summary) ||
        !machine_select_lower_report_get_block_summary_artifact_by_id(
            &report, 0, 0, &block_index, &block_summary_by_id) ||
        block_index != 0 ||
        !summary ||
        !block_summary ||
        !block_summary_by_id ||
        named_summary->call_spill_count != 1 ||
        summary->call_spill_count != 1 ||
        summary->load_global_count != 1 ||
        summary->plain_branch_count != 1 ||
        summary->blocks_with_calls_count != 1 ||
        summary->blocks_with_memory_ops_count != 1 ||
        summary->branch_block_count != 1 ||
        summary->jump_block_count != 0 ||
        summary->return_block_count != 0 ||
        block_summary->block_id != 0 ||
        block_summary->op_count != 2 ||
        block_summary->call_count != 1 ||
        block_summary->load_global_count != 1 ||
        block_summary->store_global_count != 0 ||
        block_summary_by_id->block_id != 0 ||
        block_summary_by_id->call_count != 1 ||
        !block_summary->has_branch_terminator ||
        block_summary->has_return_terminator ||
        !summary->has_spills) {
        fprintf(stderr, "[machine-select] FAIL: report program/function query mismatch\n");
        ok = 0;
    }

    if (!machine_select_lower_report_get_functions_with_calls(&report, &function_count, &indices) ||
        function_count != 1 || !indices || indices[0] != 0) {
        fprintf(stderr, "[machine-select] FAIL: report call filter mismatch\n");
        ok = 0;
    }
    if (!machine_select_lower_report_get_function_with_calls(
            &report, 0, &function_index, &filtered_function, &filtered_summary) ||
        function_index != 0 ||
        !filtered_function ||
        !filtered_summary ||
        filtered_summary->call_spill_count != 1) {
        fprintf(stderr, "[machine-select] FAIL: report call filter navigation mismatch\n");
        ok = 0;
    }
    if (!machine_select_lower_report_get_function_with_calls_by_name(
            &report, "main", &filtered_index, &function_index, &filtered_function, &filtered_summary) ||
        filtered_index != 0 ||
        function_index != 0 ||
        !filtered_function ||
        !filtered_summary ||
        filtered_summary->call_spill_count != 1) {
        fprintf(stderr, "[machine-select] FAIL: report call filter-by-name mismatch\n");
        ok = 0;
    }
    if (!machine_select_lower_report_get_functions_with_spills(&report, &function_count, &indices) ||
        function_count != 1 || !indices || indices[0] != 0) {
        fprintf(stderr, "[machine-select] FAIL: report spill filter mismatch\n");
        ok = 0;
    }
    if (!machine_select_lower_report_get_function_with_spills(
            &report, 0, &function_index, &filtered_function, &filtered_summary) ||
        function_index != 0 ||
        !filtered_function ||
        !filtered_summary ||
        !filtered_summary->has_spills) {
        fprintf(stderr, "[machine-select] FAIL: report spill filter navigation mismatch\n");
        ok = 0;
    }
    if (!machine_select_lower_report_get_function_with_spills_by_name(
            &report, "main", &filtered_index, &function_index, &filtered_function, &filtered_summary) ||
        filtered_index != 0 ||
        function_index != 0 ||
        !filtered_function ||
        !filtered_summary ||
        !filtered_summary->has_spills) {
        fprintf(stderr, "[machine-select] FAIL: report spill filter-by-name mismatch\n");
        ok = 0;
    }
    if (!machine_select_lower_report_get_functions_with_memory_ops(&report, &function_count, &indices) ||
        function_count != 1 || !indices || indices[0] != 0) {
        fprintf(stderr, "[machine-select] FAIL: report memory-op filter mismatch\n");
        ok = 0;
    }
    if (!machine_select_lower_report_get_function_with_memory_ops(
            &report, 0, &function_index, &filtered_function, &filtered_summary) ||
        function_index != 0 ||
        !filtered_function ||
        !filtered_summary ||
        filtered_summary->load_global_count != 1) {
        fprintf(stderr, "[machine-select] FAIL: report memory-op filter navigation mismatch\n");
        ok = 0;
    }
    if (!machine_select_lower_report_get_function_with_memory_ops_by_name(
            &report, "main", &filtered_index, &function_index, &filtered_function, &filtered_summary) ||
        filtered_index != 0 ||
        function_index != 0 ||
        !filtered_function ||
        !filtered_summary ||
        filtered_summary->load_global_count != 1) {
        fprintf(stderr, "[machine-select] FAIL: report memory-op filter-by-name mismatch\n");
        ok = 0;
    }
    if (!machine_select_lower_report_get_functions_with_branches(&report, &function_count, &indices) ||
        function_count != 1 || !indices || indices[0] != 0) {
        fprintf(stderr, "[machine-select] FAIL: report branch filter mismatch\n");
        ok = 0;
    }
    if (!machine_select_lower_report_get_function_with_branches(
            &report, 0, &function_index, &filtered_function, &filtered_summary) ||
        function_index != 0 ||
        !filtered_function ||
        !filtered_summary ||
        filtered_summary->plain_branch_count != 1) {
        fprintf(stderr, "[machine-select] FAIL: report branch filter navigation mismatch\n");
        ok = 0;
    }
    if (!machine_select_lower_report_get_function_with_branches_by_name(
            &report, "main", &filtered_index, &function_index, &filtered_function, &filtered_summary) ||
        filtered_index != 0 ||
        function_index != 0 ||
        !filtered_function ||
        !filtered_summary ||
        filtered_summary->plain_branch_count != 1) {
        fprintf(stderr, "[machine-select] FAIL: report branch filter-by-name mismatch\n");
        ok = 0;
    }

    machine_ir_program_free(&machine_program);
    machine_select_lower_report_free(&report);
    return ok;
}

static int test_machine_select_report_block_filter_surface(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectLowerReport report;
    MachineSelectError select_error;
    const MachineSelectBasicBlock *block = NULL;
    const MachineSelectBlockShapeSummary *block_summary = NULL;
    MachineSelectTerminatorSummary terminator_summary;
    size_t function_index = 0;
    size_t block_index = 0;
    size_t count = 0;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_lower_report_init(&report);

    machine_program.register_bank.register_count = 2;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(2, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    machine_program.register_bank.registers[1].register_id = 1;
    machine_program.register_bank.registers[1].name = dup_text("r1");
    machine_program.register_bank.registers[1].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_local(function, "b", 1, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: block-filter setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_lower_report_free(&report);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: block-filter setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_lower_report_free(&report);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1);
    instruction.as.load_slot = machine_ir_slot_local(1);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: block-filter setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_lower_report_free(&report);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_EQ;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_register(1);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 1, 2, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_immediate(1), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(0), &machine_error) ||
        !machine_select_build_report_from_machine_ir_program(&machine_program, &report, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: block-filter lowering/build failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_lower_report_free(&report);
        return 0;
    }

    if (!machine_select_lower_report_get_function_block_filter_count(
            &report, 0, MACHINE_SELECT_BLOCK_FILTER_MEMORY_OPS, &count) ||
        count != 1 ||
        !machine_select_lower_report_get_function_block_filter_count(
            &report, 0, MACHINE_SELECT_BLOCK_FILTER_RETURNS, &count) ||
        count != 2 ||
        !machine_select_lower_report_get_function_block_filter_count(
            &report, 0, MACHINE_SELECT_BLOCK_FILTER_BRANCHES, &count) ||
        count != 0 ||
        !machine_select_lower_report_get_function_block_filter_count(
            &report, 0, MACHINE_SELECT_BLOCK_FILTER_COMPARE_BRANCHES, &count) ||
        count != 1) {
        fprintf(stderr, "[machine-select] FAIL: block-filter count mismatch\n");
        ok = 0;
    }

    if (!machine_select_lower_report_get_function_filtered_block_summary(
            &report, 0, MACHINE_SELECT_BLOCK_FILTER_COMPARE_BRANCHES, 0, &block_index, &block_summary) ||
        block_index != 0 ||
        !block_summary ||
        block_summary->block_id != 0 ||
        !block_summary->has_compare_branch_terminator ||
        !machine_select_lower_report_get_function_filtered_block_artifact(
            &report,
            0,
            MACHINE_SELECT_BLOCK_FILTER_COMPARE_BRANCHES,
            0,
            &block_index,
            &block,
            &block_summary,
            &terminator_summary) ||
        block_index != 0 ||
        !block ||
        !block_summary ||
        block_summary->block_id != 0 ||
        terminator_summary.kind != MACHINE_SELECT_TERM_COMPARE_BRANCH ||
        terminator_summary.primary_target_block_id != 1 ||
        terminator_summary.secondary_target_block_id != 2 ||
        !machine_select_lower_report_get_function_filtered_block_summary_by_name(
            &report, "main", MACHINE_SELECT_BLOCK_FILTER_RETURNS, 1, &function_index, &block_index, &block_summary) ||
        function_index != 0 ||
        block_index != 2 ||
        !block_summary ||
        block_summary->block_id != 2 ||
        !machine_select_lower_report_get_function_filtered_block_artifact_by_name(
            &report,
            "main",
            MACHINE_SELECT_BLOCK_FILTER_RETURNS,
            1,
            &function_index,
            &block_index,
            &block,
            &block_summary,
            &terminator_summary) ||
        function_index != 0 ||
        block_index != 2 ||
        !block ||
        !block_summary ||
        block_summary->block_id != 2 ||
        terminator_summary.kind != MACHINE_SELECT_TERM_RETURN_IMM ||
        !terminator_summary.has_return_value ||
        terminator_summary.return_value.immediate != 0 ||
        !block_summary->has_return_terminator) {
        fprintf(stderr, "[machine-select] FAIL: block-filter summary mismatch\n");
        ok = 0;
    }

    machine_ir_program_free(&machine_program);
    machine_select_lower_report_free(&report);
    return ok;
}

static int test_machine_select_report_dump_surface(void) {
    MachineIrProgram machine_program;
    MachineIrError machine_error;
    MachineSelectLowerReport report;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_lower_report_init(&report);

    if (!build_machine_ir_smoke_program(&machine_program, &machine_error) ||
        !machine_select_build_report_from_machine_ir_program(&machine_program, &report, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: report dump setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_lower_report_free(&report);
        return 0;
    }

    ok = expect_report_dump(
        &report,
        "machine_select report registers=2 globals=1 functions=1 with_calls=0 with_spills=0 with_memory_ops=1 with_branches=0\n"
        "target_policy preview_reg_cap=8 imm_legalization=1 cmpbr_fusion=1 preserves_spills=1 preserves_global_slots=1\n"
        "functions_with_calls:\n"
        "functions_with_spills:\n"
        "functions_with_memory_ops: 0\n"
        "functions_with_branches:\n"
        "function_summaries:\n"
        "  fn.0 main blocks=1 ops=4 calls=0 spills=0 load_local=1 store_local=0 store_locali=0 load_global=0 store_global=1 store_globali=0 ret=1 reti=0 retspill=0 br=0 cmpbr=0 cmpbri=0 blocks_with_calls=0 blocks_with_memory_ops=1 branch_blocks=0 jump_blocks=0 return_blocks=1\n"
        "    bb.0 ops=4 calls=0 load_local=1 store_local=0 store_locali=0 load_global=0 store_global=1 store_globali=0 term_ret=1 term_jmp=0 term_br=0 term_cmpbr=0\n"
        "\n"
        "machine_select\n"
        "function main params=1 locals=1 spills=0\n"
        "  bb.0:\n"
        "    reg.0 = load_local local.0\n"
        "    reg.1 = alui.0 reg.0, 1\n"
        "    store_global global.0, reg.1\n"
        "    reg.0 = copy reg.1\n"
        "    ret reg.0\n");

    machine_ir_program_free(&machine_program);
    machine_select_lower_report_free(&report);
    return ok;
}

static int test_machine_select_canonicalized_report_surface(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectLowerReport report;
    MachineSelectError select_error;
    const MachineSelectFunctionSummary *summary = NULL;
    size_t function_index = 0;
    char *actual_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_lower_report_init(&report);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: canonicalized report setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_lower_report_free(&report);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_immediate(7);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_register(0), &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: canonicalized report setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_lower_report_free(&report);
        return 0;
    }

    if (!machine_select_build_report_from_canonicalized_machine_ir_program(&machine_program, &report, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: canonicalized report build failed: %s\n", select_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_lower_report_free(&report);
        return 0;
    }

    if (!machine_select_lower_report_get_function_summary_artifact_by_name(&report, "main", &function_index, &summary) ||
        function_index != 0 ||
        !summary ||
        summary->block_count != 1) {
        fprintf(stderr, "[machine-select] FAIL: canonicalized report summary mismatch\n");
        ok = 0;
    }
    if (!machine_select_dump_report_from_canonicalized_machine_ir_program(&machine_program, &actual_text, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: canonicalized report dump failed: %s\n", select_error.message);
        ok = 0;
    } else if (!strstr(actual_text, "functions=1") ||
        !strstr(actual_text, "fn.0 main blocks=1") ||
        strstr(actual_text, "bb.1")) {
        fprintf(stderr, "[machine-select] FAIL: canonicalized report dump mismatch\nactual:\n%s\n", actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&machine_program);
    machine_select_lower_report_free(&report);
    return ok;
}

static int test_machine_select_builds_canonicalized_flat_report_from_value_ssa(void) {
    ValueSsaProgram ssa_program;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t value_id;
    ValueSsaError ssa_error;
    MachineSelectLowerReport report;
    MachineSelectError select_error;
    char *actual_text = NULL;
    int ok = 1;

    memset(&ssa_error, 0, sizeof(ssa_error));
    memset(&select_error, 0, sizeof(select_error));
    value_ssa_program_init(&ssa_program);
    machine_select_lower_report_init(&report);

    if (!value_ssa_program_append_function(&ssa_program, "main", 1, &function, &ssa_error) ||
        !function ||
        !value_ssa_function_append_block(function, NULL, &block, &ssa_error)) {
        fprintf(stderr, "[machine-select] FAIL: canonicalized flat report setup failed: %s\n", ssa_error.message);
        value_ssa_program_free(&ssa_program);
        machine_select_lower_report_free(&report);
        return 0;
    }

    value_id = value_ssa_function_allocate_value(function);
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value_id);
    instruction.as.mov_value = value_ssa_value_immediate(5);
    if (!value_ssa_block_append_instruction(block, &instruction, &ssa_error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(value_id), &ssa_error)) {
        fprintf(stderr, "[machine-select] FAIL: canonicalized flat report setup failed: %s\n", ssa_error.message);
        value_ssa_program_free(&ssa_program);
        machine_select_lower_report_free(&report);
        return 0;
    }

    if (!machine_select_build_allocate_and_rewrite_program_single_block_spills_canonicalized_flat_report(
            &ssa_program, 1, 1, &report, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: canonicalized flat report build failed: %s\n", select_error.message);
        value_ssa_program_free(&ssa_program);
        machine_select_lower_report_free(&report);
        return 0;
    }

    if (!machine_select_dump_lower_report_artifact(&report, &actual_text, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: canonicalized flat report dump failed: %s\n", select_error.message);
        ok = 0;
    } else if (!strstr(actual_text, "functions=1") || !strstr(actual_text, "reti 5")) {
        fprintf(stderr, "[machine-select] FAIL: canonicalized flat report dump mismatch\nactual:\n%s\n", actual_text);
        ok = 0;
    }

    free(actual_text);
    value_ssa_program_free(&ssa_program);
    machine_select_lower_report_free(&report);
    return ok;
}

static int test_machine_select_report_query_dump_reject_missing_function_table(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectLowerReport report;
    MachineSelectError select_error;
    const MachineSelectFunction *function_view = NULL;
    const MachineSelectFunctionSummary *summary_view = NULL;
    char *actual_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_lower_report_init(&report);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: malformed-report setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_lower_report_free(&report);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = dup_text("sink");
    if (!instruction.as.call.callee_name ||
        !machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_register(0), &machine_error) ||
        !machine_select_build_report_from_machine_ir_program(&machine_program, &report, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: malformed-report build failed: %s\n", select_error.message);
        free(instruction.as.call.callee_name);
        machine_ir_program_free(&machine_program);
        machine_select_lower_report_free(&report);
        return 0;
    }

    free(report.program.functions);
    report.program.functions = NULL;
    memset(&select_error, 0, sizeof(select_error));
    if (machine_select_lower_report_get_function_by_name(&report, "main", NULL, &function_view) ||
        machine_select_lower_report_get_function_with_calls_by_name(
            &report, "main", NULL, NULL, &function_view, &summary_view) ||
        machine_select_dump_lower_report_artifact(&report, &actual_text, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: malformed report query/dump should fail on missing function table\n");
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&machine_program);
    machine_select_lower_report_free(&report);
    return ok;
}

static int test_machine_select_reuses_repeated_internal_pure_call_in_indirect_block(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *helper = NULL;
    MachineIrFunction *main_function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 2u;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(2u, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0u;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    machine_program.register_bank.registers[1].register_id = 1u;
    machine_program.register_bank.registers[1].name = dup_text("r1");
    machine_program.register_bank.registers[1].allocatable = 1u;
    if (!machine_program.register_bank.registers[0].name || !machine_program.register_bank.registers[1].name) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    if (!machine_ir_program_append_function(&machine_program, "helper", 1, &helper, &machine_error) ||
        !helper ||
        !machine_ir_function_append_local(helper, "x", 1, NULL, &machine_error) ||
        !machine_ir_function_append_local(helper, "y", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(helper, NULL, NULL, &machine_error) ||
        !machine_ir_program_append_function(&machine_program, "main", 1, &main_function, &machine_error) ||
        !main_function ||
        !machine_ir_function_append_local(main_function, "x", 1, NULL, &machine_error) ||
        !machine_ir_function_append_local(main_function, "y", 1, NULL, &machine_error) ||
        !machine_ir_function_append_local(main_function, "addr", 0, NULL, &machine_error) ||
        !machine_ir_function_append_block(main_function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: repeated internal pure-call setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.load_slot = machine_ir_slot_local(0u);
    if (!machine_ir_block_append_instruction(&helper->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: helper setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1u);
    instruction.as.load_slot = machine_ir_slot_local(1u);
    if (!machine_ir_block_append_instruction(&helper->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: helper setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(0u);
    instruction.as.binary.rhs = machine_ir_operand_register(1u);
    if (!machine_ir_block_append_instruction(&helper->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&helper->blocks[0], machine_ir_operand_register(0u), &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: helper setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_ADDR_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1u);
    instruction.as.addr_slot = machine_ir_slot_local(2u);
    if (!machine_ir_block_append_instruction(&main_function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: main setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.load_slot = machine_ir_slot_local(0u);
    if (!machine_ir_block_append_instruction(&main_function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: main setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_INDIRECT;
    instruction.as.store_indirect.addr = machine_ir_operand_register(1u);
    instruction.as.store_indirect.value = machine_ir_operand_register(0u);
    if (!machine_ir_block_append_instruction(&main_function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: main setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1u);
    instruction.as.load_slot = machine_ir_slot_local(0u);
    if (!machine_ir_block_append_instruction(&main_function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: main setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.load_slot = machine_ir_slot_local(1u);
    if (!machine_ir_block_append_instruction(&main_function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: main setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.call.callee_name = "helper";
    instruction.as.call.arg_count = 2u;
    instruction.as.call.args = (MachineIrOperand *)calloc(2u, sizeof(MachineIrOperand));
    if (!instruction.as.call.args) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    instruction.as.call.args[0] = machine_ir_operand_register(1u);
    instruction.as.call.args[1] = machine_ir_operand_register(0u);
    if (!machine_ir_block_append_instruction(&main_function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: main setup failed: %s\n", machine_error.message);
        free(instruction.as.call.args);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    free(instruction.as.call.args);
    instruction.as.call.args = NULL;

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_INDIRECT;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1u);
    instruction.as.load_indirect_addr = machine_ir_operand_register(0u);
    if (!machine_ir_block_append_instruction(&main_function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: main setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1u);
    instruction.as.load_slot = machine_ir_slot_local(0u);
    if (!machine_ir_block_append_instruction(&main_function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: main setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.load_slot = machine_ir_slot_local(1u);
    if (!machine_ir_block_append_instruction(&main_function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: main setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1u);
    instruction.as.call.callee_name = "helper";
    instruction.as.call.arg_count = 2u;
    instruction.as.call.args = (MachineIrOperand *)calloc(2u, sizeof(MachineIrOperand));
    if (!instruction.as.call.args) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    instruction.as.call.args[0] = machine_ir_operand_register(1u);
    instruction.as.call.args[1] = machine_ir_operand_register(0u);
    if (!machine_ir_block_append_instruction(&main_function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&main_function->blocks[0], machine_ir_operand_register(1u), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: repeated internal pure-call lowering failed: %s\n", select_error.message);
        free(instruction.as.call.args);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    free(instruction.as.call.args);
    instruction.as.call.args = NULL;

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function helper params=2 locals=2 spills=0\n"
        "  bb.0:\n"
        "    reg.0 = load_local local.0\n"
        "    reg.1 = load_local local.1\n"
        "    reg.0 = alu.0 reg.0, reg.1\n"
        "    ret reg.0\n"
        "function main params=2 locals=3 spills=1\n"
        "  bb.0:\n"
        "    reg.1 = addr_local local.2\n"
        "    reg.0 = load_local local.0\n"
        "    store_indirect reg.1, reg.0\n"
        "    reg.1 = load_local local.0\n"
        "    reg.0 = load_local local.1\n"
        "    spill.0 = call_spill helper(reg.1, reg.0)\n"
        "    reg.1 = load_indirect spill.0\n"
        "    retspill spill.0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_reuses_repeated_internal_pure_call_across_unrelated_store_indirect(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *helper = NULL;
    MachineIrFunction *main_function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 3u;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(3u, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0u;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    machine_program.register_bank.registers[1].register_id = 1u;
    machine_program.register_bank.registers[1].name = dup_text("r1");
    machine_program.register_bank.registers[1].allocatable = 1u;
    machine_program.register_bank.registers[2].register_id = 2u;
    machine_program.register_bank.registers[2].name = dup_text("r2");
    machine_program.register_bank.registers[2].allocatable = 1u;
    if (!machine_program.register_bank.registers[0].name ||
        !machine_program.register_bank.registers[1].name ||
        !machine_program.register_bank.registers[2].name) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    if (!machine_ir_program_append_function(&machine_program, "helper", 1, &helper, &machine_error) ||
        !helper ||
        !machine_ir_function_append_local(helper, "x", 1, NULL, &machine_error) ||
        !machine_ir_function_append_local(helper, "y", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(helper, NULL, NULL, &machine_error) ||
        !machine_ir_program_append_function(&machine_program, "main", 1, &main_function, &machine_error) ||
        !main_function ||
        !machine_ir_function_append_local(main_function, "x", 1, NULL, &machine_error) ||
        !machine_ir_function_append_local(main_function, "y", 1, NULL, &machine_error) ||
        !machine_ir_function_append_local(main_function, "addr", 0, NULL, &machine_error) ||
        !machine_ir_function_append_block(main_function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: store-indirect reuse setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.load_slot = machine_ir_slot_local(0u);
    if (!machine_ir_block_append_instruction(&helper->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: store-indirect helper setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1u);
    instruction.as.load_slot = machine_ir_slot_local(1u);
    if (!machine_ir_block_append_instruction(&helper->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: store-indirect helper setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(0u);
    instruction.as.binary.rhs = machine_ir_operand_register(1u);
    if (!machine_ir_block_append_instruction(&helper->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&helper->blocks[0], machine_ir_operand_register(0u), &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: store-indirect helper setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_ADDR_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(2u);
    instruction.as.addr_slot = machine_ir_slot_local(2u);
    if (!machine_ir_block_append_instruction(&main_function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: store-indirect main setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.load_slot = machine_ir_slot_local(0u);
    if (!machine_ir_block_append_instruction(&main_function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: store-indirect main setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1u);
    instruction.as.load_slot = machine_ir_slot_local(1u);
    if (!machine_ir_block_append_instruction(&main_function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: store-indirect main setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.call.callee_name = "helper";
    instruction.as.call.arg_count = 2u;
    instruction.as.call.args = (MachineIrOperand *)calloc(2u, sizeof(MachineIrOperand));
    if (!instruction.as.call.args) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    instruction.as.call.args[0] = machine_ir_operand_register(0u);
    instruction.as.call.args[1] = machine_ir_operand_register(1u);
    if (!machine_ir_block_append_instruction(&main_function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: store-indirect main setup failed: %s\n", machine_error.message);
        free(instruction.as.call.args);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    free(instruction.as.call.args);
    instruction.as.call.args = NULL;

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_INDIRECT;
    instruction.as.store_indirect.addr = machine_ir_operand_register(2u);
    instruction.as.store_indirect.value = machine_ir_operand_register(0u);
    if (!machine_ir_block_append_instruction(&main_function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: store-indirect main setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.load_slot = machine_ir_slot_local(0u);
    if (!machine_ir_block_append_instruction(&main_function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: store-indirect main setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1u);
    instruction.as.load_slot = machine_ir_slot_local(1u);
    if (!machine_ir_block_append_instruction(&main_function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: store-indirect main setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1u);
    instruction.as.call.callee_name = "helper";
    instruction.as.call.arg_count = 2u;
    instruction.as.call.args = (MachineIrOperand *)calloc(2u, sizeof(MachineIrOperand));
    if (!instruction.as.call.args) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    instruction.as.call.args[0] = machine_ir_operand_register(0u);
    instruction.as.call.args[1] = machine_ir_operand_register(1u);
    if (!machine_ir_block_append_instruction(&main_function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&main_function->blocks[0], machine_ir_operand_register(1u), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: store-indirect repeated-call lowering failed: %s\n", select_error.message);
        free(instruction.as.call.args);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    free(instruction.as.call.args);
    instruction.as.call.args = NULL;

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function helper params=2 locals=2 spills=0\n"
        "  bb.0:\n"
        "    reg.0 = load_local local.0\n"
        "    reg.1 = load_local local.1\n"
        "    reg.0 = alu.0 reg.0, reg.1\n"
        "    ret reg.0\n"
        "function main params=2 locals=3 spills=1\n"
        "  bb.0:\n"
        "    reg.2 = addr_local local.2\n"
        "    reg.0 = load_local local.0\n"
        "    reg.1 = load_local local.1\n"
        "    spill.0 = call_spill helper(reg.0, reg.1)\n"
        "    store_indirect reg.2, spill.0\n"
        "    retspill spill.0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_keeps_repeated_internal_pure_call_across_global_write_indirect(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *helper = NULL;
    MachineIrFunction *main_function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 2u;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(2u, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0u;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    machine_program.register_bank.registers[1].register_id = 1u;
    machine_program.register_bank.registers[1].name = dup_text("r1");
    machine_program.register_bank.registers[1].allocatable = 1u;
    if (!machine_program.register_bank.registers[0].name || !machine_program.register_bank.registers[1].name) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    if (!machine_ir_program_append_global(&machine_program, "g", NULL, &machine_error) ||
        !machine_ir_program_append_function(&machine_program, "helper", 1, &helper, &machine_error) ||
        !helper ||
        !machine_ir_function_append_local(helper, "x", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(helper, NULL, NULL, &machine_error) ||
        !machine_ir_program_append_function(&machine_program, "main", 1, &main_function, &machine_error) ||
        !main_function ||
        !machine_ir_function_append_local(main_function, "x", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(main_function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: global-write-indirect setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.load_slot = machine_ir_slot_global(0u);
    if (!machine_ir_block_append_instruction(&helper->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: helper global-read setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1u);
    instruction.as.load_slot = machine_ir_slot_local(0u);
    if (!machine_ir_block_append_instruction(&helper->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: helper global-read setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(0u);
    instruction.as.binary.rhs = machine_ir_operand_register(1u);
    if (!machine_ir_block_append_instruction(&helper->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&helper->blocks[0], machine_ir_operand_register(0u), &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: helper global-read setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.load_slot = machine_ir_slot_local(0u);
    if (!machine_ir_block_append_instruction(&main_function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: main global-write-indirect setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.call.callee_name = "helper";
    instruction.as.call.arg_count = 1u;
    instruction.as.call.args = (MachineIrOperand *)calloc(1u, sizeof(MachineIrOperand));
    if (!instruction.as.call.args) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    instruction.as.call.args[0] = machine_ir_operand_register(0u);
    if (!machine_ir_block_append_instruction(&main_function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: main global-write-indirect setup failed: %s\n", machine_error.message);
        free(instruction.as.call.args);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    free(instruction.as.call.args);
    instruction.as.call.args = NULL;

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_ADDR_GLOBAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1u);
    instruction.as.addr_slot = machine_ir_slot_global(0u);
    if (!machine_ir_block_append_instruction(&main_function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: main global-write-indirect setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_INDIRECT;
    instruction.as.store_indirect.addr = machine_ir_operand_register(1u);
    instruction.as.store_indirect.value = machine_ir_operand_register(0u);
    if (!machine_ir_block_append_instruction(&main_function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: main global-write-indirect setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.load_slot = machine_ir_slot_local(0u);
    if (!machine_ir_block_append_instruction(&main_function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: main global-write-indirect setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1u);
    instruction.as.call.callee_name = "helper";
    instruction.as.call.arg_count = 1u;
    instruction.as.call.args = (MachineIrOperand *)calloc(1u, sizeof(MachineIrOperand));
    if (!instruction.as.call.args) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    instruction.as.call.args[0] = machine_ir_operand_register(0u);
    if (!machine_ir_block_append_instruction(&main_function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&main_function->blocks[0], machine_ir_operand_register(1u), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: global-write-indirect lowering failed: %s\n", select_error.message);
        free(instruction.as.call.args);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    free(instruction.as.call.args);
    instruction.as.call.args = NULL;

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function helper params=1 locals=1 spills=0\n"
        "  bb.0:\n"
        "    reg.0 = load_global global.0\n"
        "    reg.1 = load_local local.0\n"
        "    reg.0 = alu.0 reg.0, reg.1\n"
        "    ret reg.0\n"
        "function main params=1 locals=1 spills=0\n"
        "  bb.0:\n"
        "    reg.0 = load_local local.0\n"
        "    reg.0 = call helper(reg.0)\n"
        "    reg.1 = addr_global global.0\n"
        "    store_indirect reg.1, reg.0\n"
        "    reg.0 = load_local local.0\n"
        "    reg.1 = call helper(reg.0)\n"
        "    ret reg.1\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_reuses_repeated_spill_pure_expr(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 1u;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1u, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0u;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    if (!machine_program.register_bank.registers[0].name) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "arr", 0, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: spill-pure-expr setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    function->spill_slot_count = 2u;

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_ADDR_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_spill_slot(0u);
    instruction.as.addr_slot = machine_ir_slot_local(0u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: spill-pure-expr setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_spill_slot(1u);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_spill_slot(0u);
    instruction.as.binary.rhs = machine_ir_operand_immediate(2);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: spill-pure-expr setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_spill_slot(0u);
    instruction.as.binary.rhs = machine_ir_operand_immediate(2);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: spill-pure-expr lowering failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_INDIRECT;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.load_indirect_addr = machine_ir_operand_register(0u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: spill-pure-expr lowering failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_INDIRECT;
    instruction.as.store_indirect.addr = machine_ir_operand_spill_slot(1u);
    instruction.as.store_indirect.value = machine_ir_operand_register(0u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_register(0u), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: spill-pure-expr lowering failed: %s\n", select_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=0 locals=1 spills=2\n"
        "  bb.0:\n"
        "    spill.0 = addr_local local.0\n"
        "    spill.1 = alui.0 spill.0, 2\n"
        "    reg.0 = load_indirect spill.1\n"
        "    store_indirect spill.1, reg.0\n"
        "    ret reg.0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_does_not_reuse_spill_pure_expr_across_redefined_operand(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 1u;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1u, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0u;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    if (!machine_program.register_bank.registers[0].name) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 0, NULL, &machine_error) ||
        !machine_ir_function_append_local(function, "b", 0, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: spill-pure-redef setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    function->spill_slot_count = 2u;

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_ADDR_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_spill_slot(0u);
    instruction.as.addr_slot = machine_ir_slot_local(0u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: spill-pure-redef setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_spill_slot(1u);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_spill_slot(0u);
    instruction.as.binary.rhs = machine_ir_operand_immediate(2);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: spill-pure-redef setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_ADDR_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_spill_slot(0u);
    instruction.as.addr_slot = machine_ir_slot_local(1u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: spill-pure-redef setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_spill_slot(0u);
    instruction.as.binary.rhs = machine_ir_operand_immediate(2);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: spill-pure-redef setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_INDIRECT;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.load_indirect_addr = machine_ir_operand_register(0u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_register(0u), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: spill-pure-redef lowering failed: %s\n", select_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=0 locals=2 spills=2\n"
        "  bb.0:\n"
        "    spill.0 = addr_local local.1\n"
        "    reg.0 = alui.0 spill.0, 2\n"
        "    reg.0 = load_indirect reg.0\n"
        "    ret reg.0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_reuses_repeated_register_pure_expr(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 3u;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(3u, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0u;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    machine_program.register_bank.registers[1].register_id = 1u;
    machine_program.register_bank.registers[1].name = dup_text("r1");
    machine_program.register_bank.registers[1].allocatable = 1u;
    machine_program.register_bank.registers[2].register_id = 2u;
    machine_program.register_bank.registers[2].name = dup_text("r2");
    machine_program.register_bank.registers[2].allocatable = 1u;
    if (!machine_program.register_bank.registers[0].name ||
        !machine_program.register_bank.registers[1].name ||
        !machine_program.register_bank.registers[2].name) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "base", 0, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.load_slot = machine_ir_slot_local(0u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1u);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(0u);
    instruction.as.binary.rhs = machine_ir_operand_immediate(4);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(2u);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(0u);
    instruction.as.binary.rhs = machine_ir_operand_immediate(4);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_register(2u), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=0 locals=1 spills=0\n"
        "  bb.0:\n"
        "    reg.0 = load_local local.0\n"
        "    reg.2 = alui.0 reg.0, 4\n"
        "    reg.0 = copy reg.2\n"
        "    ret reg.0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_does_not_reuse_register_pure_expr_across_redefined_operand(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 3u;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(3u, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0u;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    machine_program.register_bank.registers[1].register_id = 1u;
    machine_program.register_bank.registers[1].name = dup_text("r1");
    machine_program.register_bank.registers[1].allocatable = 1u;
    machine_program.register_bank.registers[2].register_id = 2u;
    machine_program.register_bank.registers[2].name = dup_text("r2");
    machine_program.register_bank.registers[2].allocatable = 1u;
    if (!machine_program.register_bank.registers[0].name ||
        !machine_program.register_bank.registers[1].name ||
        !machine_program.register_bank.registers[2].name) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "base", 0, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.load_slot = machine_ir_slot_local(0u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1u);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(0u);
    instruction.as.binary.rhs = machine_ir_operand_immediate(4);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1u);
    instruction.as.mov_value = machine_ir_operand_immediate(9);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(2u);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(0u);
    instruction.as.binary.rhs = machine_ir_operand_immediate(4);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_register(2u), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=0 locals=1 spills=0\n"
        "  bb.0:\n"
        "    reg.0 = load_local local.0\n"
        "    reg.2 = alui.0 reg.0, 4\n"
        "    reg.0 = copy reg.2\n"
        "    ret reg.0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_reuses_commutative_register_pure_expr(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 4u;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(4u, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    for (size_t i = 0; i < 4u; ++i) {
        machine_program.register_bank.registers[i].register_id = i;
        machine_program.register_bank.registers[i].name = dup_text(i == 0u ? "r0" : i == 1u ? "r1" : i == 2u ? "r2" : "r3");
        machine_program.register_bank.registers[i].allocatable = 1u;
        if (!machine_program.register_bank.registers[i].name) {
            machine_ir_program_free(&machine_program);
            machine_select_program_free(&select_program);
            return 0;
        }
    }

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_local(function, "b", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.load_slot = machine_ir_slot_local(0u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1u);
    instruction.as.load_slot = machine_ir_slot_local(1u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(2u);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(0u);
    instruction.as.binary.rhs = machine_ir_operand_register(1u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(3u);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(1u);
    instruction.as.binary.rhs = machine_ir_operand_register(0u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_register(3u), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=2 locals=2 spills=0\n"
        "  bb.0:\n"
        "    reg.0 = load_local local.0\n"
        "    reg.1 = load_local local.1\n"
        "    reg.3 = alu.0 reg.1, reg.0\n"
        "    reg.0 = copy reg.3\n"
        "    ret reg.0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_reuses_commutative_register_expr_after_shared_shift(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 4u;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(4u, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    for (size_t i = 0; i < 4u; ++i) {
        machine_program.register_bank.registers[i].register_id = i;
        machine_program.register_bank.registers[i].name = dup_text(i == 0u ? "r0" : i == 1u ? "r1" : i == 2u ? "r2" : "r3");
        machine_program.register_bank.registers[i].allocatable = 1u;
        if (!machine_program.register_bank.registers[i].name) {
            machine_ir_program_free(&machine_program);
            machine_select_program_free(&select_program);
            return 0;
        }
    }

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "base", 0, NULL, &machine_error) ||
        !machine_ir_function_append_local(function, "idx", 0, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    function->spill_slot_count = 1u;

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.load_slot = machine_ir_slot_local(0u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_spill_slot(0u);
    instruction.as.load_slot = machine_ir_slot_local(1u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1u);
    instruction.as.binary.op = MACHINE_IR_BINARY_MUL;
    instruction.as.binary.lhs = machine_ir_operand_spill_slot(0u);
    instruction.as.binary.rhs = machine_ir_operand_immediate(4);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(2u);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(1u);
    instruction.as.binary.rhs = machine_ir_operand_register(0u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(3u);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(0u);
    instruction.as.binary.rhs = machine_ir_operand_register(1u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_register(3u), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=0 locals=2 spills=1\n"
        "  bb.0:\n"
        "    reg.0 = load_local local.0\n"
        "    spill.0 = load_local local.1\n"
        "    reg.1 = alui.2 spill.0, 4\n"
        "    reg.3 = alu.0 reg.0, reg.1\n"
        "    reg.0 = copy reg.3\n"
        "    ret reg.0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_reuses_repeated_internal_pure_call_with_live_in_spill_args(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *helper = NULL;
    MachineIrFunction *main_function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 1u;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1u, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0u;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    if (!machine_program.register_bank.registers[0].name) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    if (!machine_ir_program_append_function(&machine_program, "helper", 1, &helper, &machine_error) ||
        !helper ||
        !machine_ir_function_append_local(helper, "x", 1, NULL, &machine_error) ||
        !machine_ir_function_append_local(helper, "y", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(helper, NULL, NULL, &machine_error) ||
        !machine_ir_program_append_function(&machine_program, "main", 1, &main_function, &machine_error) ||
        !main_function ||
        !machine_ir_function_append_local(main_function, "x", 1, NULL, &machine_error) ||
        !machine_ir_function_append_local(main_function, "y", 1, NULL, &machine_error) ||
        !machine_ir_function_append_local(main_function, "z", 0, NULL, &machine_error) ||
        !machine_ir_function_append_block(main_function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(main_function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: live-in spill-call setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    helper->spill_slot_count = 1u;
    main_function->spill_slot_count = 3u;

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.load_slot = machine_ir_slot_local(0u);
    if (!machine_ir_block_append_instruction(&helper->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: helper setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_spill_slot(0u);
    instruction.as.load_slot = machine_ir_slot_local(1u);
    if (!machine_ir_block_append_instruction(&helper->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: helper setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(0u);
    instruction.as.binary.rhs = machine_ir_operand_spill_slot(0u);
    if (!machine_ir_block_append_instruction(&helper->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&helper->blocks[0], machine_ir_operand_register(0u), &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: helper setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_spill_slot(0u);
    instruction.as.load_slot = machine_ir_slot_local(0u);
    if (!machine_ir_block_append_instruction(&main_function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: live-in spill-call setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_spill_slot(1u);
    instruction.as.load_slot = machine_ir_slot_local(1u);
    if (!machine_ir_block_append_instruction(&main_function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&main_function->blocks[0], 1u, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: live-in spill-call setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.call.callee_name = "helper";
    instruction.as.call.arg_count = 2u;
    instruction.as.call.args = (MachineIrOperand *)calloc(2u, sizeof(MachineIrOperand));
    if (!instruction.as.call.args) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    instruction.as.call.args[0] = machine_ir_operand_spill_slot(0u);
    instruction.as.call.args[1] = machine_ir_operand_spill_slot(1u);
    if (!machine_ir_block_append_instruction(&main_function->blocks[1], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: live-in spill-call setup failed: %s\n", machine_error.message);
        free(instruction.as.call.args);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    free(instruction.as.call.args);
    instruction.as.call.args = NULL;

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_LOCAL;
    instruction.as.store.slot = machine_ir_slot_local(2u);
    instruction.as.store.value = machine_ir_operand_register(0u);
    if (!machine_ir_block_append_instruction(&main_function->blocks[1], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: live-in spill-call setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.call.callee_name = "helper";
    instruction.as.call.arg_count = 2u;
    instruction.as.call.args = (MachineIrOperand *)calloc(2u, sizeof(MachineIrOperand));
    if (!instruction.as.call.args) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    instruction.as.call.args[0] = machine_ir_operand_spill_slot(0u);
    instruction.as.call.args[1] = machine_ir_operand_spill_slot(1u);
    if (!machine_ir_block_append_instruction(&main_function->blocks[1], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&main_function->blocks[1], machine_ir_operand_register(0u), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: live-in spill-call lowering failed: %s\n", select_error.message);
        free(instruction.as.call.args);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    free(instruction.as.call.args);
    instruction.as.call.args = NULL;

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function helper params=2 locals=2 spills=1\n"
        "  bb.0:\n"
        "    reg.0 = load_local local.0\n"
        "    spill.0 = load_local local.1\n"
        "    reg.0 = alu.0 reg.0, spill.0\n"
        "    ret reg.0\n"
        "function main params=2 locals=3 spills=4\n"
        "  bb.0:\n"
        "    spill.0 = load_local local.0\n"
        "    spill.1 = load_local local.1\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    spill.3 = call_spill helper(spill.0, spill.1)\n"
        "    store_local local.2, spill.3\n"
        "    retspill spill.3\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_reuses_repeated_internal_pure_call_from_unique_predecessor(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *helper = NULL;
    MachineIrFunction *main_function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 2u;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(2u, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0u;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    machine_program.register_bank.registers[1].register_id = 1u;
    machine_program.register_bank.registers[1].name = dup_text("r1");
    machine_program.register_bank.registers[1].allocatable = 1u;
    if (!machine_program.register_bank.registers[0].name || !machine_program.register_bank.registers[1].name) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    if (!machine_ir_program_append_function(&machine_program, "helper", 1, &helper, &machine_error) ||
        !helper ||
        !machine_ir_function_append_local(helper, "x", 1, NULL, &machine_error) ||
        !machine_ir_function_append_local(helper, "y", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(helper, NULL, NULL, &machine_error) ||
        !machine_ir_program_append_function(&machine_program, "main", 1, &main_function, &machine_error) ||
        !main_function ||
        !machine_ir_function_append_local(main_function, "x", 1, NULL, &machine_error) ||
        !machine_ir_function_append_local(main_function, "y", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(main_function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(main_function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(main_function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: unique-pred pure-call setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    main_function->spill_slot_count = 2u;

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.load_slot = machine_ir_slot_local(0u);
    if (!machine_ir_block_append_instruction(&helper->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: helper setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1u);
    instruction.as.load_slot = machine_ir_slot_local(1u);
    if (!machine_ir_block_append_instruction(&helper->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: helper setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(0u);
    instruction.as.binary.rhs = machine_ir_operand_register(1u);
    if (!machine_ir_block_append_instruction(&helper->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&helper->blocks[0], machine_ir_operand_register(0u), &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: helper setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_spill_slot(0u);
    instruction.as.load_slot = machine_ir_slot_local(0u);
    if (!machine_ir_block_append_instruction(&main_function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: unique-pred pure-call setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_spill_slot(1u);
    instruction.as.load_slot = machine_ir_slot_local(1u);
    if (!machine_ir_block_append_instruction(&main_function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: unique-pred pure-call setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.call.callee_name = "helper";
    instruction.as.call.arg_count = 2u;
    instruction.as.call.args = (MachineIrOperand *)calloc(2u, sizeof(MachineIrOperand));
    if (!instruction.as.call.args) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    instruction.as.call.args[0] = machine_ir_operand_spill_slot(0u);
    instruction.as.call.args[1] = machine_ir_operand_spill_slot(1u);
    if (!machine_ir_block_append_instruction(&main_function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: unique-pred pure-call setup failed: %s\n", machine_error.message);
        free(instruction.as.call.args);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    free(instruction.as.call.args);
    instruction.as.call.args = NULL;

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1u);
    instruction.as.binary.op = MACHINE_IR_BINARY_NE;
    instruction.as.binary.lhs = machine_ir_operand_register(0u);
    instruction.as.binary.rhs = machine_ir_operand_immediate(7);
    if (!machine_ir_block_append_instruction(&main_function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(
            &main_function->blocks[0], machine_ir_operand_register(1u), 1u, 2u, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: unique-pred pure-call setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.call.callee_name = "helper";
    instruction.as.call.arg_count = 2u;
    instruction.as.call.args = (MachineIrOperand *)calloc(2u, sizeof(MachineIrOperand));
    if (!instruction.as.call.args) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    instruction.as.call.args[0] = machine_ir_operand_spill_slot(0u);
    instruction.as.call.args[1] = machine_ir_operand_spill_slot(1u);
    if (!machine_ir_block_append_instruction(&main_function->blocks[1], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&main_function->blocks[1], machine_ir_operand_register(0u), &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: unique-pred pure-call setup failed: %s\n", machine_error.message);
        free(instruction.as.call.args);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    free(instruction.as.call.args);
    instruction.as.call.args = NULL;

    if (!machine_ir_block_set_return(&main_function->blocks[2], machine_ir_operand_immediate(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: unique-pred pure-call lowering failed: %s\n", select_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function helper params=2 locals=2 spills=0\n"
        "  bb.0:\n"
        "    reg.0 = load_local local.0\n"
        "    reg.1 = load_local local.1\n"
        "    reg.0 = alu.0 reg.0, reg.1\n"
        "    ret reg.0\n"
        "function main params=2 locals=2 spills=2\n"
        "  bb.0:\n"
        "    spill.0 = load_local local.0\n"
        "    spill.1 = load_local local.1\n"
        "    reg.0 = call helper(spill.0, spill.1)\n"
        "    cmpbri.11 reg.0, 7, bb.1, bb.2\n"
        "  bb.1:\n"
        "    ret reg.0\n"
        "  bb.2:\n"
        "    reti 0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}


static int test_machine_select_reuses_repeated_internal_pure_call_with_same_block_indirect_load_arg(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *helper = NULL;
    MachineIrFunction *main_function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 2u;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(2u, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0u;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    machine_program.register_bank.registers[1].register_id = 1u;
    machine_program.register_bank.registers[1].name = dup_text("r1");
    machine_program.register_bank.registers[1].allocatable = 1u;
    if (!machine_program.register_bank.registers[0].name || !machine_program.register_bank.registers[1].name) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    if (!machine_ir_program_append_function(&machine_program, "helper", 1, &helper, &machine_error) ||
        !helper ||
        !machine_ir_function_append_local(helper, "x", 1, NULL, &machine_error) ||
        !machine_ir_function_append_local(helper, "y", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(helper, NULL, NULL, &machine_error) ||
        !machine_ir_program_append_function(&machine_program, "main", 1, &main_function, &machine_error) ||
        !main_function ||
        !machine_ir_function_append_local(main_function, "y", 1, NULL, &machine_error) ||
        !machine_ir_function_append_local(main_function, "slot", 0, NULL, &machine_error) ||
        !machine_ir_function_append_block(main_function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: indirect-load pure-call setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    main_function->spill_slot_count = 1u;

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.load_slot = machine_ir_slot_local(0u);
    if (!machine_ir_block_append_instruction(&helper->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: helper setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1u);
    instruction.as.load_slot = machine_ir_slot_local(1u);
    if (!machine_ir_block_append_instruction(&helper->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: helper setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(0u);
    instruction.as.binary.rhs = machine_ir_operand_register(1u);
    if (!machine_ir_block_append_instruction(&helper->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&helper->blocks[0], machine_ir_operand_register(0u), &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: helper setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_ADDR_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_spill_slot(0u);
    instruction.as.addr_slot = machine_ir_slot_local(1u);
    if (!machine_ir_block_append_instruction(&main_function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: main setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_INDIRECT;
    instruction.as.store_indirect.addr = machine_ir_operand_spill_slot(0u);
    instruction.as.store_indirect.value = machine_ir_operand_immediate(7);
    if (!machine_ir_block_append_instruction(&main_function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: main setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_INDIRECT;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1u);
    instruction.as.load_indirect_addr = machine_ir_operand_spill_slot(0u);
    if (!machine_ir_block_append_instruction(&main_function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: main setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.load_slot = machine_ir_slot_local(0u);
    if (!machine_ir_block_append_instruction(&main_function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: main setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.call.callee_name = "helper";
    instruction.as.call.arg_count = 2u;
    instruction.as.call.args = (MachineIrOperand *)calloc(2u, sizeof(MachineIrOperand));
    if (!instruction.as.call.args) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    instruction.as.call.args[0] = machine_ir_operand_register(1u);
    instruction.as.call.args[1] = machine_ir_operand_register(0u);
    if (!machine_ir_block_append_instruction(&main_function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: main setup failed: %s\n", machine_error.message);
        free(instruction.as.call.args);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    free(instruction.as.call.args);
    instruction.as.call.args = NULL;

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_INDIRECT;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1u);
    instruction.as.load_indirect_addr = machine_ir_operand_spill_slot(0u);
    if (!machine_ir_block_append_instruction(&main_function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: main setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.load_slot = machine_ir_slot_local(0u);
    if (!machine_ir_block_append_instruction(&main_function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: main setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1u);
    instruction.as.call.callee_name = "helper";
    instruction.as.call.arg_count = 2u;
    instruction.as.call.args = (MachineIrOperand *)calloc(2u, sizeof(MachineIrOperand));
    if (!instruction.as.call.args) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    instruction.as.call.args[0] = machine_ir_operand_register(1u);
    instruction.as.call.args[1] = machine_ir_operand_register(0u);
    if (!machine_ir_block_append_instruction(&main_function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&main_function->blocks[0], machine_ir_operand_register(1u), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: indirect-load pure-call lowering failed: %s\n", select_error.message);
        free(instruction.as.call.args);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    free(instruction.as.call.args);
    instruction.as.call.args = NULL;

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function helper params=2 locals=2 spills=0\n"
        "  bb.0:\n"
        "    reg.0 = load_local local.0\n"
        "    reg.1 = load_local local.1\n"
        "    reg.0 = alu.0 reg.0, reg.1\n"
        "    ret reg.0\n"
        "function main params=1 locals=2 spills=2\n"
        "  bb.0:\n"
        "    spill.0 = addr_local local.1\n"
        "    store_indirect spill.0, 7\n"
        "    reg.1 = load_indirect spill.0\n"
        "    reg.0 = load_local local.0\n"
        "    spill.1 = call_spill helper(reg.1, reg.0)\n"
        "    reg.1 = load_indirect spill.0\n"
        "    retspill spill.1\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_forwards_same_block_indirect_load_across_non_alias_store(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 1u;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1u, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0u;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    if (!machine_program.register_bank.registers[0].name) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "addr", 0, NULL, &machine_error) ||
        !machine_ir_function_append_local(function, "tmp", 0, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: indirect-load-forward setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    function->spill_slot_count = 1u;

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_ADDR_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_spill_slot(0u);
    instruction.as.addr_slot = machine_ir_slot_local(0u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: indirect-load-forward setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_INDIRECT;
    instruction.as.store_indirect.addr = machine_ir_operand_spill_slot(0u);
    instruction.as.store_indirect.value = machine_ir_operand_immediate(7);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: indirect-load-forward setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_INDIRECT;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.load_indirect_addr = machine_ir_operand_spill_slot(0u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: indirect-load-forward setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_LOCAL;
    instruction.as.store.slot = machine_ir_slot_local(1u);
    instruction.as.store.value = machine_ir_operand_register(0u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: indirect-load-forward setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_INDIRECT;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.load_indirect_addr = machine_ir_operand_spill_slot(0u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_register(0u), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: indirect-load-forward lowering failed: %s\n", select_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=0 locals=2 spills=2\n"
        "  bb.0:\n"
        "    spill.0 = addr_local local.0\n"
        "    store_indirect spill.0, 7\n"
        "    spill.1 = load_indirect spill.0\n"
        "    store_local local.1, spill.1\n"
        "    retspill spill.1\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_does_not_forward_indirect_load_across_aliasing_sibling_local_store(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 1u;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1u, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0u;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    if (!machine_program.register_bank.registers[0].name) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "arr0", 0, NULL, &machine_error) ||
        !machine_ir_function_append_local(function, "arr1", 0, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: aliasing indirect-load-forward setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    function->spill_slot_count = 2u;

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_ADDR_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_spill_slot(0u);
    instruction.as.addr_slot = machine_ir_slot_local(0u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: aliasing indirect-load-forward setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_spill_slot(1u);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_spill_slot(0u);
    instruction.as.binary.rhs = machine_ir_operand_immediate(4);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: aliasing indirect-load-forward setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_INDIRECT;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.load_indirect_addr = machine_ir_operand_spill_slot(1u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: aliasing indirect-load-forward setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_LOCAL;
    instruction.as.store.slot = machine_ir_slot_local(1u);
    instruction.as.store.value = machine_ir_operand_immediate(9);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: aliasing indirect-load-forward setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_INDIRECT;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.load_indirect_addr = machine_ir_operand_spill_slot(1u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_register(0u), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: aliasing indirect-load-forward lowering failed: %s\n", select_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=0 locals=2 spills=2\n"
        "  bb.0:\n"
        "    spill.0 = addr_local local.0\n"
        "    spill.1 = alui.0 spill.0, 4\n"
        "    reg.0 = load_indirect spill.1\n"
        "    store_locali local.1, 9\n"
        "    reg.0 = load_indirect spill.1\n"
        "    ret reg.0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_does_not_force_repeated_indirect_load_through_new_spill_when_result_feeds_shift_chain(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 6u;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(6u, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    for (size_t i = 0; i < 6u; ++i) {
        machine_program.register_bank.registers[i].register_id = i;
        machine_program.register_bank.registers[i].name =
            dup_text(i == 0u ? "r0" : i == 1u ? "r1" : i == 2u ? "r2" : i == 3u ? "r3" : i == 4u ? "r4" : "r5");
        machine_program.register_bank.registers[i].allocatable = 1u;
        if (!machine_program.register_bank.registers[i].name) {
            machine_ir_program_free(&machine_program);
            machine_select_program_free(&select_program);
            return 0;
        }
    }

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "idx_addr", 0, NULL, &machine_error) ||
        !machine_ir_function_append_local(function, "base", 0, NULL, &machine_error) ||
        !machine_ir_function_append_local(function, "val_addr", 0, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(3u);
    instruction.as.load_slot = machine_ir_slot_local(0u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(5u);
    instruction.as.load_slot = machine_ir_slot_local(1u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.load_slot = machine_ir_slot_local(2u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_INDIRECT;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(4u);
    instruction.as.load_indirect_addr = machine_ir_operand_register(3u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.binary.op = MACHINE_IR_BINARY_MUL;
    instruction.as.binary.lhs = machine_ir_operand_register(4u);
    instruction.as.binary.rhs = machine_ir_operand_immediate(4);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(2u);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(0u);
    instruction.as.binary.rhs = machine_ir_operand_register(5u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_INDIRECT;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.load_indirect_addr = machine_ir_operand_register(3u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.binary.op = MACHINE_IR_BINARY_MUL;
    instruction.as.binary.lhs = machine_ir_operand_register(0u);
    instruction.as.binary.rhs = machine_ir_operand_immediate(4);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(5u);
    instruction.as.binary.rhs = machine_ir_operand_register(0u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_INDIRECT;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(4u);
    instruction.as.load_indirect_addr = machine_ir_operand_register(0u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.load_slot = machine_ir_slot_local(1u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(4u);
    instruction.as.binary.rhs = machine_ir_operand_register(0u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_register(0u), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=0 locals=3 spills=1\n"
        "  bb.0:\n"
        "    reg.3 = load_local local.0\n"
        "    reg.5 = load_local local.1\n"
        "    spill.0 = load_indirect reg.3\n"
        "    reg.0 = alui.2 spill.0, 4\n"
        "    reg.0 = alu.0 reg.5, reg.0\n"
        "    reg.4 = load_indirect reg.0\n"
        "    reg.0 = load_local local.1\n"
        "    reg.0 = alu.0 reg.4, reg.0\n"
        "    ret reg.0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_exposes_mm_like_row_base_address_pattern(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 7u;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(7u, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    for (size_t i = 0; i < 7u; ++i) {
        machine_program.register_bank.registers[i].register_id = i;
        machine_program.register_bank.registers[i].name =
            dup_text(i == 0u ? "r0" : i == 1u ? "r1" : i == 2u ? "r2" : i == 3u ? "r3" :
                i == 4u ? "r4" : i == 5u ? "r5" : "r6");
        machine_program.register_bank.registers[i].allocatable = 1u;
        if (!machine_program.register_bank.registers[i].name) {
            machine_ir_program_free(&machine_program);
            machine_select_program_free(&select_program);
            return 0;
        }
    }

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "i", 0, NULL, &machine_error) ||
        !machine_ir_function_append_local(function, "b_base", 0, NULL, &machine_error) ||
        !machine_ir_function_append_local(function, "j", 0, NULL, &machine_error) ||
        !machine_ir_function_append_local(function, "c_base", 0, NULL, &machine_error) ||
        !machine_ir_function_append_local(function, "k", 0, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.load_slot = machine_ir_slot_local(0u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(5u);
    instruction.as.binary.op = MACHINE_IR_BINARY_MUL;
    instruction.as.binary.lhs = machine_ir_operand_register(0u);
    instruction.as.binary.rhs = machine_ir_operand_immediate(1024);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.load_slot = machine_ir_slot_local(1u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(2u);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(5u);
    instruction.as.binary.rhs = machine_ir_operand_register(0u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(6u);
    instruction.as.load_slot = machine_ir_slot_local(2u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.binary.op = MACHINE_IR_BINARY_MUL;
    instruction.as.binary.lhs = machine_ir_operand_register(6u);
    instruction.as.binary.rhs = machine_ir_operand_immediate(4);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(2u);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(2u);
    instruction.as.binary.rhs = machine_ir_operand_register(0u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_INDIRECT;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(3u);
    instruction.as.load_indirect_addr = machine_ir_operand_register(2u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.load_slot = machine_ir_slot_local(3u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(5u);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(5u);
    instruction.as.binary.rhs = machine_ir_operand_register(0u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.load_slot = machine_ir_slot_local(4u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(6u);
    instruction.as.binary.op = MACHINE_IR_BINARY_MUL;
    instruction.as.binary.lhs = machine_ir_operand_register(0u);
    instruction.as.binary.rhs = machine_ir_operand_immediate(1024);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.load_slot = machine_ir_slot_local(1u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(6u);
    instruction.as.binary.rhs = machine_ir_operand_register(0u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_INDIRECT;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(5u);
    instruction.as.load_indirect_addr = machine_ir_operand_register(0u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(6u);
    instruction.as.load_slot = machine_ir_slot_local(4u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(6u);
    instruction.as.binary.op = MACHINE_IR_BINARY_MUL;
    instruction.as.binary.lhs = machine_ir_operand_register(6u);
    instruction.as.binary.rhs = machine_ir_operand_immediate(4);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.load_slot = machine_ir_slot_local(3u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(6u);
    instruction.as.binary.rhs = machine_ir_operand_register(0u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_INDIRECT;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.load_indirect_addr = machine_ir_operand_register(0u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.binary.op = MACHINE_IR_BINARY_MUL;
    instruction.as.binary.lhs = machine_ir_operand_register(5u);
    instruction.as.binary.rhs = machine_ir_operand_register(0u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(3u);
    instruction.as.binary.rhs = machine_ir_operand_register(0u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_register(0u), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=0 locals=5 spills=0\n"
        "  bb.0:\n"
        "    reg.0 = load_local local.0\n"
        "    reg.5 = alui.2 reg.0, 1024\n"
        "    reg.0 = load_local local.1\n"
        "    reg.2 = alu.0 reg.5, reg.0\n"
        "    reg.6 = load_local local.2\n"
        "    reg.0 = alui.2 reg.6, 4\n"
        "    reg.2 = alu.0 reg.2, reg.0\n"
        "    reg.3 = load_indirect reg.2\n"
        "    reg.0 = load_local local.4\n"
        "    reg.6 = alui.2 reg.0, 1024\n"
        "    reg.0 = load_local local.1\n"
        "    reg.0 = alu.0 reg.6, reg.0\n"
        "    reg.5 = load_indirect reg.0\n"
        "    reg.6 = load_local local.4\n"
        "    reg.6 = alui.2 reg.6, 4\n"
        "    reg.0 = load_local local.3\n"
        "    reg.0 = alu.0 reg.6, reg.0\n"
        "    reg.0 = load_indirect reg.0\n"
        "    reg.0 = alu.2 reg.5, reg.0\n"
        "    reg.0 = alu.0 reg.3, reg.0\n"
        "    ret reg.0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_forwards_same_block_local_load_without_intervening_store(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 2u;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(2u, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0u;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    machine_program.register_bank.registers[1].register_id = 1u;
    machine_program.register_bank.registers[1].name = dup_text("r1");
    machine_program.register_bank.registers[1].allocatable = 1u;
    if (!machine_program.register_bank.registers[0].name ||
        !machine_program.register_bank.registers[1].name) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.load_slot = machine_ir_slot_local(0u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1u);
    instruction.as.load_slot = machine_ir_slot_local(0u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_register(1u), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=1 locals=1 spills=0\n"
        "  bb.0:\n"
        "    reg.1 = load_local local.0\n"
        "    reg.0 = copy reg.1\n"
        "    ret reg.0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_keeps_same_address_read_modify_write_shape_compact(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 2u;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(2u, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0u;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    machine_program.register_bank.registers[1].register_id = 1u;
    machine_program.register_bank.registers[1].name = dup_text("r1");
    machine_program.register_bank.registers[1].allocatable = 1u;
    if (!machine_program.register_bank.registers[0].name ||
        !machine_program.register_bank.registers[1].name) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "arr", 1, NULL, &machine_error) ||
        !machine_ir_function_append_local(function, "idx", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.load_slot = machine_ir_slot_local(0u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1u);
    instruction.as.load_slot = machine_ir_slot_local(1u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1u);
    instruction.as.binary.op = MACHINE_IR_BINARY_MUL;
    instruction.as.binary.lhs = machine_ir_operand_register(1u);
    instruction.as.binary.rhs = machine_ir_operand_immediate(4);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(0u);
    instruction.as.binary.rhs = machine_ir_operand_register(1u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_INDIRECT;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1u);
    instruction.as.load_indirect_addr = machine_ir_operand_register(0u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1u);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(1u);
    instruction.as.binary.rhs = machine_ir_operand_immediate(1);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_INDIRECT;
    instruction.has_result = 0;
    instruction.as.store_indirect.addr = machine_ir_operand_register(0u);
    instruction.as.store_indirect.value = machine_ir_operand_register(1u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_register(1u), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=2 locals=2 spills=0\n"
        "  bb.0:\n"
        "    reg.0 = load_local local.0\n"
        "    reg.1 = load_local local.1\n"
        "    reg.1 = alui.2 reg.1, 4\n"
        "    reg.0 = alu.0 reg.0, reg.1\n"
        "    reg.1 = load_indirect reg.0\n"
        "    reg.1 = alui.0 reg.1, 1\n"
        "    store_indirect reg.0, reg.1\n"
        "    ret reg.1\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_does_not_forward_same_block_local_load_across_store_local(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 3u;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(3u, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0u;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    machine_program.register_bank.registers[1].register_id = 1u;
    machine_program.register_bank.registers[1].name = dup_text("r1");
    machine_program.register_bank.registers[1].allocatable = 1u;
    machine_program.register_bank.registers[2].register_id = 2u;
    machine_program.register_bank.registers[2].name = dup_text("r2");
    machine_program.register_bank.registers[2].allocatable = 1u;
    if (!machine_program.register_bank.registers[0].name ||
        !machine_program.register_bank.registers[1].name ||
        !machine_program.register_bank.registers[2].name) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.load_slot = machine_ir_slot_local(0u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(2u);
    instruction.as.mov_value = machine_ir_operand_immediate(7);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_LOCAL;
    instruction.as.store.slot = machine_ir_slot_local(0u);
    instruction.as.store.value = machine_ir_operand_register(2u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1u);
    instruction.as.load_slot = machine_ir_slot_local(0u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_register(1u), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=1 locals=1 spills=0\n"
        "  bb.0:\n"
        "    store_locali local.0, 7\n"
        "    reg.1 = load_local local.0\n"
        "    reg.0 = copy reg.1\n"
        "    ret reg.0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

int main(void) {
    const char *filter = getenv("MACHINE_SELECT_REG_FILTER");

    if (filter && filter[0] != '\0') {
        if (strstr("MACHINE-SELECT-FLOAT-SOURCE-TRANSPORT", filter) != NULL) {
            return test_machine_select_lowers_extension_float_transport() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-GLOBAL-LITERAL-RUNTIME-INIT", filter) != NULL) {
            return test_machine_select_lowers_extension_float_global_literal_runtime_init() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-REPORT-QUERY", filter) != NULL) {
            return test_machine_select_float_report_query_surface() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-GLOBAL-LITERAL-REPORT-QUERY", filter) != NULL) {
            return test_machine_select_float_global_literal_report_query_surface() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-ASSIGN-LIVE-DEFAULT", filter) != NULL) {
            return test_machine_select_default_pipeline_preserves_live_extension_float_assignment_transport() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-RETURN-GLOBAL-LIVE-DEFAULT", filter) != NULL) {
            return test_machine_select_default_pipeline_preserves_float_return_transport_from_global() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-RETURN-GLOBAL-CALL-LIVE-DEFAULT", filter) != NULL) {
            return test_machine_select_default_pipeline_preserves_float_return_transport_from_global_call() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-GLOBAL-IDENT-INIT-DEFAULT", filter) != NULL) {
            return test_machine_select_default_pipeline_preserves_float_global_identifier_runtime_init() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-GLOBAL-CALL-INIT-DEFAULT", filter) != NULL) {
            return test_machine_select_default_pipeline_preserves_float_global_call_runtime_init() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-GLOBAL-OP-REJECT", filter) != NULL) {
            return test_machine_select_rejects_global_float_operator_expression_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-GLOBAL-OP-INIT-REJECT", filter) != NULL) {
            return test_machine_select_rejects_global_float_operator_expression_in_initializer_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-GLOBAL-CALL-INIT-TYPE-REJECT", filter) != NULL) {
            return test_machine_select_rejects_global_float_call_result_in_initializer_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-ASSIGN-TYPE-REJECT", filter) != NULL) {
            return test_machine_select_rejects_float_assignment_to_int_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-TERNARY-VALUE-RETURN-INT-REJECT", filter) != NULL) {
            return test_machine_select_rejects_float_ternary_value_return_to_int_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-UNARY-CALL-TERNARY-RETURN-INT-REJECT", filter) != NULL) {
            return test_machine_select_rejects_unary_call_ternary_value_return_to_int_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-IF-COND-ACCEPT", filter) != NULL) {
            return test_machine_select_accepts_float_if_condition_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-WHILE-COND-ACCEPT", filter) != NULL) {
            return test_machine_select_accepts_float_while_condition_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-FOR-COND-ACCEPT", filter) != NULL) {
            return test_machine_select_accepts_float_for_condition_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-LOGICAL-COND-COMPOSE-ACCEPT", filter) != NULL) {
            return test_machine_select_accepts_float_logical_condition_composition_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-EQ-COMPARE-ACCEPT", filter) != NULL) {
            return test_machine_select_accepts_float_equality_compare_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-LT-COMPARE-ACCEPT", filter) != NULL) {
            return test_machine_select_accepts_float_relational_compare_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-SIGNED-ZERO-EQ-ACCEPT", filter) != NULL) {
            return test_machine_select_accepts_signed_zero_float_equality_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-NEG-LT-ZERO-ACCEPT", filter) != NULL) {
            return test_machine_select_accepts_negative_float_relational_compare_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-ADD-ACCEPT", filter) != NULL) {
            return test_machine_select_accepts_float_addition_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-SUB-ACCEPT", filter) != NULL) {
            return test_machine_select_accepts_float_subtraction_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-NEG-ADD-COMBO-ACCEPT", filter) != NULL) {
            return test_machine_select_accepts_negative_float_addition_combo_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-NEG-SUB-COMBO-ACCEPT", filter) != NULL) {
            return test_machine_select_accepts_negative_float_subtraction_combo_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-MUL-ACCEPT", filter) != NULL) {
            return test_machine_select_accepts_float_multiplication_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-DIV-ACCEPT", filter) != NULL) {
            return test_machine_select_accepts_float_division_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-NEG-MUL-COMBO-ACCEPT", filter) != NULL) {
            return test_machine_select_accepts_negative_float_multiplication_combo_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-NEG-DIV-COMBO-ACCEPT", filter) != NULL) {
            return test_machine_select_accepts_negative_float_division_combo_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-CHAIN-ADD-ACCEPT", filter) != NULL) {
            return test_machine_select_accepts_chained_float_addition_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-TO-INT-CONVERT-ACCEPT", filter) != NULL) {
            return test_machine_select_accepts_explicit_int_from_float_conversion_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-INT-TO-FLOAT-CONVERT-ACCEPT", filter) != NULL) {
            return test_machine_select_accepts_explicit_float_from_int_conversion_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-INT-TO-FLOAT-RECURSIVE-INIT-BRIDGE-ACCEPT", filter) != NULL) {
            return test_machine_select_accepts_explicit_float_from_recursive_int_initializer_bridge_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-INT-TO-FLOAT-RECURSIVE-ASSIGN-BRIDGE-ACCEPT", filter) != NULL) {
            return test_machine_select_accepts_explicit_float_from_recursive_int_assignment_bridge_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-INT-TO-FLOAT-COMPARE-BRIDGE-ACCEPT", filter) != NULL) {
            return test_machine_select_accepts_explicit_float_from_int_compare_bridge_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-INT-TO-FLOAT-RECURSIVE-ARITH-BRIDGE-ACCEPT", filter) != NULL) {
            return test_machine_select_accepts_explicit_float_from_recursive_int_arithmetic_bridge_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-TO-INT-TERNARY-BRIDGE-ACCEPT", filter) != NULL) {
            return test_machine_select_accepts_explicit_int_from_float_ternary_bridge_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-TO-INT-RECURSIVE-CALLARG-BRIDGE-ACCEPT", filter) != NULL) {
            return test_machine_select_accepts_explicit_int_from_recursive_float_call_argument_bridge_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-TO-INT-RECURSIVE-ASSIGN-BRIDGE-ACCEPT", filter) != NULL) {
            return test_machine_select_accepts_explicit_int_from_recursive_float_assignment_bridge_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-TO-INT-COMPARE-BRIDGE-ACCEPT", filter) != NULL) {
            return test_machine_select_accepts_explicit_int_from_float_compare_bridge_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-TO-INT-RECURSIVE-ARITH-BRIDGE-ACCEPT", filter) != NULL) {
            return test_machine_select_accepts_explicit_int_from_recursive_float_arithmetic_bridge_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-NESTED-MUL-DIV-ACCEPT", filter) != NULL) {
            return test_machine_select_accepts_nested_float_mul_div_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-ARITH-INT-TYPE-REJECT", filter) != NULL) {
            return test_machine_select_rejects_mixed_float_int_arithmetic_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-CALL-ARITH-INT-TYPE-REJECT", filter) != NULL) {
            return test_machine_select_rejects_float_call_int_arithmetic_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-LITERAL-ARITH-INT-TYPE-REJECT", filter) != NULL) {
            return test_machine_select_rejects_float_literal_int_arithmetic_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-NEG-CALL-ARITH-INT-TYPE-REJECT", filter) != NULL) {
            return test_machine_select_rejects_negative_float_call_int_arithmetic_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-NESTED-TREE-PLUS-INT-REJECT", filter) != NULL) {
            return test_machine_select_rejects_nested_float_tree_plus_int_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-NESTED-MULDIV-PLUS-INT-REJECT", filter) != NULL) {
            return test_machine_select_rejects_nested_float_muldiv_plus_int_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-TERNARY-VALUE-PLUS-INT-REJECT", filter) != NULL) {
            return test_machine_select_rejects_float_ternary_value_plus_int_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-UNARY-CALL-TERNARY-PLUS-INT-REJECT", filter) != NULL) {
            return test_machine_select_rejects_unary_call_ternary_value_plus_int_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-TERNARY-PLUS-FLOAT-CALLARG-REJECT", filter) != NULL) {
            return test_machine_select_rejects_float_ternary_value_plus_float_call_argument_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-UNARY-CALL-TERNARY-PLUS-FLOAT-CALLARG-REJECT", filter) != NULL) {
            return test_machine_select_rejects_unary_call_ternary_value_plus_float_call_argument_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-TERNARY-VALUE-ASSIGN-INT-REJECT", filter) != NULL) {
            return test_machine_select_rejects_float_ternary_value_assignment_to_int_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-UNARY-CALL-TERNARY-ASSIGN-INT-REJECT", filter) != NULL) {
            return test_machine_select_rejects_unary_call_ternary_value_assignment_to_int_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-RECURSIVE-CALL-INIT-INT-REJECT", filter) != NULL) {
            return test_machine_select_rejects_recursive_float_call_result_in_initializer_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-RECURSIVE-CALL-CALLARG-INT-REJECT", filter) != NULL) {
            return test_machine_select_rejects_recursive_float_call_argument_to_int_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-TERNARY-VALUE-CALLARG-INT-REJECT", filter) != NULL) {
            return test_machine_select_rejects_float_ternary_value_call_argument_to_int_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-UNARY-CALL-TERNARY-CALLARG-INT-REJECT", filter) != NULL) {
            return test_machine_select_rejects_unary_call_ternary_value_call_argument_to_int_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-TERNARY-VALUE-CALLARG-FLOAT-ACCEPT", filter) != NULL) {
            return test_machine_select_accepts_float_ternary_value_call_argument_to_float_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-UNARY-CALL-TERNARY-CALLARG-FLOAT-ACCEPT", filter) != NULL) {
            return test_machine_select_accepts_unary_call_float_ternary_value_call_argument_to_float_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-TERNARY-VALUE-INIT-INT-REJECT", filter) != NULL) {
            return test_machine_select_rejects_float_ternary_value_initializer_to_int_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-UNARY-CALL-TERNARY-INIT-INT-REJECT", filter) != NULL) {
            return test_machine_select_rejects_unary_call_ternary_value_initializer_to_int_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-TERNARY-VALUE-RETURN-FLOAT-ACCEPT", filter) != NULL) {
            return test_machine_select_accepts_float_ternary_value_return_to_float_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-TERNARY-VALUE-ASSIGN-FLOAT-ACCEPT", filter) != NULL) {
            return test_machine_select_accepts_float_ternary_value_assignment_to_float_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-TERNARY-VALUE-INIT-FLOAT-ACCEPT", filter) != NULL) {
            return test_machine_select_accepts_float_ternary_value_initializer_to_float_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-TERNARY-VALUE-COMPARE-INT-REJECT", filter) != NULL) {
            return test_machine_select_rejects_float_ternary_value_compare_against_int_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-UNARY-CALL-TERNARY-COMPARE-INT-REJECT", filter) != NULL) {
            return test_machine_select_rejects_unary_call_ternary_value_compare_against_int_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-TERNARY-VALUE-COMPARE-FLOAT-REJECT", filter) != NULL) {
            return test_machine_select_rejects_float_ternary_value_compare_against_float_under_extension() ? 0 : 1;
        }
        if (strstr("MACHINE-SELECT-FLOAT-UNARY-CALL-TERNARY-COMPARE-FLOAT-REJECT", filter) != NULL) {
            return test_machine_select_rejects_unary_call_ternary_value_compare_against_float_under_extension() ? 0 : 1;
        }
    }

    if (!test_machine_select_lower_machine_ir_smoke()) {
        return 1;
    }
    if (!test_machine_select_rejects_phi_input()) {
        return 1;
    }
    if (!test_machine_select_rejects_load_store_slot_kind_mismatch()) {
        return 1;
    }
    if (!test_machine_select_rejects_missing_backing_tables()) {
        return 1;
    }
    if (!test_machine_select_query_dump_rejects_missing_backing_tables()) {
        return 1;
    }
    if (!test_machine_select_dump_rejects_missing_call_args()) {
        return 1;
    }
    if (!test_machine_select_summary_surface()) {
        return 1;
    }
    if (!test_machine_select_reuses_duplicate_addr_local_spill_roots_in_block()) {
        return 1;
    }
    if (!test_machine_select_forwards_self_copy_until_last_visible_use()) {
        return 1;
    }
    if (!test_machine_select_removes_redundant_same_result_addr_local_redefs()) {
        return 1;
    }
    if (!test_machine_select_removes_copy_self_noop()) {
        return 1;
    }
    if (!test_machine_select_folds_addr_local_copy_pair_into_final_result()) {
        return 1;
    }
    if (!test_machine_select_folds_pure_producer_copy_pair_into_final_result()) {
        return 1;
    }
    if (!test_machine_select_keeps_parameter_pointer_copy_when_original_value_is_used_in_successor()) {
        return 1;
    }
    if (!test_machine_select_forwards_compare_inputs_through_adjacent_copy()) {
        return 1;
    }
    if (!test_machine_select_fuses_branch_with_adjacent_compare_after_copy_forward()) {
        return 1;
    }
    if (!test_machine_select_distinguishes_cmp_ops()) {
        return 1;
    }
    if (!test_machine_select_distinguishes_alu_immediate_ops()) {
        return 1;
    }
    if (!test_machine_select_distinguishes_cmp_immediate_ops()) {
        return 1;
    }
    if (!test_machine_select_normalizes_commutative_immediate_to_rhs()) {
        return 1;
    }
    if (!test_machine_select_keeps_noncommutative_lhs_immediate_out_of_imm_family()) {
        return 1;
    }
    if (!test_machine_select_materializes_constant_binary_results()) {
        return 1;
    }
    if (!test_machine_select_materializes_overflowing_constant_binary_results()) {
        return 1;
    }
    if (!test_machine_select_cleanup_removes_shadowed_trivial_defs()) {
        return 1;
    }
    if (!test_machine_select_canonicalized_bridge()) {
        return 1;
    }
    if (!test_machine_select_canonicalized_conservative_bridge()) {
        return 1;
    }
    if (!test_machine_select_float_metadata_roundtrip()) {
        return 1;
    }
    if (!test_machine_select_lowers_extension_float_transport()) {
        return 1;
    }
    if (!test_machine_select_lowers_extension_float_global_literal_runtime_init()) {
        return 1;
    }
    if (!test_machine_select_builds_report_from_extension_float_machine_ir_report()) {
        return 1;
    }
    if (!test_machine_select_builds_report_from_extension_float_global_literal_machine_ir_report()) {
        return 1;
    }
    if (!test_machine_select_float_report_query_surface()) {
        return 1;
    }
    if (!test_machine_select_float_global_literal_report_query_surface()) {
        return 1;
    }
    if (!test_machine_select_default_pipeline_preserves_live_extension_float_assignment_transport()) {
        return 1;
    }
    if (!test_machine_select_default_pipeline_preserves_float_return_transport_from_global()) {
        return 1;
    }
    if (!test_machine_select_default_pipeline_preserves_float_return_transport_from_global_call()) {
        return 1;
    }
    if (!test_machine_select_default_pipeline_preserves_float_global_identifier_runtime_init()) {
        return 1;
    }
    if (!test_machine_select_default_pipeline_preserves_float_global_call_runtime_init()) {
        return 1;
    }
    if (!test_machine_select_rejects_global_float_operator_expression_under_extension()) {
        return 1;
    }
    if (!test_machine_select_rejects_global_float_operator_expression_in_initializer_under_extension()) {
        return 1;
    }
    if (!test_machine_select_rejects_global_float_call_result_in_initializer_under_extension()) {
        return 1;
    }
    if (!test_machine_select_rejects_float_assignment_to_int_under_extension()) {
        return 1;
    }
    if (!test_machine_select_accepts_float_if_condition_under_extension()) {
        return 1;
    }
    if (!test_machine_select_accepts_float_while_condition_under_extension()) {
        return 1;
    }
    if (!test_machine_select_accepts_float_for_condition_under_extension()) {
        return 1;
    }
    if (!test_machine_select_accepts_float_logical_condition_composition_under_extension()) {
        return 1;
    }
    if (!test_machine_select_accepts_float_equality_compare_under_extension()) {
        return 1;
    }
    if (!test_machine_select_accepts_float_relational_compare_under_extension()) {
        return 1;
    }
    if (!test_machine_select_accepts_signed_zero_float_equality_under_extension()) {
        return 1;
    }
    if (!test_machine_select_accepts_negative_float_relational_compare_under_extension()) {
        return 1;
    }
    if (!test_machine_select_accepts_float_addition_under_extension()) {
        return 1;
    }
    if (!test_machine_select_accepts_float_subtraction_under_extension()) {
        return 1;
    }
    if (!test_machine_select_accepts_negative_float_addition_combo_under_extension()) {
        return 1;
    }
    if (!test_machine_select_accepts_negative_float_subtraction_combo_under_extension()) {
        return 1;
    }
    if (!test_machine_select_accepts_float_multiplication_under_extension()) {
        return 1;
    }
    if (!test_machine_select_accepts_float_division_under_extension()) {
        return 1;
    }
    if (!test_machine_select_accepts_negative_float_multiplication_combo_under_extension()) {
        return 1;
    }
    if (!test_machine_select_accepts_negative_float_division_combo_under_extension()) {
        return 1;
    }
    if (!test_machine_select_accepts_chained_float_addition_under_extension()) {
        return 1;
    }
    if (!test_machine_select_accepts_explicit_int_from_float_conversion_under_extension()) {
        return 1;
    }
    if (!test_machine_select_accepts_explicit_float_from_int_conversion_under_extension()) {
        return 1;
    }
    if (!test_machine_select_accepts_explicit_float_from_recursive_int_initializer_bridge_under_extension()) {
        return 1;
    }
    if (!test_machine_select_accepts_explicit_float_from_recursive_int_assignment_bridge_under_extension()) {
        return 1;
    }
    if (!test_machine_select_accepts_explicit_float_from_int_compare_bridge_under_extension()) {
        return 1;
    }
    if (!test_machine_select_accepts_explicit_float_from_recursive_int_arithmetic_bridge_under_extension()) {
        return 1;
    }
    if (!test_machine_select_accepts_explicit_int_from_float_ternary_bridge_under_extension()) {
        return 1;
    }
    if (!test_machine_select_accepts_explicit_int_from_recursive_float_call_argument_bridge_under_extension()) {
        return 1;
    }
    if (!test_machine_select_accepts_explicit_int_from_recursive_float_assignment_bridge_under_extension()) {
        return 1;
    }
    if (!test_machine_select_accepts_explicit_int_from_float_compare_bridge_under_extension()) {
        return 1;
    }
    if (!test_machine_select_accepts_explicit_int_from_recursive_float_arithmetic_bridge_under_extension()) {
        return 1;
    }
    if (!test_machine_select_accepts_float_ternary_value_call_argument_to_float_under_extension()) {
        return 1;
    }
    if (!test_machine_select_accepts_unary_call_float_ternary_value_call_argument_to_float_under_extension()) {
        return 1;
    }
    if (!test_machine_select_accepts_nested_float_mul_div_under_extension()) {
        return 1;
    }
    if (!test_machine_select_rejects_mixed_float_int_arithmetic_under_extension()) {
        return 1;
    }
    if (!test_machine_select_rejects_float_call_int_arithmetic_under_extension()) {
        return 1;
    }
    if (!test_machine_select_rejects_float_literal_int_arithmetic_under_extension()) {
        return 1;
    }
    if (!test_machine_select_rejects_negative_float_call_int_arithmetic_under_extension()) {
        return 1;
    }
    if (!test_machine_select_conservative_no_phi_report_bridge_rejects_phi_input()) {
        return 1;
    }
    if (!test_machine_select_materializes_immediates()) {
        return 1;
    }
    if (!test_machine_select_distinguishes_immediate_store_families()) {
        return 1;
    }
    if (!test_machine_select_cleanup_forwards_tail_copy_into_last_store()) {
        return 1;
    }
    if (!test_machine_select_cleanup_forwards_tail_imm_into_last_alu()) {
        return 1;
    }
    if (!test_machine_select_cleanup_forwards_tail_imm_into_last_call()) {
        return 1;
    }
    if (!test_machine_select_cleanup_forwards_multihop_copy_chain_into_store()) {
        return 1;
    }
    if (!test_machine_select_cleanup_forwards_multihop_imm_chain_into_call()) {
        return 1;
    }
    if (!test_machine_select_cleanup_propagates_cross_block_copy_into_unique_successor()) {
        return 1;
    }
    if (!test_machine_select_cleanup_propagates_cross_block_spill_alias_into_unique_successor()) {
        return 1;
    }
    if (!test_machine_select_cleanup_propagates_must_agree_immediate_at_join()) {
        return 1;
    }
    if (!test_machine_select_cleanup_propagates_must_agree_spill_immediate_at_join()) {
        return 1;
    }
    if (!test_machine_select_cleanup_keeps_disagreeing_join_values()) {
        return 1;
    }
    if (!test_machine_select_cleanup_keeps_disagreeing_spill_join_values()) {
        return 1;
    }
    if (!test_machine_select_cleanup_does_not_propagate_caller_clobbered_copy_through_call()) {
        return 1;
    }
    if (!test_machine_select_cleanup_keeps_callee_preserved_copy_through_call()) {
        return 1;
    }
    if (!test_machine_select_cleanup_keeps_spill_alias_through_call()) {
        return 1;
    }
    if (!test_machine_select_builds_from_machine_ir_report()) {
        return 1;
    }
    if (!test_machine_select_lowers_compare_branch_terminator()) {
        return 1;
    }
    if (!test_machine_select_lowers_compare_branch_immediate_terminator()) {
        return 1;
    }
    if (!test_machine_select_normalizes_compare_branch_lhs_immediate()) {
        return 1;
    }
    if (!test_machine_select_terminator_query_surface()) {
        return 1;
    }
    if (!test_machine_select_folds_constant_branches_to_jump()) {
        return 1;
    }
    if (!test_machine_select_folds_normalized_constant_branches_to_jump()) {
        return 1;
    }
    if (!test_machine_select_folds_constant_compare_branches_to_jump()) {
        return 1;
    }
    if (!test_machine_select_folds_materialized_boolean_branches_to_jump()) {
        return 1;
    }
    if (!test_machine_select_folds_normalized_materialized_boolean_branches_to_jump()) {
        return 1;
    }
    if (!test_machine_select_keeps_compare_result_when_live_out_of_block()) {
        return 1;
    }
    if (!test_machine_select_keeps_compare_result_when_live_out_via_store_indirect()) {
        return 1;
    }
    if (!test_machine_select_cleanup_fuses_branch_with_adjacent_compare_after_selected_copy_forward()) {
        return 1;
    }
    if (!test_machine_select_cleanup_removes_dead_load_before_folded_branch()) {
        return 1;
    }
    if (!test_machine_select_cleanup_propagates_multiconsumer_immediate()) {
        return 1;
    }
    if (!test_machine_select_cleanup_normalizes_folded_branch_immediate()) {
        return 1;
    }
    if (!test_machine_select_cleanup_removes_dead_cross_block_register_def_redefined_in_successor()) {
        return 1;
    }
    if (!test_machine_select_cleanup_removes_dead_cross_block_spill_def_redefined_in_successor()) {
        return 1;
    }
    if (!test_machine_select_cleanup_keeps_cross_block_register_def_when_only_one_successor_path_redefines()) {
        return 1;
    }
    if (!test_machine_select_cleanup_keeps_cross_block_spill_def_when_only_one_successor_path_redefines()) {
        return 1;
    }
    if (!test_machine_select_cleanup_keeps_register_def_live_across_partial_call_clobber_path()) {
        return 1;
    }
    if (!test_machine_select_cleanup_keeps_spill_alias_through_partial_call_path()) {
        return 1;
    }
    if (!test_machine_select_cleanup_removes_register_def_when_successor_paths_call_or_redefine_before_use()) {
        return 1;
    }
    if (!test_machine_select_cleanup_keeps_spill_def_live_when_successor_paths_call_or_redefine_before_use()) {
        return 1;
    }
    if (!test_machine_select_cleanup_keeps_spill_def_when_register_def_is_dead_in_same_predecessor()) {
        return 1;
    }
    if (!test_machine_select_cleanup_keeps_spill_live_while_dropping_dead_register_in_same_predecessor()) {
        return 1;
    }
    if (!test_machine_select_lowers_value_call_with_immediate_arg_family()) {
        return 1;
    }
    if (!test_machine_select_lowers_spill_result_call_families()) {
        return 1;
    }
    if (!test_machine_select_distinguishes_void_calls()) {
        return 1;
    }
    if (!test_machine_select_builds_report_artifact()) {
        return 1;
    }
    if (!test_machine_select_rejects_riscv32_preview_incompatible_register_bank()) {
        return 1;
    }
    if (!test_machine_select_rejects_riscv32_preview_bytes_incompatible_branch_range()) {
        return 1;
    }
    if (!test_machine_select_report_refresh_surface()) {
        return 1;
    }
    if (!test_machine_select_report_query_surface()) {
        return 1;
    }
    if (!test_machine_select_report_block_filter_surface()) {
        return 1;
    }
    if (!test_machine_select_report_dump_surface()) {
        return 1;
    }
    if (!test_machine_select_canonicalized_report_surface()) {
        return 1;
    }
    if (!test_machine_select_builds_canonicalized_flat_report_from_value_ssa()) {
        return 1;
    }
    if (!test_machine_select_report_query_dump_reject_missing_function_table()) {
        return 1;
    }
    if (!test_machine_select_reuses_repeated_internal_pure_call_in_indirect_block()) {
        return 1;
    }
    if (!test_machine_select_reuses_repeated_internal_pure_call_across_unrelated_store_indirect()) {
        return 1;
    }
    if (!test_machine_select_keeps_repeated_internal_pure_call_across_global_write_indirect()) {
        return 1;
    }
    if (!test_machine_select_reuses_repeated_internal_pure_call_with_live_in_spill_args()) {
        return 1;
    }
    if (!test_machine_select_reuses_repeated_internal_pure_call_from_unique_predecessor()) {
        return 1;
    }
    if (!test_machine_select_reuses_repeated_internal_pure_call_with_same_block_indirect_load_arg()) {
        return 1;
    }
    if (!test_machine_select_forwards_same_block_indirect_load_across_non_alias_store()) {
        return 1;
    }
    if (!test_machine_select_does_not_forward_indirect_load_across_aliasing_sibling_local_store()) {
        return 1;
    }
    if (!test_machine_select_does_not_force_repeated_indirect_load_through_new_spill_when_result_feeds_shift_chain()) {
        return 1;
    }
    if (!test_machine_select_exposes_mm_like_row_base_address_pattern()) {
        return 1;
    }
    if (!test_machine_select_forwards_same_block_local_load_without_intervening_store()) {
        return 1;
    }
    if (!test_machine_select_keeps_same_address_read_modify_write_shape_compact()) {
        return 1;
    }
    if (!test_machine_select_does_not_forward_same_block_local_load_across_store_local()) {
        return 1;
    }
    if (!test_machine_select_reuses_repeated_spill_pure_expr()) {
        return 1;
    }
    if (!test_machine_select_does_not_reuse_spill_pure_expr_across_redefined_operand()) {
        return 1;
    }
    if (!test_machine_select_reuses_repeated_register_pure_expr()) {
        return 1;
    }
    if (!test_machine_select_does_not_reuse_register_pure_expr_across_redefined_operand()) {
        return 1;
    }
    if (!test_machine_select_reuses_commutative_register_pure_expr()) {
        return 1;
    }
    if (!test_machine_select_reuses_commutative_register_expr_after_shared_shift()) {
        return 1;
    }
    printf("[machine-select] PASS\n");
    return 0;
}
