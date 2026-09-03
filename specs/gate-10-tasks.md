# Gate 10 tasks — the `SQLDA`

**Slice:** [gate-10.md](gate-10.md) · **Plan:** [gate-10-plan.md](gate-10-plan.md)

Phase A fixtures, then Phase B tests, then Phase C implementation. No Phase C
task starts until the Phase B test it names fails for the right reason
(Principle IV).

26 scoped requirements. Every one appears in at least one Phase B and one
Phase C task; the coverage check is at the end.

**All eight of this slice's diagnostics are runtime conditions**, not
preprocessor ones — `EXECUTE` of an unprepared name, a NULL `var_ptr`, a
capacity too small. So they are Tier 2 fixtures asserting `sqlcode`, not
`run_negative.sh` fixtures. `ESQLC-7012` included: the preprocessor cannot know
a column's type, so a character column is discovered at `DESCRIBE` time.

## Phase A — fixtures and harness

| ID | Task | Reqs | Deps |
|----|------|------|------|
| T1010 | `schema.sql` + `seed.sql` — a table of three numeric columns, one nullable, and one whose **name exceeds 8 characters** so `DIV-057` has something to hit | FR-007.3 | — |
| T1011 [P] | `sqlda_constants.sqlc` — Tier 1: the five published constants | FR-007.6, FR-007.7, FR-007.26 | — |
| T1012 [P] | `sqlda_layout.sqlc` — Tier 1: Example 10-1's field order at a chosen `sqlvar-count` | FR-007.6a, FR-007.6b, NFR-007.3 | — |
| T1013 [P] | `sqlda_buffers.sqlc` — Tier 1: `INCLUDE SQLDA (name, n, nb, nbsize)` and the two size formulas | FR-007.7a | — |
| T1014 [P] | `sqlda_prepare_hostvar.sqlc` — Tier 1: the statement text held in a `char` array, with its width asserted | NFR-002.2, FR-002.9 | — |
| T1015 [P] | `rt/sqlda_malloc.sqlc` — **p.10-30's arithmetic**: allocate for three entries by the published formula and prove the result holds three | FR-007.6, FR-007.9 | T1010 |
| T1016 | `rt/sqlda_prepare_describe.sqlc` — the slice's main fixture: `PREPARE` from a host variable, then `DESCRIBE` | FR-007.1, FR-007.3 | T1010 |
| T1017 [P] | `rt/sqlda_datatypes.sqlc` — the published `_SQLDT_*` code per numeric column | FR-007.18 | T1016 |
| T1018 [P] | `rt/sqlda_datalen.sqlc` — byte length and scale, read back separately | FR-007.11a | T1016 |
| T1019 [P] | `rt/sqlda_precision.sqlc` — decimal digits per SD-18: 5, 10, 19 | FR-007.13 | T1016 |
| T1020 [P] | `rt/sqlda_nullinfo.sqlc` — negative for the nullable column and **not** for the others | FR-007.14 | T1016 |
| T1021 | `rt/sqlda_untouched.sqlc` — **the FR-007.8 fixture.** Sentinels into `eye_catcher`, `var_ptr` and `ind_ptr` before `DESCRIBE`; all three must survive byte-identical | FR-007.8 | T1016 |
| T1022 [P] | `rt/sqlda_reserved.sqlc` — a sentinel in `reserved` survives, per FR-007.6b's "must not assume the entry ends after `cprl_ptr`" | FR-007.6b | T1016 |
| T1023 | `rt/sqlda_execute.sqlc` — allocate buffers, set `var_ptr` and `ind_ptr`, `EXECUTE`, read values back | FR-007.2, FR-007.15, FR-007.22 | T1016 |
| T1024 [P] | `rt/sqlda_prepare_arm.sqlc` — `output_num` and `output_names_len` from the `SQLSA`. **The arm Gate 5 emitted as layout only** | FR-005.18, FR-007.23 | T1016 |
| T1025 | `rt/sqlda_datalen_unit.c` — a **pure unit test** of the packing, no database. NFR-007.2 asks for independence, and SD-17's bit order is exactly what a round-trip hides by encoding and decoding with one wrong convention | FR-007.11a | — |
| T1026 [P] | `rt/negative/sqlda_unprepared.sqlc` — `EXECUTE` of a name never prepared | FR-007.2 | T1010 |
| T1027 [P] | `rt/negative/sqlda_capacity.sqlc` — `num_entries` below `PREPARE`'s count | FR-007.9 | T1016 |
| T1028 [P] | `rt/negative/sqlda_null_varptr.sqlc` — `EXECUTE` with `var_ptr` left NULL | FR-007.8 | T1016 |
| T1029 [P] | `rt/negative/sqlda_bad_datatype.sqlc` — a `data_type` the program set to a value not in the table | FR-007.18 | T1016 |
| T1030 [P] | `rt/negative/sqlda_bad_datalen.sqlc` — a byte length that is not 2, 4 or 8 | FR-007.11a | T1016 |
| T1031 [P] | `rt/negative/sqlda_small_namesbuf.sqlc` — a names buffer below `output_names_len` | FR-007.7a | T1016 |
| T1032 [P] | `rt/negative/sqlda_uninit_eyecatcher.sqlc` — a zeroed structure, so the missing eye-catcher is detectable | FR-007.8 | T1016 |
| T1033 [P] | `rt/negative/sqlda_char_column.sqlc` — a **character** column described. Refused rather than given a guessed charset ID; this is why the slice is numeric-only | FR-007.3 | T1010 |
| T1034 | `tests/harness/sqlda_layout_sync.sh` — emitter offsets against `rt_sqlda_offsets.h`, both directions. Register in `CMakeLists.txt` | NFR-007.3 | T1012 |
| T1035 | **Extend `diag_registry.sh` with the converse check** — report codes registered in a spec but never emitted, against an explicit allowlist. Gate 9 found three such codes and nothing was watching | — | — |
| T1036 | Extend `run_tier2.sh` with the Gate 10 cases | — | T1016 |

27 tasks.

## Phase B — failing tests

All must fail for the right reason before any Phase C task starts.

### Tier 1 — preprocessor, no database

| ID | Task | Reqs | Deps |
|----|------|------|------|
| T1040 | `sqlda_constants` — eye-catcher `D1`, header 4, `sqlvar` **40** (`DIV-040`, not the published 24), names overhead 11, collation overhead 4 | FR-007.6, FR-007.26 | T1011 |
| T1041 | `sqlda_layout` — Example 10-1's order: four 16-bit fields then four address-width ones | FR-007.6a | T1012 |
| T1042 | `sqlda_layout` — **every field's `offsetof` asserted**, `reserved` at 32 and last. NFR-007.3 is stricter than Gate 5's bounded set and affordable at eight fields | FR-007.6b, NFR-007.3 | T1012 |
| T1043 | `sqlda_layout` — `sqlvar` follows the 4-byte header, and the emitted unit **compiles** | NFR-007.3 | T1012 |
| T1044 [P] | `sqlda_layout` — the count comes from the directive: `INCLUDE SQLDA (d, 3)` yields `sqlvar[3]`, **not** a flexible member and not a fixed 16 | FR-007.6a | T1012 |
| T1045 [P] | `sqlda_buffers` — names buffer `(size + 11) × count`, collation `(size + 4) × count` | FR-007.7, FR-007.7a | T1013 |
| T1046 [P] | `sqlda_prepare_hostvar` — the statement's `char` array carries width and capacity assertions | NFR-002.2, FR-002.9 | T1014 |
| T1047 | `sqlda_layout_sync` — emitter and runtime agree on every offset, both directions | NFR-007.3 | T1034 |
| T1048 [P] | `sqlda_datalen_unit` — the packing as a pure function: `(bytes << 8) \| scale` per SD-17, **encode and decode asserted separately** so they cannot agree on a mistake | FR-007.11a | T1025 |
| T1049 [P] | `abi_isolation` and `contract_sync` cover the three new entry points | FR-003.2, FR-003.3 | — |
| T1050 [P] | `abi_only_symbols` — the emitted unit calls `esqlc_*` and nothing else | FR-003.1 | T1014 |
| T1051 [P] | `opaque_body_unchanged` — the prepared text passes through verbatim | NFR-001.1 | T1014 |
| T1052 [P] | `diag_registry` converse — the allowlist names `ESQLC-2015`, `6005` and `6006` and nothing else silently | — | T1035 |

### Tier 2 — live server

| ID | Task | Reqs | Deps |
|----|------|------|------|
| T1053 | `rt/sqlda_malloc` — p.10-30's formula yields room for three entries. **Fails against a flexible member**, which is the mutation that matters | FR-007.6, FR-007.9 | T1015 |
| T1054 | `rt/sqlda_prepare_describe` — one `sqlvar` filled per output column | FR-007.1, FR-007.3 | T1016 |
| T1055 [P] | `rt/sqlda_datatypes` — the published code per numeric type | FR-007.18 | T1017 |
| T1056 [P] | `rt/sqlda_datalen` — byte length and scale decode to 2/4/8 and 0 | FR-007.11a | T1018 |
| T1057 [P] | `rt/sqlda_precision` — 5, 10, 19 (SD-18: digits, not bits) | FR-007.13 | T1019 |
| T1058 [P] | `rt/sqlda_nullinfo` — negative exactly for the nullable column | FR-007.14 | T1020 |
| T1059 | `rt/sqlda_untouched` — `eye_catcher`, `var_ptr` and `ind_ptr` **byte-identical** after `DESCRIBE`. The only way FR-007.8 is observable | FR-007.8 | T1021 |
| T1060 [P] | `rt/sqlda_reserved` — the sentinel in `reserved` survives | FR-007.6b | T1022 |
| T1061 | `rt/sqlda_execute` — values read back through `var_ptr`, a null through `ind_ptr`, a `?` parameter bound | FR-007.2, FR-007.15, FR-007.22 | T1023 |
| T1062 | `rt/sqlda_prepare_arm` — `output_num` is 3 and `output_names_len` matches the formula. **Gate 5's arm stops being layout only** | FR-005.18, FR-007.23 | T1024 |
| T1063 [P] | The eight refusals fire with their own codes | FR-007.2, FR-007.3, FR-007.7a, FR-007.8, FR-007.9, FR-007.11a, FR-007.18 | T1026–T1033 |

24 tasks.

## Phase C — implementation

| ID | Task | Reqs | Makes pass | Deps |
|----|------|------|-----------|------|
| T1070 | `src/pp/pp.h` — the `SQLDA` constants and the directive's parsed parameters | FR-007.6 | T1040 | Phase B |
| T1071 | `src/pp/sqlda.cc` — Example 10-1's declaration, `sqlvar` at the directive's count | FR-007.6a, FR-007.26 | T1041, T1044 | T1070 |
| T1072 | `src/pp/sqlda.cc` — the five constants, `SQLDA_SQLVAR_LEN` 40 per `DIV-040` | FR-007.6, FR-007.7 | T1040 | T1071 |
| T1073 | `src/pp/sqlda.cc` — an `offsetof` assertion for **every** field, `reserved` last | FR-007.6b, NFR-007.3 | T1042, T1043 | T1072 |
| T1074 | `src/pp/sqlda.cc` — the names and collation buffer declarations, sized by the published formulas | FR-007.7a | T1045 | T1072 |
| T1075 | `src/pp/emit.cc` — parse `INCLUDE SQLDA (name, count[, nb, nbsize])` | FR-007.6a | T1044 | T1071 |
| T1076 | `src/pp/emit.cc` — the `PREPARE` handler; the statement text stays verbatim | FR-007.1, NFR-001.1, FR-003.1 | T1050, T1051 | T1075 |
| T1077 | `src/pp/emit.cc` — the `DESCRIBE` handler | FR-007.3 | T1054 | T1076 |
| T1078 | `src/pp/emit.cc` — the `EXECUTE` handler | FR-007.2 | T1061 | T1077 |
| T1079 | `src/pp/dispatch.cc` — the four keywords; `RELEASE`, `DESCRIBE INPUT`, `EXECUTE IMMEDIATE` and dynamic `DECLARE CURSOR` keep `ESQLC-1012` | FR-007.1 | T1054 | T1075 |
| T1080 | `include/esqlc.h` — the three entry points | FR-003.2, FR-003.3 | T1049 | Phase B |
| T1081 | `src/rt/rt_sqlda_offsets.h` — the layout the runtime writes by | NFR-007.3 | T1047 | T1080 |
| T1082 | `src/rt/dynamic.c` — `esqlc_prepare`, keyed by name | FR-007.1 | T1054 | T1081 |
| T1083 | `src/rt/dynamic.c` — `esqlc_describe`: **the four fields and no others** | FR-007.3, FR-007.8 | T1054, T1059 | T1082 |
| T1084 | `src/rt/dynamic.c` — `data_type` from the published table, numeric codes | FR-007.18 | T1055 | T1083 |
| T1085 | `src/rt/dynamic.c` — `data_len` packing per SD-17 | FR-007.11a | T1048, T1056 | T1083 |
| T1086 | `src/rt/dynamic.c` — `precision` as decimal digits per SD-18 | FR-007.13 | T1057 | T1083 |
| T1087 | `src/rt/dynamic.c` — `null_info` negative for a nullable column | FR-007.14 | T1058 | T1083 |
| T1088 | `src/rt/dynamic.c` — the names buffer, `VARCHAR`-shaped. **`DIV-057`'s choice is made here** | FR-007.7a | T1045 | T1083 |
| T1089 | `src/rt/dynamic.c` — `num_entries` validated against `PREPARE`'s count | FR-007.9 | T1063 | T1083 |
| T1090 | `src/rt/dynamic.c` — `esqlc_execute` binds through `var_ptr` | FR-007.2, FR-007.22 | T1061 | T1083 |
| T1091 | `src/rt/dynamic.c` — `ind_ptr`'s null flag, written by the implementation on output | FR-007.15 | T1061 | T1090 |
| T1092 | `src/rt/dynamic.c` — `reserved` never written | FR-007.6b | T1060 | T1083 |
| T1093 | `src/rt/sqlsa.c` — populate the `prepare` arm's `output_num` and `output_names_len` | FR-005.18, FR-007.23 | T1062 | T1083 |
| T1094 | `tests/harness/diag_registry.sh` — the converse check and its allowlist | — | T1052 | Phase B |

25 tasks.

## Phase D — diagnostics

One task per diagnostic row this slice touches. All eight are runtime
conditions, so each is a `sqlcode` assertion rather than a preprocessor
diagnostic.

| ID | Task | Code | Reqs | Deps |
|----|------|------|------|------|
| T1100 [P] | `EXECUTE` of an unprepared or released name | `ESQLC-7001` | FR-007.2 | T1090 |
| T1101 [P] | `num_entries` below the count `PREPARE` reported | `ESQLC-7002` | FR-007.9 | T1089 |
| T1102 [P] | `var_ptr` NULL at execution — refused, not allocated for | `ESQLC-7003` | FR-007.8 | T1090 |
| T1103 [P] | A `data_type` value not in the published table | `ESQLC-7004` | FR-007.18 | T1084 |
| T1104 [P] | A `data_len` byte length that is not 2, 4 or 8. **An independent check on SD-17**: under the wrong bit order a scale-0 `INTEGER` decodes as byte length 0 | `ESQLC-7005` | FR-007.11a | T1085 |
| T1105 [P] | A names buffer smaller than `output_names_len` | `ESQLC-7008` | FR-007.7a | T1088 |
| T1106 [P] | An eye-catcher the program never initialised — a **warning**, because it is only reliably detectable in zeroed memory | `ESQLC-7010` | FR-007.8 | T1083 |
| T1107 [P] | A character column described — refused rather than given a guessed charset ID (002 Q7) | `ESQLC-7012` | FR-007.3 | T1084 |

`ESQLC-7006` (date-time qualifier), `7007` (charset ID mismatch), `7009`
(collation buffer) and `7011` (`RELEASE`) get no task: their requirements are
out of scope, so the conditions cannot arise. Recorded in Phase E, and now
**visible to `diag_registry`'s converse check** rather than only to a reader.

## Phase E — documentation and registry

| ID | Task | Reqs | Deps |
|----|------|------|------|
| T1110 | Move the slice's rows in `docs/traceability.md` off `spec` — the 007 rows this slice covers | — | Phase D |
| T1111 | **Resolve `DIV-057`** — record which of truncate / omit / refuse was chosen for a table name over 8 characters, and why | — | T1088 |
| T1112 | Confirm `DIV-040`'s sharpened migration note matches what was built, and that `rt/sqlda_malloc` proves the `sizeof` idiom safe | — | T1053 |
| T1113 | Record the four out-of-scope 007 codes in the converse allowlist, with the requirement that puts each out of reach | — | T1094 |
| T1114 | Record whether SD-17's bit order and SD-18's digits reading survived; both are provisional and `SQLRM` may settle them | — | T1085, T1086 |
| T1115 | Record that the `prepare` arm is now **two-thirds** sentinel rather than wholly — `input_num`, `input_names_len`, `name_map_len` need `DESCRIBE INPUT`, `sql_statement_type` needs FR-005.24 | — | T1093 |
| T1116 | Re-examine SD-2, SD-10, SD-17, SD-18 against what was built; record drift as a defect, not as precedent | — | Phase C |
| T1117 | Confirm every harness is clean, including the new `sqlda_layout_sync` and the extended `diag_registry` | — | Phase D |
| T1118 | Reconcile the slice's non-proof list against the as-built state | — | Phase D |
| T1119 | Run `/speckit.analyze`, including the Principle VIII slice checks | — | T1110–T1118 |

10 tasks.

## Phase D′ — mutation, run after Phase C

| Mutation | Must fail |
|---|---|
| **Emit `sqlvar[]` as a C99 flexible member** | `rt/sqlda_malloc` (T1053) |
| Swap SD-17's bit order | `sqlda_datalen_unit` (T1048) |
| Report `precision` in bits rather than digits | `rt/sqlda_precision` (T1057) |
| Have `DESCRIBE` write `var_ptr` | `rt/sqlda_untouched` (T1059) |
| Have `DESCRIBE` write `reserved` | `rt/sqlda_reserved` (T1060) |
| Shift a runtime offset by 2 | `sqlda_layout_sync` (T1047) |
| Skip the `num_entries` check | `rt/sqlda_capacity` (T1063) |
| Drop a code from the converse allowlist | `diag_registry` (T1052) |

**The flexible-member mutation is the one to watch.** The unit still compiles,
every field still reads correctly, and only the *allocation size* is wrong — by
one entry. That is a heap overflow appearing at a customer's column count and
not at a fixture's, which is why T1053 allocates for three and checks the size
rather than merely running.

The `precision` mutation is the plausible-wrong class: 16 instead of 5 for a
`SMALLINT` looks like a reasonable answer to a reasonable question, and only
SD-18's reasoning says which question was asked.

**The standing rebuild warning, at eight occurrences with eight distinct
causes.** Most recent: `make` treats an object as current when source and object
share an mtime, and `stat` resolves to whole seconds. After every mutation:
confirm the mutation is present in the file, confirm the binary changed **by
content hash rather than timestamp**, and confirm the restore restored the right
file.

## Requirement coverage

| Requirement | Phase B | Phase C |
|---|---|---|
| NFR-001.1 | T1051 | T1076 |
| FR-002.9 | T1046 | T1071 |
| FR-003.1 | T1050 | T1076 |
| FR-003.2 | T1049 | T1080 |
| FR-003.3 | T1049 | T1080 |
| FR-005.18 | T1062 | T1093 |
| NFR-002.2 | T1046 | T1071 |
| FR-007.1 | T1054 | T1076, T1079, T1082 |
| FR-007.2 | T1061, T1063 | T1078, T1090 |
| FR-007.3 | T1054, T1063 | T1077, T1083 |
| FR-007.6 | T1040, T1053 | T1070, T1072 |
| FR-007.6a | T1041, T1044 | T1071, T1075 |
| FR-007.6b | T1042, T1060 | T1073, T1092 |
| FR-007.7 | T1045 | T1072 |
| FR-007.7a | T1045, T1063 | T1074, T1088 |
| FR-007.8 | T1059, T1063 | T1083 |
| FR-007.9 | T1053, T1063 | T1089 |
| FR-007.11a | T1048, T1056, T1063 | T1085 |
| FR-007.13 | T1057 | T1086 |
| FR-007.14 | T1058 | T1087 |
| FR-007.15 | T1061 | T1091 |
| FR-007.18 | T1055, T1063 | T1084 |
| FR-007.22 | T1061 | T1090 |
| FR-007.23 | T1062 | T1093 |
| FR-007.26 | T1040 | T1071 |
| NFR-007.3 | T1042, T1043, T1047 | T1073, T1081 |

**26 of 26 covered. Zero requirements without an implementing task.**

## Critical path

```
T1012 ─ the layout fixture
  └─ T1041 ─ its test fails
       └─ T1070 ─ constants and parsed parameters
            └─ T1071 ─ Example 10-1's declaration
                 └─ T1072 ─ the five constants
                      └─ T1073 ─ the per-field assertions
                           └─ T1080 ─ the ABI header
                                └─ T1081 ─ the runtime offset table
                                     └─ T1082 ─ prepare
                                          └─ T1083 ─ describe, four fields only
                                               └─ T1093 ─ the SQLSA prepare arm
                                                    └─ T1062 ─ its test passes
```

Twelve deep, and the shape is the `SQLSA`'s again: nothing runtime can be
written before the layout is fixed, because the runtime addresses the structure
by offset.

**T1035 is off the path and worth doing first anyway.** Extending
`diag_registry` with the converse check costs little, and this slice adds four
more out-of-reach codes to the four already known — `7006`, `7007`, `7009`,
`7011` joining `2015`, `6005`, `6006`. Landing the check before those four
arrive means the allowlist is written as a decision rather than backfilled as an
excuse.

## Exit criteria

The slice's twelve, plus:

13. Every mutation in Phase D′ fails its named test, with the mutation verified
    present and the binary verified changed by content.
14. `diag_registry`'s converse check green, with an allowlist naming all seven
    known out-of-reach codes and their reasons.
15. `DIV-057` resolved; `DIV-040`'s migration note confirmed against the build.
16. `docs/traceability.md` 007 rows moved off `spec`.
