# Gate 6 — searched `UPDATE` and `DELETE`

**Slice of:** 001, 002, 003, 004, 005 · **Status:** ready to plan ·
**Predecessor:** Gate 5 (the `SQLSA`)

Four requirements, and the point at which the subset stops being a
demonstration. Insert, retrieve, iterate and now modify: a program can do all
four things a customer program actually does.

Deliberately the smallest gate so far. Most of the machinery exists — the
statement path, descriptors, `sqlcode`, the diagnostic areas — and this slice
adds two keywords to it. What makes it worth doing is not the two keywords.

## Why this rather than type breadth

Type breadth is the larger prize and is equally unblocked, but this comes first
for three reasons that are not "it is next in the spec".

**It exercises input indicators, which nothing has tested.** Gate 2 proved the
output direction — reading a null column into a host variable with an indicator.
Setting a column *to* null with a negative indicator (FR-004.8) is the other
direction and a different code path. It has been declared in the descriptor
since Gate 1 and never once driven.

**`sqlcode` 100 for a statement that succeeded.** FR-004.10 makes a searched
`UPDATE` or `DELETE` matching no rows set 100. The transport succeeded, the
statement was valid, and the answer is still "not found" — which is the exact
shape a runtime gets silently wrong by reporting 0.

**It confronts the gap Gate 5 left.** `table_name` has no source on the DML
path, and `UPDATE`/`DELETE` are DML. Gate 5 stamped the character sentinel and
named it a real gap rather than a scoping choice. This slice either answers it
or formally accepts it; leaving it unowned for a third gate is how a documented
gap turns into a forgotten one.

## The table-name landmark

The gap has an answer, and it does not require parsing.

`table_name` was called unsourceable on the DML path because result-set metadata
does not exist there and NFR-001.1 forbids parsing the statement body. But the
project already has a third option it has used twice: a **landmark** — a
position the lexer notes in passing, in the same pass, without parsing anything.
`INTO` and `FROM` are landmarks (Gate 2). Cursor names are read by
`name_after_verb` (Gate 3), which is the same idea applied to an identifier.

The table name in all three DML forms sits at a fixed position relative to the
leading keyword:

| Statement | Landmark |
|---|---|
| `INSERT INTO parts …` | identifier after `INTO` |
| `UPDATE parts SET …` | identifier after `UPDATE` |
| `DELETE FROM parts …` | identifier after `FROM` |

That is the same operation `name_after_verb` already performs for `OPEN c1`. It
reads one identifier at a known offset; it does not interpret the statement,
and everything after the landmark stays opaque.

Recorded as **SD-9** and provisional, because the landmark is defeated by forms
this slice does not cover — a multi-table `UPDATE`, a subquery before the table
name, a delimited identifier. Those must reach the sentinel rather than a wrong
name, which is a requirement of the slice and not an afterthought.

## The programs

**A — `UPDATE` that matches rows.** Change a column on a known row, verify the
new value with a fresh read, and check `records_used` reflects the rows changed.

**B — `UPDATE` and `DELETE` matching nothing.** A `WHERE` clause that selects no
row. `sqlcode` must be 100, not 0, and the transport must still be a success.
Both statements, because the two take different paths through affected-rows
handling.

**C — setting a column to null.** `UPDATE … SET weight = :w :ind` with the
indicator negative. The column must become null, and the *value* in `:w` must be
ignored rather than stored — the failure mode is a runtime that writes the
buffer contents and sets null only when the buffer happens to be zero.

**D — `DELETE` that matches many rows.** More than one row removed by a single
statement, with `records_used` matching the count. This is also the first
statement in the project whose `SQLSA` reports a number greater than 1.

**E — the table landmark.** `INSERT`, `UPDATE` and `DELETE` each report their
table in `SQLSA.stats[0].table_name`, and a form the landmark cannot read
reports the `?` sentinel rather than a wrong name.

## Exit criteria

1. A searched `UPDATE` changes the rows its `WHERE` selects, and no others.
2. A searched `DELETE` removes the rows its `WHERE` selects, and no others.
3. An `UPDATE` matching no rows sets `sqlcode` 100 with a successful transport.
4. A `DELETE` matching no rows does the same.
5. A negative indicator sets the column null, and the host variable's value is
   not stored.
6. A multi-row `DELETE` reports its row count in `records_used`.
7. `table_name` is populated for all three DML statements via the landmark.
8. A statement form the landmark cannot read yields the `?` sentinel, never a
   wrong or partial name.
9. `UPDATE`/`DELETE` are removed from the `ESQLC-1012` list, and `INCLUDE SQLDA`
   and the rest stay on it.
10. Tier 1 green with no MariaDB present; registry, contract and citation
    harnesses clean; `sqlsa_layout_sync` still green.

## Slice decisions

SD-1, SD-2, SD-7, SD-8 carry forward, still **provisional**. One new.

- **SD-1** — `UNKNOWN` single-byte charset binds as the connection default.
  Narrows 002 Q4.
- **SD-2** — the program declares `long sqlcode;`. Narrows 005 Q8.
- **SD-7** — an unmappable numeric `SQLSA` field carries `-1` in its own width.
  Narrows 005 Q3.
- **SD-8** — an unmappable character field carries `?` to full width. Narrows
  005 Q3. **This slice narrows where it applies**: with SD-9 in place, the
  sentinel becomes the answer for statement forms the landmark cannot read,
  rather than for the whole DML path.
- **SD-9 (new)** — `table_name` on the DML path comes from a **landmark**: the
  identifier after `INTO`, `UPDATE`, or `FROM` as the statement's leading
  keyword dictates. Narrows nothing in the open-question list — it settles the
  gap Gate 5's implementation report raised. **Provisional**, because the
  landmark is defeated by multi-table `UPDATE`, a leading subquery, and
  delimited identifiers; all of those must reach SD-8's sentinel rather than a
  wrong name, and a test pins that.

## Design questions this slice must settle

- **Does the landmark belong in the scanner or the emitter?** The scanner,
  alongside `into_off` and `from_off`, because that is where same-pass position
  capture already lives and a string literal must not be mistaken for a table
  name. Putting it in the emitter would mean a second scan of the body.
- **What reaches the runtime?** `esqlc_stmt_exec` gains the table name. The
  contract marks that entry point **outline, not frozen**, so extending it is
  within its stated latitude rather than a breaking change — but the contract
  must be updated in the same change (Principle V).
- **Is a zero-row `UPDATE` distinguishable from a failure?** It must be: the
  transport returns success and `sqlcode` is 100. The runtime already
  distinguishes these for `SELECT`; this confirms the same split on the write
  path rather than assuming it.
- **What does a negative indicator do with the buffer?** Nothing is read from
  it. The bind must send SQL NULL and never the buffer contents, which is the
  input-side counterpart of Gate 1's `width`/`capacity` care.

## Open-question avoidance

Every open question in the five specs this slice touches.

| Question | Touched? | Why not |
|---|---|---|
| 001 Q1–Q4 | no | all carry decisions; 001 is `Ready` |
| 002 Q1 warning codes | no | host variables are sized to their columns |
| 002 Q2 `SETSCALE` | no | no scaled column |
| 002 Q3 C `fixed` | no | not used |
| **002 Q4 charset mapping** | **yes** | `char` arrays with no `CHARACTER SET`. Carried decision **SD-1** |
| 002 Q5 storage class | no | plain declarations |
| 002 Q6 declarators | no | one per statement |
| 003 Q1 outside `BEGIN WORK` | no | statements are wrapped |
| 003 Q2 nested `BEGIN WORK` | no | no nesting |
| 003 Q3 open lifecycle / 8204 | no | happy path plus a zero-row case |
| 003 Q4 connection scope | no | single-threaded fixtures |
| 003 Q5 configuration mechanism | no | settled by the implemented resolution order |
| 003 Q6 `DEFMODE` | no | directly-mapped table names |
| 004 Q1 position table contents | no | resolved by Gate 3 |
| 004 Q2 multi-row single-row `SELECT` | no | verification reads are by primary key |
| 004 Q3 cursor stability | no | **no cursors at all in this slice** — searched DML has none. This is why the slice is available while positioned operations are not |
| 004 Q4 `CLOSE` inside vs outside a transaction | no | no cursors |
| 004 Q5 cursor scope | no | no cursors |
| 004 Q6 position after exhaustion | no | no cursors |
| 004 Q7 position after positioned `UPDATE` | no | **searched, not positioned.** `WHERE CURRENT OF` is out of scope precisely because it would touch this |
| 004 Q8 cursor PAID | no | no cursors |
| 004 Q9 `DECLARE CURSOR` dispatch | no | fixed by Gate 3 |
| 005 Q1 `SQLCA` layout | no | resolved — `DIV-041` |
| 005 Q2 `SQLSA` offsets | no | resolved by Gate 5 |
| **005 Q3 `SQLSA` sentinels** | **yes** | the DML path populates the area. Carried **SD-7** and **SD-8**, and **SD-9** narrows where SD-8 applies |
| 005 Q4 conversion warning codes | no | no conversion is provoked |
| 005 Q5 `WHENEVER` and dynamic SQL | no | no dynamic SQL |
| 005 Q6 SQL message file | no | no rendering |
| 005 Q7 item-22 sign inversion | no | `SQLCAGETINFOLIST` unchanged; **SD-4** stands |
| **005 Q8 who declares `sqlcode`** | **yes** | the fixtures reference it. Carried decision **SD-2** |
| 005 Q9 `WHENEVER` and transaction control | no | this slice checks `sqlcode` directly |
| 005 Q10 `CALL` handler signature | no | no `CALL` |

## Scoped requirement set

**In:** FR-004.7, FR-004.8, FR-004.9, FR-004.10, FR-005.17 *(the DML arm, which
Gate 5 exercised only for `INSERT`)*, FR-005.22 *(`table_name` via SD-9)*.

Carried and re-exercised: FR-001.15, FR-002.15, FR-002.16, FR-002.30, FR-003.1,
FR-003.2, FR-003.3, FR-003.10, FR-005.1, NFR-001.1, NFR-003.2.

**Out:** FR-004.11 through FR-004.19 — every positioned operation, `WHERE
CURRENT OF`, and cursor `UPDATE`/`DELETE`. They need 004 Q3 and Q7, and Q3 needs
`SQLRM`. FR-004.8's null-setting is in scope; the *cursor* route to it is not.

## The ABI

`esqlc_stmt_exec` gains the table landmark:

```c
/* `table` is the landmark identifier the scanner captured, or NULL when the
   statement's form did not yield one. NULL means the SQLSA reports SD-8's
   character sentinel — never a guess. */
int esqlc_stmt_exec(const char *body, size_t body_len,
                    const esqlc_hostvar_t *vars, int var_count,
                    const char *table);
```

The contract marks this entry point **outline, not frozen**, so extending it is
within its stated latitude. It is still a signature change and the contract is
updated in the same change (Principle V).

No other entry point moves. `esqlc_sqlsa_register` is unchanged, and the cursor
entry points are untouched.

## What Gate 6 will not prove

- **Positioned operations.** `WHERE CURRENT OF` in either form. Still blocked on
  004 Q3 and Q7, and Q3 still needs `SQLRM`.
- **Cursor stability.** Untouched, and this slice does not bring it closer.
- **The landmark on hard forms.** Multi-table `UPDATE`, a leading subquery and
  delimited identifiers are proven to reach the *sentinel*, which is not the
  same as proving they could be read. They remain unread.
- **Type breadth.** Still `char[]` and 16-bit integers. This slice adds
  statements, not types, and a program using `INTEGER` still cannot compile.
- **`records_accessed` and the rest of `stats[]`.** Still sentinels per
  `DIV-011`. Only `records_used`, `num_tables` and now `table_name` are real.
- **Concurrency.** Every modification is single-session. What another session
  observes mid-statement is not examined, and is the same question 004 Q3 asks.
