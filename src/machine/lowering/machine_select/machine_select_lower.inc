#if defined(__INTELLISENSE__) && !defined(MACHINE_SELECT_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_SELECT_SPLIT_AGGREGATOR)
#define MACHINE_SELECT_SPLIT_INTELLISENSE_CONTEXT
#include "machine_select.c"
#else

static MachineSelectOperand machine_select_lower_operand(MachineIrOperand operand) {
    switch (operand.kind) {
        case MACHINE_IR_OPERAND_IMMEDIATE:
            return machine_select_operand_immediate(operand.immediate);
        case MACHINE_IR_OPERAND_REGISTER:
            return machine_select_operand_register(operand.machine_register_id);
        case MACHINE_IR_OPERAND_SPILL_SLOT:
            return machine_select_operand_spill_slot(operand.spill_slot);
        default:
            return machine_select_operand_none();
    }
}

static MachineSelectSlotRef machine_select_lower_slot(MachineIrSlotRef slot) {
    return slot.kind == MACHINE_IR_SLOT_LOCAL
        ? machine_select_slot_local(slot.id)
        : machine_select_slot_global(slot.id);
}

static int machine_select_binary_is_commutative(MachineIrBinaryOp op) {
    switch (op) {
        case MACHINE_IR_BINARY_ADD:
        case MACHINE_IR_BINARY_MUL:
        case MACHINE_IR_BINARY_BIT_AND:
        case MACHINE_IR_BINARY_BIT_XOR:
        case MACHINE_IR_BINARY_BIT_OR:
        case MACHINE_IR_BINARY_EQ:
        case MACHINE_IR_BINARY_NE:
            return 1;
        default:
            return 0;
    }
}

static void machine_select_canonicalize_binary_immediate_side(
    MachineIrBinaryOp *op,
    MachineIrOperand *lhs,
    MachineIrOperand *rhs) {
    MachineIrOperand tmp;

    if (!op || !lhs || !rhs ||
        lhs->kind != MACHINE_IR_OPERAND_IMMEDIATE ||
        rhs->kind == MACHINE_IR_OPERAND_IMMEDIATE) {
        return;
    }
    if (machine_select_binary_is_commutative(*op)) {
        tmp = *lhs;
        *lhs = *rhs;
        *rhs = tmp;
        return;
    }
    if (machine_select_binary_is_compare(*op)) {
        tmp = *lhs;
        *lhs = *rhs;
        *rhs = tmp;
        switch (*op) {
            case MACHINE_IR_BINARY_LT:
                *op = MACHINE_IR_BINARY_GT;
                break;
            case MACHINE_IR_BINARY_LE:
                *op = MACHINE_IR_BINARY_GE;
                break;
            case MACHINE_IR_BINARY_GT:
                *op = MACHINE_IR_BINARY_LT;
                break;
            case MACHINE_IR_BINARY_GE:
                *op = MACHINE_IR_BINARY_LE;
                break;
            default:
                break;
        }
    }
}

static int machine_select_binary_uses_immediate_form(MachineIrBinaryOp op,
    MachineIrOperand lhs,
    MachineIrOperand rhs) {
    (void)op;
    return lhs.kind != MACHINE_IR_OPERAND_IMMEDIATE &&
        rhs.kind == MACHINE_IR_OPERAND_IMMEDIATE;
}

static int machine_select_try_eval_binary_immediate(MachineIrBinaryOp op,
    MachineIrOperand lhs,
    MachineIrOperand rhs,
    long long *out_value) {
    int32_t lhs_value;
    int32_t rhs_value;

    if (!out_value ||
        lhs.kind != MACHINE_IR_OPERAND_IMMEDIATE ||
        rhs.kind != MACHINE_IR_OPERAND_IMMEDIATE) {
        return 0;
    }

    lhs_value = (int32_t)machine_select_normalize_sysy_int_value(lhs.immediate);
    rhs_value = (int32_t)machine_select_normalize_sysy_int_value(rhs.immediate);
    switch (op) {
        case MACHINE_IR_BINARY_ADD:
            *out_value = machine_select_normalize_sysy_int_value((long long)lhs_value + (long long)rhs_value);
            return 1;
        case MACHINE_IR_BINARY_SUB:
            *out_value = machine_select_normalize_sysy_int_value((long long)lhs_value - (long long)rhs_value);
            return 1;
        case MACHINE_IR_BINARY_MUL:
            *out_value = machine_select_normalize_sysy_int_value((long long)lhs_value * (long long)rhs_value);
            return 1;
        case MACHINE_IR_BINARY_DIV:
            if (rhs_value == 0) {
                return 0;
            }
            *out_value = machine_select_normalize_sysy_int_value((long long)lhs_value / (long long)rhs_value);
            return 1;
        case MACHINE_IR_BINARY_MOD:
            if (rhs_value == 0) {
                return 0;
            }
            *out_value = machine_select_normalize_sysy_int_value((long long)lhs_value % (long long)rhs_value);
            return 1;
        case MACHINE_IR_BINARY_BIT_AND:
            *out_value = machine_select_normalize_sysy_int_value((long long)(lhs_value & rhs_value));
            return 1;
        case MACHINE_IR_BINARY_BIT_XOR:
            *out_value = machine_select_normalize_sysy_int_value((long long)(lhs_value ^ rhs_value));
            return 1;
        case MACHINE_IR_BINARY_BIT_OR:
            *out_value = machine_select_normalize_sysy_int_value((long long)(lhs_value | rhs_value));
            return 1;
        case MACHINE_IR_BINARY_SHIFT_LEFT:
            if (rhs_value < 0 || rhs_value >= 32) {
                return 0;
            }
            *out_value =
                machine_select_normalize_sysy_int_value((long long)((uint32_t)lhs_value << (uint32_t)rhs_value));
            return 1;
        case MACHINE_IR_BINARY_SHIFT_RIGHT:
            if (rhs_value < 0 || rhs_value >= 32) {
                return 0;
            }
            *out_value = machine_select_normalize_sysy_int_value((long long)(lhs_value >> rhs_value));
            return 1;
        case MACHINE_IR_BINARY_EQ:
            *out_value = lhs_value == rhs_value ? 1 : 0;
            return 1;
        case MACHINE_IR_BINARY_NE:
            *out_value = lhs_value != rhs_value ? 1 : 0;
            return 1;
        case MACHINE_IR_BINARY_LT:
            *out_value = lhs_value < rhs_value ? 1 : 0;
            return 1;
        case MACHINE_IR_BINARY_LE:
            *out_value = lhs_value <= rhs_value ? 1 : 0;
            return 1;
        case MACHINE_IR_BINARY_GT:
            *out_value = lhs_value > rhs_value ? 1 : 0;
            return 1;
        case MACHINE_IR_BINARY_GE:
            *out_value = lhs_value >= rhs_value ? 1 : 0;
            return 1;
        default:
            return 0;
    }
}

static int machine_select_try_eval_selected_binary_immediate(MachineIrBinaryOp op,
    MachineSelectOperand lhs,
    MachineSelectOperand rhs,
    long long *out_value) {
    if (!out_value ||
        lhs.kind != MACHINE_SELECT_OPERAND_IMMEDIATE ||
        rhs.kind != MACHINE_SELECT_OPERAND_IMMEDIATE) {
        return 0;
    }
    return machine_select_try_eval_binary_immediate(
        op,
        machine_ir_operand_immediate(lhs.immediate),
        machine_ir_operand_immediate(rhs.immediate),
        out_value);
}

static int machine_select_program_uses_indirect_memory(const MachineSelectProgram *program) {
    size_t function_index;

    if (!program) {
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const MachineSelectFunction *function = &program->functions[function_index];
        size_t block_index;

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            const MachineSelectBasicBlock *block = &function->blocks[block_index];
            size_t op_index;

            for (op_index = 0; op_index < block->op_count; ++op_index) {
                switch (block->ops[op_index].kind) {
                case MACHINE_SELECT_OP_ADDR_LOCAL:
                case MACHINE_SELECT_OP_ADDR_GLOBAL:
                case MACHINE_SELECT_OP_LOAD_INDIRECT:
                case MACHINE_SELECT_OP_STORE_INDIRECT:
                    return 1;
                default:
                    break;
                }
            }
        }
    }

    return 0;
}

static int machine_select_function_is_extreme_single_block_cleanup_candidate(
    const MachineSelectFunction *function) {
    const MachineSelectBasicBlock *block = NULL;
    size_t op_index;
    size_t call_count = 0u;

    if (!function || !function->has_body || function->block_count != 1u || !function->blocks) {
        return 0;
    }
    if (function->spill_slot_count < 20000u) {
        return 0;
    }

    block = &function->blocks[0];
    if (block->op_count < 12000u || !block->has_terminator) {
        return 0;
    }
    switch (block->terminator.kind) {
        case MACHINE_SELECT_TERM_RETURN:
        case MACHINE_SELECT_TERM_RETURN_IMM:
        case MACHINE_SELECT_TERM_RETURN_SPILL:
            break;
        default:
            return 0;
    }

    for (op_index = 0u; op_index < block->op_count; ++op_index) {
        if (machine_select_op_is_call_kind(block->ops[op_index].kind)) {
            ++call_count;
        }
    }
    return call_count == 0u;
}

static int machine_select_program_should_skip_cleanup_pure(const MachineSelectProgram *program) {
    size_t function_index;

    if (machine_select_skip_cleanup_pure_enabled()) {
        return 1;
    }
    if (!program || program->function_count != 1u || !program->functions) {
        return 0;
    }

    for (function_index = 0u; function_index < program->function_count; ++function_index) {
        if (!machine_select_function_is_extreme_single_block_cleanup_candidate(&program->functions[function_index])) {
            return 0;
        }
    }
    return 1;
}

static int machine_select_program_should_skip_reuse_addr_roots(const MachineSelectProgram *program) {
    size_t function_index;

    if (machine_select_skip_cleanup_reuse_addr_roots_enabled()) {
        return 1;
    }
    if (!program || program->function_count != 1u || !program->functions) {
        return 0;
    }

    for (function_index = 0u; function_index < program->function_count; ++function_index) {
        if (!machine_select_function_is_extreme_single_block_cleanup_candidate(&program->functions[function_index])) {
            return 0;
        }
    }
    return 1;
}

static int machine_select_lower_value_instruction(const MachineIrInstruction *src_op,
    MachineSelectOp *dest_op,
    int *out_handled);
static int machine_select_lower_memory_instruction(const MachineIrInstruction *src_op,
    MachineSelectOp *dest_op,
    int *out_handled);
static int machine_select_lower_call_instruction(const MachineIrInstruction *src_op,
    MachineSelectOp *dest_op,
    MachineSelectError *error);
static int machine_select_lower_terminator(const MachineIrFunction *src_function,
    const MachineIrBasicBlock *src_block,
    MachineSelectBasicBlock *dest_block,
    MachineSelectError *error);
static int machine_select_cleanup_pure_program(MachineSelectProgram *program, MachineSelectError *error);
static int machine_select_cleanup_reuse_addr_roots_program(MachineSelectProgram *program);
static int machine_select_cleanup_reuse_internal_pure_calls_program(MachineSelectProgram *program,
    MachineSelectError *error);
static int machine_select_cleanup_reuse_unique_predecessor_pure_calls_program(MachineSelectProgram *program,
    MachineSelectError *error);
static int machine_select_cleanup_forward_same_block_indirect_loads_program(MachineSelectProgram *program);
static int machine_select_cleanup_reuse_spill_pure_expr_program(MachineSelectProgram *program);
static int machine_select_cleanup_program(MachineSelectProgram *program, MachineSelectError *error);

#include "machine_select_lower_value.inc"
#include "machine_select_lower_memory.inc"
#include "machine_select_lower_call.inc"
#include "machine_select_lower_control.inc"

static int machine_select_lower_instruction(const MachineIrInstruction *src_op,
    MachineSelectOp *dest_op,
    MachineSelectProgram *out_program,
    MachineSelectError *error) {
    int handled = 0;

    (void)out_program;
    if (!src_op || !dest_op) {
        machine_select_set_error(error, 0, 0, "MACHINE-SELECT-313: invalid machine-ir instruction");
        return 0;
    }

    if (!machine_select_lower_value_instruction(src_op, dest_op, &handled)) {
        return 0;
    }
    if (handled) {
        return 1;
    }
    if (!machine_select_lower_memory_instruction(src_op, dest_op, &handled)) {
        return 0;
    }
    if (handled) {
        return 1;
    }
    if (src_op->kind == MACHINE_IR_INSTR_CALL) {
        return machine_select_lower_call_instruction(src_op, dest_op, error);
    }

    machine_select_set_error(error, 0, 0, "MACHINE-SELECT-313: unknown machine-ir instruction kind");
    return 0;
}

int machine_select_lower_program_from_machine_ir(const MachineIrProgram *source,
    MachineSelectProgram *out_program,
    MachineSelectError *error) {
    size_t register_index;
    size_t global_index;
    size_t function_index;
    int cleanup_changed = 0;
    int trace_timing = machine_select_trace_enabled();
    double total_start = trace_timing ? machine_select_now_s() : 0.0;
    double stage_start = total_start;

    if (!source || !out_program) {
        machine_select_set_error(error, 0, 0, "MACHINE-SELECT-300: invalid lowering contract");
        return 0;
    }

    machine_select_program_free(out_program);
    machine_select_program_init(out_program);

    if (trace_timing) {
        fprintf(stderr,
            "[machine-select-timing] flags skip_pure=%d skip_reuse_addr_roots=%d\n",
            machine_select_skip_cleanup_pure_enabled(),
            machine_select_skip_cleanup_reuse_addr_roots_enabled());
    }

    out_program->register_bank.register_count = source->register_bank.register_count;
    if (source->register_bank.register_count > 0) {
        out_program->register_bank.registers =
            (MachineSelectRegisterDesc *)calloc(source->register_bank.register_count, sizeof(MachineSelectRegisterDesc));
        if (!out_program->register_bank.registers) {
            machine_select_set_error(error, 0, 0, "MACHINE-SELECT-301: out of memory copying register bank");
            machine_select_program_free(out_program);
            return 0;
        }
    }
    for (register_index = 0; register_index < source->register_bank.register_count; ++register_index) {
        MachineSelectRegisterDesc *dest = &out_program->register_bank.registers[register_index];
        const MachineIrRegisterDesc *src = &source->register_bank.registers[register_index];

        dest->register_id = src->register_id;
        dest->name = machine_select_strdup(src->name);
        if (src->name && !dest->name) {
            machine_select_set_error(error, 0, 0, "MACHINE-SELECT-302: out of memory copying register name");
            machine_select_program_free(out_program);
            return 0;
        }
        dest->register_class = src->register_class;
        dest->allocatable = src->allocatable;
        dest->caller_clobbered = src->caller_clobbered;
        dest->callee_preserved = src->callee_preserved;
    }

    out_program->global_count = source->global_count;
    out_program->global_capacity = source->global_count;
    if (source->global_count > 0) {
        out_program->globals = (MachineSelectGlobal *)calloc(source->global_count, sizeof(MachineSelectGlobal));
        if (!out_program->globals) {
            machine_select_set_error(error, 0, 0, "MACHINE-SELECT-303: out of memory copying globals");
            machine_select_program_free(out_program);
            return 0;
        }
    }
    for (global_index = 0; global_index < source->global_count; ++global_index) {
        MachineSelectGlobal *dest = &out_program->globals[global_index];
        const MachineIrGlobal *src = &source->globals[global_index];

        dest->id = src->id;
        dest->name = machine_select_strdup(src->name);
        if (src->name && !dest->name) {
            machine_select_set_error(error, 0, 0, "MACHINE-SELECT-304: out of memory copying global name");
            machine_select_program_free(out_program);
            return 0;
        }
        dest->byte_size = src->byte_size;
        dest->has_initializer = src->has_initializer;
        dest->initializer_value = src->initializer_value;
        dest->has_runtime_initializer = src->has_runtime_initializer;
    }

    out_program->function_count = source->function_count;
    out_program->function_capacity = source->function_count;
    if (source->function_count > 0) {
        out_program->functions = (MachineSelectFunction *)calloc(source->function_count, sizeof(MachineSelectFunction));
        if (!out_program->functions) {
            machine_select_set_error(error, 0, 0, "MACHINE-SELECT-305: out of memory copying functions");
            machine_select_program_free(out_program);
            return 0;
        }
    }

    for (function_index = 0; function_index < source->function_count; ++function_index) {
        const MachineIrFunction *src_function = &source->functions[function_index];
        MachineSelectFunction *dest_function = &out_program->functions[function_index];
        size_t local_index;
        size_t block_index;

        dest_function->name = machine_select_strdup(src_function->name);
        if (src_function->name && !dest_function->name) {
            machine_select_set_error(error, 0, 0, "MACHINE-SELECT-306: out of memory copying function name");
            machine_select_program_free(out_program);
            return 0;
        }
        dest_function->has_body = src_function->has_body;
        dest_function->parameter_count = src_function->parameter_count;
        dest_function->spill_slot_count = src_function->spill_slot_count;
        dest_function->local_count = src_function->local_count;
        dest_function->local_capacity = src_function->local_count;
        dest_function->block_count = src_function->block_count;
        dest_function->block_capacity = src_function->block_count;

        if (src_function->local_count > 0) {
            dest_function->locals = (MachineSelectLocal *)calloc(src_function->local_count, sizeof(MachineSelectLocal));
            if (!dest_function->locals) {
                machine_select_set_error(error, 0, 0, "MACHINE-SELECT-307: out of memory copying locals");
                machine_select_program_free(out_program);
                return 0;
            }
        }
        if (src_function->block_count > 0) {
            dest_function->blocks =
                (MachineSelectBasicBlock *)calloc(src_function->block_count, sizeof(MachineSelectBasicBlock));
            if (!dest_function->blocks) {
                machine_select_set_error(error, 0, 0, "MACHINE-SELECT-308: out of memory copying blocks");
                machine_select_program_free(out_program);
                return 0;
            }
        }

        for (local_index = 0; local_index < src_function->local_count; ++local_index) {
            dest_function->locals[local_index].id = src_function->locals[local_index].id;
            dest_function->locals[local_index].source_name =
                machine_select_strdup(src_function->locals[local_index].source_name);
            if (src_function->locals[local_index].source_name &&
                !dest_function->locals[local_index].source_name) {
                machine_select_set_error(error, 0, 0, "MACHINE-SELECT-309: out of memory copying local name");
                machine_select_program_free(out_program);
                return 0;
            }
            dest_function->locals[local_index].is_parameter = src_function->locals[local_index].is_parameter;
        }

        for (block_index = 0; block_index < src_function->block_count; ++block_index) {
            const MachineIrBasicBlock *src_block = &src_function->blocks[block_index];
            MachineSelectBasicBlock *dest_block = &dest_function->blocks[block_index];
            size_t op_index;

            if (src_block->phi_count != 0) {
                machine_select_set_error(
                    error,
                    0,
                    0,
                    "MACHINE-SELECT-315: machine_select lowering currently requires phi-free machine_ir input");
                machine_select_program_free(out_program);
                return 0;
            }

            dest_block->id = src_block->id;
            dest_block->op_count = src_block->instruction_count;
            dest_block->op_capacity = src_block->instruction_count;
            dest_block->has_terminator = src_block->has_terminator;
            if (src_block->instruction_count > 0) {
                dest_block->ops = (MachineSelectOp *)calloc(src_block->instruction_count, sizeof(MachineSelectOp));
                if (!dest_block->ops) {
                    machine_select_set_error(error, 0, 0, "MACHINE-SELECT-310: out of memory copying ops");
                    machine_select_program_free(out_program);
                    return 0;
                }
            }

            for (op_index = 0; op_index < src_block->instruction_count; ++op_index) {
                const MachineIrInstruction *src_op = &src_block->instructions[op_index];
                MachineSelectOp *dest_op = &dest_block->ops[op_index];

                dest_op->has_result = src_op->has_result;
                dest_op->result = machine_select_lower_operand(src_op->result);
                if (!machine_select_lower_instruction(src_op, dest_op, out_program, error)) {
                    machine_select_program_free(out_program);
                    return 0;
                }
            }

            if (src_block->has_terminator) {
                if (!machine_select_lower_terminator(src_function, src_block, dest_block, error)) {
                    machine_select_program_free(out_program);
                    return 0;
                }
            }
        }
    }

    if (trace_timing) {
        machine_select_trace_timing("clone+lower", machine_select_now_s() - stage_start);
        stage_start = machine_select_now_s();
        fprintf(stderr,
            "[machine-select-timing] decisions skip_pure=%d skip_reuse_addr_roots=%d skip_forward_indirect=%d skip_reuse_spill=%d\n",
            machine_select_program_should_skip_cleanup_pure(out_program),
            machine_select_program_should_skip_reuse_addr_roots(out_program),
            machine_select_skip_cleanup_forward_same_block_indirect_loads_enabled(),
            machine_select_skip_cleanup_reuse_spill_pure_expr_enabled());
    }

    if (!machine_select_verify_program(out_program, error)) {
        machine_select_program_free(out_program);
        return 0;
    }
    if (trace_timing) {
        machine_select_trace_timing("verify-initial", machine_select_now_s() - stage_start);
        stage_start = machine_select_now_s();
    }
    if (!machine_select_aggressive_opt_mode_enabled()) {
        if (trace_timing) {
            machine_select_trace_timing("cleanup-conservative-skipped", machine_select_now_s() - stage_start);
            machine_select_trace_timing("total", machine_select_now_s() - total_start);
        }
        return 1;
    }
    if (!machine_select_program_should_skip_cleanup_pure(out_program)) {
        cleanup_changed = machine_select_cleanup_pure_program(out_program, error);
        if (cleanup_changed < 0) {
            machine_select_program_free(out_program);
            return 0;
        }
        if (cleanup_changed > 0) {
            if (trace_timing) {
                machine_select_trace_timing("cleanup-pure", machine_select_now_s() - stage_start);
                stage_start = machine_select_now_s();
            }
            if (!machine_select_verify_program(out_program, error)) {
                machine_select_program_free(out_program);
                return 0;
            }
            if (trace_timing) {
                machine_select_trace_timing("verify-after-pure", machine_select_now_s() - stage_start);
                stage_start = machine_select_now_s();
            }
        } else if (trace_timing) {
            machine_select_trace_timing("cleanup-pure", machine_select_now_s() - stage_start);
            stage_start = machine_select_now_s();
        }
    } else if (trace_timing) {
        machine_select_trace_timing("cleanup-pure-skipped", machine_select_now_s() - stage_start);
        stage_start = machine_select_now_s();
    }
    if (!machine_select_program_should_skip_reuse_addr_roots(out_program)) {
        if (machine_select_cleanup_reuse_addr_roots_program(out_program)) {
            if (trace_timing) {
                machine_select_trace_timing("cleanup-reuse-addr-roots", machine_select_now_s() - stage_start);
                stage_start = machine_select_now_s();
            }
            if (!machine_select_verify_program(out_program, error)) {
                machine_select_program_free(out_program);
                return 0;
            }
            if (trace_timing) {
                machine_select_trace_timing("verify-after-reuse-addr-roots", machine_select_now_s() - stage_start);
                stage_start = machine_select_now_s();
            }
        } else if (trace_timing) {
            machine_select_trace_timing("cleanup-reuse-addr-roots", machine_select_now_s() - stage_start);
            stage_start = machine_select_now_s();
        }
    } else if (trace_timing) {
        machine_select_trace_timing("cleanup-reuse-addr-roots-skipped", machine_select_now_s() - stage_start);
        stage_start = machine_select_now_s();
    }
    if (!machine_select_skip_cleanup_reuse_internal_pure_calls_enabled()) {
        cleanup_changed = machine_select_cleanup_reuse_internal_pure_calls_program(out_program, error);
        if (cleanup_changed < 0) {
            machine_select_program_free(out_program);
            return 0;
        }
        if (cleanup_changed > 0) {
            if (trace_timing) {
                machine_select_trace_timing("cleanup-reuse-internal-pure-calls", machine_select_now_s() - stage_start);
                stage_start = machine_select_now_s();
            }
            cleanup_changed = machine_select_cleanup_pure_program(out_program, error);
            if (cleanup_changed < 0) {
                machine_select_program_free(out_program);
                return 0;
            }
            if (trace_timing) {
                machine_select_trace_timing("cleanup-pure-after-reuse-internal-pure-calls", machine_select_now_s() - stage_start);
                stage_start = machine_select_now_s();
            }
            if (!machine_select_verify_program(out_program, error)) {
                machine_select_program_free(out_program);
                return 0;
            }
            if (trace_timing) {
                machine_select_trace_timing("verify-after-reuse-internal-pure-calls", machine_select_now_s() - stage_start);
                stage_start = machine_select_now_s();
            }
        } else if (trace_timing) {
            machine_select_trace_timing("cleanup-reuse-internal-pure-calls", machine_select_now_s() - stage_start);
            stage_start = machine_select_now_s();
        }
    } else if (trace_timing) {
        machine_select_trace_timing("cleanup-reuse-internal-pure-calls-skipped", machine_select_now_s() - stage_start);
        stage_start = machine_select_now_s();
    }
    if (!machine_select_skip_cleanup_reuse_unique_predecessor_pure_calls_enabled()) {
        cleanup_changed = machine_select_cleanup_reuse_unique_predecessor_pure_calls_program(out_program, error);
        if (cleanup_changed < 0) {
            machine_select_program_free(out_program);
            return 0;
        }
        if (cleanup_changed > 0) {
            if (trace_timing) {
                machine_select_trace_timing("cleanup-reuse-unique-predecessor-pure-calls", machine_select_now_s() - stage_start);
                stage_start = machine_select_now_s();
            }
            cleanup_changed = machine_select_cleanup_pure_program(out_program, error);
            if (cleanup_changed < 0) {
                machine_select_program_free(out_program);
                return 0;
            }
            if (trace_timing) {
                machine_select_trace_timing("cleanup-pure-after-reuse-unique-predecessor-pure-calls", machine_select_now_s() - stage_start);
                stage_start = machine_select_now_s();
            }
            if (!machine_select_verify_program(out_program, error)) {
                machine_select_program_free(out_program);
                return 0;
            }
            if (trace_timing) {
                machine_select_trace_timing("verify-after-reuse-unique-predecessor-pure-calls", machine_select_now_s() - stage_start);
                stage_start = machine_select_now_s();
            }
        } else if (trace_timing) {
            machine_select_trace_timing("cleanup-reuse-unique-predecessor-pure-calls", machine_select_now_s() - stage_start);
            stage_start = machine_select_now_s();
        }
    } else if (trace_timing) {
        machine_select_trace_timing("cleanup-reuse-unique-predecessor-pure-calls-skipped", machine_select_now_s() - stage_start);
        stage_start = machine_select_now_s();
    }
    if (!machine_select_skip_cleanup_forward_same_block_indirect_loads_enabled() &&
        machine_select_program_uses_indirect_memory(out_program) &&
        machine_select_cleanup_forward_same_block_indirect_loads_program(out_program)) {
        if (trace_timing) {
            machine_select_trace_timing("cleanup-forward-same-block-indirect-loads", machine_select_now_s() - stage_start);
            stage_start = machine_select_now_s();
        }
        cleanup_changed = machine_select_cleanup_pure_program(out_program, error);
        if (cleanup_changed < 0) {
            machine_select_program_free(out_program);
            return 0;
        }
        if (trace_timing) {
            machine_select_trace_timing("cleanup-pure-after-forward-same-block-indirect-loads", machine_select_now_s() - stage_start);
            stage_start = machine_select_now_s();
        }
        if (!machine_select_verify_program(out_program, error)) {
            machine_select_program_free(out_program);
            return 0;
        }
        if (trace_timing) {
            machine_select_trace_timing("verify-after-forward-same-block-indirect-loads", machine_select_now_s() - stage_start);
            stage_start = machine_select_now_s();
        }
    } else if (trace_timing) {
        machine_select_trace_timing("cleanup-forward-same-block-indirect-loads", machine_select_now_s() - stage_start);
        stage_start = machine_select_now_s();
    }
    if (!machine_select_skip_cleanup_reuse_spill_pure_expr_enabled() &&
        machine_select_program_uses_indirect_memory(out_program) &&
        machine_select_cleanup_reuse_spill_pure_expr_program(out_program)) {
        if (trace_timing) {
            machine_select_trace_timing("cleanup-reuse-spill-pure-expr", machine_select_now_s() - stage_start);
            stage_start = machine_select_now_s();
        }
        cleanup_changed = machine_select_cleanup_pure_program(out_program, error);
        if (cleanup_changed < 0) {
            machine_select_program_free(out_program);
            return 0;
        }
        if (trace_timing) {
            machine_select_trace_timing("cleanup-pure-after-reuse-spill-pure-expr", machine_select_now_s() - stage_start);
            stage_start = machine_select_now_s();
        }
        if (!machine_select_verify_program(out_program, error)) {
            machine_select_program_free(out_program);
            return 0;
        }
        if (trace_timing) {
            machine_select_trace_timing("verify-after-reuse-spill-pure-expr", machine_select_now_s() - stage_start);
            stage_start = machine_select_now_s();
        }
    } else if (trace_timing) {
        machine_select_trace_timing("cleanup-reuse-spill-pure-expr", machine_select_now_s() - stage_start);
        stage_start = machine_select_now_s();
    }
    if (!machine_select_program_uses_indirect_memory(out_program)) {
        if (!machine_select_cleanup_program(out_program, error)) {
            machine_select_program_free(out_program);
            return 0;
        }
        if (trace_timing) {
            machine_select_trace_timing("cleanup-full", machine_select_now_s() - stage_start);
            stage_start = machine_select_now_s();
        }
        if (!machine_select_verify_program(out_program, error)) {
            machine_select_program_free(out_program);
            return 0;
        }
        if (trace_timing) {
            machine_select_trace_timing("verify-after-full", machine_select_now_s() - stage_start);
            machine_select_trace_timing("total", machine_select_now_s() - total_start);
        }
    } else if (trace_timing) {
        machine_select_trace_timing("cleanup-full-skipped-indirect-memory", machine_select_now_s() - stage_start);
        machine_select_trace_timing("total", machine_select_now_s() - total_start);
    }
    return 1;
}

int machine_select_lower_canonicalized_program_from_machine_ir(const MachineIrProgram *source,
    MachineSelectProgram *out_program,
    MachineSelectError *error) {
    MachineIrProgram copy;
    MachineIrError machine_error;
    int ok;

    if (!source || !out_program) {
        machine_select_set_error(error, 0, 0, "MACHINE-SELECT-316: invalid canonicalized lowering contract");
        return 0;
    }

    memset(&machine_error, 0, sizeof(machine_error));
    machine_ir_program_init(&copy);
    ok = machine_ir_build_canonicalized_program(source, &copy, &machine_error);
    if (!ok) {
        machine_select_set_error(error, machine_error.line, machine_error.column, "%s", machine_error.message);
        machine_ir_program_free(&copy);
        return 0;
    }
    ok = machine_select_lower_program_from_machine_ir(&copy, out_program, error);
    machine_ir_program_free(&copy);
    return ok;
}

#endif
