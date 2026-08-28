# esqlc-speckit Constitution

Version 1.1.0 · Ratified 2026-08-27 · Amended 2026-08-28

These principles govern every spec, plan, and task in this repo. They are
non-negotiable. A spec that violates one is rejected at review, not fixed
downstream.

---

## I. The manual is the contract

Every functional requirement traces to a cited location in *HP NonStop SQL/MP
Programming Manual for C* (429847-008).

- Citation format: `[SQLPM/C §9 p.9-6]` — section number and the manual's own
  page label, not the PDF page index.
- A requirement with no citation is either (a) a divergence, which must be
  registered in [docs/divergences.md](../../docs/divergences.md) with an ID, or
  (b) out of scope. There is no third case.
- Where the manual defers to another HP manual (`SQLRM`, `C/C++ Programmer's
  Guide`, `SQL/MP Version Management Guide`), the deferral itself is the
  citation and the requirement is marked `[EXTERNAL]`. External requirements
  must be resolved to a concrete decision before their task is implementable.

**Rationale:** the manual is the only artefact both this project and the
existing NonStop programs agree on. Guessing at semantics produces a
preprocessor that compiles customer code and returns wrong answers, which is
strictly worse than one that refuses to compile it.

## II. Source compatibility over elegance

Existing ESQL/C source compiles unmodified, or the divergence is registered.

- No new **required** syntax. Extensions must be opt-in (pragma, CLI flag, or
  configuration file) and must never be needed to preprocess conforming source.
- The preprocessor accepts the manual's coding rules as-is: `EXEC SQL` … `;`
  delimiters, SQL-only comments (`--`) inside embedded statements, `"` as the
  only string delimiter, statements spanning arbitrary line counts, and no
  nesting. [SQLPM/C §3 pp.3-1..3-2]
- Identifier and structure names that the manual specifies as generated
  (`sqlcode`, `SQLCA_LEN`, `SQLDA_SQLVAR_LEN`, `struct { short len; char val[]; }`
  for VARCHAR, …) keep exactly those names and values.

**Rationale:** the entire value of the project is that customers do not rewrite
their applications. A preprocessor that requires a source sweep has no
advantage over a hand port.

## III. No silent semantic change

Anything that cannot be emulated faithfully produces a **diagnostic**, never a
plausible wrong answer.

- Three policies per unsupported construct, selectable per-construct:
  `error` (default), `warn` (emit + continue with documented behaviour),
  `ignore` (accept as no-op). `ignore` is never the default for anything that
  can change a query result, a lock, or a transaction boundary.
- Guardian-specific constructs (`\node.$vol.subvol.file` names, `=defines`,
  TACL DEFINEs, `CONTROL TABLE`, `LOCK TABLE`, VSBB, PAID checks) are handled
  under this principle, not ad-hoc.
- Silent truncation, silent scale loss, and silent character-set coercion are
  prohibited. Where the manual specifies a warning (e.g. right-truncation of a
  too-short receiving string, fixed-point to floating-point precision loss,
  fixed-point to integer fraction loss) the runtime must return the
  corresponding warning code. [SQLPM/C §2 pp.2-5, 2-11]

**Rationale:** a wrong row is undetectable in production; a failed compile is
detected in seconds.

## IV. Manual-derived tests, written first

Each requirement ships with an executable test derived from the manual before
the implementing code exists.

- **Golden-file preprocessor tests**: input `.sqlc` → expected emitted C. Assert
  on structure and emitted runtime calls, not on incidental formatting.
- **Runtime conformance tests**: run against a live MariaDB using the sample
  database of App. A, asserting `sqlcode`, `SQLCA`, and `SQLSA` contents.
- **Negative tests**: every diagnostic from Principle III has a test that proves
  it fires.
- Where the manual contains a worked example, the conformance suite carries an
  equivalent test case authored for this project — reimplemented, not copied.

**Rationale:** the manual's examples are the closest thing available to an
acceptance suite for a system nobody here can run.

## V. Layered architecture with a frozen runtime ABI

The preprocessor emits calls into a documented runtime ABI and nothing else.

- Generated C contains **no** MariaDB client API calls, no `#include <mysql.h>`,
  and no MariaDB type names. It calls `esqlc_*` runtime entry points only.
- The runtime ABI is versioned and specified in
  `specs/003-runtime-mariadb-binding/contracts/`. Changing it is a
  minor-version event with a migration note.
- Consequence: the preprocessor is testable with a stub runtime and no database;
  the runtime is testable without the preprocessor.

**Rationale:** the two halves have completely different failure modes and test
harnesses. Coupling them makes both untestable.

## VI. Structure layouts are byte-exact where observable

`SQLCA`, `SQLSA`, and `SQLDA` are laid out to the sizes and field names the
manual specifies, per version.

- Documented lengths are assertions in code, not comments:
  `SQLCA_LEN` = 430; `SQLSA_LEN` = 838 (versions 300–325) or 1790 (version 330+);
  `SQLDA_HEADER_LEN` = 4; `SQLDA_SQLVAR_LEN` = 24;
  `SQLDA_NAMESBUF_OVHD_LEN` = 11. [SQLPM/C §9 pp.9-12, 9-14; §10 p.10-5]
- Eye-catcher values (`CA`, `SA`, `D1`) and the requirement that the *program*
  initialises them are preserved. [SQLPM/C §9 p.9-12; §10 p.10-5]
- `INCLUDE STRUCTURES` version selection (1, 2, 300, 315, 330 SQLSA-only, 340+,
  and `SQLSA VERSION CURRENT`) is honoured, including the default-to-version-2
  behaviour and its informational message when the directive is absent.
  [SQLPM/C §9 pp.9-1..9-3]
- Static assertions on `sizeof` and `offsetof` are mandatory for every generated
  structure version.

**Rationale:** customer code indexes into these structures, copies them, shares
them `EXTERNAL` across modules, and allocates extra ones using the length
constants. Layout is API.

## VII. Divergence is registered, never discovered

Every intentional difference from the manual gets an entry in
[docs/divergences.md](../../docs/divergences.md) before the code that causes it
merges.

Each entry carries: ID (`DIV-nnn`), manual citation, what NonStop does, what
this implementation does, why, how a program can detect the difference at
runtime, and the migration advice for an affected program.

**Rationale:** divergences are inevitable — TMF is not InnoDB, Guardian is not
Linux. Undocumented divergences are the ones that cost a customer a weekend.

---

## VIII. Slices may proceed where whole specs cannot

A spec whose open questions do not affect a given vertical slice may be planned
and implemented **for that slice only**, without being marked `Ready`.

This exists because Principle I has a failure mode. Several questions across
002, 003, 004 and 008 are answerable only from documents this project may never
obtain (`SQLRM`, `CPG`, 9075-2). Under a strict Ready gate, those specs never
become implementable, and the project stalls permanently on procurement rather
than on engineering. That is not caution, it is paralysis.

A slice is legitimate only with all four of:

1. **An enumerated requirement subset.** Explicitly listed IDs. "The simple
   parts" is not a subset.
2. **An avoidance table.** Every open question in every spec the slice touches,
   with a stated reason the slice does not depend on it. A question that *is*
   touched must be narrowed to a scoped decision or the slice shrinks.
3. **Scoped decisions recorded as provisional.** A narrow choice made to unblock
   a slice is a *slice decision*, never a resolution. It does not close the
   question, and it must be revisited when the question closes — a slice decision
   that later contradicts the real answer is a defect to fix, not precedent to
   defend.
4. **A stated non-proof.** What a green slice does *not* demonstrate, so nobody
   mistakes a passing gate for a working implementation.

The spec's own status stays `Clarifying`. Slice status lives in the slice
document. `/speckit.analyze` checks slices against these four conditions.

**Rationale:** the alternative to slicing is either stalling indefinitely or
quietly relaxing Principle I, and the second is far more dangerous — it would let
guesses enter the implementation under cover of progress. Slicing keeps the
guesses enumerated, provisional, and small.

---

## Amendment

Amendments require: a new version number here (semver — MAJOR for removing or
inverting a principle, MINOR for adding one, PATCH for wording), a dated note in
this file's history, and a re-run of `/speckit.analyze` across all active specs.

### History

- **1.1.0** (2026-08-28) — Added Principle VIII, scoped slices. Prompted by
  Gate 1: the slice was demonstrably achievable while three of its specs
  remained `Clarifying`, and the strict Ready gate would have blocked it
  indefinitely on documents that may never arrive. Principle I is unchanged;
  VIII constrains how work may proceed around its gaps rather than relaxing it.
  `/speckit.analyze` and `/speckit.plan` updated. Re-run of analyze across
  active specs required.
- **1.0.0** (2026-08-27) — Initial ratification. Seven principles.
