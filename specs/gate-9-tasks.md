# Gate 9 tasks — `INVOKE`

**Slice:** [gate-9.md](gate-9.md) · **Plan:** [gate-9-plan.md](gate-9-plan.md)

Phase A fixtures, then Phase B tests, then Phase C implementation. No Phase C
task starts until the Phase B test it names fails for the right reason
(Principle IV).

26 scoped requirements. Every one appears in at least one Phase B and one
Phase C task; the coverage check is at the end.

## Phase A — fixtures and harness

| ID | Task | Reqs | Deps |
|----|------|------|------|
| T910 | `tests/conformance/gate-1/schema.cache` — the tab-separated cache for `parts` and `charsets`, with a `!captured` line | NFR-006.2 | — |
| T911 | **Plumb `--schema` through the Tier 1 harnesses** (`run_golden.sh`, `run_negative.sh`, `spec_assertions.py`). Without it no `INVOKE` fixture can preprocess at all, so this gates every other Phase B task | NFR-006.2 | T910 |
| T912 [P] | `invoke_basic.sqlc` — `INVOKE parts AS parts_rec` | FR-006.1, FR-006.2a | T910 |
| T913 [P] | `invoke_types.sqlc` — `CHAR`, `SMALLINT`, and a nullable column | FR-006.2, FR-006.3 | T910 |
| T914 [P] | `invoke_charset.sqlc` — a column with a character set, invoked | FR-006.2b | T910 |
| T915 [P] | `invoke_varchar.sqlc` — a `VARCHAR` column, invoked | FR-006.4 | T910 |
| T916 [P] | `invoke_indicators.sqlc` — one nullable and one `NOT NULL` column, so a per-column indicator is distinguishable from a per-nullable one | FR-006.5 | T910 |
| T917 [P] | `invoke_provenance.sqlc` — the generated comment | FR-006.5d | T910 |
| T918 [P] | `invoke_placeholders.sqlc` — a generated field used in a statement | FR-003.10 | T910 |
| T919 [P] | `rt/invoke_roundtrip.sqlc` — `:parts_rec.part_num` inserted and read back | FR-006.8, FR-002.30 | T910 |
| T920 [P] | `rt/invoke_null.sqlc` — a null through a generated indicator | FR-002.16 | T910 |
| T921 [P] | `negative/invoke_unknown_object.sqlc` + `.expected.diag` | FR-006.6 | T910 |
| T922 [P] | `negative/invoke_no_cache.sqlc` + `.expected.diag` — no `--schema` given | NFR-006.2 | — |
| T923 [P] | `negative/invoke_unmapped_type.sqlc` + a cache row with a type 002 does not map | FR-006.2 | T910 |
| T924 [P] | `negative/invoke_bad_identifier.sqlc` + a column named `int`. **A C keyword, so the generated structure would not compile** and the error would point at generated text | FR-006.2 | T910 |
| T925 [P] | `negative/invoke_exec_position.sqlc` + `.expected.diag` | FR-006.1 | T910 |
| T926 [P] | `negative/invoke_indicator_collision.sqlc` + a 30-character nullable column name | FR-006.5c | T910 |
| T927 [P] | `negative/invoke_unreadable_cache.sqlc` — a cache named but not readable, distinct from absent | NFR-006.2 | T910 |
| T928 | Extend `run_tier2.sh` with the Gate 9 cases | — | T919 |

19 tasks.

## Phase B — failing tests

All must fail for the right reason before any Phase C task starts.

### Tier 1 — preprocessor, no database

| ID | Task | Reqs | Deps |
|----|------|------|------|
| T930 | `invoke_basic` — the tag is `parts_type` and the fields are the columns, lowercased | FR-006.1, FR-006.2a | T912 |
| T931 | `invoke_basic` — **the emitted unit compiles.** A generator whose output does not compile is the whole failure mode | NFR-002.2 | T912 |
| T932 [P] | `invoke_types` — `CHAR(18)` → `char[19]`, `SMALLINT` → `short` width 2, per 002's table | FR-006.2, FR-006.3, FR-002.3, FR-002.9 | T913 |
| T933 [P] | `invoke_charset` — `CHARACTER SET` emitted **inline, before the identifier**, as §2 p.2-22 writes it | FR-006.2b, FR-002.4 | T914 |
| T934 [P] | `invoke_charset` — the emitted clause is one **Gate 8's parser reads back**, and the descriptor carries the set. Generation and parsing must meet | FR-002.8 | T914 |
| T935 | `invoke_varchar` — the nested `short len; char val[]` group, carrying **Gate 7's three layout assertions** | FR-006.4, FR-002.6, NFR-002.2 | T915 |
| T936 [P] | `invoke_indicators` — one indicator for the nullable column and **none** for the `NOT NULL` one | FR-006.5 | T916 |
| T937 | `invoke_indicators` — the indicator **precedes** its field. FR-006.5 says so and p.2-22 shows it; a structure with them after would compile and be wrong | FR-006.5 | T916 |
| T938 [P] | `invoke_indicators` — spelled `_i`, lowercase. FR-006.5b appends `_I` and FR-006.2 lowercases; **a fixture asserting `_I` would pass a wrong implementation** | FR-006.5b | T916 |
| T939 [P] | `invoke_provenance` — the comment names the object and the cache's capture timestamp | FR-006.5d | T917 |
| T940 [P] | `invoke_placeholders` — a generated field becomes a placeholder; no value in the statement text | FR-003.10 | T918 |
| T941 [P] | `abi_only_symbols` — the emitted unit calls `esqlc_*` and nothing else | FR-003.1 | T918 |
| T942 [P] | `opaque_body_unchanged` — statement bodies still verbatim | NFR-001.1 | T918 |
| T943 | `abi_isolation` — **the preprocessor still links no MariaDB library and opens no socket.** The slice's central claim | NFR-001.2, NFR-006.2 | T911 |
| T944 [P] | `negative/invoke_unknown_object` — code, line and column | FR-006.6 | T921 |
| T945 [P] | `negative/invoke_no_cache` — a distinct code from an unreadable one | NFR-006.2 | T922 |
| T946 [P] | `negative/invoke_unmapped_type` — refused by name, not guessed | FR-006.2 | T923 |
| T947 [P] | `negative/invoke_bad_identifier` — refused at preprocess time, **not left to the C compiler** | FR-006.2 | T924 |
| T948 [P] | `negative/invoke_exec_position` | FR-006.1 | T925 |
| T949 [P] | `negative/invoke_indicator_collision` — `ESQLC-6007`, diagnosed rather than reproduced (`DIV-056`) | FR-006.5c | T926 |
| T950 [P] | `negative/invoke_unreadable_cache` | NFR-006.2 | T927 |
| T951 | `#line` stays at the `INVOKE` statement and does not drift into generated territory. **Generated multi-line output is the largest opportunity yet for the defect Gates 1 and 2 both hit** | NFR-001.1 | T912 |

### Tier 2 — live server

| ID | Task | Reqs | Deps |
|----|------|------|------|
| T952 | `rt/invoke_roundtrip` — `:parts_rec.part_num` and `:parts_rec.part_desc` insert and read back, bytes verbatim | FR-006.8, FR-002.30, FR-002.15 | T919 |
| T953 [P] | `rt/invoke_null` — a null via the generated indicator | FR-002.16 | T920 |

24 tasks.

## Phase C — implementation

| ID | Task | Reqs | Makes pass | Deps |
|----|------|------|-----------|------|
| T960 | `src/pp/pp.h` — `SchemaColumn` and `Schema` | FR-006.2 | T930 | Phase B |
| T961 | `src/pp/schema.cc` — the tab-separated reader | NFR-006.2 | T945 | T960 |
| T962 | `src/pp/schema.cc` — the `!captured` timestamp line | FR-006.5d | T939 | T961 |
| T963 | `src/pp/schema.cc` — an object absent from the cache is an error naming it | FR-006.6 | T944 | T961 |
| T964 | `src/pp/schema.cc` — an unreadable cache is a **distinct** error from an absent one | NFR-006.2 | T950 | T961 |
| T965 | `src/pp/main.cc` — the `--schema` option. **No socket, no MariaDB link** | NFR-001.2, NFR-006.2 | T943, T945 | T961 |
| T966 | `src/pp/invoke.cc` — the structure skeleton; tag is `<name>_type` | FR-006.1, FR-006.2a | T930, T931 | T965 |
| T967 | `src/pp/invoke.cc` — field names lowercased; types from 002's table | FR-006.2, FR-002.3, FR-002.9 | T932 | T966 |
| T968 | `src/pp/invoke.cc` — the terminator byte for character fields (SD-10) | FR-006.3 | T932 | T967 |
| T969 | `src/pp/invoke.cc` — `CHARACTER SET` inline before the identifier | FR-006.2b, FR-002.4, FR-002.8 | T933, T934 | T967 |
| T970 | `src/pp/invoke.cc` — the `VARCHAR` nested group | FR-006.4, FR-002.6 | T935 | T969 |
| T971 | `src/pp/invoke.cc` — an indicator for each nullable column and no other | FR-006.5 | T936 | T967 |
| T972 | `src/pp/invoke.cc` — the indicator **precedes** its field | FR-006.5 | T937 | T971 |
| T973 | `src/pp/invoke.cc` — the `_i` spelling | FR-006.5b | T938 | T971 |
| T974 | `src/pp/invoke.cc` — a 30/31-character collision refused, not truncated | FR-006.5c | T949 | T973 |
| T975 | `src/pp/invoke.cc` — a column name that is a C keyword, or not a valid identifier, refused | FR-006.2 | T947 | T967 |
| T976 | `src/pp/invoke.cc` — a column type with no 002 mapping refused | FR-006.2 | T946 | T967 |
| T977 | `src/pp/invoke.cc` — the two-line provenance comment of p.2-22 | FR-006.5d | T939 | T962 |
| T978 | `src/pp/dispatch.cc` — `INVOKE` implemented in declaration position | FR-006.1 | T948 | T966 |
| T979 | `src/pp/emit.cc` — emit generated text into the declare-section stream | FR-003.1, NFR-001.1 | T941, T942 | T978 |
| T980 | `src/pp/emit.cc` — **re-parse** the generated text as declarations, through `decl.cc` | FR-006.8 | T952 | T979 |
| T981 | `src/pp/emit.cc` — `#line` stays at the `INVOKE` statement | NFR-001.1 | T951 | T979 |
| T982 | `src/pp/emit.cc` — a generated `VARCHAR` group carries Gate 7's assertions; the structure itself gets **no `sizeof` assertion**, because its total is not API | NFR-002.2 | T935 | T980 |
| T983 | `src/pp/emit.cc` — no interpolation from a generated field | FR-003.10 | T940 | T980 |
| T984 | `src/pp/emit.cc` — indicator association for generated fields | FR-002.15, FR-002.16, FR-002.30 | T952, T953 | T980 |

25 tasks.

## Phase D — diagnostics

One task per diagnostic row this slice touches. All seven codes were registered
when 006 was written, so none needs adding first — unlike Gate 8.

| ID | Task | Code | Reqs | Deps |
|----|------|------|------|------|
| T990 [P] | Invoked object absent from the cache | `ESQLC-6001` | FR-006.6 | T963 |
| T991 [P] | No schema source: `--schema` not given | `ESQLC-6002` | NFR-006.2 | T965 |
| T992 [P] | A column type with no mapping in 002's table | `ESQLC-6003` | FR-006.2 | T976 |
| T993 [P] | A column name that cannot become a valid, unique C identifier — including a C keyword | `ESQLC-6004` | FR-006.2 | T975 |
| T994 [P] | `INVOKE` outside declaration position | `ESQLC-6006` | FR-006.1 | T978 |
| T995 [P] | The 30/31-character indicator collision (`DIV-056`) | `ESQLC-6007` | FR-006.5c | T974 |
| T996 [P] | A cache file named but unreadable | `ESQLC-6008` | NFR-006.2 | T964 |

**`ESQLC-6005` gets no task.** "Cached schema is stale relative to a reachable
database" needs a reachable database, which is exactly what the preprocessor
does not have — SD-16 and NFR-001.2 both forbid it. It is registered and
unreachable in this design, the second such code after Gate 8's `ESQLC-2015`.
Recorded in Phase E rather than left looking unimplemented.

## Phase E — documentation and registry

| ID | Task | Reqs | Deps |
|----|------|------|------|
| T1000 | Move the slice's rows in `docs/traceability.md` off `spec` — the three 006 rows | — | Phase D |
| T1001 | **Resolve `DIV-056`** — `proposed` → `accepted`, or amended with what the collision refusal actually required | — | T974 |
| T1002 | **Narrow `DIV-055`** — generated declarations now agree with their columns by construction, so its silent failure applies only to hand-written ones | — | T969 |
| T1003 | Record that `ESQLC-6005` is unreachable in this design, with the reason, alongside Gate 8's note on `ESQLC-2015`. **Two registered-but-unreachable codes is a pattern worth naming**, not two footnotes | — | Phase D |
| T1004 | Record whether a C-keyword column name landed under `ESQLC-6004` or needed its own code | — | T975 |
| T1005 | Re-examine SD-2, SD-10, SD-15, SD-16 against what was built; record drift as a defect, not as precedent | — | Phase C |
| T1006 | Record the cache format as built, and that it has one producer and one consumer — so it is provisional until a capture tool exists | — | T961 |
| T1007 | Confirm `diag_registry`, `contract_sync`, `citation_check`, `sqlsa_layout_sync` and `abi_isolation` are clean | — | Phase D |
| T1008 | Reconcile the slice's non-proof list against the as-built state, including that a stale cache is undetectable | — | Phase D |
| T1009 | Run `/speckit.analyze`, including the Principle VIII slice checks | — | T1000–T1008 |

10 tasks.

## Phase D′ — mutation, run after Phase C

| Mutation | Must fail |
|---|---|
| Put the indicator **after** its field | `invoke_indicators` (T937) |
| Emit `_I` instead of `_i` | `invoke_indicators` (T938) |
| Generate an indicator for every column | `invoke_indicators` (T936) |
| Drop the inline `CHARACTER SET` clause | `invoke_charset` (T933) |
| Truncate the suffix instead of refusing | `negative/invoke_indicator_collision` (T949) |
| Omit the `!captured` timestamp | `invoke_provenance` (T939) |
| Have the preprocessor open a socket | `abi_isolation` (T943) |

**The first two are the plausible-wrong class**, and they are why T937 and T938
assert order and spelling rather than presence. Both produce a structure that
compiles and reads correctly to a human, and both break a customer program at a
name or an offset — the same shape as Gate 5's surviving accumulator mutant and
Gate 8's dead charset table.

**The standing rebuild warning, at eight occurrences with eight distinct
causes.** Most recent: `make` treats an object as current when source and object
share an mtime, and `stat` resolves to whole seconds. After every mutation:
confirm the mutation is present in the file, confirm the binary changed **by
content hash rather than timestamp**, and confirm the restore restored the right
file.

## Requirement coverage

| Requirement | Phase B | Phase C |
|---|---|---|
| NFR-001.1 | T942, T951 | T979, T981 |
| NFR-001.2 | T943 | T965 |
| FR-002.3 | T932 | T967 |
| FR-002.4 | T933 | T969 |
| FR-002.6 | T935 | T970 |
| FR-002.8 | T934 | T969 |
| FR-002.9 | T932 | T967 |
| FR-002.15 | T952 | T984 |
| FR-002.16 | T953 | T984 |
| FR-002.30 | T952 | T984 |
| FR-003.1 | T941 | T979 |
| FR-003.10 | T940 | T983 |
| NFR-002.2 | T931, T935 | T982 |
| FR-006.1 | T930, T948 | T966, T978 |
| FR-006.2 | T932, T946, T947 | T967, T975, T976 |
| FR-006.2a | T930 | T966 |
| FR-006.2b | T933 | T969 |
| FR-006.3 | T932 | T968 |
| FR-006.4 | T935 | T970 |
| FR-006.5 | T936, T937 | T971, T972 |
| FR-006.5b | T938 | T973 |
| FR-006.5c | T949 | T974 |
| FR-006.5d | T939 | T962, T977 |
| FR-006.6 | T944 | T963 |
| FR-006.8 | T952 | T980 |
| NFR-006.2 | T943, T945, T950 | T961, T964, T965 |

**26 of 26 covered. Zero requirements without an implementing task.**

## Critical path

```
T910 ─ the cache fixture
  └─ T911 ─ --schema plumbed through the Tier 1 harnesses
       └─ T915 ─ the VARCHAR fixture
            └─ T935 ─ its test fails
                 └─ T960 ─ SchemaColumn / Schema
                      └─ T961 ─ the cache reader
                           └─ T965 ─ the --schema option
                                └─ T966 ─ the structure skeleton
                                     └─ T967 ─ fields and types
                                          └─ T969 ─ the CHARACTER SET clause
                                               └─ T970 ─ the VARCHAR group
                                                    └─ T979 ─ emit into the stream
                                                         └─ T980 ─ re-parse it
                                                              └─ T982 ─ the assertions
```

Thirteen deep — the longest of any gate — and the depth is real: nothing can be
generated before the cache can be read, and nothing can be re-parsed before it
is generated.

**T911 gates everything and is not optional.** The Tier 1 harnesses invoke
`esqlcpp` with no `--schema`, so until it is plumbed through, *no* `INVOKE`
fixture can preprocess and every Phase B task in the slice fails for the wrong
reason. It is the analogue of Gate 8's T874: do the enabling work first, then
the work.

## Exit criteria

The slice's ten, plus:

11. Every mutation in Phase D′ fails its named test, with the mutation verified
    present and the binary verified changed by content.
12. `abi_isolation` unchanged — **the preprocessor's dependency set is the same
    as it was at Gate 1.**
13. `DIV-056` resolved; `DIV-055` narrowed.
14. `ESQLC-6005`'s unreachability recorded next to `ESQLC-2015`'s.
15. `docs/traceability.md` 006 rows moved off `spec`.
