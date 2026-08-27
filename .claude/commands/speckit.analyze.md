---
description: Cross-check a feature's spec, plan, and tasks against the constitution
---

Audit the feature in `$ARGUMENTS` (default: every directory under `specs/`).
Read-only — report, do not fix, unless asked.

Checks:

**Principle I — citations**
- Every `FR`/`NFR` has a `[SQLPM/C §n p.n-n]` citation, an `[EXTERNAL]` marker,
  or a `[DIV-nnn]` reference. List violations.
- Spot-check three citations per feature against the extracted manual text; a
  citation pointing at the wrong content is worse than none.

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
- Every acceptance scenario names a concrete test path.

**Principle V — ABI**
- No plan component emits MariaDB API calls into generated C.
- Every `esqlc_*` signature in a plan exists in
  `specs/003-runtime-mariadb-binding/contracts/`.

**Principle VI — structures**
- Every generated structure version has static assertions planned.
- Documented constants match: SQLCA 430; SQLSA 838 / 1790; SQLDA header 4,
  sqlvar 24, names-buffer overhead 11.

**Principle VII — divergences**
- Every `DIV-nnn` referenced anywhere exists in `docs/divergences.md` with all
  six required fields.
- Every divergence entry is referenced by at least one spec.

**Cross-feature**
- `docs/traceability.md` has no manual section owned by two features, and none
  unowned.
- No requirement duplicated across features.

Report as a table: check, status, offending file:line. Finish with a single
verdict line — `CLEAN` or `n findings`.
