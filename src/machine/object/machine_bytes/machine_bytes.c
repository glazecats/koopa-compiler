#include "machine/bytes.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} MachineBytesStringBuilder;

static void machine_bytes_set_error(MachineBytesError *error, int line, int column, const char *fmt, ...);
static char *machine_bytes_strdup(const char *text);
static int machine_bytes_append_format(MachineBytesStringBuilder *builder, const char *fmt, ...);
static int machine_bytes_target_profile_is_valid(MachineBytesTargetProfile profile);
static const char *machine_bytes_target_profile_name(MachineBytesTargetProfile profile);
static int machine_bytes_profile_supports_register_bank(
    MachineBytesTargetProfile profile,
    size_t register_count,
    MachineBytesError *error);
static int machine_bytes_op_has_call_payload(MachineSelectOpKind kind);
static unsigned char machine_bytes_encode_op_kind(MachineSelectOpKind kind);
static unsigned char machine_bytes_encode_terminator_kind(MachineLayoutTerminatorKind kind);
static unsigned char machine_bytes_truncate_u4(size_t value);
static int machine_bytes_bytecode_u8_target_fits(size_t value);
static int machine_bytes_bytecode_u4_target_fits(size_t value);
static int machine_bytes_bytecode_u8_immediate_fits(long long value);
static int machine_bytes_bytecode_u4_immediate_fits(long long value);
static int machine_bytes_bytecode_operand_descriptor_fits(const MachineEmitOperand *operand);
static int machine_bytes_bytecode_slot_imm_payload_fits(
    const MachineEmitSlotRef *slot,
    const MachineEmitOperand *operand);
static int machine_bytes_verify_op_encoding_for_profile(
    MachineBytesTargetProfile profile,
    const MachineEmitOp *op,
    MachineBytesError *error);
static int machine_bytes_verify_control_target_encoding_for_profile(
    MachineBytesTargetProfile profile,
    const MachineEmitTerminator *terminator,
    MachineBytesError *error);
static int machine_bytes_verify_terminator_encoding_for_profile(
    MachineBytesTargetProfile profile,
    const MachineEmitTerminator *terminator,
    MachineBytesError *error);
static unsigned char machine_bytes_encode_operand_descriptor(const MachineEmitOperand *operand);
static unsigned char machine_bytes_encode_slot_imm_payload(
    const MachineEmitSlotRef *slot,
    const MachineEmitOperand *operand);
static unsigned char machine_bytes_encode_target_pair(size_t primary_target, size_t secondary_target);
static unsigned char machine_bytes_encode_compare_flags(
    MachineIrBinaryOp op,
    unsigned char is_immediate_rhs,
    unsigned char is_fallthrough_form,
    unsigned char branch_on_true);
static size_t machine_bytes_op_encoded_size_for_profile(
    MachineBytesTargetProfile profile,
    const MachineBytesFunction *function,
    const MachineEmitOp *op);
static size_t machine_bytes_terminator_encoded_size_for_profile(
    MachineBytesTargetProfile profile,
    const MachineBytesFunction *function,
    const MachineEmitTerminator *terminator);
static int machine_bytes_write_block_bytes_for_profile(
    MachineBytesTargetProfile profile,
    const MachineBytesProgram *program,
    const MachineBytesFunction *function,
    size_t function_byte_offset,
    const size_t *function_byte_offsets,
    MachineBytesBlock *dest_block,
    MachineBytesError *error);
static int machine_bytes_riscv_is_signed_12_immediate(long long imm);
static int machine_bytes_riscv_is_shift_immediate(long long imm);
static int machine_bytes_riscv_is_branch_immediate(long long imm);
static int machine_bytes_riscv_is_jump_immediate(long long imm);
static int machine_bytes_riscv_immediate_uses_zero_register(long long imm);
static size_t machine_bytes_riscv_materialize_size(long long imm);
static uint32_t machine_bytes_riscv_encode_i_type(
    uint32_t opcode,
    uint32_t rd,
    uint32_t funct3,
    uint32_t rs1,
    long long imm);
static uint32_t machine_bytes_riscv_encode_u_type(uint32_t opcode, uint32_t rd, long long imm);
static uint32_t machine_bytes_riscv_encode_r_type(
    uint32_t opcode,
    uint32_t rd,
    uint32_t funct3,
    uint32_t rs1,
    uint32_t rs2,
    uint32_t funct7);
static uint32_t machine_bytes_riscv_encode_s_type(
    uint32_t opcode,
    uint32_t funct3,
    uint32_t rs1,
    uint32_t rs2,
    long long imm);
static uint32_t machine_bytes_riscv_encode_b_type(
    uint32_t opcode,
    uint32_t funct3,
    uint32_t rs1,
    uint32_t rs2,
    long long imm);
static uint32_t machine_bytes_riscv_encode_j_type(uint32_t opcode, uint32_t rd, long long imm);
static void machine_bytes_write_u32_le(unsigned char *bytes, size_t offset, uint32_t value);
static size_t machine_bytes_riscv_emit_materialize_immediate(
    unsigned char *bytes,
    size_t offset,
    uint32_t rd,
    long long imm);
static size_t machine_bytes_riscv_emit_copy_register(
    unsigned char *bytes,
    size_t offset,
    uint32_t rd,
    uint32_t rs);
static size_t machine_bytes_align_up(size_t value, size_t alignment);
static size_t machine_bytes_riscv_stack_arg_count(const MachineEmitOp *op);
static size_t machine_bytes_riscv_stack_arg_byte_count(const MachineEmitOp *op);
static size_t machine_bytes_riscv_adjust_sp_size(long long delta);
static size_t machine_bytes_riscv_emit_adjust_sp(
    unsigned char *bytes,
    size_t offset,
    long long delta);
static size_t machine_bytes_riscv_load_from_base_size(long long byte_offset);
static size_t machine_bytes_riscv_store_to_base_size(long long byte_offset);
static size_t machine_bytes_riscv_emit_load_from_base(
    unsigned char *bytes,
    size_t offset,
    uint32_t rd,
    uint32_t base_reg,
    long long byte_offset,
    uint32_t base_scratch);
static size_t machine_bytes_riscv_emit_store_to_base(
    unsigned char *bytes,
    size_t offset,
    uint32_t base_reg,
    uint32_t rs,
    long long byte_offset,
    uint32_t base_scratch);
static size_t machine_bytes_riscv_load_from_stack_size(long long stack_byte_offset);
static size_t machine_bytes_riscv_store_to_stack_size(long long stack_byte_offset);
static size_t machine_bytes_riscv_emit_load_from_stack(
    unsigned char *bytes,
    size_t offset,
    uint32_t rd,
    long long stack_byte_offset,
    uint32_t base_scratch);
static size_t machine_bytes_riscv_emit_store_to_stack(
    unsigned char *bytes,
    size_t offset,
    uint32_t rs,
    long long stack_byte_offset,
    uint32_t base_scratch);
static uint32_t machine_bytes_riscv_result_scratch_register(void);
static uint32_t machine_bytes_riscv_effective_operand_register(
    const MachineEmitOperand *operand,
    uint32_t scratch_reg);
static size_t machine_bytes_riscv_prepare_operand_size(
    const MachineBytesFunction *function,
    const MachineEmitOperand *operand);
static size_t machine_bytes_riscv_emit_prepare_operand(
    const MachineBytesFunction *function,
    unsigned char *bytes,
    size_t offset,
    const MachineEmitOperand *operand,
    uint32_t scratch_reg);
static size_t machine_bytes_riscv_move_operand_to_register_size(
    const MachineBytesFunction *function,
    const MachineEmitOperand *operand);
static size_t machine_bytes_riscv_emit_move_operand_to_register(
    const MachineBytesFunction *function,
    unsigned char *bytes,
    size_t offset,
    const MachineEmitOperand *operand,
    uint32_t target_reg);
static uint32_t machine_bytes_riscv_result_register(const MachineEmitOperand *result);
static size_t machine_bytes_riscv_emit_writeback_result(
    const MachineBytesFunction *function,
    unsigned char *bytes,
    size_t offset,
    const MachineEmitOperand *result,
    uint32_t produced_reg);
static size_t machine_bytes_riscv_writeback_result_size(
    const MachineBytesFunction *function,
    const MachineEmitOperand *result,
    uint32_t produced_reg);
static size_t machine_bytes_riscv_call_setup_size(
    const MachineBytesFunction *function,
    const MachineEmitOp *op);
static size_t machine_bytes_riscv_call_result_size(
    const MachineBytesFunction *function,
    const MachineEmitOp *op);
static size_t machine_bytes_riscv_call_patch_offset(
    const MachineBytesFunction *function,
    size_t owner_byte_offset,
    const MachineEmitOp *op);
static uint32_t machine_bytes_riscv_scratch_register(void);
static uint32_t machine_bytes_riscv_secondary_scratch_register(void);
static uint32_t machine_bytes_riscv_map_register_id(size_t machine_register_id);
static uint32_t machine_bytes_riscv_map_operand_register(const MachineEmitOperand *operand);
static uint32_t machine_bytes_riscv_slot_base_register(const MachineEmitSlotRef *slot);
static long long machine_bytes_riscv_slot_offset(const MachineEmitSlotRef *slot);
static long long machine_bytes_riscv_spill_slot_offset(
    const MachineBytesFunction *function,
    size_t spill_slot);
static int machine_bytes_riscv_can_encode_alu_imm_word(const MachineEmitOp *op);
static int machine_bytes_riscv_can_encode_compare_branch_imm_word(const MachineEmitTerminator *terminator);
static size_t machine_bytes_riscv_cmp_core_size(MachineIrBinaryOp op);
static size_t machine_bytes_riscv_cmp_result_size(
    const MachineBytesFunction *function,
    const MachineEmitOp *op);
static size_t machine_bytes_riscv_emit_cmp_result(
    const MachineBytesFunction *function,
    unsigned char *bytes,
    size_t offset,
    const MachineEmitOp *op);
static MachineIrBinaryOp machine_bytes_riscv_invert_compare_op(MachineIrBinaryOp op);
static uint32_t machine_bytes_riscv_encode_alu_word_from_regs(
    MachineIrBinaryOp op,
    uint32_t rd,
    uint32_t rs1,
    uint32_t rs2);
static uint32_t machine_bytes_riscv_encode_cmp_word_from_regs(
    MachineIrBinaryOp op,
    uint32_t rd,
    uint32_t rs1,
    uint32_t rs2);
static uint32_t machine_bytes_riscv_encode_branch_zero_word(
    uint32_t rs1,
    unsigned char branch_on_true,
    long long imm);
static uint32_t machine_bytes_riscv_encode_compare_branch_word_from_parts(
    MachineIrBinaryOp op,
    uint32_t rs1,
    uint32_t rs2,
    long long imm);
static uint32_t machine_bytes_riscv_encode_call_word(long long imm);
static int machine_bytes_riscv_call_prefers_pair(const MachineEmitOp *op);
static uint32_t machine_bytes_riscv_encode_return_word(void);
static uint32_t machine_bytes_riscv_encode_jump_word(long long imm);
static long long machine_bytes_riscv_pc_relative_imm(
    size_t instruction_byte_offset,
    size_t target_byte_offset);
static int machine_bytes_riscv_resolve_call_target_byte_offset(
    const MachineBytesProgram *program,
    const size_t *function_byte_offsets,
    const MachineEmitOp *op,
    size_t *out_target_byte_offset);
static int machine_bytes_riscv_resolve_block_target_byte_offset(
    const MachineBytesFunction *function,
    size_t function_byte_offset,
    size_t target_index,
    size_t *out_target_byte_offset);
static void machine_bytes_debug_materialize_failure(
    const char *tag,
    const MachineBytesBlock *block,
    size_t byte_index,
    size_t op_index,
    const MachineEmitOp *op,
    const MachineEmitTerminator *terminator);
static int machine_bytes_reference_patch_for_call(
    MachineBytesTargetProfile profile,
    size_t owner_byte_offset,
    size_t owner_byte_count,
    size_t *out_patch_byte_offset,
    size_t *out_patch_byte_count);
static int machine_bytes_reference_patch_for_control(
    MachineBytesTargetProfile profile,
    MachineBytesReferenceKind kind,
    MachineBytesTerminatorKind terminator_kind,
    size_t owner_byte_offset,
    size_t owner_byte_count,
    size_t *out_patch_byte_offset,
    size_t *out_patch_byte_count);
static int machine_bytes_reference_patch_for_global_access(
    MachineBytesTargetProfile profile,
    const MachineEmitOp *op,
    const MachineBytesFunction *function,
    size_t owner_byte_offset,
    size_t owner_byte_count,
    size_t *out_patch_byte_offset,
    size_t *out_patch_byte_count);
static int machine_bytes_verify_target_index(size_t target_index, size_t block_count);
static int machine_bytes_op_clone(MachineEmitOp *dest, const MachineEmitOp *src);
static void machine_bytes_op_free(MachineEmitOp *op);
static void machine_bytes_block_free(MachineBytesBlock *block);
static void machine_bytes_function_free(MachineBytesFunction *function);
static void machine_bytes_register_desc_free(MachineEmitRegisterDesc *desc);
static size_t machine_bytes_function_total_byte_count(const MachineBytesFunction *function);
static const MachineBytesBlock *machine_bytes_target_block(const MachineBytesFunction *function, size_t target_index);
static size_t machine_bytes_function_reference_count(const MachineBytesFunction *function);
static int machine_bytes_copy_function_bytes(
    const MachineBytesFunction *function,
    unsigned char *dest_bytes,
    size_t dest_byte_count);
static int machine_bytes_report_append_call_reference(
    MachineBytesReferenceSummary *summary,
    MachineBytesTargetProfile profile,
    size_t function_index,
    size_t emit_index,
    const MachineBytesBlock *block,
    size_t owner_byte_offset,
    size_t owner_byte_count,
    const MachineEmitOp *op,
    const MachineBytesFunction *function,
    const MachineBytesProgram *program,
    const size_t *function_byte_offsets);
static int machine_bytes_report_append_control_reference(
    MachineBytesReferenceSummary *summary,
    MachineBytesReferenceKind kind,
    size_t function_index,
    size_t emit_index,
    const MachineBytesBlock *block,
    size_t owner_byte_offset,
    size_t owner_byte_count,
    size_t patch_byte_offset,
    size_t patch_byte_count,
    MachineBytesTerminatorKind terminator_kind,
    const char *target_name,
    size_t target_function_index,
    int has_target_emit_index,
    size_t target_emit_index,
    int has_target_byte_offset,
    size_t target_byte_offset);
static int machine_bytes_report_append_global_data_reference(
    MachineBytesReferenceSummary *summary,
    MachineBytesReferenceKind kind,
    MachineBytesTargetProfile profile,
    size_t function_index,
    size_t emit_index,
    const MachineBytesBlock *block,
    size_t owner_byte_offset,
    size_t owner_byte_count,
    const MachineEmitOp *op,
    const MachineBytesFunction *function,
    const MachineBytesProgram *program);
static int machine_bytes_report_populate_function_references(
    const MachineBytesReport *report,
    size_t function_index,
    size_t start_reference_index);
static size_t machine_bytes_report_find_symbol_index_by_name(
    const MachineBytesReport *report,
    const char *symbol_name);
static int machine_bytes_report_populate_symbols(MachineBytesReport *report);
static int machine_bytes_report_populate_fixups(MachineBytesReport *report);
static int machine_bytes_report_populate_sections(MachineBytesReport *report);
static int machine_bytes_report_build_symbol_name_index(MachineBytesReport *report);
static int machine_bytes_clone_block_payload(MachineBytesBlock *dest, const MachineEncodeBlock *src);
static int machine_bytes_clone_block_bytes(MachineBytesBlock *dest, const MachineBytesBlock *src);
static const MachineBytesBlock *machine_bytes_target_block(const MachineBytesFunction *function, size_t target_index);
static void machine_bytes_report_clear_shape(MachineBytesReport *report);

static void machine_bytes_set_error(MachineBytesError *error, int line, int column, const char *fmt, ...) {
    va_list args;

    if (!error || !fmt) {
        return;
    }
    error->line = line;
    error->column = column;
    va_start(args, fmt);
    vsnprintf(error->message, sizeof(error->message), fmt, args);
    va_end(args);
}

static char *machine_bytes_strdup(const char *text) {
    char *copy;
    size_t length;

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

static int machine_bytes_append_format(MachineBytesStringBuilder *builder, const char *fmt, ...) {
    va_list args;
    va_list args_copy;
    int needed;
    size_t required_length;
    char *new_data;

    if (!builder || !fmt) {
        return 0;
    }

    va_start(args, fmt);
    va_copy(args_copy, args);
    needed = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);
    if (needed < 0) {
        va_end(args);
        return 0;
    }

    required_length = builder->length + (size_t)needed + 1;
    if (required_length > builder->capacity) {
        size_t next_capacity = builder->capacity == 0 ? 128 : builder->capacity;

        while (next_capacity < required_length) {
            if (next_capacity > ((size_t)-1) / 2) {
                va_end(args);
                return 0;
            }
            next_capacity *= 2;
        }
        new_data = (char *)realloc(builder->data, next_capacity);
        if (!new_data) {
            va_end(args);
            return 0;
        }
        builder->data = new_data;
        builder->capacity = next_capacity;
    }

    vsnprintf(builder->data + builder->length, builder->capacity - builder->length, fmt, args);
    builder->length += (size_t)needed;
    va_end(args);
    return 1;
}

static int machine_bytes_target_profile_is_valid(MachineBytesTargetProfile profile) {
    switch (profile) {
        case MACHINE_BYTES_TARGET_PROFILE_GENERIC:
        case MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW:
        case MACHINE_BYTES_TARGET_PROFILE_I386_PREVIEW:
            return 1;
        default:
            return 0;
    }
}

static const char *machine_bytes_target_profile_name(MachineBytesTargetProfile profile) {
    switch (profile) {
        case MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW:
            return "riscv32-preview";
        case MACHINE_BYTES_TARGET_PROFILE_I386_PREVIEW:
            return "i386-preview";
        case MACHINE_BYTES_TARGET_PROFILE_GENERIC:
        default:
            return "generic";
    }
}

int machine_bytes_get_target_policy_summary(MachineBytesTargetProfile profile,
    MachineBytesTargetPolicySummary *out_summary) {
    if (!out_summary || !machine_bytes_target_profile_is_valid(profile)) {
        return 0;
    }

    memset(out_summary, 0, sizeof(*out_summary));
    out_summary->target_profile = profile;
    if (profile == MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW) {
        out_summary->max_logical_machine_register_count = 8u;
        out_summary->preserves_known_internal_pc_relative_targets = 1;
        out_summary->preserves_direct_fallthrough_honesty = 1;
        out_summary->uses_paired_global_data_addressing = 1;
        out_summary->supports_rv32m_alu_ops = 1;
    }
    return 1;
}

static int machine_bytes_profile_supports_register_bank(
    MachineBytesTargetProfile profile,
    size_t register_count,
    MachineBytesError *error) {
    if (profile == MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW && register_count > 8u) {
        machine_bytes_set_error(
            error,
            0,
            0,
            "MACHINE-BYTES-124: riscv32-preview currently supports at most 8 logical machine registers");
        return 0;
    }
    return 1;
}

static int machine_bytes_op_has_call_payload(MachineSelectOpKind kind) {
    switch (kind) {
        case MACHINE_SELECT_OP_CALL:
        case MACHINE_SELECT_OP_CALL_IMM:
        case MACHINE_SELECT_OP_CALL_SPILL:
        case MACHINE_SELECT_OP_CALL_IMM_SPILL:
        case MACHINE_SELECT_OP_CALL_VOID:
        case MACHINE_SELECT_OP_CALL_VOID_IMM:
            return 1;
        default:
            return 0;
    }
}

static unsigned char machine_bytes_encode_op_kind(MachineSelectOpKind kind) {
    return (unsigned char)(0x10u + (unsigned char)kind);
}

static unsigned char machine_bytes_encode_terminator_kind(MachineLayoutTerminatorKind kind) {
    return (unsigned char)(0x80u + (unsigned char)kind);
}

static unsigned char machine_bytes_truncate_u4(size_t value) {
    return (unsigned char)(value > 0x0Fu ? 0x0Fu : value);
}

static int machine_bytes_bytecode_u8_target_fits(size_t value) {
    return value <= 0xFFu;
}

static int machine_bytes_bytecode_u4_target_fits(size_t value) {
    return value <= 0x0Fu;
}

static int machine_bytes_bytecode_u8_immediate_fits(long long value) {
    return value >= 0 && value <= 0xFFll;
}

static int machine_bytes_bytecode_u4_immediate_fits(long long value) {
    return value >= 0 && value <= 0x0Fll;
}

static int machine_bytes_bytecode_operand_descriptor_fits(const MachineEmitOperand *operand) {
    if (!operand) {
        return 1;
    }
    switch (operand->kind) {
        case MACHINE_SELECT_OPERAND_NONE:
            return 1;
        case MACHINE_SELECT_OPERAND_IMMEDIATE:
            return machine_bytes_bytecode_u4_immediate_fits(operand->immediate);
        case MACHINE_SELECT_OPERAND_REGISTER:
            return machine_bytes_bytecode_u4_target_fits(operand->machine_register_id);
        case MACHINE_SELECT_OPERAND_SPILL_SLOT:
            return machine_bytes_bytecode_u4_target_fits(operand->spill_slot);
    }
    return 0;
}

static int machine_bytes_bytecode_slot_imm_payload_fits(
    const MachineEmitSlotRef *slot,
    const MachineEmitOperand *operand) {
    if (slot && !machine_bytes_bytecode_u4_target_fits(slot->id)) {
        return 0;
    }
    if (!operand) {
        return 1;
    }
    return operand->kind == MACHINE_SELECT_OPERAND_IMMEDIATE &&
        machine_bytes_bytecode_u4_immediate_fits(operand->immediate);
}

static int machine_bytes_verify_op_encoding_for_profile(
    MachineBytesTargetProfile profile,
    const MachineEmitOp *op,
    MachineBytesError *error) {
    size_t arg_index;

    if (!op) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-143: missing op for encoding check");
        return 0;
    }
    if (profile == MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW) {
        return 1;
    }

    switch (op->kind) {
        case MACHINE_SELECT_OP_MATERIALIZE_IMM:
            if (!machine_bytes_bytecode_u8_immediate_fits(op->as.copy_value.immediate)) {
                machine_bytes_set_error(
                    error, 0, 0, "MACHINE-BYTES-144: bytecode materialize immediate exceeds 8-bit encoding");
                return 0;
            }
            return 1;
        case MACHINE_SELECT_OP_ALU_IMM:
        case MACHINE_SELECT_OP_CMP_IMM:
            if (!machine_bytes_bytecode_u8_immediate_fits(op->as.binary.rhs.immediate)) {
                machine_bytes_set_error(
                    error, 0, 0, "MACHINE-BYTES-145: bytecode binary immediate exceeds 8-bit encoding");
                return 0;
            }
            return 1;
        case MACHINE_SELECT_OP_STORE_LOCAL_IMM:
        case MACHINE_SELECT_OP_STORE_GLOBAL_IMM:
            if (!machine_bytes_bytecode_slot_imm_payload_fits(&op->as.store.slot, &op->as.store.value)) {
                machine_bytes_set_error(
                    error, 0, 0, "MACHINE-BYTES-146: bytecode store-immediate payload exceeds 4-bit slot/immediate encoding");
                return 0;
            }
            return 1;
        case MACHINE_SELECT_OP_CALL:
        case MACHINE_SELECT_OP_CALL_IMM:
        case MACHINE_SELECT_OP_CALL_SPILL:
        case MACHINE_SELECT_OP_CALL_IMM_SPILL:
        case MACHINE_SELECT_OP_CALL_VOID:
        case MACHINE_SELECT_OP_CALL_VOID_IMM:
            if (op->as.call.arg_count > 0xFFu) {
                machine_bytes_set_error(
                    error, 0, 0, "MACHINE-BYTES-147: bytecode call arg-count exceeds 8-bit encoding");
                return 0;
            }
            for (arg_index = 0u; arg_index < op->as.call.arg_count; ++arg_index) {
                if (!machine_bytes_bytecode_operand_descriptor_fits(&op->as.call.args[arg_index])) {
                    machine_bytes_set_error(
                        error, 0, 0, "MACHINE-BYTES-148: bytecode call argument exceeds 4-bit operand encoding");
                    return 0;
                }
            }
            return 1;
        default:
            return 1;
    }
}

static int machine_bytes_verify_control_target_encoding_for_profile(
    MachineBytesTargetProfile profile,
    const MachineEmitTerminator *terminator,
    MachineBytesError *error) {
    if (!terminator) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-136: missing control terminator for encoding check");
        return 0;
    }
    if (profile == MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW) {
        return 1;
    }

    switch (terminator->kind) {
        case MACHINE_LAYOUT_TERM_FALLTHROUGH:
            if (!machine_bytes_bytecode_u8_target_fits(terminator->as.fallthrough_target)) {
                machine_bytes_set_error(
                    error,
                    0,
                    0,
                    "MACHINE-BYTES-137: bytecode fallthrough target exceeds 8-bit encoding");
                return 0;
            }
            return 1;
        case MACHINE_LAYOUT_TERM_JUMP:
            if (!machine_bytes_bytecode_u8_target_fits(terminator->as.jump_target)) {
                machine_bytes_set_error(
                    error,
                    0,
                    0,
                    "MACHINE-BYTES-138: bytecode jump target exceeds 8-bit encoding");
                return 0;
            }
            return 1;
        case MACHINE_LAYOUT_TERM_BRANCH:
            if (!machine_bytes_bytecode_u4_target_fits(terminator->as.branch.then_target) ||
                !machine_bytes_bytecode_u4_target_fits(terminator->as.branch.else_target)) {
                machine_bytes_set_error(
                    error,
                    0,
                    0,
                    "MACHINE-BYTES-139: bytecode branch target exceeds 4-bit paired encoding");
                return 0;
            }
            return 1;
        case MACHINE_LAYOUT_TERM_BRANCH_FALLTHROUGH:
            if (!machine_bytes_bytecode_u4_target_fits(terminator->as.branch_fallthrough.taken_target) ||
                !machine_bytes_bytecode_u4_target_fits(terminator->as.branch_fallthrough.fallthrough_target)) {
                machine_bytes_set_error(
                    error,
                    0,
                    0,
                    "MACHINE-BYTES-140: bytecode branch-fallthrough target exceeds 4-bit paired encoding");
                return 0;
            }
            return 1;
        case MACHINE_LAYOUT_TERM_COMPARE_BRANCH:
        case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM:
            if (!machine_bytes_bytecode_u4_target_fits(terminator->as.compare_branch.then_target) ||
                !machine_bytes_bytecode_u4_target_fits(terminator->as.compare_branch.else_target)) {
                machine_bytes_set_error(
                    error,
                    0,
                    0,
                    "MACHINE-BYTES-141: bytecode compare-branch target exceeds 4-bit paired encoding");
                return 0;
            }
            return 1;
        case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_FALLTHROUGH:
        case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM_FALLTHROUGH:
            if (!machine_bytes_bytecode_u4_target_fits(terminator->as.compare_branch_fallthrough.taken_target) ||
                !machine_bytes_bytecode_u4_target_fits(
                    terminator->as.compare_branch_fallthrough.fallthrough_target)) {
                machine_bytes_set_error(
                    error,
                    0,
                    0,
                    "MACHINE-BYTES-142: bytecode compare-branch-fallthrough target exceeds 4-bit paired encoding");
                return 0;
            }
            return 1;
        default:
            return 1;
    }
}

static int machine_bytes_verify_terminator_encoding_for_profile(
    MachineBytesTargetProfile profile,
    const MachineEmitTerminator *terminator,
    MachineBytesError *error) {
    if (!terminator) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-149: missing terminator for encoding check");
        return 0;
    }
    if (profile == MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW) {
        return 1;
    }
    if (!machine_bytes_verify_control_target_encoding_for_profile(profile, terminator, error)) {
        return 0;
    }

    switch (terminator->kind) {
        case MACHINE_LAYOUT_TERM_RETURN_IMM:
        case MACHINE_LAYOUT_TERM_RETURN_SPILL:
            if (!machine_bytes_bytecode_operand_descriptor_fits(&terminator->as.return_value)) {
                machine_bytes_set_error(
                    error, 0, 0, "MACHINE-BYTES-150: bytecode return operand exceeds 4-bit descriptor encoding");
                return 0;
            }
            return 1;
        case MACHINE_LAYOUT_TERM_BRANCH:
            if (!machine_bytes_bytecode_operand_descriptor_fits(&terminator->as.branch.condition)) {
                machine_bytes_set_error(
                    error, 0, 0, "MACHINE-BYTES-151: bytecode branch condition exceeds 4-bit descriptor encoding");
                return 0;
            }
            return 1;
        case MACHINE_LAYOUT_TERM_BRANCH_FALLTHROUGH:
            if (!machine_bytes_bytecode_operand_descriptor_fits(&terminator->as.branch_fallthrough.condition)) {
                machine_bytes_set_error(
                    error, 0, 0, "MACHINE-BYTES-152: bytecode branch-fallthrough condition exceeds 4-bit descriptor encoding");
                return 0;
            }
            return 1;
        case MACHINE_LAYOUT_TERM_COMPARE_BRANCH:
            if (!machine_bytes_bytecode_operand_descriptor_fits(&terminator->as.compare_branch.rhs)) {
                machine_bytes_set_error(
                    error, 0, 0, "MACHINE-BYTES-153: bytecode compare-branch rhs exceeds 4-bit descriptor encoding");
                return 0;
            }
            return 1;
        case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM:
            if (!machine_bytes_bytecode_u8_immediate_fits(terminator->as.compare_branch.rhs.immediate)) {
                machine_bytes_set_error(
                    error, 0, 0, "MACHINE-BYTES-154: bytecode compare-branch immediate exceeds 8-bit encoding");
                return 0;
            }
            return 1;
        case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_FALLTHROUGH:
            if (!machine_bytes_bytecode_operand_descriptor_fits(&terminator->as.compare_branch_fallthrough.rhs)) {
                machine_bytes_set_error(
                    error, 0, 0, "MACHINE-BYTES-155: bytecode compare-branch-fallthrough rhs exceeds 4-bit descriptor encoding");
                return 0;
            }
            return 1;
        case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM_FALLTHROUGH:
            if (!machine_bytes_bytecode_u8_immediate_fits(
                    terminator->as.compare_branch_fallthrough.rhs.immediate)) {
                machine_bytes_set_error(
                    error, 0, 0, "MACHINE-BYTES-156: bytecode compare-branch-fallthrough immediate exceeds 8-bit encoding");
                return 0;
            }
            return 1;
        default:
            return 1;
    }
}

static unsigned char machine_bytes_encode_operand_descriptor(const MachineEmitOperand *operand) {
    if (!operand) {
        return 0x00u;
    }

    switch (operand->kind) {
        case MACHINE_SELECT_OPERAND_IMMEDIATE:
            return (unsigned char)(0x10u | ((unsigned char)operand->immediate & 0x0Fu));
        case MACHINE_SELECT_OPERAND_REGISTER:
            return (unsigned char)(0x20u | machine_bytes_truncate_u4(operand->machine_register_id));
        case MACHINE_SELECT_OPERAND_SPILL_SLOT:
            return (unsigned char)(0x30u | machine_bytes_truncate_u4(operand->spill_slot));
        case MACHINE_SELECT_OPERAND_NONE:
        default:
            return 0x00u;
    }
}

static unsigned char machine_bytes_encode_slot_imm_payload(
    const MachineEmitSlotRef *slot,
    const MachineEmitOperand *operand) {
    unsigned char slot_low = 0u;
    unsigned char value_low = 0u;

    if (slot) {
        slot_low = machine_bytes_truncate_u4(slot->id);
        if (slot->kind == MACHINE_SELECT_SLOT_GLOBAL) {
            slot_low = (unsigned char)(0x80u | slot_low);
        } else {
            slot_low = (unsigned char)(0x40u | slot_low);
        }
    }
    if (operand && operand->kind == MACHINE_SELECT_OPERAND_IMMEDIATE) {
        value_low = (unsigned char)operand->immediate & 0x0Fu;
    }
    return (unsigned char)(slot_low | value_low);
}

static unsigned char machine_bytes_encode_target_pair(size_t primary_target, size_t secondary_target) {
    return (unsigned char)((machine_bytes_truncate_u4(primary_target) << 4) |
        machine_bytes_truncate_u4(secondary_target));
}

static unsigned char machine_bytes_encode_compare_flags(
    MachineIrBinaryOp op,
    unsigned char is_immediate_rhs,
    unsigned char is_fallthrough_form,
    unsigned char branch_on_true) {
    unsigned char flags = (unsigned char)op;

    if (is_immediate_rhs) {
        flags = (unsigned char)(flags | 0x20u);
    }
    if (is_fallthrough_form) {
        flags = (unsigned char)(flags | 0x40u);
    }
    if (branch_on_true) {
        flags = (unsigned char)(flags | 0x80u);
    }
    return flags;
}

static size_t machine_bytes_op_encoded_size_for_profile(
    MachineBytesTargetProfile profile,
    const MachineBytesFunction *function,
    const MachineEmitOp *op) {
    const MachineEmitOperand *imm_operand = NULL;

    if (!op) {
        return 0;
    }
    if (profile == MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW) {
        switch (op->kind) {
            case MACHINE_SELECT_OP_COPY:
                if (op->result.kind == MACHINE_SELECT_OPERAND_SPILL_SLOT) {
                    return machine_bytes_riscv_move_operand_to_register_size(function, &op->as.copy_value) +
                        machine_bytes_riscv_writeback_result_size(
                            function,
                            &op->result,
                            machine_bytes_riscv_result_scratch_register());
                }
                return machine_bytes_riscv_move_operand_to_register_size(function, &op->as.copy_value);
            case MACHINE_SELECT_OP_MATERIALIZE_IMM:
                return machine_bytes_riscv_materialize_size(op->as.copy_value.immediate) +
                    machine_bytes_riscv_writeback_result_size(
                        function,
                        &op->result,
                        machine_bytes_riscv_result_register(&op->result));
            case MACHINE_SELECT_OP_ADDR_LOCAL:
                return (machine_bytes_riscv_is_signed_12_immediate(
                            machine_bytes_riscv_slot_offset(&op->as.addr_slot))
                        ? 4u
                        : machine_bytes_riscv_materialize_size(
                              machine_bytes_riscv_slot_offset(&op->as.addr_slot)) +
                            4u) +
                    machine_bytes_riscv_writeback_result_size(
                        function,
                        &op->result,
                        machine_bytes_riscv_result_register(&op->result));
            case MACHINE_SELECT_OP_ADDR_GLOBAL:
                return 8u +
                    machine_bytes_riscv_writeback_result_size(
                        function,
                        &op->result,
                        machine_bytes_riscv_result_register(&op->result));
            case MACHINE_SELECT_OP_LOAD_LOCAL:
                return machine_bytes_riscv_load_from_base_size(
                        machine_bytes_riscv_slot_offset(&op->as.load_slot)) +
                    machine_bytes_riscv_writeback_result_size(
                        function,
                        &op->result,
                        machine_bytes_riscv_result_register(&op->result));
            case MACHINE_SELECT_OP_LOAD_GLOBAL:
                return 8u + machine_bytes_riscv_writeback_result_size(
                        function,
                        &op->result,
                        machine_bytes_riscv_result_register(&op->result));
            case MACHINE_SELECT_OP_STORE_LOCAL:
                return machine_bytes_riscv_prepare_operand_size(function, &op->as.store.value) +
                    machine_bytes_riscv_store_to_base_size(
                        machine_bytes_riscv_slot_offset(&op->as.store.slot));
            case MACHINE_SELECT_OP_STORE_GLOBAL:
                return machine_bytes_riscv_prepare_operand_size(function, &op->as.store.value) + 8u;
            case MACHINE_SELECT_OP_STORE_LOCAL_IMM:
                return (machine_bytes_riscv_immediate_uses_zero_register(op->as.store.value.immediate)
                        ? 0u
                        : machine_bytes_riscv_materialize_size(op->as.store.value.immediate)) +
                    machine_bytes_riscv_store_to_base_size(
                        machine_bytes_riscv_slot_offset(&op->as.store.slot));
            case MACHINE_SELECT_OP_STORE_GLOBAL_IMM:
                return (machine_bytes_riscv_immediate_uses_zero_register(op->as.store.value.immediate)
                        ? 0u
                        : machine_bytes_riscv_materialize_size(op->as.store.value.immediate)) +
                    8u;
            case MACHINE_SELECT_OP_LOAD_INDIRECT:
                return machine_bytes_riscv_prepare_operand_size(function, &op->as.load_indirect_addr) +
                    4u +
                    machine_bytes_riscv_writeback_result_size(
                        function,
                        &op->result,
                        machine_bytes_riscv_result_register(&op->result));
            case MACHINE_SELECT_OP_STORE_INDIRECT:
                return machine_bytes_riscv_prepare_operand_size(function, &op->as.store_indirect.addr) +
                    machine_bytes_riscv_prepare_operand_size(function, &op->as.store_indirect.value) +
                    4u;
            case MACHINE_SELECT_OP_CALL:
            case MACHINE_SELECT_OP_CALL_IMM:
            case MACHINE_SELECT_OP_CALL_SPILL:
            case MACHINE_SELECT_OP_CALL_IMM_SPILL:
            case MACHINE_SELECT_OP_CALL_VOID:
            case MACHINE_SELECT_OP_CALL_VOID_IMM:
                return machine_bytes_riscv_call_setup_size(function, op) +
                    (machine_bytes_riscv_call_prefers_pair(op) ? 8u : 4u) +
                    machine_bytes_riscv_call_result_size(function, op);
            case MACHINE_SELECT_OP_ALU_IMM:
                imm_operand = op->as.binary.lhs.kind == MACHINE_SELECT_OPERAND_IMMEDIATE
                    ? &op->as.binary.lhs
                    : &op->as.binary.rhs;
                return machine_bytes_riscv_prepare_operand_size(function, &op->as.binary.lhs) +
                    (machine_bytes_riscv_can_encode_alu_imm_word(op)
                        ? 4u
                        : machine_bytes_riscv_materialize_size(imm_operand->immediate) + 4u) +
                    machine_bytes_riscv_writeback_result_size(
                        function,
                        &op->result,
                        machine_bytes_riscv_result_register(&op->result));
            case MACHINE_SELECT_OP_ALU:
                return machine_bytes_riscv_prepare_operand_size(function, &op->as.binary.lhs) +
                    machine_bytes_riscv_prepare_operand_size(function, &op->as.binary.rhs) +
                    4u +
                    machine_bytes_riscv_writeback_result_size(
                        function,
                        &op->result,
                        machine_bytes_riscv_result_register(&op->result));
            case MACHINE_SELECT_OP_CMP:
            case MACHINE_SELECT_OP_CMP_IMM:
                return machine_bytes_riscv_cmp_result_size(function, op);
            default:
                return 4u;
        }
    }
    switch (op->kind) {
        case MACHINE_SELECT_OP_MATERIALIZE_IMM:
        case MACHINE_SELECT_OP_STORE_LOCAL_IMM:
        case MACHINE_SELECT_OP_STORE_GLOBAL_IMM:
        case MACHINE_SELECT_OP_ALU_IMM:
        case MACHINE_SELECT_OP_CMP_IMM:
            return 2;
        case MACHINE_SELECT_OP_CALL:
        case MACHINE_SELECT_OP_CALL_IMM:
        case MACHINE_SELECT_OP_CALL_SPILL:
        case MACHINE_SELECT_OP_CALL_IMM_SPILL:
        case MACHINE_SELECT_OP_CALL_VOID:
        case MACHINE_SELECT_OP_CALL_VOID_IMM:
            return 2 + op->as.call.arg_count;
        default:
            return 1;
    }
}

static size_t machine_bytes_terminator_encoded_size_for_profile(
    MachineBytesTargetProfile profile,
    const MachineBytesFunction *function,
    const MachineEmitTerminator *terminator) {
    const MachineEmitOperand *imm_operand = NULL;

    if (!terminator) {
        return 0;
    }
    if (profile == MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW) {
        switch (terminator->kind) {
            case MACHINE_LAYOUT_TERM_RETURN:
                if (terminator->as.return_value.kind == MACHINE_SELECT_OPERAND_REGISTER &&
                    machine_bytes_riscv_map_operand_register(&terminator->as.return_value) != 10u) {
                    return 8u;
                }
                return 4u;
            case MACHINE_LAYOUT_TERM_JUMP:
                return 4u;
            case MACHINE_LAYOUT_TERM_FALLTHROUGH:
                return 0u;
            case MACHINE_LAYOUT_TERM_RETURN_IMM:
                return machine_bytes_riscv_materialize_size(terminator->as.return_value.immediate) + 4u;
            case MACHINE_LAYOUT_TERM_RETURN_SPILL:
                return machine_bytes_riscv_load_from_stack_size(
                    machine_bytes_riscv_spill_slot_offset(function, terminator->as.return_value.spill_slot)) + 4u;
            case MACHINE_LAYOUT_TERM_BRANCH:
                return machine_bytes_riscv_prepare_operand_size(function, &terminator->as.branch.condition) + 8u;
            case MACHINE_LAYOUT_TERM_BRANCH_FALLTHROUGH:
                return machine_bytes_riscv_prepare_operand_size(function, &terminator->as.branch_fallthrough.condition) + 4u;
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH:
                return machine_bytes_riscv_prepare_operand_size(function, &terminator->as.compare_branch.lhs) +
                    machine_bytes_riscv_prepare_operand_size(function, &terminator->as.compare_branch.rhs) + 8u;
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_FALLTHROUGH:
                return machine_bytes_riscv_prepare_operand_size(function, &terminator->as.compare_branch_fallthrough.lhs) +
                    machine_bytes_riscv_prepare_operand_size(function, &terminator->as.compare_branch_fallthrough.rhs) + 4u;
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM:
                imm_operand = terminator->as.compare_branch.lhs.kind == MACHINE_SELECT_OPERAND_IMMEDIATE
                    ? &terminator->as.compare_branch.lhs
                    : &terminator->as.compare_branch.rhs;
                return machine_bytes_riscv_prepare_operand_size(function, &terminator->as.compare_branch.lhs) +
                    (machine_bytes_riscv_can_encode_compare_branch_imm_word(terminator)
                        ? 8u
                        : machine_bytes_riscv_materialize_size(imm_operand->immediate) + 8u);
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM_FALLTHROUGH:
                imm_operand = terminator->as.compare_branch_fallthrough.lhs.kind == MACHINE_SELECT_OPERAND_IMMEDIATE
                    ? &terminator->as.compare_branch_fallthrough.lhs
                    : &terminator->as.compare_branch_fallthrough.rhs;
                return machine_bytes_riscv_prepare_operand_size(function, &terminator->as.compare_branch_fallthrough.lhs) +
                    (machine_bytes_riscv_can_encode_compare_branch_imm_word(terminator)
                        ? 4u
                        : machine_bytes_riscv_materialize_size(imm_operand->immediate) + 4u);
            default:
                return 4u;
        }
    }
    switch (terminator->kind) {
        case MACHINE_LAYOUT_TERM_RETURN_IMM:
        case MACHINE_LAYOUT_TERM_RETURN_SPILL:
        case MACHINE_LAYOUT_TERM_JUMP:
        case MACHINE_LAYOUT_TERM_FALLTHROUGH:
            return 2;
        case MACHINE_LAYOUT_TERM_BRANCH:
        case MACHINE_LAYOUT_TERM_BRANCH_FALLTHROUGH:
            return 3;
        case MACHINE_LAYOUT_TERM_COMPARE_BRANCH:
        case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM:
        case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_FALLTHROUGH:
        case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM_FALLTHROUGH:
            return 4;
        default:
            return 1;
    }
}

static uint32_t machine_bytes_riscv_encode_i_type(
    uint32_t opcode,
    uint32_t rd,
    uint32_t funct3,
    uint32_t rs1,
    long long imm) {
    uint32_t imm12 = (uint32_t)((uint64_t)imm & 0xFFFu);

    return (imm12 << 20) |
        ((rs1 & 0x1Fu) << 15) |
        ((funct3 & 0x7u) << 12) |
        ((rd & 0x1Fu) << 7) |
        (opcode & 0x7Fu);
}

static uint32_t machine_bytes_riscv_encode_u_type(uint32_t opcode, uint32_t rd, long long imm) {
    uint32_t imm20 = (uint32_t)((uint64_t)imm & 0xFFFFF000u);

    return imm20 |
        ((rd & 0x1Fu) << 7) |
        (opcode & 0x7Fu);
}

static uint32_t machine_bytes_riscv_encode_r_type(
    uint32_t opcode,
    uint32_t rd,
    uint32_t funct3,
    uint32_t rs1,
    uint32_t rs2,
    uint32_t funct7) {
    return ((funct7 & 0x7Fu) << 25) |
        ((rs2 & 0x1Fu) << 20) |
        ((rs1 & 0x1Fu) << 15) |
        ((funct3 & 0x7u) << 12) |
        ((rd & 0x1Fu) << 7) |
        (opcode & 0x7Fu);
}

static uint32_t machine_bytes_riscv_encode_s_type(
    uint32_t opcode,
    uint32_t funct3,
    uint32_t rs1,
    uint32_t rs2,
    long long imm) {
    uint32_t imm12 = (uint32_t)((uint64_t)imm & 0xFFFu);

    return (((imm12 >> 5) & 0x7Fu) << 25) |
        ((rs2 & 0x1Fu) << 20) |
        ((rs1 & 0x1Fu) << 15) |
        ((funct3 & 0x7u) << 12) |
        ((imm12 & 0x1Fu) << 7) |
        (opcode & 0x7Fu);
}

static uint32_t machine_bytes_riscv_encode_b_type(
    uint32_t opcode,
    uint32_t funct3,
    uint32_t rs1,
    uint32_t rs2,
    long long imm) {
    uint32_t imm13 = (uint32_t)((uint64_t)imm & 0x1FFFu);

    return (((imm13 >> 12) & 0x1u) << 31) |
        (((imm13 >> 5) & 0x3Fu) << 25) |
        ((rs2 & 0x1Fu) << 20) |
        ((rs1 & 0x1Fu) << 15) |
        ((funct3 & 0x7u) << 12) |
        (((imm13 >> 1) & 0xFu) << 8) |
        (((imm13 >> 11) & 0x1u) << 7) |
        (opcode & 0x7Fu);
}

static uint32_t machine_bytes_riscv_encode_j_type(uint32_t opcode, uint32_t rd, long long imm) {
    uint32_t imm21 = (uint32_t)((uint64_t)imm & 0x1FFFFFu);

    return (((imm21 >> 20) & 0x1u) << 31) |
        (((imm21 >> 1) & 0x3FFu) << 21) |
        (((imm21 >> 11) & 0x1u) << 20) |
        (((imm21 >> 12) & 0xFFu) << 12) |
        ((rd & 0x1Fu) << 7) |
        (opcode & 0x7Fu);
}

static void machine_bytes_write_u32_le(unsigned char *bytes, size_t offset, uint32_t value) {
    bytes[offset] = (unsigned char)(value & 0xFFu);
    bytes[offset + 1u] = (unsigned char)((value >> 8u) & 0xFFu);
    bytes[offset + 2u] = (unsigned char)((value >> 16u) & 0xFFu);
    bytes[offset + 3u] = (unsigned char)((value >> 24u) & 0xFFu);
}

static int machine_bytes_riscv_is_signed_12_immediate(long long imm) {
    return imm >= -2048ll && imm <= 2047ll;
}

static int machine_bytes_riscv_is_shift_immediate(long long imm) {
    return imm >= 0ll && imm <= 31ll;
}

static int machine_bytes_riscv_is_branch_immediate(long long imm) {
    return (imm & 1ll) == 0ll && imm >= -4096ll && imm <= 4094ll;
}

static int machine_bytes_riscv_is_jump_immediate(long long imm) {
    return (imm & 1ll) == 0ll && imm >= -1048576ll && imm <= 1048574ll;
}

static int machine_bytes_riscv_immediate_uses_zero_register(long long imm) {
    return imm == 0ll;
}

static size_t machine_bytes_riscv_materialize_size(long long imm) {
    return machine_bytes_riscv_is_signed_12_immediate(imm) ? 4u : 8u;
}

static size_t machine_bytes_riscv_emit_materialize_immediate(
    unsigned char *bytes,
    size_t offset,
    uint32_t rd,
    long long imm) {
    if (machine_bytes_riscv_is_signed_12_immediate(imm)) {
        machine_bytes_write_u32_le(bytes, offset, machine_bytes_riscv_encode_i_type(0x13u, rd, 0x0u, 0u, imm));
        return 4u;
    }

    {
        long long upper = (imm + 0x800ll) >> 12;
        long long lower = imm - (upper << 12);

        machine_bytes_write_u32_le(bytes, offset, machine_bytes_riscv_encode_u_type(0x37u, rd, upper << 12));
        machine_bytes_write_u32_le(bytes, offset + 4u, machine_bytes_riscv_encode_i_type(0x13u, rd, 0x0u, rd, lower));
    }
    return 8u;
}

static size_t machine_bytes_riscv_emit_copy_register(
    unsigned char *bytes,
    size_t offset,
    uint32_t rd,
    uint32_t rs) {
    machine_bytes_write_u32_le(bytes, offset, machine_bytes_riscv_encode_i_type(0x13u, rd, 0x0u, rs, 0));
    return 4u;
}

static size_t machine_bytes_align_up(size_t value, size_t alignment) {
    size_t remainder;

    if (alignment == 0u) {
        return value;
    }
    remainder = value % alignment;
    return remainder == 0u ? value : value + (alignment - remainder);
}

static size_t machine_bytes_riscv_emit_register_call_args(
    const MachineBytesFunction *function,
    unsigned char *bytes,
    size_t offset,
    const MachineEmitOp *op,
    size_t stack_arg_bytes) {
    size_t arg_index;
    uint32_t target_regs[8];
    uint32_t source_regs[8];
    unsigned char source_is_register[8];
    unsigned char done[8] = {0};
    size_t remaining = 0u;

    if (!op) {
        return 0u;
    }

    for (arg_index = 0u; arg_index < 8u; ++arg_index) {
        target_regs[arg_index] = 10u + (uint32_t)arg_index;
        source_regs[arg_index] = 0u;
        source_is_register[arg_index] = 0u;
    }

    for (arg_index = 0u; arg_index < op->as.call.arg_count && arg_index < 8u; ++arg_index) {
        const MachineEmitOperand *arg = &op->as.call.args[arg_index];

        if (arg->kind == MACHINE_SELECT_OPERAND_REGISTER) {
            source_is_register[arg_index] = 1u;
            source_regs[arg_index] = machine_bytes_riscv_map_operand_register(arg);
        }
        ++remaining;
    }

    while (remaining > 0u) {
        size_t ready_index = (size_t)-1;
        size_t cycle_index = (size_t)-1;

        for (arg_index = 0u; arg_index < op->as.call.arg_count && arg_index < 8u; ++arg_index) {
            size_t other_index;
            int blocked = 0;

            if (done[arg_index]) {
                continue;
            }

            if (cycle_index == (size_t)-1) {
                cycle_index = arg_index;
            }

            if (source_is_register[arg_index] && source_regs[arg_index] == target_regs[arg_index]) {
                ready_index = arg_index;
                break;
            }

            for (other_index = 0u; other_index < op->as.call.arg_count && other_index < 8u; ++other_index) {
                if (other_index == arg_index || done[other_index] || !source_is_register[other_index]) {
                    continue;
                }
                if (source_regs[other_index] == target_regs[arg_index]) {
                    blocked = 1;
                    break;
                }
            }
            if (!blocked) {
                ready_index = arg_index;
                break;
            }
        }

        if (ready_index != (size_t)-1) {
            const MachineEmitOperand *arg = &op->as.call.args[ready_index];
            uint32_t target_reg = target_regs[ready_index];

            if (arg->kind == MACHINE_SELECT_OPERAND_IMMEDIATE) {
                offset += machine_bytes_riscv_emit_materialize_immediate(
                    bytes, offset, target_reg, arg->immediate);
            } else if (arg->kind == MACHINE_SELECT_OPERAND_SPILL_SLOT) {
                offset += machine_bytes_riscv_emit_load_from_stack(
                    bytes,
                    offset,
                    target_reg,
                    machine_bytes_riscv_spill_slot_offset(function, arg->spill_slot) + (long long)stack_arg_bytes,
                    machine_bytes_riscv_scratch_register());
            } else {
                uint32_t source_reg = source_regs[ready_index];

                if (source_reg != target_reg) {
                    offset += machine_bytes_riscv_emit_copy_register(bytes, offset, target_reg, source_reg);
                }
            }

            done[ready_index] = 1u;
            --remaining;
            continue;
        }

        if (cycle_index == (size_t)-1) {
            break;
        }

        offset += machine_bytes_riscv_emit_copy_register(
            bytes,
            offset,
            machine_bytes_riscv_secondary_scratch_register(),
            target_regs[cycle_index]);
        for (arg_index = 0u; arg_index < op->as.call.arg_count && arg_index < 8u; ++arg_index) {
            if (!done[arg_index] && source_is_register[arg_index] &&
                source_regs[arg_index] == target_regs[cycle_index]) {
                source_regs[arg_index] = machine_bytes_riscv_secondary_scratch_register();
                source_is_register[arg_index] = 1u;
            }
        }
    }

    return offset;
}

static size_t machine_bytes_riscv_register_call_args_size(
    const MachineBytesFunction *function,
    const MachineEmitOp *op,
    size_t stack_arg_bytes) {
    size_t arg_index;
    uint32_t target_regs[8];
    uint32_t source_regs[8];
    unsigned char source_is_register[8];
    unsigned char done[8] = {0};
    size_t remaining = 0u;
    size_t size = 0u;

    if (!op) {
        return 0u;
    }

    for (arg_index = 0u; arg_index < 8u; ++arg_index) {
        target_regs[arg_index] = 10u + (uint32_t)arg_index;
        source_regs[arg_index] = 0u;
        source_is_register[arg_index] = 0u;
    }

    for (arg_index = 0u; arg_index < op->as.call.arg_count && arg_index < 8u; ++arg_index) {
        const MachineEmitOperand *arg = &op->as.call.args[arg_index];

        if (arg->kind == MACHINE_SELECT_OPERAND_REGISTER) {
            source_is_register[arg_index] = 1u;
            source_regs[arg_index] = machine_bytes_riscv_map_operand_register(arg);
        }
        ++remaining;
    }

    while (remaining > 0u) {
        size_t ready_index = (size_t)-1;
        size_t cycle_index = (size_t)-1;

        for (arg_index = 0u; arg_index < op->as.call.arg_count && arg_index < 8u; ++arg_index) {
            size_t other_index;
            int blocked = 0;

            if (done[arg_index]) {
                continue;
            }

            if (cycle_index == (size_t)-1) {
                cycle_index = arg_index;
            }

            if (source_is_register[arg_index] && source_regs[arg_index] == target_regs[arg_index]) {
                ready_index = arg_index;
                break;
            }

            for (other_index = 0u; other_index < op->as.call.arg_count && other_index < 8u; ++other_index) {
                if (other_index == arg_index || done[other_index] || !source_is_register[other_index]) {
                    continue;
                }
                if (source_regs[other_index] == target_regs[arg_index]) {
                    blocked = 1;
                    break;
                }
            }
            if (!blocked) {
                ready_index = arg_index;
                break;
            }
        }

        if (ready_index != (size_t)-1) {
            const MachineEmitOperand *arg = &op->as.call.args[ready_index];
            uint32_t target_reg = target_regs[ready_index];

            if (arg->kind == MACHINE_SELECT_OPERAND_IMMEDIATE) {
                size += machine_bytes_riscv_materialize_size(arg->immediate);
            } else if (arg->kind == MACHINE_SELECT_OPERAND_SPILL_SLOT) {
                size += machine_bytes_riscv_load_from_stack_size(
                    machine_bytes_riscv_spill_slot_offset(function, arg->spill_slot) + (long long)stack_arg_bytes);
            } else {
                uint32_t source_reg = source_regs[ready_index];

                if (source_reg != target_reg) {
                    size += 4u;
                }
            }

            done[ready_index] = 1u;
            --remaining;
            continue;
        }

        if (cycle_index == (size_t)-1) {
            break;
        }

        size += 4u;
        for (arg_index = 0u; arg_index < op->as.call.arg_count && arg_index < 8u; ++arg_index) {
            if (!done[arg_index] && source_is_register[arg_index] &&
                source_regs[arg_index] == target_regs[cycle_index]) {
                source_regs[arg_index] = machine_bytes_riscv_secondary_scratch_register();
                source_is_register[arg_index] = 1u;
            }
        }
    }

    return size;
}

static size_t machine_bytes_riscv_stack_arg_count(const MachineEmitOp *op) {
    if (!op || op->as.call.arg_count <= 8u) {
        return 0u;
    }
    return op->as.call.arg_count - 8u;
}

static size_t machine_bytes_riscv_stack_arg_byte_count(const MachineEmitOp *op) {
    size_t raw_bytes = machine_bytes_riscv_stack_arg_count(op) * 4u;

    if (raw_bytes == 0u) {
        return 0u;
    }
    return machine_bytes_align_up(raw_bytes, 16u);
}

static size_t machine_bytes_riscv_adjust_sp_size(long long delta) {
    return machine_bytes_riscv_is_signed_12_immediate(delta)
        ? 4u
        : machine_bytes_riscv_materialize_size(delta) + 4u;
}

static size_t machine_bytes_riscv_emit_adjust_sp(
    unsigned char *bytes,
    size_t offset,
    long long delta) {
    if (machine_bytes_riscv_is_signed_12_immediate(delta)) {
        machine_bytes_write_u32_le(bytes, offset, machine_bytes_riscv_encode_i_type(0x13u, 2u, 0x0u, 2u, delta));
        return 4u;
    }

    offset += machine_bytes_riscv_emit_materialize_immediate(
        bytes, offset, machine_bytes_riscv_scratch_register(), delta);
    machine_bytes_write_u32_le(
        bytes,
        offset,
        machine_bytes_riscv_encode_r_type(
            0x33u,
            2u,
            0x0u,
            2u,
            machine_bytes_riscv_scratch_register(),
            0x00u));
    return machine_bytes_riscv_materialize_size(delta) + 4u;
}

static size_t machine_bytes_riscv_load_from_base_size(long long byte_offset) {
    return machine_bytes_riscv_is_signed_12_immediate(byte_offset)
        ? 4u
        : machine_bytes_riscv_materialize_size(byte_offset) + 8u;
}

static size_t machine_bytes_riscv_store_to_base_size(long long byte_offset) {
    return machine_bytes_riscv_is_signed_12_immediate(byte_offset)
        ? 4u
        : machine_bytes_riscv_materialize_size(byte_offset) + 8u;
}

static size_t machine_bytes_riscv_emit_load_from_base(
    unsigned char *bytes,
    size_t offset,
    uint32_t rd,
    uint32_t base_reg,
    long long byte_offset,
    uint32_t base_scratch) {
    if (machine_bytes_riscv_is_signed_12_immediate(byte_offset)) {
        machine_bytes_write_u32_le(
            bytes, offset, machine_bytes_riscv_encode_i_type(0x03u, rd, 0x2u, base_reg, byte_offset));
        return 4u;
    }

    offset += machine_bytes_riscv_emit_materialize_immediate(bytes, offset, base_scratch, byte_offset);
    machine_bytes_write_u32_le(
        bytes,
        offset,
        machine_bytes_riscv_encode_r_type(0x33u, base_scratch, 0x0u, base_reg, base_scratch, 0x00u));
    machine_bytes_write_u32_le(
        bytes,
        offset + 4u,
        machine_bytes_riscv_encode_i_type(0x03u, rd, 0x2u, base_scratch, 0));
    return machine_bytes_riscv_materialize_size(byte_offset) + 8u;
}

static size_t machine_bytes_riscv_emit_store_to_base(
    unsigned char *bytes,
    size_t offset,
    uint32_t base_reg,
    uint32_t rs,
    long long byte_offset,
    uint32_t base_scratch) {
    if (machine_bytes_riscv_is_signed_12_immediate(byte_offset)) {
        machine_bytes_write_u32_le(
            bytes, offset, machine_bytes_riscv_encode_s_type(0x23u, 0x2u, base_reg, rs, byte_offset));
        return 4u;
    }

    offset += machine_bytes_riscv_emit_materialize_immediate(bytes, offset, base_scratch, byte_offset);
    machine_bytes_write_u32_le(
        bytes,
        offset,
        machine_bytes_riscv_encode_r_type(0x33u, base_scratch, 0x0u, base_reg, base_scratch, 0x00u));
    machine_bytes_write_u32_le(
        bytes,
        offset + 4u,
        machine_bytes_riscv_encode_s_type(0x23u, 0x2u, base_scratch, rs, 0));
    return machine_bytes_riscv_materialize_size(byte_offset) + 8u;
}

static size_t machine_bytes_riscv_load_from_stack_size(long long stack_byte_offset) {
    return machine_bytes_riscv_load_from_base_size(stack_byte_offset);
}

static size_t machine_bytes_riscv_store_to_stack_size(long long stack_byte_offset) {
    return machine_bytes_riscv_store_to_base_size(stack_byte_offset);
}

static size_t machine_bytes_riscv_emit_load_from_stack(
    unsigned char *bytes,
    size_t offset,
    uint32_t rd,
    long long stack_byte_offset,
    uint32_t base_scratch) {
    return machine_bytes_riscv_emit_load_from_base(bytes, offset, rd, 2u, stack_byte_offset, base_scratch);
}

static size_t machine_bytes_riscv_emit_store_to_stack(
    unsigned char *bytes,
    size_t offset,
    uint32_t rs,
    long long stack_byte_offset,
    uint32_t base_scratch) {
    return machine_bytes_riscv_emit_store_to_base(bytes, offset, 2u, rs, stack_byte_offset, base_scratch);
}

static uint32_t machine_bytes_riscv_result_scratch_register(void) {
    return 29u;
}

static uint32_t machine_bytes_riscv_spill_address_scratch_register(void) {
    /*
     * Keep large spill-slot address materialization off the main operand
     * scratch pair (`t6` / `t5`), otherwise preparing a second spill-backed
     * operand can overwrite the first operand's live value before the ALU op.
     */
    return machine_bytes_riscv_result_scratch_register();
}

static uint32_t machine_bytes_riscv_effective_operand_register(
    const MachineEmitOperand *operand,
    uint32_t scratch_reg) {
    if (!operand) {
        return 0u;
    }
    switch (operand->kind) {
        case MACHINE_SELECT_OPERAND_REGISTER:
            return machine_bytes_riscv_map_register_id(operand->machine_register_id);
        case MACHINE_SELECT_OPERAND_IMMEDIATE:
            return operand->immediate == 0 ? 0u : scratch_reg;
        case MACHINE_SELECT_OPERAND_SPILL_SLOT:
            return scratch_reg;
        case MACHINE_SELECT_OPERAND_NONE:
        default:
            return 0u;
    }
}

static size_t machine_bytes_riscv_prepare_operand_size(
    const MachineBytesFunction *function,
    const MachineEmitOperand *operand) {
    if (!operand) {
        return 0u;
    }
    switch (operand->kind) {
        case MACHINE_SELECT_OPERAND_IMMEDIATE:
            return machine_bytes_riscv_immediate_uses_zero_register(operand->immediate)
                ? 0u
                : machine_bytes_riscv_materialize_size(operand->immediate);
        case MACHINE_SELECT_OPERAND_SPILL_SLOT:
            return machine_bytes_riscv_load_from_stack_size(
                machine_bytes_riscv_spill_slot_offset(function, operand->spill_slot));
        case MACHINE_SELECT_OPERAND_REGISTER:
        case MACHINE_SELECT_OPERAND_NONE:
        default:
            return 0u;
    }
}

static size_t machine_bytes_riscv_emit_prepare_operand(
    const MachineBytesFunction *function,
    unsigned char *bytes,
    size_t offset,
    const MachineEmitOperand *operand,
    uint32_t scratch_reg) {
    if (!operand) {
        return 0u;
    }
    switch (operand->kind) {
        case MACHINE_SELECT_OPERAND_IMMEDIATE:
            if (operand->immediate == 0) {
                return 0u;
            }
            return machine_bytes_riscv_emit_materialize_immediate(
                bytes, offset, scratch_reg, operand->immediate);
        case MACHINE_SELECT_OPERAND_SPILL_SLOT:
            return machine_bytes_riscv_emit_load_from_stack(
                bytes,
                offset,
                scratch_reg,
                machine_bytes_riscv_spill_slot_offset(function, operand->spill_slot),
                machine_bytes_riscv_spill_address_scratch_register());
        case MACHINE_SELECT_OPERAND_REGISTER:
        case MACHINE_SELECT_OPERAND_NONE:
        default:
            return 0u;
    }
}

static size_t machine_bytes_riscv_move_operand_to_register_size(
    const MachineBytesFunction *function,
    const MachineEmitOperand *operand) {
    if (!operand) {
        return 0u;
    }
    switch (operand->kind) {
        case MACHINE_SELECT_OPERAND_IMMEDIATE:
            return machine_bytes_riscv_materialize_size(operand->immediate);
        case MACHINE_SELECT_OPERAND_SPILL_SLOT:
            return machine_bytes_riscv_load_from_stack_size(
                machine_bytes_riscv_spill_slot_offset(function, operand->spill_slot));
        case MACHINE_SELECT_OPERAND_REGISTER:
            return 4u;
        case MACHINE_SELECT_OPERAND_NONE:
        default:
            return 0u;
    }
}

static size_t machine_bytes_riscv_emit_move_operand_to_register(
    const MachineBytesFunction *function,
    unsigned char *bytes,
    size_t offset,
    const MachineEmitOperand *operand,
    uint32_t target_reg) {
    if (!operand) {
        return 0u;
    }
    switch (operand->kind) {
        case MACHINE_SELECT_OPERAND_IMMEDIATE:
            if (operand->immediate == 0) {
                return target_reg == 0u
                    ? 0u
                    : machine_bytes_riscv_emit_copy_register(bytes, offset, target_reg, 0u);
            }
            return machine_bytes_riscv_emit_materialize_immediate(
                bytes, offset, target_reg, operand->immediate);
        case MACHINE_SELECT_OPERAND_SPILL_SLOT:
            return machine_bytes_riscv_emit_load_from_stack(
                bytes,
                offset,
                target_reg,
                machine_bytes_riscv_spill_slot_offset(function, operand->spill_slot),
                machine_bytes_riscv_spill_address_scratch_register());
        case MACHINE_SELECT_OPERAND_REGISTER:
            return machine_bytes_riscv_emit_copy_register(
                bytes,
                offset,
                target_reg,
                machine_bytes_riscv_map_register_id(operand->machine_register_id));
        case MACHINE_SELECT_OPERAND_NONE:
        default:
            return 0u;
    }
}

static uint32_t machine_bytes_riscv_result_register(const MachineEmitOperand *result) {
    if (!result) {
        return 0u;
    }
    if (result->kind == MACHINE_SELECT_OPERAND_REGISTER) {
        return machine_bytes_riscv_map_register_id(result->machine_register_id);
    }
    if (result->kind == MACHINE_SELECT_OPERAND_SPILL_SLOT) {
        return machine_bytes_riscv_result_scratch_register();
    }
    return 0u;
}

static size_t machine_bytes_riscv_emit_writeback_result(
    const MachineBytesFunction *function,
    unsigned char *bytes,
    size_t offset,
    const MachineEmitOperand *result,
    uint32_t produced_reg) {
    if (!result) {
        return 0u;
    }
    if (result->kind == MACHINE_SELECT_OPERAND_SPILL_SLOT) {
        return machine_bytes_riscv_emit_store_to_stack(
            bytes,
            offset,
            produced_reg,
            machine_bytes_riscv_spill_slot_offset(function, result->spill_slot),
            machine_bytes_riscv_scratch_register());
    }
    if (result->kind == MACHINE_SELECT_OPERAND_REGISTER) {
        uint32_t target_reg = machine_bytes_riscv_map_register_id(result->machine_register_id);

        if (target_reg != produced_reg) {
            return machine_bytes_riscv_emit_copy_register(
                bytes, offset, target_reg, produced_reg);
        }
    }
    return 0u;
}

static size_t machine_bytes_riscv_writeback_result_size(
    const MachineBytesFunction *function,
    const MachineEmitOperand *result,
    uint32_t produced_reg) {
    if (!result) {
        return 0u;
    }
    if (result->kind == MACHINE_SELECT_OPERAND_SPILL_SLOT) {
        return machine_bytes_riscv_store_to_stack_size(
            machine_bytes_riscv_spill_slot_offset(function, result->spill_slot));
    }
    if (result->kind == MACHINE_SELECT_OPERAND_REGISTER &&
        machine_bytes_riscv_map_register_id(result->machine_register_id) != produced_reg) {
        return 4u;
    }
    return 0u;
}

static uint32_t machine_bytes_riscv_map_register_id(size_t machine_register_id) {
    return (uint32_t)(10u + (machine_register_id % 8u));
}

static uint32_t machine_bytes_riscv_map_operand_register(const MachineEmitOperand *operand) {
    if (!operand) {
        return 0u;
    }
    switch (operand->kind) {
        case MACHINE_SELECT_OPERAND_REGISTER:
            return machine_bytes_riscv_map_register_id(operand->machine_register_id);
        case MACHINE_SELECT_OPERAND_SPILL_SLOT:
            return (uint32_t)(5u + (operand->spill_slot % 3u));
        case MACHINE_SELECT_OPERAND_IMMEDIATE:
        case MACHINE_SELECT_OPERAND_NONE:
        default:
            return 0u;
    }
}

static uint32_t machine_bytes_riscv_slot_base_register(const MachineEmitSlotRef *slot) {
    if (!slot) {
        return 0u;
    }
    return slot->kind == MACHINE_SELECT_SLOT_GLOBAL ? 3u : 2u;
}

static long long machine_bytes_riscv_slot_offset(const MachineEmitSlotRef *slot) {
    if (!slot) {
        return 0;
    }
    return (long long)(slot->id * 4u);
}

static long long machine_bytes_riscv_spill_slot_offset(
    const MachineBytesFunction *function,
    size_t spill_slot) {
    size_t spill_base = 0u;

    if (function) {
        spill_base = function->local_count;
    }
    return (long long)((spill_base + spill_slot) * 4u);
}

static size_t machine_bytes_riscv_call_setup_size(
    const MachineBytesFunction *function,
    const MachineEmitOp *op) {
    size_t size = 0u;
    size_t arg_index;
    size_t stack_arg_bytes;

    if (!op) {
        return 0u;
    }
    stack_arg_bytes = machine_bytes_riscv_stack_arg_byte_count(op);
    if (stack_arg_bytes > 0u) {
        size += machine_bytes_riscv_adjust_sp_size(-(long long)stack_arg_bytes);
    }
    size += machine_bytes_riscv_register_call_args_size(function, op, stack_arg_bytes);
    for (arg_index = 8u; arg_index < op->as.call.arg_count; ++arg_index) {
        const MachineEmitOperand *arg = &op->as.call.args[arg_index];
        long long stack_byte_offset = (long long)((arg_index - 8u) * 4u);

        if (arg->kind == MACHINE_SELECT_OPERAND_IMMEDIATE) {
            size += machine_bytes_riscv_materialize_size(arg->immediate);
            size += machine_bytes_riscv_store_to_stack_size(stack_byte_offset);
        } else if (arg->kind == MACHINE_SELECT_OPERAND_SPILL_SLOT) {
            size += machine_bytes_riscv_load_from_stack_size(
                machine_bytes_riscv_spill_slot_offset(function, arg->spill_slot) + (long long)stack_arg_bytes);
            size += machine_bytes_riscv_store_to_stack_size(stack_byte_offset);
        } else {
            size += machine_bytes_riscv_store_to_stack_size(stack_byte_offset);
        }
    }
    return size;
}

static size_t machine_bytes_riscv_call_result_size(
    const MachineBytesFunction *function,
    const MachineEmitOp *op) {
    size_t size = 0u;
    size_t stack_arg_bytes;

    if (!op || !op->has_result) {
        return machine_bytes_riscv_stack_arg_byte_count(op) > 0u
            ? machine_bytes_riscv_adjust_sp_size((long long)machine_bytes_riscv_stack_arg_byte_count(op))
            : 0u;
    }

    stack_arg_bytes = machine_bytes_riscv_stack_arg_byte_count(op);
    if (stack_arg_bytes > 0u) {
        size += machine_bytes_riscv_adjust_sp_size((long long)stack_arg_bytes);
    }
    if (op->result.kind == MACHINE_SELECT_OPERAND_REGISTER) {
        return size + (machine_bytes_riscv_map_operand_register(&op->result) == 10u ? 0u : 4u);
    }
    if (op->result.kind == MACHINE_SELECT_OPERAND_SPILL_SLOT) {
        return size + machine_bytes_riscv_store_to_stack_size(
            machine_bytes_riscv_spill_slot_offset(function, op->result.spill_slot));
    }
    return size;
}

static size_t machine_bytes_riscv_call_patch_offset(
    const MachineBytesFunction *function,
    size_t owner_byte_offset,
    const MachineEmitOp *op) {
    return owner_byte_offset + machine_bytes_riscv_call_setup_size(function, op);
}

static uint32_t machine_bytes_riscv_scratch_register(void) {
    return 31u;
}

static uint32_t machine_bytes_riscv_secondary_scratch_register(void) {
    return 30u;
}

static int machine_bytes_riscv_can_encode_alu_imm_word(const MachineEmitOp *op) {
    if (!op ||
        op->kind != MACHINE_SELECT_OP_ALU_IMM ||
        op->as.binary.lhs.kind == MACHINE_SELECT_OPERAND_IMMEDIATE ||
        op->as.binary.rhs.kind != MACHINE_SELECT_OPERAND_IMMEDIATE) {
        return 0;
    }

    switch (op->as.binary.op) {
        case MACHINE_IR_BINARY_ADD:
        case MACHINE_IR_BINARY_BIT_AND:
        case MACHINE_IR_BINARY_BIT_XOR:
        case MACHINE_IR_BINARY_BIT_OR:
        case MACHINE_IR_BINARY_LT:
            return machine_bytes_riscv_is_signed_12_immediate(op->as.binary.rhs.immediate);
        case MACHINE_IR_BINARY_SHIFT_LEFT:
        case MACHINE_IR_BINARY_SHIFT_RIGHT:
            return machine_bytes_riscv_is_shift_immediate(op->as.binary.rhs.immediate);
        default:
            return 0;
    }
}

static int machine_bytes_riscv_can_encode_compare_branch_imm_word(const MachineEmitTerminator *terminator) {
    const MachineEmitOperand *lhs = NULL;
    const MachineEmitOperand *rhs = NULL;

    if (!terminator) {
        return 0;
    }

    switch (terminator->kind) {
        case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM:
            lhs = &terminator->as.compare_branch.lhs;
            rhs = &terminator->as.compare_branch.rhs;
            break;
        case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM_FALLTHROUGH:
            lhs = &terminator->as.compare_branch_fallthrough.lhs;
            rhs = &terminator->as.compare_branch_fallthrough.rhs;
            break;
        default:
            return 0;
    }

    return lhs->kind != MACHINE_SELECT_OPERAND_IMMEDIATE &&
        rhs->kind == MACHINE_SELECT_OPERAND_IMMEDIATE &&
        rhs->immediate == 0;
}

static size_t machine_bytes_riscv_cmp_core_size(MachineIrBinaryOp op) {
    switch (op) {
        case MACHINE_IR_BINARY_LT:
        case MACHINE_IR_BINARY_GT:
            return 4u;
        case MACHINE_IR_BINARY_EQ:
        case MACHINE_IR_BINARY_NE:
        case MACHINE_IR_BINARY_LE:
        case MACHINE_IR_BINARY_GE:
            return 8u;
        default:
            return 4u;
    }
}

static size_t machine_bytes_riscv_cmp_result_size(
    const MachineBytesFunction *function,
    const MachineEmitOp *op) {
    size_t size = 0u;
    uint32_t result_reg;

    if (!op) {
        return 0u;
    }
    result_reg = machine_bytes_riscv_result_register(&op->result);

    if (op->kind == MACHINE_SELECT_OP_CMP) {
        size += machine_bytes_riscv_prepare_operand_size(function, &op->as.binary.lhs);
        size += machine_bytes_riscv_prepare_operand_size(function, &op->as.binary.rhs);
        return size + machine_bytes_riscv_cmp_core_size(op->as.binary.op) +
            machine_bytes_riscv_writeback_result_size(function, &op->result, result_reg);
    }

    if (op->kind == MACHINE_SELECT_OP_CMP_IMM) {
        if (op->as.binary.lhs.kind != MACHINE_SELECT_OPERAND_IMMEDIATE &&
            op->as.binary.rhs.kind == MACHINE_SELECT_OPERAND_IMMEDIATE) {
            long long imm = op->as.binary.rhs.immediate;

            size += machine_bytes_riscv_prepare_operand_size(function, &op->as.binary.lhs);
            switch (op->as.binary.op) {
                case MACHINE_IR_BINARY_EQ:
                case MACHINE_IR_BINARY_NE:
                    return size + (machine_bytes_riscv_is_signed_12_immediate(imm) ? 8u : machine_bytes_riscv_materialize_size(imm) + 8u) +
                        machine_bytes_riscv_writeback_result_size(function, &op->result, result_reg);
                case MACHINE_IR_BINARY_LT:
                    return size + (machine_bytes_riscv_is_signed_12_immediate(imm) ? 4u : machine_bytes_riscv_materialize_size(imm) + 4u) +
                        machine_bytes_riscv_writeback_result_size(function, &op->result, result_reg);
                case MACHINE_IR_BINARY_LE:
                    return size + ((imm < 2047ll && machine_bytes_riscv_is_signed_12_immediate(imm + 1ll))
                        ? 4u
                        : machine_bytes_riscv_materialize_size(imm) + 8u) +
                        machine_bytes_riscv_writeback_result_size(function, &op->result, result_reg);
                case MACHINE_IR_BINARY_GT:
                    return size + ((imm < 2047ll && machine_bytes_riscv_is_signed_12_immediate(imm + 1ll))
                        ? 8u
                        : machine_bytes_riscv_materialize_size(imm) + 4u) +
                        machine_bytes_riscv_writeback_result_size(function, &op->result, result_reg);
                case MACHINE_IR_BINARY_GE:
                    return size + (machine_bytes_riscv_is_signed_12_immediate(imm) ? 8u : machine_bytes_riscv_materialize_size(imm) + 8u) +
                        machine_bytes_riscv_writeback_result_size(function, &op->result, result_reg);
                default:
                    return size + machine_bytes_riscv_materialize_size(imm) + 4u +
                        machine_bytes_riscv_writeback_result_size(function, &op->result, result_reg);
            }
        }

        if (op->as.binary.lhs.kind == MACHINE_SELECT_OPERAND_IMMEDIATE) {
            return machine_bytes_riscv_materialize_size(op->as.binary.lhs.immediate) +
                machine_bytes_riscv_prepare_operand_size(function, &op->as.binary.rhs) +
                machine_bytes_riscv_cmp_core_size(op->as.binary.op) +
                machine_bytes_riscv_writeback_result_size(function, &op->result, result_reg);
        }
        size += machine_bytes_riscv_prepare_operand_size(function, &op->as.binary.lhs);
        return size + machine_bytes_riscv_materialize_size(op->as.binary.rhs.immediate) +
            machine_bytes_riscv_cmp_core_size(op->as.binary.op) +
            machine_bytes_riscv_writeback_result_size(function, &op->result, result_reg);
    }

    return 4u;
}

static size_t machine_bytes_riscv_emit_cmp_result(
    const MachineBytesFunction *function,
    unsigned char *bytes,
    size_t offset,
    const MachineEmitOp *op) {
    uint32_t rd;
    uint32_t rs1;
    uint32_t rs2;
    uint32_t lhs_scratch = machine_bytes_riscv_scratch_register();
    uint32_t rhs_scratch = machine_bytes_riscv_secondary_scratch_register();
    size_t start_offset = offset;

    if (!bytes || !op) {
        return 0u;
    }

    rd = machine_bytes_riscv_result_register(&op->result);
    if (op->kind == MACHINE_SELECT_OP_CMP) {
        rs1 = machine_bytes_riscv_effective_operand_register(&op->as.binary.lhs, lhs_scratch);
        rs2 = machine_bytes_riscv_effective_operand_register(&op->as.binary.rhs, rhs_scratch);
        offset += machine_bytes_riscv_emit_prepare_operand(function, bytes, offset, &op->as.binary.lhs, rs1);
        offset += machine_bytes_riscv_emit_prepare_operand(function, bytes, offset, &op->as.binary.rhs, rs2);
        switch (op->as.binary.op) {
            case MACHINE_IR_BINARY_LT:
            case MACHINE_IR_BINARY_GT:
                machine_bytes_write_u32_le(bytes, offset, machine_bytes_riscv_encode_cmp_word_from_regs(op->as.binary.op, rd, rs1, rs2));
                offset += 4u;
                break;
            case MACHINE_IR_BINARY_EQ:
                machine_bytes_write_u32_le(bytes, offset, machine_bytes_riscv_encode_cmp_word_from_regs(MACHINE_IR_BINARY_EQ, rd, rs1, rs2));
                machine_bytes_write_u32_le(bytes, offset + 4u, machine_bytes_riscv_encode_i_type(0x13u, rd, 0x3u, rd, 1));
                offset += 8u;
                break;
            case MACHINE_IR_BINARY_NE:
                machine_bytes_write_u32_le(bytes, offset, machine_bytes_riscv_encode_cmp_word_from_regs(MACHINE_IR_BINARY_EQ, rd, rs1, rs2));
                machine_bytes_write_u32_le(bytes, offset + 4u, machine_bytes_riscv_encode_r_type(0x33u, rd, 0x3u, 0u, rd, 0x00u));
                offset += 8u;
                break;
            case MACHINE_IR_BINARY_LE:
                machine_bytes_write_u32_le(bytes, offset, machine_bytes_riscv_encode_cmp_word_from_regs(MACHINE_IR_BINARY_LE, rd, rs1, rs2));
                machine_bytes_write_u32_le(bytes, offset + 4u, machine_bytes_riscv_encode_i_type(0x13u, rd, 0x4u, rd, 1));
                offset += 8u;
                break;
            case MACHINE_IR_BINARY_GE:
                machine_bytes_write_u32_le(bytes, offset, machine_bytes_riscv_encode_cmp_word_from_regs(MACHINE_IR_BINARY_GE, rd, rs1, rs2));
                machine_bytes_write_u32_le(bytes, offset + 4u, machine_bytes_riscv_encode_i_type(0x13u, rd, 0x4u, rd, 1));
                offset += 8u;
                break;
            default:
                machine_bytes_write_u32_le(bytes, offset, machine_bytes_riscv_encode_i_type(0x13u, rd, 0x0u, 0u, 0));
                offset += 4u;
                break;
        }
        offset += machine_bytes_riscv_emit_writeback_result(function, bytes, offset, &op->result, rd);
        return offset - start_offset;
    }

    if (op->kind != MACHINE_SELECT_OP_CMP_IMM) {
        return 0u;
    }

    if (op->as.binary.lhs.kind != MACHINE_SELECT_OPERAND_IMMEDIATE &&
        op->as.binary.rhs.kind == MACHINE_SELECT_OPERAND_IMMEDIATE) {
        long long imm = op->as.binary.rhs.immediate;
        rs1 = machine_bytes_riscv_effective_operand_register(&op->as.binary.lhs, lhs_scratch);
        offset += machine_bytes_riscv_emit_prepare_operand(function, bytes, offset, &op->as.binary.lhs, rs1);
        if (getenv("CODEX_MACHINE_BYTES_DEBUG")) {
            fprintf(stderr,
                "[machine-bytes-debug] cmp-imm lhs-prep op=%d after=%zu\n",
                (int)op->as.binary.op,
                offset - start_offset);
        }

        switch (op->as.binary.op) {
            case MACHINE_IR_BINARY_EQ:
                if (machine_bytes_riscv_is_signed_12_immediate(imm)) {
                    machine_bytes_write_u32_le(bytes, offset, machine_bytes_riscv_encode_i_type(0x13u, rd, 0x4u, rs1, imm));
                    machine_bytes_write_u32_le(bytes, offset + 4u, machine_bytes_riscv_encode_i_type(0x13u, rd, 0x3u, rd, 1));
                    offset += 8u;
                    offset += machine_bytes_riscv_emit_writeback_result(function, bytes, offset, &op->result, rd);
                    return offset - start_offset;
                }
                break;
            case MACHINE_IR_BINARY_NE:
                if (machine_bytes_riscv_is_signed_12_immediate(imm)) {
                    machine_bytes_write_u32_le(bytes, offset, machine_bytes_riscv_encode_i_type(0x13u, rd, 0x4u, rs1, imm));
                    machine_bytes_write_u32_le(bytes, offset + 4u, machine_bytes_riscv_encode_r_type(0x33u, rd, 0x3u, 0u, rd, 0x00u));
                    offset += 8u;
                    offset += machine_bytes_riscv_emit_writeback_result(function, bytes, offset, &op->result, rd);
                    return offset - start_offset;
                }
                break;
            case MACHINE_IR_BINARY_LT:
                if (machine_bytes_riscv_is_signed_12_immediate(imm)) {
                    machine_bytes_write_u32_le(bytes, offset, machine_bytes_riscv_encode_i_type(0x13u, rd, 0x2u, rs1, imm));
                    offset += 4u;
                    offset += machine_bytes_riscv_emit_writeback_result(function, bytes, offset, &op->result, rd);
                    return offset - start_offset;
                }
                break;
            case MACHINE_IR_BINARY_LE:
                if (imm < 2047ll && machine_bytes_riscv_is_signed_12_immediate(imm + 1ll)) {
                    machine_bytes_write_u32_le(bytes, offset, machine_bytes_riscv_encode_i_type(0x13u, rd, 0x2u, rs1, imm + 1ll));
                    offset += 4u;
                    offset += machine_bytes_riscv_emit_writeback_result(function, bytes, offset, &op->result, rd);
                    return offset - start_offset;
                }
                break;
            case MACHINE_IR_BINARY_GT:
                if (imm < 2047ll && machine_bytes_riscv_is_signed_12_immediate(imm + 1ll)) {
                    machine_bytes_write_u32_le(bytes, offset, machine_bytes_riscv_encode_i_type(0x13u, rd, 0x2u, rs1, imm + 1ll));
                    machine_bytes_write_u32_le(bytes, offset + 4u, machine_bytes_riscv_encode_i_type(0x13u, rd, 0x4u, rd, 1));
                    offset += 8u;
                    offset += machine_bytes_riscv_emit_writeback_result(function, bytes, offset, &op->result, rd);
                    return offset - start_offset;
                }
                break;
            case MACHINE_IR_BINARY_GE:
                if (machine_bytes_riscv_is_signed_12_immediate(imm)) {
                    machine_bytes_write_u32_le(bytes, offset, machine_bytes_riscv_encode_i_type(0x13u, rd, 0x2u, rs1, imm));
                    machine_bytes_write_u32_le(bytes, offset + 4u, machine_bytes_riscv_encode_i_type(0x13u, rd, 0x4u, rd, 1));
                    offset += 8u;
                    offset += machine_bytes_riscv_emit_writeback_result(function, bytes, offset, &op->result, rd);
                    return offset - start_offset;
                }
                break;
            default:
                break;
        }
        offset = start_offset;
    }

    if (op->as.binary.lhs.kind == MACHINE_SELECT_OPERAND_IMMEDIATE) {
        rs1 = lhs_scratch;
        offset += machine_bytes_riscv_emit_materialize_immediate(bytes, offset, rs1, op->as.binary.lhs.immediate);
        rs2 = machine_bytes_riscv_effective_operand_register(&op->as.binary.rhs, rhs_scratch);
        offset += machine_bytes_riscv_emit_prepare_operand(function, bytes, offset, &op->as.binary.rhs, rs2);
    } else {
        rs1 = machine_bytes_riscv_effective_operand_register(&op->as.binary.lhs, lhs_scratch);
        offset += machine_bytes_riscv_emit_prepare_operand(function, bytes, offset, &op->as.binary.lhs, rs1);
        if (getenv("CODEX_MACHINE_BYTES_DEBUG")) {
            fprintf(stderr,
                "[machine-bytes-debug] cmp-imm general-lhs-prep op=%d after=%zu\n",
                (int)op->as.binary.op,
                offset - start_offset);
        }
        rs2 = rhs_scratch;
        offset += machine_bytes_riscv_emit_materialize_immediate(bytes, offset, rs2, op->as.binary.rhs.immediate);
    }

    machine_bytes_write_u32_le(bytes, offset, machine_bytes_riscv_encode_cmp_word_from_regs(op->as.binary.op, rd, rs1, rs2));
    if (op->as.binary.op == MACHINE_IR_BINARY_LT || op->as.binary.op == MACHINE_IR_BINARY_GT) {
        offset += 4u;
    } else if (op->as.binary.op == MACHINE_IR_BINARY_EQ) {
        machine_bytes_write_u32_le(bytes, offset + 4u, machine_bytes_riscv_encode_i_type(0x13u, rd, 0x3u, rd, 1));
        offset += 8u;
    } else if (op->as.binary.op == MACHINE_IR_BINARY_NE) {
        machine_bytes_write_u32_le(bytes, offset + 4u, machine_bytes_riscv_encode_r_type(0x33u, rd, 0x3u, 0u, rd, 0x00u));
        offset += 8u;
    } else {
        machine_bytes_write_u32_le(bytes, offset + 4u, machine_bytes_riscv_encode_i_type(0x13u, rd, 0x4u, rd, 1));
        offset += 8u;
    }
    offset += machine_bytes_riscv_emit_writeback_result(function, bytes, offset, &op->result, rd);
    if (getenv("CODEX_MACHINE_BYTES_DEBUG")) {
        fprintf(stderr,
            "[machine-bytes-debug] cmp-imm total op=%d total=%zu\n",
            (int)op->as.binary.op,
            offset - start_offset);
    }
    return offset - start_offset;
}

static MachineIrBinaryOp machine_bytes_riscv_invert_compare_op(MachineIrBinaryOp op) {
    switch (op) {
        case MACHINE_IR_BINARY_EQ:
            return MACHINE_IR_BINARY_NE;
        case MACHINE_IR_BINARY_NE:
            return MACHINE_IR_BINARY_EQ;
        case MACHINE_IR_BINARY_LT:
            return MACHINE_IR_BINARY_GE;
        case MACHINE_IR_BINARY_LE:
            return MACHINE_IR_BINARY_GT;
        case MACHINE_IR_BINARY_GT:
            return MACHINE_IR_BINARY_LE;
        case MACHINE_IR_BINARY_GE:
            return MACHINE_IR_BINARY_LT;
        default:
            return op;
    }
}

static uint32_t machine_bytes_riscv_encode_alu_word_from_regs(
    MachineIrBinaryOp op,
    uint32_t rd,
    uint32_t rs1,
    uint32_t rs2) {
    switch (op) {
        case MACHINE_IR_BINARY_ADD:
            return machine_bytes_riscv_encode_r_type(0x33u, rd, 0x0u, rs1, rs2, 0x00u);
        case MACHINE_IR_BINARY_SUB:
            return machine_bytes_riscv_encode_r_type(0x33u, rd, 0x0u, rs1, rs2, 0x20u);
        case MACHINE_IR_BINARY_MUL:
            return machine_bytes_riscv_encode_r_type(0x33u, rd, 0x0u, rs1, rs2, 0x01u);
        case MACHINE_IR_BINARY_DIV:
            return machine_bytes_riscv_encode_r_type(0x33u, rd, 0x4u, rs1, rs2, 0x01u);
        case MACHINE_IR_BINARY_MOD:
            return machine_bytes_riscv_encode_r_type(0x33u, rd, 0x6u, rs1, rs2, 0x01u);
        case MACHINE_IR_BINARY_BIT_AND:
            return machine_bytes_riscv_encode_r_type(0x33u, rd, 0x7u, rs1, rs2, 0x00u);
        case MACHINE_IR_BINARY_BIT_XOR:
            return machine_bytes_riscv_encode_r_type(0x33u, rd, 0x4u, rs1, rs2, 0x00u);
        case MACHINE_IR_BINARY_BIT_OR:
            return machine_bytes_riscv_encode_r_type(0x33u, rd, 0x6u, rs1, rs2, 0x00u);
        case MACHINE_IR_BINARY_SHIFT_LEFT:
            return machine_bytes_riscv_encode_r_type(0x33u, rd, 0x1u, rs1, rs2, 0x00u);
        case MACHINE_IR_BINARY_SHIFT_RIGHT:
            return machine_bytes_riscv_encode_r_type(0x33u, rd, 0x5u, rs1, rs2, 0x00u);
        default:
            return machine_bytes_riscv_encode_i_type(0x13u, rd, 0x0u, 0u, 0);
    }
}

static uint32_t machine_bytes_riscv_encode_cmp_word_from_regs(
    MachineIrBinaryOp op,
    uint32_t rd,
    uint32_t rs1,
    uint32_t rs2) {
    switch (op) {
        case MACHINE_IR_BINARY_LT:
            return machine_bytes_riscv_encode_r_type(0x33u, rd, 0x2u, rs1, rs2, 0x00u);
        case MACHINE_IR_BINARY_GT:
            return machine_bytes_riscv_encode_r_type(0x33u, rd, 0x2u, rs2, rs1, 0x00u);
        case MACHINE_IR_BINARY_EQ:
            return machine_bytes_riscv_encode_r_type(0x33u, rd, 0x4u, rs1, rs2, 0x00u);
        case MACHINE_IR_BINARY_NE:
            return machine_bytes_riscv_encode_r_type(0x33u, rd, 0x4u, rs1, rs2, 0x00u);
        case MACHINE_IR_BINARY_LE:
            return machine_bytes_riscv_encode_r_type(0x33u, rd, 0x2u, rs2, rs1, 0x00u);
        case MACHINE_IR_BINARY_GE:
            return machine_bytes_riscv_encode_r_type(0x33u, rd, 0x2u, rs1, rs2, 0x00u);
        default:
            return machine_bytes_riscv_encode_i_type(0x13u, rd, 0x0u, 0u, 0);
    }
}

static uint32_t machine_bytes_riscv_encode_branch_zero_word(
    uint32_t rs1,
    unsigned char branch_on_true,
    long long imm) {
    return machine_bytes_riscv_encode_b_type(0x63u, branch_on_true ? 0x1u : 0x0u, rs1, 0u, imm);
}

static uint32_t machine_bytes_riscv_encode_compare_branch_word_from_parts(
    MachineIrBinaryOp op,
    uint32_t rs1,
    uint32_t rs2,
    long long imm) {
    uint32_t funct3 = 0x0u;

    switch (op) {
        case MACHINE_IR_BINARY_EQ:
            funct3 = 0x0u;
            break;
        case MACHINE_IR_BINARY_NE:
            funct3 = 0x1u;
            break;
        case MACHINE_IR_BINARY_LT:
            funct3 = 0x4u;
            break;
        case MACHINE_IR_BINARY_GE:
            funct3 = 0x5u;
            break;
        case MACHINE_IR_BINARY_GT:
            funct3 = 0x4u;
            {
                uint32_t tmp = rs1;
                rs1 = rs2;
                rs2 = tmp;
            }
            break;
        case MACHINE_IR_BINARY_LE:
            funct3 = 0x5u;
            {
                uint32_t tmp = rs1;
                rs1 = rs2;
                rs2 = tmp;
            }
            break;
        default:
            funct3 = 0x0u;
            break;
    }
    return machine_bytes_riscv_encode_b_type(0x63u, funct3, rs1, rs2, imm);
}

static uint32_t machine_bytes_riscv_encode_call_word(long long imm) {
    return machine_bytes_riscv_encode_j_type(0x6Fu, 1u, imm);
}

static int machine_bytes_riscv_call_prefers_pair(const MachineEmitOp *op) {
    (void)op;
    return 1;
}

static uint32_t machine_bytes_riscv_encode_return_word(void) {
    return machine_bytes_riscv_encode_i_type(0x67u, 0u, 0x0u, 1u, 0);
}

static uint32_t machine_bytes_riscv_encode_jump_word(long long imm) {
    return machine_bytes_riscv_encode_j_type(0x6Fu, 0u, imm);
}

static long long machine_bytes_riscv_pc_relative_imm(
    size_t instruction_byte_offset,
    size_t target_byte_offset) {
    return (long long)target_byte_offset - (long long)instruction_byte_offset;
}

static int machine_bytes_riscv_resolve_call_target_byte_offset(
    const MachineBytesProgram *program,
    const size_t *function_byte_offsets,
    const MachineEmitOp *op,
    size_t *out_target_byte_offset) {
    size_t target_function_index = 0u;
    const MachineBytesFunction *target_function = NULL;

    if (!program || !function_byte_offsets || !op || !out_target_byte_offset ||
        !machine_bytes_op_has_call_payload(op->kind) ||
        !op->as.call.callee_name ||
        !machine_bytes_program_get_function_by_name(
            program, op->as.call.callee_name, &target_function_index, &target_function) ||
        !target_function ||
        !target_function->has_body) {
        return 0;
    }
    *out_target_byte_offset = function_byte_offsets[target_function_index];
    return 1;
}

static int machine_bytes_riscv_resolve_block_target_byte_offset(
    const MachineBytesFunction *function,
    size_t function_byte_offset,
    size_t target_index,
    size_t *out_target_byte_offset) {
    const MachineBytesBlock *target_block = machine_bytes_target_block(function, target_index);

    if (!target_block || !out_target_byte_offset) {
        return 0;
    }
    *out_target_byte_offset = function_byte_offset + target_block->start_byte_offset;
    return 1;
}

static void machine_bytes_debug_materialize_failure(
    const char *tag,
    const MachineBytesBlock *block,
    size_t byte_index,
    size_t op_index,
    const MachineEmitOp *op,
    const MachineEmitTerminator *terminator) {
    const MachineEmitOperand *operand = NULL;
    long long lhs_imm = 0ll;
    long long rhs_imm = 0ll;
    size_t result_spill = 0u;
    size_t lhs_spill = 0u;
    size_t rhs_spill = 0u;
    int result_kind = -1;
    int binary_op = -1;
    int lhs_kind = -1;
    int rhs_kind = -1;

    if (!getenv("CODEX_MACHINE_BYTES_DEBUG")) {
        return;
    }
    if (op) {
        result_kind = (int)op->result.kind;
        result_spill = op->result.spill_slot;
        binary_op = (int)op->as.binary.op;
        lhs_kind = (int)op->as.binary.lhs.kind;
        rhs_kind = (int)op->as.binary.rhs.kind;
        lhs_imm = op->as.binary.lhs.immediate;
        rhs_imm = op->as.binary.rhs.immediate;
        lhs_spill = op->as.binary.lhs.spill_slot;
        rhs_spill = op->as.binary.rhs.spill_slot;
    }
    if (terminator) {
        switch (terminator->kind) {
            case MACHINE_LAYOUT_TERM_BRANCH_FALLTHROUGH:
                operand = &terminator->as.branch_fallthrough.condition;
                break;
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_FALLTHROUGH:
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM_FALLTHROUGH:
                operand = &terminator->as.compare_branch_fallthrough.lhs;
                break;
            default:
                break;
        }
    }
    fprintf(stderr,
        "[machine-bytes-debug] %s block_bytes=%zu byte_index=%zu op_index=%zu op_kind=%d term_kind=%d operand_kind=%d imm=%lld spill=%zu reg=%zu result_kind=%d result_spill=%zu binary_op=%d lhs_kind=%d lhs_imm=%lld lhs_spill=%zu rhs_kind=%d rhs_imm=%lld rhs_spill=%zu\n",
        tag ? tag : "?",
        block ? block->byte_count : 0u,
        byte_index,
        op_index,
        op ? (int)op->kind : -1,
        terminator ? (int)terminator->kind : -1,
        operand ? (int)operand->kind : -1,
        operand ? operand->immediate : 0ll,
        operand ? operand->spill_slot : 0u,
        operand ? operand->machine_register_id : 0u,
        result_kind,
        result_spill,
        binary_op,
        lhs_kind,
        lhs_imm,
        lhs_spill,
        rhs_kind,
        rhs_imm,
        rhs_spill);
}

static int machine_bytes_write_block_bytes_for_profile(
    MachineBytesTargetProfile profile,
    const MachineBytesProgram *program,
    const MachineBytesFunction *function,
    size_t function_byte_offset,
    const size_t *function_byte_offsets,
    MachineBytesBlock *dest_block,
    MachineBytesError *error) {
    size_t byte_index = 0;
    size_t op_index;

    if (!dest_block || (dest_block->byte_count > 0u && !dest_block->bytes)) {
        return 0;
    }

    (void)program;
    (void)function;
    (void)function_byte_offset;
    (void)function_byte_offsets;

    if (profile == MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW) {
        for (op_index = 0; op_index < dest_block->block.op_count; ++op_index) {
            const MachineEmitOp *op = &dest_block->block.ops[op_index];

            if (getenv("CODEX_MACHINE_BYTES_DEBUG")) {
                fprintf(stderr,
                    "[machine-bytes-debug] op-size block_bytes=%zu byte_index=%zu op_index=%zu op_kind=%d op_size=%zu\n",
                    dest_block->byte_count,
                    byte_index,
                    op_index,
                    (int)op->kind,
                    machine_bytes_op_encoded_size_for_profile(profile, function, op));
            }
            if (byte_index + machine_bytes_op_encoded_size_for_profile(profile, function, op) > dest_block->byte_count) {
                return 0;
            }
            switch (op->kind) {
                case MACHINE_SELECT_OP_COPY: {
                    if (op->result.kind == MACHINE_SELECT_OPERAND_SPILL_SLOT) {
                        byte_index += machine_bytes_riscv_emit_move_operand_to_register(
                            function,
                            dest_block->bytes,
                            byte_index,
                            &op->as.copy_value,
                            machine_bytes_riscv_result_scratch_register());
                        byte_index += machine_bytes_riscv_emit_writeback_result(
                            function,
                            dest_block->bytes,
                            byte_index,
                            &op->result,
                            machine_bytes_riscv_result_scratch_register());
                    } else {
                        byte_index += machine_bytes_riscv_emit_move_operand_to_register(
                            function,
                            dest_block->bytes,
                            byte_index,
                            &op->as.copy_value,
                            machine_bytes_riscv_result_register(&op->result));
                    }
                    break;
                }
                case MACHINE_SELECT_OP_MATERIALIZE_IMM: {
                    uint32_t result_reg = machine_bytes_riscv_result_register(&op->result);

                    byte_index += machine_bytes_riscv_emit_materialize_immediate(
                        dest_block->bytes,
                        byte_index,
                        result_reg,
                        op->as.copy_value.immediate);
                    byte_index += machine_bytes_riscv_emit_writeback_result(
                        function, dest_block->bytes, byte_index, &op->result, result_reg);
                    break;
                }
                case MACHINE_SELECT_OP_ADDR_LOCAL: {
                    uint32_t result_reg = machine_bytes_riscv_result_register(&op->result);
                    long long slot_offset = machine_bytes_riscv_slot_offset(&op->as.addr_slot);

                    if (machine_bytes_riscv_is_signed_12_immediate(slot_offset)) {
                        machine_bytes_write_u32_le(
                            dest_block->bytes,
                            byte_index,
                            machine_bytes_riscv_encode_i_type(0x13u, result_reg, 0x0u, 2u, slot_offset));
                        byte_index += 4u;
                    } else {
                        byte_index += machine_bytes_riscv_emit_materialize_immediate(
                            dest_block->bytes, byte_index, result_reg, slot_offset);
                        machine_bytes_write_u32_le(
                            dest_block->bytes,
                            byte_index,
                            machine_bytes_riscv_encode_r_type(0x33u, result_reg, 0x0u, 2u, result_reg, 0x00u));
                        byte_index += 4u;
                    }
                    byte_index += machine_bytes_riscv_emit_writeback_result(
                        function, dest_block->bytes, byte_index, &op->result, result_reg);
                    break;
                }
                case MACHINE_SELECT_OP_ADDR_GLOBAL: {
                    uint32_t result_reg = machine_bytes_riscv_result_register(&op->result);

                    machine_bytes_write_u32_le(
                        dest_block->bytes,
                        byte_index,
                        machine_bytes_riscv_encode_u_type(0x37u, result_reg, 0));
                    byte_index += 4u;
                    machine_bytes_write_u32_le(
                        dest_block->bytes,
                        byte_index,
                        machine_bytes_riscv_encode_i_type(0x13u, result_reg, 0x0u, result_reg, 0));
                    byte_index += 4u;
                    byte_index += machine_bytes_riscv_emit_writeback_result(
                        function, dest_block->bytes, byte_index, &op->result, result_reg);
                    break;
                }
                case MACHINE_SELECT_OP_ALU: {
                    uint32_t lhs_reg = machine_bytes_riscv_effective_operand_register(
                        &op->as.binary.lhs,
                        machine_bytes_riscv_scratch_register());
                    uint32_t rhs_reg = machine_bytes_riscv_effective_operand_register(
                        &op->as.binary.rhs,
                        machine_bytes_riscv_secondary_scratch_register());
                    uint32_t result_reg = machine_bytes_riscv_result_register(&op->result);

                        byte_index += machine_bytes_riscv_emit_prepare_operand(
                            function, dest_block->bytes, byte_index, &op->as.binary.lhs, lhs_reg);
                        byte_index += machine_bytes_riscv_emit_prepare_operand(
                            function, dest_block->bytes, byte_index, &op->as.binary.rhs, rhs_reg);
                    machine_bytes_write_u32_le(
                        dest_block->bytes,
                        byte_index,
                        machine_bytes_riscv_encode_alu_word_from_regs(
                            op->as.binary.op,
                            result_reg,
                            lhs_reg,
                            rhs_reg));
                    byte_index += 4u;
                    byte_index += machine_bytes_riscv_emit_writeback_result(
                        function, dest_block->bytes, byte_index, &op->result, result_reg);
                    break;
                }
                case MACHINE_SELECT_OP_ALU_IMM:
                    if (machine_bytes_riscv_can_encode_alu_imm_word(op)) {
                        uint32_t result_reg = machine_bytes_riscv_result_register(&op->result);
                        uint32_t lhs_reg = machine_bytes_riscv_effective_operand_register(
                            &op->as.binary.lhs,
                            machine_bytes_riscv_scratch_register());

                        byte_index += machine_bytes_riscv_emit_prepare_operand(
                            function, dest_block->bytes, byte_index, &op->as.binary.lhs, lhs_reg);
                        switch (op->as.binary.op) {
                            case MACHINE_IR_BINARY_ADD:
                                machine_bytes_write_u32_le(
                                    dest_block->bytes,
                                    byte_index,
                                    machine_bytes_riscv_encode_i_type(
                                        0x13u, result_reg, 0x0u, lhs_reg, op->as.binary.rhs.immediate));
                                break;
                            case MACHINE_IR_BINARY_BIT_AND:
                                machine_bytes_write_u32_le(
                                    dest_block->bytes,
                                    byte_index,
                                    machine_bytes_riscv_encode_i_type(
                                        0x13u, result_reg, 0x7u, lhs_reg, op->as.binary.rhs.immediate));
                                break;
                            case MACHINE_IR_BINARY_BIT_XOR:
                                machine_bytes_write_u32_le(
                                    dest_block->bytes,
                                    byte_index,
                                    machine_bytes_riscv_encode_i_type(
                                        0x13u, result_reg, 0x4u, lhs_reg, op->as.binary.rhs.immediate));
                                break;
                            case MACHINE_IR_BINARY_BIT_OR:
                                machine_bytes_write_u32_le(
                                    dest_block->bytes,
                                    byte_index,
                                    machine_bytes_riscv_encode_i_type(
                                        0x13u, result_reg, 0x6u, lhs_reg, op->as.binary.rhs.immediate));
                                break;
                            case MACHINE_IR_BINARY_SHIFT_LEFT:
                                machine_bytes_write_u32_le(
                                    dest_block->bytes,
                                    byte_index,
                                    machine_bytes_riscv_encode_i_type(
                                        0x13u, result_reg, 0x1u, lhs_reg, op->as.binary.rhs.immediate));
                                break;
                            case MACHINE_IR_BINARY_SHIFT_RIGHT:
                                machine_bytes_write_u32_le(
                                    dest_block->bytes,
                                    byte_index,
                                    machine_bytes_riscv_encode_i_type(
                                        0x13u, result_reg, 0x5u, lhs_reg, op->as.binary.rhs.immediate));
                                break;
                            case MACHINE_IR_BINARY_LT:
                                machine_bytes_write_u32_le(
                                    dest_block->bytes,
                                    byte_index,
                                    machine_bytes_riscv_encode_i_type(
                                        0x13u, result_reg, 0x2u, lhs_reg, op->as.binary.rhs.immediate));
                                break;
                            default:
                                machine_bytes_write_u32_le(
                                    dest_block->bytes,
                                    byte_index,
                                    machine_bytes_riscv_encode_i_type(0x13u, result_reg, 0x0u, 0u, 0));
                                break;
                        }
                        byte_index += 4u;
                        byte_index += machine_bytes_riscv_emit_writeback_result(
                            function, dest_block->bytes, byte_index, &op->result, result_reg);
                    } else {
                        uint32_t rd = machine_bytes_riscv_result_register(&op->result);
                        uint32_t rs1;
                        uint32_t rs2;
                        uint32_t scratch = machine_bytes_riscv_secondary_scratch_register();

                        if (op->as.binary.lhs.kind == MACHINE_SELECT_OPERAND_IMMEDIATE) {
                            rs1 = machine_bytes_riscv_scratch_register();
                            byte_index += machine_bytes_riscv_emit_materialize_immediate(
                                dest_block->bytes, byte_index, rs1, op->as.binary.lhs.immediate);
                            rs2 = machine_bytes_riscv_effective_operand_register(
                                &op->as.binary.rhs,
                                scratch);
                            byte_index += machine_bytes_riscv_emit_prepare_operand(
                                function, dest_block->bytes, byte_index, &op->as.binary.rhs, rs2);
                        } else {
                            rs1 = machine_bytes_riscv_effective_operand_register(
                                &op->as.binary.lhs,
                                machine_bytes_riscv_scratch_register());
                            byte_index += machine_bytes_riscv_emit_prepare_operand(
                                function, dest_block->bytes, byte_index, &op->as.binary.lhs, rs1);
                            byte_index += machine_bytes_riscv_emit_materialize_immediate(
                                dest_block->bytes, byte_index, scratch, op->as.binary.rhs.immediate);
                            rs2 = scratch;
                        }
                        machine_bytes_write_u32_le(
                            dest_block->bytes,
                            byte_index,
                            machine_bytes_riscv_encode_alu_word_from_regs(op->as.binary.op, rd, rs1, rs2));
                        byte_index += 4u;
                        byte_index += machine_bytes_riscv_emit_writeback_result(
                            function, dest_block->bytes, byte_index, &op->result, rd);
                    }
                    break;
                case MACHINE_SELECT_OP_CMP:
                    byte_index += machine_bytes_riscv_emit_cmp_result(function, dest_block->bytes, byte_index, op);
                    break;
                case MACHINE_SELECT_OP_CMP_IMM:
                    byte_index += machine_bytes_riscv_emit_cmp_result(function, dest_block->bytes, byte_index, op);
                    break;
                case MACHINE_SELECT_OP_LOAD_LOCAL: {
                    uint32_t result_reg = machine_bytes_riscv_result_register(&op->result);
                    byte_index += machine_bytes_riscv_emit_load_from_base(
                        dest_block->bytes,
                        byte_index,
                        result_reg,
                        machine_bytes_riscv_slot_base_register(&op->as.load_slot),
                        machine_bytes_riscv_slot_offset(&op->as.load_slot),
                        machine_bytes_riscv_secondary_scratch_register());
                    byte_index += machine_bytes_riscv_emit_writeback_result(
                        function, dest_block->bytes, byte_index, &op->result, result_reg);
                    break;
                }
                case MACHINE_SELECT_OP_LOAD_GLOBAL: {
                    uint32_t result_reg = machine_bytes_riscv_result_register(&op->result);
                    uint32_t base_reg = machine_bytes_riscv_secondary_scratch_register();

                    machine_bytes_write_u32_le(
                        dest_block->bytes,
                        byte_index,
                        machine_bytes_riscv_encode_u_type(0x37u, base_reg, 0));
                    byte_index += 4u;
                    machine_bytes_write_u32_le(
                        dest_block->bytes,
                        byte_index,
                        machine_bytes_riscv_encode_i_type(0x03u, result_reg, 0x2u, base_reg, 0));
                    byte_index += 4u;
                    byte_index += machine_bytes_riscv_emit_writeback_result(
                        function, dest_block->bytes, byte_index, &op->result, result_reg);
                    break;
                }
                case MACHINE_SELECT_OP_STORE_LOCAL: {
                    uint32_t value_reg = machine_bytes_riscv_effective_operand_register(
                        &op->as.store.value,
                        machine_bytes_riscv_scratch_register());

                    byte_index += machine_bytes_riscv_emit_prepare_operand(
                        function, dest_block->bytes, byte_index, &op->as.store.value, value_reg);
                    byte_index += machine_bytes_riscv_emit_store_to_base(
                        dest_block->bytes,
                        byte_index,
                        machine_bytes_riscv_slot_base_register(&op->as.store.slot),
                        value_reg,
                        machine_bytes_riscv_slot_offset(&op->as.store.slot),
                        machine_bytes_riscv_secondary_scratch_register());
                    break;
                }
                case MACHINE_SELECT_OP_STORE_GLOBAL: {
                    uint32_t value_reg = machine_bytes_riscv_effective_operand_register(
                        &op->as.store.value,
                        machine_bytes_riscv_scratch_register());
                    uint32_t base_reg = machine_bytes_riscv_secondary_scratch_register();

                    byte_index += machine_bytes_riscv_emit_prepare_operand(
                        function, dest_block->bytes, byte_index, &op->as.store.value, value_reg);
                    machine_bytes_write_u32_le(
                        dest_block->bytes,
                        byte_index,
                        machine_bytes_riscv_encode_u_type(0x37u, base_reg, 0));
                    byte_index += 4u;
                    machine_bytes_write_u32_le(
                        dest_block->bytes,
                        byte_index,
                        machine_bytes_riscv_encode_s_type(0x23u, 0x2u, base_reg, value_reg, 0));
                    byte_index += 4u;
                    break;
                }
                case MACHINE_SELECT_OP_STORE_LOCAL_IMM:
                    if (!machine_bytes_riscv_immediate_uses_zero_register(op->as.store.value.immediate)) {
                        byte_index += machine_bytes_riscv_emit_materialize_immediate(
                            dest_block->bytes, byte_index, 31u, op->as.store.value.immediate);
                    }
                    byte_index += machine_bytes_riscv_emit_store_to_base(
                        dest_block->bytes,
                        byte_index,
                        machine_bytes_riscv_slot_base_register(&op->as.store.slot),
                        machine_bytes_riscv_immediate_uses_zero_register(op->as.store.value.immediate) ? 0u : 31u,
                        machine_bytes_riscv_slot_offset(&op->as.store.slot),
                        machine_bytes_riscv_secondary_scratch_register());
                    break;
                case MACHINE_SELECT_OP_STORE_GLOBAL_IMM: {
                    uint32_t base_reg = machine_bytes_riscv_secondary_scratch_register();

                    if (!machine_bytes_riscv_immediate_uses_zero_register(op->as.store.value.immediate)) {
                        byte_index += machine_bytes_riscv_emit_materialize_immediate(
                            dest_block->bytes, byte_index, 31u, op->as.store.value.immediate);
                    }
                    machine_bytes_write_u32_le(
                        dest_block->bytes,
                        byte_index,
                        machine_bytes_riscv_encode_u_type(0x37u, base_reg, 0));
                    byte_index += 4u;
                    machine_bytes_write_u32_le(
                        dest_block->bytes,
                        byte_index,
                        machine_bytes_riscv_encode_s_type(
                            0x23u,
                            0x2u,
                            base_reg,
                            machine_bytes_riscv_immediate_uses_zero_register(op->as.store.value.immediate) ? 0u : 31u,
                            0));
                    byte_index += 4u;
                    break;
                }
                case MACHINE_SELECT_OP_LOAD_INDIRECT: {
                    uint32_t result_reg = machine_bytes_riscv_result_register(&op->result);
                    uint32_t base_reg = machine_bytes_riscv_effective_operand_register(
                        &op->as.load_indirect_addr,
                        machine_bytes_riscv_scratch_register());

                    byte_index += machine_bytes_riscv_emit_prepare_operand(
                        function, dest_block->bytes, byte_index, &op->as.load_indirect_addr, base_reg);
                    machine_bytes_write_u32_le(
                        dest_block->bytes,
                        byte_index,
                        machine_bytes_riscv_encode_i_type(0x03u, result_reg, 0x2u, base_reg, 0));
                    byte_index += 4u;
                    byte_index += machine_bytes_riscv_emit_writeback_result(
                        function, dest_block->bytes, byte_index, &op->result, result_reg);
                    break;
                }
                case MACHINE_SELECT_OP_STORE_INDIRECT: {
                    uint32_t base_reg = machine_bytes_riscv_effective_operand_register(
                        &op->as.store_indirect.addr,
                        machine_bytes_riscv_scratch_register());
                    uint32_t value_reg = machine_bytes_riscv_effective_operand_register(
                        &op->as.store_indirect.value,
                        machine_bytes_riscv_secondary_scratch_register());

                    byte_index += machine_bytes_riscv_emit_prepare_operand(
                        function, dest_block->bytes, byte_index, &op->as.store_indirect.addr, base_reg);
                    byte_index += machine_bytes_riscv_emit_prepare_operand(
                        function, dest_block->bytes, byte_index, &op->as.store_indirect.value, value_reg);
                    machine_bytes_write_u32_le(
                        dest_block->bytes,
                        byte_index,
                        machine_bytes_riscv_encode_s_type(0x23u, 0x2u, base_reg, value_reg, 0));
                    byte_index += 4u;
                    break;
                }
                case MACHINE_SELECT_OP_CALL:
                case MACHINE_SELECT_OP_CALL_IMM:
                case MACHINE_SELECT_OP_CALL_SPILL:
                case MACHINE_SELECT_OP_CALL_IMM_SPILL:
                case MACHINE_SELECT_OP_CALL_VOID:
                case MACHINE_SELECT_OP_CALL_VOID_IMM: {
                    size_t arg_index;
                    size_t target_byte_offset = 0u;
                    size_t stack_arg_bytes = machine_bytes_riscv_stack_arg_byte_count(op);
                    long long call_imm = 0;

                    if (stack_arg_bytes > 0u) {
                        byte_index += machine_bytes_riscv_emit_adjust_sp(
                            dest_block->bytes,
                            byte_index,
                            -(long long)stack_arg_bytes);
                    }
                    for (arg_index = 8u; arg_index < op->as.call.arg_count; ++arg_index) {
                        const MachineEmitOperand *arg = &op->as.call.args[arg_index];
                        long long stack_byte_offset = (long long)((arg_index - 8u) * 4u);

                        if (arg->kind == MACHINE_SELECT_OPERAND_IMMEDIATE) {
                            byte_index += machine_bytes_riscv_emit_materialize_immediate(
                                dest_block->bytes,
                                byte_index,
                                machine_bytes_riscv_secondary_scratch_register(),
                                arg->immediate);
                            byte_index += machine_bytes_riscv_emit_store_to_stack(
                                dest_block->bytes,
                                byte_index,
                                machine_bytes_riscv_secondary_scratch_register(),
                                stack_byte_offset,
                                machine_bytes_riscv_scratch_register());
                        } else if (arg->kind == MACHINE_SELECT_OPERAND_SPILL_SLOT) {
                            byte_index += machine_bytes_riscv_emit_load_from_stack(
                                dest_block->bytes,
                                byte_index,
                                machine_bytes_riscv_secondary_scratch_register(),
                                machine_bytes_riscv_spill_slot_offset(function, arg->spill_slot) + (long long)stack_arg_bytes,
                                machine_bytes_riscv_scratch_register());
                            byte_index += machine_bytes_riscv_emit_store_to_stack(
                                dest_block->bytes,
                                byte_index,
                                machine_bytes_riscv_secondary_scratch_register(),
                                stack_byte_offset,
                                machine_bytes_riscv_scratch_register());
                        } else {
                            byte_index += machine_bytes_riscv_emit_store_to_stack(
                                dest_block->bytes,
                                byte_index,
                                machine_bytes_riscv_map_operand_register(arg),
                                stack_byte_offset,
                                machine_bytes_riscv_scratch_register());
                        }
                    }
                    byte_index = machine_bytes_riscv_emit_register_call_args(
                        function,
                        dest_block->bytes,
                        byte_index,
                        op,
                        stack_arg_bytes);
                    if (machine_bytes_riscv_resolve_call_target_byte_offset(
                            program, function_byte_offsets, op, &target_byte_offset)) {
                        call_imm = machine_bytes_riscv_pc_relative_imm(
                            function_byte_offset + dest_block->start_byte_offset + byte_index,
                            target_byte_offset);
                    }
                    if (machine_bytes_riscv_call_prefers_pair(op)) {
                        long long upper = (call_imm + 0x800ll) >> 12;
                        long long lower = call_imm - (upper << 12);

                        machine_bytes_write_u32_le(
                            dest_block->bytes,
                            byte_index,
                            machine_bytes_riscv_encode_u_type(0x17u, 1u, upper << 12));
                        machine_bytes_write_u32_le(
                            dest_block->bytes,
                            byte_index + 4u,
                            machine_bytes_riscv_encode_i_type(0x67u, 1u, 0x0u, 1u, lower));
                        byte_index += 8u;
                    } else {
                        machine_bytes_write_u32_le(
                            dest_block->bytes, byte_index, machine_bytes_riscv_encode_call_word(call_imm));
                        byte_index += 4u;
                    }
                    if (stack_arg_bytes > 0u) {
                        byte_index += machine_bytes_riscv_emit_adjust_sp(
                            dest_block->bytes,
                            byte_index,
                            (long long)stack_arg_bytes);
                    }
                    if (op->has_result) {
                        if (op->result.kind == MACHINE_SELECT_OPERAND_REGISTER) {
                            uint32_t result_reg = machine_bytes_riscv_map_operand_register(&op->result);

                            if (result_reg != 10u) {
                                byte_index += machine_bytes_riscv_emit_copy_register(
                                    dest_block->bytes, byte_index, result_reg, 10u);
                            }
                        } else if (op->result.kind == MACHINE_SELECT_OPERAND_SPILL_SLOT) {
                            byte_index += machine_bytes_riscv_emit_store_to_stack(
                                dest_block->bytes,
                                byte_index,
                                10u,
                                machine_bytes_riscv_spill_slot_offset(function, op->result.spill_slot),
                                machine_bytes_riscv_scratch_register());
                        }
                    }
                    break;
                }
                default:
                    machine_bytes_write_u32_le(
                        dest_block->bytes, byte_index, machine_bytes_riscv_encode_i_type(0x13u, 0u, 0x0u, 0u, 0));
                    byte_index += 4u;
                    break;
            }
        }

        if (getenv("CODEX_MACHINE_BYTES_DEBUG")) {
            fprintf(stderr,
                "[machine-bytes-debug] term-size block_bytes=%zu byte_index=%zu term_kind=%d term_size=%zu\n",
                dest_block->byte_count,
                byte_index,
                (int)dest_block->block.terminator.kind,
                machine_bytes_terminator_encoded_size_for_profile(profile, function, &dest_block->block.terminator));
        }
        switch (dest_block->block.terminator.kind) {
            case MACHINE_LAYOUT_TERM_RETURN:
                if (dest_block->block.terminator.as.return_value.kind == MACHINE_SELECT_OPERAND_REGISTER) {
                    uint32_t return_reg =
                        machine_bytes_riscv_map_operand_register(&dest_block->block.terminator.as.return_value);

                    if (return_reg != 10u) {
                        byte_index += machine_bytes_riscv_emit_copy_register(
                            dest_block->bytes, byte_index, 10u, return_reg);
                    }
                }
                machine_bytes_write_u32_le(dest_block->bytes, byte_index, machine_bytes_riscv_encode_return_word());
                byte_index += 4u;
                break;
            case MACHINE_LAYOUT_TERM_RETURN_IMM:
                byte_index += machine_bytes_riscv_emit_materialize_immediate(
                    dest_block->bytes, byte_index, 10u, dest_block->block.terminator.as.return_value.immediate);
                machine_bytes_write_u32_le(dest_block->bytes, byte_index, machine_bytes_riscv_encode_return_word());
                byte_index += 4u;
                break;
            case MACHINE_LAYOUT_TERM_RETURN_SPILL:
                byte_index += machine_bytes_riscv_emit_load_from_stack(
                    dest_block->bytes,
                    byte_index,
                    10u,
                    machine_bytes_riscv_spill_slot_offset(function, dest_block->block.terminator.as.return_value.spill_slot),
                    machine_bytes_riscv_scratch_register());
                machine_bytes_write_u32_le(dest_block->bytes, byte_index, machine_bytes_riscv_encode_return_word());
                byte_index += 4u;
                break;
            case MACHINE_LAYOUT_TERM_FALLTHROUGH:
                break;
            case MACHINE_LAYOUT_TERM_JUMP: {
                size_t target_index = dest_block->block.terminator.as.jump_target;
                size_t target_byte_offset = 0u;

                if (!machine_bytes_riscv_resolve_block_target_byte_offset(
                        function, function_byte_offset, target_index, &target_byte_offset)) {
                    machine_bytes_debug_materialize_failure(
                        "jump-resolve", dest_block, byte_index, op_index, NULL, &dest_block->block.terminator);
                    return 0;
                }
                {
                    long long jump_imm = machine_bytes_riscv_pc_relative_imm(
                        function_byte_offset + dest_block->terminator_byte_offset,
                        target_byte_offset);

                    if (!machine_bytes_riscv_is_jump_immediate(jump_imm)) {
                        machine_bytes_set_error(
                            error, 0, 0, "MACHINE-BYTES-343: riscv preview jump target out of range");
                        machine_bytes_debug_materialize_failure(
                            "jump-range", dest_block, byte_index, op_index, NULL, &dest_block->block.terminator);
                        return 0;
                    }
                    machine_bytes_write_u32_le(
                        dest_block->bytes,
                        byte_index,
                        machine_bytes_riscv_encode_jump_word(jump_imm));
                }
                byte_index += 4u;
                break;
            }
            case MACHINE_LAYOUT_TERM_BRANCH:
            case MACHINE_LAYOUT_TERM_BRANCH_FALLTHROUGH: {
                int has_secondary_jump = dest_block->block.terminator.kind == MACHINE_LAYOUT_TERM_BRANCH;
                unsigned char branch_on_true = dest_block->block.terminator.kind == MACHINE_LAYOUT_TERM_BRANCH
                    ? 1u
                    : dest_block->block.terminator.as.branch_fallthrough.branch_on_true;
                size_t primary_target_index = dest_block->block.terminator.kind == MACHINE_LAYOUT_TERM_BRANCH
                    ? dest_block->block.terminator.as.branch.then_target
                    : dest_block->block.terminator.as.branch_fallthrough.taken_target;
                size_t secondary_target_index = dest_block->block.terminator.kind == MACHINE_LAYOUT_TERM_BRANCH
                    ? dest_block->block.terminator.as.branch.else_target
                    : dest_block->block.terminator.as.branch_fallthrough.fallthrough_target;
                size_t primary_target_byte_offset = 0u;
                size_t secondary_target_byte_offset = 0u;
                const MachineEmitOperand *condition = dest_block->block.terminator.kind == MACHINE_LAYOUT_TERM_BRANCH
                    ? &dest_block->block.terminator.as.branch.condition
                    : &dest_block->block.terminator.as.branch_fallthrough.condition;
                uint32_t condition_reg = machine_bytes_riscv_effective_operand_register(
                    condition,
                    machine_bytes_riscv_scratch_register());

                if (!machine_bytes_riscv_resolve_block_target_byte_offset(
                        function, function_byte_offset, primary_target_index, &primary_target_byte_offset) ||
                    !machine_bytes_riscv_resolve_block_target_byte_offset(
                        function, function_byte_offset, secondary_target_index, &secondary_target_byte_offset)) {
                    machine_bytes_debug_materialize_failure(
                        "branch-resolve", dest_block, byte_index, op_index, NULL, &dest_block->block.terminator);
                    return 0;
                }
                byte_index += machine_bytes_riscv_emit_prepare_operand(
                    function,
                    dest_block->bytes,
                    byte_index,
                    condition,
                    condition_reg);
                {
                    long long branch_imm = machine_bytes_riscv_pc_relative_imm(
                        function_byte_offset + dest_block->start_byte_offset + byte_index,
                        primary_target_byte_offset);

                    if (!machine_bytes_riscv_is_branch_immediate(branch_imm)) {
                        machine_bytes_set_error(
                            error, 0, 0, "MACHINE-BYTES-344: riscv preview branch target out of range");
                        machine_bytes_debug_materialize_failure(
                            "branch-range", dest_block, byte_index, op_index, NULL, &dest_block->block.terminator);
                        return 0;
                    }
                    machine_bytes_write_u32_le(
                        dest_block->bytes,
                        byte_index,
                        machine_bytes_riscv_encode_branch_zero_word(
                            condition_reg,
                            branch_on_true,
                            branch_imm));
                }
                byte_index += 4u;
                if (has_secondary_jump) {
                    long long jump_imm = machine_bytes_riscv_pc_relative_imm(
                        function_byte_offset + dest_block->start_byte_offset + byte_index,
                        secondary_target_byte_offset);

                    if (!machine_bytes_riscv_is_jump_immediate(jump_imm)) {
                        machine_bytes_set_error(
                            error, 0, 0, "MACHINE-BYTES-343: riscv preview jump target out of range");
                        machine_bytes_debug_materialize_failure(
                            "branch-secondary-range", dest_block, byte_index, op_index, NULL, &dest_block->block.terminator);
                        return 0;
                    }
                    machine_bytes_write_u32_le(
                        dest_block->bytes,
                        byte_index,
                        machine_bytes_riscv_encode_jump_word(jump_imm));
                    byte_index += 4u;
                }
                break;
            }
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH:
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM:
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_FALLTHROUGH:
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM_FALLTHROUGH: {
                int has_secondary_jump =
                    dest_block->block.terminator.kind == MACHINE_LAYOUT_TERM_COMPARE_BRANCH ||
                    dest_block->block.terminator.kind == MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM;
                const MachineEmitOperand *lhs = NULL;
                const MachineEmitOperand *rhs = NULL;
                MachineIrBinaryOp compare_op;
                uint32_t branch_rs1;
                uint32_t branch_rs2;
                size_t primary_target_index =
                    (dest_block->block.terminator.kind == MACHINE_LAYOUT_TERM_COMPARE_BRANCH ||
                        dest_block->block.terminator.kind == MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM)
                    ? dest_block->block.terminator.as.compare_branch.then_target
                    : dest_block->block.terminator.as.compare_branch_fallthrough.taken_target;
                size_t secondary_target_index =
                    (dest_block->block.terminator.kind == MACHINE_LAYOUT_TERM_COMPARE_BRANCH ||
                        dest_block->block.terminator.kind == MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM)
                    ? dest_block->block.terminator.as.compare_branch.else_target
                    : dest_block->block.terminator.as.compare_branch_fallthrough.fallthrough_target;
                size_t primary_target_byte_offset = 0u;
                size_t secondary_target_byte_offset = 0u;

                if (!machine_bytes_riscv_resolve_block_target_byte_offset(
                        function, function_byte_offset, primary_target_index, &primary_target_byte_offset) ||
                    !machine_bytes_riscv_resolve_block_target_byte_offset(
                        function, function_byte_offset, secondary_target_index, &secondary_target_byte_offset)) {
                    machine_bytes_debug_materialize_failure(
                        "cmpbranch-resolve", dest_block, byte_index, op_index, NULL, &dest_block->block.terminator);
                    return 0;
                }
                if (dest_block->block.terminator.kind == MACHINE_LAYOUT_TERM_COMPARE_BRANCH ||
                    dest_block->block.terminator.kind == MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM) {
                    lhs = &dest_block->block.terminator.as.compare_branch.lhs;
                    rhs = &dest_block->block.terminator.as.compare_branch.rhs;
                    compare_op = dest_block->block.terminator.as.compare_branch.op;
                } else {
                    lhs = &dest_block->block.terminator.as.compare_branch_fallthrough.lhs;
                    rhs = &dest_block->block.terminator.as.compare_branch_fallthrough.rhs;
                    compare_op = dest_block->block.terminator.as.compare_branch_fallthrough.branch_on_true
                        ? dest_block->block.terminator.as.compare_branch_fallthrough.op
                        : machine_bytes_riscv_invert_compare_op(
                            dest_block->block.terminator.as.compare_branch_fallthrough.op);
                }
                branch_rs1 = machine_bytes_riscv_effective_operand_register(
                    lhs,
                    machine_bytes_riscv_scratch_register());
                byte_index += machine_bytes_riscv_emit_prepare_operand(
                    function,
                    dest_block->bytes,
                    byte_index,
                    lhs,
                    branch_rs1);
                if ((dest_block->block.terminator.kind == MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM ||
                        dest_block->block.terminator.kind == MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM_FALLTHROUGH) &&
                    rhs->kind == MACHINE_SELECT_OPERAND_IMMEDIATE &&
                    rhs->immediate == 0) {
                    branch_rs2 = 0u;
                } else {
                    branch_rs2 = machine_bytes_riscv_effective_operand_register(
                        rhs,
                        machine_bytes_riscv_secondary_scratch_register());
                    byte_index += machine_bytes_riscv_emit_prepare_operand(
                        function,
                        dest_block->bytes,
                        byte_index,
                        rhs,
                        branch_rs2);
                }
                {
                    long long branch_imm = machine_bytes_riscv_pc_relative_imm(
                        function_byte_offset + dest_block->start_byte_offset + byte_index,
                        primary_target_byte_offset);

                    if (!machine_bytes_riscv_is_branch_immediate(branch_imm)) {
                        machine_bytes_set_error(
                            error, 0, 0, "MACHINE-BYTES-345: riscv preview compare-branch target out of range");
                        machine_bytes_debug_materialize_failure(
                            "cmpbranch-range", dest_block, byte_index, op_index, NULL, &dest_block->block.terminator);
                        return 0;
                    }
                    machine_bytes_write_u32_le(
                        dest_block->bytes,
                        byte_index,
                        machine_bytes_riscv_encode_compare_branch_word_from_parts(
                            compare_op,
                            branch_rs1,
                            branch_rs2,
                            branch_imm));
                }
                byte_index += 4u;
                if (has_secondary_jump) {
                    long long jump_imm = machine_bytes_riscv_pc_relative_imm(
                        function_byte_offset + dest_block->start_byte_offset + byte_index,
                        secondary_target_byte_offset);

                    if (!machine_bytes_riscv_is_jump_immediate(jump_imm)) {
                        machine_bytes_set_error(
                            error, 0, 0, "MACHINE-BYTES-343: riscv preview jump target out of range");
                        machine_bytes_debug_materialize_failure(
                            "cmpbranch-secondary-range", dest_block, byte_index, op_index, NULL, &dest_block->block.terminator);
                        return 0;
                    }
                    machine_bytes_write_u32_le(
                        dest_block->bytes,
                        byte_index,
                        machine_bytes_riscv_encode_jump_word(jump_imm));
                    byte_index += 4u;
                }
                break;
            }
            default:
                machine_bytes_debug_materialize_failure(
                    "terminator-unsupported", dest_block, byte_index, op_index, NULL, &dest_block->block.terminator);
                return 0;
        }
        if (byte_index != dest_block->byte_count) {
            const MachineEmitOp *last_op = op_index > 0u ? &dest_block->block.ops[op_index - 1u] : NULL;
            machine_bytes_debug_materialize_failure(
                "term-size-mismatch", dest_block, byte_index, op_index, last_op, &dest_block->block.terminator);
        }
        return byte_index == dest_block->byte_count;
    }

    for (op_index = 0; op_index < dest_block->block.op_count; ++op_index) {
        const MachineEmitOp *op = &dest_block->block.ops[op_index];
        size_t op_size = machine_bytes_op_encoded_size_for_profile(profile, function, op);

        if (getenv("CODEX_MACHINE_BYTES_DEBUG")) {
            fprintf(stderr,
                "[machine-bytes-debug] op-size block_bytes=%zu byte_index=%zu op_index=%zu op_kind=%d op_size=%zu\n",
                dest_block->byte_count,
                byte_index,
                op_index,
                (int)op->kind,
                op_size);
        }
        if (byte_index + op_size > dest_block->byte_count) {
            machine_bytes_debug_materialize_failure("op-size-overflow", dest_block, byte_index, op_index, op, NULL);
            return 0;
        }
        dest_block->bytes[byte_index++] = machine_bytes_encode_op_kind(op->kind);
        switch (op->kind) {
            case MACHINE_SELECT_OP_MATERIALIZE_IMM:
                dest_block->bytes[byte_index++] =
                    (unsigned char)op->as.copy_value.immediate;
                break;
            case MACHINE_SELECT_OP_ALU_IMM:
            case MACHINE_SELECT_OP_CMP_IMM:
                dest_block->bytes[byte_index++] =
                    (unsigned char)op->as.binary.rhs.immediate;
                break;
            case MACHINE_SELECT_OP_STORE_LOCAL_IMM:
            case MACHINE_SELECT_OP_STORE_GLOBAL_IMM:
                dest_block->bytes[byte_index++] =
                    machine_bytes_encode_slot_imm_payload(&op->as.store.slot, &op->as.store.value);
                break;
            case MACHINE_SELECT_OP_CALL:
            case MACHINE_SELECT_OP_CALL_IMM:
            case MACHINE_SELECT_OP_CALL_SPILL:
            case MACHINE_SELECT_OP_CALL_IMM_SPILL:
            case MACHINE_SELECT_OP_CALL_VOID:
            case MACHINE_SELECT_OP_CALL_VOID_IMM: {
                size_t arg_index;

                dest_block->bytes[byte_index++] = (unsigned char)op->as.call.arg_count;
                for (arg_index = 0; arg_index < op->as.call.arg_count; ++arg_index) {
                    dest_block->bytes[byte_index++] =
                        machine_bytes_encode_operand_descriptor(&op->as.call.args[arg_index]);
                }
                break;
            }
            default:
                break;
        }
    }

    {
        const MachineEmitTerminator *terminator = &dest_block->block.terminator;
        size_t term_size = machine_bytes_terminator_encoded_size_for_profile(profile, function, terminator);

        if (getenv("CODEX_MACHINE_BYTES_DEBUG")) {
            fprintf(stderr,
                "[machine-bytes-debug] term-size block_bytes=%zu byte_index=%zu term_kind=%d term_size=%zu\n",
                dest_block->byte_count,
                byte_index,
                (int)terminator->kind,
                term_size);
        }
        if (byte_index + term_size > dest_block->byte_count) {
            machine_bytes_debug_materialize_failure("term-size-overflow", dest_block, byte_index, op_index, NULL, terminator);
            return 0;
        }
        dest_block->bytes[byte_index++] = machine_bytes_encode_terminator_kind(terminator->kind);
        switch (terminator->kind) {
            case MACHINE_LAYOUT_TERM_RETURN:
                break;
            case MACHINE_LAYOUT_TERM_RETURN_IMM:
            case MACHINE_LAYOUT_TERM_RETURN_SPILL:
                dest_block->bytes[byte_index++] =
                    machine_bytes_encode_operand_descriptor(&terminator->as.return_value);
                break;
            case MACHINE_LAYOUT_TERM_JUMP:
                dest_block->bytes[byte_index++] = (unsigned char)terminator->as.jump_target;
                break;
            case MACHINE_LAYOUT_TERM_FALLTHROUGH:
                dest_block->bytes[byte_index++] = (unsigned char)terminator->as.fallthrough_target;
                break;
            case MACHINE_LAYOUT_TERM_BRANCH:
                dest_block->bytes[byte_index++] =
                    machine_bytes_encode_operand_descriptor(&terminator->as.branch.condition);
                dest_block->bytes[byte_index++] = machine_bytes_encode_target_pair(
                    terminator->as.branch.then_target,
                    terminator->as.branch.else_target);
                break;
            case MACHINE_LAYOUT_TERM_BRANCH_FALLTHROUGH:
                dest_block->bytes[byte_index++] = (unsigned char)(
                    machine_bytes_encode_operand_descriptor(&terminator->as.branch_fallthrough.condition) |
                    (terminator->as.branch_fallthrough.branch_on_true ? 0x80u : 0x00u));
                dest_block->bytes[byte_index++] = machine_bytes_encode_target_pair(
                    terminator->as.branch_fallthrough.taken_target,
                    terminator->as.branch_fallthrough.fallthrough_target);
                break;
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH:
                dest_block->bytes[byte_index++] = machine_bytes_encode_compare_flags(
                    terminator->as.compare_branch.op, 0u, 0u, 0u);
                dest_block->bytes[byte_index++] =
                    machine_bytes_encode_operand_descriptor(&terminator->as.compare_branch.rhs);
                dest_block->bytes[byte_index++] = machine_bytes_encode_target_pair(
                    terminator->as.compare_branch.then_target,
                    terminator->as.compare_branch.else_target);
                break;
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM:
                dest_block->bytes[byte_index++] = machine_bytes_encode_compare_flags(
                    terminator->as.compare_branch.op, 1u, 0u, 0u);
                dest_block->bytes[byte_index++] =
                    (unsigned char)terminator->as.compare_branch.rhs.immediate;
                dest_block->bytes[byte_index++] = machine_bytes_encode_target_pair(
                    terminator->as.compare_branch.then_target,
                    terminator->as.compare_branch.else_target);
                break;
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_FALLTHROUGH:
                dest_block->bytes[byte_index++] = machine_bytes_encode_compare_flags(
                    terminator->as.compare_branch_fallthrough.op,
                    0u,
                    1u,
                    terminator->as.compare_branch_fallthrough.branch_on_true);
                dest_block->bytes[byte_index++] =
                    machine_bytes_encode_operand_descriptor(&terminator->as.compare_branch_fallthrough.rhs);
                dest_block->bytes[byte_index++] = machine_bytes_encode_target_pair(
                    terminator->as.compare_branch_fallthrough.taken_target,
                    terminator->as.compare_branch_fallthrough.fallthrough_target);
                break;
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM_FALLTHROUGH:
                dest_block->bytes[byte_index++] = machine_bytes_encode_compare_flags(
                    terminator->as.compare_branch_fallthrough.op,
                    1u,
                    1u,
                    terminator->as.compare_branch_fallthrough.branch_on_true);
                dest_block->bytes[byte_index++] =
                    (unsigned char)terminator->as.compare_branch_fallthrough.rhs.immediate;
                dest_block->bytes[byte_index++] = machine_bytes_encode_target_pair(
                    terminator->as.compare_branch_fallthrough.taken_target,
                    terminator->as.compare_branch_fallthrough.fallthrough_target);
                break;
            default:
                machine_bytes_debug_materialize_failure("term-unsupported", dest_block, byte_index, op_index, NULL, terminator);
                return 0;
        }
    }
    if (byte_index != dest_block->byte_count) {
        const MachineEmitOp *last_op = op_index > 0u ? &dest_block->block.ops[op_index - 1u] : NULL;
        machine_bytes_debug_materialize_failure(
            "op-size-mismatch", dest_block, byte_index, op_index, last_op, &dest_block->block.terminator);
    }
    return byte_index == dest_block->byte_count;
}

static int machine_bytes_reference_patch_for_call(
    MachineBytesTargetProfile profile,
    size_t owner_byte_offset,
    size_t owner_byte_count,
    size_t *out_patch_byte_offset,
    size_t *out_patch_byte_count) {
    if (!out_patch_byte_offset || !out_patch_byte_count) {
        return 0;
    }
    if (profile == MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW) {
        (void)owner_byte_count;
        *out_patch_byte_offset = owner_byte_offset;
        *out_patch_byte_count = 4u;
        return 1;
    }
    *out_patch_byte_offset = owner_byte_offset;
    *out_patch_byte_count = 0;
    return 1;
}

static int machine_bytes_reference_patch_for_control(
    MachineBytesTargetProfile profile,
    MachineBytesReferenceKind kind,
    MachineBytesTerminatorKind terminator_kind,
    size_t owner_byte_offset,
    size_t owner_byte_count,
    size_t *out_patch_byte_offset,
    size_t *out_patch_byte_count) {
    if (!out_patch_byte_offset || !out_patch_byte_count) {
        return 0;
    }
    if (profile == MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW) {
        switch (terminator_kind) {
            case MACHINE_LAYOUT_TERM_JUMP:
                *out_patch_byte_offset = owner_byte_offset;
                *out_patch_byte_count = owner_byte_count >= 4u ? 4u : owner_byte_count;
                return 1;
            case MACHINE_LAYOUT_TERM_FALLTHROUGH:
                *out_patch_byte_offset = owner_byte_offset;
                *out_patch_byte_count = 0u;
                return 1;
            case MACHINE_LAYOUT_TERM_BRANCH:
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH:
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM:
                *out_patch_byte_offset = owner_byte_offset +
                    (owner_byte_count >= 8u ? owner_byte_count - 8u : 0u) +
                    (kind == MACHINE_BYTES_REFERENCE_CONTROL_SECONDARY ? 4u : 0u);
                *out_patch_byte_count = 4u;
                return 1;
            case MACHINE_LAYOUT_TERM_BRANCH_FALLTHROUGH:
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_FALLTHROUGH:
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM_FALLTHROUGH:
                if (kind == MACHINE_BYTES_REFERENCE_CONTROL_PRIMARY) {
                    *out_patch_byte_offset = owner_byte_offset +
                        (owner_byte_count >= 4u ? owner_byte_count - 4u : 0u);
                    *out_patch_byte_count = 4u;
                } else {
                    *out_patch_byte_offset = owner_byte_offset + owner_byte_count;
                    *out_patch_byte_count = 0u;
                }
                return 1;
            default:
                break;
        }
    }
    *out_patch_byte_offset = owner_byte_offset + (owner_byte_count > 1 ? owner_byte_count - 1 : 0);
    *out_patch_byte_count = owner_byte_count > 1 ? 1u : 0u;
    return 1;
}

static int machine_bytes_reference_patch_for_global_access(
    MachineBytesTargetProfile profile,
    const MachineEmitOp *op,
    const MachineBytesFunction *function,
    size_t owner_byte_offset,
    size_t owner_byte_count,
    size_t *out_patch_byte_offset,
    size_t *out_patch_byte_count) {
    (void)owner_byte_count;

    if (!op || !out_patch_byte_offset || !out_patch_byte_count) {
        return 0;
    }
    *out_patch_byte_offset = owner_byte_offset;
    *out_patch_byte_count = 0u;
    if (profile != MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW) {
        return 1;
    }
    if (op->kind == MACHINE_SELECT_OP_LOAD_GLOBAL) {
        if (!machine_bytes_riscv_is_signed_12_immediate(machine_bytes_riscv_slot_offset(&op->as.load_slot))) {
            return 1;
        }
        *out_patch_byte_offset = owner_byte_offset;
        *out_patch_byte_count = 4u;
        return 1;
    }
    if (op->kind == MACHINE_SELECT_OP_STORE_GLOBAL) {
        if (!machine_bytes_riscv_is_signed_12_immediate(machine_bytes_riscv_slot_offset(&op->as.store.slot))) {
            return 1;
        }
        *out_patch_byte_offset = owner_byte_offset +
            machine_bytes_riscv_prepare_operand_size(function, &op->as.store.value);
        *out_patch_byte_count = 4u;
        return 1;
    }
    if (op->kind == MACHINE_SELECT_OP_STORE_GLOBAL_IMM) {
        if (!machine_bytes_riscv_is_signed_12_immediate(machine_bytes_riscv_slot_offset(&op->as.store.slot))) {
            return 1;
        }
        *out_patch_byte_offset = owner_byte_offset +
            machine_bytes_riscv_materialize_size(op->as.store.value.immediate);
        *out_patch_byte_count = 4u;
        return 1;
    }
    if (op->kind == MACHINE_SELECT_OP_ADDR_GLOBAL) {
        *out_patch_byte_offset = owner_byte_offset;
        *out_patch_byte_count = 4u;
        return 1;
    }
    return 1;
}

static int machine_bytes_verify_target_index(size_t target_index, size_t block_count) {
    return target_index < block_count;
}

static int machine_bytes_op_clone(MachineEmitOp *dest, const MachineEmitOp *src) {
    if (!dest || !src) {
        return 0;
    }

    *dest = *src;
    if (!machine_bytes_op_has_call_payload(src->kind)) {
        return 1;
    }

    dest->as.call.callee_name = machine_bytes_strdup(src->as.call.callee_name);
    if (src->as.call.callee_name && !dest->as.call.callee_name) {
        return 0;
    }
    if (src->as.call.arg_count > 0) {
        dest->as.call.args = (MachineEmitOperand *)calloc(src->as.call.arg_count, sizeof(MachineEmitOperand));
        if (!dest->as.call.args) {
            free(dest->as.call.callee_name);
            dest->as.call.callee_name = NULL;
            return 0;
        }
        memcpy(dest->as.call.args, src->as.call.args, src->as.call.arg_count * sizeof(MachineEmitOperand));
    } else {
        dest->as.call.args = NULL;
    }
    return 1;
}

static void machine_bytes_op_free(MachineEmitOp *op) {
    if (!op || !machine_bytes_op_has_call_payload(op->kind)) {
        return;
    }

    free(op->as.call.callee_name);
    free(op->as.call.args);
    op->as.call.callee_name = NULL;
    op->as.call.args = NULL;
    op->as.call.arg_count = 0;
}

static void machine_bytes_block_free(MachineBytesBlock *block) {
    size_t op_index;

    if (!block) {
        return;
    }
    free(block->label_name);
    block->label_name = NULL;
    free(block->bytes);
    block->bytes = NULL;
    block->byte_count = 0;
    free(block->block.label_name);
    block->block.label_name = NULL;
    for (op_index = 0; op_index < block->block.op_count; ++op_index) {
        machine_bytes_op_free(&block->block.ops[op_index]);
    }
    free(block->block.ops);
    block->block.ops = NULL;
    block->block.op_count = 0;
    block->block.op_capacity = 0;
    block->block.has_terminator = 0;
}

static void machine_bytes_function_free(MachineBytesFunction *function) {
    size_t block_index;

    if (!function) {
        return;
    }
    free(function->name);
    function->name = NULL;
    for (block_index = 0; block_index < function->block_count; ++block_index) {
        machine_bytes_block_free(&function->blocks[block_index]);
    }
    free(function->blocks);
    function->blocks = NULL;
    function->block_count = 0;
    function->block_capacity = 0;
    function->parameter_count = 0;
    function->local_count = 0;
    function->spill_slot_count = 0;
    function->has_body = 0;
}

static void machine_bytes_register_desc_free(MachineEmitRegisterDesc *desc) {
    if (!desc) {
        return;
    }
    free(desc->name);
    desc->name = NULL;
}

static size_t machine_bytes_function_total_byte_count(const MachineBytesFunction *function) {
    if (!function || function->block_count == 0 || !function->blocks) {
        return 0;
    }
    return function->blocks[function->block_count - 1].end_byte_offset;
}

static int machine_bytes_copy_function_bytes(
    const MachineBytesFunction *function,
    unsigned char *dest_bytes,
    size_t dest_byte_count) {
    size_t block_index;
    size_t running_offset = 0;

    if (!function) {
        return 0;
    }
    if (dest_byte_count == 0) {
        return machine_bytes_function_total_byte_count(function) == 0;
    }
    if (!dest_bytes) {
        return 0;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        const MachineBytesBlock *block = &function->blocks[block_index];

        if (running_offset != block->start_byte_offset ||
            running_offset + block->byte_count > dest_byte_count ||
            (block->byte_count > 0 && !block->bytes)) {
            return 0;
        }
        if (block->byte_count > 0) {
            memcpy(dest_bytes + running_offset, block->bytes, block->byte_count);
        }
        running_offset += block->byte_count;
    }
    return running_offset == dest_byte_count;
}

static size_t machine_bytes_function_reference_count(const MachineBytesFunction *function) {
    size_t reference_count = 0;
    size_t block_index;

    if (!function || !function->blocks) {
        return 0;
    }
    for (block_index = 0; block_index < function->block_count; ++block_index) {
        const MachineBytesBlock *block = &function->blocks[block_index];
        size_t op_index;

        for (op_index = 0; op_index < block->block.op_count; ++op_index) {
            MachineSelectOpKind op_kind = block->block.ops[op_index].kind;

            if (machine_bytes_op_has_call_payload(op_kind)) {
                ++reference_count;
            } else if (op_kind == MACHINE_SELECT_OP_ADDR_GLOBAL) {
                reference_count += 2u;
            } else if (op_kind == MACHINE_SELECT_OP_LOAD_GLOBAL ||
                op_kind == MACHINE_SELECT_OP_STORE_GLOBAL ||
                op_kind == MACHINE_SELECT_OP_STORE_GLOBAL_IMM) {
                reference_count += 2u;
            }
        }
        switch (block->block.terminator.kind) {
            case MACHINE_LAYOUT_TERM_FALLTHROUGH:
            case MACHINE_LAYOUT_TERM_JUMP:
                ++reference_count;
                break;
            case MACHINE_LAYOUT_TERM_BRANCH:
            case MACHINE_LAYOUT_TERM_BRANCH_FALLTHROUGH:
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH:
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM:
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_FALLTHROUGH:
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM_FALLTHROUGH:
                reference_count += 2;
                break;
            default:
                break;
        }
    }
    return reference_count;
}

static int machine_bytes_report_append_call_reference(
    MachineBytesReferenceSummary *summary,
    MachineBytesTargetProfile profile,
    size_t function_index,
    size_t emit_index,
    const MachineBytesBlock *block,
    size_t owner_byte_offset,
    size_t owner_byte_count,
    const MachineEmitOp *op,
    const MachineBytesFunction *function,
    const MachineBytesProgram *program,
    const size_t *function_byte_offsets) {
    size_t target_function_index = 0;
    const MachineBytesFunction *target_function = NULL;

    if (!summary || !block || !op || !program) {
        return 0;
    }

    memset(summary, 0, sizeof(*summary));
    summary->kind = MACHINE_BYTES_REFERENCE_CALL;
    summary->function_index = function_index;
    summary->emit_index = emit_index;
    summary->source_label_name = block->label_name;
    summary->owner_byte_offset = owner_byte_offset;
    summary->owner_byte_count = owner_byte_count;
    if (profile == MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW) {
        summary->patch_byte_offset = machine_bytes_riscv_call_patch_offset(function, owner_byte_offset, op);
        summary->patch_byte_count = 4u;
    } else {
        if (!machine_bytes_reference_patch_for_call(
                profile,
                owner_byte_offset,
                owner_byte_count,
                &summary->patch_byte_offset,
                &summary->patch_byte_count)) {
            return 0;
        }
    }
    summary->op_kind = op->kind;
    summary->terminator_kind = (MachineBytesTerminatorKind)0;
    summary->target_name = op->as.call.callee_name;

    if (op->as.call.callee_name &&
        machine_bytes_program_get_function_by_name(program, op->as.call.callee_name, &target_function_index, &target_function)) {
        summary->has_target_function_index = 1;
        summary->target_function_index = target_function_index;
        summary->has_target_emit_index = 0;
        summary->has_target_byte_offset = function_byte_offsets != NULL;
        if (function_byte_offsets) {
            summary->target_byte_offset = function_byte_offsets[target_function_index];
        }
    }
    return 1;
}

static int machine_bytes_report_append_control_reference(
    MachineBytesReferenceSummary *summary,
    MachineBytesReferenceKind kind,
    size_t function_index,
    size_t emit_index,
    const MachineBytesBlock *block,
    size_t owner_byte_offset,
    size_t owner_byte_count,
    size_t patch_byte_offset,
    size_t patch_byte_count,
    MachineBytesTerminatorKind terminator_kind,
    const char *target_name,
    size_t target_function_index,
    int has_target_emit_index,
    size_t target_emit_index,
    int has_target_byte_offset,
    size_t target_byte_offset) {
    if (!summary || !block || !target_name) {
        return 0;
    }

    memset(summary, 0, sizeof(*summary));
    summary->kind = kind;
    summary->function_index = function_index;
    summary->emit_index = emit_index;
    summary->source_label_name = block->label_name;
    summary->owner_byte_offset = owner_byte_offset;
    summary->owner_byte_count = owner_byte_count;
    summary->patch_byte_offset = patch_byte_offset;
    summary->patch_byte_count = patch_byte_count;
    summary->op_kind = (MachineSelectOpKind)0;
    summary->terminator_kind = terminator_kind;
    summary->target_name = target_name;
    summary->has_target_function_index = 1;
    summary->target_function_index = target_function_index;
    summary->has_target_emit_index = has_target_emit_index;
    summary->target_emit_index = target_emit_index;
    summary->has_target_byte_offset = has_target_byte_offset;
    summary->target_byte_offset = target_byte_offset;
    return 1;
}

static int machine_bytes_report_append_global_data_reference(
    MachineBytesReferenceSummary *summary,
    MachineBytesReferenceKind kind,
    MachineBytesTargetProfile profile,
    size_t function_index,
    size_t emit_index,
    const MachineBytesBlock *block,
    size_t owner_byte_offset,
    size_t owner_byte_count,
    const MachineEmitOp *op,
    const MachineBytesFunction *function,
    const MachineBytesProgram *program) {
    const MachineEmitSlotRef *slot = NULL;
    const MachineEmitGlobal *global = NULL;
    size_t target_byte_offset = 0u;
    size_t prior_index;

    if (!summary || !block || !op || !program) {
        return 0;
    }
    switch (op->kind) {
        case MACHINE_SELECT_OP_ADDR_GLOBAL:
            slot = &op->as.addr_slot;
            break;
        case MACHINE_SELECT_OP_LOAD_GLOBAL:
            slot = &op->as.load_slot;
            break;
        case MACHINE_SELECT_OP_STORE_GLOBAL:
        case MACHINE_SELECT_OP_STORE_GLOBAL_IMM:
            slot = &op->as.store.slot;
            break;
        default:
            return 0;
    }
    if (!slot || slot->kind != MACHINE_SELECT_SLOT_GLOBAL || slot->id >= program->global_count || !program->globals) {
        return 0;
    }
    global = &program->globals[slot->id];
    if (!global->name || global->name[0] == '\0') {
        return 0;
    }
    for (prior_index = 0u; prior_index < slot->id; ++prior_index) {
        const MachineEmitGlobal *prior_global = &program->globals[prior_index];
        size_t prior_byte_size = prior_global->byte_size ? prior_global->byte_size : 4u;

        if (prior_global->has_initializer == global->has_initializer) {
            target_byte_offset += prior_byte_size;
        }
    }

    memset(summary, 0, sizeof(*summary));
    summary->kind = kind;
    summary->function_index = function_index;
    summary->emit_index = emit_index;
    summary->source_label_name = block->label_name;
    summary->owner_byte_offset = owner_byte_offset;
    summary->owner_byte_count = owner_byte_count;
    if (!machine_bytes_reference_patch_for_global_access(
            profile,
            op,
            function,
            owner_byte_offset,
            owner_byte_count,
            &summary->patch_byte_offset,
            &summary->patch_byte_count)) {
        return 0;
    }
    if (profile == MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW) {
        size_t prepare_size = 0u;

        if (op->kind == MACHINE_SELECT_OP_ADDR_GLOBAL) {
            if (kind == MACHINE_BYTES_REFERENCE_DATA_ADDR) {
                summary->patch_byte_offset = owner_byte_offset;
                summary->patch_byte_count = 4u;
            } else if (kind == MACHINE_BYTES_REFERENCE_DATA_LOAD) {
                summary->patch_byte_offset = owner_byte_offset + 4u;
                summary->patch_byte_count = 4u;
            }
        } else if (op->kind == MACHINE_SELECT_OP_STORE_GLOBAL) {
            prepare_size = machine_bytes_riscv_prepare_operand_size(function, &op->as.store.value);
        } else if (op->kind == MACHINE_SELECT_OP_STORE_GLOBAL_IMM) {
            prepare_size = machine_bytes_riscv_immediate_uses_zero_register(op->as.store.value.immediate)
                ? 0u
                : machine_bytes_riscv_materialize_size(op->as.store.value.immediate);
        }
        if (op->kind != MACHINE_SELECT_OP_ADDR_GLOBAL && kind == MACHINE_BYTES_REFERENCE_DATA_ADDR) {
            summary->patch_byte_offset = owner_byte_offset + prepare_size;
            summary->patch_byte_count = 4u;
        } else if (op->kind != MACHINE_SELECT_OP_ADDR_GLOBAL && kind == MACHINE_BYTES_REFERENCE_DATA_LOAD) {
            summary->patch_byte_offset = owner_byte_offset + 4u;
            summary->patch_byte_count = 4u;
        } else if (op->kind != MACHINE_SELECT_OP_ADDR_GLOBAL && kind == MACHINE_BYTES_REFERENCE_DATA_STORE) {
            summary->patch_byte_offset = owner_byte_offset + prepare_size + 4u;
            summary->patch_byte_count = 4u;
        }
    }
    summary->op_kind = op->kind;
    summary->terminator_kind = (MachineBytesTerminatorKind)0;
    summary->target_name = global->name;
    summary->has_target_byte_offset = 1;
    summary->target_byte_offset = target_byte_offset;
    return 1;
}

static int machine_bytes_report_populate_function_references(
    const MachineBytesReport *report,
    size_t function_index,
    size_t start_reference_index) {
    const MachineBytesFunction *function;
    size_t reference_index = start_reference_index;
    size_t block_index;

    if (!report || function_index >= report->program.function_count) {
        return 0;
    }
    function = &report->program.functions[function_index];
    if (!report->reference_summaries) {
        return machine_bytes_function_reference_count(function) == 0u;
    }
    for (block_index = 0; block_index < function->block_count; ++block_index) {
        const MachineBytesBlock *block = &function->blocks[block_index];
        size_t program_byte_offset =
            (report->function_byte_offsets ? report->function_byte_offsets[function_index] : 0) + block->start_byte_offset;
        size_t op_index;

        for (op_index = 0; op_index < block->block.op_count; ++op_index) {
            const MachineEmitOp *op = &block->block.ops[op_index];
            size_t op_size = machine_bytes_op_encoded_size_for_profile(report->program.target_profile, function, op);

            if (machine_bytes_op_has_call_payload(op->kind)) {
                if (!machine_bytes_report_append_call_reference(
                        &report->reference_summaries[reference_index++],
                        report->program.target_profile,
                        function_index,
                        block_index,
                        block,
                        program_byte_offset,
                        op_size,
                        op,
                        function,
                        &report->program,
                        report->function_byte_offsets)) {
                    return 0;
                }
            } else if (op->kind == MACHINE_SELECT_OP_ADDR_GLOBAL) {
                if (!machine_bytes_report_append_global_data_reference(
                        &report->reference_summaries[reference_index++],
                        MACHINE_BYTES_REFERENCE_DATA_ADDR,
                        report->program.target_profile,
                        function_index,
                        block_index,
                        block,
                        program_byte_offset,
                        4u,
                        op,
                        function,
                        &report->program) ||
                    !machine_bytes_report_append_global_data_reference(
                        &report->reference_summaries[reference_index++],
                        MACHINE_BYTES_REFERENCE_DATA_LOAD,
                        report->program.target_profile,
                        function_index,
                        block_index,
                        block,
                        program_byte_offset,
                        8u,
                        op,
                        function,
                        &report->program)) {
                    return 0;
                }
            } else if (op->kind == MACHINE_SELECT_OP_LOAD_GLOBAL ||
                op->kind == MACHINE_SELECT_OP_STORE_GLOBAL ||
                op->kind == MACHINE_SELECT_OP_STORE_GLOBAL_IMM) {
                if (!machine_bytes_report_append_global_data_reference(
                        &report->reference_summaries[reference_index++],
                        MACHINE_BYTES_REFERENCE_DATA_ADDR,
                        report->program.target_profile,
                        function_index,
                        block_index,
                        block,
                        program_byte_offset,
                        op_size,
                        op,
                        function,
                        &report->program) ||
                    !machine_bytes_report_append_global_data_reference(
                        &report->reference_summaries[reference_index++],
                        op->kind == MACHINE_SELECT_OP_LOAD_GLOBAL
                            ? MACHINE_BYTES_REFERENCE_DATA_LOAD
                            : MACHINE_BYTES_REFERENCE_DATA_STORE,
                        report->program.target_profile,
                        function_index,
                        block_index,
                        block,
                        program_byte_offset,
                        op_size,
                        op,
                        function,
                        &report->program)) {
                    return 0;
                }
            }
            program_byte_offset += op_size;
        }

        {
            MachineBytesTerminatorSummary terminator_summary;
            size_t owner_byte_offset =
                (report->function_byte_offsets ? report->function_byte_offsets[function_index] : 0) +
                block->terminator_byte_offset;
            size_t owner_byte_count = machine_bytes_terminator_encoded_size_for_profile(
                report->program.target_profile, function, &block->block.terminator);
            size_t patch_byte_offset = 0;
            size_t patch_byte_count = 0;

            if (!machine_bytes_function_get_block_terminator_summary(function, block_index, &terminator_summary)) {
                return 0;
            }
            switch (block->block.terminator.kind) {
                case MACHINE_LAYOUT_TERM_FALLTHROUGH:
                case MACHINE_LAYOUT_TERM_JUMP:
                    if (!machine_bytes_reference_patch_for_control(
                            report->program.target_profile,
                            MACHINE_BYTES_REFERENCE_CONTROL_PRIMARY,
                            block->block.terminator.kind,
                            owner_byte_offset,
                            owner_byte_count,
                            &patch_byte_offset,
                            &patch_byte_count)) {
                        return 0;
                    }
                    if (!machine_bytes_report_append_control_reference(
                            &report->reference_summaries[reference_index++],
                            MACHINE_BYTES_REFERENCE_CONTROL_PRIMARY,
                            function_index,
                            block_index,
                            block,
                            owner_byte_offset,
                            owner_byte_count,
                            patch_byte_offset,
                            patch_byte_count,
                            block->block.terminator.kind,
                            terminator_summary.primary_target_label_name,
                            function_index,
                            1,
                            block->block.terminator.kind == MACHINE_LAYOUT_TERM_JUMP
                                ? block->block.terminator.as.jump_target
                                : block->block.terminator.as.fallthrough_target,
                            1,
                            (report->function_byte_offsets ? report->function_byte_offsets[function_index] : 0) +
                                terminator_summary.primary_target_byte_offset)) {
                        return 0;
                    }
                    break;
                case MACHINE_LAYOUT_TERM_BRANCH:
                    if (!machine_bytes_reference_patch_for_control(
                            report->program.target_profile,
                            MACHINE_BYTES_REFERENCE_CONTROL_PRIMARY,
                            block->block.terminator.kind,
                            owner_byte_offset,
                            owner_byte_count,
                            &patch_byte_offset,
                            &patch_byte_count)) {
                        return 0;
                    }
                    if (!machine_bytes_report_append_control_reference(
                            &report->reference_summaries[reference_index++],
                            MACHINE_BYTES_REFERENCE_CONTROL_PRIMARY,
                            function_index,
                            block_index,
                            block,
                            owner_byte_offset,
                            owner_byte_count,
                            patch_byte_offset,
                            patch_byte_count,
                            block->block.terminator.kind,
                            terminator_summary.primary_target_label_name,
                            function_index,
                            1,
                            block->block.terminator.as.branch.then_target,
                            1,
                            (report->function_byte_offsets ? report->function_byte_offsets[function_index] : 0) +
                                terminator_summary.primary_target_byte_offset) ||
                        !machine_bytes_reference_patch_for_control(
                            report->program.target_profile,
                            MACHINE_BYTES_REFERENCE_CONTROL_SECONDARY,
                            block->block.terminator.kind,
                            owner_byte_offset,
                            owner_byte_count,
                            &patch_byte_offset,
                            &patch_byte_count) ||
                        !machine_bytes_report_append_control_reference(
                            &report->reference_summaries[reference_index++],
                            MACHINE_BYTES_REFERENCE_CONTROL_SECONDARY,
                            function_index,
                            block_index,
                            block,
                            owner_byte_offset,
                            owner_byte_count,
                            patch_byte_offset,
                            patch_byte_count,
                            block->block.terminator.kind,
                            terminator_summary.secondary_target_label_name,
                            function_index,
                            1,
                            block->block.terminator.as.branch.else_target,
                            1,
                            (report->function_byte_offsets ? report->function_byte_offsets[function_index] : 0) +
                                terminator_summary.secondary_target_byte_offset)) {
                        return 0;
                    }
                    break;
                case MACHINE_LAYOUT_TERM_BRANCH_FALLTHROUGH:
                    if (!machine_bytes_reference_patch_for_control(
                            report->program.target_profile,
                            MACHINE_BYTES_REFERENCE_CONTROL_PRIMARY,
                            block->block.terminator.kind,
                            owner_byte_offset,
                            owner_byte_count,
                            &patch_byte_offset,
                            &patch_byte_count)) {
                        return 0;
                    }
                    if (!machine_bytes_report_append_control_reference(
                            &report->reference_summaries[reference_index++],
                            MACHINE_BYTES_REFERENCE_CONTROL_PRIMARY,
                            function_index,
                            block_index,
                            block,
                            owner_byte_offset,
                            owner_byte_count,
                            patch_byte_offset,
                            patch_byte_count,
                            block->block.terminator.kind,
                            terminator_summary.primary_target_label_name,
                            function_index,
                            1,
                            block->block.terminator.as.branch_fallthrough.taken_target,
                            1,
                            (report->function_byte_offsets ? report->function_byte_offsets[function_index] : 0) +
                                terminator_summary.primary_target_byte_offset) ||
                        !machine_bytes_reference_patch_for_control(
                            report->program.target_profile,
                            MACHINE_BYTES_REFERENCE_CONTROL_SECONDARY,
                            block->block.terminator.kind,
                            owner_byte_offset,
                            owner_byte_count,
                            &patch_byte_offset,
                            &patch_byte_count) ||
                        !machine_bytes_report_append_control_reference(
                            &report->reference_summaries[reference_index++],
                            MACHINE_BYTES_REFERENCE_CONTROL_SECONDARY,
                            function_index,
                            block_index,
                            block,
                            owner_byte_offset,
                            owner_byte_count,
                            patch_byte_offset,
                            patch_byte_count,
                            block->block.terminator.kind,
                            terminator_summary.secondary_target_label_name,
                            function_index,
                            1,
                            block->block.terminator.as.branch_fallthrough.fallthrough_target,
                            1,
                            (report->function_byte_offsets ? report->function_byte_offsets[function_index] : 0) +
                                terminator_summary.secondary_target_byte_offset)) {
                        return 0;
                    }
                    break;
                case MACHINE_LAYOUT_TERM_COMPARE_BRANCH:
                case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM:
                    if (!machine_bytes_reference_patch_for_control(
                            report->program.target_profile,
                            MACHINE_BYTES_REFERENCE_CONTROL_PRIMARY,
                            block->block.terminator.kind,
                            owner_byte_offset,
                            owner_byte_count,
                            &patch_byte_offset,
                            &patch_byte_count)) {
                        return 0;
                    }
                    if (!machine_bytes_report_append_control_reference(
                            &report->reference_summaries[reference_index++],
                            MACHINE_BYTES_REFERENCE_CONTROL_PRIMARY,
                            function_index,
                            block_index,
                            block,
                            owner_byte_offset,
                            owner_byte_count,
                            patch_byte_offset,
                            patch_byte_count,
                            block->block.terminator.kind,
                            terminator_summary.primary_target_label_name,
                            function_index,
                            1,
                            block->block.terminator.as.compare_branch.then_target,
                            1,
                            (report->function_byte_offsets ? report->function_byte_offsets[function_index] : 0) +
                                terminator_summary.primary_target_byte_offset) ||
                        !machine_bytes_reference_patch_for_control(
                            report->program.target_profile,
                            MACHINE_BYTES_REFERENCE_CONTROL_SECONDARY,
                            block->block.terminator.kind,
                            owner_byte_offset,
                            owner_byte_count,
                            &patch_byte_offset,
                            &patch_byte_count) ||
                        !machine_bytes_report_append_control_reference(
                            &report->reference_summaries[reference_index++],
                            MACHINE_BYTES_REFERENCE_CONTROL_SECONDARY,
                            function_index,
                            block_index,
                            block,
                            owner_byte_offset,
                            owner_byte_count,
                            patch_byte_offset,
                            patch_byte_count,
                            block->block.terminator.kind,
                            terminator_summary.secondary_target_label_name,
                            function_index,
                            1,
                            block->block.terminator.as.compare_branch.else_target,
                            1,
                            (report->function_byte_offsets ? report->function_byte_offsets[function_index] : 0) +
                                terminator_summary.secondary_target_byte_offset)) {
                        return 0;
                    }
                    break;
                case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_FALLTHROUGH:
                case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM_FALLTHROUGH:
                    if (!machine_bytes_reference_patch_for_control(
                            report->program.target_profile,
                            MACHINE_BYTES_REFERENCE_CONTROL_PRIMARY,
                            block->block.terminator.kind,
                            owner_byte_offset,
                            owner_byte_count,
                            &patch_byte_offset,
                            &patch_byte_count)) {
                        return 0;
                    }
                    if (!machine_bytes_report_append_control_reference(
                            &report->reference_summaries[reference_index++],
                            MACHINE_BYTES_REFERENCE_CONTROL_PRIMARY,
                            function_index,
                            block_index,
                            block,
                            owner_byte_offset,
                            owner_byte_count,
                            patch_byte_offset,
                            patch_byte_count,
                            block->block.terminator.kind,
                            terminator_summary.primary_target_label_name,
                            function_index,
                            1,
                            block->block.terminator.as.compare_branch_fallthrough.taken_target,
                            1,
                            (report->function_byte_offsets ? report->function_byte_offsets[function_index] : 0) +
                                terminator_summary.primary_target_byte_offset) ||
                        !machine_bytes_reference_patch_for_control(
                            report->program.target_profile,
                            MACHINE_BYTES_REFERENCE_CONTROL_SECONDARY,
                            block->block.terminator.kind,
                            owner_byte_offset,
                            owner_byte_count,
                            &patch_byte_offset,
                            &patch_byte_count) ||
                        !machine_bytes_report_append_control_reference(
                            &report->reference_summaries[reference_index++],
                            MACHINE_BYTES_REFERENCE_CONTROL_SECONDARY,
                            function_index,
                            block_index,
                            block,
                            owner_byte_offset,
                            owner_byte_count,
                            patch_byte_offset,
                            patch_byte_count,
                            block->block.terminator.kind,
                            terminator_summary.secondary_target_label_name,
                            function_index,
                            1,
                            block->block.terminator.as.compare_branch_fallthrough.fallthrough_target,
                            1,
                            (report->function_byte_offsets ? report->function_byte_offsets[function_index] : 0) +
                                terminator_summary.secondary_target_byte_offset)) {
                        return 0;
                    }
                    break;
                default:
                    break;
            }
        }
    }

    return 1;
}

static size_t machine_bytes_report_find_symbol_index_by_name(
    const MachineBytesReport *report,
    const char *symbol_name) {
    size_t lo = 0u;
    size_t hi;

    if (!report || !symbol_name || !report->symbol_summaries || !report->symbol_summaries_by_name) {
        return (size_t)-1;
    }
    hi = report->total_symbol_summary_count;
    while (lo < hi) {
        size_t mid = lo + ((hi - lo) / 2u);
        MachineBytesSymbolSummary *symbol = report->symbol_summaries_by_name[mid];
        int cmp;

        if (!symbol || !symbol->name) {
            return (size_t)-1;
        }
        cmp = strcmp(symbol->name, symbol_name);
        if (cmp < 0) {
            lo = mid + 1u;
        } else {
            hi = mid;
        }
    }
    if (lo < report->total_symbol_summary_count &&
        report->symbol_summaries_by_name[lo] &&
        report->symbol_summaries_by_name[lo]->name &&
        strcmp(report->symbol_summaries_by_name[lo]->name, symbol_name) == 0) {
        return (size_t)(report->symbol_summaries_by_name[lo] - report->symbol_summaries);
    }
    return (size_t)-1;
}

static int machine_bytes_compare_symbol_name_ptrs(const void *lhs, const void *rhs) {
    const MachineBytesSymbolSummary *const *left = (const MachineBytesSymbolSummary *const *)lhs;
    const MachineBytesSymbolSummary *const *right = (const MachineBytesSymbolSummary *const *)rhs;
    const char *left_name;
    const char *right_name;
    int cmp;

    left_name = (*left && (*left)->name) ? (*left)->name : "";
    right_name = (*right && (*right)->name) ? (*right)->name : "";
    cmp = strcmp(left_name, right_name);
    if (cmp != 0) {
        return cmp;
    }
    if (*left < *right) {
        return -1;
    }
    if (*left > *right) {
        return 1;
    }
    return 0;
}

static int machine_bytes_report_build_symbol_name_index(MachineBytesReport *report) {
    size_t symbol_index;

    if (!report) {
        return 0;
    }
    free(report->symbol_summaries_by_name);
    report->symbol_summaries_by_name = NULL;
    if (report->total_symbol_summary_count == 0u) {
        return 1;
    }
    if (!report->symbol_summaries) {
        return 0;
    }
    report->symbol_summaries_by_name = (MachineBytesSymbolSummary **)malloc(
        report->total_symbol_summary_count * sizeof(MachineBytesSymbolSummary *));
    if (!report->symbol_summaries_by_name) {
        return 0;
    }
    for (symbol_index = 0u; symbol_index < report->total_symbol_summary_count; ++symbol_index) {
        report->symbol_summaries_by_name[symbol_index] = &report->symbol_summaries[symbol_index];
    }
    qsort(report->symbol_summaries_by_name,
        report->total_symbol_summary_count,
        sizeof(MachineBytesSymbolSummary *),
        machine_bytes_compare_symbol_name_ptrs);
    return 1;
}

static int machine_bytes_report_populate_symbols(MachineBytesReport *report) {
    size_t symbol_index = 0;
    size_t function_index;
    size_t global_index;
    size_t reference_index;
    size_t sbss_byte_offset = 0u;
    size_t sdata_byte_offset = 0u;

    if (!report || !report->symbol_summaries) {
        return report && report->total_symbol_summary_count == 0;
    }

    for (function_index = 0; function_index < report->program.function_count; ++function_index) {
        const MachineBytesFunction *function = &report->program.functions[function_index];
        size_t block_index;
        MachineBytesSymbolSummary *symbol = &report->symbol_summaries[symbol_index++];

        memset(symbol, 0, sizeof(*symbol));
        symbol->kind = MACHINE_BYTES_SYMBOL_FUNCTION;
        symbol->name = function->name;
        symbol->is_defined = 1;
        symbol->has_section_index = 1;
        symbol->section_index = 0u;
        symbol->has_function_index = 1;
        symbol->function_index = function_index;
        symbol->has_byte_offset = 1;
        symbol->byte_offset = report->function_byte_offsets ? report->function_byte_offsets[function_index] : 0;
        symbol->byte_count = machine_bytes_function_total_byte_count(function);

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            const MachineBytesBlock *block = &function->blocks[block_index];
            MachineBytesSymbolSummary *block_symbol = &report->symbol_summaries[symbol_index++];

            memset(block_symbol, 0, sizeof(*block_symbol));
            block_symbol->kind = MACHINE_BYTES_SYMBOL_BLOCK;
            block_symbol->name = block->label_name;
            block_symbol->is_defined = 1;
            block_symbol->has_section_index = 1;
            block_symbol->section_index = 0u;
            block_symbol->has_function_index = 1;
            block_symbol->function_index = function_index;
            block_symbol->has_emit_index = 1;
            block_symbol->emit_index = block_index;
            block_symbol->has_byte_offset = 1;
            block_symbol->byte_offset =
                (report->function_byte_offsets ? report->function_byte_offsets[function_index] : 0) + block->start_byte_offset;
            block_symbol->byte_count = block->byte_count;
        }
    }

    for (global_index = 0u; global_index < report->program.global_count; ++global_index) {
        const MachineEmitGlobal *global = &report->program.globals[global_index];
        MachineBytesSymbolSummary *symbol;
        size_t byte_size = global->byte_size ? global->byte_size : 4u;

        if (!global->name || global->name[0] == '\0' || global->has_initializer) {
            continue;
        }

        symbol = &report->symbol_summaries[symbol_index++];
        memset(symbol, 0, sizeof(*symbol));
        symbol->kind = MACHINE_BYTES_SYMBOL_GLOBAL_OBJECT;
        symbol->name = global->name;
        symbol->is_defined = 1;
        symbol->has_section_index = 1;
        symbol->section_index = 1u;
        symbol->has_byte_offset = 1;
        symbol->byte_offset = sbss_byte_offset;
        symbol->byte_count = byte_size;
        sbss_byte_offset += byte_size;
    }

    for (global_index = 0u; global_index < report->program.global_count; ++global_index) {
        const MachineEmitGlobal *global = &report->program.globals[global_index];
        MachineBytesSymbolSummary *symbol;
        size_t byte_size = global->byte_size ? global->byte_size : 4u;

        if (!global->name || global->name[0] == '\0' || !global->has_initializer) {
            continue;
        }

        symbol = &report->symbol_summaries[symbol_index++];
        memset(symbol, 0, sizeof(*symbol));
        symbol->kind = MACHINE_BYTES_SYMBOL_GLOBAL_OBJECT;
        symbol->name = global->name;
        symbol->is_defined = 1;
        symbol->has_section_index = 1;
        symbol->section_index = 2u;
        symbol->has_byte_offset = 1;
        symbol->byte_offset = sdata_byte_offset;
        symbol->byte_count = byte_size;
        sdata_byte_offset += byte_size;
    }

    for (reference_index = 0; reference_index < report->total_reference_summary_count; ++reference_index) {
        const MachineBytesReferenceSummary *reference = &report->reference_summaries[reference_index];

        if (reference->kind != MACHINE_BYTES_REFERENCE_CALL ||
            reference->has_target_function_index ||
            !reference->target_name || reference->target_name[0] == '\0' ||
            machine_bytes_report_find_symbol_index_by_name(report, reference->target_name) != (size_t)-1) {
            continue;
        }

        {
            MachineBytesSymbolSummary *symbol = &report->symbol_summaries[symbol_index++];

            memset(symbol, 0, sizeof(*symbol));
            symbol->kind = MACHINE_BYTES_SYMBOL_EXTERNAL;
            symbol->name = reference->target_name;
            symbol->is_defined = 0;
            symbol->byte_count = 0;
        }
    }

    report->total_symbol_summary_count = symbol_index;
    return 1;
}

static int machine_bytes_report_populate_fixups(MachineBytesReport *report) {
    size_t function_index;
    size_t fixup_index = 0;

    if (!report) {
        return 0;
    }
    if (report->total_reference_summary_count == 0) {
        report->total_fixup_summary_count = 0;
        return 1;
    }
    if (!report->reference_summaries || !report->fixup_summaries || !report->function_fixup_offsets) {
        return 0;
    }

    for (function_index = 0; function_index < report->program.function_count; ++function_index) {
        const MachineBytesReferenceSummary *function_references = NULL;
        size_t reference_count = 0;
        size_t reference_index;

        report->function_fixup_offsets[function_index] = fixup_index;
        if (!machine_bytes_report_get_function_reference_summaries(
                report, function_index, &reference_count, &function_references)) {
            return 0;
        }
        for (reference_index = 0; reference_index < reference_count; ++reference_index) {
            const MachineBytesReferenceSummary *reference = &function_references[reference_index];
            MachineBytesFixupSummary *fixup;

            if ((reference->kind == MACHINE_BYTES_REFERENCE_CONTROL_PRIMARY ||
                    reference->kind == MACHINE_BYTES_REFERENCE_CONTROL_SECONDARY) &&
                reference->patch_byte_count == 0u) {
                continue;
            }
            fixup = &report->fixup_summaries[fixup_index++];

            memset(fixup, 0, sizeof(*fixup));
            switch (reference->kind) {
                case MACHINE_BYTES_REFERENCE_CALL:
                    fixup->kind = MACHINE_BYTES_FIXUP_CALL_TARGET;
                    break;
                case MACHINE_BYTES_REFERENCE_CONTROL_PRIMARY:
                    fixup->kind = MACHINE_BYTES_FIXUP_CONTROL_PRIMARY;
                    break;
                case MACHINE_BYTES_REFERENCE_CONTROL_SECONDARY:
                    fixup->kind = MACHINE_BYTES_FIXUP_CONTROL_SECONDARY;
                    break;
                case MACHINE_BYTES_REFERENCE_DATA_ADDR:
                    fixup->kind = MACHINE_BYTES_FIXUP_DATA_ADDR_TARGET;
                    break;
                case MACHINE_BYTES_REFERENCE_DATA_LOAD:
                    fixup->kind = MACHINE_BYTES_FIXUP_DATA_LOAD_TARGET;
                    break;
                case MACHINE_BYTES_REFERENCE_DATA_STORE:
                    fixup->kind = MACHINE_BYTES_FIXUP_DATA_STORE_TARGET;
                    break;
                default:
                    return 0;
            }
            fixup->target_kind = reference->has_target_emit_index
                ? MACHINE_BYTES_FIXUP_TARGET_BLOCK
                : MACHINE_BYTES_FIXUP_TARGET_SYMBOL;
            fixup->function_index = reference->function_index;
            fixup->emit_index = reference->emit_index;
            fixup->source_label_name = reference->source_label_name;
            fixup->owner_byte_offset = reference->owner_byte_offset;
            fixup->owner_byte_count = reference->owner_byte_count;
            fixup->patch_byte_offset = reference->patch_byte_offset;
            fixup->patch_byte_count = reference->patch_byte_count;
            fixup->target_name = reference->target_name;
            fixup->has_target_function_index = reference->has_target_function_index;
            fixup->target_function_index = reference->target_function_index;
            fixup->has_target_emit_index = reference->has_target_emit_index;
            fixup->target_emit_index = reference->target_emit_index;
            fixup->has_target_byte_offset = reference->has_target_byte_offset;
            fixup->target_byte_offset = reference->target_byte_offset;
            fixup->has_target_symbol_index = reference->target_name != NULL;
            if (reference->target_name) {
                size_t target_symbol_index = machine_bytes_report_find_symbol_index_by_name(report, reference->target_name);

                if (target_symbol_index == (size_t)-1) {
                    return 0;
                }
                fixup->target_symbol_index = target_symbol_index;
                ++report->symbol_summaries[target_symbol_index].incoming_fixup_count;
            }
        }
    }

    report->total_fixup_summary_count = fixup_index;
    return fixup_index == report->total_fixup_summary_count;
}

static int machine_bytes_report_populate_sections(MachineBytesReport *report) {
    size_t symbol_index;
    size_t sbss_symbol_start_index = 0u;
    size_t sdata_symbol_start_index = 0u;
    size_t sbss_symbol_count = 0u;
    size_t sdata_symbol_count = 0u;
    size_t sbss_byte_count = 0u;
    size_t sdata_byte_count = 0u;
    size_t section_index = 0u;

    if (!report) {
        return 0;
    }
    if (report->program.function_count == 0u && report->program.global_count == 0u) {
        report->total_section_summary_count = 0;
        return 1;
    }
    if (!report->section_summaries) {
        return 0;
    }

    for (symbol_index = 0; symbol_index < report->total_symbol_summary_count; ++symbol_index) {
        const MachineBytesSymbolSummary *symbol = &report->symbol_summaries[symbol_index];

        if (!symbol->is_defined || !symbol->has_section_index) {
            continue;
        }
        if (symbol->section_index == 1u) {
            if (sbss_symbol_count == 0u) {
                sbss_symbol_start_index = symbol_index;
            }
            sbss_symbol_count += 1u;
            sbss_byte_count += symbol->byte_count;
        } else if (symbol->section_index == 2u) {
            if (sdata_symbol_count == 0u) {
                sdata_symbol_start_index = symbol_index;
            }
            sdata_symbol_count += 1u;
            sdata_byte_count += symbol->byte_count;
        }
    }

    memset(report->section_summaries, 0, 3u * sizeof(report->section_summaries[0]));

    if (report->program.function_count > 0u) {
        report->section_summaries[section_index].kind = MACHINE_BYTES_SECTION_TEXT;
        report->section_summaries[section_index].name = ".text";
        report->section_summaries[section_index].start_byte_offset = 0u;
        report->section_summaries[section_index].end_byte_offset = report->total_program_byte_count;
        report->section_summaries[section_index].byte_count = report->total_program_byte_count;
        report->section_summaries[section_index].function_count = report->program.function_count;
        report->section_summaries[section_index].block_count = report->total_block_summary_count;
        report->section_summaries[section_index].symbol_start_index = 0u;
        report->section_summaries[section_index].symbol_count =
            report->program.function_count + report->total_block_summary_count;
        report->section_summaries[section_index].fixup_start_index = 0u;
        report->section_summaries[section_index].fixup_count = report->total_fixup_summary_count;
        section_index += 1u;
    }
    if (sbss_symbol_count > 0u) {
        size_t start_byte_offset = section_index == 0u
            ? 0u
            : report->section_summaries[section_index - 1u].end_byte_offset;

        report->section_summaries[section_index].kind = MACHINE_BYTES_SECTION_SBSS;
        report->section_summaries[section_index].name = ".sbss";
        report->section_summaries[section_index].start_byte_offset = start_byte_offset;
        report->section_summaries[section_index].end_byte_offset = start_byte_offset + sbss_byte_count;
        report->section_summaries[section_index].byte_count = sbss_byte_count;
        report->section_summaries[section_index].symbol_start_index = sbss_symbol_start_index;
        report->section_summaries[section_index].symbol_count = sbss_symbol_count;
        report->section_summaries[section_index].fixup_start_index = report->total_fixup_summary_count;
        report->section_summaries[section_index].fixup_count = 0u;
        section_index += 1u;
    }
    if (sdata_symbol_count > 0u) {
        size_t start_byte_offset = section_index == 0u
            ? 0u
            : report->section_summaries[section_index - 1u].end_byte_offset;

        report->section_summaries[section_index].kind = MACHINE_BYTES_SECTION_SDATA;
        report->section_summaries[section_index].name = ".sdata";
        report->section_summaries[section_index].start_byte_offset = start_byte_offset;
        report->section_summaries[section_index].end_byte_offset = start_byte_offset + sdata_byte_count;
        report->section_summaries[section_index].byte_count = sdata_byte_count;
        report->section_summaries[section_index].symbol_start_index = sdata_symbol_start_index;
        report->section_summaries[section_index].symbol_count = sdata_symbol_count;
        report->section_summaries[section_index].fixup_start_index = report->total_fixup_summary_count;
        report->section_summaries[section_index].fixup_count = 0u;
        section_index += 1u;
    }

    report->total_section_summary_count = section_index;
    return 1;
}

static int machine_bytes_clone_block_payload(MachineBytesBlock *dest, const MachineEncodeBlock *src) {
    size_t op_index;

    if (!dest || !src) {
        return 0;
    }

    dest->emit_index = src->emit_index;
    dest->original_layout_index = src->original_layout_index;
    dest->original_block_id = src->original_block_id;
    dest->label_name = machine_bytes_strdup(src->label_name);
    dest->start_byte_offset = src->start_offset;
    dest->terminator_byte_offset = src->terminator_offset;
    dest->end_byte_offset = src->end_offset;
    if (src->label_name && !dest->label_name) {
        return 0;
    }

    dest->block = src->block;
    dest->block.label_name = machine_bytes_strdup(src->block.label_name);
    if (src->block.label_name && !dest->block.label_name) {
        machine_bytes_block_free(dest);
        return 0;
    }
    if (src->block.op_count > 0) {
        dest->block.ops = (MachineEmitOp *)calloc(src->block.op_count, sizeof(MachineEmitOp));
        if (!dest->block.ops) {
            machine_bytes_block_free(dest);
            return 0;
        }
        for (op_index = 0; op_index < src->block.op_count; ++op_index) {
            if (!machine_bytes_op_clone(&dest->block.ops[op_index], &src->block.ops[op_index])) {
                machine_bytes_block_free(dest);
                return 0;
            }
        }
    }
    return 1;
}

static int machine_bytes_clone_block_bytes(MachineBytesBlock *dest, const MachineBytesBlock *src) {
    if (!dest || !src) {
        return 0;
    }
    if (src->byte_count > 0) {
        dest->bytes = (unsigned char *)malloc(src->byte_count);
        if (!dest->bytes) {
            return 0;
        }
        memcpy(dest->bytes, src->bytes, src->byte_count);
    }
    dest->byte_count = src->byte_count;
    return 1;
}

static const MachineBytesBlock *machine_bytes_target_block(const MachineBytesFunction *function, size_t target_index) {
    if (!function || !function->blocks || target_index >= function->block_count) {
        return NULL;
    }
    return &function->blocks[target_index];
}

static void machine_bytes_report_clear_shape(MachineBytesReport *report) {
    if (!report) {
        return;
    }
    free(report->function_summaries);
    free(report->function_byte_offsets);
    free(report->function_block_summary_offsets);
    free(report->function_reference_offsets);
    free(report->function_fixup_offsets);
    free(report->block_summaries);
    free(report->reference_summaries);
    free(report->fixup_summaries);
    free(report->symbol_summaries);
    free(report->symbol_summaries_by_name);
    free(report->section_summaries);
    free(report->function_indices_with_calls);
    free(report->function_indices_with_fallthrough);
    free(report->function_indices_with_branches);
    memset(&report->target_policy_summary, 0, sizeof(report->target_policy_summary));
    report->function_summaries = NULL;
    report->function_byte_offsets = NULL;
    report->function_block_summary_offsets = NULL;
    report->function_reference_offsets = NULL;
    report->function_fixup_offsets = NULL;
    report->block_summaries = NULL;
    report->reference_summaries = NULL;
    report->fixup_summaries = NULL;
    report->symbol_summaries = NULL;
    report->symbol_summaries_by_name = NULL;
    report->section_summaries = NULL;
    report->total_block_summary_count = 0;
    report->total_program_byte_count = 0;
    report->total_reference_summary_count = 0;
    report->total_fixup_summary_count = 0;
    report->total_symbol_summary_count = 0;
    report->total_section_summary_count = 0;
    report->functions_with_calls_count = 0;
    report->functions_with_fallthrough_count = 0;
    report->functions_with_branches_count = 0;
    report->function_indices_with_calls = NULL;
    report->function_indices_with_fallthrough = NULL;
    report->function_indices_with_branches = NULL;
}

#define MACHINE_BYTES_SPLIT_AGGREGATOR
#include "machine_bytes_core.inc"
#include "machine_bytes_report.inc"
#include "machine_bytes_verify.inc"
#include "machine_bytes_lower.inc"
#include "machine_bytes_dump.inc"
