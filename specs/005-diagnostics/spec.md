# Feature Spec: sqlcode, SQLCA, SQLSA, WHENEVER

**ID:** 005-diagnostics · **Status:** Clarifying
**Manual coverage:** §9 pp.9-1..9-18; §5 pp.5-2..5-22 (SQLCA/SQLSA procedures)
**Depends on:** 001, 002, 003

## 1. Problem

Every ESQL/C program's control flow runs through `sqlcode` and `WHENEVER`. Beyond
that, real programs read the `SQLCA` for the up-to-seven error and warning codes
a single statement can produce, and read the `SQLSA` for row counts and access
statistics. They call the display and buffer procedures to render messages, share
structures `EXTERNAL` across modules to save memory, and select structure
versions with `INCLUDE STRUCTURES`.

The structures are the difficulty. They are API — programs index into them,
allocate extra copies using the published length constants, and share them across
translation units — but the manual documents them unevenly. `SQLSA` has a full
field list; `SQLCA` has none, and its content is documented only indirectly
through the `SQLCAGETINFOLIST` item codes in §5. And a substantial fraction of the
`SQLSA` statistics measure NonStop process and disk-process behaviour that has no
MariaDB counterpart.

## 2. Scope

**In scope**

- `sqlcode`: declaration, semantics, and the value classes.
- `WHENEVER`: the three conditions, the four action forms, and the fixed
  precedence order.
- `INCLUDE STRUCTURES`: all version specs, the default-to-version-2 behaviour and
  its informational message, error 11203, and `EXTERNAL`.
- `SQLCA`: declaration, eye-catcher, length, up to seven codes.
- `SQLSA`: declaration, eye-catcher, both lengths, all fields, reset semantics.
- `SQLSA VERSION CURRENT` dual generation and its `SQLGETSYSTEMVERSION`
  dependency.
- The v330+ field-alignment pragma.
- `SQLCADISPLAY`, `SQLCATOBUFFER`, `SQLCAGETINFOLIST`, `SQLCAFSCODE`,
  `SQLSADISPLAY`.
- The SQL message file equivalent.

**Out of scope**

- `SQLDA` → 007
- `SQLGETCATALOGVERSION` / `SQLGETOBJECTVERSION` / `SQLGETSYSTEMVERSION`
  themselves → 008 (005 depends on the last one existing)
- `cextdecs` shimming → 008
- Legacy v1/v2 `SQLSA` layouts from App. D → 008

## 3. Requirements

| ID | Requirement | Citation |
|----|-------------|----------|
| FR-005.1 | `sqlcode` is `0` on success, `100` for not found, negative for errors, and positive non-`100` for warnings. | `[SQLPM/C §9 p.9-6]` |
| FR-005.2 | Not-found conditions can also arrive as specific error numbers rather than 100 — e.g. 8230 (subquery returned no rows) and 8423 (no indicator for a null output) — and these are not remapped to 100. | `[SQLPM/C §9 p.9-6]` |
| FR-005.3 | `WHENEVER` supports the conditions `NOT FOUND`, `SQLERROR`, and `SQLWARNING`. | `[SQLPM/C §9 p.9-6]` |
| FR-005.4 | `WHENEVER` supports the actions `CALL :handler`, `GOTO :label`, `GO TO :label`, and `CONTINUE`. | `[SQLPM/C §9 p.9-6]` |
| FR-005.5 | Generated checks run in the order NOT FOUND, then SQLERROR, then SQLWARNING, with the conditions tested as `sqlcode == 100`, `sqlcode < 0`, and `sqlcode > 0 && sqlcode != 100` respectively. | `[SQLPM/C §9 p.9-6 Table 9-1]` |
| FR-005.6 | A `WHENEVER` directive applies to statements following it in source order and can be re-specified to change or disable the action. | `[SQLPM/C §9 pp.9-6, 9-9]` |
| FR-005.7 | `WHENEVER` applies to DML, DCL, and DDL statements. | `[SQLPM/C §9 p.9-6]` |
| FR-005.8 | `INCLUDE STRUCTURES` accepts `[ALL] VERSION v`, per-structure `VERSION v`, `SQLSA VERSION CURRENT`, and `{SQLCA|SQLSA} [EXTERNAL]`, with mixed specs in one directive. | `[SQLPM/C §9 p.9-2]` |
| FR-005.9 | Accepted versions are 1, 2, 300, 340 or later for all three structures, and additionally 330 for `SQLSA` only. | `[SQLPM/C §9 p.9-2]` |
| FR-005.10 | Omitting `INCLUDE STRUCTURES` generates version 2 structures and emits an informational message to that effect. | `[SQLPM/C §9 p.9-1]` |
| FR-005.11 | Requesting a structure version the implementation cannot generate produces SQL error 11203. | `[SQLPM/C §9 p.9-3]` |
| FR-005.12 | `INCLUDE STRUCTURES` must precede any `INCLUDE SQLCA`/`SQLSA`/`SQLDA`; in a multi-procedure unit it belongs in the global or first-procedure declarations and applies unit-wide. | `[SQLPM/C §9 p.9-1]` |
| FR-005.13 | `EXTERNAL` declares without allocating; exactly one non-`EXTERNAL` declaration of each shared structure must exist in the program, and its absence is diagnosed. | `[SQLPM/C §9 p.9-3]` |
| FR-005.14 | `SQLCA_EYE_CATCHER` is `CA` and `SQLCA_LEN` is 430; the program initialises the eye-catcher. | `[SQLPM/C §9 p.9-12]` |
| FR-005.14a | The `SQLCA` layout is implementation-private and covers all 29 documented item-code contents; direct field access is unsupported and the accessor procedures are the only supported path. | `[SQLPM/C §5 pp.5-11..5-12]` `[DIV-041]` |
| FR-005.15 | The `SQLCA` can carry up to seven error or warning codes from one statement, in any combination. | `[SQLPM/C §9 p.9-12]` |
| FR-005.16 | `SQLSA_EYE_CATCHER` is `SA`; `SQLSA_LEN` is 838 for versions 300–325 and 1790 for version 330 or later. | `[SQLPM/C §9 p.9-14]` |
| FR-005.17 | `SQLSA` is populated after `INSERT`, `UPDATE`, `DELETE`, `SELECT … INTO`, and cursor `OPEN`/`CLOSE`/`FETCH` where the cursor carries a `SELECT`. | `[SQLPM/C §9 p.9-13]` |
| FR-005.18 | `SQLSA` is populated after `PREPARE`, `DESCRIBE`, and `DESCRIBE INPUT`. | `[SQLPM/C §9 p.9-13]` |
| FR-005.19 | `SQLSA` is undefined after DSL, DDL, DCL, and transaction control statements, and the implementation must not make it accidentally meaningful. | `[SQLPM/C §9 p.9-13]` |
| FR-005.20 | Every statement resets `SQLSA`, including every `FETCH`. | `[SQLPM/C §9 pp.9-13, 9-14]` |
| FR-005.21 | `SQLSA` field names and layout match the published declarations for both version families, with **packed** field alignment — the only alignment under which the published 838 and 1790 sizes are reachable. | `[SQLPM/C §9 pp.9-15..9-16]` |
| FR-005.21a | `dml` and `prepare` are arms of a **union**, not coexisting substructures; only the arm matching the last statement's class is meaningful. | `[SQLPM/C §9 pp.9-15..9-16]` |
| FR-005.21b | `table_name` is 24 bytes. In version 300–325 the five `stats[]` counters are 32-bit and `waits`/`escalations` are 16-bit; in version 330+ the counters are 64-bit and `waits`/`escalations` are 32-bit. | `[SQLPM/C §9 pp.9-15..9-16]` |
| FR-005.21c | The VSBB flags exist only in version 330+; in version 300–325 that 4-byte slot is `sqlsa_reserved`. | `[SQLPM/C §9 pp.9-15..9-16]` |
| FR-005.22 | `num_tables` is capped at 16 and `stats[]` carries exactly that many valid entries. | `[SQLPM/C §9 p.9-17]` |
| FR-005.23 | `vsbb_write` and `vsbb_flushed` use `-1` for true and `0` for false. | `[SQLPM/C §9 p.9-17]` |
| FR-005.23a | `SQLCAGETINFOLIST` honours all 29 item codes with their documented sizes, and returns error codes 8510–8517 for the documented failure conditions. | `[SQLPM/C §5 pp.5-11..5-12]` |
| FR-005.23b | Item code 22 reports errors as positive and warnings as negative — the inverse of `sqlcode` — and this inversion is reproduced as published. | `[SQLPM/C §5 p.5-12]` |
| FR-005.24 | `sql_statement_type` uses the published values 1–8 with the `_SQL_STATEMENT_*` names. | `[SQLPM/C §9 p.9-18]` |
| FR-005.25 | `SQLSA` fields with no MariaDB analogue return a documented sentinel, never zero. | `[DIV-011]`, Principle III |
| FR-005.26 | `SQLSA VERSION CURRENT` generates both version 300 and 330 layouts and emits the `SQLGETSYSTEMVERSION` call the option implies. | `[SQLPM/C §9 p.9-2]` |
| FR-005.27 | Version 330 or later `SQLSA` carries the field-alignment pragma over the four `*_R330` types. | `[SQLPM/C §9 p.9-14]` |
| FR-005.28 | `SQLCADISPLAY` writes error and warning messages to a file or terminal. | `[SQLPM/C §5 p.5-3]` |
| FR-005.29 | `SQLCATOBUFFER` writes those messages into a program-supplied record area. | `[SQLPM/C §5 p.5-14]` |
| FR-005.30 | `SQLCAGETINFOLIST` returns a caller-selected subset of `SQLCA` information, honouring the published item codes and returning the published error codes. | `[SQLPM/C §5 pp.5-9..5-13]` |
| FR-005.31 | `SQLCAFSCODE` returns file-system, disk-process, and operating-system error detail. | `[SQLPM/C §5 p.5-8]` |
| FR-005.32 | `SQLSADISPLAY` writes the statistics to a file or terminal, covering the published display elements. | `[SQLPM/C §5 pp.5-20..5-22]` |
| NFR-005.1 | Every generated structure version carries `_Static_assert` on `sizeof` and on every externally visible `offsetof`. | Principle VI |
| NFR-005.2 | The `WHENEVER` precedence order has a test that fails if the three checks are reordered. | Principle IV |

## 4. Acceptance scenarios

### AS-005.1 — WHENEVER precedence
- **Given** a statement that produces both an error and a warning, and another
  that produces both a warning and not-found
- **When** all three `WHENEVER` conditions are active with distinguishable
  handlers
- **Then** the error handler runs for the first and the not-found handler for the
  second — matching the published order
- **Test:** `tests/conformance/005/whenever_precedence.sqlc`

### AS-005.2 — WHENEVER disable and re-enable
- **Given** `WHENEVER SQLERROR CALL`, two failing statements, `WHENEVER SQLERROR
  CONTINUE`, two more failing statements
- **When** run
- **Then** the handler runs exactly twice
- **Test:** `tests/conformance/005/whenever_toggle.sqlc`

### AS-005.3 — Structure sizes
- **Given** every supported `INCLUDE STRUCTURES` version spec
- **When** compiled
- **Then** `sizeof` matches: `SQLCA` 430; `SQLSA` 838 for 300–325 and 1790 for
  330+
- **Test:** `tests/conformance/005/structure_sizes.sqlc`

### AS-005.4 — Default version and its message
- **Given** a program with `INCLUDE SQLCA` and no `INCLUDE STRUCTURES`
- **When** preprocessed
- **Then** version 2 structures are generated and the informational message is
  emitted
- **Test:** `tests/conformance/005/default_version2.sqlc`

### AS-005.5 — EXTERNAL sharing
- **Given** three translation units, two declaring `SQLCA EXTERNAL` and one
  declaring it normally
- **When** compiled and linked
- **Then** it links, and one `SQLCA` is allocated
- **Test:** `tests/conformance/005/external_share/`
- **And Given** all three declared `EXTERNAL`
- **Then** a diagnostic, not a link error
- **Test:** `tests/conformance/005/negative/external_no_definition/`

### AS-005.6 — SQLSA reset on every FETCH
- **Given** a cursor over three rows
- **When** each `FETCH` is followed by reading `records_used`
- **Then** each read reflects only that `FETCH`, and an accumulator is required to
  total them
- **Test:** `tests/conformance/005/sqlsa_reset.sqlc`

### AS-005.7 — Sentinels, not zeros
- **Given** any DML statement
- **When** the unmappable `SQLSA` fields are read
- **Then** each holds the documented sentinel, distinguishable from a genuine zero
- **Test:** `tests/conformance/005/sqlsa_sentinels.sqlc`

### AS-005.8 — Seven codes
- **Given** a statement engineered to produce multiple diagnostics
- **When** `SQLCAGETINFOLIST` is called
- **Then** up to seven codes are retrievable in the order SQL/MP would report them
- **Test:** `tests/conformance/005/sqlca_multiple_codes.sqlc`

## 5. Diagnostics

| Code | Condition | Default policy | Citation |
|------|-----------|----------------|----------|
| `ESQLC-5001` | `INCLUDE STRUCTURES` after an `INCLUDE SQLCA`/`SQLSA`/`SQLDA` | error | `[§9 p.9-1]` |
| `ESQLC-5002` | Unsupported structure version requested — reported as SQL error 11203 | error | `[§9 p.9-3]` |
| `ESQLC-5003` | Version 330 requested for `SQLCA` or `SQLDA` | error | `[§9 p.9-2]` |
| `ESQLC-5004` | `SQLCA`/`SQLSA` declared `EXTERNAL` everywhere, no definition | error | `[§9 p.9-3]` |
| `ESQLC-5005` | More than one non-`EXTERNAL` definition of a shared structure | error | `[§9 p.9-3]` |
| `ESQLC-5006` | `INCLUDE STRUCTURES` omitted — version 2 assumed | info | `[§9 p.9-1]` |
| `ESQLC-5007` | `SQLSA VERSION CURRENT` without a reachable `SQLGETSYSTEMVERSION` | error | `[§9 p.9-2]` |
| `ESQLC-5008` | `WHENEVER` action references an undeclared identifier | error | `[§9 p.9-6]` |
| `ESQLC-5009` | `SQLSA` read after a statement class that leaves it undefined | warn | `[§9 p.9-13]` |
| `ESQLC-5010` | Reading an `SQLSA` field that holds a sentinel | ignore | `[DIV-011]` |

`ESQLC-5009` is a warning rather than an error because the manual says
"undefined", not "prohibited", and legacy programs may read it harmlessly.
`ESQLC-5010` defaults to `ignore` because it cannot change a result — but it is
present so a debugging build can surface it.

## 6. Open questions

| # | Question | Blocks | Resolution |
|---|----------|--------|------------|
| Q1 | `SQLCA` field layout — the manual never gives one. | FR-005.14, NFR-005.1 | **RESOLVED** — `DIV-041`. No layout exists anywhere in the manual, so no conforming program can depend on one. Define a private 430-byte layout covering the 29 documented item codes; the accessor procedures are the only supported path |
| Q2 | `SQLSA` field offsets. | FR-005.21, NFR-005.1 | **RESOLVED** — §9 pp.9-15..9-16 publish both layouts, and they validate exactly to 838 and 1790 under packed alignment. Offsets fully determined |
| Q3 | Which sentinel values for the unmappable `SQLSA` fields? | FR-005.25 | unresolved — and now known to need **per-width** values, since the counters are 32-bit pre-330 and 64-bit in v330+ |
| Q4 | `sqlcode` values for the §2 conversion warnings (shared with 002 Q1). | FR-005.1 | unresolved — `DIV-042`. The only one of the three original conflicts this manual cannot settle |
| Q5 | Does `WHENEVER` apply to dynamic SQL statements? §9 names DML, DCL, and DDL only. | FR-005.7 | unresolved — decide with 007 |
| Q6 | What does the SQL message file become — a bundled message catalogue, or MariaDB's own messages? Affects `SQLCADISPLAY` output text. | FR-005.28, .32 | unresolved |
| Q7 | Item code 22 reports errors positive and warnings negative, inverting `sqlcode`. Is that inversion faithful to reproduce, or a manual defect? | FR-005.30 | unresolved — reproduce as published pending evidence, and pin it with a test in both directions |
| Q8 | Who declares `sqlcode` — the program, or the preprocessor? §10 p.10-23 lists "declare the sqlcode variable" as a program development step, implying the program owns it; §9 discusses checking it without ever saying who declares it. Its type is also unstated. | FR-005.1 | unresolved — narrowed for Gate 1 by a slice decision (program-declared `long`) |

Q1 and Q2 are closed, and the earlier assessment that "the structures cannot be
implemented byte-exactly from the manual alone" was wrong for SQLSA: the layouts
are published in §9's worked examples, which I had not read when the spec was
first written. The arithmetic confirms them independently.

SQLCA genuinely has no published layout, but that turns out to be liberating
rather than blocking — the absence means no conforming program can be indexing
it, so choosing a layout breaks nothing that was ever specified. Holding the
total at 430 preserves what programs do rely on: `SQLCA_LEN` allocation and
`EXTERNAL` sharing.

## 7. Constitution check

| Principle | Compliant? | Note |
|-----------|-----------|------|
| I manual is the contract | partial | Q1 and Q2 resolved from §9 pp.9-15..9-16 and §5 pp.5-11..5-12. Q3–Q7 remain open; Q4 needs an external document |
| II source compatibility | yes | All directive forms and identifier names preserved |
| III no silent semantic change | yes | FR-005.25 forbids zero-filling; FR-005.19 forbids making an undefined structure accidentally meaningful |
| IV manual-derived tests first | yes | Eight scenarios; NFR-005.2 guards the precedence order specifically |
| V layered / frozen ABI | yes | Structures are runtime-owned; the preprocessor emits declarations and `esqlc_*` accessors |
| VI byte-exact structures | yes | `SQLSA` fully determined and size-validated for both version families. `SQLCA` size exact at 430; its layout is private by `DIV-041`, which breaks nothing the manual specified |
| VII divergence registered | yes | `DIV-011` (updated with layout findings), `DIV-041` accepted, `DIV-042` proposed for the warning codes |
