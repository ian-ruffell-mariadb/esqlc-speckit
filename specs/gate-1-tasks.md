# Tasks: Gate 1

**Slice:** [gate-1.md](gate-1.md) · **Plan:** [gate-1-plan.md](gate-1-plan.md)

Rules in force: tests before implementation (Principle IV); every Phase C task
names the Phase B task it makes pass; `[P]` only where tasks touch disjoint files
with no dependency between them; every task lists requirement IDs.

---

## Implementation status — 2026-08-28

**Gate 1 is green end to end.** Branch `gate-1-implementation`.

| Area | State |
|---|---|
| Phase A harness and fixtures | done |
| Tier 1 suite (golden, negative, isolation, contract) | **4/4 automated, passing** |
| Preprocessor Phase C | done for slice scope |
| Runtime Phase C | done for slice scope |
| Tier 2 live-database checks | **8/8 automated, passing; skips cleanly with no server** |
| Phase D diagnostics | 6/6 firing at correct code, line, column |
| Phase E | partial |

`ctest` is 6/6 with a server and 5/6 + 1 skip without one. Under
`-DESQLC_NO_MARIADB=ON` it is 5/5 with the runtime target and `tier2_live`
absent entirely — which is the shape CI's first job runs in.

**CI (T084–T086) is in place**: `.github/workflows/ci.yml`, two jobs.
`tier1-no-mariadb` runs in a `debian:bookworm-slim` container — not the hosted
runner image, which ships a MySQL client and would have made the job prove
nothing — and asserts up front that no MariaDB tool, header or library is
present before building. `tier2-live` runs against a MariaDB service container
and asserts Tier 2 did **not** skip, since a silent skip would turn a broken
database into a green build.

### Process debts — both now paid (2026-08-28)

1. ~~**Principle IV was not honoured for most of Phase C.**~~ **Fixed.** The
   golden `.expected.c` files were snapshots, asserting what the code does
   rather than what the spec requires. They are retained as regression guards,
   and `tests/harness/spec_assertions.py` now carries **26 specification
   assertions, each naming the requirement it derives from** — width/capacity
   separation, placeholder-to-descriptor correspondence, no host-variable
   reference surviving into statement text, no value appearing in statement
   text, pragma consumption, verbatim C, `#line` presence, and the span-capture
   rules for strings and comments.

   Proven load-bearing by mutation: injecting `width = capacity` — the mistake
   a `strlen`- or `sizeof`-based implementation makes — fails exactly the two
   FR-002.30 assertions, by name. Writing them also caught a defect in the
   FR-003.1 check itself, which had been matching identifiers inside comments
   and string literals.

2. ~~**Tier 2 is not wired into `ctest`.**~~ **Fixed.**
   `tests/harness/run_tier2.sh` automates all eight live checks and is a ctest
   target. It reads the same `ESQLC_*` variables the runtime resolves, so the
   harness cannot test a different database than the code sees, and it exits 77
   (`SKIP_RETURN_CODE`) when no server is reachable, preserving NFR-001.2.

### Remaining deviations

3. **C11, not the C99 the plan named.** Principle VI mandates `_Static_assert`
   on the ABI structure and C99 has no such facility, so the plan's language
   choice was incompatible with the constitution. C11 resolves it.
4. **Component consolidation.** The plan named `context.cc`, `pragma.cc` and
   `stmt.cc`; position tracking and pragma detection live in `scan.cc`, and the
   statement handlers in `emit.cc`. Six pp files rather than nine.
5. **`ESQLC-1014` was invented during implementation** for "host variable
   referenced but not declared". No spec defines it. It needs registering in
   001 or 002 before it can be considered legitimate.

### Defects found and fixed while building

- `#line` after a generated block carried the construct's own line rather than
  the following line, so every line number after an embedded statement was
  wrong. Caught by the line-fidelity check, not by inspection.
- The golden runner compared embedded source paths, making it sensitive to the
  cwd the tool was invoked from — it passed by hand and failed under `ctest`.
- The gate fixture reported `sqlcode` *after* `ROLLBACK WORK`, which resets it.
  The fixture now saves it first, per §9 p.9-13.

### Found and registered, not fixed

- **`DIV-052`** — MariaDB strips trailing blanks from `CHAR` on retrieval unless
  `PAD_CHAR_TO_FULL_LENGTH` is set. Storage is faithful; retrieval is not.
  Gate 1 has no `SELECT` so it does not hit this, but feature 004 will.

---

Phase D covers the six diagnostics the slice exercises — the subset of 001/002/003's
diagnostic tables reachable from the gate fixtures. The remaining diagnostics in
those specs are out of slice scope and are served by `ESQLC-1012` (T092), which
refuses every unimplemented construct by name rather than letting it no-op.

## Phase A — fixtures and harness

| ID | Task | Reqs | Deps |
|----|------|------|------|
| T001 | Build system: `esqlcpp` (C++17) and `libesqlc` (C99) targets; assert the preprocessor target links no MariaDB library | NFR-001.2 | — |
| T002 | `include/esqlc.h` declarations only — entry-point prototypes, `ESQLC_T_*`/`ESQLC_DIR_*`, `esqlc_hostvar_t`, its two static assertions. No definitions | FR-003.1, FR-003.2 | T001 |
| T003 | Golden-file runner: `.sqlc` → `.expected.c`, whitespace-normalised, unified diff on failure | — | T001 |
| T004 | Negative-diagnostic runner: asserts code, line **and** column against `.expected.diag`; a right code at a wrong position fails | NFR-001.3 | T001 |
| T005 [P] | Compile-isolation harness: compile emitted C with only `include/esqlc.h` reachable, no MariaDB header on the include path | FR-003.2 | T002 |
| T006 [P] | Contract-sync checker: diff `include/esqlc.h` declarations against `specs/003-.../contracts/esqlc-abi.md`, fail on drift | FR-003.3 | T002 |
| T007 [P] | Stub runtime `tests/stub/esqlc_stub.c`: implements the ABI, records call name and arguments, touches no database | NFR-001.2 | T002 |
| T008 | MariaDB schema fixture: `parts` table per the slice — `part_num SMALLINT SIGNED NOT NULL`, `part_desc CHAR(18) NOT NULL`, PK `part_num` | — | T001 |
| T009 | Second-connection verification helper: reads committed state on a connection distinct from the one under test | — | T008 |
| T010 | Primary gate fixture `tests/conformance/gate-1/insert.sqlc` exactly as listed in the slice | — | T008 |
| T011 [P] | Fixture variants: `rollback.sqlc` (PK violation), `underfilled.sqlc` (short array), `injection.sqlc` (`'; DROP TABLE` value) | — | T010 |

## Phase B — failing tests

All must fail for the right reason before any Phase C task starts. A test that
passes against no implementation is broken, not done.

### Tier 1 — preprocessor, no database

| ID | Task | Reqs | Deps |
|----|------|------|------|
| T020 [P] | `scan_basic` — `EXEC SQL` … `;` recognised as one construct | FR-001.1 | T003 |
| T021 [P] | `scan_multiline` — construct spanning five lines, no continuation marker | FR-001.2 | T003 |
| T022 [P] | `hostvar_spans` — `:name` spans captured; **`"a :b"` and `-- :c` must not be captured** | FR-001.16 | T003 |
| T023 [P] | `opaque_body` — body text unknown to the preprocessor reaches the handler intact | NFR-001.1 | T003 |
| T024 [P] | `passthrough` — C regions byte-identical apart from `#line` and pragma expansion | FR-001.19 | T003 |
| T025 [P] | `line_fidelity` — deliberate C error after three embedded statements reports the original line, via the pinned real compiler | FR-001.18 | T005 |
| T026 [P] | `declare_section` — `short` and `char[19]` recognised; identifiers accepted | FR-002.1, FR-002.2 | T003 |
| T027 [P] | `type_char` — `CHAR(18)` ↔ `char[19]`; descriptor carries `width` 18, `capacity` 19 | FR-002.3 | T003 |
| T028 [P] | `type_int_widths` — `short` emits a 16-bit descriptor with `is_signed` set | FR-002.9 | T003 |
| T029 [P] | `static_asserts` — emitted declarations carry width and signedness assertions that fail if a width drifts | NFR-002.2 | T003 |
| T030 [P] | `abi_only_symbols` — emitted C references no symbol outside the `esqlc_` prefix | FR-003.1 | T005 |
| T031 [P] | `negative/no_pragma` | FR-001.7 | T004 |
| T032 [P] | `negative/decl_in_exec` — `BEGIN DECLARE SECTION` in executable position | FR-001.11 | T004 |
| T033 [P] | `negative/exec_in_decl` — `INSERT` at file scope | FR-001.12 | T004 |
| T034 [P] | `negative/unimplemented` — `EXEC SQL SELECT` names feature 004 | FR-001.15 | T004 |
| T035 | `no_mariadb_suite` — the whole Tier 1 suite runs green with no MariaDB installed | NFR-001.2 | T020–T034 |
| T036 | `contract_sync` test — header and contract agree; mutating one fails the test | FR-003.3 | T006 |
| T037 | `abi_isolation` test — emitted C compiles with no MariaDB header reachable | FR-003.2 | T005 |

### Tier 2 — runtime, live MariaDB

| ID | Task | Reqs | Deps |
|----|------|------|------|
| T040 [P] | `rt/direct_abi` — runtime driven by handwritten C against the ABI, no preprocessor involved | NFR-003.1 | T002, T008 |
| T041 [P] | `rt/implicit_connect` — no connect statement; connection resolved from configuration | FR-003.4 | T008 |
| T042 [P] | `rt/config_precedence` — environment beats file beats compiled; `esqlc_context_origin` asserted per setting | FR-003.19 | T008 |
| T043 [P] | `rt/single_connection` — one connection per process; state explicitly process-scoped | FR-003.16, NFR-003.3 | T008 |
| T044 [P] | `rt/txn_commit` — committed row visible on the second connection | FR-003.6 | T009 |
| T045 [P] | `rt/txn_rollback` — rolled-back row **absent** on the second connection; proves the transaction is real, not autocommit | FR-003.6, FR-003.8 | T009 |
| T046 [P] | `rt/char_verbatim` — 18 program-supplied bytes land in the column unaltered, no terminator stored | FR-002.30 | T009 |
| T047 [P] | `rt/underfilled_stores_null` — a short array **stores its null byte**; a `strlen`-based bind fails only this test | FR-002.31 | T009 |
| T048 [P] | `rt/injection_literal` — `'; DROP TABLE` stored literally; no second statement executes | FR-003.10, NFR-003.2 | T009 |
| T049 [P] | `rt/sqlcode_zero` — successful insert yields `sqlcode` 0 | FR-003.13 | T008 |
| T050 [P] | `rt/sqlcode_error` — PK violation yields a negative `sqlcode`, process still running | FR-003.13 | T008 |
| T051 [P] | `rt/negative/bad_config` — deliberately invalid configuration; first statement reports via `sqlcode` and the process is still running | FR-003.5 | T008 |
| T052 [P] | `rt/negative/second_thread` — ABI entered from a second thread is refused, and no database work occurs | FR-003.17 | T008 |
| T053 [P] | `rt/negative/creds_in_source` — credentials offered by an unsupported route are refused | FR-003.21 | T008 |
| T054 | `diag_positions` — every negative case in Tiers 1 and 2 asserts code, line and column; a right code at a wrong position fails | NFR-001.3 | T031–T034, T051–T053 |

## Phase C — implementation

| ID | Task | Reqs | Makes pass | Deps |
|----|------|------|-----------|------|
| T060 | `src/pp/diag.cc` — code registry, `file:line:col` formatting | NFR-001.3 | T004 | Phase B |
| T061 | `src/pp/source.cc` — file read, line and column tracking | NFR-001.3 | T004 | T060 |
| T062 | `src/pp/scan.cc` — C/SQL region split, `--` comments, `"` strings, `;` terminator | FR-001.1, FR-001.2 | T020, T021 | T061 |
| T063 | `src/pp/scan.cc` — host-variable span capture **in the same lexer pass**, so strings and comments are excluded by construction | FR-001.16 | T022 | T062 |
| T064 | `src/pp/scan.cc` — body carried as an opaque span, untouched apart from recorded host-var offsets | NFR-001.1 | T023 | T063 |
| T065 | `src/pp/context.cc` — brace depth and statement-boundary tracking → position class | FR-001.11, FR-001.12 | T032, T033 | T062 |
| T066 | `src/pp/pragma.cc` — `#pragma SQL` recognition and mandatory-position check | FR-001.7 | T031 | T062 |
| T067 | `src/pp/decl.cc` — parse `short`, build a 16-bit signed descriptor | FR-002.9 | T028 | T062 |
| T068 | `src/pp/decl.cc` — parse `char[n]`, set `width` = n−1 and `capacity` = n | FR-002.1, FR-002.2, FR-002.3 | T026, T027 | T067 |
| T069 | `src/pp/dispatch.cc` — handler table, position enforcement, `ESQLC-1012` naming the owning feature for everything out of slice | FR-001.15 | T034 | T065, T066 |
| T070 | `src/pp/stmt.cc` — `INSERT` handler: emit body with recorded spans replaced by `?`, plus the ordered descriptor array | FR-003.10 | T048 | T069, T068 |
| T071 | `src/pp/stmt.cc` — `BEGIN`/`COMMIT`/`ROLLBACK WORK` handlers emitting the three txn calls | FR-003.6 | T044, T045 | T069 |
| T072 | `src/pp/emit.cc` — verbatim C regions, generated-block insertion, `#line` restoration | FR-001.18, FR-001.19 | T024, T025 | T069 |
| T073 | `src/pp/emit.cc` — emit width and signedness static assertions for each host variable | NFR-002.2 | T029 | T072, T068 |
| T074 | `src/pp/emit.cc` — emit only `esqlc_`-prefixed references | FR-003.1 | T030 | T072 |
| T075 | `src/rt/context.c` — env → file → compiled resolution; `esqlc_context_origin` | FR-003.4, FR-003.19 | T041, T042 | T002 |
| T075a | `src/rt/context.c` — an unresolvable connection is reported through `sqlcode` at the first statement; the process is never aborted | FR-003.5 | T051 | T075 |
| T076 | `src/rt/context.c` — credentials routed through MariaDB's own option-file mechanisms only | FR-003.21 | T053 | T075 |
| T077 | `src/rt/session.c` — lazy connect, one connection per process, teardown | FR-003.16, NFR-003.3 | T043 | T075 |
| T078 | `src/rt/session.c` — single-thread affinity check | FR-003.17 | T052 | T077 |
| T079 | `src/rt/exec.c` — bind descriptors positionally; bind exactly `width` bytes verbatim, never `strlen`, never pad | FR-002.30, FR-002.31 | T046, T047 | T077 |
| T080 | `src/rt/exec.c` — execute the placeholder statement; no SQL text manipulation anywhere in the runtime | FR-003.10, NFR-003.2 | T048 | T079 |
| T081 | `src/rt/diag.c` — `sqlcode` classes: 0, 100, negative, positive non-100 | FR-003.13 | T049, T050 | T080 |
| T082 | `src/rt/txn.c` — begin, commit, rollback; commit and rollback release resources | FR-003.6, FR-003.8 | T044, T045 | T080 |
| T083 | `libesqlc` satisfies the ABI standalone, exercised without the preprocessor | NFR-003.1 | T040 | T082 |
| T084 | CI job runs the Tier 1 suite in an image with **no MariaDB installed**, so a client dependency creeping into the preprocessor breaks the build rather than being caught by review | NFR-001.2 | T035 | T074 |
| T085 [P] | Wire `abi_isolation` into CI so a MariaDB type reaching the ABI header fails the build | FR-003.2 | T037 | T084 |
| T086 [P] | Wire `contract_sync` into CI so header/contract drift fails the build | FR-003.3 | T036 | T084 |

## Phase D — diagnostics

One task per diagnostic the slice reaches. Each asserts code, message and
position.

| ID | Task | Code | Reqs | Deps |
|----|------|------|------|------|
| T090 [P] | Missing `#pragma SQL` | `ESQLC-1005` | FR-001.7 | T066 |
| T091 [P] | Construct in wrong position class | `ESQLC-1008` | FR-001.11, FR-001.12 | T065 |
| T092 [P] | Recognised statement with no handler in this slice, naming the owning feature | `ESQLC-1012` | FR-001.15 | T069 |
| T093 [P] | Connection unresolvable from configuration — reported via `sqlcode`, process survives | `ESQLC-3001` | FR-003.5 | T075 |
| T094 [P] | ABI entered from a second thread | `ESQLC-3006` | FR-003.17 | T078 |
| T095 [P] | Credentials supplied by an unsupported route | `ESQLC-3009` | FR-003.21 | T076 |

Phase B tasks T051–T053 are the failing counterparts of T093–T095 and are
written with Tier 2: `rt/negative/bad_config` (T051), `rt/negative/second_thread`
(T052), `rt/negative/creds_in_source` (T053).

## Phase E — documentation and registry

| ID | Task | Reqs | Deps |
|----|------|------|------|
| T100 | Mark the slice's requirement rows in `docs/traceability.md` as `tested` — the slice's subset only, not whole sections | — | Phase D |
| T101 | Confirm `DIV-001` and `DIV-002` behaviour matches what the slice implemented; amend either if not | — | Phase C |
| T102 | Re-examine slice decisions SD-1 and SD-2 against what was built; record any drift as a defect, not as precedent | — | Phase C |
| T103 | Record in `specs/gate-1.md` what the green gate did **not** prove, reconciled against the as-built non-proof list | — | Phase D |
| T104 | Run `/speckit.analyze` including the Principle VIII slice checks | — | T100–T103 |

## Critical path

Two chains run in parallel and converge on the live end-to-end criteria:

```
Preprocessor  T001 → T002 → T003/T004 → T060 → T061 → T062 → T063 → T069 → T070 → T072 → T074
Runtime       T001 → T002 → T075 → T077 → T079 → T080 → T082 → T083
Converge      → live criteria T044–T048 → T084 (CI, no MariaDB) → T103 → T104
```

Longest chain is the preprocessor's, at ten sequential tasks. `T062`
(`scan.cc` region split) is the single most blocking task: `T063`, `T065`,
`T066`, `T067` and `T072` all wait on it, and `T063`'s
same-pass span capture is the design bet the whole slice rests on.

The runtime chain can start as soon as `T002` lands, so the two halves are
genuinely concurrent after the header exists — which is the practical payoff of
Principle V.

## Exit criteria

Mapped to the slice's seven criteria plus the added 4a:

- [ ] Fixture preprocesses with no diagnostics — T010, Phase C
- [ ] Emitted C compiles with no MariaDB header reachable — T037
- [ ] Links against the real runtime and runs — T083
- [ ] Row present, 18 bytes verbatim, no terminator stored — T046
- [ ] Under-filled variant stores its null byte — T047
- [ ] `sqlcode` is 0 on success — T049
- [ ] `ROLLBACK WORK` variant leaves the table unchanged — T045
- [ ] `#line` fidelity holds against the real compiler — T025
- [ ] All six diagnostics fire at the correct position — T090–T095
- [ ] Tier 1 suite green with no MariaDB present — T035
- [ ] Header and contract in sync — T036
- [ ] `/speckit.analyze` clean, slice checks included — T104
