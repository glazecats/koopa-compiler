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
    size_t machine_register_id;
    const char *machine_register_name;
    size_t value_count;
    size_t *value_ids;
} ValueSsaMachineRegisterValueGroup;

typedef struct {
    char *function_name;
    size_t value_count;
    size_t machine_register_count;
    size_t spill_slot_count;
    size_t machine_colored_count;
    size_t spilled_count;
    size_t caller_clobbered_value_count;
    size_t callee_preserved_value_count;
    size_t used_machine_register_count;
    size_t machine_register_group_count;
    ValueSsaMachineAllocatedValuePlacement *placements;
    size_t *machine_colored_value_ids;
    size_t *spilled_value_ids;
    size_t *caller_clobbered_value_ids;
    size_t *callee_preserved_value_ids;
    size_t *used_machine_register_ids;
    ValueSsaMachineRegisterValueGroup *machine_register_groups;
} ValueSsaFunctionMachineAllocationView;

typedef struct {
    size_t function_count;
    ValueSsaFunctionMachineAllocationView *function_views;
    size_t total_value_count;
    size_t total_machine_colored_count;
    size_t total_spilled_count;
    size_t total_caller_clobbered_value_count;
    size_t total_callee_preserved_value_count;
    size_t functions_with_machine_colors_count;
    size_t functions_with_spills_count;
    size_t functions_with_caller_clobbered_values_count;
    size_t functions_with_callee_preserved_values_count;
    size_t *function_indices_with_machine_colors;
    size_t *function_indices_with_spills;
    size_t *function_indices_with_caller_clobbered_values;
    size_t *function_indices_with_callee_preserved_values;
} ValueSsaProgramMachineAllocationView;

typedef struct {
    ValueSsaAllocateRewriteStats stats;
    ValueSsaProgramMachineAllocationView view;
} ValueSsaAllocateRewriteMachineReport;

typedef enum {
    VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_NONE = 0,
    VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_CROSSING = 1,
    VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_REPEATED = 2,
    VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_HOT = 3,
    VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_COUNT
} ValueSsaMachineCallClobberRiskClass;

typedef struct {
    size_t value_id;
    size_t machine_register_id;
    const char *machine_register_name;
    size_t call_crossing_count;
    size_t call_density_sum;
    size_t use_call_density_sum;
    ValueSsaMachineCallClobberRiskClass risk_class;
} ValueSsaMachineCallClobberRiskItem;

typedef struct {
    size_t value_count;
    size_t item_count;
    size_t risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_COUNT];
    ValueSsaMachineCallClobberRiskItem *items;
} ValueSsaMachineCallClobberRiskReport;

typedef struct {
    char *function_name;
    ValueSsaMachineCallClobberRiskReport risk_report;
} ValueSsaFunctionMachineCallClobberRiskView;

typedef struct {
    size_t function_count;
    ValueSsaFunctionMachineCallClobberRiskView *function_views;
    size_t total_value_count;
    size_t total_risky_value_count;
    size_t total_risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_COUNT];
    size_t functions_with_risky_values_count;
    size_t *function_indices_with_risky_values;
    size_t functions_with_risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_COUNT];
    size_t *function_indices_with_risk_class[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_COUNT];
} ValueSsaProgramMachineCallClobberRiskReport;

typedef struct {
    size_t value_id;
    size_t machine_register_id;
    const char *machine_register_name;
    ValueSsaMachineCallClobberRiskClass risk_class;
    size_t protection_priority;
    size_t call_crossing_count;
    size_t call_density_sum;
    size_t use_call_density_sum;
    size_t loop_depth_sum;
    size_t use_loop_depth_sum;
    unsigned char single_block_live_range;
    unsigned char rematerializable;
} ValueSsaMachineProtectionAgendaItem;

typedef struct {
    size_t value_count;
    size_t item_count;
    size_t risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_COUNT];
    ValueSsaMachineProtectionAgendaItem *items;
} ValueSsaMachineProtectionAgenda;

typedef struct {
    char *function_name;
    ValueSsaMachineProtectionAgenda agenda;
} ValueSsaFunctionMachineProtectionView;

typedef struct {
    size_t function_count;
    ValueSsaFunctionMachineProtectionView *function_views;
    size_t total_value_count;
    size_t total_item_count;
    size_t total_risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_COUNT];
    size_t functions_with_items_count;
    size_t *function_indices_with_items;
    size_t functions_with_risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_COUNT];
    size_t *function_indices_with_risk_class[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_COUNT];
} ValueSsaProgramMachineProtectionReport;

typedef struct {
    size_t machine_register_id;
    const char *machine_register_name;
    size_t item_count;
    size_t total_protection_priority;
    size_t max_protection_priority;
    size_t risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_COUNT];
    size_t *value_ids;
} ValueSsaMachineRegisterProtectionPressureItem;

typedef struct {
    size_t machine_register_count;
    size_t pressured_register_count;
    size_t total_value_count;
    size_t total_item_count;
    size_t total_protection_priority;
    size_t risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_COUNT];
    ValueSsaMachineRegisterProtectionPressureItem *items;
} ValueSsaMachineRegisterProtectionPressureView;

typedef struct {
    char *function_name;
    ValueSsaMachineRegisterProtectionPressureView pressure_view;
} ValueSsaFunctionMachineRegisterProtectionPressureView;

typedef struct {
    size_t function_count;
    ValueSsaFunctionMachineRegisterProtectionPressureView *function_views;
    size_t total_value_count;
    size_t total_item_count;
    size_t total_protection_priority;
    size_t total_pressured_register_count;
    size_t risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_COUNT];
    size_t functions_with_pressure_count;
    size_t *function_indices_with_pressure;
    size_t functions_with_risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_COUNT];
    size_t *function_indices_with_risk_class[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_COUNT];
} ValueSsaProgramMachineRegisterProtectionPressureReport;

typedef struct {
    size_t machine_register_id;
    const char *machine_register_name;
    size_t item_count;
    size_t total_protection_priority;
    size_t max_protection_priority;
    size_t risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_COUNT];
} ValueSsaMachineRegisterProtectionHotspotItem;

typedef struct {
    char *function_name;
    unsigned char has_hotspot;
    ValueSsaMachineRegisterProtectionHotspotItem hotspot;
} ValueSsaFunctionMachineRegisterProtectionHotspotView;

typedef struct {
    size_t function_count;
    ValueSsaFunctionMachineRegisterProtectionHotspotView *function_views;
    size_t total_value_count;
    size_t total_item_count;
    size_t total_hotspot_priority;
    size_t total_hotspot_item_count;
    size_t functions_with_hotspots_count;
    size_t *function_indices_with_hotspots;
} ValueSsaProgramMachineRegisterProtectionHotspotReport;

typedef struct {
    size_t machine_register_id;
    const char *machine_register_name;
    size_t suggested_machine_register_id;
    const char *suggested_machine_register_name;
    size_t suggested_register_item_count;
    size_t suggested_register_total_priority;
    size_t suggested_register_max_priority;
    const char *primary_reason;
    size_t protected_item_count;
    size_t protected_total_priority;
    size_t protected_max_priority;
} ValueSsaMachinePreservationHintCandidate;

typedef enum {
    VALUE_SSA_MACHINE_PRESERVATION_REASON_UNKNOWN = 0,
    VALUE_SSA_MACHINE_PRESERVATION_REASON_TARGET_EMPTY,
    VALUE_SSA_MACHINE_PRESERVATION_REASON_TARGET_LESS_OCCUPIED,
    VALUE_SSA_MACHINE_PRESERVATION_REASON_TARGET_LOWER_PROTECTION_PRESSURE,
    VALUE_SSA_MACHINE_PRESERVATION_REASON_COUNT
} ValueSsaMachinePreservationReason;

typedef struct {
    char *function_name;
    unsigned char has_hint;
    size_t source_machine_register_id;
    const char *source_machine_register_name;
    size_t suggested_machine_register_id;
    const char *suggested_machine_register_name;
    size_t protected_item_count;
    size_t protected_total_priority;
    size_t protected_max_priority;
    size_t source_risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_COUNT];
    ValueSsaMachinePreservationReason primary_reason;
    size_t candidate_count;
    ValueSsaMachinePreservationHintCandidate *candidates;
} ValueSsaFunctionMachinePreservationHintView;

typedef struct {
    size_t function_count;
    ValueSsaFunctionMachinePreservationHintView *function_views;
    size_t total_value_count;
    size_t total_item_count;
    size_t total_hint_priority;
    size_t functions_with_hints_count;
    size_t *function_indices_with_hints;
    size_t reason_counts[VALUE_SSA_MACHINE_PRESERVATION_REASON_COUNT];
    size_t functions_with_reason_counts[VALUE_SSA_MACHINE_PRESERVATION_REASON_COUNT];
    size_t *function_indices_with_reason[VALUE_SSA_MACHINE_PRESERVATION_REASON_COUNT];
} ValueSsaProgramMachinePreservationHintReport;

typedef struct {
    char *function_name;
    ValueSsaMachineRegisterProtectionPressureView pressure_view;
    ValueSsaFunctionMachineRegisterProtectionHotspotView hotspot_view;
    ValueSsaFunctionMachinePreservationHintView preservation_hint_view;
} ValueSsaFunctionMachinePlanningView;

typedef struct {
    size_t function_count;
    ValueSsaFunctionMachinePlanningView *function_views;
    size_t total_value_count;
    size_t total_pressure_item_count;
    size_t total_pressure_priority;
    size_t total_hotspot_priority;
    size_t total_hint_priority;
    size_t functions_with_pressure_count;
    size_t *function_indices_with_pressure;
    size_t pressure_reason_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_COUNT];
    size_t functions_with_pressure_reason_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_COUNT];
    size_t *function_indices_with_pressure_reason[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_COUNT];
    size_t functions_with_hotspots_count;
    size_t *function_indices_with_hotspots;
    size_t functions_with_hints_count;
    size_t *function_indices_with_hints;
    size_t hint_reason_counts[VALUE_SSA_MACHINE_PRESERVATION_REASON_COUNT];
    size_t functions_with_hint_reason_counts[VALUE_SSA_MACHINE_PRESERVATION_REASON_COUNT];
    size_t *function_indices_with_hint_reason[VALUE_SSA_MACHINE_PRESERVATION_REASON_COUNT];
} ValueSsaProgramMachinePlanningReport;

void value_ssa_machine_register_bank_init(ValueSsaMachineRegisterBank *bank);
void value_ssa_machine_register_bank_free(ValueSsaMachineRegisterBank *bank);
void value_ssa_function_machine_allocation_view_init(ValueSsaFunctionMachineAllocationView *view);
void value_ssa_function_machine_allocation_view_free(ValueSsaFunctionMachineAllocationView *view);
void value_ssa_program_machine_allocation_view_init(ValueSsaProgramMachineAllocationView *view);
void value_ssa_program_machine_allocation_view_free(ValueSsaProgramMachineAllocationView *view);
void value_ssa_allocate_rewrite_machine_report_init(ValueSsaAllocateRewriteMachineReport *report);
void value_ssa_allocate_rewrite_machine_report_free(ValueSsaAllocateRewriteMachineReport *report);
void value_ssa_machine_call_clobber_risk_report_init(ValueSsaMachineCallClobberRiskReport *report);
void value_ssa_machine_call_clobber_risk_report_free(ValueSsaMachineCallClobberRiskReport *report);
void value_ssa_function_machine_call_clobber_risk_view_init(ValueSsaFunctionMachineCallClobberRiskView *view);
void value_ssa_function_machine_call_clobber_risk_view_free(ValueSsaFunctionMachineCallClobberRiskView *view);
void value_ssa_program_machine_call_clobber_risk_report_init(ValueSsaProgramMachineCallClobberRiskReport *report);
void value_ssa_program_machine_call_clobber_risk_report_free(ValueSsaProgramMachineCallClobberRiskReport *report);
void value_ssa_machine_protection_agenda_init(ValueSsaMachineProtectionAgenda *agenda);
void value_ssa_machine_protection_agenda_free(ValueSsaMachineProtectionAgenda *agenda);
void value_ssa_function_machine_protection_view_init(ValueSsaFunctionMachineProtectionView *view);
void value_ssa_function_machine_protection_view_free(ValueSsaFunctionMachineProtectionView *view);
void value_ssa_program_machine_protection_report_init(ValueSsaProgramMachineProtectionReport *report);
void value_ssa_program_machine_protection_report_free(ValueSsaProgramMachineProtectionReport *report);
void value_ssa_machine_register_protection_pressure_view_init(
    ValueSsaMachineRegisterProtectionPressureView *view);
void value_ssa_machine_register_protection_pressure_view_free(
    ValueSsaMachineRegisterProtectionPressureView *view);
void value_ssa_function_machine_register_protection_pressure_view_init(
    ValueSsaFunctionMachineRegisterProtectionPressureView *view);
void value_ssa_function_machine_register_protection_pressure_view_free(
    ValueSsaFunctionMachineRegisterProtectionPressureView *view);
void value_ssa_program_machine_register_protection_pressure_report_init(
    ValueSsaProgramMachineRegisterProtectionPressureReport *report);
void value_ssa_program_machine_register_protection_pressure_report_free(
    ValueSsaProgramMachineRegisterProtectionPressureReport *report);
void value_ssa_function_machine_register_protection_hotspot_view_init(
    ValueSsaFunctionMachineRegisterProtectionHotspotView *view);
void value_ssa_function_machine_register_protection_hotspot_view_free(
    ValueSsaFunctionMachineRegisterProtectionHotspotView *view);
void value_ssa_program_machine_register_protection_hotspot_report_init(
    ValueSsaProgramMachineRegisterProtectionHotspotReport *report);
void value_ssa_program_machine_register_protection_hotspot_report_free(
    ValueSsaProgramMachineRegisterProtectionHotspotReport *report);
void value_ssa_function_machine_preservation_hint_view_init(
    ValueSsaFunctionMachinePreservationHintView *view);
void value_ssa_function_machine_preservation_hint_view_free(
    ValueSsaFunctionMachinePreservationHintView *view);
void value_ssa_program_machine_preservation_hint_report_init(
    ValueSsaProgramMachinePreservationHintReport *report);
void value_ssa_program_machine_preservation_hint_report_free(
    ValueSsaProgramMachinePreservationHintReport *report);
void value_ssa_function_machine_planning_view_init(
    ValueSsaFunctionMachinePlanningView *view);
void value_ssa_function_machine_planning_view_free(
    ValueSsaFunctionMachinePlanningView *view);
void value_ssa_program_machine_planning_report_init(
    ValueSsaProgramMachinePlanningReport *report);
void value_ssa_program_machine_planning_report_free(
    ValueSsaProgramMachinePlanningReport *report);
const char *value_ssa_machine_preservation_reason_name(
    ValueSsaMachinePreservationReason reason);

int value_ssa_build_flat_machine_register_bank(size_t register_count,
    ValueSsaMachineRegisterBank *out_bank,
    ValueSsaError *error);
int value_ssa_build_split_machine_register_bank(size_t caller_clobbered_count,
    size_t callee_preserved_count,
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
int value_ssa_build_program_machine_allocation_view_from_report(
    const ValueSsaAllocateRewriteLayoutReport *report,
    const ValueSsaMachineRegisterBank *bank,
    ValueSsaProgramMachineAllocationView *out_view,
    ValueSsaError *error);
int value_ssa_allocate_function_machine_view(const ValueSsaFunction *function,
    size_t color_budget,
    const ValueSsaMachineRegisterBank *bank,
    ValueSsaFunctionMachineAllocationView *out_view,
    ValueSsaError *error);
int value_ssa_allocate_program_machine_view(const ValueSsaProgram *program,
    size_t color_budget,
    const ValueSsaMachineRegisterBank *bank,
    ValueSsaProgramMachineAllocationView *out_view,
    ValueSsaError *error);
int value_ssa_build_allocate_rewrite_machine_report(
    const ValueSsaAllocateRewriteLayoutReport *report,
    const ValueSsaMachineRegisterBank *bank,
    ValueSsaAllocateRewriteMachineReport *out_report,
    ValueSsaError *error);
int value_ssa_build_machine_call_clobber_risk_report(
    const ValueSsaFunctionMachineAllocationView *machine_view,
    const ValueSsaAllocationPrep *allocation_prep,
    ValueSsaMachineCallClobberRiskReport *out_report,
    ValueSsaError *error);
int value_ssa_compute_function_machine_call_clobber_risk(
    const ValueSsaFunction *function,
    size_t color_budget,
    const ValueSsaMachineRegisterBank *bank,
    const ValueSsaAllocationPrep *allocation_prep,
    ValueSsaMachineCallClobberRiskReport *out_report,
    ValueSsaError *error);
int value_ssa_compute_function_flat_machine_call_clobber_risk(
    const ValueSsaFunction *function,
    size_t color_budget,
    size_t machine_register_count,
    const ValueSsaAllocationPrep *allocation_prep,
    ValueSsaMachineCallClobberRiskReport *out_report,
    ValueSsaError *error);
int value_ssa_compute_program_machine_call_clobber_risk(
    const ValueSsaProgram *program,
    size_t color_budget,
    const ValueSsaMachineRegisterBank *bank,
    ValueSsaProgramMachineCallClobberRiskReport *out_report,
    ValueSsaError *error);
int value_ssa_compute_program_flat_machine_call_clobber_risk(
    const ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    ValueSsaProgramMachineCallClobberRiskReport *out_report,
    ValueSsaError *error);
int value_ssa_build_machine_protection_agenda(
    const ValueSsaFunctionMachineAllocationView *machine_view,
    const ValueSsaAllocationPrep *allocation_prep,
    ValueSsaMachineProtectionAgenda *out_agenda,
    ValueSsaError *error);
int value_ssa_compute_function_machine_protection_agenda(
    const ValueSsaFunction *function,
    size_t color_budget,
    const ValueSsaMachineRegisterBank *bank,
    const ValueSsaAllocationPrep *allocation_prep,
    ValueSsaMachineProtectionAgenda *out_agenda,
    ValueSsaError *error);
int value_ssa_compute_function_flat_machine_protection_agenda(
    const ValueSsaFunction *function,
    size_t color_budget,
    size_t machine_register_count,
    const ValueSsaAllocationPrep *allocation_prep,
    ValueSsaMachineProtectionAgenda *out_agenda,
    ValueSsaError *error);
int value_ssa_compute_program_machine_protection_report(
    const ValueSsaProgram *program,
    size_t color_budget,
    const ValueSsaMachineRegisterBank *bank,
    ValueSsaProgramMachineProtectionReport *out_report,
    ValueSsaError *error);
int value_ssa_compute_program_flat_machine_protection_report(
    const ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    ValueSsaProgramMachineProtectionReport *out_report,
    ValueSsaError *error);
int value_ssa_build_machine_register_protection_pressure_view(
    const ValueSsaFunctionMachineAllocationView *machine_view,
    const ValueSsaAllocationPrep *allocation_prep,
    ValueSsaMachineRegisterProtectionPressureView *out_view,
    ValueSsaError *error);
int value_ssa_compute_function_machine_register_protection_pressure_view(
    const ValueSsaFunction *function,
    size_t color_budget,
    const ValueSsaMachineRegisterBank *bank,
    const ValueSsaAllocationPrep *allocation_prep,
    ValueSsaMachineRegisterProtectionPressureView *out_view,
    ValueSsaError *error);
int value_ssa_compute_function_flat_machine_register_protection_pressure_view(
    const ValueSsaFunction *function,
    size_t color_budget,
    size_t machine_register_count,
    const ValueSsaAllocationPrep *allocation_prep,
    ValueSsaMachineRegisterProtectionPressureView *out_view,
    ValueSsaError *error);
int value_ssa_compute_program_machine_register_protection_pressure_report(
    const ValueSsaProgram *program,
    size_t color_budget,
    const ValueSsaMachineRegisterBank *bank,
    ValueSsaProgramMachineRegisterProtectionPressureReport *out_report,
    ValueSsaError *error);
int value_ssa_compute_program_flat_machine_register_protection_pressure_report(
    const ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    ValueSsaProgramMachineRegisterProtectionPressureReport *out_report,
    ValueSsaError *error);
int value_ssa_build_function_machine_register_protection_hotspot_view(
    const char *function_name,
    const ValueSsaMachineRegisterProtectionPressureView *pressure_view,
    ValueSsaFunctionMachineRegisterProtectionHotspotView *out_view,
    ValueSsaError *error);
int value_ssa_compute_function_machine_register_protection_hotspot_view(
    const ValueSsaFunction *function,
    size_t color_budget,
    const ValueSsaMachineRegisterBank *bank,
    const ValueSsaAllocationPrep *allocation_prep,
    ValueSsaFunctionMachineRegisterProtectionHotspotView *out_view,
    ValueSsaError *error);
int value_ssa_compute_function_flat_machine_register_protection_hotspot_view(
    const ValueSsaFunction *function,
    size_t color_budget,
    size_t machine_register_count,
    const ValueSsaAllocationPrep *allocation_prep,
    ValueSsaFunctionMachineRegisterProtectionHotspotView *out_view,
    ValueSsaError *error);
int value_ssa_compute_program_machine_register_protection_hotspot_report(
    const ValueSsaProgram *program,
    size_t color_budget,
    const ValueSsaMachineRegisterBank *bank,
    ValueSsaProgramMachineRegisterProtectionHotspotReport *out_report,
    ValueSsaError *error);
int value_ssa_compute_program_flat_machine_register_protection_hotspot_report(
    const ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    ValueSsaProgramMachineRegisterProtectionHotspotReport *out_report,
    ValueSsaError *error);
int value_ssa_build_function_machine_preservation_hint_view(
    const char *function_name,
    const ValueSsaFunctionMachineAllocationView *machine_view,
    const ValueSsaMachineRegisterProtectionPressureView *pressure_view,
    const ValueSsaMachineRegisterBank *bank,
    ValueSsaFunctionMachinePreservationHintView *out_view,
    ValueSsaError *error);
int value_ssa_compute_function_machine_preservation_hint_view(
    const ValueSsaFunction *function,
    size_t color_budget,
    const ValueSsaMachineRegisterBank *bank,
    const ValueSsaAllocationPrep *allocation_prep,
    ValueSsaFunctionMachinePreservationHintView *out_view,
    ValueSsaError *error);
int value_ssa_compute_function_flat_machine_preservation_hint_view(
    const ValueSsaFunction *function,
    size_t color_budget,
    size_t machine_register_count,
    const ValueSsaAllocationPrep *allocation_prep,
    ValueSsaFunctionMachinePreservationHintView *out_view,
    ValueSsaError *error);
int value_ssa_compute_program_machine_preservation_hint_report(
    const ValueSsaProgram *program,
    size_t color_budget,
    const ValueSsaMachineRegisterBank *bank,
    ValueSsaProgramMachinePreservationHintReport *out_report,
    ValueSsaError *error);
int value_ssa_compute_program_flat_machine_preservation_hint_report(
    const ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    ValueSsaProgramMachinePreservationHintReport *out_report,
    ValueSsaError *error);
int value_ssa_build_function_machine_planning_view(
    const ValueSsaFunctionMachineAllocationView *machine_view,
    const ValueSsaAllocationPrep *allocation_prep,
    const ValueSsaMachineRegisterBank *bank,
    ValueSsaFunctionMachinePlanningView *out_view,
    ValueSsaError *error);
int value_ssa_compute_function_machine_planning_view(
    const ValueSsaFunction *function,
    size_t color_budget,
    const ValueSsaMachineRegisterBank *bank,
    const ValueSsaAllocationPrep *allocation_prep,
    ValueSsaFunctionMachinePlanningView *out_view,
    ValueSsaError *error);
int value_ssa_compute_function_flat_machine_planning_view(
    const ValueSsaFunction *function,
    size_t color_budget,
    size_t machine_register_count,
    const ValueSsaAllocationPrep *allocation_prep,
    ValueSsaFunctionMachinePlanningView *out_view,
    ValueSsaError *error);
int value_ssa_compute_program_machine_planning_report(
    const ValueSsaProgram *program,
    size_t color_budget,
    const ValueSsaMachineRegisterBank *bank,
    ValueSsaProgramMachinePlanningReport *out_report,
    ValueSsaError *error);
int value_ssa_compute_program_flat_machine_planning_report(
    const ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    ValueSsaProgramMachinePlanningReport *out_report,
    ValueSsaError *error);
int value_ssa_allocate_and_rewrite_program_single_block_spills_machine_report(
    ValueSsaProgram *program,
    size_t color_budget,
    const ValueSsaMachineRegisterBank *bank,
    ValueSsaAllocateRewriteMachineReport *out_report,
    ValueSsaError *error);

int value_ssa_machine_register_bank_get_summary(const ValueSsaMachineRegisterBank *bank,
    size_t *out_register_count,
    size_t *out_allocatable_count,
    size_t *out_caller_clobbered_count,
    size_t *out_callee_preserved_count);
int value_ssa_machine_register_bank_get_register(const ValueSsaMachineRegisterBank *bank,
    size_t register_index,
    const ValueSsaMachineRegisterDesc **out_desc);
int value_ssa_machine_register_bank_find_register_by_name(const ValueSsaMachineRegisterBank *bank,
    const char *register_name,
    size_t *out_register_index,
    const ValueSsaMachineRegisterDesc **out_desc);
int value_ssa_function_machine_allocation_view_get_summary(const ValueSsaFunctionMachineAllocationView *view,
    const char **out_function_name,
    size_t *out_value_count,
    size_t *out_machine_register_count,
    size_t *out_spill_slot_count,
    size_t *out_machine_colored_count,
    size_t *out_spilled_count,
    size_t *out_caller_clobbered_value_count,
    size_t *out_callee_preserved_value_count,
    size_t *out_used_machine_register_count,
    size_t *out_machine_register_group_count);
int value_ssa_function_machine_allocation_view_get_placement(const ValueSsaFunctionMachineAllocationView *view,
    size_t value_id,
    const ValueSsaMachineAllocatedValuePlacement **out_placement);
int value_ssa_function_machine_allocation_view_get_machine_colored_values(
    const ValueSsaFunctionMachineAllocationView *view,
    size_t *out_count,
    const size_t **out_value_ids);
int value_ssa_function_machine_allocation_view_get_spilled_values(
    const ValueSsaFunctionMachineAllocationView *view,
    size_t *out_count,
    const size_t **out_value_ids);
int value_ssa_function_machine_allocation_view_get_caller_clobbered_values(
    const ValueSsaFunctionMachineAllocationView *view,
    size_t *out_count,
    const size_t **out_value_ids);
int value_ssa_function_machine_allocation_view_get_callee_preserved_values(
    const ValueSsaFunctionMachineAllocationView *view,
    size_t *out_count,
    const size_t **out_value_ids);
int value_ssa_function_machine_allocation_view_get_used_machine_registers(
    const ValueSsaFunctionMachineAllocationView *view,
    size_t *out_count,
    const size_t **out_machine_register_ids);
int value_ssa_function_machine_allocation_view_get_machine_register_group(
    const ValueSsaFunctionMachineAllocationView *view,
    size_t machine_register_id,
    const ValueSsaMachineRegisterValueGroup **out_group);
int value_ssa_function_machine_allocation_view_get_machine_register_group_at(
    const ValueSsaFunctionMachineAllocationView *view,
    size_t group_index,
    const ValueSsaMachineRegisterValueGroup **out_group);
int value_ssa_program_machine_allocation_view_get_summary(const ValueSsaProgramMachineAllocationView *view,
    size_t *out_function_count,
    size_t *out_total_value_count,
    size_t *out_total_machine_colored_count,
    size_t *out_total_spilled_count,
    size_t *out_total_caller_clobbered_value_count,
    size_t *out_total_callee_preserved_value_count);
int value_ssa_program_machine_allocation_view_get_function_view(const ValueSsaProgramMachineAllocationView *view,
    size_t function_index,
    const ValueSsaFunctionMachineAllocationView **out_function_view);
int value_ssa_program_machine_allocation_view_get_function_view_by_name(
    const ValueSsaProgramMachineAllocationView *view,
    const char *function_name,
    size_t *out_function_index,
    const ValueSsaFunctionMachineAllocationView **out_function_view);
int value_ssa_program_machine_allocation_view_get_functions_with_machine_colors(
    const ValueSsaProgramMachineAllocationView *view,
    size_t *out_count,
    const size_t **out_function_indices);
int value_ssa_program_machine_allocation_view_get_functions_with_spills(
    const ValueSsaProgramMachineAllocationView *view,
    size_t *out_count,
    const size_t **out_function_indices);
int value_ssa_program_machine_allocation_view_get_functions_with_caller_clobbered_values(
    const ValueSsaProgramMachineAllocationView *view,
    size_t *out_count,
    const size_t **out_function_indices);
int value_ssa_program_machine_allocation_view_get_functions_with_callee_preserved_values(
    const ValueSsaProgramMachineAllocationView *view,
    size_t *out_count,
    const size_t **out_function_indices);
int value_ssa_allocate_rewrite_machine_report_get_stats(
    const ValueSsaAllocateRewriteMachineReport *report,
    size_t *out_allocation_rounds,
    size_t *out_rewrite_rounds);
int value_ssa_allocate_rewrite_machine_report_get_program_view(
    const ValueSsaAllocateRewriteMachineReport *report,
    const ValueSsaProgramMachineAllocationView **out_view);
int value_ssa_allocate_rewrite_machine_report_get_function_view(
    const ValueSsaAllocateRewriteMachineReport *report,
    size_t function_index,
    const ValueSsaFunctionMachineAllocationView **out_function_view);
int value_ssa_allocate_rewrite_machine_report_get_function_view_by_name(
    const ValueSsaAllocateRewriteMachineReport *report,
    const char *function_name,
    size_t *out_function_index,
    const ValueSsaFunctionMachineAllocationView **out_function_view);
int value_ssa_allocate_rewrite_machine_report_get_summary(
    const ValueSsaAllocateRewriteMachineReport *report,
    size_t *out_allocation_rounds,
    size_t *out_rewrite_rounds,
    size_t *out_function_count,
    size_t *out_total_value_count,
    size_t *out_total_machine_colored_count,
    size_t *out_total_spilled_count);
const char *value_ssa_machine_call_clobber_risk_class_name(
    ValueSsaMachineCallClobberRiskClass risk_class);
int value_ssa_machine_call_clobber_risk_report_get_summary(
    const ValueSsaMachineCallClobberRiskReport *report,
    size_t *out_value_count,
    size_t *out_total_risky_value_count);
int value_ssa_machine_call_clobber_risk_report_get_class_count(
    const ValueSsaMachineCallClobberRiskReport *report,
    ValueSsaMachineCallClobberRiskClass risk_class,
    size_t *out_count);
int value_ssa_machine_call_clobber_risk_report_find_value(
    const ValueSsaMachineCallClobberRiskReport *report,
    size_t value_id,
    size_t *out_index,
    const ValueSsaMachineCallClobberRiskItem **out_item);
int value_ssa_program_machine_call_clobber_risk_report_get_class_count(
    const ValueSsaProgramMachineCallClobberRiskReport *report,
    ValueSsaMachineCallClobberRiskClass risk_class,
    size_t *out_count);
int value_ssa_program_machine_call_clobber_risk_report_get_summary(
    const ValueSsaProgramMachineCallClobberRiskReport *report,
    size_t *out_function_count,
    size_t *out_total_value_count,
    size_t *out_total_risky_value_count);
int value_ssa_program_machine_call_clobber_risk_report_get_function_view(
    const ValueSsaProgramMachineCallClobberRiskReport *report,
    size_t function_index,
    const ValueSsaFunctionMachineCallClobberRiskView **out_view);
int value_ssa_program_machine_call_clobber_risk_report_get_function_view_by_name(
    const ValueSsaProgramMachineCallClobberRiskReport *report,
    const char *function_name,
    size_t *out_function_index,
    const ValueSsaFunctionMachineCallClobberRiskView **out_view);
int value_ssa_program_machine_call_clobber_risk_report_get_functions_with_risky_values(
    const ValueSsaProgramMachineCallClobberRiskReport *report,
    size_t *out_count,
    const size_t **out_function_indices);
int value_ssa_program_machine_call_clobber_risk_report_get_functions_with_risk_class(
    const ValueSsaProgramMachineCallClobberRiskReport *report,
    ValueSsaMachineCallClobberRiskClass risk_class,
    size_t *out_count,
    const size_t **out_function_indices);
int value_ssa_machine_protection_agenda_get_summary(
    const ValueSsaMachineProtectionAgenda *agenda,
    size_t *out_value_count,
    size_t *out_item_count,
    size_t *out_crossing_count,
    size_t *out_repeated_count,
    size_t *out_hot_count);
int value_ssa_machine_protection_agenda_find_value(
    const ValueSsaMachineProtectionAgenda *agenda,
    size_t value_id,
    size_t *out_index,
    const ValueSsaMachineProtectionAgendaItem **out_item);
int value_ssa_program_machine_protection_report_get_summary(
    const ValueSsaProgramMachineProtectionReport *report,
    size_t *out_function_count,
    size_t *out_total_value_count,
    size_t *out_total_item_count);
int value_ssa_program_machine_protection_report_get_function_view(
    const ValueSsaProgramMachineProtectionReport *report,
    size_t function_index,
    const ValueSsaFunctionMachineProtectionView **out_view);
int value_ssa_program_machine_protection_report_get_function_view_by_name(
    const ValueSsaProgramMachineProtectionReport *report,
    const char *function_name,
    size_t *out_function_index,
    const ValueSsaFunctionMachineProtectionView **out_view);
int value_ssa_program_machine_protection_report_get_functions_with_items(
    const ValueSsaProgramMachineProtectionReport *report,
    size_t *out_count,
    const size_t **out_function_indices);
int value_ssa_program_machine_protection_report_get_functions_with_risk_class(
    const ValueSsaProgramMachineProtectionReport *report,
    ValueSsaMachineCallClobberRiskClass risk_class,
    size_t *out_count,
    const size_t **out_function_indices);
int value_ssa_machine_register_protection_pressure_view_get_summary(
    const ValueSsaMachineRegisterProtectionPressureView *view,
    size_t *out_machine_register_count,
    size_t *out_pressured_register_count,
    size_t *out_total_value_count,
    size_t *out_total_item_count,
    size_t *out_total_protection_priority,
    size_t *out_crossing_count,
    size_t *out_repeated_count,
    size_t *out_hot_count);
int value_ssa_machine_register_protection_pressure_view_get_register_item(
    const ValueSsaMachineRegisterProtectionPressureView *view,
    size_t machine_register_id,
    const ValueSsaMachineRegisterProtectionPressureItem **out_item);
int value_ssa_machine_register_protection_pressure_view_get_register_item_at(
    const ValueSsaMachineRegisterProtectionPressureView *view,
    size_t item_index,
    const ValueSsaMachineRegisterProtectionPressureItem **out_item);
int value_ssa_program_machine_register_protection_pressure_report_get_summary(
    const ValueSsaProgramMachineRegisterProtectionPressureReport *report,
    size_t *out_function_count,
    size_t *out_total_value_count,
    size_t *out_total_item_count,
    size_t *out_total_protection_priority,
    size_t *out_total_pressured_register_count);
int value_ssa_program_machine_register_protection_pressure_report_get_function_view(
    const ValueSsaProgramMachineRegisterProtectionPressureReport *report,
    size_t function_index,
    const ValueSsaFunctionMachineRegisterProtectionPressureView **out_view);
int value_ssa_program_machine_register_protection_pressure_report_get_function_view_by_name(
    const ValueSsaProgramMachineRegisterProtectionPressureReport *report,
    const char *function_name,
    size_t *out_function_index,
    const ValueSsaFunctionMachineRegisterProtectionPressureView **out_view);
int value_ssa_program_machine_register_protection_pressure_report_get_functions_with_pressure(
    const ValueSsaProgramMachineRegisterProtectionPressureReport *report,
    size_t *out_count,
    const size_t **out_function_indices);
int value_ssa_program_machine_register_protection_pressure_report_get_functions_with_risk_class(
    const ValueSsaProgramMachineRegisterProtectionPressureReport *report,
    ValueSsaMachineCallClobberRiskClass risk_class,
    size_t *out_count,
    const size_t **out_function_indices);
int value_ssa_function_machine_register_protection_hotspot_view_get_summary(
    const ValueSsaFunctionMachineRegisterProtectionHotspotView *view,
    const char **out_function_name,
    int *out_has_hotspot,
    size_t *out_total_item_count,
    size_t *out_total_value_count,
    const ValueSsaMachineRegisterProtectionHotspotItem **out_hotspot);
int value_ssa_program_machine_register_protection_hotspot_report_get_summary(
    const ValueSsaProgramMachineRegisterProtectionHotspotReport *report,
    size_t *out_function_count,
    size_t *out_total_value_count,
    size_t *out_total_item_count,
    size_t *out_total_hotspot_priority,
    size_t *out_total_hotspot_item_count);
int value_ssa_program_machine_register_protection_hotspot_report_get_function_view(
    const ValueSsaProgramMachineRegisterProtectionHotspotReport *report,
    size_t function_index,
    const ValueSsaFunctionMachineRegisterProtectionHotspotView **out_view);
int value_ssa_program_machine_register_protection_hotspot_report_get_function_view_by_name(
    const ValueSsaProgramMachineRegisterProtectionHotspotReport *report,
    const char *function_name,
    size_t *out_function_index,
    const ValueSsaFunctionMachineRegisterProtectionHotspotView **out_view);
int value_ssa_program_machine_register_protection_hotspot_report_get_functions_with_hotspots(
    const ValueSsaProgramMachineRegisterProtectionHotspotReport *report,
    size_t *out_count,
    const size_t **out_function_indices);
int value_ssa_function_machine_preservation_hint_view_get_summary(
    const ValueSsaFunctionMachinePreservationHintView *view,
    const char **out_function_name,
    int *out_has_hint,
    size_t *out_protected_item_count,
    size_t *out_total_value_count,
    size_t *out_total_hint_priority);
int value_ssa_function_machine_preservation_hint_view_get_candidate_at(
    const ValueSsaFunctionMachinePreservationHintView *view,
    size_t candidate_index,
    const ValueSsaMachinePreservationHintCandidate **out_candidate);
int value_ssa_program_machine_preservation_hint_report_get_summary(
    const ValueSsaProgramMachinePreservationHintReport *report,
    size_t *out_function_count,
    size_t *out_total_value_count,
    size_t *out_total_item_count,
    size_t *out_total_hint_priority);
int value_ssa_program_machine_preservation_hint_report_get_reason_count(
    const ValueSsaProgramMachinePreservationHintReport *report,
    ValueSsaMachinePreservationReason reason,
    size_t *out_count);
int value_ssa_program_machine_preservation_hint_report_get_function_view(
    const ValueSsaProgramMachinePreservationHintReport *report,
    size_t function_index,
    const ValueSsaFunctionMachinePreservationHintView **out_view);
int value_ssa_program_machine_preservation_hint_report_get_function_view_by_name(
    const ValueSsaProgramMachinePreservationHintReport *report,
    const char *function_name,
    size_t *out_function_index,
    const ValueSsaFunctionMachinePreservationHintView **out_view);
int value_ssa_program_machine_preservation_hint_report_get_functions_with_hints(
    const ValueSsaProgramMachinePreservationHintReport *report,
    size_t *out_count,
    const size_t **out_function_indices);
int value_ssa_program_machine_preservation_hint_report_get_functions_with_reason(
    const ValueSsaProgramMachinePreservationHintReport *report,
    ValueSsaMachinePreservationReason reason,
    size_t *out_count,
    const size_t **out_function_indices);
int value_ssa_function_machine_planning_view_get_summary(
    const ValueSsaFunctionMachinePlanningView *view,
    const char **out_function_name,
    size_t *out_total_value_count,
    size_t *out_pressure_item_count,
    size_t *out_pressure_priority,
    int *out_has_hotspot,
    int *out_has_hint);
int value_ssa_program_machine_planning_report_get_summary(
    const ValueSsaProgramMachinePlanningReport *report,
    size_t *out_function_count,
    size_t *out_total_value_count,
    size_t *out_total_pressure_item_count,
    size_t *out_total_pressure_priority,
    size_t *out_total_hotspot_priority,
    size_t *out_total_hint_priority);
int value_ssa_program_machine_planning_report_get_hint_reason_count(
    const ValueSsaProgramMachinePlanningReport *report,
    ValueSsaMachinePreservationReason reason,
    size_t *out_count);
int value_ssa_program_machine_planning_report_get_function_view(
    const ValueSsaProgramMachinePlanningReport *report,
    size_t function_index,
    const ValueSsaFunctionMachinePlanningView **out_view);
int value_ssa_program_machine_planning_report_get_function_view_by_name(
    const ValueSsaProgramMachinePlanningReport *report,
    const char *function_name,
    size_t *out_function_index,
    const ValueSsaFunctionMachinePlanningView **out_view);
int value_ssa_program_machine_planning_report_get_functions_with_pressure(
    const ValueSsaProgramMachinePlanningReport *report,
    size_t *out_count,
    const size_t **out_function_indices);
int value_ssa_program_machine_planning_report_get_functions_with_hotspots(
    const ValueSsaProgramMachinePlanningReport *report,
    size_t *out_count,
    const size_t **out_function_indices);
int value_ssa_program_machine_planning_report_get_functions_with_hints(
    const ValueSsaProgramMachinePlanningReport *report,
    size_t *out_count,
    const size_t **out_function_indices);
int value_ssa_program_machine_planning_report_get_functions_with_hint_reason(
    const ValueSsaProgramMachinePlanningReport *report,
    ValueSsaMachinePreservationReason reason,
    size_t *out_count,
    const size_t **out_function_indices);
int value_ssa_program_machine_planning_report_get_pressure_reason_count(
    const ValueSsaProgramMachinePlanningReport *report,
    ValueSsaMachineCallClobberRiskClass risk_class,
    size_t *out_count);
int value_ssa_program_machine_planning_report_get_functions_with_pressure_reason(
    const ValueSsaProgramMachinePlanningReport *report,
    ValueSsaMachineCallClobberRiskClass risk_class,
    size_t *out_count,
    const size_t **out_function_indices);

int value_ssa_dump_machine_register_bank(const ValueSsaMachineRegisterBank *bank,
    char **out_text,
    ValueSsaError *error);
int value_ssa_dump_function_machine_allocation_view(const ValueSsaFunctionMachineAllocationView *view,
    char **out_text,
    ValueSsaError *error);
int value_ssa_dump_program_machine_allocation_view(const ValueSsaProgramMachineAllocationView *view,
    char **out_text,
    ValueSsaError *error);
int value_ssa_dump_allocate_rewrite_machine_report_artifact(
    const ValueSsaAllocateRewriteMachineReport *report,
    char **out_text,
    ValueSsaError *error);
int value_ssa_dump_allocate_rewrite_machine_report(
    const ValueSsaAllocateRewriteLayoutReport *report,
    const ValueSsaMachineRegisterBank *bank,
    char **out_text,
    ValueSsaError *error);
int value_ssa_dump_machine_call_clobber_risk_report(
    const ValueSsaMachineCallClobberRiskReport *report,
    char **out_text,
    ValueSsaError *error);
int value_ssa_dump_program_machine_call_clobber_risk_report(
    const ValueSsaProgramMachineCallClobberRiskReport *report,
    char **out_text,
    ValueSsaError *error);
int value_ssa_dump_machine_protection_agenda(
    const ValueSsaMachineProtectionAgenda *agenda,
    char **out_text,
    ValueSsaError *error);
int value_ssa_dump_program_machine_protection_report(
    const ValueSsaProgramMachineProtectionReport *report,
    char **out_text,
    ValueSsaError *error);
int value_ssa_dump_machine_register_protection_pressure_view(
    const ValueSsaMachineRegisterProtectionPressureView *view,
    char **out_text,
    ValueSsaError *error);
int value_ssa_dump_program_machine_register_protection_pressure_report(
    const ValueSsaProgramMachineRegisterProtectionPressureReport *report,
    char **out_text,
    ValueSsaError *error);
int value_ssa_dump_function_machine_register_protection_hotspot_view(
    const ValueSsaFunctionMachineRegisterProtectionHotspotView *view,
    char **out_text,
    ValueSsaError *error);
int value_ssa_dump_program_machine_register_protection_hotspot_report(
    const ValueSsaProgramMachineRegisterProtectionHotspotReport *report,
    char **out_text,
    ValueSsaError *error);
int value_ssa_dump_function_machine_preservation_hint_view(
    const ValueSsaFunctionMachinePreservationHintView *view,
    char **out_text,
    ValueSsaError *error);
int value_ssa_dump_program_machine_preservation_hint_report(
    const ValueSsaProgramMachinePreservationHintReport *report,
    char **out_text,
    ValueSsaError *error);
int value_ssa_dump_function_machine_planning_view(
    const ValueSsaFunctionMachinePlanningView *view,
    char **out_text,
    ValueSsaError *error);
int value_ssa_dump_program_machine_planning_report(
    const ValueSsaProgramMachinePlanningReport *report,
    char **out_text,
    ValueSsaError *error);
int value_ssa_allocate_and_rewrite_program_single_block_spills_machine_report_dump(
    ValueSsaProgram *program,
    size_t color_budget,
    const ValueSsaMachineRegisterBank *bank,
    char **out_text,
    ValueSsaError *error);
int value_ssa_allocate_and_rewrite_program_single_block_spills_flat_machine_report(
    ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    ValueSsaAllocateRewriteMachineReport *out_report,
    ValueSsaError *error);
int value_ssa_allocate_function_flat_machine_view(const ValueSsaFunction *function,
    size_t color_budget,
    size_t machine_register_count,
    ValueSsaFunctionMachineAllocationView *out_view,
    ValueSsaError *error);
int value_ssa_allocate_program_flat_machine_view(const ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    ValueSsaProgramMachineAllocationView *out_view,
    ValueSsaError *error);
int value_ssa_allocate_function_machine_view_dump(const ValueSsaFunction *function,
    size_t color_budget,
    const ValueSsaMachineRegisterBank *bank,
    char **out_text,
    ValueSsaError *error);
int value_ssa_allocate_program_machine_view_dump(const ValueSsaProgram *program,
    size_t color_budget,
    const ValueSsaMachineRegisterBank *bank,
    char **out_text,
    ValueSsaError *error);
int value_ssa_allocate_function_flat_machine_view_dump(const ValueSsaFunction *function,
    size_t color_budget,
    size_t machine_register_count,
    char **out_text,
    ValueSsaError *error);
int value_ssa_allocate_program_flat_machine_view_dump(const ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    char **out_text,
    ValueSsaError *error);
int value_ssa_allocate_and_rewrite_program_single_block_spills_flat_machine_report_dump(
    ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    char **out_text,
    ValueSsaError *error);

#endif
