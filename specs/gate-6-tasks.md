# Gate 6 tasks — searched `UPDATE` and `DELETE`

**Slice:** [gate-6.md](gate-6.md) · **Plan:** [gate-6-plan.md](gate-6-plan.md)

Phase A fixtures, then Phase B tests, then Phase C implementation. No Phase C
task starts until the Phase B test it names fails for the right reason
(Principle IV).

17 scoped requirements. Every one appears in at least one Phase B and one
Phase C task; the coverage check is at the end.

## Phase A — fixtures and harness

| ID | Task | Reqs | Deps |
|----|------|------|------|
| T610 | `rt/update_rows.sqlc` — an `UPDATE` whose `WHERE` selects known rows, verified by re-reading | FR-004.7 | — |
| T611 [P] | `rt/update_zero_rows.sqlc` — a `WHERE` matching nothing | FR-004.10 | — |
| T612 [P] | `rt/update_matched_unchanged.sqlc` — **the slice's most important fixture**: set a row to the value it already holds | FR-004.10, FR-005.17 | — |
| T613 [P] | `rt/update_set_null.sqlc` — a negative indicator, with a non-zero value left in the buffer so a runtime that stores it is caught | FR-004.8, FR-002.16 | — |
| T614 [P] | `rt/update_char_verbatim.sqlc` — an underfilled `char` array with an embedded null | FR-002.30 | — |
| T615 [P] | `rt/delete_rows.sqlc` — one `DELETE` removing more than one row | FR-004.9 | — |
| T616 [P] | `rt/delete_zero_rows.sqlc` — a `WHERE` matching nothing | FR-004.10 | — |
| T617 [P] | `rt/dml_sqlsa_stats.sqlc` — read the `SQLSA` after an `UPDATE` and after a `DELETE` | FR-005.17 | — |
| T618 [P] | `rt/dml_table_name.sqlc` — `table_name` after each of `INSERT`, `UPDATE`, `DELETE` | FR-005.22 | — |
| T619 [P] | `table_landmark.sqlc` — Tier 1: the three DML forms, plus a table-like token inside a string literal | FR-005.22, NFR-001.1 | — |
| T620 [P] | `table_landmark_absent.sqlc` — Tier 1: a multi-table `UPDATE`, a leading subquery, a delimited identifier | FR-005.22 | — |
| T621 [P] | `update_placeholders.sqlc` — Tier 1: host variables in both `SET` and `WHERE` | FR-003.10 | — |
| T622 [P] | `rt/update_injection_literal.sqlc` — a host variable holding SQL text | NFR-003.2 | — |
| T623 [P] | `negative/update_where_current_of.sqlc` + `.expected.diag` | FR-001.15 | — |
| T624 [P] | `negative/delete_where_current_of.sqlc` + `.expected.diag` | FR-001.15 | — |
| T625 | Extend `run_tier2.sh` with the Gate 6 cases | — | T610 |

16 tasks.

## Phase B — failing tests

All must fail for the right reason before any Phase C task starts.

### Tier 1 — preprocessor, no database

| ID | Task | Reqs | Deps |
|----|------|------|------|
| T630 | `table_landmark` — `INSERT INTO t`, `UPDATE t`, `DELETE FROM t` each yield `t` | FR-005.22 | T619 |
| T631 [P] | `table_landmark` — a table-like token inside a string literal yields nothing. The landmark runs in the scanner precisely so a literal cannot be mistaken for one | FR-005.22, NFR-001.1 | T619 |
| T632 [P] | `table_landmark_absent` — the three hard forms yield **nothing**. A wrong name is worse than none, because `table_name` reads as authoritative | FR-005.22 | T620 |
| T633 [P] | `update_placeholders` — every host variable becomes a placeholder and no value appears in the statement text | FR-003.10 | T621 |
| T634 [P] | `opaque_body_unchanged` — `UPDATE` and `DELETE` bodies pass through verbatim | NFR-001.1 | T621 |
| T635 [P] | `abi_only_symbols` — the emitted unit calls `esqlc_*` and nothing else | FR-003.1 | T621 |
| T636 [P] | `abi_isolation` and `contract_sync` cover the new `esqlc_stmt_exec` signature | FR-003.2, FR-003.3 | — |
| T637 [P] | `negative/update_where_current_of` — code, line **and** column | FR-001.15 | T623 |
| T638 [P] | `negative/delete_where_current_of` — code, line **and** column | FR-001.15 | T624 |
| T639 [P] | `update_indicator_assoc` — `:w :ind` associates on the **input** side; `:a, :b` does not | FR-002.15 | T613 |

### Tier 2 — live server

| ID | Task | Reqs | Deps |
|----|------|------|------|
| T640 | `rt/update_rows` — the selected rows change and no others do | FR-004.7 | T610 |
| T641 [P] | `rt/update_zero_rows` — `sqlcode` 100 with a **successful** transport | FR-004.10, FR-005.1 | T611 |
| T642 | `rt/update_matched_unchanged` — `sqlcode` **0** and `records_used` **0**. The one fixture that proves the two counts come from different sources; without it `CLIENT_FOUND_ROWS` is untested | FR-004.10, FR-005.17 | T612 |
| T643 [P] | `rt/update_set_null` — the column becomes null and the buffer's value is **not** stored | FR-004.8, FR-002.16 | T613 |
| T644 [P] | `rt/update_char_verbatim` — `width` bytes bound verbatim, embedded null included | FR-002.30 | T614 |
| T645 [P] | `rt/delete_rows` — more than one row removed, `records_used` matching | FR-004.9 | T615 |
| T646 [P] | `rt/delete_zero_rows` — `sqlcode` 100 | FR-004.10 | T616 |
| T647 [P] | `rt/dml_sqlsa_stats` — the `SQLSA` is populated after both statements | FR-005.17 | T617 |
| T648 | `rt/dml_table_name` — `table_name` is the real table for all three DML statements | FR-005.22 | T618 |
| T649 [P] | `rt/update_injection_literal` — the value is stored, never executed | NFR-003.2 | T622 |
| T650 | Gate 1's `insert` fixtures stay green under `CLIENT_FOUND_ROWS`. `INSERT` shares the `n_out == 0` path, so a change made for `UPDATE` lands on it | FR-004.10 | T625 |

21 tasks.

## Phase C — implementation

| ID | Task | Reqs | Makes pass | Deps |
|----|------|------|-----------|------|
| T660 | `src/pp/pp.h` — `Construct::table` | FR-005.22 | T630 | Phase B |
| T661 | `src/pp/scan.cc` — capture the identifier after `INTO`/`UPDATE`/`FROM`, in the same pass as the existing landmarks | FR-005.22, NFR-001.1 | T630, T631 | T660 |
| T662 | `src/pp/scan.cc` — yield nothing unless what was read is a plain identifier | FR-005.22 | T632 | T661 |
| T663 | `src/pp/dispatch.cc` — `UPDATE`/`DELETE` implemented; `INCLUDE SQLDA` and the rest keep `ESQLC-1012` | FR-004.7 | T640 | Phase B |
| T664 | `src/pp/emit.cc` — the `UPDATE` handler: placeholders at recorded spans, ordered descriptors | FR-004.7, FR-003.10, FR-003.1, NFR-001.1 | T633, T634, T635 | T663 |
| T665 | `src/pp/emit.cc` — the `DELETE` handler | FR-004.9 | T645 | T664 |
| T666 | `src/pp/emit.cc` — `WHERE CURRENT OF` in an `UPDATE` is refused. It is a clause, so the dispatch table cannot see it | FR-001.15 | T637 | T664 |
| T667 | `src/pp/emit.cc` — `WHERE CURRENT OF` in a `DELETE` is refused | FR-001.15 | T638 | T665 |
| T668 | `src/pp/emit.cc` — pass the landmark to `esqlc_stmt_exec` | FR-005.22 | T648 | T662, T665 |
| T669 | `src/pp/emit.cc` — indicator association on the input side | FR-002.15 | T639 | T664 |
| T670 | `include/esqlc.h` — `esqlc_stmt_exec` gains `table` | FR-003.2, FR-003.3 | T636 | Phase B |
| T671 | `tests/stub/esqlc_stub.c` — the same signature | FR-003.3 | T636 | T670 |
| T672 | `src/rt/context.c` — connect with `CLIENT_FOUND_ROWS`, so `affected_rows` reports rows **matched** | FR-004.10 | T641 | T670 |
| T673 | `src/rt/exec.c` — bind `is_null` for an input whose indicator is negative, and read nothing from its buffer | FR-004.8, FR-002.16 | T643 | T670 |
| T674 | `src/rt/exec.c` — `width` bytes bound verbatim on the input path | FR-002.30 | T644 | T673 |
| T675 | `src/rt/exec.c` — `sqlcode` 100 iff no rows were **found** | FR-004.10, FR-005.1 | T641, T646 | T672 |
| T676 | `src/rt/exec.c` — `records_used` from `mysql_info`'s `Changed:` for an `UPDATE` | FR-005.17 | T642 | T675 |
| T677 | `src/rt/exec.c` — a failed `mysql_info` parse falls back to the **sentinel**, never to zero | FR-005.17 | T642 | T676 |
| T678 | `src/rt/exec.c` — landmark passthrough to the `SQLSA` | FR-005.22 | T648 | T668 |
| T679 | `src/rt/sqlsa.c` — accept an explicit table name alongside the metadata path | FR-005.22 | T648 | T678 |
| T680 | `src/rt/exec.c` — no interpolation on the write path under any circumstance | NFR-003.2 | T649 | T664 |

21 tasks.

## Phase D — diagnostics

One task per diagnostic behaviour this slice touches. Both are `ESQLC-1012`
refusals, because positioned operations are out of scope and must fail loudly
rather than silently doing something searched.

| ID | Task | Code | Reqs | Deps |
|----|------|------|------|------|
| T690 [P] | `UPDATE … WHERE CURRENT OF` refused, naming feature 004 | `ESQLC-1012` | FR-001.15 | T666 |
| T691 [P] | `DELETE … WHERE CURRENT OF` refused, naming feature 004 | `ESQLC-1012` | FR-001.15 | T667 |

`ESQLC-4004` — positioned `UPDATE`/`DELETE` with no current row — gets no task.
It cannot arise while positioned operations are refused at compile time, and
implementing it would mean implementing them. Recorded in Phase E.

## Phase E — documentation and registry

| ID | Task | Reqs | Deps |
|----|------|------|------|
| T700 | Move the slice's rows in `docs/traceability.md` off `spec` — `tested` only where wholly covered, `partial` with the gap named | — | Phase D |
| T701 | **Resolve `DIV-053`** — `proposed` → `accepted` if the matched/altered split works as planned, or amended with what actually happened | — | T677 |
| T702 | Record whether `mysql_info` parsing proved reliable. If it did not, `records_used` for `UPDATE` becomes a `DIV-011` sentinel and this says so | — | T676 |
| T703 | Record whether the landmark survived the hard forms, or whether any of them yielded a wrong name before T662 caught it | — | T662 |
| T704 | Re-examine SD-1, SD-2, SD-7, SD-8, SD-9 against what was built; record drift as a defect, not as precedent | — | Phase C |
| T705 | Record that `ESQLC-4004` is unreachable while positioned operations are refused, so its absence is a consequence and not an omission | — | Phase D |
| T706 | Confirm `diag_registry`, `contract_sync` and `citation_check` are clean | — | Phase D |
| T707 | Reconcile the slice's non-proof list against the as-built state | — | Phase D |
| T708 | Run `/speckit.analyze`, including the Principle VIII slice checks | — | T700–T707 |

9 tasks.

## Phase D′ — mutation, run after Phase C

| Mutation | Must fail |
|---|---|
| Drop `CLIENT_FOUND_ROWS` | `update_matched_unchanged` (T642) |
| Take `records_used` from matched rather than changed | `update_matched_unchanged` (T642), in the other field |
| Remove input `is_null` binding | `update_set_null` (T643) |
| Return a landmark for a multi-table `UPDATE` | `table_landmark_absent` (T632) |
| Fall back to `0` instead of the sentinel on a parse failure | `update_matched_unchanged` (T642) |
| Bind `strlen` bytes instead of `width` on input | `update_char_verbatim` (T644) |

**The first two produce plausible wrong numbers, not crashes**, and they are the
reason `update_matched_unchanged` is written first and trusted least. A single
number reported for both counts looks entirely reasonable in either field.

Standing warning, now at five occurrences: this project's mutation harness has
produced false results every time by failing to rebuild — a `perl` substitution
without `/g`, a guard matching its own comment, swallowed build output leaving a
stale binary, a restore without `touch`, and a backup filename that did not
match the restore path. **Check the binary's timestamp changed, and check the
mutation is still present in the file after the run.**

## Requirement coverage

| Requirement | Phase B | Phase C |
|---|---|---|
| NFR-001.1 | T631, T634 | T661, T664 |
| FR-001.15 | T637, T638 | T666, T667 |
| FR-002.15 | T639 | T669 |
| FR-002.16 | T643 | T673 |
| FR-002.30 | T644 | T674 |
| FR-003.1 | T635 | T664 |
| FR-003.2 | T636 | T670 |
| FR-003.3 | T636 | T670, T671 |
| FR-003.10 | T633 | T664 |
| NFR-003.2 | T649 | T680 |
| FR-004.7 | T640 | T663, T664 |
| FR-004.8 | T643 | T673 |
| FR-004.9 | T645 | T665 |
| FR-004.10 | T641, T642, T646, T650 | T672, T675 |
| FR-005.1 | T641 | T675 |
| FR-005.17 | T642, T647 | T676, T677 |
| FR-005.22 | T630, T631, T632, T648 | T660, T661, T662, T668, T678, T679 |

**17 of 17 covered. Zero requirements without an implementing task.**

## Critical path

```
T619 ─ landmark fixture
  └─ T630 ─ landmark test fails
       └─ T660 ─ Construct::table
            └─ T661 ─ scanner capture
                 └─ T662 ─ identifier validation
                      └─ T668 ─ emitter passes it
                           └─ T678 ─ runtime passthrough
                                └─ T679 ─ SQLSA accepts it
                                     └─ T648 ─ table_name passes
```

Nine deep, and it is the longer of two chains. The other runs
T612 → T642 → T672 → T675 → T676 → T677, six deep, and carries the harder
work: `CLIENT_FOUND_ROWS`, then `sqlcode` from matched rows, then `records_used`
from a parsed server message, then the sentinel fallback. **Start that one
first** despite being shorter — the landmark chain is mechanical, and the
matched/altered split is where the slice can be quietly wrong.

`T650` sits outside both chains and gates the whole slice: `INSERT` shares the
`n_out == 0` path with `UPDATE`, so Gate 1's fixtures are the only thing
standing between `CLIENT_FOUND_ROWS` and a silent regression on the insert path.

## Exit criteria

The slice's ten, plus:

11. Every mutation in Phase D′ fails its named test, with a verified rebuild.
12. Gate 1's insert fixtures pass unchanged under `CLIENT_FOUND_ROWS`.
13. `DIV-053` resolved — accepted, or amended with what was actually built.
14. `docs/traceability.md` moved off `spec` for this slice's rows.

---

## Implementation report — 2026-09-01

All 69 tasks complete. 10/10 ctest; Tier 1 84 assertions and 21 negatives
clean; Tier 2 42 cases.

### The two counts, as built (T701, T702)

`DIV-053` is accepted and works as planned. `CLIENT_FOUND_ROWS` makes
`affected_rows` report rows matched, which is the basis `sqlcode` 100 needs, and
`mysql_info`'s `Changed:` field supplies the altered count `records_used` wants.
`update_matched_unchanged` — set a row to the value it already holds — reports
`sqlcode` 0 with `records_used` 0, which is the pair SQL/MP's own two pages
describe.

`mysql_info` parsing proved reliable against MariaDB's current format. Two
things implementation settled that the plan did not anticipate:

`INSERT` and `DELETE` emit no `Changed:` field, so `mysql_info` returns `NULL`
for them. That is a **normal path, not a failure** — matched and altered are the
same number for those two — so the parser returns `matched` there rather than
the sentinel. Conflating it with a parse failure would have made every insert
report a sentinel.

The failure path is **unreachable from any live fixture**, which mutation
testing found rather than review: a real `UPDATE` always gets a `Changed:`
field, so a mutant returning `0` instead of the sentinel survived untouched.
The parser is now split out as the pure `esqlc_rt_parse_changed` and unit-tested
over eight crafted inputs. A guard no test can reach is not a guard, and this
one guarded the difference between a missing measurement and an untrue one.

### The landmark, as built (T703)

SD-9 works. `table_name` is real for all three DML statements. Two corrections:

**A fixed offset was wrong.** `k.body` is the raw text between `EXEC SQL` and
`;`, so it can open with whitespace or a newline, and `at = 6` landed
mid-keyword — yielding no landmark at all, silently. It now finds the keyword
rather than assuming its position.

**One of the three "hard forms" was not hard.** The fixture called
`DELETE FROM parts WHERE part_num IN (SELECT ... FROM suppliers)` a leading
subquery, and it is nothing of the kind: an ordinary single-table `DELETE` whose
target sits right after the first `FROM`, which the landmark reads correctly.
The fixture was wrong, not the code. Replaced with a multi-table `DELETE`, where
the token after `FROM` carries an alias and the landmark must decline.

The remaining unread forms are the multi-table `UPDATE`, the multi-table
`DELETE`, and a delimited identifier. All three reach the sentinel, and
`table_landmark_absent` pins that in both directions.

### T704 — slice decisions as built

SD-1, SD-2, SD-7 carried unchanged. SD-9 held, with the offset correction above.

**SD-8's scope narrowed exactly as the slice predicted, and its Gate 5 fixture
had to move.** `sqlsa_sentinel_char` used a plain `INSERT`, which now has a
landmark and so reports a real table name. It was repointed to a multi-table
`DELETE` — a form the landmark declines — so it still tests the sentinel, but
tests it where the sentinel is now the right answer. A fixture that keeps
passing while the thing it tests has moved is worse than one that fails.

### T705 — `ESQLC-4004` is unreachable, not omitted

Positioned `UPDATE`/`DELETE` with no current row cannot arise while
`WHERE CURRENT OF` is refused at compile time. Implementing the diagnostic would
mean implementing positioned operations, which need 004 Q3, which needs `SQLRM`.
Recorded so its absence is a consequence rather than an oversight.

### Defects the tests found, not review

**A negative test that passed against no implementation.** Both
`WHERE CURRENT OF` fixtures went green immediately: the negative harness
compares code, line and column, and `ESQLC-1012` at that position is satisfied
equally by "UPDATE is not implemented in this slice" and by the
positioned-operation refusal this slice adds. Fixed by asserting the *reason* —
the message must name `CURRENT OF` — which failed until the handler checked for
the clause.

**Registration at file scope, latent since Gate 4.** Found during Gate 5 and
worth restating here because the same shape recurred: a pending emission
flushing on the first *construct* rather than the first *executable* one.

**`unimplemented.sqlc` repointed a third time.** Gate 1 used `SELECT`, Gate 2
moved it to `UPDATE`, Gate 6 implements that, so it now uses `PREPARE`. The
churn is the fixture working: each gate that implements the example must find a
new one, and the alternative is a fixture that silently stops testing FR-001.15.

**A stale binary, for the sixth time.** After restoring the landmark mutation
the sources were correct and the tests still failed identically — the mutation
was gone from the file but not from `esqlcpp`. Every previous instance had a
different cause; this one was a `cmake --build` that did not pick up a restored
file. The standing rule now has to be: after any mutation run, re-run the full
suite from a forced rebuild before believing green **or** red.

### Not proved, as scoped

Positioned operations in either form. Cursor stability. The landmark on the hard
forms — proven to reach the sentinel, which is not the same as proven readable.
Type breadth: still `char[]` and 16-bit integers, so a program using `INTEGER`
still cannot compile. `records_accessed` and the rest of `stats[]` remain
sentinels per `DIV-011`; only `records_used`, `num_tables` and `table_name` are
real. Every modification was single-session, so what another session observes
mid-statement is untouched — the same question 004 Q3 asks.
