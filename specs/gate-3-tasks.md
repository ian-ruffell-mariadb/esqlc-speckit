# Tasks: Gate 3 (read-only cursors)

**Slice:** [gate-3.md](gate-3.md) · **Plan:** [gate-3-plan.md](gate-3-plan.md)

Rules in force: tests before implementation (Principle IV); every Phase C task
names the Phase B task it makes pass; `[P]` only where tasks touch disjoint files
with no dependency between them; every task lists requirement IDs.

Task IDs start at T300, distinct from Gate 1's T0xx and Gate 2's T2xx.

Phase D covers the five cursor diagnostics. Out-of-slice cursor forms — `FOR
UPDATE`, positioned `UPDATE`/`DELETE` — keep `ESQLC-1012` entries naming
feature 004.

**Gates 1 and 2 fixtures stay in the suite throughout.** Gate 2 showed why: its
`SELECT` implementation silently changed what Gate 1's `unimplemented.sqlc` was
testing. T311 exists to keep a live `ESQLC-1012` case once four more verbs
become implemented.

---

## Implementation status — 2026-08-29

**Gate 3 is green end to end.** Branch `gate-3-implementation`.
`ctest` 8/8 with a server, 7/7 under `-DESQLC_NO_MARIADB=ON`; Tier 2 19/19;
negatives 11/11; diagnostic registry clean at 26 codes.

**Principle IV honoured.** Phase B failures were captured before any Phase C
work: four preprocessor negatives failing (the Q9 defect visible live as
`ESQLC-1009 unrecognised 'DECLARE'` in three of them) and six Tier 2 cursor
cases failing to build, with Gates 1 and 2's thirteen still passing.

### Mutation checks

| Check | Result |
|---|---|
| T350 stale parameter snapshot | failed `open_binds` **only** |
| T351 success instead of 100 when exhausted | failed `fetch_exhausted` **only** |
| T352 null-terminate the fetch buffer | failed `cursor_loop` **only** |
| T353 break the cursor type | failed `streaming_guard` — **and one functional test**, see below |

### Two defects in my own verification

Both worth recording, because a guard that cannot fail is worse than no guard.

1. **A mutation that did not mutate.** The first T353 attempt used `perl` without
   `/g`, so it replaced the *comment* mention of `STMT_ATTR_CURSOR_TYPE` and left
   the actual call untouched. The guard "passed" against unmutated code.
2. **A guard that grepped comments.** `streaming_guard` searched the whole file,
   so a comment mention satisfied it. Hardened to strip comments and to require
   the `mysql_stmt_attr_set` call itself. This is the same defect as Gate 2's
   FR-003.1 check matching identifiers inside comments — third occurrence of
   that pattern.

### T353's prediction was too strong

The plan asserted the streaming defect is invisible to every functional test,
which is why `streaming_guard` is structural. Switching the cursor type to
`CURSOR_TYPE_NO_CURSOR` failed the guard **and** one functional test. So the
attribute is not entirely invisible for that particular mutation. Whether plain
removal of the call is invisible was not characterised — recorded as observed
rather than tidied into agreement with the plan.

### The registry caught an orphan, automatically

`ESQLC-4010` (cursor type refused) was invented during implementation and
registered in no spec. `diag_registry` — added after Gate 1, where two orphans
were found only by an ad-hoc audit — failed the build and named it. **First time
an orphan diagnostic was caught by a guard rather than by remembering to look.**

### Third occurrence of a familiar pattern

Implementing `FETCH` silently changed what Gate 2's `unimplemented_cursor.sqlc`
was testing: it began asserting `ESQLC-4005` instead of `ESQLC-1012`. Retired in
favour of T310's `unimplemented_for_update.sqlc`, which was created for exactly
this. Gate 1 → Gate 2 → Gate 3, the same lesson each time: a negative fixture
pinned to "not implemented yet" expires the moment the thing is implemented.

### Deviations

- **T303 reframed during implementation.** The task described binding "at
  DECLARE rather than OPEN", but `DECLARE CURSOR` is a declaration with no
  runtime moment, so capturing a variable's value there is not expressible in C.
  The real risk is a runtime caching bound values across opens, which is what
  the fixture now pins by opening twice with different values.
- **T341 folded into `cursor_loop`** rather than a separate fixture: the loop
  poisons before every fetch and asserts the terminator byte survived each one,
  which is stronger than a single-fetch check would have been.

---

## Phase A — fixtures and harness

| ID | Task | Reqs | Deps |
|----|------|------|------|
| T300 | Extend `seed.sql` with several rows spanning the cursor's `WHERE` range, so a loop has something to iterate and an ordering to verify | — | — |
| T301 | `rt/cursor_loop.sqlc` — the canonical loop from the slice, poisoning `part_desc` before **every** fetch, not just the last | FR-004.13 | T300 |
| T302 [P] | `rt/fetch_exhausted.sqlc` — fetches past the end twice, to pin SD-3 | FR-004.14 | T300 |
| T303 [P] | `rt/open_binds_at_open.sqlc` — assigns `min_num` **after** the `DECLARE` and before the `OPEN`; the only fixture that distinguishes correct binding time | FR-004.12 | T300 |
| T304 [P] | `rt/close_then_reopen.sqlc` — close, reopen, and read the set again | FR-004.15 | T300 |
| T305 [P] | `rt/commit_frees_cursor.sqlc` — commit with the cursor still open, then fetch | FR-003.8 | T300 |
| T306 [P] | `rt/negative/cursor_order.sqlc` — fetch before open, double open, close unopened, in one fixture with distinguishable exits | FR-004.19 | T300 |
| T307 [P] | `negative/undeclared_cursor.sqlc` + `.expected.diag` — `OPEN` of a name never declared | FR-004.11 | — |
| T308 [P] | `negative/duplicate_cursor.sqlc` + `.expected.diag` — the same cursor name declared twice | FR-004.11 | — |
| T309 [P] | `negative/undeclared_in_fetch.sqlc` + `.expected.diag` — a `FETCH … INTO` reference with no declaration | FR-001.25 | — |
| T310 [P] | `negative/unimplemented_for_update.sqlc` + `.expected.diag` — `DECLARE … CURSOR FOR … FOR UPDATE`, keeping a live `ESQLC-1012` case | FR-001.15 | — |
| T311 [P] | `declared_never_opened.sqlc` — declares a cursor and never opens it; must compile warning-free | FR-004.11 | — |
| T312 | Extend `run_tier2.sh` with the cursor cases and their seeding | — | T300 |

## Phase B — failing tests

All must fail for the right reason before any Phase C task starts.

### Tier 1 — preprocessor, no database

| ID | Task | Reqs | Deps |
|----|------|------|------|
| T320 | `fetch_into_landmark` — every reference in a `FETCH … INTO` is `DIR_OUT`. This answers the deferral recorded as T285 in Gate 2's task list **by test rather than by assumption** | NFR-001.1 | T301 |
| T321 [P] | `cursor_declare_registry` — the cursor's SQL is emitted exactly once, at the `DECLARE` site, as a `static const` | FR-004.11 | T301 |
| T322 [P] | `cursor_hostvar_dirs` — `OPEN` emits only input descriptors; `FETCH` emits only output descriptors **and no SQL text at all** | FR-001.16 | T301 |
| T323 [P] | `negative/undeclared_in_fetch` fires `ESQLC-1014` at the right position | FR-001.25 | T309 |
| T324 [P] | `negative/undeclared_cursor` fires `ESQLC-4005` | FR-004.11 | T307 |
| T325 [P] | `negative/duplicate_cursor` fires `ESQLC-4006` | FR-004.11 | T308 |
| T326 [P] | `negative/unimplemented_for_update` fires `ESQLC-1012` naming feature 004 | FR-001.15 | T310 |
| T327 [P] | `declared_never_opened` compiles with no warnings under `-Wall -Wextra` | FR-004.11 | T311 |
| T328 [P] | `abi_only_symbols` and `abi_isolation` still hold once cursor calls are emitted | FR-003.1, FR-003.2 | T301 |
| T329 | `contract_sync` reports the three cursor entry points as **implemented**, not planned | FR-003.3 | — |
| T330 | `order_by_lint` — a deliberately bad fixture that asserts row order with no `ORDER BY` in its cursor SQL must be **rejected** by the check T377 adds. A lint with no failing case is a lint nobody has proven works | FR-004.16b | T312 |

### Tier 2 — runtime, live MariaDB

| ID | Task | Reqs | Deps |
|----|------|------|------|
| T340 [P] | `rt/cursor_loop` — every seeded row exactly once, in `ORDER BY` order, with the correct count | FR-004.13, FR-004.16 | T312 |
| T341 [P] | `rt/cursor_no_terminator` — the sentinel byte survives **every** fetch, not merely the last | FR-002.28 | T312 |
| T342 [P] | `rt/fetch_exhausted` — 100 at end of set, host variables untouched, and a second fetch also 100 and still untouched | FR-004.14, FR-005.1 | T302 |
| T343 [P] | `rt/open_binds_at_open` — the value assigned after `DECLARE` is the one used | FR-004.12 | T303 |
| T344 [P] | `rt/close_then_reopen` — the reopened cursor returns the full set again | FR-004.15 | T304 |
| T345 [P] | `rt/commit_frees_cursor` — a fetch after an uncommitted-cursor commit is an error, not a stale row | FR-003.8 | T305 |
| T346 [P] | `rt/negative/cursor_order` — all three out-of-order operations are errors, each distinguishable | FR-004.19 | T306 |
| T347 | `rt/streaming_attr` — assert `STMT_ATTR_CURSOR_TYPE` was **accepted**, not inferred from behaviour. The only guard on the streaming risk | — | T312 |

### Mutation checks

| ID | Task | Guards | Deps |
|----|------|--------|------|
| T350 | Bind cursor inputs at `DECLARE` instead of `OPEN` — must fail T343 | FR-004.12 | T343 |
| T351 | Return the last row again instead of 100 when exhausted — must fail T342 | FR-004.14 | T342 |
| T352 | Null-terminate the fetch buffer — must fail T341 | FR-002.28 | T341 |
| T353 | Client-buffer the result instead of streaming — must fail **T347 and nothing else**. If any functional test also fails, the guard was accidental and T347 needs strengthening | — | T347 |

T353 is the unusual one: it asserts that a defect is invisible to every
functional test, which is precisely why the non-functional check exists.

## Phase C — implementation

| ID | Task | Reqs | Makes pass | Deps |
|----|------|------|-----------|------|
| T360 | `src/pp/scan.cc` — recognise `DECLARE <name> CURSOR FOR` as the `DECLARE CURSOR` keyword. **The 004 Q9 defect fix** | FR-004.11 | T321 | Phase B |
| T361 | `src/pp/dispatch.cc` — `DECLARE CURSOR`, `OPEN`, `FETCH`, `CLOSE` implemented; `FOR UPDATE` and positioned forms keep `ESQLC-1012` | FR-001.15 | T326 | T360 |
| T362 | `src/pp/emit.cc` — cursor registry populated at `DECLARE`; SQL emitted there as a `static const` | FR-004.11 | T321 | T361 |
| T363 | `src/pp/emit.cc` — `OPEN` emits input descriptors and the open call | FR-001.16, FR-003.1 | T322 | T362 |
| T364 | `src/pp/emit.cc` — `FETCH` emits output descriptors and the fetch call, with no SQL text | FR-001.16, NFR-001.1 | T320, T322 | T363 |
| T365 | `src/pp/emit.cc` — `CLOSE` emits the close call | FR-004.15 | T344 | T364 |
| T366 | `src/pp/emit.cc` — cursor name resolution: undeclared and duplicate names diagnosed | FR-004.11 | T324, T325 | T362 |
| T367 | `src/pp/emit.cc` — an undeclared reference in a `FETCH … INTO` is diagnosed | FR-001.25 | T323 | T364 |
| T368 | `src/pp/emit.cc` — a declared-but-never-opened cursor emits no unused-variable warning | FR-004.11 | T327 | T362 |
| T369 | `include/esqlc.h` — the three cursor entry points | FR-003.2, FR-003.3 | T328, T329 | Phase B |
| T370 | `src/rt/cursor.c` — cursor table and `open`, setting `STMT_ATTR_CURSOR_TYPE` and **asserting it was accepted** | FR-004.12 | T343, T347 | T369 |
| T371 | `src/rt/cursor.c` — `fetch`: bind results once, write outputs with `buffer_length` = `width` | FR-002.28, FR-004.13, FR-004.16 | T340, T341 | T370 |
| T372 | `src/rt/cursor.c` — exhausted state and SD-3 idempotency | FR-004.14 | T342 | T371 |
| T373 | `src/rt/cursor.c` — `close` releases the result set and keeps the prepared statement, so a reopen does not re-prepare | FR-004.15 | T344 | T372 |
| T374 | `src/rt/cursor.c` — state machine rejects out-of-order operations | FR-004.19 | T346 | T373 |
| T375 | `src/rt/txn.c` — commit and rollback close open cursors | FR-003.8 | T345 | T374 |
| T376 | `src/rt/diag.c` — `sqlcode` 100 on exhaustion, distinct from an error | FR-005.1 | T342 | T372 |
| T377 | `tests/harness/` — a check that any fixture asserting row order contains `ORDER BY` in its cursor SQL, so FR-004.16b is enforced structurally rather than by reviewer memory | FR-004.16b | T340 | T312 |

## Phase D — diagnostics

| ID | Task | Code | Reqs | Deps |
|----|------|------|------|------|
| T380 [P] | `FETCH` on a cursor that is not open | `ESQLC-4001` | FR-004.19 | T374 |
| T381 [P] | `OPEN` on an already-open cursor | `ESQLC-4002` | FR-004.19 | T374 |
| T382 [P] | `CLOSE` on a cursor that is not open | `ESQLC-4003` | FR-004.19 | T374 |
| T383 [P] | Cursor name not declared | `ESQLC-4005` | FR-004.11 | T366 |
| T384 [P] | Duplicate cursor name in scope | `ESQLC-4006` | FR-004.11 | T366 |

## Phase E — documentation and registry

| ID | Task | Reqs | Deps |
|----|------|------|------|
| T390 | Mark the slice's requirement rows in `docs/traceability.md` as `tested` | — | Phase D |
| T391 | **Close 004 Q9** — the dispatch defect is repaired, not narrowed. Record it as resolved with the fix's location | — | T360 |
| T392 | Re-examine SD-1, SD-2 and SD-3 against what was built; record drift as a defect, not precedent | — | Phase C |
| T393 | Record whether `STMT_ATTR_CURSOR_TYPE` was accepted. **If it was not, register a divergence** — client buffering versus SQL/MP streaming is not something to absorb silently | — | T347 |
| T394 | Record the name-based cursor identity as pre-judging 004 Q5, and carry it forward as an explicit Gate 4 input | — | T369 |
| T395 | Confirm `diag_registry` is clean — the five cursor codes registered before they are emitted | — | Phase D |
| T396 | Reconcile the slice's non-proof list against the as-built state | — | Phase D |
| T397 | Run `/speckit.analyze` including the Principle VIII slice checks | — | T390–T396 |

## Critical path

Two chains of roughly equal length, converging on the live criteria:

```
Preprocessor  T300 → T301 → T321 → T360 → T361 → T362 → T363 → T364 → T365
Runtime       T300 → T312 → T369 → T370 → T371 → T372 → T373 → T374 → T375
Converge      → live T340–T347 → mutations T350–T353 → T396
```

Nine sequential tasks each, so neither dominates — unlike Gate 1 (preprocessor
heavy) or Gate 2 (runtime heavy). The two halves can genuinely proceed in
parallel once `T369` lands the header.

**`T362` is the most blocking preprocessor task**: the registry is what `T363`,
`T364`, `T366` and `T368` all consult. **`T370` is its runtime counterpart**,
since every other cursor operation needs the table and the opened statement.

**`T360` is the riskiest**, not the most blocking. It widens keyword matching for
one special case, and an over-match would route a non-cursor statement into the
cursor handler. Its guard is that `CURSOR` must appear before `FOR`, and Gates 1
and 2's fixtures stay in the suite to catch collateral damage.

## Exit criteria

- [ ] All nine slice criteria demonstrated — T340–T347
- [ ] Sentinel survives every fetch, not merely the last — T341
- [ ] Exhaustion is 100, untouched, and idempotent — T342
- [ ] Binding happens at `OPEN`, proven by changing the value first — T343
- [ ] `STMT_ATTR_CURSOR_TYPE` accepted, or a divergence registered — T347, T393
- [ ] All four mutation checks fail their intended guard; T353 fails **only** the non-functional one — T350–T353
- [ ] Gates 1 and 2 fixtures still pass unchanged — full suite
- [ ] Five cursor diagnostics fire at correct code, line, column — T380–T384
- [ ] 004 Q9 closed as repaired — T391
- [ ] Tier 1 green with no MariaDB present; `diag_registry` clean — T395
- [ ] `/speckit.analyze` clean, slice checks included — T397
