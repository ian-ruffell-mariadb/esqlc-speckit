# Feature Spec: Static DML & cursors

**ID:** 004-static-dml-cursors · **Status:** Clarifying
**Manual coverage:** §4 pp.4-4..4-24 (except PAID, VSBB, 8204, foreign cursors → 008)
**Depends on:** 001, 002, 003

## 1. Problem

This is the feature customers actually use. Section 4 covers the single-row
`SELECT … INTO`, the multirow `SELECT` via cursor, `INSERT`, `UPDATE`, `DELETE`,
and the full cursor lifecycle — `DECLARE CURSOR`, `OPEN`, `FETCH`, `CLOSE`,
plus `UPDATE`/`DELETE … WHERE CURRENT OF`. Getting the statement forms right is
the easy half.

The hard half is cursor position. The manual specifies exactly where a cursor
sits after each operation and in each outcome, including after a failed `FETCH`,
after an `UPDATE` through the cursor, and at end of set. Programs loop on these
rules. An implementation that is one row off, or that leaves the position
undefined where the manual defines it, produces silently wrong output from
correct-looking code.

## 2. Scope

**In scope**

- Single-row `SELECT … INTO` with host variables, by column value and by primary
  key.
- Multirow `SELECT` through a cursor.
- `INSERT`: single row, null values via indicators, timestamp values.
- `UPDATE`: single row, multiple rows, setting columns to null.
- `DELETE`: single row, multiple rows.
- `DECLARE CURSOR` (static), `OPEN`, `FETCH`, `CLOSE`.
- `UPDATE` and `DELETE` positioned through a cursor.
- Cursor position after every operation and outcome.
- Cursor stability semantics.
- The prescribed order of cursor operations.

**Out of scope**

- PAID / process access requirements → 008
- VSBB → 008
- SQL error 8204 (lost open) and its recovery → 008
- Foreign cursors → 008
- `sqlcode` / `SQLCA` / `SQLSA` mechanics → 005 (004 consumes them)
- Dynamic cursors → 007
- Locking statements, `CONTROL TABLE` → 008

## 3. Requirements

| ID | Requirement | Citation |
|----|-------------|----------|
| FR-004.1 | A single-row `SELECT` places the selected column values into the listed host variables. | `[SQLPM/C §4 p.4-4]` |
| FR-004.2 | A single-row `SELECT` returning no rows sets `sqlcode` to 100. | `[SQLPM/C §9 p.9-6]` |
| FR-004.3 | A single-row `SELECT` matching more than one row is an error, and host variables are not modified. | `[EXTERNAL — SQLRM]` |
| FR-004.4 | `INSERT` inserts one or more rows into a table or protection view. | `[SQLPM/C §4 p.4-8]` |
| FR-004.5 | `INSERT` with a negative indicator stores a null. | `[SQLPM/C §4 p.4-9]`, `[SQLPM/C §2 p.2-17]` |
| FR-004.6 | `INSERT` of a timestamp value is supported through a character host variable with `TYPE AS`. | `[SQLPM/C §4 p.4-10]` |
| FR-004.7 | `UPDATE` without a cursor updates one or many rows per its `WHERE` clause. | `[SQLPM/C §4 pp.4-10..4-12]` |
| FR-004.8 | `UPDATE` can set a column to null via a negative indicator. | `[SQLPM/C §4 p.4-12]` |
| FR-004.9 | `DELETE` without a cursor deletes one or many rows per its `WHERE` clause. | `[SQLPM/C §4 pp.4-12..4-13]` |
| FR-004.10 | An `UPDATE` or `DELETE` affecting no rows sets `sqlcode` to 100. | `[SQLPM/C §9 p.9-6]` |
| FR-004.11 | `DECLARE CURSOR` in declaration position associates a cursor name with a `SELECT`, `UPDATE`, or `DELETE`. | `[SQLPM/C §4 p.4-18]`, `[SQLPM/C §3 p.3-2]` |
| FR-004.12 | `OPEN` runs the associated statement and positions the cursor before the first row. | `[SQLPM/C §10 p.10-2]`, `[SQLPM/C §4 p.4-19]` |
| FR-004.13 | `FETCH` advances to the next row and copies values into host variables. | `[SQLPM/C §4 p.4-20]` |
| FR-004.14 | `FETCH` past the last row sets `sqlcode` to 100 and leaves host variables unmodified. | `[SQLPM/C §9 p.9-6]`, `[SQLPM/C §4 p.4-16]` |
| FR-004.15 | `CLOSE` terminates the cursor and frees its result table. | `[SQLPM/C §4 p.4-24]`, `[SQLPM/C §10 p.10-2]` |
| FR-004.16 | Cursor position follows the manual's position table where it constrains behaviour: `OPEN` positions before the first row; `FETCH` positions at the retrieved row (or leaves the current position); `CLOSE` leaves no position and releases the result table; `SELECT` fixes the row order only when `ORDER BY` is given, and the order is otherwise undefined. | `[SQLPM/C §4 p.4-16 Table 4-2]` |
| FR-004.16a | After a positioned `DELETE` the cursor is between rows — the manual permits either "between rows" or "before the next row and after the preceding row", so this implementation picks one, documents it, and stays consistent. | `[SQLPM/C §4 p.4-16]` `[DIV-051]` |
| FR-004.16b | Row order for a cursor `SELECT` without `ORDER BY` is explicitly undefined and must not be relied on by any conformance test. | `[SQLPM/C §4 p.4-16]` |
| FR-004.17 | `UPDATE … WHERE CURRENT OF` updates the row the cursor is positioned on. | `[SQLPM/C §4 p.4-22]` |
| FR-004.18 | `DELETE … WHERE CURRENT OF` deletes the row the cursor is positioned on, and the resulting position follows the position table. | `[SQLPM/C §4 pp.4-16, 4-23]` |
| FR-004.19 | Operating on a cursor out of order — `FETCH` before `OPEN`, `OPEN` twice, positioned update with no current row — is an error, not undefined behaviour. | `[SQLPM/C §4 p.4-15]` |
| FR-004.20 | Cursor stability semantics are documented and implemented, or the divergence is registered. | `[SQLPM/C §4 p.4-17]` |
| NFR-004.1 | Every row of the manual's cursor position table has a dedicated test, and every position the table leaves unspecified (Q6, Q7) has a test pinning **this implementation's** documented choice, marked as such so it is never mistaken for manual-derived behaviour. | Principle IV |
| NFR-004.2 | Every statement form in §4 has a test against the App. A sample schema. | Principle IV |

## 4. Acceptance scenarios

### AS-004.1 — Position table exhaustively
- **Given** a cursor over a known three-row result set
- **When** each sequence in the manual's position table is executed
- **Then** the observable position — what the next `FETCH` returns, or whether a
  positioned update succeeds — matches the table in every case
- **Test:** `tests/conformance/004/cursor_position/*.sqlc`

### AS-004.2 — Not-found is 100, not an error
- **Given** a `SELECT … INTO`, an `UPDATE`, a `DELETE`, and a `FETCH`, each
  matching nothing
- **When** run
- **Then** `sqlcode` is exactly 100 in all four cases and host variables are
  unmodified
- **Test:** `tests/conformance/004/not_found.sqlc`

### AS-004.3 — Positioned update and delete
- **Given** a cursor positioned on row 2 of 3
- **When** `UPDATE … WHERE CURRENT OF`, then `FETCH`
- **Then** row 2 is modified and the `FETCH` returns row 3
- **Test:** `tests/conformance/004/positioned_update.sqlc`

### AS-004.4 — Out-of-order cursor use
- **Given** `FETCH` before `OPEN`, a double `OPEN`, and a positioned `DELETE`
  with no current row
- **When** run
- **Then** each produces an error `sqlcode`, and none corrupts data
- **Test:** `tests/conformance/004/negative/cursor_order.sqlc`

### AS-004.5 — Null round trip through DML
- **Given** a nullable column
- **When** inserted null, updated to non-null, updated back to null, and selected
  each time
- **Then** indicators are `-1` / `0` / `-1` respectively
- **Test:** `tests/conformance/004/null_dml.sqlc`

## 5. Diagnostics

| Code | Condition | Default policy | Citation |
|------|-----------|----------------|----------|
| `ESQLC-4001` | `FETCH` on a cursor that is not open | error | `[§4 p.4-15]` |
| `ESQLC-4002` | `OPEN` on an already-open cursor | error | `[§4 p.4-15]` |
| `ESQLC-4003` | `CLOSE` on a cursor that is not open | error | `[§4 p.4-15]` |
| `ESQLC-4004` | Positioned `UPDATE`/`DELETE` with no current row | error | `[§4 p.4-16]` |
| `ESQLC-4005` | Cursor name not declared | error | `[§4 p.4-18]` |
| `ESQLC-4006` | Duplicate cursor name in scope | error | `[§4 p.4-18]` |
| `ESQLC-4007` | Single-row `SELECT` matched multiple rows | error | `[EXTERNAL — SQLRM]` |
| `ESQLC-4008` | Host variable list length does not match the select list | error | Principle III |
| `ESQLC-4009` | Null retrieved into a host variable with no indicator | error | `[§9 p.9-6]` (cf. SQL error 8423) |

`ESQLC-4009` mirrors SQL error 8423, which the manual cites as a case where
SQL/MP returns something other than 100 for a not-found-like condition. Reuse the
8423 value rather than inventing one.

## 6. Open questions

| # | Question | Blocks | Resolution |
|---|----------|--------|------------|
| Q1 | Exact contents of the manual's cursor position table (Table 4-2). | FR-004.16 | **RESOLVED, and the answer changes the requirement.** Transcribed below. The table has five rows and is materially *vaguer* than this spec assumed — it constrains far less than FR-004.16 originally claimed |
| Q2 | What does SQL/MP do when a single-row `SELECT` matches many rows? | FR-004.3 | unresolved, `[EXTERNAL — SQLRM]` |
| Q3 | Cursor stability: which SQL/MP access modes exist, and which MariaDB isolation level is the closest honest match for each? | FR-004.20 | unresolved — likely a divergence per mode |
| Q4 | Does `CLOSE` outside a transaction differ from `CLOSE` inside one, given `COMMIT WORK` frees cursors? | FR-004.15, FR-003.8 | unresolved |
| Q5 | Are cursors declared in declaration position scoped to the compilation unit or the function? | FR-004.11, `ESQLC-4006` | unresolved |
| Q6 | Where does the cursor sit after a `FETCH` past the last row? Table 4-2 does not say, and FR-004.14 currently asserts an answer the manual does not give. | FR-004.14 | unresolved — `[EXTERNAL — SQLRM]`. Must become a documented choice or a divergence |
| Q7 | Where does the cursor sit after a positioned `UPDATE`? Table 4-2 omits `UPDATE` entirely, yet AS-004.3 asserts the row stays current and the next `FETCH` advances. | FR-004.17 | unresolved — `[EXTERNAL — SQLRM]`. Same treatment as Q6 |
| Q8 | Cursor PAID rules are finer than assumed: read access is tested on the objects in the cursor's `SELECT` at `OPEN`, write access only on `DELETE`, and a cursor without `FOR UPDATE` may still locate rows to delete. Does `IN EXCLUSIVE MODE` need modelling? | FR-004.20, 008 FR-008.14 | unresolved — `[SQLPM/C §4 p.4-16]` |

Q1 is not really a question — it is a task. It sits here because the position
table is the single highest-risk item in this feature and must not be
paraphrased from memory during implementation.

## 7. Constitution check

| Principle | Compliant? | Note |
|-----------|-----------|------|
| I manual is the contract | partial | Q1 must be closed by transcription; Q2 is `[EXTERNAL]` |
| II source compatibility | yes | No statement form is altered or restricted |
| III no silent semantic change | yes | Nine diagnostics; out-of-order cursor use errors rather than being undefined |
| IV manual-derived tests first | yes | NFR-004.1 requires one test per position-table row |
| V layered / frozen ABI | yes | Cursor state lives in the runtime; the preprocessor emits `esqlc_*` cursor calls |
| VI byte-exact structures | n/a | No SQL structures generated here |
| VII divergence registered | partial | Q3 will produce one per access mode |
