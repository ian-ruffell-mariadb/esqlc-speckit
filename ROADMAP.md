# Roadmap

Eight features, four phases. Each phase has a gate — a demonstrable capability,
not a date. A phase does not start until its gate predecessor's exit criteria
pass and `/speckit.analyze` is clean.

```
Phase 1  ──▶ Phase 2  ──▶ Phase 3  ──▶ Phase 4
001,002,003   004,005      006,007      008
```

## Phase 1 — Something compiles and runs

| Feature | Deliverable |
|---|---|
| [001 Preprocessor core](specs/001-preprocessor-core/spec.md) | `EXEC SQL` scanning, placement enforcement, `#pragma SQL`, `#line` fidelity, C emission, listing output |
| [002 Host variables](specs/002-host-variables/spec.md) | Declare sections, full type mapping, indicators, `TYPE AS`, `SETSCALE` |
| [003 Runtime & MariaDB binding](specs/003-runtime-mariadb-binding/spec.md) | `esqlc_*` ABI, connection model, transaction control |

**Gate 1:** fully specified in [specs/gate-1.md](specs/gate-1.md) — a declare
section, `#pragma SQL`, `BEGIN WORK`, one `INSERT` with host variables, and
`COMMIT WORK` preprocesses, compiles against the ABI header with no MariaDB
header present, links, runs, and changes a MariaDB table. A `ROLLBACK WORK`
variant proves the transaction is real rather than autocommit in disguise.

**Status: ready to plan.** The slice touches exactly one open question across its
three specs — the `UNKNOWN` character-set corner of 002 Q4 — which is narrowed by
a recorded slice decision. Notably, 003 Q1 concerns statements *outside* a
transaction, so keeping explicit `BEGIN`/`COMMIT WORK` in the gate **avoids** it;
dropping them would make the gate riskier, not simpler. Proceeding under
Principle VIII while 002 and 003 remain `Clarifying`.

Rationale for this ordering: 003 before 004 because there is no way to test a
`SELECT … INTO` without a runtime, and no way to design the runtime ABI without
knowing what host variables look like (002). 001 and 002 are near-inseparable;
they are split because their test harnesses differ — 001 is golden-file, 002 is
type-table-driven.

## Phase 2 — Correct and observable

| Feature | Deliverable |
|---|---|
| [004 Static DML & cursors](specs/004-static-dml-cursors/spec.md) | Single/multirow `SELECT`, `INSERT`/`UPDATE`/`DELETE`, full cursor lifecycle, cursor position and stability |
| [005 Diagnostics](specs/005-diagnostics/spec.md) | `sqlcode`, `SQLCA`, `SQLSA`, `WHENEVER`, `INCLUDE STRUCTURES` versioning, the SQLCA/SQLSA access procedures |

**Gate 2 — retrieval**, specified in [specs/gate-2.md](specs/gate-2.md) and
**ready to plan**. Deliberately the smallest retrieval slice: single-row
`SELECT … INTO` by primary key, one indicator, and the not-found path. It
discharges the two requirements Gate 1 had to drop as untestable (`FR-002.28`,
`FR-003.12`) and resolves `DIV-052`. It touches only the two questions Gate 1
already narrowed, carrying slice decisions SD-1 and SD-2 unchanged.

**Gate 3 and the full Phase 2 gate:** the conformance suite covers every §4
statement form and every §9 diagnostic path, including cursors, `WHENEVER`
precedence (NOT FOUND, then SQLERROR, then SQLWARNING) and the four mandatory
conversion warnings from §2. Gate 2 proves none of that — see its non-proof
section.

004 and 005 are concurrent and mutually dependent — cursor tests need `sqlcode`,
and `SQLSA` statistics need cursor operations to populate them. Run them as one
work item with two specs if the team is small.

## Phase 3 — Real applications

| Feature | Deliverable |
|---|---|
| [006 INVOKE](specs/006-invoke-schema-gen/spec.md) | Schema-derived structure generation, indicator arrays, `CHAR_AS_STRING`/`CHAR_AS_ARRAY` |
| [007 Dynamic SQL](specs/007-dynamic-sql/spec.md) | `PREPARE`/`EXECUTE`/`DESCRIBE`, `SQLDA` + names + collation buffers, dynamic cursors, legacy v1/v2 descriptors |

**Gate 3:** a nontrivial application — a dynamic SQL query tool over the App. A
sample database, of the shape §10 develops — works end to end.

006 before or alongside 007: `INVOKE` is what makes the conformance fixtures
practical to write, and dynamic SQL is the largest single feature in the manual.

## Phase 4 — The honest edges

| Feature | Deliverable |
|---|---|
| [008 NonStop compatibility surface](specs/008-nonstop-compat-surface/spec.md) | Guardian naming, TACL DEFINEs, DDL/DCL/DSL policy, PAID, VSBB, program invalidation, CPRL, version management, `SQLMEM`, Pathway, App. B/C |

**Gate 4:** every construct in [nonstop-specifics.md](docs/reference/nonstop-specifics.md)
has a stated policy and a test proving that policy fires. No construct reaches
the runtime undecided.

008 is last by necessity and not by importance. It is the feature that decides
whether a customer's program is *rejected clearly* or *accepted wrongly*, and it
is deliberately scheduled after the core works so that the policies are chosen
with real code in front of the team rather than from the manual alone.

## Standing risks

| Risk | Phase | Note |
|---|---|---|
| `DIV-002` TACL DEFINEs unresolved | 1 | Blocks fixtures transcribed from manual examples; 001 must avoid `=name` or resolve it early |
| Structure layout guesswork | 2 | The manual gives field names and total sizes but not offsets. Sizes must be hit exactly; field order is inferred and needs validation against real NonStop output if any is obtainable |
| SQLCA field-level content | 2 | Documented only via `SQLCAGETINFOLIST` item codes (§5), never as a field list. 005 is working from indirect evidence |
| `SQLRM` deferrals | 3 | Statement syntax is out-of-manual. 001's opaque-token-stream approach contains this, but `DECLARE CURSOR` and `SELECT … INTO` still need real parsing |
| CPRL scope | 4 | 22 procedures over collation objects that MariaDB does not have. Partial implementation may be worse than none |
