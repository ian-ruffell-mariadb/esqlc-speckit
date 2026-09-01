# Gate 5 tasks — the SQLSA

**Slice:** [gate-5.md](gate-5.md) · **Plan:** [gate-5-plan.md](gate-5-plan.md)

Phase A fixtures, then Phase B tests, then Phase C implementation. No Phase C
task starts until the Phase B test it names fails for the right reason
(Principle IV).

25 scoped requirements. Every one appears in at least one Phase B and one
Phase C task; the coverage check is at the end.

## Phase A — fixtures and harness

| ID | Task | Reqs | Deps |
|----|------|------|------|
| T510 | `sqlsa_sizes.sqlc` — both versions declared in one unit, `sizeof` asserted at 838 and 1790 | FR-005.16, FR-005.27 | — |
| T511 [P] | `sqlsa_layout.sqlc` — the union arms, the per-family counter widths, VSBB at v330 against `sqlsa_reserved` at v300 | FR-005.21a, .21b, .21c | — |
| T512 [P] | `sqlsa_version_select.sqlc` — `INCLUDE STRUCTURES SQLSA VERSION 300` and `330` | FR-005.8, FR-005.9 | — |
| T513 [P] | `rt/sqlsa_cursor_stats.sqlc` — the slice's main fixture: `DECLARE`/`OPEN`/`FETCH`/`CLOSE`, reading `SQLSA` at each step | FR-005.17 | — |
| T514 [P] | `rt/sqlsa_accumulate.sqlc` — the accumulator idiom of §9 p.9-13. **The only fixture that can detect a missing reset** | FR-005.20 | T513 |
| T515 [P] | `rt/sqlsa_two_tables.sqlc` — a two-table join, so `num_tables` is 2 and `stats[1]` is exercised rather than only `stats[0]` | FR-005.22 | T513 |
| T516 [P] | `rt/sqlsa_sentinels.sqlc` — read `messages`, `message_bytes`, `escalations`; assert the sentinel and **assert not zero** | FR-005.25 | T513 |
| T517 [P] | `rt/sqlsa_after_commit.sqlc` — `COMMIT WORK`, then read the structure | FR-005.19 | T513 |
| T518 [P] | `negative/structures_after_sqlsa.sqlc` + `.expected.diag` | FR-005.12 | — |
| T519 [P] | `negative/sqlsa_version_current.sqlc` + `.expected.diag` — out of slice, refused | FR-005.9 | — |
| T520 [P] | `negative/sqlsa_bad_version.sqlc` + `.expected.diag` — an unsupported version number | FR-005.9 | — |
| T521 [P] | `negative/sqlca_version_330.sqlc` + `.expected.diag` — 330 is `SQLSA`-only | FR-005.9 | — |
| T522 [P] | `negative/sqlsa_duplicate.sqlc` + `.expected.diag` — two non-`EXTERNAL` definitions | FR-005.8 | — |
| T523 | `tests/harness/sqlsa_layout_sync.sh` — extract emitted `offsetof` values and the runtime's offset table, compare. Register in `CMakeLists.txt` | NFR-005.1 | T510 |
| T524 | Extend `run_tier2.sh` with the Gate 5 cases | — | T513 |

15 tasks.

## Phase B — failing tests

All must fail for the right reason before any Phase C task starts.

### Tier 1 — preprocessor, no database

| ID | Task | Reqs | Deps |
|----|------|------|------|
| T530 | `sqlsa_sizes` — 838 and 1790 exactly, eye-catcher `SA`. **Fails without the packing attribute**, which is the point | FR-005.16, FR-005.27 | T510 |
| T531 [P] | `sqlsa_layout` — `dml` and `prepare` resolve to the same offset | FR-005.21a | T511 |
| T532 [P] | `sqlsa_layout` — counters are 32-bit at v300 and 64-bit at v330; `waits`/`escalations` widen with them | FR-005.21b | T511 |
| T533 [P] | `sqlsa_layout` — VSBB flags exist only at v330; v300 has `sqlsa_reserved` in that slot | FR-005.21c | T511 |
| T534 [P] | `sqlsa_layout` — the VSBB flags are `-1` for true and `0` for false | FR-005.23 | T511 |
| T535 [P] | `sqlsa_layout` — every integer field is a fixed-width type. **A native `long` passes on ILP32 and silently breaks the layout on LP64**, so this is asserted directly and not left to `sizeof` | FR-005.21 | T511 |
| T536 | `sqlsa_layout_sync` — the emitter's offsets and the runtime's offset table agree, field for field, both versions | NFR-005.1 | T523 |
| T537 [P] | `sqlsa_version_select` — both version forms accepted, 330 accepted for `SQLSA` only | FR-005.8, FR-005.9 | T512 |
| T538 [P] | `negative/structures_after_sqlsa` — code, line **and** column | FR-005.12 | T518 |
| T539 [P] | `abi_isolation` and `contract_sync` cover `esqlc_sqlsa_register` | FR-003.2, FR-003.3 | — |
| T540 [P] | `abi_only_symbols` — the emitted unit calls `esqlc_*` and nothing else | FR-003.1 | T510 |
| T541 [P] | `opaque_body_unchanged` — statement bodies still pass through verbatim | NFR-001.1 | T513 |

### Tier 2 — live server

| ID | Task | Reqs | Deps |
|----|------|------|------|
| T542 | `rt/sqlsa_cursor_stats` — populated after `OPEN`, `FETCH` and `CLOSE` | FR-005.17, FR-004.12, FR-004.13, FR-004.15 | T513 |
| T543 [P] | `rt/sqlsa_accumulate` — the accumulated total matches the row count. **Over-counts if the structure is not reset per `FETCH`** | FR-005.20, FR-004.14 | T514 |
| T544 [P] | `rt/sqlsa_two_tables` — `num_tables` is 2, `stats[1]` populated, both `table_name`s correct | FR-005.22 | T515 |
| T545 [P] | `rt/sqlsa_sentinels` — unmappable numeric fields are `-1` in their own width, and **none is zero** (SD-7) | FR-005.25 | T516 |
| T546 [P] | `rt/sqlsa_sentinels` — an unmappable character field is `?` to its full width (SD-8) | FR-005.25 | T516 |
| T547 [P] | `rt/sqlsa_after_commit` — sentinels throughout, not the previous statement's values | FR-005.19 | T517 |
| T548 [P] | `cursor_declare` — `DECLARE CURSOR` still dispatches; Gate 3 unregressed | FR-004.11 | T513 |

19 tasks.

## Phase C — implementation

| ID | Task | Reqs | Makes pass | Deps |
|----|------|------|-----------|------|
| T560 | `src/pp/sqlsa.cc` — the v300 declaration, fixed-width integers, packed | FR-005.16, FR-005.21, FR-005.27 | T530, T535 | Phase B |
| T561 | `src/pp/sqlsa.cc` — the v330 declaration, 64-bit counters, VSBB flags | FR-005.21b, FR-005.21c | T532, T533 | T560 |
| T562 | `src/pp/sqlsa.cc` — `dml` and `prepare` emitted as a union | FR-005.21a | T531 | T560 |
| T563 | `src/pp/sqlsa.cc` — the bounded static-assertion set: header fields, both arms, all of `stats[0]`, `stats[1]` for the stride, `sizeof` for the total | NFR-005.1 | T530 | T562 |
| T564 | `src/pp/sqlsa.cc` — the VSBB flag constants | FR-005.23 | T534 | T561 |
| T565 | `src/pp/dispatch.cc` — `INCLUDE SQLSA` implemented; `INCLUDE SQLDA` keeps its `ESQLC-1012` | FR-004.11, FR-005.8 | T537, T548 | T563 |
| T566 | `src/pp/dispatch.cc` — version acceptance: 1, 2, 300, 340+ for all three, 330 for `SQLSA` only | FR-005.9 | T537 | T565 |
| T567 | `src/pp/emit.cc` — emit the declaration and its registration call | NFR-001.1, FR-003.1, FR-005.17 | T540, T541 | T565 |
| T568 | `src/pp/emit.cc` — `INCLUDE STRUCTURES` must precede `INCLUDE SQLSA` | FR-005.12 | T538 | T567 |
| T569 | `include/esqlc.h` — `esqlc_sqlsa_register` | FR-003.2, FR-003.3 | T539 | Phase B |
| T570 | `src/rt/sqlsa.c` — registration, validating `len` against `version` | FR-005.16 | T542 | T569 |
| T571 | `src/rt/sqlsa.c` — the offset table, both versions | NFR-005.1 | T536 | T570 |
| T572 | `src/rt/sqlsa.c` — stamp every numeric field with its sentinel at statement start (SD-7) | FR-005.19, FR-005.25 | T545, T547 | T571 |
| T573 | `src/rt/sqlsa.c` — stamp character fields with `?` to full width (SD-8) | FR-005.25 | T546 | T572 |
| T574 | `src/rt/sqlsa.c` — populate `num_tables` and `table_name` from result-set metadata | FR-005.22 | T544 | T573 |
| T575 | `src/rt/cursor.c` — populate after `OPEN`, `FETCH` and `CLOSE` | FR-004.12, FR-004.13, FR-004.14, FR-004.15, FR-005.17 | T542, T543 | T574 |
| T576 | `src/rt/exec.c` — stamp on every statement; supply `records_used` for DML | FR-005.20 | T543 | T572 |

17 tasks.

## Phase D — diagnostics

One task per diagnostic row this slice touches.

| ID | Task | Code | Reqs | Deps |
|----|------|------|------|------|
| T590 [P] | `INCLUDE STRUCTURES` after an `INCLUDE SQLSA` — the `SQLSA` arm of an existing check | `ESQLC-5001` | FR-005.12 | T568 |
| T591 [P] | An unsupported structure version, reported as SQL error 11203 | `ESQLC-5002` | FR-005.9 | T566 |
| T592 [P] | Version 330 requested for `SQLCA` or `SQLDA` | `ESQLC-5003` | FR-005.9 | T566 |
| T593 [P] | A second non-`EXTERNAL` `SQLSA` definition | `ESQLC-5005` | FR-005.8 | T565 |
| T594 [P] | `SQLSA VERSION CURRENT` — out of slice, and `ESQLC-5007` is exactly the right refusal rather than a generic `ESQLC-1012`, because `SQLGETSYSTEMVERSION` is never reachable here | `ESQLC-5007` | FR-005.9 | T566 |

`ESQLC-5009` and `ESQLC-5010` get no task. Both require dataflow over the host
program to detect a *read*, which the preprocessor cannot do and should not grow
for two diagnostics. Recorded in Phase E rather than left silently absent.

## Phase E — documentation and registry

| ID | Task | Reqs | Deps |
|----|------|------|------|
| T600 | Move the slice's rows in `docs/traceability.md` off `spec` — `tested` only where wholly covered, `partial` with the gap named otherwise | — | Phase D |
| T601 | **Update `DIV-011`** — status `proposed` → `accepted`, and fill its **Detection** field, which currently says "to be fixed by 005's spec": SD-7 gives `-1` per width, SD-8 gives `?` for character fields | — | T573 |
| T602 | **Raise a finding against 005: FR-005.27 understates the requirement.** The alignment pragma is documented for the four `*_R330` types, but v300 measures 840 unpacked against a published 838 and needs it too | — | T560 |
| T603 | Record that `ESQLC-5009` and `ESQLC-5010` are deliberately unimplemented, with the dataflow reason, so their absence is a decision and not an oversight | — | Phase D |
| T604 | Re-examine SD-1, SD-2, SD-3, SD-7, SD-8 against what was built; record drift as a defect, not as precedent | — | Phase C |
| T605 | Record whether `records_accessed` found an honest source. If it did not, it becomes a sentinel and `DIV-011` grows by a field | — | T574 |
| T606 | Confirm `diag_registry` and `citation_check` are clean | — | Phase D |
| T607 | Reconcile the slice's non-proof list against the as-built state | — | Phase D |
| T608 | Run `/speckit.analyze`, including the Principle VIII slice checks | — | T600–T607 |

## Phase D′ — mutation, run after Phase C

Not a phase of its own in the template, but these are the guards that must be
proved load-bearing rather than assumed to be.

| Mutation | Must fail |
|---|---|
| Remove the packing attribute | `sqlsa_sizes` |
| Emit `long` instead of `int32_t` | `sqlsa_layout` (T535) |
| Widen a v300 counter to 64-bit | `sqlsa_layout` (T532) |
| Skip the per-statement stamp | `sqlsa_accumulate` (T543) |
| Return `0` instead of a sentinel | `sqlsa_sentinels` (T545) |
| Shift one runtime offset by 2 | `sqlsa_layout_sync` (T536) |

The stamp mutation is the one that matters. A missing reset produces *plausible*
numbers — the previous statement's — which no size assertion can catch, and is
the reason §9 prescribes the accumulator idiom in the first place.

**This project's mutation harness has produced false passes four times**, every
time by failing to rebuild: a `perl` substitution without `/g`, a guard matching
its own comment, swallowed build output leaving a stale binary, and a restore
without `touch`. Check the binary's timestamp changed before believing a result.

## Requirement coverage

| Requirement | Phase B | Phase C |
|---|---|---|
| NFR-001.1 | T541 | T567 |
| FR-003.1 | T540 | T567 |
| FR-003.2 | T539 | T569 |
| FR-003.3 | T539 | T569 |
| FR-004.11 | T548 | T565 |
| FR-004.12 | T542 | T575 |
| FR-004.13 | T542 | T575 |
| FR-004.14 | T543 | T575 |
| FR-004.15 | T542 | T575 |
| FR-005.8 | T537 | T565 |
| FR-005.9 | T537 | T566 |
| FR-005.12 | T538 | T568 |
| FR-005.16 | T530 | T560, T570 |
| FR-005.17 | T542 | T567, T575 |
| FR-005.19 | T547 | T572 |
| FR-005.20 | T543 | T576 |
| FR-005.21 | T535 | T560 |
| FR-005.21a | T531 | T562 |
| FR-005.21b | T532 | T561 |
| FR-005.21c | T533 | T561 |
| FR-005.22 | T544 | T574 |
| FR-005.23 | T534 | T564 |
| FR-005.25 | T545, T546 | T572, T573 |
| FR-005.27 | T530 | T560 |
| NFR-005.1 | T536 | T563, T571 |

**25 of 25 covered. Zero requirements without an implementing task.**

## Critical path

```
T510 ─ sizes fixture
  └─ T530 ─ size test fails
       └─ T560 ─ v300 declaration, packed, fixed-width
            └─ T561 ─ v330 declaration
                 └─ T562 ─ union
                      └─ T563 ─ static assertions
                           └─ T565 ─ dispatch
                                └─ T567 ─ emission
                                     └─ T569 ─ ABI header
                                          └─ T570 ─ registration
                                               └─ T571 ─ runtime offset table
                                                    └─ T572 ─ sentinel stamping
                                                         └─ T575 ─ cursor population
                                                              └─ T543 ─ accumulator passes
```

Fourteen deep. The shape is forced: nothing runtime can be written until the
layout is fixed, because the runtime addresses the structure by offset, and
nothing can be populated until stamping exists, because population overwrites
sentinels rather than zeros.

`T536` — the layout sync guard — is the one task that cannot run early. It needs
both encodings of the layout to exist, so it sits between T571 and T572 in
practice even though its fixture (T523) is written in Phase A.

## Exit criteria

The slice's ten, plus:

11. `sqlsa_layout_sync` green — the emitter and the runtime agree, field for
    field, on both layouts.
12. Every mutation in Phase D′ fails its named test, with a rebuilt binary.
13. `docs/traceability.md` moved off `spec` for this slice's rows.
14. `DIV-011` accepted, with its **Detection** field filled.
