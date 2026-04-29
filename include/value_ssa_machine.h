#ifndef VALUE_SSA_MACHINE_H
#define VALUE_SSA_MACHINE_H

#include "value_ssa_alloc.h"

typedef enum {
    VALUE_SSA_MACHINE_REGISTER_CLASS_GENERAL = 0,
} ValueSsaMachineRegisterClass;

typedef struct {
    size_t register_id;
    const char *name;
    ValueSsaMachineRegisterClass register_class;
    unsigned char allocatable;
    unsigned char caller_clobbered;
    unsigned char callee_preserved;
} ValueSsaMachineRegisterDesc;

typedef struct {
    size_t register_count;
    ValueSsaMachineRegisterDesc *registers;
} ValueSsaMachineRegisterBank;

typedef struct {
    size_t value_id;
    ValueSsaAllocatedValuePlacementKind kind;
    size_t machine_register_id;
    const char *machine_register_name;
    size_t spill_slot;
    unsigned char spill_intended;
    unsigned char spill_confirmed;
    unsigned char spill_recovered;
} ValueSsaMachineAllocatedValuePlacement;

typedef struct {
    char *function_name;
    size_t value_count;
    size_t machine_register_count;
    size_t spill_slot_count;
    size_t machine_colored_count;
    size_t spilled_count;
    ValueSsaMachineAllocatedValuePlacement *placements;
} ValueSsaFunctionMachineAllocationView;

typedef struct {
    size_t function_count;
    ValueSsaFunctionMachineAllocationView *function_views;
    size_t total_value_count;
    size_t total_machine_colored_count;
    size_t total_spilled_count;
} ValueSsaProgramMachineAllocationView;

void value_ssa_machine_register_bank_init(ValueSsaMachineRegisterBank *bank);
void value_ssa_machine_register_bank_free(ValueSsaMachineRegisterBank *bank);
void value_ssa_function_machine_allocation_view_init(ValueSsaFunctionMachineAllocationView *view);
void value_ssa_function_machine_allocation_view_free(ValueSsaFunctionMachineAllocationView *view);
void value_ssa_program_machine_allocation_view_init(ValueSsaProgramMachineAllocationView *view);
void value_ssa_program_machine_allocation_view_free(ValueSsaProgramMachineAllocationView *view);

int value_ssa_build_flat_machine_register_bank(size_t register_count,
    ValueSsaMachineRegisterBank *out_bank,
    ValueSsaError *error);
int value_ssa_build_function_machine_allocation_view(const ValueSsaFunctionAllocationLayout *layout,
    const ValueSsaMachineRegisterBank *bank,
    ValueSsaFunctionMachineAllocationView *out_view,
    ValueSsaError *error);
int value_ssa_build_program_machine_allocation_view(const ValueSsaProgramAllocationLayout *layout,
    const ValueSsaMachineRegisterBank *bank,
    ValueSsaProgramMachineAllocationView *out_view,
    ValueSsaError *error);

int value_ssa_dump_machine_register_bank(const ValueSsaMachineRegisterBank *bank,
    char **out_text,
    ValueSsaError *error);
int value_ssa_dump_function_machine_allocation_view(const ValueSsaFunctionMachineAllocationView *view,
    char **out_text,
    ValueSsaError *error);
int value_ssa_dump_program_machine_allocation_view(const ValueSsaProgramMachineAllocationView *view,
    char **out_text,
    ValueSsaError *error);

#endif
