# Gate 10 — the `SQLDA`, via `PREPARE` / `DESCRIBE` / `EXECUTE`

**Slice of:** 001, 002, 003, 005, 007 · **Status:** ready to plan ·
**Predecessor:** Gate 9 (`INVOKE`) · **Phase 3**

The third and last SQL/MP structure. `SQLCA` at Gate 4, `SQLSA` at Gate 5, and
this completes the set — after which every structure a program can allocate has
a byte-exact generated layout.

It is also the first slice where the *program* owns the structure and the
runtime fills it in. The `SQLCA` and `SQLSA` are declared at a fixed size and
registered; an `SQLDA` is `malloc`ed at a size the program computes from a count
the runtime told it.

## Why now: 007 is far better resolved than it looks

Five of its eight open questions are resolved and a sixth provisionally, which
is what makes a first slice possible:

| | |
|---|---|
| Q1 field offsets, and whether 24 bytes is determined | **resolved** — §10 p.10-7 |
| Q2 the address fields cannot hold a 64-bit pointer | **resolved** — `DIV-040` |
| Q3 v1 and v2 layouts | **resolved** — FR-007.27a..27c |
| Q4 duplicated qualifier codes | **resolved** — emit the lower value |
| Q6 collation buffer format | **resolved** — §10 p.10-7 |
| Q8 legacy eye-catchers | **provisionally resolved** — Table D-1 |
| Q5 `WHENEVER` and dynamic statements | open — **avoided**, no `WHENEVER` here |
| Q7 `data_len = 0` to ignore scale | open — **avoided**, the idiom is unused |

And **002 Q7 is avoided by construction.** The `SQLDA`'s `precision` field
carries the character-set ID for `CHAR` and `VARCHAR` columns, and those IDs
live in the `sqlh` header this project does not have. A slice describing
**numeric columns only** never populates one, so the missing document does not
block it — the same avoidance Gate 8 used with single-byte fixtures.

## Two findings from the manual's own worked code

**The `SQLDA` is a one-element flexible array, allocated by the program.** §10
p.10-30:

```c
mem_reqd = sizeof(struct SQLDA_TYPE) + ((num_entries - 1) * sizeof(struct SQLVAR_TYPE));
sqlda_ptr = (sqldaptr) malloc(mem_reqd);
sqlda_ptr->num_entries = num_entries;
```

**Corrected at plan stage.** This document first said the structure *"declares
`sqlvar[1]`, not a fixed array"*. The count is in fact a **directive
parameter** — §10 p.10-3 gives `INCLUDE SQLDA ( sqlda-name [ , sqlvar-count ]
… )` and Example 10-1 declares `} sqlvar[sqlvar-count];`, a placeholder the
generator fills in. The program chooses.

A count of `1` is idiomatic, and p.10-29 calls it *"the SQLDA template"*,
precisely because p.10-30's arithmetic is then exact. A larger declared count
still *over*-allocates safely under that formula, so it is not fragile — only
exact at 1.

What would break it is a **C99 flexible member** (`sqlvar[]`): `sizeof` would
exclude the array and the formula would under-allocate by one entry, which is a
heap overflow appearing at a customer's column count and not at a fixture's.

**And that idiom is what makes `DIV-040` survivable.** `DIV-040` widens the
`sqlvar` from the published 24 bytes to 40 so it can hold real pointers. A
program using p.10-30's `sizeof` arithmetic allocates correctly at either width,
because it never mentions 24. A program that hard-codes 24 under-allocates and
corrupts the heap — which is exactly the population `DIV-040`'s migration note
should be addressing, and this slice is the first chance to check that it does.

**The program initialises more than the eye-catcher.** §10 p.10-59 shows
`strncpy(sqlda->eye_catcher, SQLDA_EYE_CATCHER, 2)`, `num_entries`, and — with
the comment *"ind_ptr must always be initialized, even when the program does not
handle null values"* — `ind_ptr` for every entry. FR-007.8 and FR-007.16 both
follow from that page, and the implementation must **never write** the fields
the program owns.

## The programs

**A — the layout, at Tier 1, with no server.** `INCLUDE SQLDA` generates
`struct SQLDA_TYPE` with `sqlvar[1]`, and static assertions pin every field:
`SQLDA_HEADER_LEN` 4, the widened 40-byte `sqlvar`, the four 16-bit fields, the
four address-width fields including `reserved`. NFR-007.3 in full, as Gate 5 did
for the `SQLSA`.

**B — the `malloc` arithmetic.** A fixture that allocates by p.10-30's formula
for three entries and asserts the result is large enough for three, proving the
one-element declaration rather than assuming it.

**C — `PREPARE` then `DESCRIBE`.** A `SELECT` of three numeric columns, prepared
from a host variable, then described. `num_entries` capacity honoured,
`data_type` per column, `data_len` and `precision` populated, `null_info`
negative for the nullable column.

**D — the sizes come from the `SQLSA`.** FR-007.23: `output_num` and
`output_names_len` from the `prepare` arm — **the arm Gate 5 emitted as layout
only and never populated.** This is where FR-005.18 finally fires.

**E — `EXECUTE` with bound output.** The program allocates buffers, sets
`var_ptr`, executes, and reads values back. The end-to-end proof that a
descriptor the runtime filled is one the program can use.

**F — the refusals.** `num_entries` too small (`ESQLC-7002`), `var_ptr` unset
(`ESQLC-7003`), an eye-catcher the program never initialised (`ESQLC-7010`,
a warning), and a `data_len` byte length that is not 2, 4 or 8 (`ESQLC-7005`).

## Exit criteria

1. `INCLUDE SQLDA` generates `struct SQLDA_TYPE` with `sqlvar[1]`, and
   p.10-30's `sizeof` arithmetic yields room for `n` entries.
2. Every field carries a `sizeof` or `offsetof` assertion — `SQLDA_HEADER_LEN`
   4, `sqlvar` 40 under `DIV-040`, `reserved` present and last.
3. `PREPARE` from a host variable associates a statement with a name.
4. `DESCRIBE` fills one `sqlvar` per output column, for numeric columns.
5. `data_type` matches the published `_SQLDT_*` value for each numeric type.
6. `data_len` packs byte length and scale as FR-007.11 specifies for binary
   numeric.
7. `precision` holds the numeric precision (FR-007.13).
8. `null_info` is negative exactly for the nullable column.
9. `EXECUTE` writes through `var_ptr` and the program reads the values back.
10. The `SQLSA`'s `prepare` arm is populated — `output_num` and
    `output_names_len` — which Gate 5 left as layout only.
11. The implementation never writes `eye_catcher` or `var_ptr` (FR-007.8).
12. All four refusals fire; Tier 1 green with no MariaDB; Gates 1–9 unregressed.

## Slice decisions

SD-2 and SD-10 carry forward. Two new, and both are bit-level readings the
manual states without disambiguating.

- **SD-2** — the program declares `long sqlcode;`. Narrows 005 Q8.
- **SD-10** — `CHAR_AS_STRING`'s extra byte. Narrows nothing; still 001 Q2.
- **SD-17 (new)** — in `data_len`, *"scale in bits 0:7 and byte length in bits
  8:15"* (FR-007.11) is read as **bits 0:7 being the low-order byte** — so
  `data_len = (bytes << 8) | scale`. Narrows nothing in the question list; the
  manual gives bit positions without saying which end is bit 0, and both
  readings produce a plausible 16-bit value. **Provisional.** A test pins both
  halves independently, so a later reversal is a visible change rather than a
  silent one, and `ESQLC-7005`'s "byte length not 2, 4 or 8" gives an
  independent check: under the wrong reading a scale-0 `INTEGER` would decode as
  byte length 0.
- **SD-18 (new)** — `precision` for a binary numeric column holds the column's
  **decimal** precision as the catalogue reports it: 5 for `SMALLINT`, 10 for
  `INTEGER`, 19 for `BIGINT`. Narrows nothing. FR-007.13 says *"numeric
  precision for binary numeric"* without saying whether that is decimal digits
  or bits, and SQL/MP's own type table maps `NUMERIC(1..4)` to 16 bits — which
  is a digit count driving a width, so digits is the consistent reading.
  **Provisional**, and worth re-checking against `SQLRM`.

## Design questions this slice must settle

- **Who allocates, and what the ABI carries.** The program does, per p.10-30, so
  every entry point takes the `SQLDA` pointer and its `num_entries`. The runtime
  validates capacity against what `PREPARE` reported (`ESQLC-7002`) and never
  reallocates.
- **What "never writes `var_ptr`" means for `DESCRIBE`.** `DESCRIBE` fills
  `data_type`, `data_len`, `precision` and `null_info`; the program then
  allocates and sets `var_ptr` before `EXECUTE`. So `DESCRIBE` must leave
  `var_ptr` exactly as it found it, including when the program left it NULL —
  and `EXECUTE` refuses a NULL one rather than allocating helpfully.
- **How a prepared statement is named.** By name, as cursors are (Gate 3), for
  the same reason: the name is already the source language's identifier and a
  runtime table keyed by it needs no state in generated code.
- **Whether the widened `sqlvar` is visible as 40.** Yes — `SQLDA_SQLVAR_LEN` is
  40, per FR-007.6, which already says *"not the published 24"*. A program using
  `sizeof` is unaffected; one using the literal is `DIV-040`'s migration case.

## Open-question avoidance

Every open question in the five specs this slice touches.

| Question | Touched? | Why not |
|---|---|---|
| 001 Q1 declaration vs executable position | no | `INCLUDE SQLDA` is declaration position; the statements are executable |
| **001 Q2 `#pragma SQL` option set** | **yes** | `CHAR_AS_STRING` still governs generated character fields. Carried **SD-10** |
| 001 Q3 `SQL SOURCE` | no | not used |
| 001 Q4 C label prefix | no | not used |
| 002 Q1 conversion warning codes | no | no conversion is provoked |
| 002 Q2 `SETSCALE` | no | scale is 0 throughout; no `DECIMAL` |
| 002 Q3 C `fixed` | no | not used |
| 002 Q4 charset mapping | no | resolved for Gate 8's sets, and no character column is described here |
| 002 Q5 storage class | no | plain declarations |
| 002 Q6 multiple declarators | no | one per declaration |
| **002 Q7 published charset ids** | **no, by construction** | `precision` carries a charset ID for `CHAR`/`VARCHAR` only. **Numeric columns only**, so the `sqlh` values are never needed. FR-007.20 and FR-007.21 are out of scope for exactly this reason |
| 003 Q1 outside `BEGIN WORK` | no | statements are wrapped |
| 003 Q2 nested `BEGIN WORK` | no | no nesting |
| 003 Q3 open lifecycle / 8204 | no | happy path plus the refusals |
| 003 Q4 connection scope | no | single-threaded fixtures |
| 003 Q5 configuration mechanism | no | settled by the implemented resolution order |
| 003 Q6 `DEFMODE` | no | directly-mapped table names |
| 004 Q1–Q9 | no | **no cursors at all.** Dynamic cursors are FR-007.5, out of scope |
| 005 Q1 `SQLCA` layout | no | resolved — `DIV-041` |
| 005 Q2 `SQLSA` offsets | no | resolved by Gate 5 |
| 005 Q3 `SQLSA` sentinels | no | the `prepare` arm's fields are populated, not sentinelled |
| 005 Q4 conversion warning codes | no | see 002 Q1 |
| **005 Q5 `WHENEVER` and dynamic SQL** | **no, by construction** | shared with 007 Q5. No `WHENEVER` directive appears in any fixture, so the question is untouched rather than narrowed |
| 005 Q6 SQL message file | no | no rendering |
| 005 Q7 item-22 sign inversion | no | `SQLCAGETINFOLIST` unchanged |
| **005 Q8 who declares `sqlcode`** | **yes** | the fixtures reference it. Carried **SD-2** |
| 005 Q9 `WHENEVER` and transaction control | no | `sqlcode` checked directly |
| 005 Q10 `CALL` handler signature | no | no `CALL` |
| 007 Q1 `SQLDA` field offsets | no | **resolved** — the premise of this slice |
| 007 Q2 address fields vs 64-bit pointers | no | **resolved** — `DIV-040` |
| 007 Q3 v1 and v2 layouts | no | resolved, and **out of scope**: only version 300+ is generated |
| 007 Q4 duplicated qualifier codes | no | resolved, and unreachable — no date-time column |
| **007 Q5 `WHENEVER` and dynamic statements** | **no, by construction** | see 005 Q5 |
| 007 Q6 collation buffer format | no | resolved, and out of scope — `cprl_ptr` is untouched |
| **007 Q7 `data_len = 0` to ignore scale** | **no, by construction** | the idiom is never used; every fixture sets a real byte length. Named in the non-proof list |
| 007 Q8 legacy eye-catchers | no | provisionally resolved, and out of scope with v1/v2 |

## Scoped requirement set

**In:** FR-007.1, FR-007.2 *(`EXECUTE` only)*, FR-007.3 *(`DESCRIBE` only)*,
FR-007.6, FR-007.6a, FR-007.6b, FR-007.7, FR-007.7a, FR-007.8, FR-007.9,
**FR-007.11a** *(the binary-numeric packing — see the split note below)*,
FR-007.13, FR-007.14, FR-007.15,
FR-007.18 *(the numeric codes only)*, FR-007.22, FR-007.23, FR-007.26,
NFR-007.3.

Carried and re-exercised: FR-002.9, FR-003.1, FR-003.2, FR-003.3, **FR-005.18**
*(the `SQLSA`'s `prepare` arm, layout-only since Gate 5)*, NFR-001.1,
NFR-002.2.

**The FR-007.11 split.** The requirement covers two packings in one sentence —
*"decimal and binary numeric"* — and only one is buildable here, because
`DECIMAL` has no implementation to describe. It is referred to as **FR-007.11a**
(binary numeric, in scope) and **FR-007.11b** (decimal, out) so the two halves
can be tracked separately, exactly as 002's spec splits FR-002.21 from
FR-002.21a. **Feature 007's spec should carry that split**; recorded here and
raised at plan stage rather than left as one requirement that is half done.

No other requirement appears in both lists.

**Out:**

- **FR-007.20, FR-007.21** — character-set IDs and the execution-time check.
  Both need `sqlh` (002 Q7), and both are avoided by describing numeric columns
  only.
- **FR-007.10, FR-007.12, FR-007.19** — `data_len` for character and date-time,
  and the qualifier codes. Character needs the charset ID; date-time needs
  `TYPE AS`, which Gate 7 left out.
- **FR-007.11b, the decimal half** — `DECIMAL` is unimplemented (002 Q2/Q3).
- **FR-007.4 `RELEASE`, FR-007.5 dynamic cursors, FR-007.24 `DESCRIBE INPUT`,
  FR-007.25 `CAST`** — each a slice of its own.
- **FR-007.16** — the invalid-`ind_ptr` exemption. Deliberately reading an
  address the program declared invalid is a quirk worth reproducing carefully
  and not casually.
- **FR-007.17** — `cprl_ptr` and collation. CPRL is Phase 4 and the roadmap's
  standing risk.
- **FR-007.7b** — the ILP32 mode that preserves the published 24-byte layout.
  `DIV-040`'s other half.
- **FR-007.27, .27a–.27d** — version 1 and 2 layouts. Only 300+ here.
- **NFR-007.1, NFR-007.2** — a round-trip per `data_type` and a unit test per
  packing rule. In scope only for the numeric codes and the binary-numeric
  packing this slice implements; stated so neither is read as satisfied whole.



## The ABI grows, for the first time since Gate 3

Three new entry points. Gates 4–9 added one apiece or none; a prepared statement
is long-lived state addressed by name across three statements, which is the same
shape that forced the cursor entry points.

```c
/* Compile a statement held in a host variable and associate it with a name.
   The text is the program's, carried verbatim (NFR-001.1). */
int esqlc_prepare(const char *name, const char *sql, size_t sql_len);

/* Fill one sqlvar per output column. `sqlda` is the PROGRAM's storage,
   allocated per §10 p.10-30, and `num_entries` its capacity — checked against
   what PREPARE reported (ESQLC-7002) and never reallocated.

   Writes data_type, data_len, precision and null_info. Leaves eye_catcher,
   var_ptr and ind_ptr exactly as found: FR-007.8 makes those the program's,
   and §10 p.10-59 has the program initialise ind_ptr even when it handles no
   nulls. */
int esqlc_describe(const char *name, void *sqlda, int num_entries, int version);

/* Run a prepared statement, binding through the descriptor's var_ptr fields.
   Refuses a NULL var_ptr (ESQLC-7003) rather than allocating helpfully. */
int esqlc_execute(const char *name, void *sqlda, int num_entries);
```

`esqlc_stmt_exec` is untouched: a dynamic statement's text is not known at
compile time, so it cannot carry the descriptor array or the table landmark
that entry point takes.

## What Gate 10 will not prove

- **Any character column in a descriptor.** The whole charset-ID half of the
  `SQLDA` — FR-007.20, .21, and `data_len` for character types — is out, on
  002 Q7's missing `sqlh`. This is the largest single omission and it is the
  same document that has blocked nothing else so far.
- **`DECIMAL` and date-time entries.** Their `data_len` packings and the 28
  qualifier codes are untouched.
- **The ILP32 24-byte layout.** `DIV-040` has two halves and this slice does
  one; a program compiled for the published layout is unproven.
- **Version 1 and 2 descriptors.** Resolved on paper (Q3, Q8) and not built.
- **Dynamic cursors, `RELEASE`, `DESCRIBE INPUT`, `CAST`.** Four separate
  slices.
- **`WHENEVER` over a dynamic statement.** 005 Q5 and 007 Q5 are the same
  question and this slice avoids both rather than answering either.
- **`data_len = 0`'s scale-ignoring idiom.** 007 Q7, unused here.
- **The collation buffer.** Sized by a published formula this slice does not
  compute, because `cprl_ptr` is out.
