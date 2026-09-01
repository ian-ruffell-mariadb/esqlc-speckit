# Gate 5 plan — the SQLSA

**Slice:** [specs/gate-5.md](gate-5.md) · **Specs:** 001, 002, 003, 004, 005 ·
**Planned under Principle VIII** (005 is `Clarifying`)

Slice conditions verified before planning: enumerated subset (13 in-scope plus 8
carried), avoidance table covering all 35 open questions across the five specs,
four provisional decisions, and a specific non-proof section.

## 1. Approach

**Emit the published layout as fixed-width packed structures, and make the
runtime write into the program's storage through one shared offset table.**

The `SQLSA` inverts Gate 4's problem. The `SQLCA` had no published layout, so
`DIV-041` made it an opaque blob and the only obligation was the 430-byte
total. Here the layout *is* published, so the preprocessor must emit real named
fields that programs will reference directly — `sqlsa.u.dml.stats[0].records_used`
is source a customer writes.

Two facts settle the emission, both established by compiling the published
declarations rather than by reading them:

**The manual's `long` is 32-bit.** Emitting the declarations verbatim on any
LP64 host gives a `long` of 8 bytes and a structure nowhere near 838. Every
integer field is therefore emitted as an explicit `int16_t`/`int32_t`/`int64_t`.
This is not a style preference; it is the difference between conforming and not.

**Both versions need packing, not just v330.** FR-005.27 records the alignment
pragma for the four `*_R330` types, which reads as though v300 needs none. It
does:

| | natural | packed | manual |
|---|---|---|---|
| v300 | 840 | **838** ✓ | 838 |
| v330 | 1864 | **1790** ✓ | 1790 |

v300 misses by exactly the two padding bytes a compiler inserts after
`num_tables` before the 4-aligned `stats[]`. So the pragma applies to both
families, and FR-005.27 understates the requirement. Recorded as a finding
against 005 rather than silently implemented.

**One offset table, two consumers.** The preprocessor emits the struct; the
runtime, which cannot include preprocessor output, writes fields by offset. Two
independent encodings of the same layout is exactly the drift Principle VI
exists to prevent, so a Tier 1 harness extracts both and compares them.

**Reset and undefined are the same mechanism.** FR-005.20 wants every statement
to reset the structure; FR-005.19 wants it not to be accidentally meaningful
after a statement class that leaves it undefined. Both are satisfied by stamping
every field with its sentinel at the *start* of each statement, then filling
only what that statement can honestly supply. A statement that supplies nothing
leaves sentinels, which is precisely the required behaviour, with no separate
code path to get wrong.

## 2. Alternatives rejected

**An opaque blob with accessors, as Gate 4 did for the `SQLCA`.** Rejected
because the layout is published. Programs index these fields by name; hiding
them behind accessors would force a source change on every program that reads
statistics, which Principle II forbids. The `SQLCA` could be opaque *only*
because no conforming program could have been indexing it.

**Deriving `table_name` by parsing the statement.** Rejected: NFR-001.1 makes
statement bodies opaque to the preprocessor, and parsing them here would
undo the single decision that has kept the preprocessor tractable across four
gates. The runtime reads table names from result-set metadata instead, which
costs nothing extra and is accurate by construction.

**Populating both version families at runtime.** Rejected as doubling the work
to exercise one code path at a different integer width. SD-7 is deliberately
width-independent so nothing depends on which family runs.

**Generating the struct from a shared header included by both sides.** Attractive,
and rejected: the runtime is C11 with no MariaDB or preprocessor dependency, and
the emitted structure must be named exactly as the manual names it in the
*program's* namespace. A comparison harness gets the same safety without
coupling the two.

## 3. Components

| Component | Path | Change | Slice scope |
|-----------|------|--------|-------------|
| SQLSA emission | `src/pp/sqlsa.cc` | **new file** — both layouts, fixed-width, packed, static asserts | v300 and v330 declarations |
| Dispatcher | `src/pp/dispatch.cc` | `INCLUDE SQLSA` implemented; `INCLUDE SQLDA` keeps `ESQLC-1012` | — |
| Emitter | `src/pp/emit.cc` | emit the declaration and its registration, mirroring the `SQLCA` path | version from `INCLUDE STRUCTURES` |
| Runtime: SQLSA | `src/rt/sqlsa.c` | **new file** — registration, sentinel stamping, population by offset | v300 populated |
| Runtime: cursor | `src/rt/cursor.c` | populate after `OPEN`, `FETCH`, `CLOSE` | the slice's live path |
| Runtime: exec | `src/rt/exec.c` | stamp on every statement; populate what DML can supply | `records_used` only |
| ABI header | `include/esqlc.h` | one new entry point | — |
| Contract | `specs/003-…/contracts/` | the same one, same change (Principle V) | — |
| Layout sync guard | `tests/harness/sqlsa_layout_sync.sh` | **new** — emitter offsets vs runtime offsets | both versions |

Nine components, two new source files, one new harness.

## 4. Runtime ABI surface

**One new entry point.**

```c
/* Register the program's SQLSA so the runtime populates it after each
   statement. Emitted by INCLUDE SQLSA. `version` is 300 or 330 and `len` must
   equal that version's SQLSA_LEN — two published layouts mean the runtime
   cannot infer which one it was handed from the pointer alone. */
int esqlc_sqlsa_register(void *sqlsa, size_t len, int version);
```

Registration rather than runtime-held state, for the reason Gate 4 established
and §9 p.9-13 reinforces: the manual tells programs to save values immediately
after a statement and to declare more than one `SQLSA` where needed. Both
idioms require the data to live in the program's own storage.

No accessor entry points. Unlike the `SQLCA`, the fields are public by
publication, so a program reads them directly.

## 5. Data structures

Emitted per `INCLUDE SQLSA`, at the version selected by `INCLUDE STRUCTURES`:

```c
#define SQLSA_EYE_CATCHER "SA"
#define SQLSA_LEN 838                       /* or 1790 at v330 */

struct SQLSA_TYPE { … } __attribute__((packed));

_Static_assert(sizeof(struct SQLSA_TYPE) == SQLSA_LEN, "SQLSA_LEN");
_Static_assert(offsetof(struct SQLSA_TYPE, eye_catcher) == 0, "eye-catcher leads");
_Static_assert(offsetof(struct SQLSA_TYPE, u.dml) ==
               offsetof(struct SQLSA_TYPE, u.prepare), "dml/prepare is a union");
_Static_assert(sizeof(struct STATS_TYPE) == 52, "stats stride");   /* 108 at v330 */
```

**Which offsets get asserted.** Principle VI asks for every externally visible
`offsetof`. Asserting all 16 `stats[]` entries field-by-field would be 100+
assertions restating one stride. Instead: every header field, both union arms at
the same offset, every field within `stats[0]`, and `stats[1]` against
`stats[0]` — which pins the stride — with `sizeof` pinning the total. Any layout
error is caught by that set; the remaining 14 entries follow arithmetically.

## 6. Requirement → component map

| Requirement | Component(s) | Test |
|-------------|--------------|------|
| NFR-001.1 opaque bodies | emit | `opaque_body_unchanged` |
| FR-003.1 `esqlc_*` calls only | emit | `abi_only_symbols` |
| FR-003.2 no MariaDB type in the header | include/esqlc.h | `abi_isolation` |
| FR-003.3 signatures mirrored in the contract | contract | `contract_sync` |
| FR-004.11 `DECLARE CURSOR` | dispatch | `cursor_declare` |
| FR-004.12 `OPEN` reads inputs now | rt/cursor | `rt/sqlsa_cursor_stats` |
| FR-004.13 `FETCH` writes outputs | rt/cursor | `rt/sqlsa_cursor_stats` |
| FR-004.14 `sqlcode` 100 at end of set | rt/cursor | `rt/sqlsa_accumulate` |
| FR-004.15 `CLOSE` releases the result set | rt/cursor | `rt/sqlsa_cursor_stats` |
| FR-005.8 `INCLUDE STRUCTURES` forms | dispatch | `sqlsa_version_select` |
| FR-005.9 accepted versions incl. 330 for `SQLSA` | dispatch | `sqlsa_version_select` |
| FR-005.12 ordering before `INCLUDE SQLSA` | emit | `negative/structures_after_sqlsa` |
| FR-005.16 `SQLSA_LEN` 838/1790, eye-catcher `SA` | pp/sqlsa | `sqlsa_sizes` |
| FR-005.17 populated after DML and cursor ops | rt/cursor, rt/exec | `rt/sqlsa_cursor_stats` |
| FR-005.19 undefined after DSL/DDL/DCL/txn control | rt/sqlsa | `rt/sqlsa_after_commit` |
| FR-005.20 every statement resets, incl. `FETCH` | rt/sqlsa | `rt/sqlsa_accumulate` |
| FR-005.21 published layout, packed | pp/sqlsa | `sqlsa_layout`, `sqlsa_layout_sync` |
| FR-005.21a `dml`/`prepare` is a union | pp/sqlsa | `sqlsa_layout` |
| FR-005.21b counter widths per family | pp/sqlsa | `sqlsa_layout` |
| FR-005.21c VSBB only at v330 | pp/sqlsa | `sqlsa_layout` |
| FR-005.22 `num_tables` and valid `stats[]` entries | rt/sqlsa | `rt/sqlsa_two_tables` |
| FR-005.23 VSBB flags `-1`/`0` | pp/sqlsa | `sqlsa_layout` |
| FR-005.25 sentinels, never zero | rt/sqlsa | `rt/sqlsa_sentinels` |
| FR-005.27 alignment pragma | pp/sqlsa | `sqlsa_sizes` |
| NFR-005.1 byte-exact structures | pp/sqlsa | `sqlsa_sizes`, `sqlsa_layout_sync` |

**25 requirements, all mapped exactly once. Zero unmapped.**

FR-005.22 is mapped but scoped: `num_tables` reaches 2, and the 16-entry cap is
in the slice's non-proof list.

## 7. Test strategy

**Tier 1, no server.** The whole of the layout obligation runs here: sizes,
offsets, the union, the version differences, and the emitter-versus-runtime
offset comparison. This is the majority of the gate's value and it costs a
compile.

**Tier 2, live.** Statistics need a real statement to measure. Four fixtures,
matching the slice's four programs — the accumulator loop, the two-table join,
the sentinel read, and the post-`COMMIT` read.

**Mutation, in Phase D.** The guards that must be proved load-bearing: remove
the packing attribute and `sqlsa_sizes` must fail; widen a counter at v300 and
`sqlsa_layout` must fail; skip the per-statement stamp and `sqlsa_accumulate`
must fail; return zero instead of a sentinel and `sqlsa_sentinels` must fail.

The stamping mutation matters most. A missing reset produces *plausible* numbers
— the previous statement's — which is the failure no size assertion can catch
and the reason §9 prescribes the accumulator idiom. Note for whoever runs this:
this project's mutation harness has produced false passes four times, always by
failing to rebuild. Confirm the binary's timestamp changed.

## 8. Risks

**`records_accessed` may have no honest source.** MariaDB exposes rows-examined
per statement only through `performance_schema` or the slow log, neither of
which is a per-connection synchronous read. If it cannot be sourced cheaply it
becomes a sentinel and `DIV-011` grows by one field — which is a documented
outcome, not a failure, but it means `stats[]` may end up more sentinel than
statistic. Decide during Phase C with the numbers in front of you.

**The emitter and the runtime encode the same layout twice.** Mitigated by the
sync harness, which is why that harness is a component and not a nicety. Without
it, a field added to one side and not the other produces silent corruption of
the program's own memory.

**Stamping cost is 1790 bytes per statement at v330.** Fourteen of sixteen
`stats[]` entries are always sentinels in this slice, and they are rewritten on
every statement including every `FETCH` in a tight loop. Acceptable at fixture
scale; measure before assuming it stays acceptable, because a `FETCH` loop over
a large result set is the exact shape where it would not.

**`table_name` from result metadata covers `SELECT` but not DML.** `INSERT`,
`UPDATE`, and `DELETE` return no result-set metadata, so the field has no source
without parsing the statement — which NFR-001.1 forbids. In this slice DML
leaves it at the character sentinel. A general answer is needed before
FR-005.17 can be called done.

**v300's packing requirement is inferred, not documented.** The arithmetic is
unambiguous — 838 is only reachable packed — but the manual documents the pragma
only for v330. If a real NonStop v300 `SQLSA` measures 840, the manual's stated
size is wrong rather than our reading of it, and this needs a divergence.

## 9. Divergences introduced

**`DIV-011` is updated, not added to.** Its **Detection** field currently reads
"the sentinel value, to be fixed by 005's spec"; SD-7 and SD-8 fix it. Status
moves `proposed` → `accepted`.

**SD-8 is new and this plan proposes it.** SD-7 covers unmappable *numeric*
fields (`-1` in the field's own width) but says nothing about character fields,
and `table_name` needs an answer for the DML path. Proposed: an unmappable
character field is filled with `?` to its full declared width — visibly not a
table name, distinguishable from both an empty string and a blank-padded one,
and safe to print. Added to the slice document in the same change.

No new divergence entry. If `records_accessed` proves unmappable, `DIV-011`
absorbs it.
