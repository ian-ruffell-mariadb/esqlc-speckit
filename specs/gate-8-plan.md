# Gate 8 plan — character sets

**Slice:** [specs/gate-8.md](gate-8.md) · **Specs:** 001, 002, 003, 004, 005 ·
**Planned under Principle VIII** (002, 004, 005 are `Clarifying`)

Slice conditions verified: enumerated subset (2 in-scope plus 15 carried),
avoidance table covering all 35 open questions, five provisional decisions, and
a specific non-proof section.

## 1. Approach

**Bind bytes verbatim by making the client character set `binary`, carry the
declared set in the descriptor as a project-internal id, and check it against
the column on retrieval.**

The slice was scoped as "add the `CHARACTER SET` clause". Checking the protocol
turned it into something more consequential.

**`MYSQL_BIND` has no character-set field.** A per-parameter charset is not
expressible in the MariaDB client protocol at all — parameter bytes are
interpreted using the connection's `character_set_client` and transcoded to the
column's set by the server. Two host variables in one statement carrying
different sets cannot be expressed by any per-bind mechanism, because none
exists.

**And the current default silently corrupts.** The runtime never sets a client
charset, so it inherits `latin1`. Measured on the server:

| path | result |
|---|---|
| `latin1 → euckr` | `B0A1B0A2` → `A1C6A2AEA1C63F` |
| `binary → euckr` | `B0A1B0A2` → `B0A1B0A2` |

Four bytes become seven, ending in `3F` — the `?` MariaDB substitutes for
characters it cannot map. **Gates 1 through 7 claim byte-verbatim binding
(FR-002.30) and only achieve it by accident**: every fixture to date is ASCII,
where `latin1` transcoding is the identity. The claim has never been tested
against a byte above 0x7F.

So the central decision is not about charsets at all. It is that the connection
must use `character_set_client = binary`, which makes FR-002.30 and FR-002.28
true by construction rather than true for ASCII. The host variable's declared
set then means what the manual says it means — a statement about what the bytes
*are* — and the column's own set defines what the column holds. When they
agree, bytes pass through untouched.

**The declared set earns its keep on retrieval.** §10 p.10-11 says NonStop
SQL/MP *"checks the precision field to ensure that the character-set ID matches
the expected character set of the parameter or column"*. That check is real
SQL/MP behaviour, and result metadata carries `charsetnr`, so the runtime can
perform it where it matters most — reading bytes into a host variable declared
for a different set is exactly how mojibake enters a program silently.

**The ids are ours, and that is a recorded debt.** §10 p.10-6 puts the
character-set ID in the SQLDA's `precision` field, and p.10-11 says the values
come from *"declarations in the `sqlh` file"* — an external header this project
does not have. The numeric ids are therefore **not derivable from the manual**.
This slice uses a project-internal numbering, which is sufficient because
nothing outside the runtime sees it yet; feature 007 will need the published
values for the SQLDA, so `sqlh` joins `SQLRM` and `CPG` as an external
dependency. Raised as 002 Q7.

## 2. Alternatives rejected

**Leave the client charset alone and map the host variable's set per-bind.**
Rejected because it is impossible: the protocol has no such field. This was the
plan's original assumption and the header disproved it.

**Issue `SET NAMES <set>` around each statement.** Expressible, and rejected:
it is connection-wide, so it cannot serve two host variables with different sets
in one statement, and it makes every statement three round trips.

**Use `utf8mb4` as the client charset and let the server transcode.** This is
the modern-application answer and it is wrong here. Transcoding is precisely
what FR-002.30 forbids — the program's bytes must arrive unaltered — and
`utf8mb4 → euckr` corrupts identically to `latin1 → euckr` (measured above).

**Bind as `MYSQL_TYPE_BLOB` to force binary treatment per parameter.**
Rejected: it changes the column's type expectations and would defeat the
server's own length and charset validation, trading a narrow problem for a
broader one.

**Publish our internal charset ids in the ABI header.** Rejected while they are
provisional. `esqlc_hostvar_t.charset` is already declared; its *values* stay
private to the runtime until `sqlh` settles them, so 007 is not forced to
inherit a guess.

## 3. Components

| Component | Path | Change | Slice scope |
|-----------|------|--------|-------------|
| Charset table (pp) | `src/pp/charset.cc` | **new** — keyword → internal id, and mapped / unmapped / unspecified | the 12 keywords |
| Declaration parser | `src/pp/decl.cc` | the infix `CHARACTER SET [IS] cs` clause | `char` arrays and `VARCHAR` structures |
| Shared types | `src/pp/pp.h` | `HostVar::charset` | — |
| Emitter | `src/pp/emit.cc` | emit `charset` in the descriptor; refuse unmapped and unspecified sets | — |
| Charset table (rt) | `src/rt/charset.c` | **new** — internal id → MariaDB charset name and number | — |
| Runtime: context | `src/rt/context.c` | `character_set_client = binary` | **the corruption fix** |
| Runtime: exec | `src/rt/exec.c` | retrieval-side charset check against `charsetnr` | outputs only |
| Table sync guard | `tests/harness/charset_sync.sh` | **new** — the two tables must agree | both directions |
| Schema / harness | `tests/conformance/gate-1/` | charset columns, seed rows, Gate 8 cases | — |

Nine components, three new files.

**Stubs that must fail loudly.** `KANJI` is refused with a diagnostic saying the
encoding is *unspecified*; `ISO88593`/`4`/`5`/`6` are refused as *unmapped*.
The two messages differ because the conditions differ — one is a gap in the
manual, the other a gap in MariaDB, and collapsing them would tell a user to go
looking in the wrong place. `NATIONAL CHARACTER` and `NATIONAL CHARACTER
VARYING` refuse naming their dependency on `KANJI`.

## 4. Runtime ABI surface

**No new entry points and no signature change**, for the second slice running.
`esqlc_hostvar_t.charset` has been declared since Gate 1 as *"SQLDA charset id;
0 = UNKNOWN"* and has been `0` in every descriptor ever emitted — verified, not
assumed. This slice is what it was reserved for.

The field's **meaning** is stated rather than changed: `0` remains `UNKNOWN`,
i.e. use the connection default, so every descriptor Gates 1–7 emit stays
valid without one being rewritten. What changes is that non-zero values now
occur.

The contract needs one amendment: the comment calls it the *SQLDA* charset id,
which it is not yet — those values are external (`sqlh`, 002 Q7). The field
carries a project-internal id until then, and the contract must say so rather
than imply conformance it does not have.

## 5. Data structures

No new structure layouts, so Principle VI adds no `sizeof`/`offsetof`
obligation beyond what Gates 5 and 7 already assert. `esqlc_hostvar_t` is
untouched, and its `sizeof <= 40` and `offsetof(addr) == 0` assertions continue
to hold — deliberately, since a per-variable charset was one of the things that
40-byte budget was reserved for.

The one new shared thing is a **table, not a layout**: the keyword→id mapping
exists in `src/pp/charset.cc` and the id→MariaDB mapping in `src/rt/charset.c`.
That is the same drift hazard the `SQLSA` had, so it gets the same treatment —
`charset_sync.sh` compares them in both directions, and a keyword present on one
side only fails the build rather than binding as something plausible.

## 6. Requirement → component map

| Requirement | Component(s) | Test |
|-------------|--------------|------|
| NFR-001.1 opaque bodies | emit | `opaque_body_unchanged` |
| FR-002.3 `CHAR(l)` → `char v[l+1]` | decl | `charset_clause` |
| FR-002.4 the clause carries into the declaration | decl, pp/charset | `charset_clause`, `charset_keywords` |
| FR-002.6 `VARCHAR` structure | decl | `charset_varchar` |
| FR-002.8 the recognised keywords | pp/charset | `charset_keywords`, `negative/charset_*` |
| FR-002.15 indicator association | emit | `charset_varchar` |
| FR-002.16 negative indicator means null | rt/exec | `rt/charset_null` |
| FR-002.22 conversion within families | rt/exec | `rt/charset_family` |
| FR-002.28 no terminator on retrieval | rt/exec | `rt/charset_roundtrip_1byte` |
| FR-002.30 `width` bytes bound verbatim | rt/context, rt/exec | `rt/charset_high_bytes` |
| FR-002.31 padding is the program's job | rt/exec | `underfilled_stores_null` (carried) |
| FR-003.1 `esqlc_*` calls only | emit | `abi_only_symbols` |
| FR-003.2 no MariaDB type in the header | include/esqlc.h | `abi_isolation` |
| FR-003.3 signatures mirrored in the contract | contract | `contract_sync` |
| FR-003.10 values bound, never interpolated | emit | `charset_clause` |
| NFR-002.1 a round-trip per mapping row | schema, harness | `rt/charset_roundtrip_1byte`, `rt/charset_roundtrip_2byte` |
| NFR-002.2 widths asserted statically | emit | `charset_varchar` |

**17 requirements, all mapped exactly once. Zero unmapped.**

`NFR-002.1` is mapped but partial by design, as in Gate 7: a round-trip exists
for each *mappable* set, and the refused sets have none because they have no
implementation.

## 7. Test strategy

**Tier 1.** The clause, the keyword classification, and the refusals — all
without a server. The infix position is the interesting parse: Gate 7's
`VARCHAR` shape check is positional, so `struct { short len; char CHARACTER SET
KSC5601 val[11]; }` must still match the shape with three extra tokens in the
middle.

**Tier 2, and this is where the slice earns its place.** Two fixtures carry it:

`charset_high_bytes` inserts a byte above 0x7F and reads it back, comparing
hex. **This fails against the code as it stands today** — not because the
charset clause is missing, but because `latin1` transcoding alters the bytes.
It is the regression test for a defect that has been latent since Gate 1.

`charset_roundtrip_2byte` stores two EUC-KR syllables in a `KSC5601 VARCHAR`
and asserts `len == 4`. Measured on the server: `length()` is 4 and
`char_length()` is 2, so the fixture distinguishes bytes from characters and no
other test in the project can.

**Mutation, Phase D′.** Revert the client charset to `latin1` and
`charset_high_bytes` must fail; map `KSC5601` to `sjis` and the 2-byte
round-trip must fail; accept `KANJI` and its negative must fail; drop a keyword
from either table and `charset_sync` must fail; skip the retrieval check and
`charset_family` must fail.

The first is the one that matters, and it is a mutation *back to the current
behaviour* — which is the clearest possible statement of what this slice fixes.

## 8. Risks

**`character_set_client = binary` changes every statement, not just the ones
with a charset clause.** SQL text is then binary too, so a string literal a
program writes inside a statement is a binary literal compared against a
character column. MariaDB coerces, but collation of such a comparison follows
the column, and Gate 6's `update_injection_literal` fixture contains exactly
such a literal. All seven previous gates' Tier 2 fixtures must pass unchanged,
and if any does not, that is information about this decision rather than a test
to adjust.

**The internal charset ids will collide with the published ones.** §10 p.10-6
puts the real ids in the SQLDA's `precision` field, and they live in the `sqlh`
header this project does not have. Feature 007 cannot use our numbering, so
either the ids become a translation layer or they get renumbered when `sqlh`
arrives. Raised as 002 Q7 rather than left to be discovered by 007.

**`ISO88591` is not ISO 8859-1.** `latin1` is cp1252: bytes round-trip, but
comparison, sorting and case folding across 0x80–0x9F follow Windows-1252. No
test can prove that away, and a program relying on 8859-1 collation in that
range will behave differently here. It is the one mapping in the table that is
an approximation rather than an equivalence.

**Refusing `KANJI` removes a market rather than a feature.** Japanese is
plausibly the largest single user base for NonStop SQL/MP, and this slice
declines to serve it rather than guess between four encodings. That is correct
under Constitution III and it is also the most consequential refusal the project
has made; it should be visible to whoever plans Phase 3, not buried in a
divergence entry.

**The retrieval check can produce a false refusal.** A column declared in
MariaDB with a charset that happens to differ from the host variable's declared
set — because the schema was created outside this project — would now be
refused where it previously worked by accident. That is the intended behaviour
under Principle III, but it will read as a regression to anyone whose schema was
built without the mapping table in mind.

## 9. Divergences introduced

**One new: `DIV-055` — character-set mapping, and byte-verbatim binding.**
Covers the whole topic as one divergence because the facets are inseparable:
the mapping table; `ISO88591` resolving to cp1252; four of the nine ISO 8859
sets having no counterpart; `KANJI` refused as unspecified; and the client
charset being `binary` so that the declared set describes bytes rather than
directing a conversion.

Its **Detection** field is the useful part: an out-of-scope set is refused at
compile time with a diagnostic naming which kind of gap it is, and a byte above
0x7F now round-trips unaltered where it previously did not.

**Also recorded there, because it is the same decision seen from the other
side:** FR-002.30's byte-verbatim guarantee was, before this slice, true only
for ASCII. That is a correction to what Gates 1–7 claimed, not a new limitation.

**002 Q7 is new**, not a divergence: the published character-set ids are in the
`sqlh` header, which is a third external dependency alongside `SQLRM` and `CPG`.
Nothing in this slice needs it; feature 007 does.
