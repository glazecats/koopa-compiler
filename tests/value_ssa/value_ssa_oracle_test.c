#include "value_ssa.h"
#include "value_ssa_interp.h"
#include "value_ssa_pass.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    VALUE_SSA_ORACLE_LOCAL_CHAIN = 0,
    VALUE_SSA_ORACLE_GLOBAL_CALL_CHAIN,
    VALUE_SSA_ORACLE_LOCAL_DIAMOND,
    VALUE_SSA_ORACLE_GLOBAL_DIAMOND,
    VALUE_SSA_ORACLE_MIXED_HYBRID,
} ValueSsaOracleProgramKind;

typedef struct {
    ValueSsaOracleProgramKind kind;
    size_t chain_length;
    size_t post_call_hops;
    int repeated_seed_store;
    int duplicate_final_store;
    int duplicate_then_store;
    int duplicate_else_store;
    long long seed_value;
    long long final_value;
    long long then_value;
    long long else_value;
} ValueSsaOracleCase;

typedef int (*ValueSsaOracleBuilderFn)(const ValueSsaOracleCase *spec,
    ValueSsaProgram *program,
    ValueSsaError *error);

typedef int (*ValueSsaOracleRawBuilderFn)(ValueSsaProgram *program, ValueSsaError *error);

static int value_ssa_oracle_append_blocks(ValueSsaFunction *function,
    size_t block_count,
    size_t *out_ids,
    ValueSsaError *error) {
    size_t block_index;

    if (!function || !out_ids) {
        return 0;
    }

    for (block_index = 0; block_index < block_count; ++block_index) {
        if (!value_ssa_function_append_block(function, &out_ids[block_index], NULL, error)) {
            return 0;
        }
    }

    return 1;
}

static int build_local_chain_program(const ValueSsaOracleCase *spec,
    ValueSsaProgram *program,
    ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaInstruction instruction;
    size_t local_id;
    size_t load_value;
    size_t *block_ids = NULL;
    size_t block_index;
    size_t block_count;
    int ok = 0;

    if (!spec || !program || spec->chain_length == 0) {
        return 0;
    }

    value_ssa_program_init(program);
    block_count = spec->chain_length + 1;
    block_ids = (size_t *)malloc(block_count * sizeof(size_t));
    if (!block_ids) {
        return 0;
    }

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "x", 0, &local_id, error) ||
        !value_ssa_oracle_append_blocks(function, block_count, block_ids, error)) {
        goto cleanup;
    }

    load_value = value_ssa_function_allocate_value(function);
    if (load_value == (size_t)-1) {
        goto cleanup;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(local_id);
    instruction.as.store.value = value_ssa_value_immediate(spec->seed_value);
    if (!value_ssa_block_append_instruction(&function->blocks[block_ids[0]], &instruction, error) ||
        !value_ssa_block_set_jump(&function->blocks[block_ids[0]], block_ids[1], error)) {
        goto cleanup;
    }

    for (block_index = 1; block_index < spec->chain_length; ++block_index) {
        if (spec->repeated_seed_store && (block_index % 2u) == 1u &&
            !value_ssa_block_append_instruction(&function->blocks[block_ids[block_index]], &instruction, error)) {
            goto cleanup;
        }
        if (!value_ssa_block_set_jump(&function->blocks[block_ids[block_index]],
                block_ids[block_index + 1],
                error)) {
            goto cleanup;
        }
    }

    instruction.as.store.value = value_ssa_value_immediate(spec->final_value);
    if (!value_ssa_block_append_instruction(&function->blocks[block_ids[spec->chain_length]], &instruction, error)) {
        goto cleanup;
    }
    if (spec->duplicate_final_store &&
        !value_ssa_block_append_instruction(&function->blocks[block_ids[spec->chain_length]], &instruction, error)) {
        goto cleanup;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(load_value);
    instruction.as.load_slot = value_ssa_slot_local(local_id);
    if (!value_ssa_block_append_instruction(&function->blocks[block_ids[spec->chain_length]], &instruction, error) ||
        !value_ssa_block_set_return(&function->blocks[block_ids[spec->chain_length]],
            value_ssa_value_id(load_value),
            error)) {
        goto cleanup;
    }

    ok = 1;

cleanup:
    if (!ok) {
        value_ssa_program_free(program);
    }
    free(block_ids);
    return ok;
}

static int build_global_call_chain_program(const ValueSsaOracleCase *spec,
    ValueSsaProgram *program,
    ValueSsaError *error) {
    ValueSsaGlobal *global = NULL;
    ValueSsaFunction *decl = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaInstruction instruction;
    size_t load_value;
    size_t *block_ids = NULL;
    size_t block_index;
    size_t block_count;
    size_t call_block_index;
    size_t final_block_index;
    int ok = 0;

    if (!spec || !program || spec->chain_length == 0) {
        return 0;
    }

    value_ssa_program_init(program);
    block_count = spec->chain_length + spec->post_call_hops + 2;
    block_ids = (size_t *)malloc(block_count * sizeof(size_t));
    if (!block_ids) {
        return 0;
    }

    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "touch", 0, &decl, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_oracle_append_blocks(function, block_count, block_ids, error)) {
        goto cleanup;
    }

    load_value = value_ssa_function_allocate_value(function);
    if (load_value == (size_t)-1) {
        goto cleanup;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = value_ssa_slot_global(global->id);
    instruction.as.store.value = value_ssa_value_immediate(spec->seed_value);
    if (!value_ssa_block_append_instruction(&function->blocks[block_ids[0]], &instruction, error)) {
        goto cleanup;
    }

    for (block_index = 0; block_index + 1 < spec->chain_length; ++block_index) {
        if (block_index > 0 && spec->repeated_seed_store && (block_index % 2u) == 1u &&
            !value_ssa_block_append_instruction(&function->blocks[block_ids[block_index]], &instruction, error)) {
            goto cleanup;
        }
        if (!value_ssa_block_set_jump(&function->blocks[block_ids[block_index]], block_ids[block_index + 1], error)) {
            goto cleanup;
        }
    }

    if (!value_ssa_block_set_jump(&function->blocks[block_ids[spec->chain_length - 1]],
            block_ids[spec->chain_length],
            error)) {
        goto cleanup;
    }

    call_block_index = spec->chain_length;
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_CALL;
    instruction.as.call.callee_name = "touch";
    instruction.as.call.args = NULL;
    instruction.as.call.arg_count = 0;
    if (!value_ssa_block_append_instruction(&function->blocks[block_ids[call_block_index]], &instruction, error)) {
        goto cleanup;
    }

    if (spec->post_call_hops == 0) {
        if (!value_ssa_block_set_jump(&function->blocks[block_ids[call_block_index]],
                block_ids[call_block_index + 1],
                error)) {
            goto cleanup;
        }
    } else {
        if (!value_ssa_block_set_jump(&function->blocks[block_ids[call_block_index]],
                block_ids[call_block_index + 1],
                error)) {
            goto cleanup;
        }
        for (block_index = call_block_index + 1; block_index < call_block_index + spec->post_call_hops; ++block_index) {
            if (!value_ssa_block_set_jump(&function->blocks[block_ids[block_index]],
                    block_ids[block_index + 1],
                    error)) {
                goto cleanup;
            }
        }
        if (!value_ssa_block_set_jump(&function->blocks[block_ids[call_block_index + spec->post_call_hops]],
                block_ids[call_block_index + spec->post_call_hops + 1],
                error)) {
            goto cleanup;
        }
    }

    final_block_index = block_count - 1;
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = value_ssa_slot_global(global->id);
    instruction.as.store.value = value_ssa_value_immediate(spec->final_value);
    if (!value_ssa_block_append_instruction(&function->blocks[block_ids[final_block_index]], &instruction, error)) {
        goto cleanup;
    }
    if (spec->duplicate_final_store &&
        !value_ssa_block_append_instruction(&function->blocks[block_ids[final_block_index]], &instruction, error)) {
        goto cleanup;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(load_value);
    instruction.as.load_slot = value_ssa_slot_global(global->id);
    if (!value_ssa_block_append_instruction(&function->blocks[block_ids[final_block_index]], &instruction, error) ||
        !value_ssa_block_set_return(&function->blocks[block_ids[final_block_index]],
            value_ssa_value_id(load_value),
            error)) {
        goto cleanup;
    }

    ok = 1;

cleanup:
    if (!ok) {
        value_ssa_program_free(program);
    }
    free(block_ids);
    return ok;
}

static int build_local_diamond_program(const ValueSsaOracleCase *spec,
    ValueSsaProgram *program,
    ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaInstruction instruction;
    size_t cond_local_id;
    size_t slot_local_id;
    size_t cond_value;
    size_t load_value;
    size_t block_ids[4];
    int ok = 0;

    if (!spec || !program) {
        return 0;
    }

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "cond", 1, &cond_local_id, error) ||
        !value_ssa_function_append_local(function, "x", 0, &slot_local_id, error) ||
        !value_ssa_oracle_append_blocks(function, 4, block_ids, error)) {
        goto cleanup;
    }

    cond_value = value_ssa_function_allocate_value(function);
    load_value = value_ssa_function_allocate_value(function);
    if (cond_value == (size_t)-1 || load_value == (size_t)-1) {
        goto cleanup;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_value);
    instruction.as.load_slot = value_ssa_slot_local(cond_local_id);
    if (!value_ssa_block_append_instruction(&function->blocks[block_ids[0]], &instruction, error) ||
        !value_ssa_block_set_branch(&function->blocks[block_ids[0]],
            value_ssa_value_id(cond_value),
            block_ids[1],
            block_ids[2],
            error)) {
        goto cleanup;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(slot_local_id);
    instruction.as.store.value = value_ssa_value_immediate(spec->then_value);
    if (!value_ssa_block_append_instruction(&function->blocks[block_ids[1]], &instruction, error)) {
        goto cleanup;
    }
    if (spec->duplicate_then_store &&
        !value_ssa_block_append_instruction(&function->blocks[block_ids[1]], &instruction, error)) {
        goto cleanup;
    }
    if (!value_ssa_block_set_jump(&function->blocks[block_ids[1]], block_ids[3], error)) {
        goto cleanup;
    }

    instruction.as.store.value = value_ssa_value_immediate(spec->else_value);
    if (!value_ssa_block_append_instruction(&function->blocks[block_ids[2]], &instruction, error)) {
        goto cleanup;
    }
    if (spec->duplicate_else_store &&
        !value_ssa_block_append_instruction(&function->blocks[block_ids[2]], &instruction, error)) {
        goto cleanup;
    }
    if (!value_ssa_block_set_jump(&function->blocks[block_ids[2]], block_ids[3], error)) {
        goto cleanup;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(load_value);
    instruction.as.load_slot = value_ssa_slot_local(slot_local_id);
    if (!value_ssa_block_append_instruction(&function->blocks[block_ids[3]], &instruction, error) ||
        !value_ssa_block_set_return(&function->blocks[block_ids[3]], value_ssa_value_id(load_value), error)) {
        goto cleanup;
    }

    ok = 1;

cleanup:
    if (!ok) {
        value_ssa_program_free(program);
    }
    return ok;
}

static int build_global_diamond_program(const ValueSsaOracleCase *spec,
    ValueSsaProgram *program,
    ValueSsaError *error) {
    ValueSsaGlobal *global = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaInstruction instruction;
    size_t cond_local_id;
    size_t cond_value;
    size_t load_value;
    size_t block_ids[4];
    int ok = 0;

    if (!spec || !program) {
        return 0;
    }

    value_ssa_program_init(program);
    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "cond", 1, &cond_local_id, error) ||
        !value_ssa_oracle_append_blocks(function, 4, block_ids, error)) {
        goto cleanup;
    }

    cond_value = value_ssa_function_allocate_value(function);
    load_value = value_ssa_function_allocate_value(function);
    if (cond_value == (size_t)-1 || load_value == (size_t)-1) {
        goto cleanup;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_value);
    instruction.as.load_slot = value_ssa_slot_local(cond_local_id);
    if (!value_ssa_block_append_instruction(&function->blocks[block_ids[0]], &instruction, error) ||
        !value_ssa_block_set_branch(&function->blocks[block_ids[0]],
            value_ssa_value_id(cond_value),
            block_ids[1],
            block_ids[2],
            error)) {
        goto cleanup;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = value_ssa_slot_global(global->id);
    instruction.as.store.value = value_ssa_value_immediate(spec->then_value);
    if (!value_ssa_block_append_instruction(&function->blocks[block_ids[1]], &instruction, error)) {
        goto cleanup;
    }
    if (spec->duplicate_then_store &&
        !value_ssa_block_append_instruction(&function->blocks[block_ids[1]], &instruction, error)) {
        goto cleanup;
    }
    if (!value_ssa_block_set_jump(&function->blocks[block_ids[1]], block_ids[3], error)) {
        goto cleanup;
    }

    instruction.as.store.value = value_ssa_value_immediate(spec->else_value);
    if (!value_ssa_block_append_instruction(&function->blocks[block_ids[2]], &instruction, error)) {
        goto cleanup;
    }
    if (spec->duplicate_else_store &&
        !value_ssa_block_append_instruction(&function->blocks[block_ids[2]], &instruction, error)) {
        goto cleanup;
    }
    if (!value_ssa_block_set_jump(&function->blocks[block_ids[2]], block_ids[3], error)) {
        goto cleanup;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(load_value);
    instruction.as.load_slot = value_ssa_slot_global(global->id);
    if (!value_ssa_block_append_instruction(&function->blocks[block_ids[3]], &instruction, error) ||
        !value_ssa_block_set_return(&function->blocks[block_ids[3]], value_ssa_value_id(load_value), error)) {
        goto cleanup;
    }

    ok = 1;

cleanup:
    if (!ok) {
        value_ssa_program_free(program);
    }
    return ok;
}

static int build_mixed_hybrid_program(const ValueSsaOracleCase *spec,
    ValueSsaProgram *program,
    ValueSsaError *error) {
    ValueSsaGlobal *global = NULL;
    ValueSsaFunction *decl = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaInstruction instruction;
    size_t cond_local_id;
    size_t slot_local_id;
    size_t cond_value;
    size_t local_load_value;
    size_t global_load_value;
    size_t local_norm_value;
    size_t global_norm_value;
    size_t sum_value;
    size_t result_value;
    size_t result_copy_value = (size_t)-1;
    size_t *block_ids = NULL;
    size_t block_count;
    size_t block_index;
    size_t branch_block_index;
    size_t then_block_index;
    size_t else_block_index;
    size_t join_block_index;
    size_t final_block_index;
    int ok = 0;

    if (!spec || !program || spec->chain_length == 0) {
        return 0;
    }

    value_ssa_program_init(program);
    block_count = spec->chain_length + spec->post_call_hops + 5;
    block_ids = (size_t *)malloc(block_count * sizeof(size_t));
    if (!block_ids) {
        return 0;
    }

    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "touch", 0, &decl, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "cond", 1, &cond_local_id, error) ||
        !value_ssa_function_append_local(function, "x", 0, &slot_local_id, error) ||
        !value_ssa_oracle_append_blocks(function, block_count, block_ids, error)) {
        goto cleanup;
    }

    cond_value = value_ssa_function_allocate_value(function);
    local_load_value = value_ssa_function_allocate_value(function);
    global_load_value = value_ssa_function_allocate_value(function);
    local_norm_value = value_ssa_function_allocate_value(function);
    global_norm_value = value_ssa_function_allocate_value(function);
    sum_value = value_ssa_function_allocate_value(function);
    result_value = value_ssa_function_allocate_value(function);
    if (cond_value == (size_t)-1 || local_load_value == (size_t)-1 || global_load_value == (size_t)-1 ||
        local_norm_value == (size_t)-1 || global_norm_value == (size_t)-1 || sum_value == (size_t)-1 ||
        result_value == (size_t)-1) {
        goto cleanup;
    }

    branch_block_index = spec->chain_length;
    then_block_index = branch_block_index + 1;
    else_block_index = branch_block_index + 2;
    join_block_index = branch_block_index + 3;
    final_block_index = block_count - 1;

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(slot_local_id);
    instruction.as.store.value = value_ssa_value_immediate(spec->seed_value);
    if (!value_ssa_block_append_instruction(&function->blocks[block_ids[0]], &instruction, error)) {
        goto cleanup;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = value_ssa_slot_global(global->id);
    instruction.as.store.value = value_ssa_value_immediate(spec->seed_value + 1);
    if (!value_ssa_block_append_instruction(&function->blocks[block_ids[0]], &instruction, error) ||
        !value_ssa_block_set_jump(&function->blocks[block_ids[0]], block_ids[1], error)) {
        goto cleanup;
    }

    for (block_index = 1; block_index < branch_block_index; ++block_index) {
        if (spec->repeated_seed_store && (block_index % 2u) == 1u) {
            memset(&instruction, 0, sizeof(instruction));
            instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
            instruction.as.store.slot = value_ssa_slot_local(slot_local_id);
            instruction.as.store.value = value_ssa_value_immediate(spec->seed_value);
            if (!value_ssa_block_append_instruction(&function->blocks[block_ids[block_index]], &instruction, error)) {
                goto cleanup;
            }

            memset(&instruction, 0, sizeof(instruction));
            instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
            instruction.as.store.slot = value_ssa_slot_global(global->id);
            instruction.as.store.value = value_ssa_value_immediate(spec->seed_value + 1);
            if (!value_ssa_block_append_instruction(&function->blocks[block_ids[block_index]], &instruction, error)) {
                goto cleanup;
            }
        }

        if (!value_ssa_block_set_jump(&function->blocks[block_ids[block_index]], block_ids[block_index + 1], error)) {
            goto cleanup;
        }
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_value);
    instruction.as.load_slot = value_ssa_slot_local(cond_local_id);
    if (!value_ssa_block_append_instruction(&function->blocks[block_ids[branch_block_index]], &instruction, error) ||
        !value_ssa_block_set_branch(&function->blocks[block_ids[branch_block_index]],
            value_ssa_value_id(cond_value),
            block_ids[then_block_index],
            block_ids[else_block_index],
            error)) {
        goto cleanup;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(slot_local_id);
    instruction.as.store.value = value_ssa_value_immediate(spec->then_value);
    if (!value_ssa_block_append_instruction(&function->blocks[block_ids[then_block_index]], &instruction, error)) {
        goto cleanup;
    }
    if (spec->duplicate_then_store &&
        !value_ssa_block_append_instruction(&function->blocks[block_ids[then_block_index]], &instruction, error)) {
        goto cleanup;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = value_ssa_slot_global(global->id);
    instruction.as.store.value = value_ssa_value_immediate(spec->then_value + 10);
    if (!value_ssa_block_append_instruction(&function->blocks[block_ids[then_block_index]], &instruction, error)) {
        goto cleanup;
    }
    if (spec->duplicate_then_store &&
        !value_ssa_block_append_instruction(&function->blocks[block_ids[then_block_index]], &instruction, error)) {
        goto cleanup;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_CALL;
    instruction.as.call.callee_name = "touch";
    instruction.as.call.args = NULL;
    instruction.as.call.arg_count = 0;
    if (!value_ssa_block_append_instruction(&function->blocks[block_ids[then_block_index]], &instruction, error) ||
        !value_ssa_block_set_jump(&function->blocks[block_ids[then_block_index]], block_ids[join_block_index], error)) {
        goto cleanup;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = value_ssa_slot_global(global->id);
    instruction.as.store.value = value_ssa_value_immediate(spec->else_value + 10);
    if (!value_ssa_block_append_instruction(&function->blocks[block_ids[else_block_index]], &instruction, error)) {
        goto cleanup;
    }
    if (spec->duplicate_else_store &&
        !value_ssa_block_append_instruction(&function->blocks[block_ids[else_block_index]], &instruction, error)) {
        goto cleanup;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(slot_local_id);
    instruction.as.store.value = value_ssa_value_immediate(spec->else_value);
    if (!value_ssa_block_append_instruction(&function->blocks[block_ids[else_block_index]], &instruction, error)) {
        goto cleanup;
    }
    if (spec->duplicate_else_store &&
        !value_ssa_block_append_instruction(&function->blocks[block_ids[else_block_index]], &instruction, error)) {
        goto cleanup;
    }
    if (!value_ssa_block_set_jump(&function->blocks[block_ids[else_block_index]], block_ids[join_block_index], error)) {
        goto cleanup;
    }

    if (spec->post_call_hops == 0) {
        if (!value_ssa_block_set_jump(&function->blocks[block_ids[join_block_index]], block_ids[final_block_index], error)) {
            goto cleanup;
        }
    } else {
        if (!value_ssa_block_set_jump(&function->blocks[block_ids[join_block_index]], block_ids[join_block_index + 1], error)) {
            goto cleanup;
        }
        for (block_index = join_block_index + 1; block_index < final_block_index; ++block_index) {
            if (!value_ssa_block_set_jump(&function->blocks[block_ids[block_index]], block_ids[block_index + 1], error)) {
                goto cleanup;
            }
        }
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(local_load_value);
    instruction.as.load_slot = value_ssa_slot_local(slot_local_id);
    if (!value_ssa_block_append_instruction(&function->blocks[block_ids[final_block_index]], &instruction, error)) {
        goto cleanup;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(global_load_value);
    instruction.as.load_slot = value_ssa_slot_global(global->id);
    if (!value_ssa_block_append_instruction(&function->blocks[block_ids[final_block_index]], &instruction, error)) {
        goto cleanup;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(local_norm_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(local_load_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(0);
    if (!value_ssa_block_append_instruction(&function->blocks[block_ids[final_block_index]], &instruction, error)) {
        goto cleanup;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(global_norm_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_immediate(0);
    instruction.as.binary.rhs = value_ssa_value_id(global_load_value);
    if (!value_ssa_block_append_instruction(&function->blocks[block_ids[final_block_index]], &instruction, error)) {
        goto cleanup;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(sum_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(local_norm_value);
    instruction.as.binary.rhs = value_ssa_value_id(global_norm_value);
    if (!value_ssa_block_append_instruction(&function->blocks[block_ids[final_block_index]], &instruction, error)) {
        goto cleanup;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(result_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(sum_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(spec->final_value);
    if (!value_ssa_block_append_instruction(&function->blocks[block_ids[final_block_index]], &instruction, error)) {
        goto cleanup;
    }

    if (spec->duplicate_final_store) {
        result_copy_value = value_ssa_function_allocate_value(function);
        if (result_copy_value == (size_t)-1) {
            goto cleanup;
        }

        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = VALUE_SSA_INSTR_MOV;
        instruction.has_result = 1;
        instruction.result = value_ssa_value_id(result_copy_value);
        instruction.as.mov_value = value_ssa_value_id(result_value);
        if (!value_ssa_block_append_instruction(&function->blocks[block_ids[final_block_index]], &instruction, error) ||
            !value_ssa_block_set_return(&function->blocks[block_ids[final_block_index]],
                value_ssa_value_id(result_copy_value),
                error)) {
            goto cleanup;
        }
    } else {
        if (!value_ssa_block_set_return(&function->blocks[block_ids[final_block_index]],
                value_ssa_value_id(result_value),
                error)) {
            goto cleanup;
        }
    }

    ok = 1;

cleanup:
    if (!ok) {
        value_ssa_program_free(program);
    }
    free(block_ids);
    return ok;
}

static int build_scrambled_canonicalize_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaPhiInput phi_inputs[2];
    size_t local_a_id;
    size_t value0;
    size_t value1;
    size_t value2;
    size_t value3;
    size_t value4;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 1, &local_a_id, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &then_block, error) ||
        !value_ssa_function_append_block(function, NULL, &else_block, error) ||
        !value_ssa_function_append_block(function, NULL, &join_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    value0 = value_ssa_function_allocate_value(function);
    value1 = value_ssa_function_allocate_value(function);
    value2 = value_ssa_function_allocate_value(function);
    value3 = value_ssa_function_allocate_value(function);
    value4 = value_ssa_function_allocate_value(function);
    if (value0 == (size_t)-1 || value1 == (size_t)-1 || value2 == (size_t)-1 ||
        value3 == (size_t)-1 || value4 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value4);
    instruction.as.load_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value3);
    instruction.as.mov_value = value_ssa_value_id(value4);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_id(value3), 1, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value2);
    instruction.as.mov_value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error) ||
        !value_ssa_block_set_jump(then_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.mov_value = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(else_block, &instruction, error) ||
        !value_ssa_block_set_jump(else_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    phi_inputs[0].predecessor_block_id = 1;
    phi_inputs[0].value = value_ssa_value_id(value2);
    phi_inputs[1].predecessor_block_id = 2;
    phi_inputs[1].value = value_ssa_value_id(value0);
    if (!value_ssa_block_append_phi(join_block, value1, phi_inputs, 2, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_id(value1), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_dead_global_store_preserves_branch_observer_program(ValueSsaProgram *program,
    ValueSsaError *error) {
    ValueSsaGlobal *global = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaInstruction instruction;
    size_t local_a_id;
    size_t condition_value;
    size_t else_load_value;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 1, &local_a_id, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &then_block, error) ||
        !value_ssa_function_append_block(function, NULL, &else_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    condition_value = value_ssa_function_allocate_value(function);
    else_load_value = value_ssa_function_allocate_value(function);
    if (condition_value == (size_t)-1 || else_load_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = value_ssa_slot_global(global->id);
    instruction.as.store.value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(condition_value);
    instruction.as.load_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_id(condition_value), 1, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = value_ssa_slot_global(global->id);
    instruction.as.store.value = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error) ||
        !value_ssa_block_set_return(then_block, value_ssa_value_immediate(0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(else_load_value);
    instruction.as.load_slot = value_ssa_slot_global(global->id);
    if (!value_ssa_block_append_instruction(else_block, &instruction, error) ||
        !value_ssa_block_set_return(else_block, value_ssa_value_id(else_load_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_canonicalize_complex_local_chain_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaInstruction instruction;
    size_t local_a_id;
    size_t load_value;
    size_t bb0_id;
    size_t bb1_id;
    size_t bb2_id;
    size_t bb3_id;
    size_t bb4_id;
    size_t bb5_id;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 1, &local_a_id, error) ||
        !value_ssa_function_append_block(function, &bb0_id, NULL, error) ||
        !value_ssa_function_append_block(function, &bb1_id, NULL, error) ||
        !value_ssa_function_append_block(function, &bb2_id, NULL, error) ||
        !value_ssa_function_append_block(function, &bb3_id, NULL, error) ||
        !value_ssa_function_append_block(function, &bb4_id, NULL, error) ||
        !value_ssa_function_append_block(function, &bb5_id, NULL, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    load_value = value_ssa_function_allocate_value(function);
    if (load_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(local_a_id);
    instruction.as.store.value = value_ssa_value_immediate(7);
    if (!value_ssa_block_append_instruction(&function->blocks[bb0_id], &instruction, error) ||
        !value_ssa_block_set_jump(&function->blocks[bb0_id], bb1_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    if (!value_ssa_block_append_instruction(&function->blocks[bb1_id], &instruction, error) ||
        !value_ssa_block_set_jump(&function->blocks[bb1_id], bb2_id, error) ||
        !value_ssa_block_set_jump(&function->blocks[bb2_id], bb3_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.as.store.value = value_ssa_value_immediate(8);
    if (!value_ssa_block_append_instruction(&function->blocks[bb3_id], &instruction, error) ||
        !value_ssa_block_set_jump(&function->blocks[bb3_id], bb4_id, error) ||
        !value_ssa_block_set_jump(&function->blocks[bb4_id], bb5_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(load_value);
    instruction.as.load_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(&function->blocks[bb5_id], &instruction, error) ||
        !value_ssa_block_set_return(&function->blocks[bb5_id], value_ssa_value_id(load_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_canonicalize_complex_global_call_chain_program(ValueSsaProgram *program,
    ValueSsaError *error) {
    ValueSsaGlobal *global = NULL;
    ValueSsaFunction *decl = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaInstruction instruction;
    size_t load_value;
    size_t bb0_id;
    size_t bb1_id;
    size_t bb2_id;
    size_t bb3_id;
    size_t bb4_id;
    size_t bb5_id;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "touch", 0, &decl, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, &bb0_id, NULL, error) ||
        !value_ssa_function_append_block(function, &bb1_id, NULL, error) ||
        !value_ssa_function_append_block(function, &bb2_id, NULL, error) ||
        !value_ssa_function_append_block(function, &bb3_id, NULL, error) ||
        !value_ssa_function_append_block(function, &bb4_id, NULL, error) ||
        !value_ssa_function_append_block(function, &bb5_id, NULL, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    load_value = value_ssa_function_allocate_value(function);
    if (load_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = value_ssa_slot_global(global->id);
    instruction.as.store.value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(&function->blocks[bb0_id], &instruction, error) ||
        !value_ssa_block_set_jump(&function->blocks[bb0_id], bb1_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    if (!value_ssa_block_append_instruction(&function->blocks[bb1_id], &instruction, error) ||
        !value_ssa_block_set_jump(&function->blocks[bb1_id], bb2_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_CALL;
    instruction.as.call.callee_name = "touch";
    instruction.as.call.args = NULL;
    instruction.as.call.arg_count = 0;
    if (!value_ssa_block_append_instruction(&function->blocks[bb2_id], &instruction, error) ||
        !value_ssa_block_set_jump(&function->blocks[bb2_id], bb3_id, error) ||
        !value_ssa_block_set_jump(&function->blocks[bb3_id], bb4_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = value_ssa_slot_global(global->id);
    instruction.as.store.value = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(&function->blocks[bb4_id], &instruction, error) ||
        !value_ssa_block_set_jump(&function->blocks[bb4_id], bb5_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(load_value);
    instruction.as.load_slot = value_ssa_slot_global(global->id);
    if (!value_ssa_block_append_instruction(&function->blocks[bb5_id], &instruction, error) ||
        !value_ssa_block_set_return(&function->blocks[bb5_id], value_ssa_value_id(load_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int value_ssa_oracle_noop_extern(const ValueSsaProgram *program,
    const char *callee_name,
    const long long *args,
    size_t arg_count,
    int has_result,
    long long *out_result,
    long long *global_values,
    size_t global_count,
    void *user_data,
    ValueSsaInterpError *error) {
    (void)program;
    (void)args;
    (void)arg_count;
    (void)global_values;
    (void)global_count;
    (void)user_data;
    (void)error;

    if (!callee_name || strcmp(callee_name, "touch") != 0) {
        return 0;
    }
    if (has_result && out_result) {
        *out_result = 0;
    }
    return 1;
}

static int value_ssa_oracle_compare_before_after(const char *case_id,
    ValueSsaOracleBuilderFn builder,
    const ValueSsaOracleCase *spec,
    const long long *args,
    size_t arg_count,
    const ValueSsaInterpOptions *options) {
    ValueSsaProgram before_program;
    ValueSsaProgram after_program;
    ValueSsaError build_error;
    ValueSsaError canonicalize_error;
    ValueSsaInterpResult before_result;
    ValueSsaInterpResult after_result;
    ValueSsaInterpError interp_error;
    size_t global_index;
    int ok = 1;

    if (!case_id || !builder || !spec) {
        return 0;
    }

    if (!builder(spec, &before_program, &build_error) || !builder(spec, &after_program, &build_error)) {
        fprintf(stderr,
            "[value-ssa-oracle] FAIL: %s setup failed at %d:%d: %s\n",
            case_id,
            build_error.line,
            build_error.column,
            build_error.message);
        return 0;
    }

    value_ssa_interp_result_init(&before_result);
    value_ssa_interp_result_init(&after_result);

    if (!value_ssa_interp_execute_main(&before_program, args, arg_count, options, &before_result, &interp_error)) {
        fprintf(stderr,
            "[value-ssa-oracle] FAIL: %s pre-canonicalize interpretation failed: %s\n",
            case_id,
            interp_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!value_ssa_canonicalize_program(&after_program, &canonicalize_error)) {
        fprintf(stderr,
            "[value-ssa-oracle] FAIL: %s canonicalize failed at %d:%d: %s\n",
            case_id,
            canonicalize_error.line,
            canonicalize_error.column,
            canonicalize_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!value_ssa_interp_execute_main(&after_program, args, arg_count, options, &after_result, &interp_error)) {
        fprintf(stderr,
            "[value-ssa-oracle] FAIL: %s post-canonicalize interpretation failed: %s\n",
            case_id,
            interp_error.message);
        ok = 0;
        goto cleanup;
    }

    if (before_result.return_value != after_result.return_value ||
        before_result.global_count != after_result.global_count) {
        fprintf(stderr,
            "[value-ssa-oracle] FAIL: %s return/global-count mismatch before/after canonicalize\n",
            case_id);
        ok = 0;
        goto cleanup;
    }

    for (global_index = 0; global_index < before_result.global_count; ++global_index) {
        if (before_result.global_values[global_index] != after_result.global_values[global_index]) {
            fprintf(stderr,
                "[value-ssa-oracle] FAIL: %s g.%zu mismatch before/after canonicalize (%lld vs %lld)\n",
                case_id,
                global_index,
                before_result.global_values[global_index],
                after_result.global_values[global_index]);
            ok = 0;
            goto cleanup;
        }
    }

cleanup:
    value_ssa_interp_result_free(&before_result);
    value_ssa_interp_result_free(&after_result);
    value_ssa_program_free(&before_program);
    value_ssa_program_free(&after_program);
    return ok;
}

static int value_ssa_oracle_compare_before_after_raw(const char *case_id,
    ValueSsaOracleRawBuilderFn builder,
    const long long *args,
    size_t arg_count,
    const ValueSsaInterpOptions *options) {
    ValueSsaProgram before_program;
    ValueSsaProgram after_program;
    ValueSsaError build_error;
    ValueSsaError canonicalize_error;
    ValueSsaInterpResult before_result;
    ValueSsaInterpResult after_result;
    ValueSsaInterpError interp_error;
    size_t global_index;
    int ok = 1;

    if (!case_id || !builder) {
        return 0;
    }

    if (!builder(&before_program, &build_error) || !builder(&after_program, &build_error)) {
        fprintf(stderr,
            "[value-ssa-oracle] FAIL: %s setup failed at %d:%d: %s\n",
            case_id,
            build_error.line,
            build_error.column,
            build_error.message);
        return 0;
    }

    value_ssa_interp_result_init(&before_result);
    value_ssa_interp_result_init(&after_result);

    if (!value_ssa_interp_execute_main(&before_program, args, arg_count, options, &before_result, &interp_error)) {
        fprintf(stderr,
            "[value-ssa-oracle] FAIL: %s pre-canonicalize interpretation failed: %s\n",
            case_id,
            interp_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!value_ssa_canonicalize_program(&after_program, &canonicalize_error)) {
        fprintf(stderr,
            "[value-ssa-oracle] FAIL: %s canonicalize failed at %d:%d: %s\n",
            case_id,
            canonicalize_error.line,
            canonicalize_error.column,
            canonicalize_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!value_ssa_interp_execute_main(&after_program, args, arg_count, options, &after_result, &interp_error)) {
        fprintf(stderr,
            "[value-ssa-oracle] FAIL: %s post-canonicalize interpretation failed: %s\n",
            case_id,
            interp_error.message);
        ok = 0;
        goto cleanup;
    }

    if (before_result.return_value != after_result.return_value ||
        before_result.global_count != after_result.global_count) {
        fprintf(stderr,
            "[value-ssa-oracle] FAIL: %s return/global-count mismatch before/after canonicalize\n",
            case_id);
        ok = 0;
        goto cleanup;
    }

    for (global_index = 0; global_index < before_result.global_count; ++global_index) {
        if (before_result.global_values[global_index] != after_result.global_values[global_index]) {
            fprintf(stderr,
                "[value-ssa-oracle] FAIL: %s g.%zu mismatch before/after canonicalize (%lld vs %lld)\n",
                case_id,
                global_index,
                before_result.global_values[global_index],
                after_result.global_values[global_index]);
            ok = 0;
            goto cleanup;
        }
    }

cleanup:
    value_ssa_interp_result_free(&before_result);
    value_ssa_interp_result_free(&after_result);
    value_ssa_program_free(&before_program);
    value_ssa_program_free(&after_program);
    return ok;
}

static int test_value_ssa_oracle_local_chain_matrix(void) {
    size_t chain_length;
    int repeated_seed_store;
    int duplicate_final_store;
    long long seed_values[] = {1, 7};
    long long final_values[] = {2, 8};
    size_t seed_index;
    size_t final_index;
    char case_id[128];
    int ok = 1;

    for (chain_length = 1; chain_length <= 6; ++chain_length) {
        for (repeated_seed_store = 0; repeated_seed_store <= 1; ++repeated_seed_store) {
            for (duplicate_final_store = 0; duplicate_final_store <= 1; ++duplicate_final_store) {
                for (seed_index = 0; seed_index < sizeof(seed_values) / sizeof(seed_values[0]); ++seed_index) {
                    for (final_index = 0; final_index < sizeof(final_values) / sizeof(final_values[0]); ++final_index) {
                        ValueSsaOracleCase spec;

                        spec.kind = VALUE_SSA_ORACLE_LOCAL_CHAIN;
                        spec.chain_length = chain_length;
                        spec.post_call_hops = 0;
                        spec.repeated_seed_store = repeated_seed_store;
                        spec.duplicate_final_store = duplicate_final_store;
                        spec.duplicate_then_store = 0;
                        spec.duplicate_else_store = 0;
                        spec.seed_value = seed_values[seed_index];
                        spec.final_value = final_values[final_index];
                        spec.then_value = 0;
                        spec.else_value = 0;

                        snprintf(case_id,
                            sizeof(case_id),
                            "ORACLE-LOCAL-CHAIN-L%zu-R%d-D%d-S%lld-F%lld",
                            chain_length,
                            repeated_seed_store,
                            duplicate_final_store,
                            spec.seed_value,
                            spec.final_value);
                        ok &= value_ssa_oracle_compare_before_after(
                            case_id,
                            build_local_chain_program,
                            &spec,
                            NULL,
                            0,
                            NULL);
                    }
                }
            }
        }
    }

    return ok;
}

static int test_value_ssa_oracle_global_call_chain_matrix(void) {
    size_t chain_length;
    size_t post_call_hops;
    int repeated_seed_store;
    int duplicate_final_store;
    char case_id[128];
    ValueSsaInterpOptions options;
    int ok = 1;

    value_ssa_interp_options_init(&options);
    options.extern_call = value_ssa_oracle_noop_extern;

    for (chain_length = 1; chain_length <= 4; ++chain_length) {
        for (post_call_hops = 0; post_call_hops <= 3; ++post_call_hops) {
            for (repeated_seed_store = 0; repeated_seed_store <= 1; ++repeated_seed_store) {
                for (duplicate_final_store = 0; duplicate_final_store <= 1; ++duplicate_final_store) {
                    ValueSsaOracleCase spec;

                    spec.kind = VALUE_SSA_ORACLE_GLOBAL_CALL_CHAIN;
                    spec.chain_length = chain_length;
                    spec.post_call_hops = post_call_hops;
                    spec.repeated_seed_store = repeated_seed_store;
                    spec.duplicate_final_store = duplicate_final_store;
                    spec.duplicate_then_store = 0;
                    spec.duplicate_else_store = 0;
                    spec.seed_value = 1;
                    spec.final_value = 2 + (long long)chain_length + (long long)post_call_hops;
                    spec.then_value = 0;
                    spec.else_value = 0;

                    snprintf(case_id,
                        sizeof(case_id),
                        "ORACLE-GLOBAL-CALL-CHAIN-L%zu-P%zu-R%d-D%d",
                        chain_length,
                        post_call_hops,
                        repeated_seed_store,
                        duplicate_final_store);
                    ok &= value_ssa_oracle_compare_before_after(
                        case_id,
                        build_global_call_chain_program,
                        &spec,
                        NULL,
                        0,
                        &options);
                }
            }
        }
    }

    return ok;
}

static int test_value_ssa_oracle_local_diamond_matrix(void) {
    long long args[] = {0};
    size_t value_index;
    int duplicate_then_store;
    int duplicate_else_store;
    const long long value_pairs[][2] = {
        {10, 20},
        {3, 9},
        {11, 11},
    };
    char case_id[128];
    int ok = 1;

    for (value_index = 0; value_index < sizeof(value_pairs) / sizeof(value_pairs[0]); ++value_index) {
        for (duplicate_then_store = 0; duplicate_then_store <= 1; ++duplicate_then_store) {
            for (duplicate_else_store = 0; duplicate_else_store <= 1; ++duplicate_else_store) {
                ValueSsaOracleCase spec;

                spec.kind = VALUE_SSA_ORACLE_LOCAL_DIAMOND;
                spec.chain_length = 0;
                spec.post_call_hops = 0;
                spec.repeated_seed_store = 0;
                spec.duplicate_final_store = 0;
                spec.duplicate_then_store = duplicate_then_store;
                spec.duplicate_else_store = duplicate_else_store;
                spec.seed_value = 0;
                spec.final_value = 0;
                spec.then_value = value_pairs[value_index][0];
                spec.else_value = value_pairs[value_index][1];

                args[0] = 0;
                snprintf(case_id,
                    sizeof(case_id),
                    "ORACLE-LOCAL-DIAMOND-ELSE-T%lld-E%lld-DT%d-DE%d",
                    spec.then_value,
                    spec.else_value,
                    duplicate_then_store,
                    duplicate_else_store);
                ok &= value_ssa_oracle_compare_before_after(
                    case_id,
                    build_local_diamond_program,
                    &spec,
                    args,
                    1,
                    NULL);

                args[0] = 1;
                snprintf(case_id,
                    sizeof(case_id),
                    "ORACLE-LOCAL-DIAMOND-THEN-T%lld-E%lld-DT%d-DE%d",
                    spec.then_value,
                    spec.else_value,
                    duplicate_then_store,
                    duplicate_else_store);
                ok &= value_ssa_oracle_compare_before_after(
                    case_id,
                    build_local_diamond_program,
                    &spec,
                    args,
                    1,
                    NULL);
            }
        }
    }

    return ok;
}

static int test_value_ssa_oracle_global_diamond_matrix(void) {
    long long args[] = {0};
    size_t value_index;
    int duplicate_then_store;
    int duplicate_else_store;
    const long long value_pairs[][2] = {
        {4, 8},
        {7, 1},
        {12, 12},
    };
    char case_id[128];
    int ok = 1;

    for (value_index = 0; value_index < sizeof(value_pairs) / sizeof(value_pairs[0]); ++value_index) {
        for (duplicate_then_store = 0; duplicate_then_store <= 1; ++duplicate_then_store) {
            for (duplicate_else_store = 0; duplicate_else_store <= 1; ++duplicate_else_store) {
                ValueSsaOracleCase spec;

                spec.kind = VALUE_SSA_ORACLE_GLOBAL_DIAMOND;
                spec.chain_length = 0;
                spec.post_call_hops = 0;
                spec.repeated_seed_store = 0;
                spec.duplicate_final_store = 0;
                spec.duplicate_then_store = duplicate_then_store;
                spec.duplicate_else_store = duplicate_else_store;
                spec.seed_value = 0;
                spec.final_value = 0;
                spec.then_value = value_pairs[value_index][0];
                spec.else_value = value_pairs[value_index][1];

                args[0] = 0;
                snprintf(case_id,
                    sizeof(case_id),
                    "ORACLE-GLOBAL-DIAMOND-ELSE-T%lld-E%lld-DT%d-DE%d",
                    spec.then_value,
                    spec.else_value,
                    duplicate_then_store,
                    duplicate_else_store);
                ok &= value_ssa_oracle_compare_before_after(
                    case_id,
                    build_global_diamond_program,
                    &spec,
                    args,
                    1,
                    NULL);

                args[0] = 1;
                snprintf(case_id,
                    sizeof(case_id),
                    "ORACLE-GLOBAL-DIAMOND-THEN-T%lld-E%lld-DT%d-DE%d",
                    spec.then_value,
                    spec.else_value,
                    duplicate_then_store,
                    duplicate_else_store);
                ok &= value_ssa_oracle_compare_before_after(
                    case_id,
                    build_global_diamond_program,
                    &spec,
                    args,
                    1,
                    NULL);
            }
        }
    }

    return ok;
}

static int test_value_ssa_oracle_mixed_hybrid_matrix(void) {
    size_t chain_index;
    size_t hop_index;
    int repeated_seed_store;
    int duplicate_then_store;
    int duplicate_else_store;
    int duplicate_final_store;
    long long args[] = {0};
    const size_t chain_lengths[] = {2, 5};
    const size_t post_call_hops[] = {0, 4};
    const long long branch_pairs[][2] = {
        {5, 9},
        {8, 8},
    };
    size_t pair_index;
    char case_id[128];
    ValueSsaInterpOptions options;
    int ok = 1;

    value_ssa_interp_options_init(&options);
    options.extern_call = value_ssa_oracle_noop_extern;

    for (chain_index = 0; chain_index < sizeof(chain_lengths) / sizeof(chain_lengths[0]); ++chain_index) {
        for (hop_index = 0; hop_index < sizeof(post_call_hops) / sizeof(post_call_hops[0]); ++hop_index) {
            for (repeated_seed_store = 0; repeated_seed_store <= 1; ++repeated_seed_store) {
                for (duplicate_then_store = 0; duplicate_then_store <= 1; ++duplicate_then_store) {
                    for (duplicate_else_store = 0; duplicate_else_store <= 1; ++duplicate_else_store) {
                        for (duplicate_final_store = 0; duplicate_final_store <= 1; ++duplicate_final_store) {
                            for (pair_index = 0; pair_index < sizeof(branch_pairs) / sizeof(branch_pairs[0]); ++pair_index) {
                                ValueSsaOracleCase spec;

                                spec.kind = VALUE_SSA_ORACLE_MIXED_HYBRID;
                                spec.chain_length = chain_lengths[chain_index];
                                spec.post_call_hops = post_call_hops[hop_index];
                                spec.repeated_seed_store = repeated_seed_store;
                                spec.duplicate_final_store = duplicate_final_store;
                                spec.duplicate_then_store = duplicate_then_store;
                                spec.duplicate_else_store = duplicate_else_store;
                                spec.seed_value = 3 + (long long)chain_lengths[chain_index];
                                spec.final_value = 2 + (long long)post_call_hops[hop_index];
                                spec.then_value = branch_pairs[pair_index][0];
                                spec.else_value = branch_pairs[pair_index][1];

                                args[0] = 0;
                                snprintf(case_id,
                                    sizeof(case_id),
                                    "ORACLE-MIXED-HYBRID-ELSE-L%zu-P%zu-R%d-DT%d-DE%d-DF%d-T%lld-E%lld",
                                    spec.chain_length,
                                    spec.post_call_hops,
                                    repeated_seed_store,
                                    duplicate_then_store,
                                    duplicate_else_store,
                                    duplicate_final_store,
                                    spec.then_value,
                                    spec.else_value);
                                ok &= value_ssa_oracle_compare_before_after(
                                    case_id,
                                    build_mixed_hybrid_program,
                                    &spec,
                                    args,
                                    1,
                                    &options);

                                args[0] = 1;
                                snprintf(case_id,
                                    sizeof(case_id),
                                    "ORACLE-MIXED-HYBRID-THEN-L%zu-P%zu-R%d-DT%d-DE%d-DF%d-T%lld-E%lld",
                                    spec.chain_length,
                                    spec.post_call_hops,
                                    repeated_seed_store,
                                    duplicate_then_store,
                                    duplicate_else_store,
                                    duplicate_final_store,
                                    spec.then_value,
                                    spec.else_value);
                                ok &= value_ssa_oracle_compare_before_after(
                                    case_id,
                                    build_mixed_hybrid_program,
                                    &spec,
                                    args,
                                    1,
                                    &options);
                            }
                        }
                    }
                }
            }
        }
    }

    return ok;
}

static unsigned int value_ssa_oracle_next_random(unsigned int *state) {
    unsigned int x = *state;

    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static int test_value_ssa_oracle_randomized_stress(void) {
    unsigned int state = 0xC0FFEEu;
    size_t iteration;
    ValueSsaInterpOptions options;
    int ok = 1;

    value_ssa_interp_options_init(&options);
    options.extern_call = value_ssa_oracle_noop_extern;

    for (iteration = 0; iteration < 384; ++iteration) {
        ValueSsaOracleCase spec;
        ValueSsaOracleBuilderFn builder = NULL;
        long long args[] = {0};
        const ValueSsaInterpOptions *active_options = NULL;
        char case_id[128];
        unsigned int draw = value_ssa_oracle_next_random(&state);

        spec.kind = (ValueSsaOracleProgramKind)(draw % 5u);
        spec.chain_length = 1u + (value_ssa_oracle_next_random(&state) % 8u);
        spec.post_call_hops = value_ssa_oracle_next_random(&state) % 6u;
        spec.repeated_seed_store = (int)(value_ssa_oracle_next_random(&state) & 1u);
        spec.duplicate_final_store = (int)(value_ssa_oracle_next_random(&state) & 1u);
        spec.duplicate_then_store = (int)(value_ssa_oracle_next_random(&state) & 1u);
        spec.duplicate_else_store = (int)(value_ssa_oracle_next_random(&state) & 1u);
        spec.seed_value = (long long)(value_ssa_oracle_next_random(&state) % 9u);
        spec.final_value = (long long)(1u + (value_ssa_oracle_next_random(&state) % 17u));
        spec.then_value = (long long)(1u + (value_ssa_oracle_next_random(&state) % 23u));
        spec.else_value = (long long)(1u + (value_ssa_oracle_next_random(&state) % 23u));

        switch (spec.kind) {
        case VALUE_SSA_ORACLE_LOCAL_CHAIN:
            builder = build_local_chain_program;
            snprintf(case_id, sizeof(case_id), "ORACLE-RAND-LOCAL-CHAIN-%zu", iteration);
            break;
        case VALUE_SSA_ORACLE_GLOBAL_CALL_CHAIN:
            builder = build_global_call_chain_program;
            active_options = &options;
            snprintf(case_id, sizeof(case_id), "ORACLE-RAND-GLOBAL-CALL-CHAIN-%zu", iteration);
            break;
        case VALUE_SSA_ORACLE_LOCAL_DIAMOND:
            builder = build_local_diamond_program;
            args[0] = (long long)(value_ssa_oracle_next_random(&state) & 1u);
            snprintf(case_id, sizeof(case_id), "ORACLE-RAND-LOCAL-DIAMOND-%zu", iteration);
            break;
        case VALUE_SSA_ORACLE_GLOBAL_DIAMOND:
            builder = build_global_diamond_program;
            args[0] = (long long)(value_ssa_oracle_next_random(&state) & 1u);
            snprintf(case_id, sizeof(case_id), "ORACLE-RAND-GLOBAL-DIAMOND-%zu", iteration);
            break;
        case VALUE_SSA_ORACLE_MIXED_HYBRID:
        default:
            builder = build_mixed_hybrid_program;
            active_options = &options;
            args[0] = (long long)(value_ssa_oracle_next_random(&state) & 1u);
            snprintf(case_id, sizeof(case_id), "ORACLE-RAND-MIXED-HYBRID-%zu", iteration);
            break;
        }

        ok &= value_ssa_oracle_compare_before_after(
            case_id,
            builder,
            &spec,
            spec.kind == VALUE_SSA_ORACLE_LOCAL_CHAIN || spec.kind == VALUE_SSA_ORACLE_GLOBAL_CALL_CHAIN ? NULL : args,
            spec.kind == VALUE_SSA_ORACLE_LOCAL_CHAIN || spec.kind == VALUE_SSA_ORACLE_GLOBAL_CALL_CHAIN ? 0 : 1,
            active_options);
    }

    return ok;
}

static int test_value_ssa_oracle_handcrafted_semantics(void) {
    long long args[] = {0};
    ValueSsaInterpOptions options;
    int ok = 1;

    value_ssa_interp_options_init(&options);
    options.extern_call = value_ssa_oracle_noop_extern;

    args[0] = 4;
    ok &= value_ssa_oracle_compare_before_after_raw("ORACLE-HAND-COMPLEX-LOCAL-CHAIN",
        build_canonicalize_complex_local_chain_program,
        args,
        1,
        NULL);

    args[0] = 1;
    ok &= value_ssa_oracle_compare_before_after_raw("ORACLE-HAND-DIAMOND-THEN",
        build_scrambled_canonicalize_program,
        args,
        1,
        NULL);

    args[0] = 0;
    ok &= value_ssa_oracle_compare_before_after_raw("ORACLE-HAND-DIAMOND-ELSE",
        build_scrambled_canonicalize_program,
        args,
        1,
        NULL);

    ok &= value_ssa_oracle_compare_before_after_raw("ORACLE-HAND-COMPLEX-GLOBAL-CALL-CHAIN",
        build_canonicalize_complex_global_call_chain_program,
        NULL,
        0,
        &options);

    args[0] = 1;
    ok &= value_ssa_oracle_compare_before_after_raw("ORACLE-HAND-GLOBAL-BRANCH-OBSERVER-THEN",
        build_dead_global_store_preserves_branch_observer_program,
        args,
        1,
        NULL);

    args[0] = 0;
    ok &= value_ssa_oracle_compare_before_after_raw("ORACLE-HAND-GLOBAL-BRANCH-OBSERVER-ELSE",
        build_dead_global_store_preserves_branch_observer_program,
        args,
        1,
        NULL);

    {
        ValueSsaOracleCase spec;

        spec.kind = VALUE_SSA_ORACLE_MIXED_HYBRID;
        spec.chain_length = 6;
        spec.post_call_hops = 5;
        spec.repeated_seed_store = 1;
        spec.duplicate_final_store = 1;
        spec.duplicate_then_store = 1;
        spec.duplicate_else_store = 1;
        spec.seed_value = 7;
        spec.final_value = 3;
        spec.then_value = 11;
        spec.else_value = 4;

        args[0] = 1;
        ok &= value_ssa_oracle_compare_before_after("ORACLE-HAND-MIXED-HYBRID-THEN",
            build_mixed_hybrid_program,
            &spec,
            args,
            1,
            &options);

        args[0] = 0;
        ok &= value_ssa_oracle_compare_before_after("ORACLE-HAND-MIXED-HYBRID-ELSE",
            build_mixed_hybrid_program,
            &spec,
            args,
            1,
            &options);
    }

    return ok;
}

int main(void) {
    int ok = 1;

    ok &= test_value_ssa_oracle_local_chain_matrix();
    ok &= test_value_ssa_oracle_global_call_chain_matrix();
    ok &= test_value_ssa_oracle_local_diamond_matrix();
    ok &= test_value_ssa_oracle_global_diamond_matrix();
    ok &= test_value_ssa_oracle_mixed_hybrid_matrix();
    ok &= test_value_ssa_oracle_randomized_stress();
    ok &= test_value_ssa_oracle_handcrafted_semantics();

    if (!ok) {
        return 1;
    }

    printf("[value-ssa-oracle] PASS\n");
    return 0;
}
