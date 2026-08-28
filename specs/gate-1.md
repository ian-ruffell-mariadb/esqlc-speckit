# Gate 1: the first vertical slice

**Status:** Planned · **Plan:** [gate-1-plan.md](gate-1-plan.md)
**Specs:** 001, 002, 003 (scoped) · **Blocks:** Phase 2

Gate 1 is the smallest program that proves the architecture end to end. It is
not a milestone in the sense of "a lot of work finished" — it is a *falsification
test* for the three decisions that would be most expensive to get wrong: the
handler interface, the runtime ABI, and the claim that opaque statement bodies
can be parameterised without the preprocessor parsing SQL.

## The program

One fixture, `tests/conformance/gate-1/insert.sqlc`. Written for this project,
not adapted from the manual.

```c
#pragma SQL

#include <stdio.h>
#include <string.h>

long sqlcode;            /* program-declared, per §10 p.10-23 */

EXEC SQL BEGIN DECLARE SECTION;
  short part_num;
  char  part_desc[19];
EXEC SQL END DECLARE SECTION;

int main(void)
{
  part_num = 4102;
  /* 18 characters exactly — blank-padded, no null terminator stored */
  memcpy(part_desc, "HEX NUT, 8MM      ", 18);
  part_desc[18] = '\0';

  EXEC SQL BEGIN WORK;

  EXEC SQL INSERT INTO parts (part_num, part_desc)
           VALUES (:part_num, :part_desc);

  if (sqlcode != 0) {
    EXEC SQL ROLLBACK WORK;
    fprintf(stderr, "insert failed, sqlcode %ld\n", sqlcode);
    return 1;
  }

  EXEC SQL COMMIT WORK;
  return 0;
}
```

Schema fixture: a single table, `part_num SMALLINT SIGNED NOT NULL`,
`part_desc CHAR(18) NOT NULL`, primary key `part_num`. Two columns, two host
variables, two type families — no more.

## Exit criteria

1. The fixture preprocesses without diagnostics.
2. The emitted C compiles against the ABI header with **no MariaDB header on the
   include path** (AS-003.5 — proves Constitution V holds).
3. It links against the real runtime and runs.
4. The row is present in MariaDB afterwards, queried from a second connection,
   with `part_desc` holding exactly the 18 bytes the program placed in the array
   — blank-padded by the program, with **no null terminator stored**
   (FR-002.30, FR-002.31).
4a. A variant that deliberately under-fills the array **stores its null byte**,
   proving the runtime neither trims at the terminator nor pads on the program's
   behalf. This is the check most likely to diverge from SQL/MP by accident,
   because a `strlen`-based binding would silently "fix" it.
5. `sqlcode` is `0`.
6. `ROLLBACK WORK` on the failure path is exercised by a variant fixture that
   violates the primary key, leaving the table unchanged.
7. `#line` fidelity holds: a deliberate C error in a variant reports the original
   line (AS-001.4).

Criterion 6 matters more than it looks: it is the only one that proves the
transaction actually *is* a transaction rather than autocommit wearing a costume.

## Why this program and not a simpler one

Every element is load-bearing:

| Element | Proves |
|---|---|
| `#pragma SQL` | pragma handling and the mandatory-position rule |
| declare section | 002's declaration path, both type families |
| `char[19]` for `CHAR(18)` | the extra-byte rule and the blank-padding trap |
| `short` for `SMALLINT` | width-exact emission (`DIV-001`) |
| `:host_var` in an opaque body | the central architectural bet — parameterisation without SQL parsing |
| `BEGIN`/`COMMIT WORK` | transaction ABI, and it *avoids* 003 Q1 (see below) |
| `ROLLBACK WORK` variant | that the transaction is real |
| `sqlcode` test | the diagnostics channel before SQLCA exists |

Dropping the transaction statements would make the gate *weaker and riskier*, not
simpler — see the next section.

## Open-question avoidance

Every open question in the three specs, and why the gate does not touch it.
This table is the gate's justification for proceeding while the specs remain
`Clarifying`.

| Question | Touched? | Why not |
|---|---|---|
| 001 Q1 position classes | no | carries a decision (plan §1) |
| 001 Q2 pragma option set | no | frozen set; gate uses bare `#pragma SQL` |
| 001 Q3 `SQL SOURCE` guards | no | gate has no `SQL SOURCE` |
| 001 Q4 label prefix | no | gate uses no label — and now resolved anyway |
| 002 Q1 warning codes | no | happy path produces no warning; exact-width values, `sqlcode` asserted `0` |
| 002 Q2 `SETSCALE` persistence | no | no scaled column, no `SETSCALE` |
| 002 Q3 C `fixed` | no | not used |
| **002 Q4 charset mapping** | **partially** | `char[]` with no `CHARACTER SET` is charset `UNKNOWN` by FR-002.8, so the `UNKNOWN` corner is unavoidable. Scoped decision below |
| 002 Q5 storage class | no | plain declarations, no storage class or initialiser |
| 002 Q6 multiple declarators | no | one declarator per statement |
| **003 Q1 outside `BEGIN WORK`** | **no** | Q1 concerns statements *outside* a transaction. The gate wraps its `INSERT`, so explicit transaction control **avoids** the question |
| 003 Q2 nested `BEGIN WORK` | no | no nesting |
| 003 Q3 open lifecycle / 8204 | no | happy path only; 8204 cannot occur here and its recovery is not exercised |
| 003 Q6 `DEFMODE` analogue | no | directly-mapped table name, no DEFINE |
| **005 Q8 who declares `sqlcode`** | **yes** | the fixture references `sqlcode`, so it cannot avoid the question. Scoped decision below |

### Scoped decisions

Both are **provisional** and neither resolves its question.

**SD-1 — 002 Q4, `UNKNOWN` charset corner.** A `char` host variable with no
`CHARACTER SET` clause carries charset `UNKNOWN` (FR-002.8). For this slice,
`UNKNOWN` single-byte binds as the connection's default character set, with no
transcoding. Narrow, defensible, and needed regardless of how the
`KANJI`/`KSC5601`/ISO-8859 mappings are eventually decided — those remain open.

**SD-2 — 005 Q8, who declares `sqlcode`.** For this slice the **program**
declares it, as `long sqlcode;`. Grounded in §10 p.10-23, which lists declaring
the variable among the program's own development steps. If the answer turns out
to be preprocessor-generated, the fixture changes and the runtime's
`esqlc_sqlcode()` accessor is unaffected — which is why this decision is cheap to
reverse and safe to make now.

Both must be revisited when their questions close. A slice decision that later
contradicts the real answer is a defect to fix, not precedent to defend
(Principle VIII condition 3).

## Scoped requirement set

The requirements this slice must satisfy. Anything not listed is out of scope for
Gate 1 and stays `Clarifying`.

**001:** FR-001.1, .2, .7, .11, .12, .15, .16, .18, .19; NFR-001.1, .2, .3
**002:** FR-002.1, .2, .3, .9, .30, .31; NFR-002.2
**003:** FR-003.1, .2, .3, .4, .5, .6, .8, .10, .13, .16, .17, .19, .21;
NFR-003.1, .2, .3

**Deferred to Gate 2** (both govern retrieval, which this slice has no `SELECT`
to exercise): FR-002.28 no terminator appended on output, FR-003.12 output
binding. Both were in an earlier draft of this set and were removed during
planning, when the requirement→component map showed neither could be mapped to a
test the slice can actually run.

Notably absent and deliberately so: every `WHENEVER` requirement, all of
`SQLCA`/`SQLSA`, indicator variables, `TYPE AS`, cursors, and every conversion
warning. The gate has no nulls, no cursor, and no diagnostic beyond `sqlcode`.

## What Gate 1 does not prove

Stated so nobody mistakes a green gate for a working implementation:

- Nothing about retrieval — no `SELECT`, so output binding is unexercised.
- Nothing about nulls or indicators.
- Nothing about the structures, which are the largest layout risk in the project.
- Nothing about conversion, warnings, or truncation.
- Nothing about dynamic SQL.
- Nothing about concurrency beyond the single-thread check.

A green Gate 1 means the spine is sound. It says nothing about the breadth.
