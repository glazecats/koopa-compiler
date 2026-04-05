# AGENTS

## Startup Contract

- Before starting work, read `AGENTS.md`.
- Then read `ir-conventions.md`.
- Then read `NEXT_STEPS.md`.

## Shared Authority

- `AGENTS.md` defines agent roles, startup rules, and scope boundaries.
- `ir-conventions.md` is working memory for current engineering facts and safety/convention notes.
- `NEXT_STEPS.md` is the roadmap, execution log, and current-plan authority.

## Agent Roles

### Implementation Agent

- This agent is the default code-changing agent for the current workstream.
- Owns compiler implementation work under `src/`, `include/`, `tests/`, and implementation-facing project notes.
- May update `NEXT_STEPS.md` and `ir-conventions.md` when implementation work changes roadmap facts or working memory.
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
- If review finds issues outside its owned scope, report them rather than editing.
- Do not overwrite, revert, or refactor another agent's owned area without explicit user approval.

## Current Default Assignment

- In this thread, treat the current agent as the Implementation Agent unless the user explicitly reassigns the role.
