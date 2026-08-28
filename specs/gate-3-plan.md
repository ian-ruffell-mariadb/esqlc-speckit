# Technical Plan: Gate 3 (read-only cursors)

**Slice:** `specs/gate-3.md` · **Status:** Draft
**Specs:** 001 (Ready), 002, 003, 004, 005 (Clarifying) — planned under Principle VIII

Preconditions verified: enumerated requirement subset ✓; avoidance table covering
every open question in all five touched specs, 24 in total ✓; SD-1, SD-2 and the
new SD-3 marked provisional ✓; non-proof section ✓.

## 1. Approach

Three mechanisms, all new. This is the first slice that adds genuinely novel
machinery rather than extending what exists.

**A cursor registry in the preprocessor.** `DECLARE <name> CURSOR FOR <select>`
sits in declaration position and carries the statement text; `OPEN <name>` sits
in executable position and is where that text must run. The emitter keeps a
name → `{sql, input reference names}` map, populated at `DECLARE` and consulted
at `OPEN`. The SQL itself is emitted **at the `DECLARE` site** as a
`static const char[]`, because that is where the programmer wrote it and where a
reader will look for it; only the descriptor array is built at `OPEN`, since that
is where host variable addresses can be taken.

That placement also makes the binding time correct without extra effort:
`:min_num` is read when `OPEN` executes, not when `DECLARE` is compiled, which is
what FR-004.12 requires.

**Name-based cursor identity in the ABI.** The three new entry points take the
cursor name as a string. The alternative — an opaque handle returned by `open`
and stored in a generated static — puts more state in emitted code for no gain
at this scale, and the cursor namespace is already per-program in the source
language. Lookup is a short linear scan; with the single-digit cursor counts real
programs use, that is not worth optimising.

**Server-side cursors, not client buffering.** `mysql_stmt_execute` materialises
the whole result set by default, which is not what a SQL/MP cursor does and would
turn a scan of a large table into a memory event. The runtime sets
`STMT_ATTR_CURSOR_TYPE` to `CURSOR_TYPE_READ_ONLY` so rows stream. See risks —
if that proves unavailable, it is a divergence to register, not to absorb
silently.

The **Q9 fix** is separate and small: `leading_keyword` learns that a body
beginning `DECLARE` and containing `CURSOR` before `FOR` is a cursor
declaration, whatever identifier sits between the two words.

## 2. Alternatives rejected

| Alternative | Why rejected |
|-------------|--------------|
| Opaque cursor handle returned by `open`, stored in a generated static | More generated state, a new lifetime to reason about, and no benefit at single-digit cursor counts. Name-based matches the source model directly. |
| Emit the cursor SQL at the `OPEN` site | Puts the text somewhere the programmer did not write it, and duplicates it if a cursor is opened twice in different places. |
| Re-`prepare` the statement on every `OPEN` | Correct but wasteful, and it would hide the fact that `DECLARE` is a declaration. Prepare once at first `OPEN`, reuse thereafter. |
| Client-side buffering (`mysql_stmt_store_result`) | Simpler, and wrong: SQL/MP cursors stream, and a full-table scan would materialise the table in the client. |
| Fix Q9 by generalising the keyword matcher to arbitrary wildcards | Over-engineering for one construct. A targeted `DECLARE … CURSOR` rule is honest about being one special case. |

## 3. Components

| Component | Path | Change | Slice scope |
|-----------|------|--------|-------------|
| Region scanner | `src/pp/scan.cc` | none — `INTO` landmark already handles `FETCH … INTO`, which a test pins | verify only |
| Keyword matcher | `src/pp/scan.cc` | `DECLARE … CURSOR` recognition — the Q9 fix | one special case |
| Dispatcher | `src/pp/dispatch.cc` | `DECLARE CURSOR`, `OPEN`, `FETCH`, `CLOSE` become implemented | read-only forms |
| Cursor registry | `src/pp/emit.cc` | name → sql + input names, populated at `DECLARE` | new |
| Emitter | `src/pp/emit.cc` | emit cursor SQL at `DECLARE`; descriptors and calls at `OPEN`/`FETCH`/`CLOSE` | new |
| Runtime: cursors | `src/rt/cursor.c` | **new file** — cursor table, open/fetch/close, exhausted state | read-only |
| Runtime: txn | `src/rt/txn.c` | commit and rollback close open cursors (FR-003.8) | — |
| ABI header | `include/esqlc.h` | three new entry points | — |
| Contract | `specs/003-…/contracts/` | the same three, same change (Principle V) | — |

Nine components, one new source file. Out-of-slice cursor forms — `FOR UPDATE`,
positioned `UPDATE`/`DELETE` — keep `ESQLC-1012` entries naming feature 004.

## 4. Runtime ABI surface

**Three new entry points.** Gates 1 and 2 added none; a cursor is long-lived
state spanning three statements, which one-shot `esqlc_stmt_exec` cannot express.

```c
/* Execute the cursor's statement and position before the first row.
   `sql` is the text captured at DECLARE; `vars` are its input references,
   read now rather than at declaration time (FR-004.12). */
int esqlc_cursor_open(const char *name,
                      const char *sql, size_t sql_len,
                      const esqlc_hostvar_t *vars, int var_count);

/* Advance one row and write the output descriptors. sqlcode 100 at end of set;
   host variables untouched in that case (FR-004.14). Idempotent once
   exhausted, per slice decision SD-3. */
int esqlc_cursor_fetch(const char *name,
                       const esqlc_hostvar_t *vars, int var_count);

/* Terminate the cursor and release its result set (FR-004.15). */
int esqlc_cursor_close(const char *name);
```

All three land in the 003 contract in this same change. No existing signature
changes; `esqlc_hostvar_t` is reused unaltered, so `direction` and `ind_addr`
carry their Gate 2 meanings on the cursor path too.

## 5. Data structures

No SQL/MP structures are generated — still no `SQLCA`, `SQLSA`, or `SQLDA` — so
Constitution VI adds no new obligations, and `esqlc_hostvar_t`'s existing
assertions continue to hold unchanged.

The runtime's cursor table is internal, never crosses the ABI, and is therefore
not layout-sensitive. It holds per cursor: name, `MYSQL_STMT *`, and a state of
`declared` / `open` / `exhausted`. The `exhausted` state is what makes SD-3
cheap — a fetch in that state returns 100 without touching the server.

## 6. Requirement → component map

Every requirement in the scoped set, exactly once.

| Requirement | Component(s) | Test |
|-------------|--------------|------|
| FR-001.15 dispatch; unimplemented refused | dispatch | `negative/unimplemented_for_update` |
| FR-001.16 host-var references in cursor statements | scan, emit | `cursor_hostvar_dirs` |
| FR-001.25 undeclared reference | emit | `negative/undeclared_in_fetch` |
| NFR-001.1 opaque bodies; `INTO` landmark extends to `FETCH` | scan | `fetch_into_landmark` |
| FR-002.28 no terminator appended on a fetch | rt/cursor | `rt/cursor_no_terminator` |
| FR-003.1 `esqlc_*` calls only | emit | `abi_only_symbols` |
| FR-003.2 no MariaDB type in the ABI header | include/esqlc.h | `abi_isolation` |
| FR-003.3 signatures mirrored in the contract | contract | `contract_sync` |
| FR-003.8 commit and rollback free cursors | rt/txn | `rt/commit_frees_cursor` |
| FR-004.11 `DECLARE CURSOR` associates name with statement | emit registry | `cursor_declare_registry` |
| FR-004.12 `OPEN` runs the statement, positions before first row | rt/cursor | `rt/open_binds_at_open` |
| FR-004.13 `FETCH` advances and copies | rt/cursor | `rt/cursor_loop` |
| FR-004.14 fetch past last: 100, variables unmodified | rt/cursor | `rt/fetch_exhausted` |
| FR-004.15 `CLOSE` terminates and frees | rt/cursor | `rt/close_then_fetch` |
| FR-004.16 specified position-table rows | rt/cursor | `rt/cursor_loop` |
| FR-004.16b order undefined without `ORDER BY` | test discipline | `fixture_review` — every ordered assertion names `ORDER BY` |
| FR-004.19 out-of-order operations are errors | rt/cursor | `rt/negative/cursor_order` |
| FR-005.1 `sqlcode` classes | rt/diag | `rt/fetch_exhausted` |

18 scoped requirements, all mapped. FR-004.16b is a discipline rather than code,
and is mapped to a review check rather than pretended into an implementation
task — Gate 1's lesson about requirements that cannot be tested where they sit.

## 7. Test strategy

**Tier 1** — golden and spec assertions, no database. New assertions: the
cursor's SQL appears once, at the `DECLARE` site, as a `static const`; `OPEN`
emits only input descriptors; `FETCH` emits only output descriptors and no SQL
text at all; every reference in a `FETCH … INTO` is `DIR_OUT`, which is the
T285 question answered by test rather than assumption.

**Tier 2** — live. The load-bearing cases, all using the poison convention:

- `cursor_loop` — every seeded row exactly once in `ORDER BY` order, and the
  sentinel byte survives *every* fetch, not just the last.
- `fetch_exhausted` — 100 at end of set, host variables untouched, and a second
  fetch also 100 and still untouched (SD-3).
- `open_binds_at_open` — change the input host variable *between* `DECLARE` and
  `OPEN` and confirm the new value is used. This is the only test that
  distinguishes correct binding time from the plausible wrong one.
- `commit_frees_cursor` — commit with a cursor still open, then fetch: an error,
  not a stale row.

**Mutation checks**, as in Gate 2:

| Injected defect | Must fail |
|---|---|
| Bind cursor inputs at `DECLARE` instead of `OPEN` | `open_binds_at_open` |
| Return the last row again instead of 100 when exhausted | `fetch_exhausted` |
| Null-terminate the fetch buffer | `cursor_no_terminator` |
| Client-buffer the result instead of streaming | none — see risks; this is why it needs a *non-test* check |

That last row is deliberately honest: no functional test distinguishes a
streamed cursor from a buffered one, which is exactly why the risk table treats
it as an assertion about the connection rather than an outcome.

## 8. Risks

| Risk | Impact | Mitigation |
|------|--------|------------|
| `mysql_stmt_execute` materialises the whole result set by default. A SQL/MP cursor streams, so a scan of a large table becomes a client-side memory event with no functional symptom at test scale | Passes every Gate 3 test on a five-row fixture and falls over on a customer's real table | Set `STMT_ATTR_CURSOR_TYPE` to `CURSOR_TYPE_READ_ONLY` at open, and **assert the attribute was accepted** rather than inferring it from behaviour. If the server or library refuses it, register a divergence — do not absorb it |
| Name-based cursor identity pre-judges 004 Q5 (are cursors scoped to the unit or the function?). Two units declaring the same name would collide in one runtime table | An ABI decision quietly answering an open specification question | Recorded here explicitly. The slice uses one cursor so nothing is exercised, but if Q5 resolves to function scope the ABI needs a scope qualifier. Noted as a Gate 4 input, not assumed away |
| `mysql_stmt_bind_result` called per `FETCH` rather than once after execute | Wasted work, or a library that rejects rebinding mid-result | Bind once at first fetch and cache; assert the descriptor shape is identical on subsequent fetches and error if a program varies its `INTO` list mid-cursor |
| The Q9 matcher could over-match a body beginning `DECLARE` that is not a cursor declaration | A non-cursor statement silently dispatched to the cursor handler | The rule requires `CURSOR` to appear before `FOR`, not merely somewhere. SQL/MP has no other `DECLARE`-initial embedded statement, and `BEGIN`/`END DECLARE SECTION` start with different words |
| A declared cursor that is never opened leaves an unused `static const` | Compiler warning noise in customer builds, which erodes trust in the output | Emit the SQL with a suppression attribute, or reference it from the registry entry; decided in implementation, tested by compiling a fixture that declares and never opens |
| `CLOSE` then `OPEN` again (legal, and how a program re-runs a query) | If close destroys the prepared statement, the second open silently re-prepares, changing timing but not results | Explicitly supported: close releases the result set, keeps the prepared statement, returns the cursor to `declared`. Tested by `close_then_reopen` |

## 9. Divergences introduced

None planned. One candidate: if `CURSOR_TYPE_READ_ONLY` cannot be set, the
cursor is client-buffered rather than streamed, which diverges from SQL/MP's
behaviour on large result sets and must be registered rather than accepted
quietly. The plan's mitigation is to detect that at open, not to discover it in
production.

SD-1, SD-2 and SD-3 carry as provisional slice decisions; `DIV-001`, `DIV-002`
and `DIV-052` are inherited unchanged.
