# Tasks: Preprocessor core & pipeline

**Spec:** `specs/001-preprocessor-core/spec.md` · **Plan:** `specs/001-preprocessor-core/plan.md`

## Phase A — Fixtures and harness

| ID | Task | Reqs | Deps |
|----|------|------|------|
| T001 | Build system: `src/pp` target producing `esqlc` binary; no MariaDB dependency | NFR-001.2 | — |
| T002 | Golden-file test runner: `.sqlc` → `.expected.c` with whitespace normalisation, diff on failure | NFR-001.2 | T001 |
| T003 | Negative-test runner: `.sqlc` → `.expected.diag` asserting code, line, column | NFR-001.3 | T001 |
| T004 | Stub runtime `tests/stub/esqlc_stub.c` recording call name and arguments | NFR-001.2 | T001 |
| T005 [P] | Compile-and-check harness: emit, compile with the pinned C compiler, assert reported error line | FR-001.18 | T001 |

## Phase B — Failing tests

| ID | Task | Reqs | Deps |
|----|------|------|------|
| T010 [P] | `scan_multiline_comment_string.sqlc` — AS-001.1 | FR-001.1, .2, .4, .6 | T002 |
| T011 [P] | `negative/nested.sqlc`, `negative/unterminated.sqlc` | FR-001.3 | T003 |
| T012 [P] | `negative/c_comment.sqlc`, `negative/single_quote.sqlc` | FR-001.5, .6 | T003 |
| T013 [P] | `negative/no_pragma.sqlc`, `negative/pragma_not_first.sqlc` — AS-001.2 | FR-001.7 | T003 |
| T014 [P] | `pragma_via_cli` case; `negative/unknown_option.sqlc`; `pragma_char_options.sqlc` | FR-001.8, .9, .10 | T002, T003 |
| T015 [P] | `negative/placement.sqlc` covering all three position classes — AS-001.3 | FR-001.11, .12, .13, .14 | T003 |
| T016 [P] | `negative/unknown_keyword.sqlc`, `negative/unimplemented.sqlc` | FR-001.15 | T003 |
| T017 [P] | `hostvar_forms.sqlc` — AS-001.6; `negative/hostvar_define.sqlc` | FR-001.16, .17 | T002, T003 |
| T018 [P] | `line_fidelity.sqlc` — AS-001.4 | FR-001.18 | T005 |
| T019 [P] | `passthrough_lookalikes.sqlc` — AS-001.5 | FR-001.19 | T002 |
| T020 [P] | `listing_sqlmap.sqlc`; `whenever_scope.sqlc` — AS-001.7 | FR-001.20, .21, .22 | T002 |
| T021 [P] | `sql_source_include.sqlc` plus a cycle case | FR-001.23 | T002, T003 |
| T022 [P] | `opaque_body.sqlc` — asserts a body with unknown-to-us SQL passes through to the handler intact | NFR-001.1 | T002 |
| T023 | Position-class edge-case corpus: 10+ cases documenting accepts and rejects of the heuristic | FR-001.11..14 | T003 |

## Phase C — Implementation

| ID | Task | Reqs | Makes pass | Deps |
|----|------|------|-----------|------|
| T030 | `src/pp/diag.c` — code registry, file:line:col formatting, policy table | NFR-001.3 | T003 | T003 |
| T031 | `src/pp/source.c` — reader, inclusion stack, name/line tracking, depth limit and cycle detection | FR-001.23 | T021 | T030 |
| T032 | `src/pp/scan.c` — region split and SQL lexical rules | FR-001.1..6 | T010, T011, T012 | T031 |
| T033 | `src/pp/context.c` — brace depth, position class heuristic | FR-001.11..13 | T023 | T032 |
| T034 | `src/pp/pragma.c` — pragma parse, CLI equivalent, option table | FR-001.7..10 | T013, T014 | T032 |
| T035 | `src/pp/hostvar.c` — all reference forms, `#define` LHS rejection | FR-001.16, .17 | T017 | T032 |
| T036 | `src/pp/dispatch.c` — handler table, position check, `ESQLC-1012` naming the owning feature | FR-001.14, .15 | T015, T016 | T033, T034 |
| T037 | `src/pp/whenever.c` — condition→action scope state, exposed at each statement site | FR-001.22 | T020 | T036 |
| T038 | `src/pp/emit.c` — verbatim C, generated block insertion, `#line` restoration | FR-001.18, .19 | T018, T019 | T036 |
| T039 | `src/pp/listing.c` — `SQLMAP` map incl. host object SQL version, `WHENEVERLIST` after each statement | FR-001.20, .21 | T020 | T037, T038 |
| T040 | Opaque body plumbing: body span carried to handlers unmodified | NFR-001.1 | T022 | T036 |

## Phase D — Diagnostics

One task per spec diagnostics row. Each asserts the code fires at the right
position with the right message.

| ID | Task | Code | Deps |
|----|------|------|------|
| T050 [P] | Nested construct | `ESQLC-1001` | T032 |
| T051 [P] | Unterminated construct at EOF | `ESQLC-1002` | T032 |
| T052 [P] | C comment in SQL region | `ESQLC-1003` | T032 |
| T053 [P] | Single-quoted string in SQL region | `ESQLC-1004` | T032 |
| T054 [P] | Missing pragma | `ESQLC-1005` | T034 |
| T055 [P] | Pragma not first | `ESQLC-1006` | T034 |
| T056 [P] | Unknown pragma option | `ESQLC-1007` | T034 |
| T057 [P] | Wrong position class | `ESQLC-1008` | T036 |
| T058 [P] | Unrecognised keyword | `ESQLC-1009` | T036 |
| T059 [P] | Host variable is a `#define` LHS | `ESQLC-1010` | T035 |
| T060 [P] | `SQL SOURCE` file not found | `ESQLC-1011` | T031 |
| T061 [P] | No handler yet, naming the owning feature | `ESQLC-1012` | T036 |

## Phase E — Documentation and registry

| ID | Task | Reqs | Deps |
|----|------|------|------|
| T090 | Mark §3 and the 001-owned §6 rows in `docs/traceability.md` as `tested` | — | Phase D |
| T091 | Resolve Q2: record the frozen `#pragma SQL` option set; register a new divergence (allocating its ID at that point) if `CPG` contradicts it | — | Phase C |
| T092 | Document the position-class heuristic's accepts/rejects in `docs/reference/directives-and-statements.md` | FR-001.11..14 | T033, T023 |
| T093 | Prove the Phase 1 gate path end to end: `INSERT … VALUES (:hv)` against the stub, then note the handoff to 003 | NFR-001.1 | T040 |

## Exit criteria

- [ ] Every spec requirement has a passing test
- [ ] All twelve diagnostics have a negative test that fires them at the correct position
- [ ] `line_fidelity` passes against the real pinned C compiler, not by inspection
- [ ] Position-class corpus documented, with known-accepts explicitly listed
- [ ] Whole suite runs green with no MariaDB present
- [ ] `docs/traceability.md` §3 rows no longer `spec`
- [ ] Q1–Q3 resolved; no unresolved open questions
- [ ] `/speckit.analyze` clean
