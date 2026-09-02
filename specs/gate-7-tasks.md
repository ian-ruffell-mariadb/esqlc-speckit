# Gate 7 tasks — host variable type breadth

**Slice:** [gate-7.md](gate-7.md) · **Plan:** [gate-7-plan.md](gate-7-plan.md)

Phase A fixtures, then Phase B tests, then Phase C implementation. No Phase C
task starts until the Phase B test it names fails for the right reason
(Principle IV).

25 scoped requirements. Every one appears in at least one Phase B and one
Phase C task; the coverage check at the end names the two that are carried
regressions rather than new behaviour, and which Phase C task could break them.

## Phase A — fixtures and harness

| ID | Task | Reqs | Deps |
|----|------|------|------|
| T710 | `schema.sql` — a typed table: `INTEGER`, `BIGINT`, `REAL`, `DOUBLE PRECISION`, `VARCHAR(26)`, `TIMESTAMP` | NFR-002.1 | — |
| T711 [P] | `int_widths.sqlc` — Tier 1: `short`, `int`, `long`, `long long` declarations. **`long` does not compile today**, which is the defect the plan found | FR-002.9 | — |
| T712 [P] | `float_widths.sqlc` — Tier 1: `float` and `double` | FR-002.10 | — |
| T713 [P] | `varchar_layout.sqlc` — Tier 1: `struct { short len; char val[27]; } v;` | FR-002.6 | — |
| T714 [P] | `types_declare_section.sqlc` — Tier 1: unusual but valid C identifiers, all inside the section | FR-002.1, FR-002.2 | — |
| T715 [P] | `rt/int_widths_roundtrip.sqlc` — insert then retrieve each width. **Round-trip, because a wrong outbound width can look right inbound** | FR-002.9, NFR-002.1 | T710 |
| T716 [P] | `rt/float_roundtrip.sqlc` — values chosen so a wrong *width* moves the result far beyond any tolerance | FR-002.10, NFR-002.1 | T710 |
| T717 [P] | `rt/varchar_roundtrip.sqlc` — `len` correct on input and written on output | FR-002.6, FR-002.30, NFR-002.1 | T710 |
| T718 [P] | `rt/varchar_null.sqlc` — a negative indicator on a `VARCHAR` | FR-002.16 | T710 |
| T719 [P] | `rt/timestamp_to_char.sqlc` — a `TIMESTAMP` column into a `char` array, terminator untouched | FR-002.13, FR-002.28 | T710 |
| T720 [P] | `rt/int_overflow_8300.sqlc` — a 32-bit value into a `SMALLINT` column | FR-002.25 | T710 |
| T721 [P] | `negative/unsigned_long_long.sqlc` + `.expected.diag` | FR-002.12 | — |
| T722 [P] | `negative/varchar_len_int.sqlc` + `.expected.diag` — `int len` rather than `short len` | FR-002.21 | — |
| T723 [P] | `negative/struct_not_varchar.sqlc` + `.expected.diag` — a two-field record used as a host variable | FR-002.20 | — |
| T724 [P] | `negative/decimal_refused.sqlc` + `.expected.diag` — the out-of-scope type refuses **by name** | FR-002.9 | — |
| T725 | Extend `seed.sql` with typed rows | NFR-002.1 | T710 |
| T726 | Extend `run_tier2.sh` with the Gate 7 cases | — | T715 |

17 tasks.

## Phase B — failing tests

All must fail for the right reason before any Phase C task starts.

### Tier 1 — preprocessor, no database

| ID | Task | Reqs | Deps |
|----|------|------|------|
| T730 | `int_widths` — each declared type yields its true width: 2, 4, 8. From `sizeof`, never from the type's spelling (`DIV-001`) | FR-002.9 | T711 |
| T731 | `int_widths` — the emitted unit **compiles**. The static assertion must agree with `sizeof`, which is exactly what fails for `long` today | NFR-002.2 | T711 |
| T732 [P] | `int_widths` — every host variable becomes a placeholder; no value in the statement text | FR-003.10 | T711 |
| T733 [P] | `float_widths` — `float` is width 4 and `double` width 8, both `ESQLC_T_FLOAT`; **neither is `ESQLC_T_DATETIME`** | FR-002.10 | T712 |
| T734 | `varchar_layout` — `ESQLC_T_CHAR_VAR`, `addr` is the structure, `capacity` is the declared `val` size, `width` is `capacity - 1` (SD-10) | FR-002.6 | T713 |
| T735 | `varchar_layout` — `offsetof(len) == 0`, `offsetof(val) == 2`, `sizeof(len) == 2`, all asserted in the emitted C | FR-002.6, FR-002.21, NFR-002.2 | T713 |
| T736 [P] | `types_declare_section` — declarations are harvested only inside the section, and any valid C identifier is accepted | FR-002.1, FR-002.2 | T714 |
| T737 [P] | `insert` — `CHAR(l)` → `char v[l+1]` unregressed by the new width path | FR-002.3 | — |
| T738 [P] | `abi_isolation` and `contract_sync` — **unchanged**, because this slice adds no ABI | FR-003.2, FR-003.3 | — |
| T739 [P] | `abi_only_symbols` — the emitted unit calls `esqlc_*` and nothing else | FR-003.1 | T711 |
| T740 [P] | `opaque_body_unchanged` — statement bodies still verbatim | NFR-001.1 | T711 |
| T741 [P] | `update_indicator_assoc` — association unregressed, now with a `VARCHAR` in the list | FR-002.15 | T713 |
| T742 [P] | `negative/unsigned_long_long` — code, line **and** column | FR-002.12 | T721 |
| T743 [P] | `negative/varchar_len_int` — code, line and column | FR-002.21 | T722 |
| T744 [P] | `negative/struct_not_varchar` — code, line and column | FR-002.20 | T723 |
| T745 [P] | `negative/decimal_refused` — `ESQLC-1012` naming feature 002, not a silent bind | FR-002.9 | T724 |

### Tier 2 — live server

| ID | Task | Reqs | Deps |
|----|------|------|------|
| T746 | `rt/int_widths_roundtrip` — all four types survive a round trip | FR-002.9, NFR-002.1 | T715 |
| T747 [P] | `rt/float_roundtrip` — `REAL` and `DOUBLE PRECISION` within tolerance | FR-002.10, NFR-002.1 | T716 |
| T748 | `rt/varchar_roundtrip` — the value and its `len`, both directions | FR-002.6, FR-002.30, NFR-002.1 | T717 |
| T749 [P] | `rt/varchar_null` — the column becomes null and the buffer is not read | FR-002.16 | T718 |
| T750 [P] | `rt/timestamp_to_char` — retrieved as characters, **no terminator appended** | FR-002.13, FR-002.28 | T719 |
| T751 | `rt/int_overflow_8300` — `sqlcode` is `-8300`, the row is **not** stored, and `esqlc_fs_detail` reports the sentinel rather than 1031 (`DIV-054`) | FR-002.25 | T720 |
| T752 [P] | Carried regressions stay green: `cross_family`, `underfilled_stores_null`, `update_injection_literal` | FR-002.22, FR-002.31, NFR-003.2 | T726 |

23 tasks.

## Phase C — implementation

| ID | Task | Reqs | Makes pass | Deps |
|----|------|------|-----------|------|
| T760 | `src/pp/decl.cc` — width comes from the host compiler's `sizeof`, not the type's spelling. **Fixes the `long` defect** | FR-002.9 | T730, T731 | Phase B |
| T761 | `src/pp/decl.cc` — `long long` accepted at width 8 | FR-002.9 | T730 | T760 |
| T762 | `src/pp/decl.cc` — `float` and `double` → `ESQLC_T_FLOAT`, width 4 and 8 | FR-002.10 | T733 | T760 |
| T763 | `src/pp/decl.cc` — recognise the `VARCHAR` structure by shape: two members, `len` then `val`, those names | FR-002.6 | T734 | T760 |
| T764 | `src/pp/decl.cc` — a `len` field that is not `short` is refused | FR-002.21 | T743 | T763 |
| T765 | `src/pp/decl.cc` — a structure that is not the `VARCHAR` shape is refused when used as a host variable | FR-002.20 | T744 | T763 |
| T766 | `src/pp/decl.cc` — the declare-section scope and identifier rules survive the new type table | FR-002.1, FR-002.2, FR-002.3 | T736, T737 | T760 |
| T767 | `src/pp/decl.cc` — out-of-scope types still refuse **by name** rather than binding as something near | FR-002.9 | T745 | T760 |
| T768 | `src/pp/pp.h` — `HostVar` carries the `val` capacity for a `VARCHAR` | FR-002.6 | T734 | T763 |
| T769 | `src/pp/emit.cc` — the `VARCHAR` descriptor: `addr` is the structure, `width`/`capacity` per SD-10 | FR-002.6, FR-003.1, FR-003.10, NFR-001.1 | T734, T732, T739, T740 | T768 |
| T770 | `src/pp/emit.cc` — the `VARCHAR` layout assertions of the plan's section 5 | FR-002.6, NFR-002.2 | T735 | T769 |
| T771 | `src/pp/emit.cc` — `float`/`double` descriptors | FR-002.10 | T733 | T762 |
| T772 | `src/pp/emit.cc` — the integer static assertion uses the same width the descriptor claims | NFR-002.2 | T731 | T760 |
| T773 | `src/pp/emit.cc` — indicator association with a `VARCHAR` in the list | FR-002.15 | T741 | T769 |
| T774 | `src/rt/exec.c` — bind `float` and `double`, and classify them as numeric for the family check | FR-002.10, FR-002.22 | T747, T752 | Phase B |
| T775 | `src/rt/exec.c` — bind a `VARCHAR` input: `len` read from the structure, `val` at offset 2 | FR-002.6, FR-002.30, FR-002.31 | T748 | T774 |
| T776 | `src/rt/exec.c` — write a retrieved `VARCHAR`'s `len` | FR-002.6 | T748 | T775 |
| T777 | `src/rt/exec.c` — a negative indicator on a `VARCHAR` sends null and reads nothing from `val` | FR-002.16 | T749 | T775 |
| T778 | `src/rt/exec.c` — a date-time column binds into a character host variable, no terminator written | FR-002.13, FR-002.28 | T750 | T774 |
| T779 | `src/rt/exec.c` — MariaDB 1264 maps to `sqlcode` `-8300` | FR-002.25 | T751 | T774 |
| T780 | `src/rt/context.c` — guarantee `STRICT_TRANS_TABLES`. Without it 1264 is a warning and the wrong value is stored (`DIV-054`) | FR-002.25 | T751 | T779 |
| T781 | `src/rt/diag.c` — the 8300 file-system detail is the sentinel, never a fabricated 1031 | FR-002.25 | T751 | T779 |
| T782 | `include/esqlc.h` — **verify unchanged.** No new entry point, no signature change, no contract edit | FR-003.2, FR-003.3 | T738 | Phase B |
| T783 | `tests/stub/esqlc_stub.c` — report the new type families so a stub-linked fixture is still legible | FR-003.3 | T738 | T782 |

24 tasks.

## Phase D — diagnostics

One task per diagnostic row this slice touches.

| ID | Task | Code | Reqs | Deps |
|----|------|------|------|------|
| T790 [P] | `unsigned long long` in a unit with embedded SQL. Already emitted since Gate 1 and **never tested**; this is its first fixture | `ESQLC-2001` | FR-002.12 | T760 |
| T791 [P] | A hand-declared `VARCHAR` length field that is not `short` | `ESQLC-2002` | FR-002.21 | T764 |
| T792 [P] | A non-`VARCHAR` structure name used as a host variable | `ESQLC-2003` | FR-002.20 | T765 |

`ESQLC-2005` (`DECIMAL` precision), `ESQLC-2006` (character set),
`ESQLC-2008` (`TYPE AS`) and `ESQLC-2010`/`.2011`/`.2012` (conversion warnings)
get no task: their requirements are out of scope, so the conditions cannot
arise. `ESQLC-2004` and `ESQLC-2009` are already implemented and covered by
carried fixtures. `ESQLC-2007` is discussed in Phase E — it may not be
reachable at all.

## Phase E — documentation and registry

| ID | Task | Reqs | Deps |
|----|------|------|------|
| T800 | Move the slice's rows in `docs/traceability.md` off `spec` — the two type-mapping rows lose their *16-bit only* note | — | Phase D |
| T801 | **`DIV-001` was corrected during planning** — confirm the corrected Detection field matches what was built, and that a hand-declared `long` now compiles | — | T760 |
| T802 | **Resolve `DIV-054`** — `proposed` → `accepted`, or amended with what strict-mode enforcement actually required | — | T780 |
| T803 | Record whether `int` and 32-bit binding were already working. If so, say so: a green round-trip is then a regression guard, not proof of new behaviour | — | T746 |
| T804 | Record whether the `VARCHAR` `val` offset of 2 held on this ABI, and that the assertion is per-variable rather than assumed once | — | T770 |
| T805 | Re-examine SD-1, SD-2, SD-10, SD-11 against what was built; record drift as a defect, not as precedent | — | Phase C |
| T806 | Decide and record `ESQLC-2007`'s status. Detecting a host-variable declaration *outside* a declare section may need whole-program C parsing, in which case `ESQLC-1014` on the reference already covers the case and 2007 is unreachable | — | Phase D |
| T807 | Confirm `diag_registry`, `contract_sync`, `citation_check` and `sqlsa_layout_sync` are clean | — | Phase D |
| T808 | Reconcile the slice's non-proof list against the as-built state | — | Phase D |
| T809 | Run `/speckit.analyze`, including the Principle VIII slice checks | — | T800–T808 |

10 tasks.

## Phase D′ — mutation, run after Phase C

| Mutation | Must fail |
|---|---|
| Map `long long` to width 4 | `int_widths` (T730) |
| Take width from the type's spelling rather than `sizeof` | `int_widths` (T731) — should not even compile |
| Bind `MYSQL_TYPE_LONG` for a `double` | `float_roundtrip` (T747) |
| Read `val` at offset 0 instead of 2 | `varchar_roundtrip` (T748) |
| Accept an `int` length field | `varchar_len_int` (T743) |
| Report 1264 unmapped | `int_overflow_8300` (T751) |
| Drop `STRICT_TRANS_TABLES` | `int_overflow_8300` (T751) |

**The `val` offset mutation is the one to watch.** Reading `val` at offset 0
yields the two length bytes as the first two characters of the string —
plausible-looking data, not a crash. That is the failure class whose mutants
survived in Gate 5 and Gate 6, both times on the guard whose comment was most
confident.

Standing warning, at six occurrences: this project's mutation harness has
produced a false result every single time by failing to rebuild — a `perl`
substitution without `/g`, a guard matching its own comment, swallowed build
output, a restore without `touch`, a backup filename that did not match the
restore path, and a build that did not pick up a restored file. **After every
mutation run: confirm the mutation is present in the file, confirm the binary's
timestamp moved, and re-run the full suite from a forced rebuild before
believing green or red.**

## Requirement coverage

| Requirement | Phase B | Phase C |
|---|---|---|
| NFR-001.1 | T740 | T769 |
| FR-002.1 | T736 | T766 |
| FR-002.2 | T736 | T766 |
| FR-002.3 | T737 | T766 |
| FR-002.6 | T734, T735, T748 | T763, T768, T769, T770, T775, T776 |
| FR-002.9 | T730, T745 | T760, T761, T767 |
| FR-002.10 | T733, T747 | T762, T771, T774 |
| FR-002.12 | T742 | T760 |
| FR-002.13 | T750 | T778 |
| FR-002.15 | T741 | T773 |
| FR-002.16 | T749 | T777 |
| FR-002.20 | T744 | T765 |
| FR-002.21 | T735, T743 | T764 |
| FR-002.22 | T752 | T774 |
| FR-002.25 | T751 | T779, T780, T781 |
| FR-002.28 | T750 | T778 |
| FR-002.30 | T748 | T775 |
| FR-002.31 | T752 | T775 † |
| FR-003.1 | T739 | T769 |
| FR-003.2 | T738 | T782 |
| FR-003.3 | T738 | T782, T783 |
| FR-003.10 | T732 | T769 |
| NFR-002.1 | T746, T747, T748 | T760, T762, T763 |
| NFR-002.2 | T731, T735 | T770, T772 |
| NFR-003.2 | T752 | T769 † |

**25 of 25 covered. Zero requirements without an implementing task.**

**† Two are carried regressions, not new behaviour.** FR-002.31 (padding is the
program's job) and NFR-003.2 (no interpolation ever) gain nothing here. Their
Phase C entries name the task that could *break* them — T775 rewrites the
character bind path, T769 rewrites descriptor emission — rather than a task that
implements them. Stated because Gate 4 hit this and dropped a requirement from
its slice instead of pretending; naming the risk is the honest version.

## Critical path

```
T713 ─ VARCHAR fixture
  └─ T734 ─ descriptor test fails
       └─ T763 ─ recognise the struct by shape
            └─ T768 ─ HostVar carries the val capacity
                 └─ T769 ─ emit the descriptor
                      └─ T770 ─ emit the layout assertions
                           └─ T775 ─ bind the input via the struct
                                └─ T776 ─ write the retrieved len
                                     └─ T748 ─ round-trip passes
```

Nine deep. The `VARCHAR` chain is the long one because it is the only new
*shape*: everything else in this slice widens a value the descriptor already
described.

**Start with T760 anyway.** It is three tasks from the end of its own chain but
it fixes the `long` defect, and until it lands `int_widths` cannot even compile
— which means the widest-reaching Phase B test in the slice is blocked on it.
The `VARCHAR` work can proceed in parallel and does not depend on it.

## Exit criteria

The slice's ten, plus:

11. A hand-declared `long` host variable compiles, and its descriptor width
    equals `sizeof(long)` on the host.
12. Every mutation in Phase D′ fails its named test, with the mutation verified
    present and the binary verified rebuilt.
13. `DIV-054` resolved; `DIV-001`'s corrected Detection field confirmed against
    the build.
14. `docs/traceability.md` type-mapping rows no longer say *16-bit only*.
