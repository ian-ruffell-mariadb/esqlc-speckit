---
description: Write or revise the current feature's spec.md from the SQL/MP C manual
---

Write or revise `spec.md` for the feature named in `$ARGUMENTS` (or, if empty,
the most recently modified directory under `specs/`).

Procedure:

1. Read `.specify/memory/constitution.md`. Every principle applies.
2. Read `docs/traceability.md` to find which manual sections this feature owns
   and which are already claimed by another spec. Do not duplicate coverage.
3. For each owned section, read the manual text. Locate pages with:
   `./.specify/scripts/extract-manual.py --find 9-6`
   then `--page N`. If `manual/` is empty, run `fetch-manual.sh` first.
4. Fill in `.specify/templates/spec-template.md`. Rules:
   - Requirements are single testable assertions. Split compound ones.
   - **Every** requirement carries `[SQLPM/C §n p.n-n]`. No citation, no
     requirement (Principle I).
   - Mark `[EXTERNAL]` where the manual defers to `SQLRM`, the C/C++
     Programmer's Guide, or the Version Management Guide, and record what
     decision is needed to close it.
   - Do not quote manual prose. State the behaviour in your own words. Factual
     tables (type maps, constant values, field names) may be restated.
   - Anything unemulable on MariaDB goes in the diagnostics table with a
     default policy, not into a "best effort" requirement (Principle III).
5. Complete the constitution check table honestly. A `no` needs either a fix or
   a `DIV-nnn` entry added to `docs/divergences.md`.
6. Leave status `Clarifying` while open questions remain; `Ready` only when the
   table is empty.

Report: requirement count, citation coverage, unresolved questions, and any new
divergences created.
