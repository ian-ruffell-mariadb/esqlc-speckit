# esqlc-speckit

**ESQL/C for MariaDB** — an embedded-SQL C preprocessor and runtime library that
aims to be source-compatible with HP NonStop SQL/MP's embedded SQL interface for
C, so that an existing `.sqlc` program recompiles and runs against MariaDB.

The behavioural contract is the manual:

> *HP NonStop SQL/MP Programming Manual for C*, part number 429847-008,
> August 2012 (331 pp.) — see [docs/source-manual.md](docs/source-manual.md).

The manual is **not vendored** (HP copyright), and neither is ISO/IEC 9075-5.
Fetch them locally with `./.specify/scripts/fetch-manual.sh`. Everything in this
repo works without them except the citation checker, which skips.

## Status

Ten vertical slices are merged. There is a working preprocessor (`esqlcpp`) and
runtime (`libesqlc.a`) — about 5,200 lines — exercised by 123 fixtures and 287
assertions across three suites.

**What compiles and runs today**, end to end against a live MariaDB:

| | |
|---|---|
| Declare sections | `char[]`, `short`/`int`/`long`/`long long`, `float`/`double`, VARCHAR structs, indicators, C comments, `CHARACTER SET` clauses |
| Transactions | `BEGIN WORK`, `COMMIT WORK`, `ROLLBACK WORK` |
| Static DML | `INSERT`, single-row `SELECT … INTO`, searched `UPDATE`, searched `DELETE` |
| Cursors | `DECLARE`/`OPEN`/`FETCH`/`CLOSE`, read-only |
| Diagnostics | `sqlcode`, `WHENEVER`, `INCLUDE SQLCA`, `INCLUDE SQLSA`, `INCLUDE STRUCTURES` version selection |
| Schema generation | `INVOKE` (tagged records, indicator arrays) |
| Dynamic SQL | `INCLUDE SQLDA`, `PREPARE`, `DESCRIBE`, `EXECUTE` |

The slices, in the order they were built and merged — each is a spec, a plan, a
task list, and a stated non-proof:

[1](specs/gate-1.md) first vertical slice ·
[2](specs/gate-2.md) retrieval ·
[3](specs/gate-3.md) read-only cursors ·
[4](specs/gate-4.md) `WHENEVER` and the `SQLCA` ·
[5](specs/gate-5.md) the `SQLSA` ·
[6](specs/gate-6.md) searched `UPDATE`/`DELETE` ·
[7](specs/gate-7.md) host-variable type breadth ·
[8](specs/gate-8.md) character sets ·
[9](specs/gate-9.md) `INVOKE` ·
[10](specs/gate-10.md) the `SQLDA`

### What is *not* done

Being explicit about this is a project principle, not modesty. Of 239
specified requirements, 16 are proven by tests, 23 are partially covered, and 59
are specified but unbuilt (see [docs/traceability.md](docs/traceability.md)).
Notably absent: positioned `UPDATE`/`DELETE` and `WHERE CURRENT OF`, cursor
stability modes, `EXECUTE IMMEDIATE`, dynamic cursors, DDL/DCL, the CPRL
collation procedures, message rendering (`SQLCADISPLAY`), and the whole of
feature 008 (Guardian naming, TACL DEFINEs, PAID, VSBB, version management).

91 diagnostics are registered; 49 are emitted, 5 are unreachable **by design**
(they describe conditions this design cannot produce, and say so), and 37 belong
to slices not yet built. `tests/harness/diag_registry.sh` fails if a diagnostic
marked unreachable is nonetheless emitted.

### Three external blockers

Work is genuinely blocked — not merely hard — on three documents this repo does
not have. Each blocked item is a named standing debt in
[ROADMAP.md](ROADMAP.md), per the phase-entry rule amended 2026-09-03.

| Document | Blocks |
|---|---|
| `SQLRM` (SQL/MP Reference Manual) | Statement syntax and semantics: transaction nesting (003 Q1/Q2), cursor stability (004 Q3), `DIV-042` |
| `CPG` (C compiler pragma set) | Whether NonStop C's `fixed` type is real (002 Q3) — decides `ESQLC_T_DECIMAL` |
| `sqlh` (published header) | The character-set ID values the `SQLDA` carries (002 Q7) |

## Divergences

19 places where this implementation cannot match SQL/MP and says so out loud,
each with a rationale, a detection method, and a migration note:
[docs/divergences.md](docs/divergences.md). The load-bearing ones are `DIV-040`
(the `sqlvar` widens from 24 to 40 bytes for 64-bit pointers), `DIV-041` (the
`SQLCA` layout stays private because the manual publishes none), and `DIV-055`
(character-set handling binds byte-verbatim via `character_set_client=binary`).

## Build and test

Needs CMake, a C11/C++17 toolchain, and MariaDB Connector/C.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
```

Tier 1 needs no database — that is [NFR-001.2], and it is enforced:

```bash
ctest --test-dir build
```

Tier 2 needs a live server. Bring up an ephemeral one on a non-default port and
point the suite at it:

```bash
ESQLC_PORT=13306 ESQLC_DATABASE=esqlc_gate1 tests/harness/run_tier2.sh build/esqlcpp build/libesqlc.a
```

## Layout

```
.specify/memory/constitution.md    Eight non-negotiable principles
.specify/templates/                spec / plan / tasks templates
.claude/commands/                  /speckit.* slash commands
include/esqlc.h                    The frozen ABI — no MariaDB symbol may appear
src/pp/                            Preprocessor: scan, dispatch, decl, emit, sqlda
src/rt/                            Runtime: context, exec, cursor, sqlca, sqlsa, dynamic
tests/conformance/gate-N/          Fixtures: positive, negative, tier-2
tests/harness/                     Suite drivers and the structural audits
docs/source-manual.md              Provenance, citation convention, re-extraction
docs/reference/                    Distilled normative reference sheets
docs/traceability.md               Manual section -> spec coverage, 99 rows
docs/divergences.md                The 19 accepted divergences
docs/status.md                     Consolidated status: what works, what is owed
specs/NNN-*/                       Feature specs, plans, tasks
specs/gate-N*.md                   The ten implemented slices
ROADMAP.md                         Phase order, gating, standing debts
```

## The eight features

| # | Feature | Manual coverage |
|---|---------|-----------------|
| [001](specs/001-preprocessor-core/spec.md) | Preprocessor core & pipeline | §3, §6 (pragma/placement/listings) |
| [002](specs/002-host-variables/spec.md) | Declare sections & host-variable type mapping | §2, App. D |
| [003](specs/003-runtime-mariadb-binding/spec.md) | Runtime library & MariaDB binding | §7, transaction control in §3 |
| [004](specs/004-static-dml-cursors/spec.md) | Static DML & cursors | §4 |
| [005](specs/005-diagnostics/spec.md) | `sqlcode`, `SQLCA`, `SQLSA`, `WHENEVER` | §9, §5 |
| [006](specs/006-invoke-schema-gen/spec.md) | `INVOKE` schema-derived structures | §2 (INVOKE), App. A |
| [007](specs/007-dynamic-sql/spec.md) | Dynamic SQL & `SQLDA` | §10, App. D |
| [008](specs/008-nonstop-compat-surface/spec.md) | NonStop compatibility surface | §5, §6, §7, §8, §11, App. B, App. C |

## How work is done here

The eight principles are in [the constitution](.specify/memory/constitution.md).
The four that shape day-to-day work most:

- **I — the manual is the contract.** Every requirement carries a
  `[SQLPM/C §n p.n-n]` citation, an `[EXTERNAL]` marker, or a `[DIV-nnn]`.
  Uncited requirements are rejected at `/speckit.analyze`.
- **III — no silent semantic change.** Anything that cannot be emulated
  faithfully must refuse loudly. "Fails loudly where it cannot" is a
  first-class requirement, not a fallback.
- **IV — tests first.** A Phase C task names the Phase B test it makes pass, and
  that test must have failed for the right reason first. Guards are proven
  load-bearing by mutation testing, not by inspection.
- **VIII — scoped slices.** A slice may be built against an unresolved spec only
  if it enumerates its requirement IDs, tables every open question it avoids,
  marks its decisions provisional, and states what it does not prove.

```bash
./.specify/scripts/new-feature.sh "short feature name"
```

```
/speckit.specify   # fill in spec.md from the manual, cite every requirement
/speckit.plan      # derive plan.md
/speckit.tasks     # derive tasks.md
/speckit.analyze   # cross-check spec/plan/tasks against the constitution
/speckit.implement # execute tasks.md
```
