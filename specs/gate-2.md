# Gate 2: retrieval

**Status:** Tasked · **Plan:** [gate-2-plan.md](gate-2-plan.md) ·
**Tasks:** [gate-2-tasks.md](gate-2-tasks.md)
**Specs:** 001, 002, 003, 004, 005 (scoped)
**Depends on:** Gate 1 (merged, green, CI-enforced) · **Blocks:** cursors, then Phase 3

Gate 1 proved data can leave a C program and reach MariaDB faithfully. Gate 2
proves it can come back. It is deliberately the *smallest* retrieval slice:
single-row `SELECT … INTO` by primary key, one indicator variable, and the
not-found path. No cursors, no `WHENEVER`, no structures.

It also discharges the two requirements Gate 1 had to drop as untestable —
`FR-002.28` and `FR-003.12` — both of which govern output binding.

## The programs

Three fixtures against the Gate 1 `parts` schema, extended with one nullable
column.

**`select_into.sqlc`** — the happy path.

```c
#pragma SQL
#include <stdio.h>
#include <string.h>

long sqlcode;

EXEC SQL BEGIN DECLARE SECTION;
  short part_num;
  char  part_desc[19];
  short weight;
  short weight_ind;
EXEC SQL END DECLARE SECTION;

int main(void)
{
  part_num = 4102;

  EXEC SQL BEGIN WORK;

  EXEC SQL SELECT part_desc, weight
             INTO :part_desc, :weight INDICATOR :weight_ind
             FROM parts
            WHERE part_num = :part_num;

  if (sqlcode != 0) { long s = sqlcode; EXEC SQL ROLLBACK WORK;
                      fprintf(stderr, "select failed %ld\n", s); return 1; }

  EXEC SQL COMMIT WORK;
  printf("%.18s|%d|%d\n", part_desc, weight, weight_ind);
  return 0;
}
```

**`not_found.sqlc`** — same query, a `part_num` that does not exist. Must yield
`sqlcode` 100 with host variables **unmodified**.

**`null_no_indicator.sqlc`** — selects the nullable column with no indicator
supplied, against a row where it is null. Must fail rather than silently produce
a value.

Schema addition: `weight SMALLINT NULL`.

## Exit criteria

1. Happy path: `part_desc` receives the column's 18 bytes, `weight` its value,
   `weight_ind` `0`; `sqlcode` `0`.
2. **`part_desc` has no null terminator appended** — byte 18 of the array holds
   whatever it held before the call (FR-002.28). This is the mirror of Gate 1's
   insert-side check and equally easy to get wrong.
3. **Trailing blanks survive retrieval**: an 18-byte column value ending in
   blanks arrives as 18 bytes, not truncated (`DIV-052`).
4. Null column with an indicator: `weight_ind` is exactly `-1`, and `weight` is
   left alone.
5. No rows: `sqlcode` is exactly 100, and every host variable is byte-identical
   to its pre-call contents.
6. Null column with no indicator: a negative `sqlcode` (`ESQLC-4009`, SQL error
   8423), not a zero-filled value.
7. Tier 1 suite still green with no MariaDB present, and the diagnostic registry
   still clean.

Criteria 2 and 5 are the ones a plausible implementation gets wrong: both
require the runtime to *not* write where it might feel helpful.

## `DIV-052` is resolved by this slice

Option 1 accepted: **the runtime sets `PAD_CHAR_TO_FULL_LENGTH` on its own
session at connect.** Reasons — it makes every retrieval path faithful at once
with no per-fetch cost, needs no customer source change, and the mode is narrow,
affecting only fixed-length character retrieval padding. It was empirically
confirmed during Gate 1 to produce the faithful 18-byte result.

Option 2 (padding from column metadata in the runtime) remains the documented
fallback if the session mode proves insufficient — for instance if a deployment's
option file overrides `sql_mode` after connect. Criterion 3 is what would catch
that.

`DIV-052` moves from `proposed` to `accepted` when this slice lands.

## Open-question avoidance

Every open question in the five specs this slice touches.

| Question | Touched? | Why not |
|---|---|---|
| 001 Q1–Q4 | no | all four carry decisions; 001 is `Ready` |
| 002 Q1 warning codes | no | host variables are sized to the column exactly, so no truncation or scale-loss warning can arise; criteria assert `sqlcode` 0 |
| 002 Q2 `SETSCALE` | no | no scaled column |
| 002 Q3 C `fixed` | no | not used |
| **002 Q4 charset mapping** | **yes** | unchanged from Gate 1 — a `char` array with no `CHARACTER SET` is charset `UNKNOWN`. Carried decision **SD-1** |
| 002 Q5 storage class | no | plain declarations |
| 002 Q6 declarators | no | one per statement |
| 003 Q1 outside `BEGIN WORK` | no | every fixture wraps its statement, as Gate 1 did |
| 003 Q2 nested `BEGIN WORK` | no | no nesting |
| 003 Q3 open lifecycle / 8204 | no | happy path only |
| 003 Q6 `DEFMODE` | no | directly-mapped table name |
| **004 Q2 multi-row single-row `SELECT`** | **no, by construction** | every query is by primary key, so exactly one row can match. The multi-row error is 004 Q2 and needs `SQLRM` — deliberately not exercised |
| 004 Q3 cursor stability | no | no cursors |
| 004 Q4 `CLOSE` semantics | no | no cursors |
| 004 Q5 cursor scope | no | no cursors |
| 004 Q6 `FETCH` past last row | no | no cursors |
| 004 Q7 positioned `UPDATE` | no | no cursors |
| 004 Q8 cursor PAID / `IN EXCLUSIVE MODE` | no | no cursors |
| 005 Q3 `SQLSA` sentinels | no | no `SQLSA` |
| 005 Q4 warning codes | no | as 002 Q1 |
| 005 Q5 `WHENEVER` and dynamic SQL | no | no `WHENEVER` |
| 005 Q6 SQL message file | no | no message rendering |
| 005 Q7 item-22 sign inversion | no | no `SQLCA` |
| **005 Q8 who declares `sqlcode`** | **yes** | the fixtures reference it. Carried decision **SD-2** |

### Slice decisions

Both carried forward from Gate 1, still **provisional**, still owing a revisit
when their questions close.

- **SD-1** — `UNKNOWN` single-byte charset binds as the connection's default
  character set, no transcoding. Narrows 002 Q4 only.
- **SD-2** — the program declares `long sqlcode;`. Narrows 005 Q8 only.

No new slice decisions. `DIV-052` is a resolution, not a narrowing.

## Design question this slice must settle

**How does the preprocessor know which host variables are outputs?** In
`SELECT a INTO :x FROM t`, `:x` is an output and `:part_num` in the `WHERE`
clause is an input. Direction cannot be inferred from the reference syntax.

`NFR-001.1` anticipated exactly this: bodies stay opaque "except for
host-variable references, the leading keyword, and constructs 004/006/007
explicitly claim". Gate 2 is where **004 claims the `INTO` clause** — the
preprocessor recognises `INTO` and marks the references between it and the next
clause keyword as `ESQLC_DIR_OUT`.

That is a real widening of the preprocessor's SQL awareness and should be
recorded as such in the plan, along with its limit: `INTO` recognition only, not
clause-structure parsing. Whether that limit holds for cursor `FETCH … INTO`
later is a question for Gate 3, not an assumption for this one.

## Scoped requirement set

**001:** FR-001.15, FR-001.16, FR-001.25; NFR-001.1
**002:** FR-002.15, FR-002.16, FR-002.22, FR-002.28
**003:** FR-003.10, FR-003.12, FR-003.13
**004:** FR-004.1, FR-004.2
**005:** FR-005.1, FR-005.2

Plus the `DIV-052` implementation and diagnostic `ESQLC-4009`.

Deliberately absent: every cursor requirement, all of `WHENEVER`, all of
`SQLCA`/`SQLSA`/`SQLDA`, `INVOKE`, and every conversion warning.

## What Gate 2 will not prove

- Nothing about cursors — no `DECLARE`, `OPEN`, `FETCH`, `CLOSE`, and therefore
  nothing about the cursor position rules, which are 004's largest risk and are
  under-specified in the manual besides.
- Nothing about the structures. `SQLCA` still has no published layout, `SQLSA`'s
  offsets remain inferred, and `SQLDA_SQLVAR_LEN` still awaits `DIV-040`. These
  are the biggest correctness exposure in the project and both gates dodge them.
- Nothing about conversion or truncation, since host variables match their
  columns exactly.
- Nothing about multi-row results, which is a live open question.
- Nothing about `WHENEVER`, so no control-flow generation is exercised.

A green Gate 2 means data round-trips faithfully in the simplest case. The
manual's harder promises remain untested.
