# Gate 4: WHENEVER and the SQLCA

**Status:** Planned · **Plan:** [gate-4-plan.md](gate-4-plan.md)
**Specs:** 001, 002, 003, 004, 005 (scoped)
**Depends on:** Gate 3 (merged, green, CI-enforced) · **Blocks:** the rest of §9, then Phase 3

Gates 1–3 proved data moves correctly. Gate 4 proves a program can *find out what
happened* — and it finally attacks the structures, which all three previous
non-proof sections named as the project's largest untested exposure.

Two things are new in kind:

- **`WHENEVER` is the first generated control flow.** Every construct so far
  emitted a call; this emits `if` statements around the calls that follow it,
  governed by a directive that can appear anywhere and applies in source order.
- **The `SQLCA` is the first SQL/MP structure the project generates.** Layout is
  API — customer code allocates copies with `SQLCA_LEN` and shares them
  `EXTERNAL` across modules.

## Why this rather than positioned operations

The obvious Gate 4 was positioned `UPDATE`/`DELETE` plus cursor stability. That
slice is not cleanly scopeable: **004 Q3 (cursor stability)** cannot be narrowed
the way Gate 3 narrowed Q6. Q6 had a reading that is obviously safe — end-of-set
idempotency makes the ordinary loop correct. Isolation levels have no such
reading: the question is what *other sessions* observe, and guessing produces
silent anomalies under concurrency, which is precisely the class Constitution III
exists to prevent. It needs `SQLRM`.

Positioned operations without stability remain a viable later slice; Q7 is
Q6-shaped and `DIV-051` already carries a documented choice.

## The programs

**`whenever_flow.sqlc`** — the directive doing its job.

```c
#pragma SQL
#include <stdio.h>
#include <string.h>

long sqlcode;
static int errors = 0, notfounds = 0;

static void on_error(void)     { ++errors; }

EXEC SQL INCLUDE SQLCA;

EXEC SQL BEGIN DECLARE SECTION;
  short part_num;
  char  part_desc[19];
EXEC SQL END DECLARE SECTION;

int main(void)
{
  EXEC SQL WHENEVER SQLERROR CALL :on_error;
  EXEC SQL WHENEVER NOT FOUND GOTO :done;

  EXEC SQL BEGIN WORK;

  part_num = 4102;
  EXEC SQL SELECT part_desc INTO :part_desc FROM parts WHERE part_num = :part_num;

  part_num = 9999;                       /* no such row — jumps to done */
  EXEC SQL SELECT part_desc INTO :part_desc FROM parts WHERE part_num = :part_num;

  printf("unreachable\n");

done:
  ++notfounds;
  EXEC SQL WHENEVER SQLERROR CONTINUE;   /* proves CONTINUE disables (criterion 3);
                                            under SD-5 the COMMIT below was never
                                            guarded in the first place */
  EXEC SQL COMMIT WORK;
  printf("errors=%d notfounds=%d\n", errors, notfounds);
  return 0;
}
```

**`whenever_scope.sqlc`** — a directive superseded mid-file; the handler must fire
for statements before the change and not after.
**`sqlca_items.sqlc`** — `INCLUDE SQLCA`, provoke a failure, read items back.
**`negative/whenever_undeclared.sqlc`** — an action naming an identifier that does
not exist.
**`negative/structures_after_include.sqlc`** — `INCLUDE STRUCTURES` after an
`INCLUDE SQLCA`.

## Exit criteria

1. `WHENEVER SQLERROR CALL` invokes the handler after a failing statement and not
   after a succeeding one.
2. `WHENEVER NOT FOUND GOTO` transfers control on `sqlcode` 100.
3. `WHENEVER … CONTINUE` disables a previously active action.
4. A directive superseded later in the file applies only to the statements
   between the two, in source order.
5. **Emitted checks appear in the order NOT FOUND, SQLERROR, SQLWARNING** — the
   published precedence, verified structurally at Tier 1.
6. `INCLUDE SQLCA` declares a structure whose `sizeof` is exactly `SQLCA_LEN`
   (430), with eye-catcher `CA`.
7. `SQLCAGETINFOLIST` returns the documented numeric items after a failing
   statement, and its documented error codes on misuse.
8. `SQLCAFSCODE` returns file-system detail for a failure that has one.
9. Omitting `INCLUDE STRUCTURES` emits the informational message and generates
   version 2.
10. Tier 1 green with no MariaDB present; diagnostic registry clean.

## Slice decisions

SD-1, SD-2 carry forward, still **provisional**. SD-3 is not touched (no
cursors). One new decision.

- **SD-1** — `UNKNOWN` single-byte charset binds as the connection default.
  Narrows 002 Q4.
- **SD-2** — the program declares `long sqlcode;`. Narrows 005 Q8.
- **SD-4 (new)** — `SQLCAGETINFOLIST` item 22 reports errors as **positive** and
  warnings as **negative**, reproducing the manual as published even though it
  inverts `sqlcode`'s convention. Narrows 005 Q7 only. Chosen because the
  inversion is stated plainly in a table of item codes rather than in prose, so
  it reads as deliberate rather than as a typo — but it is odd enough to be worth
  re-checking against `SQLRM`. **Provisional.** A test pins it in both
  directions so that a later reversal is a visible change, not a silent one.
- **SD-5 (new)** — `WHENEVER` does **not** apply to `BEGIN`/`COMMIT`/`ROLLBACK
  WORK`. Narrows 005 Q9 only. Chosen because §9 names three statement classes and
  §3 lists transaction control as a fourth, so the exclusion reads as deliberate;
  and because the safer error is a handler that fires too rarely rather than one
  that fires on a commit the program did not expect to be guarded.
  **Provisional** — the list may simply be incomplete.
- **SD-6 (new)** — a `CALL` handler is `void (*)(void)`. Narrows 005 Q10 only.
  Chosen because every example in §9 passes a bare identifier with no arguments,
  and because a handler needing state can read `sqlcode`, which is in scope by
  construction. **Provisional** — the manual states no signature at all.

## Open-question avoidance

Every open question in the five specs this slice touches.

| Question | Touched? | Why not |
|---|---|---|
| 001 Q1–Q4 | no | all carry decisions; 001 is `Ready` |
| 002 Q1 warning codes | no | see 005 Q4 — no warning is provoked at runtime |
| 002 Q2 `SETSCALE` | no | no scaled column |
| 002 Q3 C `fixed` | no | not used |
| **002 Q4 charset mapping** | **yes** | a `char` array with no `CHARACTER SET`. Carried decision **SD-1** |
| 002 Q5 storage class | no | plain declarations |
| 002 Q6 declarators | no | one per statement |
| 003 Q1 outside `BEGIN WORK` | no | statements are wrapped |
| 003 Q2 nested `BEGIN WORK` | no | no nesting |
| 003 Q3 open lifecycle / 8204 | no | happy path plus a not-found |
| 003 Q6 `DEFMODE` | no | directly-mapped table name |
| 004 Q2 multi-row single-row `SELECT` | no | queries are by primary key |
| 004 Q3 cursor stability | no | no cursors in this slice |
| 004 Q4 `CLOSE` inside vs outside a transaction | no | no cursors |
| 004 Q5 cursor scope | no | no cursors |
| 004 Q6 position after exhaustion | no | no cursors |
| 004 Q7 position after positioned `UPDATE` | no | no positioned operations |
| 004 Q8 cursor PAID | no | no cursors |
| 005 Q3 `SQLSA` sentinels | no | **no `SQLSA` in this slice** — `INCLUDE SQLSA` keeps its `ESQLC-1012` |
| **005 Q4 conversion warning codes** | **no, by construction** | host variables are sized to their columns, so no conversion warning can arise. `WHENEVER SQLWARNING` is therefore verified **structurally only** — its emitted position in the precedence order — and never fired at runtime. Recorded in the non-proof list |
| 005 Q5 `WHENEVER` and dynamic SQL | no | no dynamic SQL; §9 names DML, DCL and DDL, all of which this slice covers |
| **005 Q6 SQL message file** | **no, by exclusion** | `SQLCADISPLAY` and `SQLCATOBUFFER` render messages and are deliberately out of scope. Only the non-rendering accessors are included |
| **005 Q7 item-22 sign inversion** | **yes** | `SQLCAGETINFOLIST` item 22 is in scope. Narrowed by **SD-4** |
| **005 Q8 who declares `sqlcode`** | **yes** | the fixtures reference it. Carried decision **SD-2** |
| **005 Q9 `WHENEVER` and transaction control** | **yes** | the fixtures wrap statements in `BEGIN`/`COMMIT WORK` while a handler is active, so the question cannot be dodged. Narrowed by **SD-5** |
| **005 Q10 `CALL` handler signature** | **yes** | the slice emits a call to one. Narrowed by **SD-6** |

## Design questions this slice must settle

**1. `WHENEVER` is preprocessor state that emits code at other constructs.**
The directive itself emits nothing. It changes what every *subsequent* statement
emits, until superseded. 001's FR-001.22 already requires the scanner to track
that state; Gate 4 is where it is finally consumed. The state is per-condition —
setting `SQLERROR` does not clear `NOT FOUND`.

**2. `GOTO` needs a C label that may not exist.** `WHENEVER NOT FOUND GOTO :done`
emits `goto done;`, and if `done:` is absent the customer sees a C compiler error
about a label rather than an ESQL diagnostic. FR-005.8's `ESQLC-5008` says an
action naming an undeclared identifier is diagnosed — but the preprocessor does
not parse C well enough to know a label exists. The slice must decide how far to
go: diagnose what it can see, or accept that some cases surface as C errors, and
say which.

**3. The SQLCA is generated, not just consumed.** `INCLUDE SQLCA` emits a
structure declaration whose size must be exactly 430. Under `DIV-041` the layout
is ours, so the constraint is the total and the eye-catcher, not the offsets —
but the total is load-bearing, because programs allocate extra copies with
`SQLCA_LEN`.

## Scoped requirement set

**001:** FR-001.13, FR-001.15, FR-001.22; NFR-001.1
**002:** FR-002.28
**003:** FR-003.1, FR-003.2, FR-003.3
**005:** FR-005.1, FR-005.3, FR-005.4, FR-005.5, FR-005.6, FR-005.7,
FR-005.10, FR-005.14, FR-005.14a, FR-005.15, FR-005.23b, FR-005.30, FR-005.31

Plus diagnostics `ESQLC-5001`, `ESQLC-5006`, `ESQLC-5008`.

Deliberately absent: all of `SQLSA` and `SQLDA`; `SQLCADISPLAY` and
`SQLCATOBUFFER` (message rendering); the full `INCLUDE STRUCTURES` version matrix
(FR-005.8, .9, .11, .12, .13); `SQLSA VERSION CURRENT`; and every cursor and
positioned-operation requirement.

## The ABI grows again

`SQLCAFSCODE` maps onto the existing `esqlc_fs_detail`, but `SQLCAGETINFOLIST`
has no counterpart — the runtime must expose the `SQLCA`'s contents through an
item-code interface. New entry points land in the 003 contract in the same change
as the plan (Principle V).

## What Gate 4 will not prove

- Nothing about `SQLSA`. Its layout is now known and validated against both
  published sizes, but nothing generates or populates it, so 005 Q3's sentinels
  remain unexercised.
- Nothing about `SQLDA`, and `DIV-040`'s widening of `SQLDA_SQLVAR_LEN` is still
  unimplemented and unproven.
- **`WHENEVER SQLWARNING` is never fired at runtime.** Its emitted position in
  the precedence order is verified; whether a real warning triggers it is not,
  because the warning values themselves are `DIV-042`, still open.
- Nothing about message rendering, so `SQLCADISPLAY`'s output format is untested
  and 005 Q6 stays open.
- Nothing about structure version selection beyond the default-to-version-2 path.
- Nothing about positioned operations or cursor stability, unchanged from Gate 3.

A green Gate 4 means a program can branch on what happened and read the
diagnostic area. The statistics area, the descriptor area, and message rendering
all remain untouched.
