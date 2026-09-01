# Gate 5 — the SQLSA

**Slice of:** 001, 002, 003, 004, 005 · **Status:** ready to plan ·
**Predecessor:** Gate 4 (`WHENEVER` and the `SQLCA`)

The last SQL/MP structure, and the one every previous gate's non-proof section
named as the project's largest remaining exposure.

Gate 4 did half the structures work: it proved a program can allocate a
diagnostic area at the published size, share it, and read it back through
accessors. The `SQLCA` was the easy half — `DIV-041` makes its layout private,
because the manual never publishes one, so nothing constrained the choice
beyond the 430-byte total.

The `SQLSA` is the hard half. Its layout **is** published, twice, in §9's
worked examples, so Principle VI applies with no escape: field for field, byte
for byte, for two version families whose counters differ in width.

## Why this is now scopeable

005 Q2 — `SQLSA` field offsets — is **resolved**, and the spec's original
assessment that the structures could not be built byte-exactly from the manual
alone was simply wrong here. §9 pp.9-15..9-16 publish both declarations in
full. The arithmetic confirms them independently under packed alignment:

| | v300–325 | v330+ |
|---|---|---|
| eye-catcher + version | 4 | 4 |
| `num_tables` | 2 | 2 |
| timing fields + filler | — | 24 + 32 |
| one `stats[]` entry | 24 + 20 + 4 + 4 = **52** | 24 + 40 + 8 + 4 + 32 = **108** |
| `stats[16]` | 832 | 1728 |
| **total** | **838** ✓ | **1790** ✓ |

Both hit `SQLSA_LEN` exactly, and only under packed alignment — the natural
alignment of a `long long` after a `short` would push v330 past 1790. That
arithmetic is the proof that the inferred layout is the real one, and it is why
FR-005.27's alignment pragma is load-bearing rather than decorative.

Nothing here needs `SQLRM`. That is what makes this the right slice now:
positioned operations, cursor stability, and the conversion warnings all remain
blocked on a document that has not arrived, and the `SQLSA` does not.

## The programs

Four fixtures, in increasing order of what they can silently get wrong.

**A — layout, at Tier 1, with no server.** One unit generates both layouts and
static-asserts them: `sizeof` 838 and 1790, eye-catcher `SA`, the `dml` and
`prepare` arms overlapping at the same offset, and the VSBB flags present at
v330 where v300 has `sqlsa_reserved`. This is the whole of Principle VI for
this structure, and it costs nothing to run.

**B — statistics after a cursor loop.** A `DECLARE CURSOR`/`OPEN`/`FETCH`/`CLOSE`
loop over the fixture table, reading `SQLSA` after each `FETCH` and accumulating
into program variables — the idiom §9 p.9-13 explicitly prescribes, and the
only one that can detect a missing reset. If `SQLSA` were not reset per
statement, the accumulator over-counts, and the test fails loudly rather than
subtly.

**C — mapped values versus sentinels.** The `DIV-011` proof, and the fixture
most likely to rot silently. A two-table join, so `num_tables` is 2 and
`stats[1]` is exercised rather than only `stats[0]`: `table_name` and
`records_used` must carry real values, while `messages`, `message_bytes`, and
`escalations` — which have no MariaDB analogue at all — must carry the sentinel
and **never** zero. Zero is a legitimate statistic; that is the entire reason
FR-005.25 exists.

**D — undefined after a statement that leaves it undefined.** `COMMIT WORK`,
then read `SQLSA`. §9 says the structure is undefined; FR-005.19 additionally
forbids making it *accidentally* meaningful, which a runtime that simply leaves
the previous statement's values in place would do.

## Exit criteria

1. Both layouts generate and static-assert at 838 and 1790, packed, with
   eye-catcher `SA`.
2. `dml` and `prepare` occupy the same offset — a union, not two members.
3. VSBB flags exist at v330 and `sqlsa_reserved` occupies that slot at v300.
4. `SQLSA` is populated after `OPEN`, `FETCH`, and `CLOSE` on a `SELECT` cursor.
5. **Every `FETCH` resets it**, proven by the accumulator idiom rather than by
   inspecting one field.
6. `num_tables` is 2 for a two-table join, and `stats[1]` is populated.
7. `table_name` matches the queried table.
8. Every field with no MariaDB analogue carries its documented sentinel, and no
   such field is ever zero.
9. Reading `SQLSA` after `COMMIT WORK` yields sentinels throughout, not the
   previous statement's values.
10. Tier 1 green with no MariaDB present; diagnostic registry clean; citation
    harness green.

## Slice decisions

SD-1, SD-2, SD-3 carry forward, still **provisional**. One new decision.

- **SD-1** — `UNKNOWN` single-byte charset binds as the connection default.
  Narrows 002 Q4.
- **SD-2** — the program declares `long sqlcode;`. Narrows 005 Q8.
- **SD-3** — a `FETCH` past the last row returns 100 again and writes nothing.
  Narrows 004 Q6.
- **SD-7 (new)** — an unmappable numeric `SQLSA` field carries **-1 in its own
  declared width**. Narrows 005 Q3 only. Chosen because the field domain is
  counts, all of which are non-negative, so -1 is outside it at every width and
  needs no per-width table after all — the question anticipated one because the
  counters widen 32→64-bit, but a sentinel that is out-of-domain at 32 bits
  stays out-of-domain at 64. It also matches the one sentinel convention the
  manual does publish, `-1` for true in `vsbb_write`. **Provisional** — the
  manual states no sentinel for these fields, and `SQLRM` may name one.

## Design questions this slice must settle

Not open questions against the manual — decisions the implementation cannot
avoid making.

- **How "undefined" is represented.** Chosen: after any statement class that
  leaves `SQLSA` undefined, the runtime stamps every field with its sentinel.
  The alternative — leave the previous values — is what FR-005.19 forbids, and
  is the failure mode where a program reads plausible statistics belonging to
  the wrong statement.
- **Whether one unit may generate both layouts.** Yes, and Program A requires
  it: the two declarations have distinct type names (`SQLSA_TYPE` and
  `SQLSA_TYPE_R330`), so nothing collides, and asserting both in one
  translation unit is what makes the Tier 1 layout test possible at all.
- **Where `table_name` comes from.** The statement's own table list as the
  preprocessor captured it, not a server round-trip. A round-trip would make
  every statistic read cost a query.
- **Registration carries the version.** Two layouts mean the runtime cannot
  infer which one it was handed from the pointer alone.

## Open-question avoidance

Every open question in the five specs this slice touches.

| Question | Touched? | Why not |
|---|---|---|
| 001 Q1–Q4 | no | all carry decisions; 001 is `Ready` |
| 002 Q1 warning codes | no | see 005 Q4 |
| 002 Q2 `SETSCALE` | no | no scaled column |
| 002 Q3 C `fixed` | no | not used |
| **002 Q4 charset mapping** | **yes** | `char` arrays with no `CHARACTER SET`. Carried decision **SD-1** |
| 002 Q5 storage class | no | plain declarations |
| 002 Q6 declarators | no | one per statement |
| 003 Q1 outside `BEGIN WORK` | no | statements are wrapped |
| 003 Q2 nested `BEGIN WORK` | no | no nesting |
| 003 Q3 open lifecycle / 8204 | no | happy path only |
| 003 Q4 connection scope | no | single-threaded fixtures |
| 003 Q5 configuration mechanism | no | settled by the resolution order already implemented |
| 003 Q6 `DEFMODE` | no | directly-mapped table names |
| 004 Q1 position table contents | no | resolved by Gate 3 |
| 004 Q2 multi-row single-row `SELECT` | no | retrieval is by cursor |
| 004 Q3 cursor stability | no | one session, read-only, no concurrent writer. **This is the question that blocks positioned operations and it stays blocked** |
| 004 Q4 `CLOSE` inside vs outside a transaction | no | `CLOSE` is inside |
| **004 Q5 cursor scope** | **yes, unavoidably** | one named cursor in one unit. Carried from Gate 3's ABI note, which records that a name-keyed runtime table assumes unit scope |
| **004 Q6 position after exhaustion** | **yes** | the loop terminates by fetching past the last row. Carried decision **SD-3** |
| 004 Q7 position after positioned `UPDATE` | no | no positioned operations |
| 004 Q8 cursor PAID | no | no authorisation model in the fixtures |
| 004 Q9 `DECLARE CURSOR` dispatch | no | fixed by Gate 3 |
| 005 Q1 `SQLCA` layout | no | resolved — `DIV-041` |
| 005 Q2 `SQLSA` offsets | no | **resolved** — the premise of this slice |
| **005 Q3 `SQLSA` sentinels** | **yes — this is the slice's core** | Program C exists to exercise it. Narrowed by **SD-7** |
| 005 Q4 conversion warning codes | no | host variables are sized to their columns |
| 005 Q5 `WHENEVER` and dynamic SQL | no | no dynamic SQL |
| **005 Q6 SQL message file** | **no, by exclusion** | `SQLSADISPLAY` renders statistics as text and is deliberately out of scope |
| 005 Q7 item-22 sign inversion | no | `SQLCAGETINFOLIST` is unchanged from Gate 4; **SD-4** stands |
| **005 Q8 who declares `sqlcode`** | **yes** | the fixtures reference it. Carried decision **SD-2** |
| 005 Q9 `WHENEVER` and transaction control | no | this slice checks `sqlcode` directly; no `WHENEVER` |
| 005 Q10 `CALL` handler signature | no | no `CALL` |

## Scoped requirement set

Enumerated, not described.

**In:** FR-005.16, FR-005.17, FR-005.19, FR-005.20, FR-005.21, FR-005.21a,
FR-005.21b, FR-005.21c, FR-005.22 *(partially — `num_tables` up to 2, not the
cap)*, FR-005.23, FR-005.25, FR-005.27, NFR-005.1.

Carried and re-exercised: FR-004.11, FR-004.12, FR-004.13, FR-004.14,
FR-004.15, FR-005.8, FR-005.9, FR-005.12.

**Out:** FR-005.18 (dynamic SQL population — feature 007 does not exist yet),
FR-005.24 (`sql_statement_type` — only meaningful on the `prepare` arm),
FR-005.26 (`SQLSA VERSION CURRENT` — needs `SQLGETSYSTEMVERSION`, a Guardian
procedure with no analogue), FR-005.28/.29/.32 (message and statistics
rendering — 005 Q6), FR-005.30, FR-005.31.

**Version scope:** both layouts are *generated and asserted*; only **v300** is
*populated at runtime*. Populating v330 as well would double the runtime work
to exercise the same code path at a different integer width, and SD-7 is
deliberately width-independent so that nothing about the sentinel policy
depends on which family runs. Named in the non-proof list.

## The ABI grows

One entry point, following Gate 4's registration pattern for the same reason —
programs copy the structure and share it `EXTERNAL`, so the data must live in
the program's own storage, not the runtime's.

```c
/* Register the program's SQLSA. `version` is 300 or 330; `len` must equal that
   version's SQLSA_LEN. Two published layouts mean the runtime cannot infer
   which one it was handed. */
int esqlc_sqlsa_register(void *sqlsa, size_t len, int version);
```

`ESQLC-5009` — reading `SQLSA` after a statement class that leaves it undefined
— stays **unimplemented**. Detecting a *read* requires dataflow over the host
program, which is a preprocessor capability this project does not have and
should not grow for one warning. The runtime stamping decided above achieves
the same protection at the point it actually matters.

## What Gate 5 will not prove

- **v330 at runtime.** Its layout is asserted; its population is not. The
  64-bit counter path is untested.
- **The 16-table cap.** `num_tables` reaches 2. FR-005.22's cap and the
  behaviour of a statement touching more than 16 tables are untouched.
- **`records_accessed` and `disc_reads` fidelity.** Whether these can be
  mapped from MariaDB at all is unsettled; if they cannot, they become
  sentinels and `DIV-011` grows. The slice proves the sentinel *mechanism*, not
  which side of the line each field lands on.
- **Dynamic SQL population.** The entire `prepare` arm is asserted as a layout
  and never populated. `sql_statement_type` is never exercised.
- **Statistics rendering.** `SQLSADISPLAY` is out, so no statistic is ever
  turned into text.
- **Cursor stability.** Still blocked on 004 Q3 and `SQLRM`. Nothing here moves
  positioned operations closer.
- **Concurrency.** Every statistic is measured single-session. Whether the
  numbers remain meaningful under a concurrent writer is not examined.
