---
description: Derive plan.md from an approved feature spec
---

Derive `plan.md` for the feature in `$ARGUMENTS` (default: most recently
modified `specs/` directory).

Refuse to plan a spec whose status is not `Ready` — report the unresolved open
questions instead.

Procedure:

1. Read the constitution, the feature's `spec.md`, and
   `specs/003-runtime-mariadb-binding/contracts/` for the current runtime ABI.
2. Fill in `.specify/templates/plan-template.md`.
3. Hard constraints:
   - Generated C calls `esqlc_*` entry points only. If the plan needs a new one,
     add its signature to section 4 **and** to the 003 contracts directory in
     the same change (Principle V).
   - Every structure layout gets `_Static_assert` on `sizeof` and every
     externally visible `offsetof` (Principle VI).
   - The requirement → component map covers every `FR`/`NFR` exactly once. Stop
     and report if any is unmapped.
4. State the decision in section 1 and put the survey in section 2. One
   recommendation, not a menu.
5. Risks must be specific to this feature. "MariaDB semantics may differ" is not
   a risk; "InnoDB has no equivalent of cursor stability at the SQL/MP
   BROWSE/STABLE/REPEATABLE granularity" is.

Report: component count, new ABI entry points, unmapped requirements (must be
zero), divergences introduced.
