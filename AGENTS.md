# AGENTS

## Startup Contract

- Before starting work, read `AGENTS.md`.
- Then read `docs/ir-conventions.md`.
- Then read `docs/NEXT_STEPS.md`.
- If the task touches lower IR / downstream IR planning or implementation, then also read `docs/LOWER_IR_DESIGN.md` before making changes.

## Shared Authority

- `AGENTS.md` defines agent roles, startup rules, and scope boundaries.
- `docs/ir-conventions.md` is working memory for current engineering facts and safety/convention notes.
- `docs/NEXT_STEPS.md` is the roadmap, execution log, and current-plan authority.
- `docs/LOWER_IR_DESIGN.md` is the current design authority for lower-memory / downstream IR planning unless and until `docs/NEXT_STEPS.md` supersedes part of it.
- `docs/` holds implementation-facing design notes and proposals; unless explicitly promoted by `NEXT_STEPS.md`, treat them as discussion/design authority rather than execution-log authority.

## Agent Roles

### Implementation Agent

- This agent is the default code-changing agent for the current workstream.
- Owns compiler implementation work under `src/`, `include/`, `tests/`, `docs/`, and implementation-facing project notes.
- May update `docs/NEXT_STEPS.md`, `docs/ir-conventions.md`, and implementation-facing design documents such as `docs/LOWER_IR_DESIGN.md` when implementation work changes roadmap facts, working memory, or downstream-IR design direction.
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

## Current Default Assignment

- In this thread, treat the current agent as the Implementation Agent unless the user explicitly reassigns the role.
