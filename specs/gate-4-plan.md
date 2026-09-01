# Technical Plan: Gate 4 (WHENEVER and the SQLCA)

**Slice:** `specs/gate-4.md` · **Status:** Draft
**Specs:** 001 (Ready), 002, 003, 004, 005 (Clarifying) — planned under Principle VIII

Preconditions verified: enumerated requirement subset ✓; avoidance table covering
every open question in all five touched specs ✓; SD-1, SD-2 carried and SD-4,
SD-5, SD-6 added, all marked provisional ✓; non-proof section ✓.

Planning added two open questions to 005 before a line of this plan was written —
Q9 and Q10, both narrowed by slice decisions. See §8.

## 1. Approach

Three mechanisms, two of them new in kind.

**`WHENEVER` is emitter state, not an emitted construct.** The directive itself
produces no code. It updates a three-entry table — one action per condition — and
every *subsequent* applicable statement appends checks from that table, in the
published order, after its `sqlcode` assignment. State is per-condition: setting
`SQLERROR` leaves `NOT FOUND` untouched. 001's FR-001.22 has required the scanner
to track this since Gate 1; Gate 4 is where it is finally consumed.

**The runtime writes into the program's `SQLCA`, rather than holding its own.**
`INCLUDE SQLCA` emits the 430-byte declaration *and* a registration call, so the
runtime populates that storage after each statement. This is the decision that
keeps `DIV-041` honest: the layout stays private, but the data genuinely lives in
the struct, so a program that copies it with `SQLCA_LEN` or shares it `EXTERNAL`
— both of which §9 p.9-3 explicitly discusses — gets what it expects. An
accessor-reads-runtime-state design would have made every saved copy an empty
430-byte husk.

**Accessors are scoped to the non-rendering pair.** `SQLCAGETINFOLIST` and
`SQLCAFSCODE` need no message catalogue; `SQLCADISPLAY` and `SQLCATOBUFFER` do,
which is 005 Q6 and out of scope.

## 2. Alternatives rejected

| Alternative | Why rejected |
|-------------|--------------|
| Accessors read runtime state; the emitted `SQLCA` is inert storage | Simpler, and it silently breaks the two things §9 says programs do with the structure: copy it and share it `EXTERNAL`. A saved copy would carry nothing. |
| Emit `WHENEVER` checks as a called helper rather than inline `if`s | A helper cannot `goto` a label in the caller, and `GOTO` is one of the four required actions. |
| Track a single "current action" rather than one per condition | §9 p.9-6's precedence table tests three conditions independently in one sequence, so they must be independently settable. |
| Parse enough C to verify a `GOTO` label exists | The preprocessor has never parsed C and should not start for this. See the risk table for what is diagnosed instead. |
| Implement the full `INCLUDE STRUCTURES` version matrix now | Only the default-to-version-2 path is needed to declare an `SQLCA`; the matrix is FR-005.8/.9/.11/.12/.13 and belongs to a later slice. |

## 3. Components

| Component | Path | Change | Slice scope |
|-----------|------|--------|-------------|
| WHENEVER state | `src/pp/whenever.cc` | **new file** — per-condition table, check emission | three conditions, four actions |
| Dispatcher | `src/pp/dispatch.cc` | `WHENEVER` and `INCLUDE SQLCA` implemented; `INCLUDE SQLSA`/`SQLDA` keep `ESQLC-1012` | — |
| Emitter | `src/pp/emit.cc` | invoke check emission after applicable statements; emit the `SQLCA` declaration and registration | SD-5 governs "applicable" |
| Runtime: SQLCA | `src/rt/sqlca.c` | **new file** — registration, population, item accessors | numeric items |
| Runtime: diag | `src/rt/diag.c` | populate the registered `SQLCA` after each statement | — |
| ABI header | `include/esqlc.h` | two new entry points | — |
| Contract | `specs/003-…/contracts/` | the same two, same change (Principle V) | — |

Seven components, two new source files.

## 4. Runtime ABI surface

**Two new entry points.**

```c
/* Register the program's SQLCA so the runtime populates it after each
   statement. Emitted by INCLUDE SQLCA. `len` must equal SQLCA_LEN; a mismatch
   means the program and the runtime disagree about the structure and is an
   error rather than something to truncate into. */
int esqlc_sqlca_register(void *sqlca, size_t len);

/* SQLCAGETINFOLIST: copy a caller-selected subset of the diagnostic area into
   `buf`, in item order. Returns the documented 8510-8517 codes on misuse. */
int esqlc_sqlca_getinfolist(const int *items, int n_items,
                            void *buf, size_t buf_len);
```

`SQLCAFSCODE` maps onto the existing `esqlc_fs_detail`; no new entry point.

Both land in the 003 contract in this change. `contract_sync` will report them as
*planned* until implementation, which is the state Principle V mandates — the
check was made asymmetric during Gate 3 for exactly this.

## 5. Data structures

The `SQLCA` is the first SQL/MP structure the project generates, so Constitution
VI applies for the first time in earnest.

```c
_Static_assert(sizeof(struct sqlca_type) == 430, "SQLCA_LEN");
_Static_assert(offsetof(struct sqlca_type, eye_catcher) == 0, "eye-catcher leads");
```

Under `DIV-041` the internal offsets are ours, so `offsetof` is asserted only for
the eye-catcher, which is the one field the manual places. The **total is
load-bearing** and asserted exactly: programs allocate copies with `SQLCA_LEN`.

The runtime must not assume its own layout when writing into registered storage —
it writes through the same private accessors the read path uses, so a future
layout change cannot desynchronise the two halves.

## 6. Requirement → component map

| Requirement | Component(s) | Test |
|-------------|--------------|------|
| FR-001.13 `WHENEVER` accepted anywhere | dispatch | `whenever_positions` |
| FR-001.15 dispatch; unimplemented refused | dispatch | `negative/unimplemented_sqlsa` |
| FR-001.22 scope state exposed | whenever | `whenever_scope` |
| NFR-001.1 opaque bodies | emit | `opaque_body_unchanged` |
| FR-003.1 `esqlc_*` calls only | emit | `abi_only_symbols` |
| FR-003.2 no MariaDB type in the header | include/esqlc.h | `abi_isolation` |
| FR-003.3 signatures mirrored in the contract | contract | `contract_sync` |
| FR-005.1 `sqlcode` classes | rt/diag | `rt/whenever_flow` |
| FR-005.3 three conditions | whenever | `whenever_conditions` |
| FR-005.4 four actions | whenever | `whenever_actions` |
| FR-005.5 precedence order | whenever | `whenever_precedence` |
| FR-005.6 source-order scoping and re-specification | whenever | `whenever_scope` |
| FR-005.7 applies to DML, DCL, DDL | emit | `whenever_applies_to` |
| FR-005.10 default version 2 plus its message | emit | `negative/no_include_structures` |
| FR-005.14 `SQLCA_LEN` 430, eye-catcher `CA` | emit | `sqlca_size` |
| FR-005.14a private layout, accessor-only | rt/sqlca | `rt/sqlca_items` |
| FR-005.15 up to seven codes | rt/sqlca | `rt/sqlca_seven_codes` |
| FR-005.23b item 22 sign inversion | rt/sqlca | `rt/sqlca_item22_sign` |
| FR-005.30 `SQLCAGETINFOLIST` subset and error codes | rt/sqlca | `rt/sqlca_items`, `rt/negative/sqlca_misuse` |
| FR-005.31 `SQLCAFSCODE` detail | rt/diag | `rt/sqlca_fscode` |

20 scoped requirements, all mapped exactly once. FR-002.28 was dropped from
the slice during task derivation: Gate 2 implements it and this slice changes
nothing about retrieval, so it could only have had a regression test and no
honest Phase C task.

## 7. Test strategy

**Tier 1** — the precedence order is verified *structurally* here, because it
cannot be verified end-to-end: firing `SQLWARNING` needs a warning value, and
those are `DIV-042`. A spec assertion checks the emitted sequence is
`sqlcode == 100`, then `sqlcode < 0`, then `sqlcode > 0 && sqlcode != 100`, in
that order, and that a `CONTINUE` condition emits nothing.

**Tier 2** — `whenever_flow` (handler fires on failure, not on success), a
`GOTO` that transfers control on 100, `CONTINUE` disabling a previously active
action, and the `SQLCA` accessors after a provoked failure.

**Mutation checks:**

| Injected defect | Must fail |
|---|---|
| Reorder the checks to SQLERROR before NOT FOUND | `whenever_precedence` |
| Let a `WHENEVER` action leak past its supersession | `whenever_scope` |
| Apply `WHENEVER` to `COMMIT WORK` (violating SD-5) | `whenever_applies_to` |
| Have accessors read runtime state instead of the registered struct | `rt/sqlca_copy_survives` — a test that copies the `SQLCA` and reads the copy |

That last one guards the §1 design decision directly, and is the reason a copy
test exists at all.

## 8. Risks

| Risk | Impact | Mitigation |
|------|--------|------------|
| A `GOTO` action names a label the preprocessor cannot see. C rejects `goto` into a block and jumping over VLA initialisations, so some emitted jumps are invalid C for reasons invisible here | The customer sees a C error about a label rather than an ESQL diagnostic | Accepted and stated: `ESQLC-5008` fires only when the action is not a valid C identifier. `#line` fidelity means the C error points at the `WHENEVER` line, which is the best available outcome. A fixture pins that the error lands on the right line |
| `WHENEVER` checks are appended inside the `do { … } while (0)` each statement already emits. A `goto` out of that is legal C; a `continue` or `break` would not be | A future action form that used `break` would silently bind to the wrapper | Only `goto` and a function call are emitted. Recorded so the wrapper's existence is a known constraint on future action forms |
| SD-5 excludes transaction control from `WHENEVER`. If the manual's list was an omission rather than a deliberate exclusion, a customer's error handler silently stops firing on a failing `COMMIT WORK` | A handler that never runs is invisible; the program looks like it succeeded | `whenever_applies_to` pins the behaviour in both directions so a reversal is a visible change. 005 Q9 stays open, and `SQLRM` settles it |
| The registered `SQLCA` is written by the runtime after every statement, including ones the program never checks | A stale-looking area if registration is missed, or a dangling write if the struct goes out of scope | `esqlc_sqlca_register` validates `len == SQLCA_LEN`; the emitted declaration is file-scope, so lifetime is the program's. A function-scope `SQLCA` is out of slice and refused |
| Seven codes in a 430-byte area, with a private layout and no published field list | Under-provisioning the private layout is invisible until a statement produces more codes than fit | `rt/sqlca_seven_codes` provokes multiple diagnostics and asserts all are retrievable, so the capacity claim of FR-005.15 is exercised rather than assumed |

## 9. Divergences introduced

None new. `DIV-041` is *strengthened* rather than extended: the decision to write
into the program's registered storage means copies and `EXTERNAL` sharing behave
as §9 describes, which the original entry did not guarantee. Its detection and
migration notes should be updated to say so when this lands.

SD-1, SD-2, SD-4, SD-5, SD-6 all carry as provisional slice decisions.
