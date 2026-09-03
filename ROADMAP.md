# Roadmap

Eight features, four phases. Each phase has a gate — a demonstrable capability,
not a date.

**Phase entry rule, amended 2026-09-03.** A phase does not start until its gate
predecessor's exit criteria pass and `/speckit.analyze` is clean — *except* that
a phase may start when the predecessor's remaining criteria are blocked solely
on an **external dependency**, provided each blocked item is named here as a
standing debt with the dependency that blocks it.

The original rule assumed the external documents would arrive. Eight gates have
now routed around `SQLRM`, and Phase 2's gate cannot pass without it: positioned
`UPDATE`/`DELETE` and cursor stability need 004 Q3, the four conversion warnings
need `DIV-042`, and message rendering needs 005 Q6. Holding Phase 3 hostage to
documents that may never come is a worse outcome than a recorded gap, and it
would stall work that is fully specified and testable today.

This is the phase-level analogue of Constitution VIII, which lets a *slice*
proceed while its specs stay `Clarifying`. The conditions are deliberately the
same in spirit: the blockage must be external rather than a matter of unfinished
thinking, and it must be written down rather than assumed.

**It is not a licence to skip work that is merely hard.** A criterion blocked by
an undecided question belongs in a slice decision under Principle VIII, not
here. Only an unobtainable document qualifies.

### Phase 2's carried debt

| Blocked criterion | Dependency |
|---|---|
| Positioned `UPDATE`/`DELETE`; cursor stability | 004 Q3 — `SQLRM` |
| The four §2 conversion warnings | 002 Q1 / 005 Q4 — `DIV-042`, `SQLRM` |
| Message rendering (`SQLCADISPLAY`, `SQLSADISPLAY`) | 005 Q6 — the SQL message file |
| Transaction semantics outside / nested `BEGIN WORK` | 003 Q1, Q2 — `SQLRM` |

Three external documents are now outstanding: **`SQLRM`** (statement syntax and
semantics), **`CPG`** (the C compiler's pragma set and whether `fixed` is real),
and **`sqlh`** (the published character-set IDs — 002 Q7, raised by Gate 8).

```
Phase 1  ──▶ Phase 2  ──▶ Phase 3  ──▶ Phase 4
001,002,003   004,005      006,007      008
```

## Phase 1 — Something compiles and runs

| Feature | Deliverable |
|---|---|
| [001 Preprocessor core](specs/001-preprocessor-core/spec.md) | `EXEC SQL` scanning, placement enforcement, `#pragma SQL`, `#line` fidelity, C emission, listing output |
| [002 Host variables](specs/002-host-variables/spec.md) | Declare sections, full type mapping, indicators, `TYPE AS`, `SETSCALE` |
| [003 Runtime & MariaDB binding](specs/003-runtime-mariadb-binding/spec.md) | `esqlc_*` ABI, connection model, transaction control |

**Gate 1:** fully specified in [specs/gate-1.md](specs/gate-1.md) — a declare
section, `#pragma SQL`, `BEGIN WORK`, one `INSERT` with host variables, and
`COMMIT WORK` preprocesses, compiles against the ABI header with no MariaDB
header present, links, runs, and changes a MariaDB table. A `ROLLBACK WORK`
variant proves the transaction is real rather than autocommit in disguise.

**Status: ready to plan.** The slice touches exactly one open question across its
three specs — the `UNKNOWN` character-set corner of 002 Q4 — which is narrowed by
a recorded slice decision. Notably, 003 Q1 concerns statements *outside* a
transaction, so keeping explicit `BEGIN`/`COMMIT WORK` in the gate **avoids** it;
dropping them would make the gate riskier, not simpler. Proceeding under
Principle VIII while 002 and 003 remain `Clarifying`.

Rationale for this ordering: 003 before 004 because there is no way to test a
`SELECT … INTO` without a runtime, and no way to design the runtime ABI without
knowing what host variables look like (002). 001 and 002 are near-inseparable;
they are split because their test harnesses differ — 001 is golden-file, 002 is
type-table-driven.

## Phase 2 — Correct and observable

| Feature | Deliverable |
|---|---|
| [004 Static DML & cursors](specs/004-static-dml-cursors/spec.md) | Single/multirow `SELECT`, `INSERT`/`UPDATE`/`DELETE`, full cursor lifecycle, cursor position and stability |
| [005 Diagnostics](specs/005-diagnostics/spec.md) | `sqlcode`, `SQLCA`, `SQLSA`, `WHENEVER`, `INCLUDE STRUCTURES` versioning, the SQLCA/SQLSA access procedures |

**Gate 2 — retrieval**, specified in [specs/gate-2.md](specs/gate-2.md) and
**ready to plan**. Deliberately the smallest retrieval slice: single-row
`SELECT … INTO` by primary key, one indicator, and the not-found path. It
discharges the two requirements Gate 1 had to drop as untestable (`FR-002.28`,
`FR-003.12`) and resolves `DIV-052`. It touches only the two questions Gate 1
already narrowed, carrying slice decisions SD-1 and SD-2 unchanged.

**Gate 3 — read-only cursors**, specified in [specs/gate-3.md](specs/gate-3.md)
and **ready to plan**. `DECLARE CURSOR`, `OPEN`, a `FETCH` loop, `CLOSE`. No
`FOR UPDATE`, no positioned operations, no cursor stability.

Unlike Gates 1 and 2, this one collides with 004 rather than dodging it. Two
open questions are unavoidable: **Q9** — the `DECLARE … CURSOR` dispatch defect
Gate 2 found — must be *fixed*, since it is the slice's entry point; and **Q6**,
the cursor position after fetching past the last row, is structurally
unavoidable because that is how a loop terminates. Q6 is narrowed by slice
decision SD-3, Q9 is closed by repair.

It is also where the runtime ABI finally grows. Gates 1 and 2 added no entry
points; a cursor is long-lived state spanning three statements, which the
one-shot `esqlc_stmt_exec` cannot express. Gate 2's plan predicted this, and it
arrives where predicted.

**Gate 4 — `WHENEVER` and the SQLCA**, specified in
[specs/gate-4.md](specs/gate-4.md) and **ready to plan**. It finally attacks the
structures, which all three previous non-proof sections named as the project's
largest untested exposure.

Two things are new in kind: `WHENEVER` is the first *generated control flow*,
and the `SQLCA` is the first SQL/MP structure the project generates — layout is
API, since programs allocate copies with `SQLCA_LEN` and share them `EXTERNAL`.

**It is deliberately not positioned operations.** That slice is not cleanly
scopeable: 004 Q3 (cursor stability) cannot be narrowed the way Gate 3 narrowed
Q6. Q6 had a reading that is obviously safe; isolation levels have none, because
the question is what *other sessions* observe, and guessing there produces silent
anomalies under concurrency. It needs `SQLRM`. Positioned operations without
stability remain viable later — Q7 is Q6-shaped and `DIV-051` already carries a
documented choice.

**Gate 5 — the `SQLSA`**, specified in [specs/gate-5.md](specs/gate-5.md) and
**ready to plan**. The last SQL/MP structure, and the half of the structures
work Gate 4 left: the `SQLCA` had no published layout to conform to, and the
`SQLSA` has two.

It became scopeable when 005 Q2 resolved. The spec's original assessment — that
the structures could not be built byte-exactly from the manual alone — was
wrong for the `SQLSA`: §9 pp.9-15..9-16 publish both declarations, and the
arithmetic lands on 838 and 1790 exactly under packed alignment, which is
itself the proof that the inferred layout is the real one.

It is chosen over positioned operations for the same reason Gate 4 was: this
needs no `SQLRM` and they still do.

**Gate 6 — searched `UPDATE` and `DELETE`**, specified in
[specs/gate-6.md](specs/gate-6.md) and **ready to plan**. The smallest slice so
far, and the point where the subset stops being a demonstration: insert,
retrieve, iterate and modify.

Its value is not the two keywords, which the existing statement path mostly
already handles. It is that **input indicators have never been driven** — Gate 2
proved reading a null, never setting one — that a zero-row `UPDATE` is a
succeeded statement reporting `sqlcode` 100, which is the shape a runtime gets
silently wrong; and that it confronts the `table_name` gap Gate 5's report
raised rather than leaving it unowned for a third gate.

That gap turns out to have an answer that is not parsing. The table name sits at
a fixed position after the leading keyword in all three DML forms, which makes
it a **landmark** — the same same-pass position capture `INTO` and `FROM`
already use, and the same operation `name_after_verb` performs for cursor names.
Recorded as SD-9, provisional, with the forms it cannot read required to reach
the sentinel rather than a wrong name.

**Gate 7 — host variable type breadth**, specified in
[specs/gate-7.md](specs/gate-7.md) and **ready to plan**. Six gates in, the type
system is `char[]` and 16-bit `short`, so a program declaring `int`, `long
long`, `float` or a `VARCHAR` structure — most of them — still cannot compile.
The largest remaining work that needs no `SQLRM`.

Statements have had six gates of attention and types have had one. Both
type-mapping rows in [traceability.md](docs/traceability.md) carry the same
note: *16-bit only*.

Most of it costs nothing at the runtime. `esqlc_hostvar_t` has carried `width`,
`capacity` and the type-family constants since Gate 1 precisely so that widening
the type system would not move the interface, and `exec.c` already binds widths
2, 4 and 8. **It is the first slice since Gate 2 to add no ABI surface at all**,
and the work lands almost entirely in the declaration parser — the one component
no previous gate has had to grow.

Character sets, `DECIMAL`, `SETSCALE` and the four conversion warnings stay out,
on 002 Q4, Q2/Q3 and Q1 respectively.

**Gate 8 — character sets**, specified in [specs/gate-8.md](specs/gate-8.md)
and **ready to plan**. The first slice that closes a gap in shipped code rather
than adding capability: Gate 7 put `VARCHAR` and `char` binding into `main`
while explicitly not knowing whether `len` counts bytes or characters, and
sidestepped it by keeping every fixture single-byte. That avoidance is honest
and it is also latent wrongness, unproven for exactly the programs where it
decides whether data survives.

002 Q4 is the last open question that needs no external document — the spec says
*"likely a new divergence"*, not *"needs `SQLRM`"*.

Two findings shape it. **`len` counts bytes**, derived rather than decided:
p.2-20's `VARCHAR (10) CHARACTER SET KANJI` becomes `val[11]` at p.2-22, which
is FR-002.6's `l+1` with `l = 10`, so `VARCHAR(n)` is n bytes and a double-byte
set simply encodes fewer characters in them. And **`KANJI` is refused rather
than mapped**: MariaDB offers `sjis`, `cp932`, `ujis` and `eucjpms`, differing
in byte length and repertoire, and "KANJI" names a script rather than an
encoding. A wrong choice does not fail, it silently stores different characters
than the program wrote — the least detectable error this project can produce.

`KSC5601` → `euckr` is the multibyte mapping it does implement, defensible
because KS C 5601 is the character set and EUC-KR its encoding rather than two
guesses at one script. That is what lets the slice settle `len` at all.

**Gate 8 also closes SD-1**, carried provisionally through all seven previous
gates: p.2-24 calls `UNKNOWN` *"an unknown single-byte character set"*
*"equivalent to omitting the CHARACTER SET clause"*, so the connection default
is the faithful reading and not a convenience.

**The full Phase 2 gate** additionally needs positioned `UPDATE`/`DELETE`,
cursor stability, message rendering, and the four mandatory conversion warnings
from §2. None of the five gates proves any of that — see each one's non-proof
section. Three of the four are blocked on `SQLRM`, which is now the single
highest-leverage thing outstanding on the project: one document unblocks 003
Q1/Q2, 004 Q2/Q3, and `DIV-042` together.

004 and 005 are concurrent and mutually dependent — cursor tests need `sqlcode`,
and `SQLSA` statistics need cursor operations to populate them. Run them as one
work item with two specs if the team is small.

## Phase 3 — Real applications

| Feature | Deliverable |
|---|---|
| [006 INVOKE](specs/006-invoke-schema-gen/spec.md) | Schema-derived structure generation, indicator arrays, `CHAR_AS_STRING`/`CHAR_AS_ARRAY` |
| [007 Dynamic SQL](specs/007-dynamic-sql/spec.md) | `PREPARE`/`EXECUTE`/`DESCRIBE`, `SQLDA` + names + collation buffers, dynamic cursors, legacy v1/v2 descriptors |

**Gate 9 — `INVOKE`**, specified in [specs/gate-9.md](specs/gate-9.md) and
**ready to plan**. Entered under the amended phase rule above, with Phase 2's
debt recorded there. The first slice where the preprocessor reads something
other than source, and the first where a structure a customer program uses is
*generated* rather than hand-written and inspected. Eight gates taught the
preprocessor to read declarations; this one makes it write them.

The architectural crux is already answered by the spec. FR-006.2e wants read
access to the invoked object at preprocess time; NFR-001.2 forbids the
preprocessor depending on MariaDB at all. NFR-006.2's **optional-by-cache**
resolves it: the preprocessor reads a committed JSON cache and never opens a
socket, so its dependency set does not change. 006 Q4 — the cache's format and
invalidation — is explicitly *"a build-reproducibility decision, not a manual
question"*, so it is ours to make: SD-15 commits the cache, SD-16 says the
preprocessor cannot detect a stale one and does not pretend to.

**It also closes Gate 8's gap from the other end.** Gate 8 could not check a
host variable's declared character set against its column, because result
metadata reports the result set's charset rather than the column's. FR-006.2b
has `INVOKE` emit the `CHARACTER SET` clause from the cached definition, so a
*generated* declaration cannot disagree with its column — there is nothing to
check because there is nothing to disagree. Hand-written declarations stay
exposed, and `DIV-055` is narrowed rather than closed.

Everything `INVOKE` generates is then read back by the parsers Gates 7 and 8
built, which makes those gates the test of what this one emits — one path to be
right rather than two.

**Gate 3:** a nontrivial application — a dynamic SQL query tool over the App. A
sample database, of the shape §10 develops — works end to end.

006 before or alongside 007: `INVOKE` is what makes the conformance fixtures
practical to write, and dynamic SQL is the largest single feature in the manual.

## Phase 4 — The honest edges

| Feature | Deliverable |
|---|---|
| [008 NonStop compatibility surface](specs/008-nonstop-compat-surface/spec.md) | Guardian naming, TACL DEFINEs, DDL/DCL/DSL policy, PAID, VSBB, program invalidation, CPRL, version management, `SQLMEM`, Pathway, App. B/C |

**Gate 4:** every construct in [nonstop-specifics.md](docs/reference/nonstop-specifics.md)
has a stated policy and a test proving that policy fires. No construct reaches
the runtime undecided.

008 is last by necessity and not by importance. It is the feature that decides
whether a customer's program is *rejected clearly* or *accepted wrongly*, and it
is deliberately scheduled after the core works so that the policies are chosen
with real code in front of the team rather than from the manual alone.

## Standing risks

| Risk | Phase | Note |
|---|---|---|
| `DIV-002` TACL DEFINEs unresolved | 1 | Blocks fixtures transcribed from manual examples; 001 must avoid `=name` or resolve it early |
| Structure layout guesswork | 2 | The manual gives field names and total sizes but not offsets. Sizes must be hit exactly; field order is inferred and needs validation against real NonStop output if any is obtainable |
| SQLCA field-level content | 2 | Documented only via `SQLCAGETINFOLIST` item codes (§5), never as a field list. 005 is working from indirect evidence |
| `SQLRM` deferrals | 3 | Statement syntax is out-of-manual. 001's opaque-token-stream approach contains this, but `DECLARE CURSOR` and `SELECT … INTO` still need real parsing |
| CPRL scope | 4 | 22 procedures over collation objects that MariaDB does not have. Partial implementation may be worse than none |
