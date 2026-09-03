# Gate 9 plan — `INVOKE`

**Slice:** [specs/gate-9.md](gate-9.md) · **Specs:** 001, 002, 003, 006 ·
**Planned under Principle VIII** (002, 006 are `Clarifying`) · **Phase 3**,
entered under the amended phase rule

Slice conditions verified: enumerated subset (13 in-scope plus 13 carried),
avoidance table covering all 23 open questions across the four specs, four
provisional decisions, and a specific non-proof section.

## 1. Approach

**Read a tab-separated schema cache, generate declarations into the declare
section, and re-parse them with the parsers Gates 7 and 8 built.**

The generation itself is settled by §2 p.2-22, which prints `INVOKE`'s output
verbatim. Reading it corrected three things this plan would otherwise have got
wrong, and one thing the slice document itself got wrong.

**The indicator precedes its field.** p.2-22:

```
char  CHARACTER SET ISO88591 type_char1_null[11];   /* preceded by ... */
short type_char1_null_i;                            /* ... this */
```

— in the published order, `short type_char1_null_i;` comes *first*. FR-006.5
says so plainly (*"and precedes its host variable"*); I had written "as a
sibling field" in the slice document, which is vague enough to have been
implemented either way. It is a published detail and a program's source will
depend on it.

**The suffix appears as `_i`, lowercase.** FR-006.5b says `_I` is appended, and
every indicator in p.2-22 reads `_i`. Both are true: SQL/MP appends `_I` to the
uppercase catalogue name and FR-006.2 lowercases the whole identifier for C. The
emitter must produce `_i`, and a fixture asserting `_I` would pass a wrong
implementation.

**The provenance comment has a published format**, two lines:

```
/* Record Definition for table \NEWYORK.$DISK1.SQL.TYPESC2 */
/* Definition current at 13:52:19 - 8/27/96  */
```

FR-006.5d asks for the object and the timestamp; p.2-22 fixes the shape. Worth
matching, because it is the only provenance a program carries until 001's
listing exists.

**And the slice document was wrong about FR-006.5c.** It said the 30/31-character
truncation should be *"faithfully reproduced, because a program built on NonStop
has the truncated name in its source"*. FR-006.5c says the opposite, and is
right: SQL/MP's truncation makes the indicator name **identical to the field
name**, which is two struct members with one name — invalid C on NonStop too.
The requirement says **diagnose the collision rather than reproduce it**, and
`ESQLC-6007` is already registered for it. Corrected here; the slice document is
amended in the same change.

## 2. Alternatives rejected

**JSON for the cache, as SD-15 said.** Rejected at plan stage, and the slice
decision is amended. The preprocessor has **zero third-party dependencies** —
`grep` over `src/pp` finds only `<string>`, `<vector>`, `<sstream>` and friends
— so JSON means either vendoring a library, against a discipline eight gates
have kept, or hand-writing ~150 lines of parser as new attack surface for a file
that holds five fields per column.

A **tab-separated line format** parses in about thirty lines, is *more* diffable
than JSON in review (one column per line, no brace noise), and adds nothing to
the dependency set. SD-15's reasoning — committed, diffable, no network at build
time — is unchanged; only the encoding moves.

**Live schema access with the cache as a fallback.** Rejected: it makes
NFR-001.2 conditional on which machine ran the build, and the CI job that
asserts MariaDB is absent would then be testing a different code path from the
one developers use.

**Trusting generated declarations instead of re-parsing them.** Rejected, and
this is the load-bearing choice. Generated text goes into the declare section
and `decl.cc` harvests descriptors from it exactly as for hand-written code, so
there is one path to be right. It also makes Gates 7 and 8 the test of what this
slice emits: if `INVOKE` writes a `VARCHAR` structure Gate 7's shape check
cannot read, or a `CHARACTER SET` clause Gate 8's parser rejects, the build
fails rather than the runtime misbehaving.

**Generating a separate header per invoked object.** Rejected: `#include`
ordering would decide whether a declaration is inside a declare section, which
is exactly the fragility FR-006.1's declaration-position rule exists to avoid.

## 3. Components

| Component | Path | Change | Slice scope |
|-----------|------|--------|-------------|
| Schema cache reader | `src/pp/schema.cc` | **new** — tab-separated reader, five fields per column | one table per invoke |
| `INVOKE` generator | `src/pp/invoke.cc` | **new** — the declaration text of p.2-22 | base tables |
| Shared types | `src/pp/pp.h` | `SchemaColumn`, `Schema` | — |
| Dispatcher | `src/pp/dispatch.cc` | `INVOKE` implemented; declaration position | — |
| Emitter | `src/pp/emit.cc` | emit generated text into the declare stream, then re-parse it | — |
| Main | `src/pp/main.cc` | `--schema <file>` option | not a source-level path |
| Cache fixture | `tests/conformance/gate-1/schema.cache` | **new** — the invoked tables | `parts`, `charsets` |
| Harness | `tests/harness/` | Gate 9 cases | — |

Eight components, two new source files.

**Stubs that must fail loudly.** `NCHAR` and `NCHAR VARYING` refuse with
`ESQLC-1012` naming the `KANJI` chain, as Gate 8 refuses `NATIONAL CHARACTER`.
`PREFIX`, `SUFFIX` and `NULL STRUCTURE` refuse by name (006 Q2 partially
resolved). A `=name` MAP DEFINE for the object refuses on `DIV-002`. A column
type with no 002 mapping is `ESQLC-6003`, not a guess.

## 4. Runtime ABI surface

**No new entry points, no signature change** — third slice running, and this
time structurally rather than by luck. `INVOKE` generates *declarations*. They
are harvested into the same `esqlc_hostvar_t` descriptors by the same parser and
reach the runtime through `esqlc_stmt_exec` unchanged. **The runtime cannot tell
a generated structure from a hand-typed one**, which is the design working
rather than a coincidence.

The contract needs no edit.

## 5. Data structures

No new *runtime* layout, so Principle VI's `sizeof`/`offsetof` obligation falls
where it already does. But this slice **generates** a layout, so the obligation
applies to what it writes:

- A generated `VARCHAR` group gets Gate 7's three assertions —
  `sizeof(v.len) == 2`, `sizeof(v.val) == n`, and the `__typeof__` offset of
  `val` — because it goes through the same emitter path.
- A generated structure gets no `sizeof` assertion of its own, deliberately:
  its total is not API. Nothing copies it by length and nothing shares it
  `EXTERNAL`, unlike the `SQLCA` where 430 is load-bearing. Asserting a total
  would pin padding the manual never specifies.

The cache format, as a table rather than a layout:

```
# esqlc schema cache v1 — tab-separated, one column per line.
# table<TAB>column<TAB>sqltype<TAB>length<TAB>nullable<TAB>charset
!captured<TAB>2026-09-03T11:20:14Z
parts<TAB>part_num<TAB>SMALLINT<TAB>2<TAB>N<TAB>UNKNOWN
parts<TAB>part_desc<TAB>CHAR<TAB>18<TAB>N<TAB>ISO88591
parts<TAB>weight<TAB>SMALLINT<TAB>2<TAB>Y<TAB>UNKNOWN
```

Five fields plus a capture timestamp, which is exactly what FR-006.2 through
FR-006.5d need. A cache mirroring `information_schema` wholesale would be a
second schema language to maintain.

## 6. Requirement → component map

| Requirement | Component(s) | Test |
|-------------|--------------|------|
| NFR-001.1 opaque bodies | emit | `opaque_body_unchanged` |
| NFR-001.2 no MariaDB at Tier 1 | main, schema | `abi_isolation`, the CI no-MariaDB job |
| FR-002.3 `CHAR(l)` → `char v[l+1]` | invoke | `invoke_types` |
| FR-002.4 `CHARACTER SET` carried | invoke | `invoke_charset` |
| FR-002.6 `VARCHAR` structure | invoke | `invoke_varchar` |
| FR-002.8 recognised charset keywords | invoke, schema | `invoke_charset` |
| FR-002.9 integer widths | invoke | `invoke_types` |
| FR-002.15 indicator association | emit | `rt/invoke_roundtrip` |
| FR-002.16 negative indicator means null | emit | `rt/invoke_null` |
| FR-002.30 `width` bytes verbatim | emit | `rt/invoke_roundtrip` |
| FR-003.1 `esqlc_*` calls only | emit | `abi_only_symbols` |
| FR-003.10 values bound, never interpolated | emit | `invoke_placeholders` |
| NFR-002.2 widths asserted statically | emit | `invoke_varchar` |
| FR-006.1 `INVOKE` in declaration position | dispatch | `invoke_basic`, `negative/invoke_exec_position` |
| FR-006.2 field names lowercased, 002 types | invoke | `invoke_types` |
| FR-006.2a tag is `<name>_type` | invoke | `invoke_basic` |
| FR-006.2b `CHARACTER SET` emitted inline | invoke | `invoke_charset` |
| FR-006.3 the terminator byte | invoke | `invoke_types` |
| FR-006.4 `VARCHAR` nested group | invoke | `invoke_varchar` |
| FR-006.5 an indicator per nullable column, **preceding** it | invoke | `invoke_indicators` |
| FR-006.5b default suffix, emitted `_i` | invoke | `invoke_indicators` |
| FR-006.5c 30/31-char collision **diagnosed** | invoke | `negative/invoke_indicator_collision` |
| FR-006.5d provenance comment | invoke | `invoke_provenance` |
| FR-006.6 unknown object is an error | schema | `negative/invoke_unknown_object` |
| FR-006.8 `:tag.field` references | emit | `rt/invoke_roundtrip` |
| NFR-006.2 optional-by-cache | schema, main | `negative/invoke_no_cache`, `abi_isolation` |

**26 requirements, all mapped exactly once. Zero unmapped.**

## 7. Test strategy

**Tier 1 carries almost all of it**, which is unusual and is the point: `INVOKE`
is a code generator, so its output is checkable without a database. The
strongest check is that **generated declarations compile and re-parse** — if the
emitted `VARCHAR` group fails Gate 7's assertions or the emitted `CHARACTER SET`
clause fails Gate 8's parser, Tier 1 goes red.

**Tier 2** proves the generated structure is *usable*: `:parts_rec.part_num` in
a real statement, round-tripped, and a null through a generated indicator.
Without it, generation is only text that happens to compile.

**Mutation, Phase D′.** Put the indicator *after* its field and
`invoke_indicators` must fail; emit `_I` instead of `_i` and it must fail; drop
the `CHARACTER SET` clause and `invoke_charset` must fail; reproduce the 30/31
collision instead of diagnosing it and the negative must fail; omit the capture
timestamp and `invoke_provenance` must fail; make the preprocessor open a socket
and `abi_isolation` must fail.

The indicator-order and `_i`/`_I` mutations are the plausible-wrong class: both
produce a structure that compiles and looks right, and both would break a
customer program's source at a name or an offset. They are why those fixtures
assert order and spelling rather than mere presence.

## 8. Risks

**A stale cache generates wrong structures and compiles cleanly.** SD-16 accepts
this: the preprocessor cannot detect staleness without the connection NFR-001.2
forbids. The capture timestamp in generated output makes it diagnosable
afterwards and nothing makes it preventable here. It is the slice's largest
exposure and it is a *design* consequence rather than an oversight —
`ESQLC-6005` ("cached schema is stale relative to a reachable database") is
registered and, like Gate 8's `ESQLC-2015`, **unreachable in this design**,
because reaching a database is precisely what the preprocessor does not do.

**A generated identifier can collide with a C keyword or another field.** A
column named `int`, `struct` or `register` lowercases to a C keyword and the
generated structure will not compile — with the error pointing at generated
text, not at the customer's source. `ESQLC-6004` is registered for
untransformable names; whether a keyword collision falls under it is a decision
this slice must make rather than discover at a customer's site.

**`CHAR_AS_STRING` is assumed and is 001 Q2.** SD-10 governs both what Gate 7
reads and what this slice writes, so a program compiled `CHAR_AS_ARRAY` gets
generated arrays one byte too long. The two ends now share one assumption, which
is better than two — and worse than resolving it.

**The generated text occupies source lines that do not exist.** `#line`
directives must keep pointing at the `INVOKE` statement rather than drifting
into generated territory, or every subsequent diagnostic in the unit is
misattributed. This is the defect class Gates 1 and 2 both hit, and generated
multi-line output is the largest opportunity for it so far.

**Only the tables the fixtures invoke are cached.** NFR-006.1's App. A database
is out of scope, so nothing proves the format survives a real schema's variety —
generated columns, defaults, unusual collations. The format is provisional
precisely because it has one consumer and one producer, both written here.

## 9. Divergences introduced

**One new: `DIV-056` — the 30/31-character indicator collision is diagnosed, not
reproduced.** FR-006.5c records that SQL/MP truncates the `_I` suffix at those
name lengths, producing an indicator whose name equals its host variable's. That
is two struct members with one identifier, which no C compiler accepts, so
SQL/MP's behaviour cannot be reproduced even in principle. `ESQLC-6007` refuses
it at preprocess time. Migration: a program with a 30- or 31-character nullable
column name must shorten it or supply an explicit `SUFFIX` — which is out of
this slice, so for now it must shorten.

**`DIV-055` is narrowed, not amended.** Gate 8 left a silent failure: a
hand-written declaration whose character set disagrees with its column is not
refused, because result metadata cannot supply the column's set. `INVOKE` makes
generated declarations agree by construction (FR-006.2b), so the path a program
is *supposed* to use is no longer exposed. Hand-written declarations still are,
and that text in `DIV-055` stays.

**SD-15 is amended in the slice document** — tab-separated rather than JSON, for
the dependency reason in section 2. Not a divergence; a plan-stage correction to
a provisional decision.
