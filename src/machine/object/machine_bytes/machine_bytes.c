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
static int machine_bytes_op_has_call_payload(MachineSelectOpKind kind);
static unsigned char machine_bytes_encode_op_kind(MachineSelectOpKind kind);
static unsigned char machine_bytes_encode_terminator_kind(MachineLayoutTerminatorKind kind);
static unsigned char machine_bytes_truncate_u4(size_t value);
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
    const MachineEmitOp *op);
static size_t machine_bytes_terminator_encoded_size_for_profile(
    MachineBytesTargetProfile profile,
    const MachineEmitTerminator *terminator);
static int machine_bytes_write_block_bytes_for_profile(
    MachineBytesTargetProfile profile,
    MachineBytesBlock *dest_block);
static uint32_t machine_bytes_riscv_encode_i_type(
    uint32_t opcode,
    uint32_t rd,
    uint32_t funct3,
    uint32_t rs1,
    long long imm);
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
static uint32_t machine_bytes_riscv_map_register_id(size_t machine_register_id);
static uint32_t machine_bytes_riscv_map_operand_register(const MachineEmitOperand *operand);
static long long machine_bytes_riscv_slot_offset(const MachineEmitSlotRef *slot);
static uint32_t machine_bytes_riscv_encode_copy_word(const MachineEmitOp *op);
static uint32_t machine_bytes_riscv_encode_materialize_imm_word(const MachineEmitOp *op);
static uint32_t machine_bytes_riscv_encode_alu_word(const MachineEmitOp *op);
static uint32_t machine_bytes_riscv_encode_alu_imm_word(const MachineEmitOp *op);
static uint32_t machine_bytes_riscv_encode_cmp_word(const MachineEmitOp *op);
static uint32_t machine_bytes_riscv_encode_cmp_imm_word(const MachineEmitOp *op);
static uint32_t machine_bytes_riscv_encode_load_word(const MachineEmitOp *op);
static uint32_t machine_bytes_riscv_encode_store_word(const MachineEmitOp *op);
static uint32_t machine_bytes_riscv_encode_store_imm_materialize_word(const MachineEmitOp *op);
static uint32_t machine_bytes_riscv_encode_call_word(void);
static uint32_t machine_bytes_riscv_encode_return_word(void);
static uint32_t machine_bytes_riscv_encode_return_imm_word(const MachineEmitTerminator *terminator);
static uint32_t machine_bytes_riscv_encode_return_spill_load_word(const MachineEmitTerminator *terminator);
static uint32_t machine_bytes_riscv_encode_jump_word(void);
static uint32_t machine_bytes_riscv_encode_branch_word(const MachineEmitTerminator *terminator);
static uint32_t machine_bytes_riscv_encode_compare_branch_word(const MachineEmitTerminator *terminator);
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
static int machine_bytes_verify_target_index(size_t target_index, size_t block_count);
static int machine_bytes_op_clone(MachineEmitOp *dest, const MachineEmitOp *src);
static void machine_bytes_op_free(MachineEmitOp *op);
static void machine_bytes_block_free(MachineBytesBlock *block);
static void machine_bytes_function_free(MachineBytesFunction *function);
static void machine_bytes_register_desc_free(MachineEmitRegisterDesc *desc);
static size_t machine_bytes_function_total_byte_count(const MachineBytesFunction *function);
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
    const MachineEmitOp *op) {
    if (!op) {
        return 0;
    }
    if (profile == MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW) {
        switch (op->kind) {
            case MACHINE_SELECT_OP_STORE_LOCAL_IMM:
            case MACHINE_SELECT_OP_STORE_GLOBAL_IMM:
                return 8u;
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
    const MachineEmitTerminator *terminator) {
    if (!terminator) {
        return 0;
    }
    if (profile == MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW) {
        switch (terminator->kind) {
            case MACHINE_LAYOUT_TERM_RETURN:
            case MACHINE_LAYOUT_TERM_FALLTHROUGH:
            case MACHINE_LAYOUT_TERM_JUMP:
                return 4u;
            case MACHINE_LAYOUT_TERM_RETURN_IMM:
            case MACHINE_LAYOUT_TERM_RETURN_SPILL:
            case MACHINE_LAYOUT_TERM_BRANCH:
            case MACHINE_LAYOUT_TERM_BRANCH_FALLTHROUGH:
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH:
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM:
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_FALLTHROUGH:
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM_FALLTHROUGH:
                return 8u;
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

static long long machine_bytes_riscv_slot_offset(const MachineEmitSlotRef *slot) {
    if (!slot) {
        return 0;
    }
    return (long long)(slot->id * 4u);
}

static uint32_t machine_bytes_riscv_encode_copy_word(const MachineEmitOp *op) {
    return machine_bytes_riscv_encode_i_type(
        0x13u,
        machine_bytes_riscv_map_operand_register(&op->result),
        0x0u,
        machine_bytes_riscv_map_operand_register(&op->as.copy_value),
        0);
}

static uint32_t machine_bytes_riscv_encode_materialize_imm_word(const MachineEmitOp *op) {
    return machine_bytes_riscv_encode_i_type(
        0x13u,
        machine_bytes_riscv_map_operand_register(&op->result),
        0x0u,
        0u,
        op->as.copy_value.immediate);
}

static uint32_t machine_bytes_riscv_encode_alu_word(const MachineEmitOp *op) {
    uint32_t rd = machine_bytes_riscv_map_operand_register(&op->result);
    uint32_t rs1 = machine_bytes_riscv_map_operand_register(&op->as.binary.lhs);
    uint32_t rs2 = machine_bytes_riscv_map_operand_register(&op->as.binary.rhs);

    switch (op->as.binary.op) {
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
        case MACHINE_IR_BINARY_LT:
            return machine_bytes_riscv_encode_r_type(0x33u, rd, 0x2u, rs1, rs2, 0x00u);
        case MACHINE_IR_BINARY_GT:
            return machine_bytes_riscv_encode_r_type(0x33u, rd, 0x2u, rs2, rs1, 0x00u);
        case MACHINE_IR_BINARY_GE:
            return machine_bytes_riscv_encode_r_type(0x33u, rd, 0x2u, rs1, rs2, 0x00u);
        case MACHINE_IR_BINARY_LE:
            return machine_bytes_riscv_encode_r_type(0x33u, rd, 0x2u, rs2, rs1, 0x00u);
        default:
            return machine_bytes_riscv_encode_i_type(0x13u, rd, 0x0u, 0u, 0);
    }
}

static uint32_t machine_bytes_riscv_encode_alu_imm_word(const MachineEmitOp *op) {
    uint32_t rd = machine_bytes_riscv_map_operand_register(&op->result);
    uint32_t rs1 = machine_bytes_riscv_map_operand_register(&op->as.binary.lhs);

    switch (op->as.binary.op) {
        case MACHINE_IR_BINARY_ADD:
            return machine_bytes_riscv_encode_i_type(0x13u, rd, 0x0u, rs1, op->as.binary.rhs.immediate);
        case MACHINE_IR_BINARY_BIT_AND:
            return machine_bytes_riscv_encode_i_type(0x13u, rd, 0x7u, rs1, op->as.binary.rhs.immediate);
        case MACHINE_IR_BINARY_BIT_XOR:
            return machine_bytes_riscv_encode_i_type(0x13u, rd, 0x4u, rs1, op->as.binary.rhs.immediate);
        case MACHINE_IR_BINARY_BIT_OR:
            return machine_bytes_riscv_encode_i_type(0x13u, rd, 0x6u, rs1, op->as.binary.rhs.immediate);
        case MACHINE_IR_BINARY_SHIFT_LEFT:
            return machine_bytes_riscv_encode_i_type(0x13u, rd, 0x1u, rs1, op->as.binary.rhs.immediate);
        case MACHINE_IR_BINARY_SHIFT_RIGHT:
            return machine_bytes_riscv_encode_i_type(0x13u, rd, 0x5u, rs1, op->as.binary.rhs.immediate);
        case MACHINE_IR_BINARY_LT:
            return machine_bytes_riscv_encode_i_type(0x13u, rd, 0x2u, rs1, op->as.binary.rhs.immediate);
        default:
            return machine_bytes_riscv_encode_i_type(0x13u, rd, 0x0u, 0u, 0);
    }
}

static uint32_t machine_bytes_riscv_encode_cmp_word(const MachineEmitOp *op) {
    uint32_t rd = machine_bytes_riscv_map_operand_register(&op->result);
    uint32_t rs1 = machine_bytes_riscv_map_operand_register(&op->as.binary.lhs);
    uint32_t rs2 = machine_bytes_riscv_map_operand_register(&op->as.binary.rhs);

    switch (op->as.binary.op) {
        case MACHINE_IR_BINARY_EQ:
            return machine_bytes_riscv_encode_r_type(0x33u, rd, 0x4u, rs1, rs2, 0x00u);
        case MACHINE_IR_BINARY_NE:
            return machine_bytes_riscv_encode_r_type(0x33u, rd, 0x6u, rs1, rs2, 0x00u);
        case MACHINE_IR_BINARY_LT:
            return machine_bytes_riscv_encode_r_type(0x33u, rd, 0x2u, rs1, rs2, 0x00u);
        case MACHINE_IR_BINARY_GT:
            return machine_bytes_riscv_encode_r_type(0x33u, rd, 0x2u, rs2, rs1, 0x00u);
        default:
            return machine_bytes_riscv_encode_i_type(0x13u, rd, 0x0u, 0u, 0);
    }
}

static uint32_t machine_bytes_riscv_encode_cmp_imm_word(const MachineEmitOp *op) {
    uint32_t rd = machine_bytes_riscv_map_operand_register(&op->result);
    uint32_t rs1 = machine_bytes_riscv_map_operand_register(&op->as.binary.lhs);

    if (op->as.binary.op == MACHINE_IR_BINARY_EQ && op->as.binary.rhs.immediate == 0) {
        return machine_bytes_riscv_encode_i_type(0x13u, rd, 0x3u, rs1, 1);
    }
    if (op->as.binary.op == MACHINE_IR_BINARY_NE && op->as.binary.rhs.immediate == 0) {
        return machine_bytes_riscv_encode_r_type(0x33u, rd, 0x2u, 0u, rs1, 0x00u);
    }
    if (op->as.binary.op == MACHINE_IR_BINARY_LT) {
        return machine_bytes_riscv_encode_i_type(0x13u, rd, 0x2u, rs1, op->as.binary.rhs.immediate);
    }
    return machine_bytes_riscv_encode_i_type(0x13u, rd, 0x0u, 0u, 0);
}

static uint32_t machine_bytes_riscv_encode_load_word(const MachineEmitOp *op) {
    uint32_t rd = machine_bytes_riscv_map_operand_register(&op->result);
    uint32_t base = op->kind == MACHINE_SELECT_OP_LOAD_GLOBAL ? 3u : 2u;

    return machine_bytes_riscv_encode_i_type(
        0x03u,
        rd,
        0x2u,
        base,
        machine_bytes_riscv_slot_offset(&op->as.load_slot));
}

static uint32_t machine_bytes_riscv_encode_store_word(const MachineEmitOp *op) {
    uint32_t base = op->kind == MACHINE_SELECT_OP_STORE_GLOBAL ? 3u : 2u;

    return machine_bytes_riscv_encode_s_type(
        0x23u,
        0x2u,
        base,
        machine_bytes_riscv_map_operand_register(&op->as.store.value),
        machine_bytes_riscv_slot_offset(&op->as.store.slot));
}

static uint32_t machine_bytes_riscv_encode_store_imm_materialize_word(const MachineEmitOp *op) {
    return machine_bytes_riscv_encode_i_type(0x13u, 31u, 0x0u, 0u, op->as.store.value.immediate);
}

static uint32_t machine_bytes_riscv_encode_call_word(void) {
    return machine_bytes_riscv_encode_j_type(0x6Fu, 1u, 0);
}

static uint32_t machine_bytes_riscv_encode_return_word(void) {
    return machine_bytes_riscv_encode_i_type(0x67u, 0u, 0x0u, 1u, 0);
}

static uint32_t machine_bytes_riscv_encode_return_imm_word(const MachineEmitTerminator *terminator) {
    return machine_bytes_riscv_encode_i_type(0x13u, 10u, 0x0u, 0u, terminator->as.return_value.immediate);
}

static uint32_t machine_bytes_riscv_encode_return_spill_load_word(const MachineEmitTerminator *terminator) {
    MachineEmitSlotRef slot;

    slot.kind = MACHINE_SELECT_SLOT_LOCAL;
    slot.id = terminator->as.return_value.spill_slot;
    return machine_bytes_riscv_encode_i_type(0x03u, 10u, 0x2u, 2u, machine_bytes_riscv_slot_offset(&slot));
}

static uint32_t machine_bytes_riscv_encode_jump_word(void) {
    return machine_bytes_riscv_encode_j_type(0x6Fu, 0u, 0);
}

static uint32_t machine_bytes_riscv_encode_branch_word(const MachineEmitTerminator *terminator) {
    uint32_t rs1 = machine_bytes_riscv_map_operand_register(&terminator->as.branch.condition);

    return machine_bytes_riscv_encode_b_type(0x63u, 0x1u, rs1, 0u, 0);
}

static uint32_t machine_bytes_riscv_encode_compare_branch_word(const MachineEmitTerminator *terminator) {
    const MachineEmitOperand *lhs = NULL;
    const MachineEmitOperand *rhs = NULL;
    MachineIrBinaryOp op = MACHINE_IR_BINARY_EQ;
    uint32_t rs1;
    uint32_t rs2;
    uint32_t funct3 = 0x0u;

    switch (terminator->kind) {
        case MACHINE_LAYOUT_TERM_COMPARE_BRANCH:
        case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM:
            lhs = &terminator->as.compare_branch.lhs;
            rhs = &terminator->as.compare_branch.rhs;
            op = terminator->as.compare_branch.op;
            break;
        case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_FALLTHROUGH:
        case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM_FALLTHROUGH:
            lhs = &terminator->as.compare_branch_fallthrough.lhs;
            rhs = &terminator->as.compare_branch_fallthrough.rhs;
            op = terminator->as.compare_branch_fallthrough.op;
            break;
        default:
            break;
    }

    rs1 = machine_bytes_riscv_map_operand_register(lhs);
    rs2 = rhs && rhs->kind == MACHINE_SELECT_OPERAND_IMMEDIATE && rhs->immediate == 0
        ? 0u
        : machine_bytes_riscv_map_operand_register(rhs);
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
            rs1 = machine_bytes_riscv_map_operand_register(rhs);
            rs2 = machine_bytes_riscv_map_operand_register(lhs);
            break;
        case MACHINE_IR_BINARY_LE:
            funct3 = 0x5u;
            rs1 = machine_bytes_riscv_map_operand_register(rhs);
            rs2 = machine_bytes_riscv_map_operand_register(lhs);
            break;
        default:
            funct3 = 0x0u;
            break;
    }
    return machine_bytes_riscv_encode_b_type(0x63u, funct3, rs1, rs2, 0);
}

static int machine_bytes_write_block_bytes_for_profile(
    MachineBytesTargetProfile profile,
    MachineBytesBlock *dest_block) {
    size_t byte_index = 0;
    size_t op_index;

    if (!dest_block || !dest_block->bytes || dest_block->byte_count == 0) {
        return 0;
    }

    if (profile == MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW) {
        for (op_index = 0; op_index < dest_block->block.op_count; ++op_index) {
            const MachineEmitOp *op = &dest_block->block.ops[op_index];

            if (byte_index + machine_bytes_op_encoded_size_for_profile(profile, op) > dest_block->byte_count) {
                return 0;
            }
            switch (op->kind) {
                case MACHINE_SELECT_OP_COPY:
                    machine_bytes_write_u32_le(dest_block->bytes, byte_index, machine_bytes_riscv_encode_copy_word(op));
                    byte_index += 4u;
                    break;
                case MACHINE_SELECT_OP_MATERIALIZE_IMM:
                    machine_bytes_write_u32_le(
                        dest_block->bytes, byte_index, machine_bytes_riscv_encode_materialize_imm_word(op));
                    byte_index += 4u;
                    break;
                case MACHINE_SELECT_OP_ALU:
                    machine_bytes_write_u32_le(dest_block->bytes, byte_index, machine_bytes_riscv_encode_alu_word(op));
                    byte_index += 4u;
                    break;
                case MACHINE_SELECT_OP_ALU_IMM:
                    machine_bytes_write_u32_le(dest_block->bytes, byte_index, machine_bytes_riscv_encode_alu_imm_word(op));
                    byte_index += 4u;
                    break;
                case MACHINE_SELECT_OP_CMP:
                    machine_bytes_write_u32_le(dest_block->bytes, byte_index, machine_bytes_riscv_encode_cmp_word(op));
                    byte_index += 4u;
                    break;
                case MACHINE_SELECT_OP_CMP_IMM:
                    machine_bytes_write_u32_le(dest_block->bytes, byte_index, machine_bytes_riscv_encode_cmp_imm_word(op));
                    byte_index += 4u;
                    break;
                case MACHINE_SELECT_OP_LOAD_LOCAL:
                case MACHINE_SELECT_OP_LOAD_GLOBAL:
                    machine_bytes_write_u32_le(dest_block->bytes, byte_index, machine_bytes_riscv_encode_load_word(op));
                    byte_index += 4u;
                    break;
                case MACHINE_SELECT_OP_STORE_LOCAL:
                case MACHINE_SELECT_OP_STORE_GLOBAL:
                    machine_bytes_write_u32_le(dest_block->bytes, byte_index, machine_bytes_riscv_encode_store_word(op));
                    byte_index += 4u;
                    break;
                case MACHINE_SELECT_OP_STORE_LOCAL_IMM:
                case MACHINE_SELECT_OP_STORE_GLOBAL_IMM:
                    machine_bytes_write_u32_le(
                        dest_block->bytes, byte_index, machine_bytes_riscv_encode_store_imm_materialize_word(op));
                    byte_index += 4u;
                    machine_bytes_write_u32_le(
                        dest_block->bytes,
                        byte_index,
                        machine_bytes_riscv_encode_s_type(
                            0x23u,
                            0x2u,
                            op->kind == MACHINE_SELECT_OP_STORE_GLOBAL_IMM ? 3u : 2u,
                            31u,
                            machine_bytes_riscv_slot_offset(&op->as.store.slot)));
                    byte_index += 4u;
                    break;
                case MACHINE_SELECT_OP_CALL:
                case MACHINE_SELECT_OP_CALL_IMM:
                case MACHINE_SELECT_OP_CALL_SPILL:
                case MACHINE_SELECT_OP_CALL_IMM_SPILL:
                case MACHINE_SELECT_OP_CALL_VOID:
                case MACHINE_SELECT_OP_CALL_VOID_IMM:
                    machine_bytes_write_u32_le(dest_block->bytes, byte_index, machine_bytes_riscv_encode_call_word());
                    byte_index += 4u;
                    break;
                default:
                    machine_bytes_write_u32_le(
                        dest_block->bytes, byte_index, machine_bytes_riscv_encode_i_type(0x13u, 0u, 0x0u, 0u, 0));
                    byte_index += 4u;
                    break;
            }
        }

        switch (dest_block->block.terminator.kind) {
            case MACHINE_LAYOUT_TERM_RETURN:
                machine_bytes_write_u32_le(dest_block->bytes, byte_index, machine_bytes_riscv_encode_return_word());
                byte_index += 4u;
                break;
            case MACHINE_LAYOUT_TERM_RETURN_IMM:
                machine_bytes_write_u32_le(
                    dest_block->bytes, byte_index, machine_bytes_riscv_encode_return_imm_word(&dest_block->block.terminator));
                byte_index += 4u;
                machine_bytes_write_u32_le(dest_block->bytes, byte_index, machine_bytes_riscv_encode_return_word());
                byte_index += 4u;
                break;
            case MACHINE_LAYOUT_TERM_RETURN_SPILL:
                machine_bytes_write_u32_le(
                    dest_block->bytes,
                    byte_index,
                    machine_bytes_riscv_encode_return_spill_load_word(&dest_block->block.terminator));
                byte_index += 4u;
                machine_bytes_write_u32_le(dest_block->bytes, byte_index, machine_bytes_riscv_encode_return_word());
                byte_index += 4u;
                break;
            case MACHINE_LAYOUT_TERM_FALLTHROUGH:
            case MACHINE_LAYOUT_TERM_JUMP:
                machine_bytes_write_u32_le(dest_block->bytes, byte_index, machine_bytes_riscv_encode_jump_word());
                byte_index += 4u;
                break;
            case MACHINE_LAYOUT_TERM_BRANCH:
            case MACHINE_LAYOUT_TERM_BRANCH_FALLTHROUGH:
                machine_bytes_write_u32_le(dest_block->bytes, byte_index, machine_bytes_riscv_encode_branch_word(&dest_block->block.terminator));
                byte_index += 4u;
                machine_bytes_write_u32_le(dest_block->bytes, byte_index, machine_bytes_riscv_encode_jump_word());
                byte_index += 4u;
                break;
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH:
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM:
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_FALLTHROUGH:
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM_FALLTHROUGH:
                machine_bytes_write_u32_le(
                    dest_block->bytes, byte_index, machine_bytes_riscv_encode_compare_branch_word(&dest_block->block.terminator));
                byte_index += 4u;
                machine_bytes_write_u32_le(dest_block->bytes, byte_index, machine_bytes_riscv_encode_jump_word());
                byte_index += 4u;
                break;
            default:
                return 0;
        }
        return byte_index == dest_block->byte_count;
    }

    for (op_index = 0; op_index < dest_block->block.op_count; ++op_index) {
        const MachineEmitOp *op = &dest_block->block.ops[op_index];
        size_t op_size = machine_bytes_op_encoded_size_for_profile(profile, op);

        if (byte_index + op_size > dest_block->byte_count) {
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
        size_t term_size = machine_bytes_terminator_encoded_size_for_profile(profile, terminator);

        if (byte_index + term_size > dest_block->byte_count) {
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
                return 0;
        }
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
        *out_patch_byte_offset = owner_byte_offset;
        *out_patch_byte_count = owner_byte_count >= 4u ? 4u : owner_byte_count;
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
            case MACHINE_LAYOUT_TERM_FALLTHROUGH:
                *out_patch_byte_offset = owner_byte_offset;
                *out_patch_byte_count = owner_byte_count >= 4u ? 4u : owner_byte_count;
                return 1;
            case MACHINE_LAYOUT_TERM_BRANCH:
            case MACHINE_LAYOUT_TERM_BRANCH_FALLTHROUGH:
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH:
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM:
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_FALLTHROUGH:
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM_FALLTHROUGH:
                *out_patch_byte_offset = owner_byte_offset +
                    (kind == MACHINE_BYTES_REFERENCE_CONTROL_SECONDARY ? 4u : 0u);
                *out_patch_byte_count = owner_byte_count >= 4u ? 4u : owner_byte_count;
                return 1;
            default:
                break;
        }
    }
    *out_patch_byte_offset = owner_byte_offset + (owner_byte_count > 1 ? owner_byte_count - 1 : 0);
    *out_patch_byte_count = owner_byte_count > 1 ? 1u : 0u;
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
            if (machine_bytes_op_has_call_payload(block->block.ops[op_index].kind)) {
                ++reference_count;
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
    if (!machine_bytes_reference_patch_for_call(
            profile,
            owner_byte_offset,
            owner_byte_count,
            &summary->patch_byte_offset,
            &summary->patch_byte_count)) {
        return 0;
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

static int machine_bytes_report_populate_function_references(
    const MachineBytesReport *report,
    size_t function_index,
    size_t start_reference_index) {
    const MachineBytesFunction *function;
    size_t reference_index = start_reference_index;
    size_t block_index;

    if (!report || function_index >= report->program.function_count || !report->reference_summaries) {
        return 0;
    }
    function = &report->program.functions[function_index];
    for (block_index = 0; block_index < function->block_count; ++block_index) {
        const MachineBytesBlock *block = &function->blocks[block_index];
        size_t program_byte_offset =
            (report->function_byte_offsets ? report->function_byte_offsets[function_index] : 0) + block->start_byte_offset;
        size_t op_index;

        for (op_index = 0; op_index < block->block.op_count; ++op_index) {
            const MachineEmitOp *op = &block->block.ops[op_index];
            size_t op_size = machine_bytes_op_encoded_size_for_profile(report->program.target_profile, op);

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
                        &report->program,
                        report->function_byte_offsets)) {
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
                report->program.target_profile, &block->block.terminator);
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
    size_t symbol_index;

    if (!report || !symbol_name || !report->symbol_summaries) {
        return (size_t)-1;
    }
    for (symbol_index = 0; symbol_index < report->total_symbol_summary_count; ++symbol_index) {
        if (report->symbol_summaries[symbol_index].name &&
            strcmp(report->symbol_summaries[symbol_index].name, symbol_name) == 0) {
            return symbol_index;
        }
    }
    return (size_t)-1;
}

static int machine_bytes_report_populate_symbols(MachineBytesReport *report) {
    size_t symbol_index = 0;
    size_t function_index;
    size_t reference_index;

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
            MachineBytesFixupSummary *fixup = &report->fixup_summaries[fixup_index++];

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

    return fixup_index == report->total_fixup_summary_count;
}

static int machine_bytes_report_populate_sections(MachineBytesReport *report) {
    size_t symbol_index;
    size_t defined_symbol_count = 0;

    if (!report) {
        return 0;
    }
    if (report->program.function_count == 0) {
        report->total_section_summary_count = 0;
        return 1;
    }
    if (!report->section_summaries) {
        return 0;
    }

    for (symbol_index = 0; symbol_index < report->total_symbol_summary_count; ++symbol_index) {
        const MachineBytesSymbolSummary *symbol = &report->symbol_summaries[symbol_index];

        if (symbol->is_defined) {
            ++defined_symbol_count;
        }
    }

    memset(&report->section_summaries[0], 0, sizeof(report->section_summaries[0]));
    report->section_summaries[0].kind = MACHINE_BYTES_SECTION_TEXT;
    report->section_summaries[0].name = ".text";
    report->section_summaries[0].start_byte_offset = 0;
    report->section_summaries[0].end_byte_offset = report->total_program_byte_count;
    report->section_summaries[0].byte_count = report->total_program_byte_count;
    report->section_summaries[0].function_count = report->program.function_count;
    report->section_summaries[0].block_count = report->total_block_summary_count;
    report->section_summaries[0].symbol_start_index = 0;
    report->section_summaries[0].symbol_count = defined_symbol_count;
    report->section_summaries[0].fixup_start_index = 0;
    report->section_summaries[0].fixup_count = report->total_fixup_summary_count;
    report->total_section_summary_count = 1;
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
    free(report->section_summaries);
    free(report->function_indices_with_calls);
    free(report->function_indices_with_fallthrough);
    free(report->function_indices_with_branches);
    report->function_summaries = NULL;
    report->function_byte_offsets = NULL;
    report->function_block_summary_offsets = NULL;
    report->function_reference_offsets = NULL;
    report->function_fixup_offsets = NULL;
    report->block_summaries = NULL;
    report->reference_summaries = NULL;
    report->fixup_summaries = NULL;
    report->symbol_summaries = NULL;
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
