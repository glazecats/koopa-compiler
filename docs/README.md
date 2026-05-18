# Docs Guide

This directory is organized by role rather than kept as one flat list.

## Root

- `NEXT_STEPS.md`: roadmap, active-stage authority, and execution log
- `ENGINEERING_MEMORY.md`: current engineering memory and stable conventions
- `README.md`: this index

## IR

- `ir/LOWER_IR_DESIGN.md`: lower-ir / downstream-ir design authority

## SSA

- `ssa/VALUE_SSA_DESIGN.md`: value-SSA design authority
- `ssa/VALUE_SSA_ALLOCATOR_PLAN.md`: allocator staging authority
- `ssa/VALUE_SSA_ALLOCATOR_RESULT_SHAPING_PLAN.md`: allocator result-shaping notes
- `ssa/VALUE_SSA_MACHINE_REGISTER_MODEL_PLAN.md`: machine-register-model staging
- `ssa/MEMORY_SSA_DESIGN.md`: memory-SSA design authority

## Backend

- `backend/MACHINE_*.md`: backend sibling-line plans

The backend plans are grouped here so the `docs/` root keeps only top-level
working-memory files while the larger machine pipeline remains browseable by
theme.
