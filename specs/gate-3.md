# Gate 3: read-only cursors

**Status:** Tasked · **Plan:** [gate-3-plan.md](gate-3-plan.md) ·
**Tasks:** [gate-3-tasks.md](gate-3-tasks.md)
**Specs:** 001, 002, 003, 004, 005 (scoped)
**Depends on:** Gate 2 (merged, green, CI-enforced) · **Blocks:** positioned operations, then Phase 3

Gate 1 proved data reaches MariaDB faithfully. Gate 2 proved one row comes back.
Gate 3 proves a *set* comes back, one row at a time, and stops correctly.

Deliberately **read-only**: `DECLARE CURSOR`, `OPEN`, a `FETCH` loop, `CLOSE`.
No `FOR UPDATE`, no positioned `UPDATE` or `DELETE`, no cursor stability. Those
are the hardest parts of §4 and they wait for Gate 4.

## Two questions this slice cannot avoid

Unlike Gates 1 and 2, which dodged every open question but two narrow corners,
Gate 3 collides with 004 head-on. Stating that plainly up front, because it
changes what the slice is for.

**004 Q9 must be *fixed*, not avoided.** The dispatch table's `DECLARE CURSOR`
entry is dead code: real syntax is `DECLARE <cursor-name> CURSOR FOR …`, with the
name *between* the two words, so the multi-word matcher never fires. Gate 2 found
this and deferred it because cursor syntax belongs to 004. Gate 3's entry point
*is* that statement, so the fix lands here. This is a defect repair, not a slice
decision.

**004 Q6 is structurally unavoidable.** A cursor loop terminates by fetching past
the last row. Table 4-2 does not say where the cursor sits afterwards, and
FR-004.14 currently asserts an answer the manual never gives. Narrowed by slice
decision **SD-3** below.

## The programs

Against the Gate 1/2 `parts` schema, seeded with several rows.

**`cursor_loop.sqlc`** — the shape every real program uses.

```c
#pragma SQL
#include <stdio.h>
#include <string.h>

long sqlcode;

EXEC SQL BEGIN DECLARE SECTION;
  short min_num;
  short part_num;
  char  part_desc[19];
EXEC SQL END DECLARE SECTION;

EXEC SQL DECLARE partcur CURSOR FOR
         SELECT part_num, part_desc
           FROM parts
          WHERE part_num >= :min_num
          ORDER BY part_num;

int main(void)
{
  int rows = 0;
  min_num = 4000;

  EXEC SQL BEGIN WORK;
  EXEC SQL OPEN partcur;

  for (;;) {
    memset(part_desc, 0xAA, sizeof part_desc);   /* poison before each fetch */
    EXEC SQL FETCH partcur INTO :part_num, :part_desc;
    if (sqlcode == 100) break;
    if (sqlcode != 0) { long s = sqlcode; EXEC SQL CLOSE partcur;
                        EXEC SQL ROLLBACK WORK;
                        fprintf(stderr, "fetch failed %ld\\n", s); return 1; }
    printf("%d|%.18s|%02X\\n", part_num, part_desc,
           (unsigned char)part_desc[18]);
    ++rows;
  }

  EXEC SQL CLOSE partcur;
  EXEC SQL COMMIT WORK;
  printf("rows=%d\\n", rows);
  return 0;
}
```

`ORDER BY` is not decoration: FR-004.16b makes row order **undefined** without
it, so a conformance test that asserts an order without one is asserting
unspecified behaviour.

**`fetch_exhausted.sqlc`** — fetches past the end twice, to pin SD-3.
**`negative/fetch_unopened.sqlc`** — `FETCH` before `OPEN`.
**`negative/double_open.sqlc`** — `OPEN` on an already-open cursor.
**`negative/close_unopened.sqlc`** — `CLOSE` on a cursor never opened.
**`negative/undeclared_cursor.sqlc`** — `OPEN` of a name never declared.

## Exit criteria

1. The loop returns every matching row exactly once, in `ORDER BY` order, and
   `rows` equals the seeded count.
2. `sqlcode` is 100 on the fetch past the last row, and the loop terminates.
3. **Host variables are untouched by the terminating fetch** — the poison
   sentinel survives it, mirroring Gate 2's not-found check (FR-004.14).
4. **No null terminator is appended** on any fetch: byte 18 stays `AA`
   (FR-002.28, on the cursor path this time).
5. A second `FETCH` after exhaustion also returns 100 and still writes nothing
   (SD-3).
6. `CLOSE` succeeds, and a `FETCH` after it is an error rather than a stale row.
7. All five cursor diagnostics fire at the correct code, line and column.
8. `DECLARE partcur CURSOR FOR …` is recognised — the Q9 fix — and a cursor verb
   with no handler still reaches `ESQLC-1012`.
9. Tier 1 green with no MariaDB present; diagnostic registry clean.

## Slice decisions

SD-1 and SD-2 carry forward from Gates 1 and 2 unchanged, still **provisional**.
One new decision.

- **SD-1** — `UNKNOWN` single-byte charset binds as the connection's default
  character set. Narrows 002 Q4 only.
- **SD-2** — the program declares `long sqlcode;`. Narrows 005 Q8 only.
- **SD-3 (new)** — after a `FETCH` returns 100, the cursor remains at end of set:
  further fetches also return 100 and write nothing. Narrows 004 Q6 only.
  Chosen because it is the only reading under which the common `for(;;)` loop is
  safe against an accidental extra fetch, and because the alternative — an
  undefined position — cannot be implemented at all. **Provisional**: the manual
  does not say, and `SQLRM` may.

## Open-question avoidance

Every open question in the five specs this slice touches.

| Question | Touched? | Why not |
|---|---|---|
| 001 Q1–Q4 | no | all carry decisions; 001 is `Ready` |
| 002 Q1 warning codes | no | host variables sized to their columns exactly |
| 002 Q2 `SETSCALE` | no | no scaled column |
| 002 Q3 C `fixed` | no | not used |
| **002 Q4 charset mapping** | **yes** | unchanged since Gate 1. Carried decision **SD-1** |
| 002 Q5 storage class | no | plain declarations |
| 002 Q6 declarators | no | one per statement |
| 003 Q1 outside `BEGIN WORK` | no | the whole cursor lifetime is inside a transaction |
| 003 Q2 nested `BEGIN WORK` | no | no nesting |
| 003 Q3 open lifecycle / 8204 | no | happy path only |
| 003 Q6 `DEFMODE` | no | directly-mapped table name |
| 004 Q2 multi-row single-row `SELECT` | no | no single-row `SELECT` in this slice at all |
| 004 Q3 cursor stability | no | one connection, read-only, no concurrent writer. Nothing observes an isolation difference |
| 004 Q4 `CLOSE` inside vs outside a transaction | no | `CLOSE` always precedes `COMMIT WORK`, so the interaction with FR-003.8 is never reached |
| 004 Q5 cursor scope, unit or function | no | one cursor, declared once at file scope, used in one function. The distinction is not exercised |
| **004 Q6 position after fetching past the last row** | **yes, unavoidably** | loop termination requires it. Narrowed by **SD-3** |
| 004 Q7 position after a positioned `UPDATE` | no | read-only slice, no positioned operations |
| 004 Q8 cursor PAID and `IN EXCLUSIVE MODE` | no | no PAID modelling; no `IN EXCLUSIVE MODE` |
| **004 Q9 `DECLARE … CURSOR` dispatch defect** | **yes — fixed, not narrowed** | the slice's entry point. A defect repair, closing the question |
| 005 Q3 `SQLSA` sentinels | no | no `SQLSA` |
| 005 Q4 warning codes | no | as 002 Q1 |
| 005 Q5 `WHENEVER` and dynamic SQL | no | no `WHENEVER` |
| 005 Q6 SQL message file | no | no message rendering |
| 005 Q7 item-22 sign inversion | no | no `SQLCA` |
| **005 Q8 who declares `sqlcode`** | **yes** | the fixtures reference it. Carried decision **SD-2** |

## Design questions this slice must settle

**1. A cursor's SQL is captured at one construct and executed at another.**
`DECLARE partcur CURSOR FOR SELECT …` is in declaration position and contains the
statement text; `OPEN partcur` is in executable position and is where that text
must actually run. Nothing so far has needed cross-construct state — Gates 1 and
2 emitted each statement where it stood. The preprocessor now has to carry the
declared text, and its host-variable references, from the `DECLARE` site to the
`OPEN` site.

That also means **`:min_num` in the cursor's `WHERE` clause is bound at `OPEN`,
not at `DECLARE`** — the manual is explicit that `OPEN` runs the statement
(FR-004.12). A program may legitimately change `min_num` between the declaration
and the open.

**2. `FETCH` sends no SQL at all.** Unlike every statement so far, `FETCH … INTO`
is an instruction to the runtime, not text for the server. This is the first
construct whose body is *entirely* binding metadata.

**3. Does the `INTO` landmark survive `FETCH … INTO`?** — Gate 2's T285 deferral,
answered here. In `FETCH partcur INTO :a, :b` there is no `FROM`, so the output
region runs from `INTO` to the end of the body, which the existing landmark logic
already handles (`from_off` unset). The cursor name carries no colon, so it is
not mistaken for a reference. The mechanism extends unchanged — but a test must
pin that rather than the plan assuming it.

## Scoped requirement set

**001:** FR-001.15, FR-001.16, FR-001.25; NFR-001.1
**002:** FR-002.28
**003:** FR-003.1, FR-003.2, FR-003.3, FR-003.8
**004:** FR-004.11, FR-004.12, FR-004.13, FR-004.14, FR-004.15, FR-004.16, FR-004.16b, FR-004.19
**005:** FR-005.1

Plus the 004 Q9 defect fix, and diagnostics `ESQLC-4001`, `ESQLC-4002`,
`ESQLC-4003`, `ESQLC-4005`, `ESQLC-4006`.

Deliberately absent: FR-004.16a and FR-004.17/.18 (positioned operations),
FR-004.20 (cursor stability), all of `WHENEVER`, all of
`SQLCA`/`SQLSA`/`SQLDA`, `INVOKE`, and every conversion warning.

## This is where the ABI grows

Gates 1 and 2 added no entry points — Gate 2 because `direction` and `ind_addr`
were already in the descriptor. Cursors have no such luck: a cursor is
long-lived state spanning three statements, which the current one-shot
`esqlc_stmt_exec` cannot express.

Gate 3's plan must add cursor entry points to the ABI and land them in the 003
contract in the same change (Principle V). This is the growth Gate 2's plan
predicted, arriving where predicted.

## What Gate 3 will not prove

- Nothing about positioned `UPDATE` or `DELETE`, which is where `DIV-051` and
  004 Q7 live, and where the manual is at its vaguest.
- Nothing about cursor stability or any concurrency at all — one connection, one
  reader, no writer.
- Nothing about the structures. `SQLCA` still has no published layout, `SQLSA`'s
  offsets remain inferred, `SQLDA_SQLVAR_LEN` still awaits `DIV-040`. Three gates
  will have dodged the project's largest correctness exposure.
- Nothing about `WHENEVER`, so no generated control flow.
- Nothing about multiple simultaneous cursors, or a cursor surviving a commit.

A green Gate 3 means a program can walk a result set correctly. The parts of §4
that most need `SQLRM` remain untouched.
