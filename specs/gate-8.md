# Gate 8 — character sets

**Slice of:** 001, 002, 003, 004, 005 · **Status:** ready to plan ·
**Predecessor:** Gate 7 (host variable type breadth)

The first slice that closes a gap in code already shipped rather than adding
capability. Gate 7 put `VARCHAR` and `char` binding into `main` while explicitly
not knowing whether `len` counts bytes or characters, and avoided the question by
making every fixture single-byte. That avoidance is honest and it is also a
latent wrongness: the `len` handling now in the tree is unproven for exactly the
programs — Japanese, Korean — where it decides whether data survives.

002 Q4 is also the one open question left that needs no external document. The
spec says *"unresolved — likely a new divergence"*, not *"needs `SQLRM`"*.

## Two findings before scoping

**`len` counts bytes, and the manual derives it rather than stating it.** §2
p.2-20 declares `type_varchar2 VARCHAR (10) CHARACTER SET KANJI` and §2 p.2-22
shows `INVOKE` generating `struct { short len; char CHARACTER SET KANJI
val[11]; }` for it. `val` is 11 bytes for a `VARCHAR(10)`, which is FR-002.6's
`val[l+1]` with `l = 10`. If `VARCHAR(10)` meant ten *characters* in a
double-byte set, `val` would need 21. So `VARCHAR(n)` is n **bytes**, a
double-byte set simply encodes fewer characters in them, and `len` is in the
same units as the array it describes.

That **resolves** the sub-question rather than narrowing it, and it comes from
the manual's own arithmetic rather than from a decision.

**The clause sits between the type and the name.** Not after it:

```c
char CHARACTER SET ISO88591 type_picx1[11];
struct { short len; char CHARACTER SET KANJI val[11]; } emp_name;
```

`CHARACTER SET [IS] charset` (p.2-24), in uppercase, with the keywords
`ISO8859n` (n = 1..9), `KANJI`, `KSC5601`, `UNKNOWN`. The declaration parser
Gate 7 grew must now accept a three-or-four-token interruption in the one place
it currently expects an identifier.

## What maps, and what does not

Checked against the server rather than assumed:

| SQL/MP | MariaDB | Note |
|---|---|---|
| `ISO88591` | `latin1` | **`latin1` is cp1252, not ISO 8859-1** — they differ across 0x80–0x9F |
| `ISO88592` | `latin2` | exact |
| `ISO88597` | `greek` | exact |
| `ISO88598` | `hebrew` | exact |
| `ISO88599` | `latin5` | exact |
| `ISO88593`, `ISO88594`, `ISO88595`, `ISO88596` | — | **no counterpart at all** |
| `KSC5601` | `euckr` | KS C 5601 is the character set; EUC-KR is its encoding |
| `KANJI` | ? | four candidates, and the manual names no encoding |
| `UNKNOWN` | connection default | the manual calls it "an unknown single-byte character set" |

**Four of the nine ISO 8859 sets have no MariaDB counterpart**, and `latin1` is
an approximation rather than a mapping. Both are divergences, not gaps in the
work.

**`KANJI` is refused, not guessed.** MariaDB offers `sjis`, `cp932`, `ujis` and
`eucjpms` — differing in maximum byte length (2, 2, 3, 3) *and* in repertoire.
"KANJI" names a script, not an encoding, and the manual never says which. A
wrong choice does not fail; it silently stores different characters than the
program wrote, which is the least detectable class of error this project can
produce. Refusing with a diagnostic that names the reason is the only faithful
option, and it is what Constitution III requires.

`KSC5601` is the multibyte set this slice *does* implement, and it is why the
slice can settle the `len` question at all: it is the one double-byte mapping
defensible from first principles, because KS C 5601 and EUC-KR are the standard
and its encoding rather than two guesses at the same script.

## The programs

**A — the clause parses.** Tier 1: `CHARACTER SET`, `CHARACTER SET IS`, on a
`char` array and inside a `VARCHAR` structure, with the charset reaching the
descriptor's `charset` field — declared in Gate 1 and never once non-zero.

**B — every keyword classified.** Tier 1: all nine `ISO8859n`, `KANJI`,
`KSC5601`, `UNKNOWN`, and a bad keyword. Five map, four are refused as
unmapped, `KANJI` is refused as unspecified, `UNKNOWN` is the connection
default, and a typo is `ESQLC-2006`.

**C — a single-byte round trip.** `ISO88592` end to end, proving the charset
reaches the column rather than being parsed and dropped.

**D — the multibyte round trip, and the point of the gate.** A `KSC5601`
`VARCHAR` holding characters that are two bytes each. `len` must come back in
**bytes**, and the fixture asserts a value that is only correct in bytes — a
`len` in characters would be half of it and no other test in the project could
tell.

**E — byte-verbatim across a charset.** FR-002.30 still holds: `width` bytes
leave the program unaltered even when the column's charset differs from the
connection's. This is where a runtime that re-encodes rather than binds would
show.

**F — the refusals.** `KANJI`, `ISO88593`, and a misspelled keyword, each
diagnosed with its own reason rather than one vague message.

## Exit criteria

1. `CHARACTER SET` and `CHARACTER SET IS` parse in both declaration positions.
2. The charset reaches the descriptor; `charset` is no longer always 0.
3. The five mappable `ISO8859n` sets bind to their counterparts.
4. `ISO88593`/`4`/`5`/`6` are refused as unmapped, naming the set.
5. `KANJI` is refused as *unspecified*, with a different diagnostic from the
   unmapped case — the two conditions are not the same and must not read alike.
6. `KSC5601` round-trips a two-byte-per-character value.
7. **`len` is in bytes**, proven by a value that is wrong if it is characters.
8. `width` bytes bind verbatim regardless of charset.
9. An unrecognised keyword is `ESQLC-2006`.
10. Tier 1 green with no MariaDB; registry, contract, citation and
    `sqlsa_layout_sync` clean; Gates 1–7 unregressed.

## Slice decisions

- **SD-1 — RESOLVED (2026-09-03), no longer carried.** `UNKNOWN` binds as the connection
  default. p.2-24 calls it *"an unknown single-byte character set"* and
  *"equivalent to omitting the CHARACTER SET clause"*, so the connection default
  is the faithful reading rather than a convenience. **This closes the decision
  seven gates have carried.**
- **SD-2** — the program declares `long sqlcode;`. Narrows 005 Q8. Unchanged.
- **SD-12 (new)** — `ISO8859n` maps to the MariaDB counterpart where one
  exists, and is **refused where none does**. Narrows 002 Q4. Not provisional in
  its mapping half — `latin2`/`greek`/`hebrew`/`latin5` are the same standards —
  but **provisional for `ISO88591`**, which maps to `latin1`, i.e. cp1252. Bytes
  round-trip; comparison and case folding across 0x80–0x9F do not.
- **SD-13 (new)** — `KSC5601` maps to `euckr`. Narrows 002 Q4.
  **Provisional**, though better grounded than any other multibyte choice: KS C
  5601 is the character set and EUC-KR its encoding, so this is one step rather
  than a guess.
- **SD-14 (new)** — `KANJI` is **refused**, with a diagnostic stating that the
  encoding is unspecified. Narrows 002 Q4 by explicitly declining to narrow it.
  **Provisional**: `SQLRM`, a NonStop system, or a customer's data would settle
  which encoding is meant, and until then a wrong mapping silently corrupts
  where a refusal merely blocks.

**Not a decision — derived.** `len` counts bytes, from the `VARCHAR(10)` →
`val[11]` arithmetic above. Recorded as a finding against 002 rather than as a
slice decision, because nothing was chosen.

## Design questions this slice must settle

- **Where the clause is recognised.** In the declaration parser, between the
  type keyword and the name. It is the first *infix* construct that parser has
  met, and Gate 7's `VARCHAR` shape-matching is positional, so the shape check
  has to tolerate the interruption rather than be defeated by it.
- **How the charset reaches the column.** Per-statement, not per-connection: two
  host variables in one statement may carry different sets, so a connection-wide
  `SET NAMES` cannot express it. The bind carries it, which is what
  `esqlc_hostvar_t.charset` has been reserved for since Gate 1.
- **What `charset` 0 means now.** It has meant "UNKNOWN, use the connection
  default" implicitly since Gate 1. With real values arriving it must mean that
  explicitly, and every existing descriptor keeps emitting 0 unchanged.
- **Whether a charset changes the family check.** It must not. FR-002.22's
  character/numeric split is about families, and every charset is in the
  character family — a `KSC5601` column into a `char` host variable is not a
  cross-family conversion.

## Open-question avoidance

Every open question in the five specs this slice touches.

| Question | Touched? | Why not |
|---|---|---|
| 001 Q1 declaration vs executable position | no | all declarations are in a declare section |
| **001 Q2 `#pragma SQL` option set** | **yes** | `CHAR_AS_STRING` still decides the extra `val` byte. Carried **SD-10** from Gate 7, unchanged |
| 001 Q3 `SQL SOURCE` | no | not used |
| 001 Q4 C label prefix | no | not used |
| **002 Q1 conversion warning codes** | **no, by construction** | no fixture provokes a truncation. A charset change is not a conversion here: bytes bind verbatim |
| 002 Q2 `SETSCALE` | no | no scaled column |
| 002 Q3 C `fixed` | no | not used |
| **002 Q4 charset mapping** | **yes — this is the slice** | Narrowed by **SD-12**, **SD-13**, **SD-14**, and SD-1 is resolved. The `len` sub-question is *derived*, not narrowed |
| 002 Q5 storage class | no | plain declarations |
| 002 Q6 multiple declarators | no | one declarator per declaration |
| 003 Q1 outside `BEGIN WORK` | no | statements are wrapped |
| 003 Q2 nested `BEGIN WORK` | no | no nesting |
| 003 Q3 open lifecycle / 8204 | no | happy path plus the refusals |
| 003 Q4 connection scope | no | single-threaded fixtures |
| 003 Q5 configuration mechanism | no | settled by the implemented resolution order |
| 003 Q6 `DEFMODE` | no | directly-mapped table names |
| 004 Q1 position table | no | resolved by Gate 3 |
| 004 Q2 multi-row single-row `SELECT` | no | verification reads are by primary key |
| 004 Q3 cursor stability | no | no cursors |
| 004 Q4 `CLOSE` inside vs outside a transaction | no | no cursors |
| 004 Q5 cursor scope | no | no cursors |
| 004 Q6 position after exhaustion | no | no cursors |
| 004 Q7 position after positioned `UPDATE` | no | no positioned operations |
| 004 Q8 cursor PAID | no | no cursors |
| 004 Q9 `DECLARE CURSOR` dispatch | no | fixed by Gate 3 |
| 005 Q1 `SQLCA` layout | no | resolved — `DIV-041` |
| 005 Q2 `SQLSA` offsets | no | resolved by Gate 5 |
| 005 Q3 `SQLSA` sentinels | no | no field this slice populates differs from Gate 6 |
| 005 Q4 conversion warning codes | no | shared with 002 Q1, same avoidance |
| 005 Q5 `WHENEVER` and dynamic SQL | no | no dynamic SQL |
| 005 Q6 SQL message file | no | no rendering |
| 005 Q7 item-22 sign inversion | no | `SQLCAGETINFOLIST` unchanged |
| **005 Q8 who declares `sqlcode`** | **yes** | the fixtures reference it. Carried **SD-2** |
| 005 Q9 `WHENEVER` and transaction control | no | `sqlcode` checked directly |
| 005 Q10 `CALL` handler signature | no | no `CALL` |

## Scoped requirement set

**In:** FR-002.4, FR-002.8.

Carried and re-exercised *with a character set*, which is the point — these
have all been proven single-byte only: FR-002.3, FR-002.6, FR-002.15,
FR-002.16, FR-002.22, FR-002.28, FR-002.30, FR-002.31, FR-003.1, FR-003.2,
FR-003.3, FR-003.10, NFR-001.1, NFR-002.1, NFR-002.2.

**Out:** FR-002.5 and FR-002.7 (`NATIONAL CHARACTER` and `NATIONAL CHARACTER
VARYING`). Both mean *the system default multibyte set*, which p.2-3 says is
`KANJI` unless changed at system generation. SD-14 refuses `KANJI`, so these
refuse with it — and "system generation" has no analogue to consult even if it
did not. They are refused with a diagnostic naming the dependency, not silently
mapped to something plausible.

Also out: everything Gate 7 left out — `DECIMAL`, `SETSCALE`, C `fixed`,
`TYPE AS`, `INTERVAL`, `CHAR_AS_ARRAY`, and the four conversion warnings.

## The ABI

**No new entry points and no signature change**, for the second slice running.
`esqlc_hostvar_t.charset` has been declared since Gate 1 as *"SQLDA charset id;
0 = UNKNOWN"* and has been 0 in every descriptor ever emitted. This slice is
what it was reserved for.

The field's meaning is stated rather than changed: 0 remains `UNKNOWN`, i.e. use
the connection default, and non-zero identifies a set the runtime maps to a
MariaDB charset. That keeps every descriptor Gates 1–7 emit valid without
rewriting one of them.

## What Gate 8 will not prove

- **`KANJI`, and therefore Japanese.** Refused by SD-14. The largest single
  market for a NonStop SQL/MP program is the one this slice declines to guess
  at, and that is the honest position rather than a comfortable one.
- **Four of the nine ISO 8859 sets.** Refused as unmapped. A program using
  8859-5 Cyrillic will not compile.
- **`ISO88591` fidelity across 0x80–0x9F.** Bytes round-trip; comparison, sorting
  and case folding in that range follow cp1252, not ISO 8859-1. No fixture can
  prove the difference away because it is real.
- **`NATIONAL CHARACTER`.** Out, on `KANJI`.
- **Collation.** Every mapping here is a *character set*. SQL/MP collations, and
  the whole of §11's CPRL, are untouched — and CPRL remains the standing risk
  the roadmap records for Phase 4.
- **Conversion warnings.** Still `DIV-042`. A charset change binds bytes
  verbatim rather than converting, so nothing here fires a warning, and
  `WHENEVER SQLWARNING` stays structurally verified only — as since Gate 4.
