# Technical Plan: Gate 2 (retrieval)

**Slice:** `specs/gate-2.md` · **Status:** Draft
**Specs:** 001 (Ready), 002, 003, 004, 005 (Clarifying) — planned under Principle VIII

Preconditions verified: enumerated requirement subset ✓; avoidance table covering
every open question in all five touched specs, 23 in total ✓; SD-1 and SD-2
carried and marked provisional ✓; non-proof section ✓.

## 1. Approach

Extend rather than add. Gate 2 introduces **no new files** and, more usefully,
**no new ABI entry points** — the descriptor already carries `direction`, so
`esqlc_stmt_exec` can see `ESQLC_DIR_OUT` descriptors and behave as a singleton
select. That was the payoff of putting `direction` in the descriptor during
Gate 1 even though nothing used it.

Two mechanisms carry the slice.

**Direction inference by landmark, not by parsing.** The scanner already records
host-variable spans in its single pass; it now also records the byte offsets of
the `INTO` and `FROM` keywords when it meets them. A reference whose span falls
between them is an output; everything else is an input. This keeps `NFR-001.1`
intact — the body is still never parsed, we simply note two more landmarks
alongside the ones the lexer already finds. It is exactly the carve-out
`NFR-001.1` reserved for "constructs 004/006/007 explicitly claim", and its limit
is stated: two landmarks, not clause-structure analysis.

**Output binding driven by result metadata.** The runtime asks
`mysql_stmt_result_metadata` for the result shape, asserts the column count
equals the output-descriptor count, and binds positionally. Metadata is needed
anyway, so the cross-family check of `FR-002.22` falls out of it for free — a
numeric result column against a `CHAR_FIXED` descriptor is refused rather than
coerced.

`DIV-052` is implemented at connect by **appending** `PAD_CHAR_TO_FULL_LENGTH` to
the session `sql_mode`, never replacing it.

## 2. Alternatives rejected

| Alternative | Why rejected |
|-------------|--------------|
| A new `esqlc_stmt_select_into` entry point | The descriptor's `direction` field already expresses everything needed. Adding an entry point would grow the frozen ABI for no information gain, and Gate 3's cursors are where it genuinely must grow. |
| Infer direction from result metadata alone, no `INTO` recognition | Metadata tells you how many outputs exist, not *which* references they are. `WHERE part_num = :n` and `INTO :d` are indistinguishable without the landmark. |
| Parse the statement properly to classify clauses | Needs the SQL/MP grammar this project does not have. `NFR-001.1` exists to avoid owning one, and two landmarks are sufficient for the slice's forms. |
| Replace `sql_mode` wholesale with `PAD_CHAR_TO_FULL_LENGTH` | Clobbers every other mode the server or deployment set. `CONCAT(@@sql_mode, ',PAD_CHAR_TO_FULL_LENGTH')` preserves them. |
| Pad retrieved values in the runtime (`DIV-052` option 2) | More robust against a post-connect `sql_mode` override, but needs per-fetch column metadata handling for a case not yet observed. Held as the documented fallback; criterion 3 detects if it becomes necessary. |

## 3. Components

All extensions to existing files.

| Component | Path | Change | Slice scope |
|-----------|------|--------|-------------|
| Region scanner | `src/pp/scan.cc` | record `INTO` / `FROM` offsets in the same pass | landmarks only |
| Dispatcher | `src/pp/dispatch.cc` | `SELECT` moves from "owned by 004" to implemented | single-row only |
| Emitter | `src/pp/emit.cc` | `SELECT` handler; classify each reference `IN`/`OUT`; emit `ind_addr` | no cursors |
| Runtime: exec | `src/rt/exec.c` | result metadata, output bind, single fetch, indicators, cross-family check | singleton select |
| Runtime: context | `src/rt/context.c` | append `PAD_CHAR_TO_FULL_LENGTH` at connect | — |
| Runtime: diag | `src/rt/diag.c` | map "no indicator for a null output" to 8423 | — |
| Fixtures + harness | `tests/…` | four new fixtures, Tier 2 cases | — |

Six components touched, no new files. Out-of-slice statements — `UPDATE`,
`DELETE`, all cursor verbs, `WHENEVER`, the `INCLUDE` directives — keep their
`ESQLC-1012` handler entries naming the owning feature.

## 4. Runtime ABI surface

**No new entry points.** `esqlc_stmt_exec` gains behaviour, not signature: when
the descriptor array contains any `ESQLC_DIR_OUT` entry, it executes as a
singleton select, fetching at most one row.

Two descriptor fields move from declared-but-unused to load-bearing, both already
in the contract:

- `direction` — `ESQLC_DIR_OUT` selects the retrieval path.
- `ind_addr` — non-`NULL` means the program supplied an indicator; `NULL` means
  it did not, and a null column value is then an error rather than a silent zero.

The contract document needs a note that these are now live, but no signature
change. Recorded in the same change per Principle V.

## 5. Data structures

No SQL/MP structures are generated — no `SQLCA`, `SQLSA`, or `SQLDA` — so
Constitution VI imposes nothing new. `esqlc_hostvar_t` is unchanged, so its
existing `_Static_assert`s continue to hold and no new ones are required.

Worth stating explicitly because it is the slice's main limitation: Gate 2
touches none of the structures, which remain the project's largest untested
correctness exposure.

## 6. Requirement → component map

Every requirement in the scoped set, exactly once.

| Requirement | Component(s) | Test |
|-------------|--------------|------|
| FR-001.15 dispatch, unimplemented refused | dispatch | `negative/unimplemented_cursor` |
| FR-001.16 host-var references | scan | `select_hostvar_dirs` |
| FR-001.25 undeclared reference | emit | `negative/undeclared_into` |
| NFR-001.1 opaque body | scan | `opaque_body_select` |
| FR-002.15 `INDICATOR` keyword optional | scan, emit | `indicator_forms` |
| FR-002.16 `-1` null / `0` not null on output | rt/exec | `rt/null_indicator` |
| FR-002.22 no cross-family conversion | rt/exec | `rt/negative/cross_family` |
| FR-002.28 no terminator appended on retrieval | rt/exec | `rt/no_terminator` |
| FR-003.10 inputs parameterised | scan, emit, rt/exec | `rt/select_parameterised` |
| FR-003.12 output binding | rt/exec | `rt/select_into` |
| FR-003.13 `sqlcode` classes | rt/diag | `rt/sqlcode_notfound` |
| FR-004.1 values placed in listed host variables | rt/exec | `rt/select_into` |
| FR-004.2 no rows → 100, variables untouched | rt/exec, rt/diag | `rt/not_found_untouched` |
| FR-005.1 `sqlcode` semantics | rt/diag | `rt/sqlcode_notfound` |
| FR-005.2 not-found may arrive as 8423, not remapped to 100 | rt/diag | `rt/null_no_indicator` |
| `DIV-052` padding on retrieval | rt/context | `rt/char_padding_retrieval` |
| `ESQLC-4009` null with no indicator | rt/exec, rt/diag | `rt/null_no_indicator` |

15 scoped requirements, all mapped. No unmapped requirements — unlike Gate 1,
where building this map is what caught `FR-003.12` being untestable. That
lesson is why `FR-002.22` is here with a real negative test rather than assumed
to be exercised by the happy path.

## 7. Test strategy

**Tier 1** — golden plus spec assertions, no database. New assertions:
every reference in the `INTO` region carries `ESQLC_DIR_OUT` and every other
reference `ESQLC_DIR_IN`; `ind_addr` is the address of the declared indicator
where one was supplied and `0` where not; the emitted statement still contains
no `:` reference.

**Tier 2** — live, added to `run_tier2.sh`. Three cases carry the weight, and all
three work by **poisoning the host variable before the call** and asserting what
did and did not change:

- `no_terminator` — array pre-filled with a sentinel; after retrieval bytes
  0–17 are column data and **byte 18 still holds the sentinel**.
- `not_found_untouched` — every host variable pre-poisoned; after a no-row
  query all are byte-identical and `sqlcode` is exactly 100.
- `char_padding_retrieval` — an 18-byte value ending in blanks arrives as 18
  bytes, proving `DIV-052`.

Poisoning is the technique that makes "the runtime must not write here"
falsifiable; without it, a helpful write is invisible.

**Mutation checks**, following Gate 1's practice of proving guards are
load-bearing: null-terminating the fetch buffer must fail `no_terminator`;
binding on execute rather than fetch must fail `not_found_untouched`; swapping
two output descriptors must fail `select_into` (which is why the fixture selects
a `char` and a `short`, not two integers — a swap between same-typed columns
would be undetectable).

## 8. Risks

| Risk | Impact | Mitigation |
|------|--------|------------|
| `INSERT INTO parts` contains the `INTO` landmark. Naive landmark scanning would classify the table-name region of every `INSERT` as an output region and misbind Gate 1's own fixtures | Silent regression of a working path — the worst outcome available | Landmark classification is applied **only** by the `SELECT` handler, never by `INSERT`. Gate 1's fixtures stay in the suite precisely as the regression guard, and `select_hostvar_dirs` asserts `INSERT` references are all `DIR_IN` |
| `libmariadb` may null-terminate a `MYSQL_TYPE_STRING` fetch when `buffer_length` exceeds the value length, violating FR-002.28 invisibly | A terminator lands in the customer's array where SQL/MP would leave data; §2 p.2-7 is explicit that no terminator is appended | `buffer_length` set to exactly `width`, never `capacity`, so the library cannot reach byte 18. `no_terminator` asserts the sentinel survives. If the library still writes within `width`, that is a divergence to register, not to paper over |
| `mysql_stmt_fetch` or `execute` may touch output buffers even when no row is returned | FR-004.2 and criterion 5 broken silently; a program reading stale-looking values gets zeros instead | Poison-and-compare in `not_found_untouched`; fetch is only called after a row is known to exist |
| Result metadata column order versus descriptor order | A swap binds the wrong column to the wrong variable with no error at all | Assert `column_count == out_descriptor_count`; fixture deliberately mixes a `char` and a `short` so a swap is type-detectable, and the mutation check proves it |
| Appending to `@@sql_mode` at connect could be overridden by a later `SET` from an option file or a proxy | `DIV-052` silently reverts and retrieval truncates again | `char_padding_retrieval` runs against the real connection each time rather than checking the mode was set. Option 2 is the documented fallback if this fires |
| `SELECT` moving out of the `ESQLC-1012` table removes a guard that currently catches typos like `SELCT` | Slightly weaker refusal surface | `ESQLC-1009` (unrecognised keyword) already covers unknown keywords; `negative/unimplemented_cursor` keeps a live 1012 case in the suite |

## 9. Divergences introduced

None new. `DIV-052` moves from `proposed` to `accepted` when this slice lands,
implemented as option 1. `DIV-001` and `DIV-002` are inherited unchanged, and
slice decisions SD-1 and SD-2 carry forward still provisional.
