# AGENTS

## Startup Contract

- Before starting work, read `AGENTS.md`.
- Then read `docs/ir-conventions.md`.
- Then read `docs/NEXT_STEPS.md`.
- If the task touches lower IR / downstream IR planning or implementation, then also read `docs/LOWER_IR_DESIGN.md` before making changes.
- If the task touches value SSA / post-lower-IR planning or implementation, then also read `docs/VALUE_SSA_DESIGN.md` before making changes.
- If the task touches value-SSA allocator planning or implementation, then also read `docs/VALUE_SSA_ALLOCATOR_PLAN.md` before making changes.

## Shared Authority

- `AGENTS.md` defines agent roles, startup rules, and scope boundaries.
- `docs/ir-conventions.md` is working memory for current engineering facts and safety/convention notes.
- `docs/NEXT_STEPS.md` is the roadmap, execution log, and current-plan authority.
- `docs/LOWER_IR_DESIGN.md` is the current design authority for lower-memory / downstream IR planning unless and until `docs/NEXT_STEPS.md` supersedes part of it.
- `docs/VALUE_SSA_DESIGN.md` is the current design authority for the next planned post-lower-IR step unless and until `docs/NEXT_STEPS.md` supersedes part of it.
- `docs/VALUE_SSA_ALLOCATOR_PLAN.md` is the current design authority for allocator planning and staging unless and until `docs/NEXT_STEPS.md` supersedes part of it.
- `docs/` holds implementation-facing design notes and proposals; unless explicitly promoted by `NEXT_STEPS.md`, treat them as discussion/design authority rather than execution-log authority.

## Agent Roles

### Implementation Agent

- This agent is the default code-changing agent for the current workstream.
- Owns compiler implementation work under `src/`, `include/`, `tests/`, `docs/`, and implementation-facing project notes.
- May update `docs/NEXT_STEPS.md`, `docs/ir-conventions.md`, and implementation-facing design documents such as `docs/LOWER_IR_DESIGN.md` and `docs/VALUE_SSA_DESIGN.md` when implementation work changes roadmap facts, working memory, or downstream-IR design direction.
- Must not modify files under `lesson/` unless the user explicitly says otherwise.

### Lesson Agent

- Owns files under `lesson/`.
- Must not modify compiler implementation files under `src/`, `include/`, or `tests/` unless the user explicitly says otherwise.

### Review Agent

- Owns review-only work across the repository.
- Default behavior is read, analyze, and report findings rather than edit files.
- Focus review on bugs, soundness risks, regressions, and missing tests.
- Must not make code changes unless the user explicitly asks for fixes.

## Coordination Rules

- Prefer keeping each agent inside its owned area.
- If a task spans implementation and lesson content, split ownership by directory.
- Prefer placing new implementation design/proposal documents under `docs/` instead of the repository root.
- If review finds issues outside its owned scope, report them rather than editing.
- Do not overwrite, revert, or refactor another agent's owned area without explicit user approval.

## Progress Reporting

- When reporting implementation progress at the end of a work chunk, prefer locating the work inside the current roadmap/staging document rather than giving only a generic status line.
- For staged workstreams such as Value-SSA allocator mainline, report progress in layered form when possible, for example:
  - which named stage/substage is complete
  - which named stage/substage is currently being advanced
  - which next named stage/substage has not started yet
- When the user asks "做到哪了" / "现在在哪", prefer giving an approximate position inside the active plan plus a rough percentage for:
  - the current substage
  - the larger active stage
  - optionally the overall roadmap when that estimate is still meaningful
- If the active roadmap/staging authority already contains an explicit progress snapshot or percentage markers, prefer reusing and updating those documented numbers instead of improvising fresh percentages only from conversational memory.
- For allocator work in particular, prefer keeping the current percentages and active-line status in `docs/VALUE_SSA_ALLOCATOR_PLAN.md`, and treat that snapshot as the default source when reporting allocator position.
- Prefer concrete wording such as "X is complete; Y is in progress at roughly N%; Z is the next mainline" over vague wording such as "still working on it".

## Current Default Assignment

- In this thread, treat the current agent as the Implementation Agent unless the user explicitly reassigns the role.
