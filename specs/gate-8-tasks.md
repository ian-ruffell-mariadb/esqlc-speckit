# Gate 8 tasks — character sets

**Slice:** [gate-8.md](gate-8.md) · **Plan:** [gate-8-plan.md](gate-8-plan.md)

Phase A fixtures, then Phase B tests, then Phase C implementation. No Phase C
task starts until the Phase B test it names fails for the right reason
(Principle IV).

17 scoped requirements. Every one appears in at least one Phase B and one
Phase C task; the coverage check is at the end.

## Phase A — fixtures and harness

| ID | Task | Reqs | Deps |
|----|------|------|------|
| T810 | **Register `ESQLC-2013`, `2014`, `2015` in 002's diagnostics table, before any code emits them.** Gate 1 let two codes slip through unregistered; `diag_registry` exists because of it | FR-002.8 | — |
| T811 [P] | `schema.sql` — columns in `latin2`, `euckr` and `latin1`, plus one whose charset deliberately disagrees with what a fixture will declare | NFR-002.1 | — |
| T812 [P] | `seed.sql` — a high-byte row and a two-byte-per-character row | NFR-002.1 | T811 |
| T813 [P] | `charset_clause.sqlc` — Tier 1: `CHARACTER SET` and `CHARACTER SET IS` on a `char` array | FR-002.4 | — |
| T814 [P] | `charset_varchar.sqlc` — Tier 1: the clause **inside** a `VARCHAR` structure | FR-002.6 | — |
| T815 [P] | `charset_keywords.sqlc` — Tier 1: every mappable keyword plus `UNKNOWN` and an absent clause | FR-002.8 | — |
| T816 [P] | `rt/charset_high_bytes.sqlc` — **the fixture the slice exists for**: a byte above 0x7F, compared as hex | FR-002.30 | T811 |
| T817 [P] | `rt/charset_roundtrip_1byte.sqlc` — `ISO88592` end to end | FR-002.28, NFR-002.1 | T812 |
| T818 [P] | `rt/charset_roundtrip_2byte.sqlc` — `KSC5601`, two syllables, `len` asserted as **4** | NFR-002.1 | T812 |
| T819 [P] | `rt/charset_null.sqlc` — a negative indicator on a charset `VARCHAR` | FR-002.16 | T811 |
| T820 [P] | `rt/charset_family.sqlc` — retrieval into a host variable whose declared set disagrees with the column | FR-002.22 | T811 |
| T821 [P] | `negative/charset_unknown_keyword.sqlc` + `.expected.diag` — a typo | FR-002.8 | T810 |
| T822 [P] | `negative/charset_unmapped.sqlc` + `.expected.diag` — `ISO88593`, a **known** keyword MariaDB has no counterpart for | FR-002.8 | T810 |
| T823 [P] | `negative/charset_kanji.sqlc` + `.expected.diag` — refused as *unspecified*, a different message from unmapped | FR-002.8 | T810 |
| T824 [P] | `negative/national_character.sqlc` + `.expected.diag` — `ESQLC-1012` naming the `KANJI` dependency | FR-002.8 | T810 |
| T825 | `tests/harness/charset_sync.sh` — the preprocessor's keyword table and the runtime's mapping must agree, both directions. Register in `CMakeLists.txt` | FR-002.8 | — |
| T826 | Extend `run_tier2.sh` with the Gate 8 cases | — | T816 |

17 tasks.

## Phase B — failing tests

All must fail for the right reason before any Phase C task starts.

### Tier 1 — preprocessor, no database

| ID | Task | Reqs | Deps |
|----|------|------|------|
| T830 | `charset_clause` — the declared set reaches the descriptor. `charset` has been `0` in every descriptor ever emitted; this is the first non-zero | FR-002.4 | T813 |
| T831 [P] | `charset_clause` — `CHARACTER SET IS` is accepted identically to `CHARACTER SET` | FR-002.4 | T813 |
| T832 [P] | `charset_clause` — `char v[n]` still yields capacity n and width n-1 with the clause present | FR-002.3 | T813 |
| T833 | `charset_varchar` — the `VARCHAR` shape still matches with **three extra tokens in the middle**. Gate 7's check is positional, so this is where it breaks | FR-002.6 | T814 |
| T834 [P] | `charset_varchar` — the layout assertions are still emitted and still hold | NFR-002.2 | T814 |
| T835 [P] | `charset_varchar` — indicator association survives a charset `VARCHAR` | FR-002.15 | T814 |
| T836 | `charset_keywords` — each mappable keyword yields its own distinct id | FR-002.8 | T815 |
| T837 [P] | `charset_keywords` — `UNKNOWN` and an absent clause both yield `0`. p.2-24 makes them equivalent | FR-002.8 | T815 |
| T838 | `charset_sync` — the two tables agree in both directions; a keyword on one side only fails | FR-002.8 | T825 |
| T839 [P] | `abi_isolation` and `contract_sync` — **unchanged**; this slice adds no ABI | FR-003.2, FR-003.3 | — |
| T840 [P] | `abi_only_symbols` — the emitted unit calls `esqlc_*` and nothing else | FR-003.1 | T813 |
| T841 [P] | `opaque_body_unchanged` — statement bodies still verbatim | NFR-001.1 | T813 |
| T842 [P] | `charset_clause` — placeholders, and no value in the statement text | FR-003.10 | T813 |
| T843 [P] | `negative/charset_unknown_keyword` — `ESQLC-2006`, code, line and column | FR-002.8 | T821 |
| T844 [P] | `negative/charset_unmapped` — `ESQLC-2013`, naming the set and saying the gap is MariaDB's | FR-002.8 | T822 |
| T845 [P] | `negative/charset_kanji` — `ESQLC-2014`, saying the *encoding is unspecified*. **A different code from T844**, because a gap in the manual and a gap in MariaDB send a user looking in different places | FR-002.8 | T823 |
| T846 [P] | `negative/national_character` — `ESQLC-1012`, naming the `KANJI` dependency | FR-002.8 | T824 |

### Tier 2 — live server

| ID | Task | Reqs | Deps |
|----|------|------|------|
| T847 | `rt/charset_high_bytes` — a byte above 0x7F round-trips unaltered. **This fails against the code as it stands**, because `latin1` transcoding alters it. It is the regression test for a defect latent since Gate 1 | FR-002.30 | T816 |
| T848 [P] | `rt/charset_roundtrip_1byte` — `ISO88592`, and no terminator appended | FR-002.28, NFR-002.1 | T817 |
| T849 | `rt/charset_roundtrip_2byte` — `len` is **4**, not 2. `length()` is 4 and `char_length()` is 2 on the server, so this fixture distinguishes bytes from characters and no other test can | NFR-002.1 | T818 |
| T850 [P] | `rt/charset_null` — the column becomes null and `val` is not read | FR-002.16 | T819 |
| T851 [P] | `rt/charset_family` — a column whose charset disagrees with the declared set is refused, not silently reinterpreted | FR-002.22 | T820 |
| T852 | **Every Gate 1–7 Tier 2 fixture passes unchanged under the binary client charset.** SQL text becomes binary too, so a string literal a program writes is now a binary literal; Gate 6's `update_injection_literal` contains one | FR-002.31 | T826 |

23 tasks.

## Phase C — implementation

| ID | Task | Reqs | Makes pass | Deps |
|----|------|------|-----------|------|
| T860 | `src/pp/charset.cc` — the keyword table: name → internal id, classified mapped / unmapped / unspecified | FR-002.8 | T836, T838 | Phase B |
| T861 | `src/pp/pp.h` — `HostVar::charset` | FR-002.4 | T830 | T860 |
| T862 | `src/pp/decl.cc` — parse the infix `CHARACTER SET cs` clause on a `char` array | FR-002.4, FR-002.3 | T830, T832 | T861 |
| T863 | `src/pp/decl.cc` — the optional `IS` | FR-002.4 | T831 | T862 |
| T864 | `src/pp/decl.cc` — the clause inside the `VARCHAR` shape check | FR-002.6 | T833 | T862 |
| T865 | `src/pp/emit.cc` — emit `charset` in the descriptor | FR-002.4, FR-003.1, FR-003.10, NFR-001.1 | T830, T840, T841, T842 | T864 |
| T866 | `src/pp/emit.cc` — the `VARCHAR` layout assertions survive the clause | NFR-002.2 | T834 | T865 |
| T867 | `src/pp/emit.cc` — indicator association with a charset `VARCHAR` | FR-002.15 | T835 | T865 |
| T868 | `src/pp/emit.cc` — refuse an unmapped keyword, naming the set | FR-002.8 | T844 | T865 |
| T869 | `src/pp/emit.cc` — refuse `KANJI` as *unspecified*, distinctly | FR-002.8 | T845 | T868 |
| T870 | `src/pp/emit.cc` — refuse an unrecognised keyword | FR-002.8 | T843 | T868 |
| T871 | `src/pp/emit.cc` — refuse `NATIONAL CHARACTER` naming its `KANJI` dependency | FR-002.8 | T846 | T869 |
| T872 | `src/pp/emit.cc` — `UNKNOWN` and an absent clause both emit `0` | FR-002.8 | T837 | T865 |
| T873 | `src/rt/charset.c` — internal id → MariaDB charset name and number | FR-002.8 | T838 | Phase B |
| T874 | `src/rt/context.c` — `character_set_client = binary`. **The corruption fix**, and the highest-value task in the slice | FR-002.30 | T847, T852 | Phase B |
| T875 | `src/rt/exec.c` — check the column's `charsetnr` against the declared set on retrieval | FR-002.22 | T851 | T873 |
| T876 | `src/rt/exec.c` — a charset `VARCHAR` binds, and its `len` is written in bytes | NFR-002.1 | T849 | T874 |
| T877 | `src/rt/exec.c` — no terminator appended at a charset column | FR-002.28 | T848 | T874 |
| T878 | `src/rt/exec.c` — a negative indicator on a charset `VARCHAR` sends null | FR-002.16 | T850 | T876 |
| T879 | `src/rt/exec.c` — padding stays the program's business under a binary client charset | FR-002.31 | T852 | T874 |
| T880 | `include/esqlc.h` **verified unchanged**, and the contract's `charset` comment says the id is project-internal rather than the SQLDA's (002 Q7) | FR-003.2, FR-003.3 | T839 | Phase B |

21 tasks.

## Phase D — diagnostics

One task per diagnostic condition this slice touches. **Four codes, because
four conditions**, and collapsing any two would send a user to the wrong place.

| ID | Task | Code | Reqs | Deps |
|----|------|------|------|------|
| T890 [P] | An unrecognised character-set keyword — a typo | `ESQLC-2006` | FR-002.8 | T870 |
| T891 [P] | A **known** keyword with no MariaDB counterpart: `ISO88593`/`4`/`5`/`6`. The gap is MariaDB's | `ESQLC-2013` | FR-002.8 | T868 |
| T892 [P] | `KANJI` — the keyword is known and the *encoding is unspecified*. The gap is the manual's | `ESQLC-2014` | FR-002.8 | T869 |
| T893 [P] | A retrieved column whose charset disagrees with the host variable's declared set | `ESQLC-2015` | FR-002.22 | T875 |

`NATIONAL CHARACTER` gets no code of its own: it refuses with `ESQLC-1012`
naming its dependency on `KANJI`, which is how this project refuses anything
out of slice. `ESQLC-2010`/`.2011`/`.2012` are still `DIV-042` and unreachable.

## Phase E — documentation and registry

| ID | Task | Reqs | Deps |
|----|------|------|------|
| T900 | Move the slice's rows in `docs/traceability.md` off `spec` — "Character set association with host variables" and the two type-mapping rows | — | Phase D |
| T901 | **Resolve `DIV-055`** — `proposed` → `accepted`, or amended with what the binary client charset actually required | — | T874 |
| T902 | **Record the correction to Gates 1–7 prominently**, not only inside `DIV-055`: FR-002.30's byte-verbatim guarantee held by accident for ASCII and was never tested above 0x7F | — | T847 |
| T903 | **Mark SD-1 resolved** in the slice document and stop carrying it. Seven gates have listed it as provisional; p.2-24 settles it | — | T872 |
| T904 | Record whether every Gate 1–7 Tier 2 fixture survived the binary client charset unchanged, and if any needed adjustment, why that is information about the decision rather than a test to fix | — | T852 |
| T905 | Confirm 002 Q7 (`sqlh` charset ids) is recorded as an external dependency alongside `SQLRM` and `CPG`, and that the contract warns 007 off the internal numbering | — | T880 |
| T906 | Re-examine SD-2, SD-10, SD-12, SD-13, SD-14 against what was built; record drift as a defect, not as precedent | — | Phase C |
| T907 | Confirm `diag_registry`, `contract_sync`, `citation_check`, `sqlsa_layout_sync` and `charset_sync` are clean | — | Phase D |
| T908 | Reconcile the slice's non-proof list against the as-built state, including that Japanese is refused | — | Phase D |
| T909 | Run `/speckit.analyze`, including the Principle VIII slice checks | — | T900–T908 |

10 tasks.

## Phase D′ — mutation, run after Phase C

| Mutation | Must fail |
|---|---|
| **Revert the client charset to `latin1`** | `charset_high_bytes` (T847) |
| Map `KSC5601` to `sjis` | `charset_roundtrip_2byte` (T849) |
| Accept `KANJI`, mapped to `sjis` | `negative/charset_kanji` (T845) |
| Report `len` in characters rather than bytes | `charset_roundtrip_2byte` (T849) |
| Drop a keyword from either table | `charset_sync` (T838) |
| Skip the retrieval charset check | `charset_family` (T851) |
| Collapse `ESQLC-2013` and `2014` into one code | `negative/charset_kanji` (T845) |

**The first is a mutation back to the current behaviour**, which is the clearest
available statement of what this slice fixes. It should fail loudly, and if it
does not, `charset_high_bytes` is not testing what it claims.

The `len`-in-characters mutation is the plausible-wrong-value class: 2 instead
of 4 looks entirely reasonable, and it is why T849 asserts a specific number
rather than merely a non-zero one.

**The standing rebuild warning, now at eight occurrences with eight distinct
causes.** The most recent: `make` treats an object as current when source and
object share an mtime, and `stat` resolves to whole seconds, so a `touch`
followed immediately by a build can leave the old binary in place while every
timestamp check passes. After every mutation: confirm the mutation is present in
the file, confirm the binary changed **by content hash rather than timestamp**,
and confirm the restore restored the right file — a backup filename that did not
match the restore path caused occurrence seven.

## Requirement coverage

| Requirement | Phase B | Phase C |
|---|---|---|
| NFR-001.1 | T841 | T865 |
| FR-002.3 | T832 | T862 |
| FR-002.4 | T830, T831 | T861, T862, T863, T865 |
| FR-002.6 | T833 | T864 |
| FR-002.8 | T836, T837, T838, T843, T844, T845, T846 | T860, T868, T869, T870, T871, T872, T873 |
| FR-002.15 | T835 | T867 |
| FR-002.16 | T850 | T878 |
| FR-002.22 | T851 | T875 |
| FR-002.28 | T848 | T877 |
| FR-002.30 | T847 | T874 |
| FR-002.31 | T852 | T879 |
| FR-003.1 | T840 | T865 |
| FR-003.2 | T839 | T880 |
| FR-003.3 | T839 | T880 |
| FR-003.10 | T842 | T865 |
| NFR-002.1 | T848, T849 | T876 |
| NFR-002.2 | T834 | T866 |

**17 of 17 covered. Zero requirements without an implementing task.**

## Critical path

```
T814 ─ charset VARCHAR fixture
  └─ T833 ─ the positional shape check breaks
       └─ T860 ─ keyword table
            └─ T861 ─ HostVar::charset
                 └─ T862 ─ parse the infix clause
                      └─ T864 ─ inside the VARCHAR shape
                           └─ T865 ─ emit the descriptor
                                └─ T876 ─ bind it, len in bytes
                                     └─ T849 ─ the 2-byte round-trip passes
```

Nine deep. But **start with T874**, which is three tasks from nothing:
`T816 → T847 → T874`. It is the corruption fix, it is the highest-value change
in the slice, and `T852` — every previous gate's Tier 2 fixtures under the new
client charset — gates the whole slice on it. If the binary client charset
breaks something Gates 1–7 rely on, that must be known before nine tasks of
parsing work are built on top of it.

The parsing chain is independent of T874 and can proceed in parallel.

## Exit criteria

The slice's ten, plus:

11. Every Gate 1–7 Tier 2 fixture passes unchanged under the binary client
    charset.
12. Every mutation in Phase D′ fails its named test, with the mutation verified
    present and the binary verified changed by content.
13. `DIV-055` resolved; the correction to Gates 1–7 recorded outside it too.
14. **SD-1 marked resolved** after seven gates as provisional.
15. `docs/traceability.md` charset row no longer `spec`.
