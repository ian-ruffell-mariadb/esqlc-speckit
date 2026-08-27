---
description: Execute a feature's tasks.md in dependency order
---

Execute `tasks.md` for the feature in `$ARGUMENTS` (default: the lowest-numbered
`specs/` directory whose spec status is `Ready` and whose tasks are incomplete).

Procedure:

1. Read the constitution, `spec.md`, `plan.md`, `tasks.md`.
2. Verify the feature's `Depends on:` features are complete. Stop if not.
3. Work phase by phase. Within a phase, run `[P]` tasks in parallel where that
   is genuinely faster; otherwise sequentially.
4. Non-negotiable during execution:
   - Phase B tests must **fail** for the right reason before Phase C starts.
     A test that passes against no implementation is a broken test — fix it.
   - Do not implement beyond the task's requirement IDs. Note anything else you
     spot; do not fold it in.
   - When the manual is ambiguous, stop and add an open question to `spec.md`.
     Do not pick a behaviour and move on (Principle I).
5. Mark tasks complete in `tasks.md` as you go, with the commit or file touched.
6. On completion, run the exit criteria checklist and `/speckit.analyze`.

Report: tasks completed, tests passing/failing with output, exit criteria not
met, and anything deferred with the reason.
