# Gate 6 plan — searched `UPDATE` and `DELETE`

**Slice:** [specs/gate-6.md](gate-6.md) · **Specs:** 001, 002, 003, 004, 005 ·
**Planned under Principle VIII** (002, 004, 005 are `Clarifying`)

Slice conditions verified: enumerated subset (6 in-scope plus 11 carried),
avoidance table covering all 35 open questions, five provisional decisions, and
a specific non-proof section.

## 1. Approach

**Capture the table as a scanner landmark, bind input indicators for real, and
split "rows found" from "rows altered" because the manual defines them
differently.**

The slice looked almost free. Two checks against the manual made it less so,
and both changes are forced rather than chosen.

**`sqlcode` 100 means rows *found*, not rows *changed*.** p.4-13 states it
plainly for `DELETE` — *"No rows were found on a search condition"* — and p.4-5
uses the same wording for `SELECT`. MariaDB's `affected_rows` reports rows
**changed** for an `UPDATE` by default, so `UPDATE parts SET weight = 5 WHERE
part_num = 1` on a row whose weight is already 5 reports zero, and a naive
implementation sets `sqlcode` 100 for a row it definitely found. The connection
therefore asks for `CLIENT_FOUND_ROWS`, which makes `affected_rows` report rows
matched, and `sqlcode` follows that.

**`records_used` means rows *altered*, and that is a different number.** p.9-17
defines it as *"Number of records altered or returned"*. So the two values the
runtime must report after one `UPDATE` come from different counts: `sqlcode`
from rows matched, `records_used` from rows changed. Reporting one number for
both is a silent semantic change in whichever direction it is wrong, which
Principle III forbids.

MariaDB can supply both, but not from one call. With `CLIENT_FOUND_ROWS`,
`mysql_stmt_affected_rows` gives matched; `mysql_info` gives
`Rows matched: N  Changed: M  Warnings: W` for an `UPDATE`, and `M` is the
altered count. **Only `UPDATE` needs the split** — for `INSERT` and `DELETE`
matched and altered are the same number, so `affected_rows` serves both.

**The table name is a landmark, not a parse.** Gate 5 called it unsourceable on
the DML path. It is sourceable: the identifier sits at a fixed position after
the leading keyword, and the scanner already captures positions this way for
`INTO` and `FROM` in the same pass. This adds a third landmark of the same kind
and reuses the `name_after_verb` shape Gate 3 built for cursor names. The
statement body stays opaque; one identifier at a known offset is read, and
nothing after it is interpreted.

**Input indicators genuinely do not work yet.** `exec.c:33` sets
`b->is_null = NULL` for every input bind — the field has never been populated.
FR-004.8 is therefore real implementation, not a re-test, which is the main
reason this slice is worth a gate.

## 2. Alternatives rejected

**Leave `affected_rows` as changed-rows and accept `sqlcode` 100 for a matched
row.** Rejected: it makes a found row indistinguishable from a missing one,
which is the single most consequential thing `sqlcode` says. A program's
`WHENEVER NOT FOUND` handler would fire on a successful update.

**Take rows-matched for both values.** Simpler, and rejected because p.9-17 says
*altered*. It would inflate `records_used` for every no-op update, and a
statistic that silently means something else is worse than one that is honestly
a sentinel.

**Parse the table name from the statement body.** Rejected — NFR-001.1, and it
would undo the decision that has kept the preprocessor tractable for six gates.
The landmark gets the same answer without interpreting anything.

**Ask the server for the table via `EXPLAIN`.** Rejected: a round trip per
statement to recover something the source text already states positionally.

**A new entry point rather than extending `esqlc_stmt_exec`.** Rejected because
the contract marks that entry point *outline, not frozen*, so extending it is
within its stated latitude, and a parallel entry point would leave two
statement paths to keep in step.

## 3. Components

| Component | Path | Change | Slice scope |
|-----------|------|--------|-------------|
| Scanner | `src/pp/scan.cc` | third landmark: table identifier after `INTO`/`UPDATE`/`FROM` | single-table forms |
| Shared types | `src/pp/pp.h` | `Construct::table` | — |
| Dispatcher | `src/pp/dispatch.cc` | `UPDATE`/`DELETE` implemented; `INCLUDE SQLDA` and the rest keep `ESQLC-1012` | searched only |
| Emitter | `src/pp/emit.cc` | `UPDATE`/`DELETE` handlers; pass the landmark; refuse `WHERE CURRENT OF` | positioned refused loudly |
| ABI header | `include/esqlc.h` | `esqlc_stmt_exec` gains `table` | — |
| Contract | `specs/003-…/contracts/` | the same signature, same change (Principle V) | — |
| Runtime: context | `src/rt/context.c` | connect with `CLIENT_FOUND_ROWS` | — |
| Runtime: exec | `src/rt/exec.c` | input `is_null` binding; matched/altered split; landmark passthrough | `UPDATE` splits, others do not |
| Runtime: SQLSA | `src/rt/sqlsa.c` | accept an explicit table name alongside the metadata path | — |
| ABI stub | `tests/stub/esqlc_stub.c` | signature change | — |

Ten components, no new source files.

**Stubs that must fail loudly.** `WHERE CURRENT OF` in either statement is
refused with `ESQLC-1012` naming feature 004, exactly as `FOR UPDATE` is on the
cursor path — it is a clause, so the dispatch table cannot see it and the
handler must check. A multi-table `UPDATE` is *not* refused; it runs, and its
`table_name` reports SD-8's sentinel rather than a wrong name.

## 4. Runtime ABI surface

**No new entry points. One signature change.**

```c
/* `table` is the landmark identifier the scanner captured, or NULL when the
   statement's form yielded none. NULL means the SQLSA reports SD-8's character
   sentinel — never a guess. */
int esqlc_stmt_exec(const char *body, size_t body_len,
                    const esqlc_hostvar_t *vars, int var_count,
                    const char *table);
```

The contract marks this entry point **outline, not frozen**. It is still a
signature change: the contract, the header, and the stub move in this commit,
and `contract_sync` covers it.

## 5. Data structures

No new structure layouts, so Principle VI adds no obligation here beyond what
Gate 5 already asserts. `esqlc_hostvar_t` is unchanged — `ind_addr` has carried
the needed meaning since Gate 1 and this slice is the first to read it on the
input side.

`Construct` gains one field, alongside the two landmarks it already holds:

```cpp
std::string table;   // landmark identifier, empty when the form yielded none
```

## 6. Requirement → component map

| Requirement | Component(s) | Test |
|-------------|--------------|------|
| NFR-001.1 opaque bodies | emit | `opaque_body_unchanged` |
| FR-001.15 dispatch; unimplemented refused | dispatch | `negative/unimplemented_sqlda` |
| FR-002.15 indicator association | scan | `update_null_indicator` |
| FR-002.16 negative indicator means null | rt/exec | `rt/update_set_null` |
| FR-002.30 `width` bytes bound verbatim | rt/exec | `rt/update_char_verbatim` |
| FR-003.1 `esqlc_*` calls only | emit | `abi_only_symbols` |
| FR-003.2 no MariaDB type in the header | include/esqlc.h | `abi_isolation` |
| FR-003.3 signatures mirrored in the contract | contract | `contract_sync` |
| FR-003.10 values bound, never interpolated | emit | `update_placeholders` |
| NFR-003.2 no interpolation under any circumstance | emit | `rt/update_injection_literal` |
| FR-004.7 searched `UPDATE` | dispatch, emit | `rt/update_rows` |
| FR-004.8 `UPDATE` sets null via indicator | rt/exec | `rt/update_set_null` |
| FR-004.9 searched `DELETE` | dispatch, emit | `rt/delete_rows` |
| FR-004.10 zero rows sets `sqlcode` 100 | rt/context, rt/exec | `rt/update_zero_rows`, `rt/delete_zero_rows` |
| FR-005.1 `sqlcode` classes | rt/exec | `rt/update_zero_rows` |
| FR-005.17 `SQLSA` populated after DML | rt/exec, rt/sqlsa | `rt/dml_sqlsa_stats` |
| FR-005.22 `table_name` and `num_tables` | scan, rt/sqlsa | `rt/dml_table_name`, `rt/table_landmark_absent` |

**17 requirements, all mapped exactly once. Zero unmapped.**

## 7. Test strategy

**Tier 1.** The landmark is entirely testable without a server: three statement
forms yield their identifier, a `:name` inside a string literal is not mistaken
for one, and the hard forms yield nothing rather than something wrong. Plus the
`ESQLC-1012` refusal for `WHERE CURRENT OF` with code, line and column.

**Tier 2.** The row semantics need a real server, and the two that matter are
the ones no unit test can fake: a zero-row `UPDATE` reporting 100 with a
successful transport, and a **matched-but-unchanged** `UPDATE` reporting
`sqlcode` 0 with `records_used` 0. That second fixture is the whole argument for
`CLIENT_FOUND_ROWS`, and without it the split is untested.

**Mutation, Phase D′.** Drop `CLIENT_FOUND_ROWS` and the matched-but-unchanged
fixture must fail; take `records_used` from matched instead of changed and it
must also fail, in the other field; remove input `is_null` binding and
`update_set_null` must fail; return the landmark for a multi-table `UPDATE` and
`table_landmark_absent` must fail.

Two of those produce *plausible* wrong values rather than crashes, which is
where this project's guards have historically been decorative. The
matched-but-unchanged fixture is the one to write first and trust least.

## 8. Risks

**`mysql_info` is a formatted string, not an API.** Reading `Changed: M` from
`Rows matched: N  Changed: M  Warnings: W` is string parsing against a server
message. The format is long-standing and documented, but it is not a contract,
and a localised or reworded server build would break it silently — the parse
would find nothing and `records_used` would fall back. The fallback must be the
sentinel, never zero, or a parse failure becomes an untrue statistic. If this
proves unreliable, `records_used` for `UPDATE` becomes a `DIV-011` sentinel and
the slice says so.

**`CLIENT_FOUND_ROWS` is connection-wide and changes `affected_rows` for every
statement, not just `UPDATE`.** For `INSERT` and `DELETE` the two counts
coincide so nothing moves, but any future statement class inherits the changed
semantics whether or not its author knows. Registered as a divergence rather
than left as a connection flag someone finds later.

**The landmark is defeated silently.** A multi-table `UPDATE`, a leading
subquery, or a delimited identifier will yield *an* identifier — possibly a
keyword or a fragment — rather than nothing. Returning a wrong table name is
worse than returning none, because `table_name` looks authoritative. The
landmark must validate that what it read is a plain identifier and reject
anything else, and `table_landmark_absent` exists to pin that.

**`UPDATE` and `DELETE` reach the same `n_out == 0` path as `INSERT`.** That
path currently sets not-found on `affected_rows == 0` for *any* statement
without outputs. Under `CLIENT_FOUND_ROWS` an `INSERT` still reports its
inserted count, so `INSERT` is unaffected — but the shared path means a change
made for `UPDATE` lands on `INSERT` too, and Gate 1's fixtures are the only
thing standing between that and a regression. They must stay green.

## 9. Divergences introduced

**One new: `DIV-053` — `CLIENT_FOUND_ROWS` and the matched/altered split.**
SQL/MP's `sqlcode` 100 means no rows *found* (p.4-13) while `records_used`
counts rows *altered* (p.9-17). MariaDB reports changed-rows from
`affected_rows` by default, so the connection requests `CLIENT_FOUND_ROWS` to
make the `sqlcode` basis correct, and recovers the altered count from
`mysql_info` for `UPDATE`. Detection: a matched-but-unchanged `UPDATE` reports
`sqlcode` 0 and `records_used` 0. Migration: none for conforming programs; a
program that inferred "rows changed" from a zero `sqlcode` was already relying
on something SQL/MP does not promise.

**`DIV-011` gains a note**, not a new entry: `table_name` is now real on the DML
path via SD-9's landmark, so the character sentinel narrows to the statement
forms the landmark cannot read. Gate 5's report listed the whole DML path as
sentinel; that is no longer accurate.
