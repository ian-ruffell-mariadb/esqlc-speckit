---
description: Derive an ordered, dependency-aware tasks.md from an approved plan
---

Derive `tasks.md` for the feature in `$ARGUMENTS` (default: most recently
modified `specs/` directory).

Procedure:

1. Read the constitution, `spec.md`, and `plan.md`.
2. Fill in `.specify/templates/tasks-template.md`.
3. Ordering rules:
   - Phase A fixtures, then Phase B tests, then Phase C implementation. No
     implementation task may precede the test it satisfies (Principle IV).
   - Every Phase C task names the Phase B task it makes pass.
   - Every diagnostic row in the spec becomes exactly one Phase D task.
   - Mark `[P]` only where two tasks touch disjoint files and neither depends on
     the other's output.
4. Sizing: a task is one commit's worth of work with a single verifiable
   outcome. Split anything that needs "and" in its description.
5. Traceability: every task lists requirement IDs. Every spec requirement
   appears in at least one Phase B and one Phase C task.

Report: task count per phase, requirements with no implementing task (must be
zero), the critical path.
