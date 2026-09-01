# Tasks: Gate 4 (WHENEVER and the SQLCA)

**Slice:** [gate-4.md](gate-4.md) · **Plan:** [gate-4-plan.md](gate-4-plan.md)

Rules in force: tests before implementation (Principle IV); every Phase C task
names the Phase B task it makes pass; `[P]` only where tasks touch disjoint files
with no dependency between them; every task lists requirement IDs.

Task IDs start at T400.

Phase D covers the three diagnostics this slice reaches. `INCLUDE SQLSA` and
`INCLUDE SQLDA` keep their `ESQLC-1012` entries.

**Gates 1–3 fixtures stay in the suite.** Three times now, implementing a verb
has silently changed what an older negative fixture was testing. T413 exists to
keep a live `ESQLC-1012` case once `WHENEVER` and `INCLUDE SQLCA` are
implemented.

## Phase A — fixtures and harness

| ID | Task | Reqs | Deps |
|----|------|------|------|
| T400 | `rt/whenever_flow.sqlc` — the slice's main fixture: handler on failure, `GOTO` on 100, `CONTINUE` disabling | FR-005.1 | — |
| T401 [P] | `whenever_scope.sqlc` — a directive superseded mid-file, with statements on both sides | FR-005.6 | — |
| T402 [P] | `whenever_conditions.sqlc` — all three conditions active simultaneously | FR-005.3 | — |
| T403 [P] | `whenever_actions.sqlc` — `CALL`, `GOTO`, `GO TO`, `CONTINUE` | FR-005.4 | — |
| T404 [P] | `whenever_applies_to.sqlc` — a handler active across a DML statement **and** a `COMMIT WORK`, so SD-5 is observable | FR-005.7 | — |
| T405 [P] | `whenever_positions.sqlc` — the directive at file scope and inside a function | FR-001.13 | — |
| T406 [P] | `rt/sqlca_items.sqlc` — `INCLUDE SQLCA`, provoke a failure, read numeric items back | FR-005.30 | — |
| T407 [P] | `rt/sqlca_copy_survives.sqlc` — `memcpy` the `SQLCA` with `SQLCA_LEN`, then read the **copy**. Guards the registration design directly | FR-005.14a | T406 |
| T408 [P] | `rt/sqlca_seven_codes.sqlc` — provoke a statement yielding several diagnostics; all must be retrievable | FR-005.15 | T406 |
| T409 [P] | `rt/sqlca_fscode.sqlc` — a failure with file-system detail | FR-005.31 | — |
| T410 [P] | `negative/whenever_undeclared.sqlc` + `.expected.diag` — an action that is not a valid C identifier | FR-005.4 | — |
| T411 [P] | `negative/structures_after_include.sqlc` + `.expected.diag` | FR-005.10 | — |
| T412 [P] | `negative/no_include_structures.sqlc` — omits the directive; expects the informational message | FR-005.10 | — |
| T413 [P] | `negative/unimplemented_sqlsa.sqlc` + `.expected.diag` — keeps a live `ESQLC-1012` case | FR-001.15 | — |
| T414 [P] | `rt/negative/sqlca_misuse.sqlc` — bad item code, undersized buffer, index out of range | FR-005.30 | T406 |
| T415 | `goto_missing_label.sqlc` — a `GOTO` naming a label that does not exist. Pins the **accepted** risk: the C compiler reports it, at the `WHENEVER` line | FR-005.4 | — |
| T416 | Extend `run_tier2.sh` with the Gate 4 cases | — | T400 |

## Phase B — failing tests

All must fail for the right reason before any Phase C task starts.

### Tier 1 — preprocessor, no database

| ID | Task | Reqs | Deps |
|----|------|------|------|
| T430 | `whenever_precedence` — emitted checks are `sqlcode == 100`, then `sqlcode < 0`, then `sqlcode > 0 && sqlcode != 100`, **in that order**. Verified structurally because `SQLWARNING` cannot be fired at runtime in this slice | FR-005.5 | T402 |
| T431 [P] | `whenever_scope` — state is per-condition; a superseded action stops applying at exactly the right statement | FR-001.22, FR-005.6 | T401 |
| T432 [P] | `whenever_conditions` — all three conditions emit their own check | FR-005.3 | T402 |
| T433 [P] | `whenever_actions` — `CALL` emits a call, `GOTO`/`GO TO` a jump, and **`CONTINUE` emits nothing at all** | FR-005.4 | T403 |
| T434 [P] | `whenever_applies_to` — checks follow the DML statement and **not** the `COMMIT WORK` (SD-5) | FR-005.7 | T404 |
| T435 [P] | `whenever_positions` — accepted in both positions | FR-001.13 | T405 |
| T436 [P] | `sqlca_size` — `sizeof` is exactly 430 and the eye-catcher leads | FR-005.14 | T406 |
| T437 [P] | `opaque_body_unchanged` — statement bodies still reach handlers intact | NFR-001.1 | T400 |
| T438 [P] | `abi_only_symbols` and `abi_isolation` still hold with the new calls emitted | FR-003.1, FR-003.2 | T400 |
| T439 | `contract_sync` reports the two new entry points as **implemented**, not planned | FR-003.3 | — |
| T440 [P] | `negative/whenever_undeclared` fires `ESQLC-5008` | FR-005.4 | T410 |
| T441 [P] | `negative/structures_after_include` fires `ESQLC-5001` | FR-005.10 | T411 |
| T442 [P] | `negative/no_include_structures` emits `ESQLC-5006` and generates version 2 | FR-005.10 | T412 |
| T443 [P] | `negative/unimplemented_sqlsa` fires `ESQLC-1012` naming feature 005 | FR-001.15 | T413 |
| T444 | `goto_missing_label` — compile the emitted C and assert the label error is reported at the `WHENEVER` line, proving `#line` makes the accepted risk survivable | FR-005.4 | T415 |

### Tier 2 — runtime, live MariaDB

| ID | Task | Reqs | Deps |
|----|------|------|------|
| T450 [P] | `rt/whenever_flow` — handler fires after the failing statement and not the succeeding one; `GOTO` transfers on 100; `CONTINUE` disables | FR-005.1 | T416 |
| T451 [P] | `rt/sqlca_items` — documented numeric items return their documented values after a failure | FR-005.14a, FR-005.30 | T416 |
| T452 [P] | `rt/sqlca_copy_survives` — reading a `memcpy`'d copy yields the same items as the original | FR-005.14a | T407 |
| T453 [P] | `rt/sqlca_seven_codes` — several diagnostics from one statement, all retrievable | FR-005.15 | T408 |
| T454 [P] | `rt/sqlca_item22_sign` — item 22 is **positive for an error**, pinned in both directions so SD-4's reversal would be visible | FR-005.23b | T406 |
| T455 [P] | `rt/sqlca_fscode` — file-system detail returned for a failure that has one | FR-005.31 | T409 |
| T456 [P] | `rt/negative/sqlca_misuse` — the documented 8510–8517 codes | FR-005.30 | T414 |

### Mutation checks

| ID | Task | Guards | Deps |
|----|------|--------|------|
| T460 | Reorder the emitted checks, SQLERROR before NOT FOUND — must fail T430 | FR-005.5 | T430 |
| T461 | Let a superseded action keep applying — must fail T431 | FR-005.6 | T431 |
| T462 | Apply `WHENEVER` to `COMMIT WORK`, violating SD-5 — must fail T434 | FR-005.7 | T434 |
| T463 | Make accessors read runtime state instead of the registered struct — must fail **T452 and only T452**. If T451 also fails the copy test was redundant; if nothing fails, the registration design is unguarded | FR-005.14a | T452 |

T463 is the one that matters most: it guards the plan's central design decision,
and its expected blast radius is stated so an unexpected one is itself a finding.

## Phase C — implementation

| ID | Task | Reqs | Makes pass | Deps |
|----|------|------|-----------|------|
| T470 | `src/pp/whenever.cc` — per-condition action table, set and superseded in source order | FR-001.22, FR-005.3, FR-005.6 | T431, T432 | Phase B |
| T471 | `src/pp/whenever.cc` — emit checks in the published precedence order | FR-005.5 | T430 | T470 |
| T472 | `src/pp/whenever.cc` — `CONTINUE` emits nothing; `CALL` and `GOTO`/`GO TO` emit their forms | FR-005.4 | T433 | T471 |
| T473 | `src/pp/dispatch.cc` — `WHENEVER` and `INCLUDE SQLCA` implemented; `INCLUDE SQLSA`/`SQLDA` keep `ESQLC-1012` | FR-001.13, FR-001.15 | T435, T443 | T472 |
| T474 | `src/pp/emit.cc` — append checks after each **applicable** statement, applicability governed by SD-5 | FR-005.7, NFR-001.1, FR-003.1 | T434, T437 | T473 |
| T475 | `src/pp/emit.cc` — a `WHENEVER` action that is not a valid C identifier is diagnosed | FR-005.4 | T440 | T474 |
| T476 | `src/pp/emit.cc` — `INCLUDE SQLCA` emits the declaration, its two static assertions, and the registration call | FR-005.14 | T436 | T473 |
| T477 | `src/pp/emit.cc` — `INCLUDE STRUCTURES` ordering check, and the default-version-2 informational message | FR-005.10 | T441, T442 | T476 |
| T478 | `include/esqlc.h` — the two new entry points | FR-003.2, FR-003.3 | T438, T439 | Phase B |
| T479 | `src/rt/sqlca.c` — registration, validating `len == SQLCA_LEN` | FR-005.14a | T451 | T478 |
| T480 | `src/rt/sqlca.c` — populate the registered storage after each statement, up to seven codes | FR-005.15 | T453 | T479 |
| T481 | `src/rt/sqlca.c` — `getinfolist` item copying, in item order | FR-005.30 | T451, T452 | T480 |
| T482 | `src/rt/sqlca.c` — item 22's sign inversion (SD-4) | FR-005.23b | T454 | T481 |
| T483 | `src/rt/sqlca.c` — the documented 8510–8517 misuse codes | FR-005.30 | T456 | T481 |
| T484 | `src/rt/diag.c` — hook population into the statement path so every statement refreshes the area | FR-005.1 | T450 | T480 |
| T485 | `src/rt/diag.c` — file-system detail for `SQLCAFSCODE` | FR-005.31 | T455 | T484 |

## Phase D — diagnostics

| ID | Task | Code | Reqs | Deps |
|----|------|------|------|------|
| T490 [P] | `INCLUDE STRUCTURES` after an `INCLUDE SQLCA` | `ESQLC-5001` | FR-005.10 | T477 |
| T491 [P] | `INCLUDE STRUCTURES` omitted — version 2 assumed, informational | `ESQLC-5006` | FR-005.10 | T477 |
| T492 [P] | `WHENEVER` action names an invalid identifier | `ESQLC-5008` | FR-005.4 | T475 |

## Phase E — documentation and registry

| ID | Task | Reqs | Deps |
|----|------|------|------|
| T500 | Mark the slice's requirement rows in `docs/traceability.md` as `tested` | — | Phase D |
| T501 | **Update `DIV-041`** — registration strengthens it: copies and `EXTERNAL` sharing now behave as §9 p.9-3 describes, which the original entry did not guarantee | — | T479 |
| T502 | Re-examine SD-1, SD-2, SD-4, SD-5, SD-6 against what was built; record drift as a defect, not precedent | — | Phase C |
| T503 | Record plainly that `WHENEVER SQLWARNING` was **never fired at runtime**, only verified structurally, and that this is `DIV-042`'s consequence | — | T430 |
| T504 | Confirm `diag_registry` is clean — every new code registered before it is emitted | — | Phase D |
| T505 | Reconcile the slice's non-proof list against the as-built state | — | Phase D |
| T506 | Run `/speckit.analyze` including the Principle VIII slice checks | — | T500–T505 |

## Critical path

```
Preprocessor  T400 → T430 → T470 → T471 → T472 → T473 → T474 → T476 → T477
Runtime       T400 → T416 → T478 → T479 → T480 → T481 → T482 → T484 → T485
Converge      → live T450–T456 → mutations T460–T463 → T505
```

Nine sequential tasks each, as in Gate 3 — the two halves proceed in parallel
once `T478` lands the header.

**`T470` is the most blocking**: `T471`, `T472`, `T473` and everything the
emitter does with checks depend on the action table. **`T479`/`T480` are its
runtime counterpart**, since every accessor reads what population wrote.

**`T474` is the riskiest.** It decides which statements get checks appended, and
SD-5 makes that decision non-obvious. Getting it wrong in the permissive
direction means a handler firing on a commit the program never expected guarded;
in the restrictive direction, a handler that silently never runs. `T462` exists
precisely to pin it.

## Exit criteria

- [ ] All ten slice criteria demonstrated — T450–T456, T430, T436
- [ ] Precedence order verified structurally, with its limitation recorded — T430, T503
- [ ] `SQLCA` is exactly 430 bytes with the eye-catcher leading — T436
- [ ] A copied `SQLCA` yields the same items as the original — T452
- [ ] Item 22's inversion pinned in both directions — T454
- [ ] `WHENEVER` does not fire on transaction control — T434
- [ ] A missing `GOTO` label reports at the `WHENEVER` line — T444
- [ ] All four mutation checks fail their intended guard; T463 fails only T452 — T460–T463
- [ ] Gates 1–3 fixtures still pass unchanged — full suite
- [ ] Three diagnostics fire at correct code, line, column — T490–T492
- [ ] Tier 1 green with no MariaDB present; `diag_registry` clean — T504
- [ ] `DIV-041` updated to record what registration guarantees — T501
- [ ] `/speckit.analyze` clean, slice checks included — T506
