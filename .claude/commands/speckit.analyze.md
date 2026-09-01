---
description: Cross-check a feature's spec, plan, and tasks against the constitution
---

Audit the feature in `$ARGUMENTS` (default: every directory under `specs/`).
Read-only — report, do not fix, unless asked.

Checks:

**Principle I — citations**
- Every `FR`/`NFR` has a `[SQLPM/C §n p.n-n]` citation, an `[EXTERNAL]` marker,
  or a `[DIV-nnn]` reference. List violations.
- Verify citations against the manual by running the harness, not by
  improvising a check:

  ```
  python3 tests/harness/citation_check.py
  ```

  It resolves every page label, confirms the cited section matches the label's
  own section, and content-checks a curated set. It skips (77) when `manual/`
  is absent. Do **not** hand-roll a PDF extraction here: the first time this
  audit ran, an improvised checker reported five false citation mismatches
  because its virtualenv had lost `pypdf`. If the harness reports
  `unverifiable`, that is an extraction gap in the manual footer, not a spec
  defect — report it as info, never as a finding.

**Principle II — source compatibility**
- No requirement mandates a source change in customer code.
- No extension is on by default.

**Principle III — diagnostics**
- Every construct the plan cannot faithfully emulate appears in the spec's
  diagnostics table.
- No default policy is `ignore` for anything affecting a result set, a lock, or
  a transaction boundary.

**Principle IV — tests first**
- Every Phase C task names a Phase B task.
- Every acceptance scenario names a concrete test path. A scenario may name
  more than one — a positive and a negative fixture is the usual reason — so
  compare scenarios against scenarios-without-a-test, never against a count of
  `**Test:**` lines.

**Principle V — ABI**
- No plan component emits MariaDB API calls into generated C.
- Every `esqlc_*` signature in a plan exists in
  `specs/003-runtime-mariadb-binding/contracts/`.

**Principle VI — structures**
- Every generated structure version has static assertions planned.
- Documented constants match: SQLCA 430; SQLSA 838 / 1790; SQLDA header 4,
  sqlvar 24, names-buffer overhead 11.

  `SQLDA_SQLVAR_LEN` legitimately appears as **both** 24 and 40 — the published
  32-bit value and the widened one. That is `DIV-040`, not drift.

  Grep this in Python, not zsh: a bracket expression like `[^0-9]` inside a
  double-quoted zsh string is parsed as a math expression and the check
  silently reports nothing found for every constant.

**Principle VII — divergences**
- Every `DIV-nnn` referenced anywhere exists in `docs/divergences.md` with all
  six required fields.
- Every divergence entry is referenced by at least one spec.

**Principle VIII — slices**

For every slice document (e.g. `specs/gate-*.md`):
- The requirement subset is enumerated as explicit IDs, not described.
- The avoidance table covers **every** open question in every spec the slice
  touches. A missing row is a finding; so is a row whose stated reason is
  contradicted by the slice's own fixture.
- Every scoped decision is labelled provisional and names the question it
  narrows, without claiming to resolve it.
- A non-proof section exists and is specific.
- No spec has been marked `Ready` on the strength of a slice.
- Every requirement in the subset still carries its citation (Principle I is not
  relaxed inside a slice).

**Cross-feature**
- `docs/traceability.md` has no manual section owned by two features, and none
  unowned.
- `docs/traceability.md` reflects the gates that have merged. After a gate
  merges, its topics move off `spec` — to `tested` only where the topic is
  wholly covered, otherwise to `partial` with the gap named. Four gates merged
  before anyone updated this file; a slice that ships without moving its rows
  makes the document understate the project.
- No requirement duplicated across features.

Report as a table: check, status, offending file:line. Finish with a single
verdict line — `CLEAN` or `n findings`.
