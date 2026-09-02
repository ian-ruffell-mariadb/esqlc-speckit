# Gate 7 — host variable type breadth

**Slice of:** 001, 002, 003, 004, 005 · **Status:** ready to plan ·
**Predecessor:** Gate 6 (searched `UPDATE` and `DELETE`)

Six gates in, the type system is `char[]` and 16-bit `short`. A customer program
declaring `int`, `long long`, `float` or a `VARCHAR` structure — which is most
of them — still cannot compile. This is the gate that changes that, and it is
the largest remaining thing that needs no `SQLRM`.

Statements have had six gates of attention and types have had one. The
imbalance is why `docs/traceability.md` shows both type-mapping rows as
`partial` with the same note: *16-bit only*.

## What is actually blocked, and what is not

The type table splits cleanly along the open questions, which is what makes
this scopeable at all.

| Family | Status |
|---|---|
| Integer widths 16/32/64 | **available** — FR-002.9 |
| `float` / `double` | **available** — FR-002.10 |
| `VARCHAR` structures | **available** — FR-002.6, .20, .21 |
| Date-time as `char` | **available** — FR-002.13 |
| `unsigned long long` refusal | **available** — FR-002.12 |
| Character sets | blocked — 002 Q4 |
| `DECIMAL`, `SETSCALE`, C `fixed` | blocked — 002 Q2, Q3 |
| Conversion warnings | blocked — 002 Q1, `DIV-042` |
| `TYPE AS`, `INTERVAL` | out — needs the date-time declaration surface |
| `CHAR_AS_ARRAY` | out — feature 006 |

Three of the five available families cost almost nothing at the runtime, and
that is worth saying plainly: `esqlc_hostvar_t` has carried `width` since Gate
1, `exec.c` already binds widths 2, 4 and 8, and `ESQLC_T_CHAR_VAR` and
`ESQLC_T_FLOAT` have been declared and unused since the same gate. **The work
is almost entirely in the declaration parser**, which is the one component no
previous gate has had to grow.

## Two findings before implementation

**FR-002.6 states the manual's conclusion without its condition.** The
requirement says `VARCHAR(l)` maps to a structure with `short len` and
`char val[l+1]`. §2 p.2-9 says the extra byte exists *"if the SQL pragma
specifies the `CHAR_AS_STRING` option"* — so `val[l+1]` is one of two shapes,
not the shape. FR-002.6 should carry the condition.

It also matters less than it looks, because the same page settles the case this
slice actually covers: *"If you explicitly declare a structure as a host
variable for a `VARCHAR` column (rather than using `INVOKE`), declare the length
as a `short`."* A hand-declared structure's `val` size is whatever the program
wrote, so the preprocessor reads it rather than deriving it. `val[l+1]` is a
statement about what `INVOKE` **generates**, which belongs to feature 006.

**`len` in bytes or in characters is not answerable from the manual**, which
says only that it *"represents the length"* (p.2-9). This slice **avoids the
question by construction** rather than narrowing it: every fixture uses a
single-byte character set, where the two are the same number. Named in the
non-proof section, because a multibyte set makes it load-bearing and that
arrives with 002 Q4.

## The programs

**A — the integer widths.** A table with `SMALLINT`, `INTEGER` and `BIGINT`
columns, round-tripped through `short`, `int` and `long long` host variables.
Round-trip, not insert-only: a width that is wrong on the way out can look
right on the way in.

**B — the boundary.** A 32-bit value inserted into a `SMALLINT` column. This is
FR-002.25's error 8300, and it is the negative that makes the widening
meaningful — without it, "wider integers work" is untested against the case
where they must not.

**C — `float` and `double`.** `REAL` and `DOUBLE PRECISION` columns, round-tripped.
Compared with a tolerance, because exact float equality across a wire format
would be testing the wrong thing.

**D — a `VARCHAR` structure.** `struct { short len; char val[N]; }` declared by
hand in a declare section, used as `:v`, inserted and retrieved, with `len`
carrying the length in both directions.

**E — the `VARCHAR` refusals.** A length field declared `int` rather than
`short` (FR-002.21), and a non-`VARCHAR` structure used as a host variable
(FR-002.20). Both must be diagnosed, not bound wrongly.

**F — date-time as character.** A `TIMESTAMP` column retrieved into a `char`
array. The column type is the interesting half; the host variable is an
ordinary character array, which is why this needs no `TYPE AS`.

**G — `unsigned long long` refused.** FR-002.12, and the natural ceiling of the
widening work.

## Exit criteria

1. `short`, `int` and `long long` host variables round-trip against
   `SMALLINT`, `INTEGER` and `BIGINT`.
2. Each carries its true width in the descriptor — 2, 4, 8 — asserted
   statically, not inferred from the C type name (`DIV-001`).
3. A value too large for its column yields error 8300, not a silent truncation.
4. `float` and `double` round-trip against `REAL` and `DOUBLE PRECISION`.
5. A hand-declared `VARCHAR` structure binds as one host variable, with `len`
   correct on input and written on output.
6. A `VARCHAR` length field declared `int` is refused.
7. A structure that is not a `VARCHAR` shape is refused when used as a host
   variable.
8. A `TIMESTAMP` column retrieves into a `char` array with no terminator
   appended (FR-002.28 still holds at the new column type).
9. `unsigned long long` is refused as a host variable.
10. Tier 1 green with no MariaDB present; registry, contract, citation and
    `sqlsa_layout_sync` harnesses clean; Gate 1–6 fixtures unregressed.

## Slice decisions

SD-1, SD-2 carry forward, still **provisional**. SD-7, SD-8, SD-9 are not
touched — no `SQLSA` field this slice populates differs from Gate 6's. Two new.

- **SD-1** — `UNKNOWN` single-byte charset binds as the connection default.
  Narrows 002 Q4. **Load-bearing here in a way it has not been before**: it is
  what makes `len` in bytes indistinguishable from `len` in characters.
- **SD-2** — the program declares `long sqlcode;`. Narrows 005 Q8.
- **SD-10 (new)** — for a hand-declared `VARCHAR` structure, `capacity` is the
  declared size of `val` and `width` is `capacity - 1`, mirroring FR-002.3's
  treatment of `char v[l+1]`. Narrows nothing in the open-question list; it
  settles the FR-002.6 / p.2-9 conflict above for the hand-declared case.
  **Provisional** — it assumes the `CHAR_AS_STRING` shape, and a program
  compiled `CHAR_AS_ARRAY` would declare `val[l]` and be off by one. The pragma
  option set is 001 Q2, so this cannot be resolved here; a test pins the
  assumption so a later change is visible.
- **SD-11 (new)** — MariaDB's out-of-range condition maps to SQL error 8300.
  Narrows nothing; FR-002.25 names the code and the mapping has to be chosen.
  **Provisional** — the manual pairs 8300 with a file-system detail of 1031,
  which has no analogue, so the detail is a `DIV-011`-style sentinel while the
  code itself is faithful.

## Design questions this slice must settle

- **How the declaration parser recognises a `VARCHAR` structure.** It is the
  first structured declaration the parser has met. A `struct { short len; char
  val[N]; }` is recognised by shape — two members, those names, those types —
  and anything else is refused under FR-002.20 rather than guessed at.
- **Whether `:v.val` is a host variable.** p.2-9 says both the structure name
  and the individual items are usable. The slice covers `:v`; whether
  `:v.len` and `:v.val` are separately bindable is a question the parser must
  answer one way and the fixtures must pin.
- **Where integer width comes from.** From `sizeof` of the declared C type, not
  from its spelling. `DIV-001` already records that `long` is 32-bit on NonStop
  and 64-bit on LP64, so the type name is not the answer and this is the gate
  where that stops being theoretical.
- **What `len` means on output.** The runtime writes the retrieved length; the
  program's prior value is not consulted. The alternative — treating `len` as a
  capacity limit on output — is a different requirement and not this one.

## Open-question avoidance

Every open question in the five specs this slice touches.

| Question | Touched? | Why not |
|---|---|---|
| 001 Q1 declaration vs executable position | no | all declarations are in a declare section |
| **001 Q2 `#pragma SQL` option set** | **yes, unavoidably** | `CHAR_AS_STRING` decides whether `val` carries the extra byte. Narrowed by **SD-10**, which assumes the `CHAR_AS_STRING` shape |
| 001 Q3 `SQL SOURCE` | no | not used |
| 001 Q4 C label prefix | no | not used |
| **002 Q1 conversion warning codes** | **no, by construction** | no fixture provokes a truncation or a precision loss. FR-002.23/.24/.26/.27 are all out of scope for exactly this reason |
| 002 Q2 `SETSCALE` | no | no scaled column, no `DECIMAL` |
| 002 Q3 C `fixed` | no | not used |
| **002 Q4 charset mapping** | **yes** | every character host variable. Carried **SD-1**, and it is what keeps `len`'s units unobservable |
| 002 Q5 storage class | no | plain declarations, no `static` or `extern` |
| 002 Q6 multiple declarators | no | one declarator per declaration |
| 003 Q1 outside `BEGIN WORK` | no | statements are wrapped |
| 003 Q2 nested `BEGIN WORK` | no | no nesting |
| 003 Q3 open lifecycle / 8204 | no | happy path plus the 8300 boundary |
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
| 005 Q4 conversion warning codes | no | see 002 Q1 — shared question, same avoidance |
| 005 Q5 `WHENEVER` and dynamic SQL | no | no dynamic SQL |
| 005 Q6 SQL message file | no | no rendering |
| 005 Q7 item-22 sign inversion | no | `SQLCAGETINFOLIST` unchanged |
| **005 Q8 who declares `sqlcode`** | **yes** | the fixtures reference it. Carried **SD-2** |
| 005 Q9 `WHENEVER` and transaction control | no | `sqlcode` is checked directly |
| 005 Q10 `CALL` handler signature | no | no `CALL` |

## Scoped requirement set

**In:** FR-002.6, FR-002.9, FR-002.10, FR-002.12, FR-002.13, FR-002.20,
FR-002.21, FR-002.25.

Carried and re-exercised: FR-002.1, FR-002.2, FR-002.3, FR-002.15, FR-002.16,
FR-002.22, FR-002.28, FR-002.30, FR-002.31, FR-003.1, FR-003.2, FR-003.3,
FR-003.10, NFR-001.1, NFR-002.1, NFR-002.2, NFR-003.2.

**Out:** FR-002.4, .5, .7, .8 (character sets — 002 Q4); FR-002.11, .18, .19
(`DECIMAL`, `SETSCALE`, C `fixed` — 002 Q2/Q3); FR-002.14, .17 (`INTERVAL` and
`TYPE AS` — both need a date-time *declaration* surface, which the in-scope
retrieval case does not); FR-002.23, .24, .26, .27 (conversion warnings — 002
Q1, `DIV-042`); FR-002.29 (`CHAR_AS_ARRAY` — feature 006).

No requirement appears in both lists.

**NFR-002.1 is in scope but only for the rows this slice adds.** It asks for a
round-trip test per mapping row, and the rows still out of scope have no test
because they have no implementation. Stated so the requirement is not read as
satisfied whole.

## The ABI

**No new entry points, and no signature change.** This is the first slice since
Gate 2 to add nothing: `esqlc_hostvar_t` has carried `width`, `capacity`,
`is_signed` and the type-family constants since Gate 1, specifically so that
widening the type system would not move the interface.

Three constants stop being decorative:

- `ESQLC_T_CHAR_VAR` (3) — declared Gate 1, first used here
- `ESQLC_T_FLOAT` (5) — likewise
- `ESQLC_T_DATETIME` (6) — **not** used here; FR-002.13 binds a date-time
  *column* into a *character* host variable, so the descriptor stays
  `ESQLC_T_CHAR_FIXED`. The constant waits for `TYPE AS`.

That last distinction is worth pinning in a test, because binding a `TIMESTAMP`
column with `ESQLC_T_DATETIME` would be the obvious wrong move.

## What Gate 7 will not prove

- **Character sets.** Every fixture is single-byte. `KANJI`, `KSC5601` and the
  ISO 8859 sets are untouched, and 002 Q4 is unmoved.
- **`len` in characters.** Indistinguishable from bytes under a single-byte
  set, so the slice proves the field works without proving what it counts. A
  multibyte set makes that load-bearing.
- **`DECIMAL` and fixed-point.** The largest remaining type family, and the one
  where SQL/MP's `decimal` array has no MariaDB counterpart at all.
- **Conversion warnings.** Four requirements the manual mandates and
  `DIV-042` blocks. Nothing here fires a warning of any kind, so the
  `WHENEVER SQLWARNING` path remains structurally verified only — as it has
  been since Gate 4.
- **`CHAR_AS_ARRAY`.** SD-10 assumes the `CHAR_AS_STRING` shape. A program
  compiled the other way declares `val[l]` and would be off by one, which the
  slice pins as an assumption rather than resolves.
- **`TYPE AS` and `INTERVAL`.** The date-time *declaration* surface, as opposed
  to retrieving a date-time column into a character array.
- **Timestamps on the write path.** FR-002.13 is exercised on retrieval.
  Inserting a date-time value from a character host variable is not proved, and
  Gate 1's `INSERT` row in the traceability table still says *no timestamps*.
