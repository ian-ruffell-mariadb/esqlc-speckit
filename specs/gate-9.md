# Gate 9 — `INVOKE`

**Slice of:** 001, 002, 003, 006 · **Status:** ready to plan ·
**Predecessor:** Gate 8 (character sets) · **Phase 3**, entered under the
amended phase rule with Phase 2's debt recorded in [ROADMAP.md](../ROADMAP.md)

The first slice where the preprocessor reads something other than source, and
the first where a structure a customer program uses is *generated* rather than
hand-written and inspected.

Eight gates have taught the preprocessor to read declarations. This one makes it
write them.

## The architectural crux, and it is already answered

FR-006.2e says `INVOKE` *"requires read access to the invoked object at
preprocess time"* (§2 p.2-19). NFR-001.2 says the Tier 1 suite must run on a
machine with no MariaDB at all, and `abi_isolation` plus a CI job enforce it.
Those look irreconcilable.

NFR-006.2 already resolves it: schema access is **optional-by-cache**. The
preprocessor reads a cache file and never opens a socket. A separate tool
captures the cache from a live database. NFR-001.2 survives intact — the
preprocessor's dependency set does not change at all — and `INVOKE` gets the
schema it needs.

That is the decision this slice has to make concrete, and 006 Q4 is explicitly
*"a build-reproducibility decision, not a manual question"*, so it is ours.

## `INVOKE` closes Gate 8's gap without doing Gate 8's check

Gate 8 planned a runtime check that a host variable's declared character set
matches the column's, and found it unimplementable: result metadata reports the
*result set's* charset, never the column's. `ESQLC-2015` is registered and never
emitted, and `DIV-055` records the silent failure that leaves.

`INVOKE` closes it from the other end. FR-006.2b has it emit `CHARACTER SET`
inline in each generated field declaration, from the cached column definition —
so for a generated structure the declared set **is** the column's set, by
construction. There is nothing to check because there is nothing to disagree.

Hand-written declarations stay unchecked, and that remains `DIV-055`. But the
path a program is *supposed* to use stops being exposed, which is a better
outcome than the check would have been.

Gate 8 also made this possible: FR-006.2b needs the emitter to produce a
`CHARACTER SET` clause the preprocessor can then read back, and until Gate 8
neither end existed.

## The programs

**A — one table, one structure.** `INVOKE parts AS parts_rec` against the
cached schema. Field names are the column names lowercased (FR-006.2), the tag
is `parts_type` (FR-006.2a), types follow 002's mapping, and the emitted C
compiles.

**B — the generated structure is usable.** `:parts_rec.part_num` in a real
statement, round-tripped. FR-006.8 makes individual fields referenceable, and
this is the fixture that proves generation is not just text.

**C — indicators for nullable columns only.** `weight` is nullable and
`part_num` is not, so exactly one indicator is generated, named `weight_I`
(FR-006.5, FR-006.5b). A structure with an indicator for every column would
look right and be wrong.

**D — the 30/31-character truncation.** FR-006.5c: at those lengths SQL/MP
truncates the `_I` suffix rather than the name. A quirk, faithfully reproduced,
because a program built on NonStop will have the truncated name in its source.

**E — a `VARCHAR` column.** FR-006.4 generates the nested `short len; char
val[]` structure — the shape Gate 7 taught the preprocessor to *read*, now
written by it, with Gate 8's `CHARACTER SET` clause inside it.

**F — provenance.** FR-006.5d: the generated output carries a comment naming the
invoked object and the timestamp of its definition, so a compiled program's
assumptions are auditable.

**G — the refusals.** An object absent from the cache (FR-006.6), and no cache
at all. Both are preprocess-time errors naming what is missing, never a silently
empty structure.

## Exit criteria

1. `INVOKE obj AS tag` generates a structure whose tag is `obj_type`, with
   fields named for the columns, lowercased.
2. The generated C compiles, and its field widths match 002's mapping.
3. `:tag.field` is usable as a host variable in a statement, and round-trips.
4. An indicator is generated for each nullable column and for no other.
5. The default indicator suffix is `_I`, and the 30/31-character truncation
   behaves as FR-006.5c describes.
6. A `VARCHAR` column generates the nested two-field structure.
7. A column with a character set generates an inline `CHARACTER SET` clause
   carrying that set — so a generated declaration cannot disagree with its
   column.
8. Generated output names the invoked object and its definition timestamp.
9. An object absent from the cache is a preprocess-time error naming it; a
   missing cache is a distinct error naming the file.
10. **The preprocessor still links no MariaDB library and opens no socket.**
    `abi_isolation` unchanged; Tier 1 green with no MariaDB present.

## Slice decisions

SD-2 and SD-10 carry forward. SD-1 is **resolved** (Gate 8) and no longer
carried. Two new.

- **SD-2** — the program declares `long sqlcode;`. Narrows 005 Q8.
- **SD-10** — a hand-declared `VARCHAR`'s `capacity` is the declared `val` size
  and `width` is `capacity - 1`. Narrows nothing; still assumes
  `CHAR_AS_STRING`. **Now also governs what `INVOKE` generates**, which makes
  FR-006.3 and 001 Q2 the same question seen twice.
- **SD-15 (new)** — the schema cache is a **committed JSON file** at a path the
  pragma or a compiler option names, and the preprocessor reads only that.
  Narrows 006 Q4. Committed rather than generated at build time because
  FR-006.7 wants a program's assumptions auditable and a build must be
  reproducible without network access; JSON because the project already reads
  `manual/pages.json` and because a schema change should show up in review as a
  diff. **Provisional** — the capture tool's own shape is out of scope, so
  nothing yet proves the format survives a second consumer.
- **SD-16 (new)** — **the preprocessor cannot detect a stale cache and does not
  pretend to.** It records the cache's capture timestamp in generated output
  (FR-006.5d) so a mismatch is diagnosable after the fact, and staleness
  detection belongs to the build system. Narrows 006 Q4's invalidation half.
  **Provisional**, and deliberately modest: a preprocessor that silently
  regenerated from a live database would break NFR-001.2, and one that refused
  on any doubt would make offline builds impossible.

## Design questions this slice must settle

- **Where the cache path comes from.** A compiler option, not the source: a
  `.sqlc` file naming a filesystem path would make the source unportable, which
  is the opposite of what this project is for.
- **What the cache contains.** Column name, SQL type and length, nullability,
  character set, and the definition timestamp — exactly what FR-006.2 through
  FR-006.5d need and nothing else. A cache that mirrors `information_schema`
  wholesale would be a second schema language to maintain.
- **Whether generated declarations are re-parsed or trusted.** Re-parsed. The
  emitter writes them into the declare section and `decl.cc` harvests
  descriptors from them exactly as it does for hand-written ones, so there is
  one path to be right rather than two. It also means Gate 7's and Gate 8's
  parsers are the test of what Gate 9 emits.
- **Where the indicator lives.** Inside the generated structure, as a sibling
  field. §2 p.2-22's example puts it there, and it keeps `:tag.weight_I`
  addressable by the same `:struct.field` rule as everything else.

## Open-question avoidance

Every open question in the four specs this slice touches.

| Question | Touched? | Why not |
|---|---|---|
| 001 Q1 declaration vs executable position | no | `INVOKE` is declaration position, which the dispatch table already enforces |
| **001 Q2 `#pragma SQL` option set** | **yes** | `CHAR_AS_STRING` decides FR-006.3's extra byte, and FR-006.2c's `NCHAR` needs the default multibyte set. Carried **SD-10**; the `NCHAR` half is out of scope |
| 001 Q3 `SQL SOURCE` | no | not used |
| 001 Q4 C label prefix | no | not used |
| 002 Q1 conversion warning codes | no | nothing provokes a conversion |
| 002 Q2 `SETSCALE` | no | no scaled column in the invoked table |
| 002 Q3 C `fixed` | no | not used |
| 002 Q4 charset mapping | no | **resolved for this slice's sets by Gate 8**; the cache carries a set Gate 8 already maps, and an unmapped one refuses there |
| 002 Q5 storage class | no | generated declarations carry none |
| 002 Q6 multiple declarators | no | one field per line, generated |
| **002 Q7 published charset ids** | **no, by construction** | the generated clause carries the *keyword*, not the id, so the `sqlh` values are not needed. Relevant to 007, not here |
| 003 Q1 outside `BEGIN WORK` | no | statements are wrapped |
| 003 Q2 nested `BEGIN WORK` | no | no nesting |
| 003 Q3 open lifecycle / 8204 | no | happy path plus the refusals |
| 003 Q4 connection scope | no | single-threaded fixtures |
| 003 Q5 configuration mechanism | no | settled by the implemented resolution order |
| 003 Q6 `DEFMODE` | no | directly-mapped table names |
| 006 Q1 column-to-field rule | no | **resolved** — lowercased column names |
| **006 Q2 `INVOKE` syntax** | **yes, partially** | `AS tag` is in scope; `PREFIX`/`SUFFIX` are out, so only the default `_I` is exercised. Q2 is *partially resolved* and this slice uses only the resolved part |
| 006 Q3 indicator naming | no | **resolved** — FR-006.5a..5d |
| **006 Q4 cache format and invalidation** | **yes — this is the slice's own question** | Narrowed by **SD-15** (format, committed) and **SD-16** (invalidation is not the preprocessor's job) |
| 006 Q5 protection views | no | the slice invokes a base table only |
| 006 Q6 MAP DEFINE on the structure tag | no | FR-006.2d is out of scope; `DIV-002` still has TACL DEFINEs unresolved |

## Scoped requirement set

**In:** FR-006.1, FR-006.2, FR-006.2a, FR-006.2b, FR-006.3, FR-006.4,
FR-006.5, FR-006.5b, FR-006.5c, FR-006.5d, FR-006.6, FR-006.8, NFR-006.2.

Carried and re-exercised — **and this is the point**: everything `INVOKE`
generates is then read by the parsers Gates 7 and 8 built, so those requirements
are the test of what this slice emits: FR-002.3, FR-002.4, FR-002.6, FR-002.8,
FR-002.9, FR-002.15, FR-002.16, FR-002.30, FR-003.1, FR-003.10, NFR-001.1,
NFR-001.2, NFR-002.2.

**Out:**

- **FR-006.2c (`NCHAR`, `NCHAR VARYING`)** — both mean the system default
  multibyte set, which §2 p.2-3 makes `KANJI`, which SD-14 refuses. Refused with
  a diagnostic naming that chain, exactly as Gate 8 refuses `NATIONAL
  CHARACTER`.
- **FR-006.2d (class MAP DEFINE for the object name)** — `DIV-002` still has
  TACL DEFINEs unresolved, and the manual's own examples use `=customer`, so
  this needs the DEFINE machinery first.
- **FR-006.5a (`PREFIX`/`SUFFIX`)** — 006 Q2 is only partially resolved. The
  default `_I` is exercised; the options are not.
- **FR-006.7 (schema recorded in the listing)** — the listing infrastructure is
  001's and still `spec`. FR-006.5d's in-source comment carries the same
  provenance in the meantime, which is why it is in scope and this is not.
- **NFR-006.1 (the whole App. A sample database)** — only the tables this slice
  invokes are built. A complete App. A transcription is a fixture project of its
  own.

## The ABI

**No new entry points and no signature change**, for the third slice running —
and this time for a structural reason rather than by luck. `INVOKE` generates
*declarations*. Those declarations are harvested into the same
`esqlc_hostvar_t` descriptors as hand-written ones, by the same parser, and
reach the runtime through `esqlc_stmt_exec` unchanged. The runtime cannot tell a
generated structure from a typed one, and that is the design working.

## What Gate 9 will not prove

- **Live schema access.** The preprocessor reads a cache and never a database.
  FR-006.2e's *"read access at preprocess time"* is satisfied against the cache,
  not against MariaDB, and the capture tool is out of scope — so nothing here
  proves a cache can be produced from a real schema.
- **A stale cache.** SD-16 says the preprocessor cannot detect one. A program
  compiled against a cache that no longer matches the database will generate
  wrong structures and compile cleanly, and only the recorded timestamp makes
  that diagnosable afterwards. This is the slice's largest exposure.
- **`NCHAR`, and multibyte generation.** Out on `KANJI`, like everything else
  that depends on it.
- **`PREFIX`/`SUFFIX`.** Only the default `_I` suffix is generated, so 006 Q2
  stays partially resolved.
- **Protection views.** 006 Q5 untouched; only a base table is invoked.
- **The listing.** FR-006.7 waits on 001's listing output. Provenance exists in
  the generated source and nowhere else.
- **The hand-written charset mismatch.** `INVOKE` makes generated declarations
  agree with their columns by construction, which does nothing for a
  hand-written declaration. `DIV-055`'s silent failure is narrowed, not closed.
