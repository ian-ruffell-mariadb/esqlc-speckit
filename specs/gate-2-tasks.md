# Tasks: Gate 2 (retrieval)

**Slice:** [gate-2.md](gate-2.md) · **Plan:** [gate-2-plan.md](gate-2-plan.md)

Rules in force: tests before implementation (Principle IV); every Phase C task
names the Phase B task it makes pass; `[P]` only where tasks touch disjoint files
with no dependency between them; every task lists requirement IDs.

Task IDs start at T200 to stay distinct from Gate 1's T0xx.

Phase D covers the four diagnostics this slice reaches. Everything else out of
scope keeps its `ESQLC-1012` handler entry naming the owning feature.

**Gate 1's fixtures stay in the suite throughout.** They are the regression
guard for the `INSERT INTO` landmark risk, which is the sharpest hazard in this
slice — see T220 and the plan's risk table.

---

## Implementation status — 2026-08-28

**Gate 2 is green end to end.** Branch `gate-2-implementation`.
`ctest` is 7/7 with a server and 6/6 under `-DESQLC_NO_MARIADB=ON`; Tier 2 is
13/13; spec assertions 38/38; diagnostic registry clean at 20 codes.

**Principle IV was honoured this time.** Every Phase B test was written and run
*before* any Phase C work, and the pre-implementation failures were the right
ones — `ESQLC-1012: 'SELECT' is not implemented in this slice; owned by feature
004` across all five Tier 2 fixtures, with the negatives failing on missing
codes rather than passing vacuously. That is the difference from Gate 1, where
code came first and the golden files were snapshotted after the fact.

### Mutation checks — all four caught

| Check | Injected defect | Result |
|---|---|---|
| T240 | null-terminate the fetch buffer | `select_into` failed with `\|00` where `\|AA` was required — the terminator byte overwritten. Only that test failed |
| T241 | write to output buffers on the no-row path | `not_found_untouched` failed, only that test |
| T242 | swap two output descriptors | `select_into` failed — **broader than predicted**, taking two other cases with it, but fired correctly |
| T243 | let the `INTO` landmark leak into `INSERT` | the Gate 1 direction guard failed, naming both `part_num` and `part_desc` |

### Defects found by building

1. **`#line` emitted mid-line was invalid C.** When `EXEC SQL` follows other code
   on the same source line — `{ long s = sqlcode; EXEC SQL ROLLBACK WORK; }` —
   the emitter wrote the directive mid-line, where only whitespace may precede
   `#`. Gate 1's fixtures all began statements at column 1, so it never showed.
   Fixed by tracking line position and prefixing a newline when needed.
2. **Two fixtures checked `sqlcode` after `COMMIT WORK`**, which resets it like
   every statement does. `not_found` failed outright; `null_indicator` passed by
   accident, since it expected 0 and the commit supplied one. Both now capture
   immediately after the `SELECT`. This is the third time this trap has bitten,
   always in test code — §9 p.9-13 warns about it explicitly.
3. **Gate 1's `unimplemented.sqlc` silently changed meaning.** It used `SELECT`
   as its example of an unimplemented statement; once `SELECT` was implemented
   it began testing `ESQLC-1014` instead of `FR-001.15`. Repointed at `UPDATE`.
   T207 existed because this was anticipated.

### Found and recorded, not fixed

- **004 Q9** — the dispatch table's `DECLARE CURSOR` entry is dead code. Real
  syntax is `DECLARE <name> CURSOR FOR …`, with the name between the two words,
  so the multi-word matcher never fires and a real cursor declaration yields
  `ESQLC-1009` rather than `ESQLC-1012`. Cursor syntax belongs to feature 004,
  so this was noted rather than folded in, and T207's fixture uses `FETCH`.

### Deviations

- The golden `.expected.c` files were re-baselined after the `#line` fix. The
  diff was inspected first and confirmed to be exactly twelve directives moving
  to line-start with no semantic change. The spec assertions — which are the
  specification tests — passed throughout, which is what made re-baselining safe.

---

## Phase A — fixtures and harness

| ID | Task | Reqs | Deps |
|----|------|------|------|
| T200 | Extend the schema fixture with `weight SMALLINT NULL`, and seed one row with a non-null weight and one with a null | — | — |
| T201 | `rt/select_into.sqlc` — the happy path exactly as the slice lists it | — | T200 |
| T202 [P] | `rt/not_found.sqlc` — same query, a `part_num` that does not exist; every host variable pre-poisoned with a sentinel before the call | FR-004.2 | T200 |
| T203 [P] | `rt/null_indicator.sqlc` — selects the null weight with an indicator supplied | FR-002.16 | T200 |
| T204 [P] | `rt/negative/null_no_indicator.sqlc` — selects the null weight with no indicator | FR-005.2 | T200 |
| T205 [P] | `rt/negative/cross_family.sqlc` — a `char` host variable against the `SMALLINT` column | FR-002.22 | T200 |
| T206 [P] | `negative/undeclared_into.sqlc` + `.expected.diag` — a reference in the `INTO` list with no declaration | FR-001.25 | — |
| T207 [P] | `negative/unimplemented_cursor.sqlc` + `.expected.diag` — `DECLARE CURSOR`, to keep a live `ESQLC-1012` case after `SELECT` leaves that table | FR-001.15 | — |
| T208 [P] | `indicator_forms.sqlc` — `:weight INDICATOR :ind` and `:weight :ind` in separate statements | FR-002.15 | — |
| T209 | Poison convention: document and apply a single sentinel-fill helper across the Tier 2 fixtures, so "the runtime must not write here" is falsifiable rather than assumed | FR-002.28, FR-004.2 | T202 |
| T210 | Extend `run_tier2.sh` with the new case scaffolding and second-connection seeding for the null row | — | T200 |

## Phase B — failing tests

All must fail for the right reason before any Phase C task starts.

### Tier 1 — preprocessor, no database

| ID | Task | Reqs | Deps |
|----|------|------|------|
| T220 | `select_hostvar_dirs` — references inside the `INTO` region are `ESQLC_DIR_OUT`, all others `ESQLC_DIR_IN`, **and every reference in Gate 1's `INSERT` fixtures is still `DIR_IN`**. The second half is the regression guard for the `INSERT INTO` landmark hazard | FR-001.16 | T201 |
| T221 [P] | `opaque_body_select` — the body reaches the handler intact; only `INTO` and `FROM` offsets are noted, nothing else is interpreted | NFR-001.1 | T201 |
| T222 [P] | `indicator_forms` — both syntaxes emit an identical `ind_addr`, and omitting an indicator emits `0` | FR-002.15 | T208 |
| T223 [P] | `negative/undeclared_into` fires `ESQLC-1014` at the right line and column | FR-001.25 | T206 |
| T224 [P] | `negative/unimplemented_cursor` fires `ESQLC-1012` naming feature 004 | FR-001.15 | T207 |
| T225 | Extend `spec_assertions.py`: `INTO`-region references carry `DIR_OUT`; `ind_addr` is the declared indicator's address where supplied and `0` where not; the emitted statement still contains no `:` reference | FR-001.16, FR-003.10 | T220 |

### Tier 2 — runtime, live MariaDB

| ID | Task | Reqs | Deps |
|----|------|------|------|
| T230 [P] | `rt/select_into` — `part_desc` and `weight` receive their column values, `weight_ind` is `0`, `sqlcode` is `0` | FR-003.12, FR-004.1 | T210 |
| T231 [P] | `rt/no_terminator` — bytes 0–17 are column data and **byte 18 still holds the sentinel** | FR-002.28 | T209 |
| T232 [P] | `rt/not_found_untouched` — `sqlcode` exactly 100 and every host variable byte-identical to its poisoned contents | FR-003.13, FR-004.2, FR-005.1 | T209 |
| T233 [P] | `rt/null_indicator` — indicator is exactly `-1` and the value variable is left alone | FR-002.16 | T203 |
| T234 [P] | `rt/null_no_indicator` — negative `sqlcode` (8423), not a zero-filled value, and **not remapped to 100** | FR-005.2 | T204 |
| T235 [P] | `rt/char_padding_retrieval` — an 18-byte value ending in blanks arrives as 18 bytes | `DIV-052` | T210 |
| T236 [P] | `rt/select_parameterised` — a `WHERE` input containing SQL metacharacters matches literally and executes nothing | FR-003.10 | T210 |
| T237 [P] | `rt/negative/cross_family` — refused, not coerced | FR-002.22 | T205 |

### Mutation checks — prove the guards are load-bearing

Each injects the specific defect its guard exists to catch, confirms exactly that
guard fails, and reverts. Gate 1's practice, which caught three real defects.

| ID | Task | Guards | Deps |
|----|------|--------|------|
| T240 | Null-terminate the fetch buffer within `width` — must fail T231 and nothing else | FR-002.28 | T231 |
| T241 | Bind output buffers on execute rather than on fetch — must fail T232 | FR-004.2 | T232 |
| T242 | Swap two output descriptors — must fail T230. Detectable only because the fixture selects a `char` and a `short`, not two integers | FR-004.1 | T230 |
| T243 | Apply landmark classification to `INSERT` as well as `SELECT` — must fail T220's Gate 1 half | FR-001.16 | T220 |

## Phase C — implementation

| ID | Task | Reqs | Makes pass | Deps |
|----|------|------|-----------|------|
| T250 | `src/pp/scan.cc` — record `INTO` and `FROM` byte offsets during the existing single pass, respecting strings and comments as the span capture already does | NFR-001.1 | T221 | Phase B |
| T251 | `src/pp/dispatch.cc` — `SELECT` becomes an implemented handler; every cursor verb keeps its `ESQLC-1012` entry naming feature 004 | FR-001.15 | T224 | T250 |
| T252 | `src/pp/emit.cc` — `SELECT` handler: classify each reference by landmark, **applied only in this handler**, and placeholderise inputs as before | FR-001.16, FR-003.10 | T220, T225 | T251 |
| T253 | `src/pp/emit.cc` — emit `ind_addr` as the declared indicator's address, or `0` when none was supplied | FR-002.15 | T222 | T252 |
| T254 | `src/pp/emit.cc` — a reference in the `INTO` list with no declaration is diagnosed, and the statement is not emitted | FR-001.25 | T223 | T252 |
| T255 | `src/rt/context.c` — append `PAD_CHAR_TO_FULL_LENGTH` to the session `sql_mode` at connect, never replacing it | `DIV-052` | T235 | Phase B |
| T256 | `src/rt/exec.c` — fetch result metadata; assert column count equals the output-descriptor count | FR-003.12, FR-004.1 | T230 | T255 |
| T257 | `src/rt/exec.c` — bind output buffers with `buffer_length` set to `width`, never `capacity`, so the library cannot reach the terminator byte | FR-002.28 | T231 | T256 |
| T258 | `src/rt/exec.c` — fetch exactly one row, and only when a row exists, so buffers stay untouched otherwise | FR-004.2 | T232 | T257 |
| T259 | `src/rt/exec.c` — write `-1` to the indicator for a null column and `0` otherwise | FR-002.16 | T233 | T258 |
| T260 | `src/rt/exec.c` — a null column with a `NULL` `ind_addr` yields 8423 rather than a zeroed variable | FR-005.2 | T234 | T259 |
| T261 | `src/rt/exec.c` — refuse a result column whose family disagrees with its descriptor | FR-002.22 | T237 | T256 |
| T262 | `src/rt/diag.c` — `sqlcode` 100 when no row is returned; 8423 passed through rather than remapped to 100 | FR-003.13, FR-005.1 | T232, T234 | T258 |

## Phase D — diagnostics

| ID | Task | Code | Reqs | Deps |
|----|------|------|------|------|
| T270 [P] | Cursor verb with no handler in this slice, naming feature 004 | `ESQLC-1012` | FR-001.15 | T251 |
| T271 [P] | Reference in the `INTO` list with no declaration | `ESQLC-1014` | FR-001.25 | T254 |
| T272 [P] | Cross-family conversion between a result column and its descriptor | `ESQLC-2004` | FR-002.22 | T261 |
| T273 [P] | Null retrieved into a host variable with no indicator | `ESQLC-4009` | FR-005.2 | T260 |

## Phase E — documentation and registry

| ID | Task | Reqs | Deps |
|----|------|------|------|
| T280 | Mark the slice's requirement rows in `docs/traceability.md` as `tested` — this subset only, not whole sections | — | Phase D |
| T281 | Move `DIV-052` from `proposed` to `accepted`, recording that option 1 was implemented and criterion 3 is its detector | — | T255, T235 |
| T282 | Re-examine SD-1 and SD-2 against what was built; record any drift as a defect, not as precedent | — | Phase C |
| T283 | Reconcile the slice's non-proof list against the as-built state, and state plainly what a green Gate 2 still does not demonstrate | — | Phase D |
| T284 | Confirm `diag_registry` is clean — `ESQLC-2004` and `ESQLC-4009` must be registered before they are emitted | — | Phase D |
| T285 | Record the `INTO`-landmark limit and whether it survives cursor `FETCH … INTO`, as an explicit input to Gate 3 rather than an assumption | — | T252 |
| T286 | Run `/speckit.analyze` including the Principle VIII slice checks | — | T280–T285 |

## Critical path

Two chains, converging on the live criteria:

```
Preprocessor  T200 → T201 → T220 → T250 → T251 → T252 → T253/T254
Runtime       T200 → T210 → T255 → T256 → T257 → T258 → T259 → T260
Converge      → live criteria T230–T237 → mutation checks T240–T243 → T283
```

The runtime chain is now the longer one at eight sequential tasks — the reverse
of Gate 1, where the preprocessor dominated. `T256` (result metadata) is the most
blocking single task: `T257`, `T261` and everything downstream of the fetch
depend on it.

`T250` remains the riskiest rather than the most blocking. It widens the
scanner's SQL awareness, and `T243` exists specifically to prove the widening
did not leak into `INSERT`.

## Exit criteria

- [ ] All seven slice criteria demonstrated — T230–T237
- [ ] Byte 18 sentinel survives retrieval — T231
- [ ] Host variables byte-identical after a not-found — T232
- [ ] Trailing blanks survive retrieval — T235
- [ ] Null with indicator gives exactly `-1`; without, gives 8423 — T233, T234
- [ ] All four mutation checks fail their intended guard and only that guard — T240–T243
- [ ] Gate 1's fixtures still pass unchanged — T220
- [ ] Tier 1 green with no MariaDB present; `diag_registry` clean — T284
- [ ] `DIV-052` accepted with its detector named — T281
- [ ] `/speckit.analyze` clean, slice checks included — T286
