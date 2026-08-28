# Feature Spec: Preprocessor core & pipeline

**ID:** 001-preprocessor-core · **Status:** Ready
**Manual coverage:** §3 pp.3-1..3-7; §6 pp.6-1..6-2, 6-25..6-27, 6-34
**Depends on:** none

## 1. Problem

An ESQL/C source file is not C. It contains `EXEC SQL` … `;` constructs in
declaration and executable positions, a mandatory `#pragma SQL`, `WHENEVER`
directives with lexical scope, and SQL comment and string conventions that
conflict with C's. Nothing downstream can be built until something can reliably
find these constructs, classify them, and emit compilable C — with line numbers
that still point at the original source, because a customer debugging a 20,000-
line program against emitted C with shifted line numbers has been handed a worse
tool than they had.

Without this feature every other feature has nowhere to put its output. It is
also the feature that decides how much SQL grammar the project owns: the manual
defers statement syntax to a manual this project does not have, so the boundary
between "the preprocessor understands this" and "the runtime translates this"
is set here and is expensive to move later.

## 2. Scope

**In scope**

- Scanning a source file into interleaved C regions and embedded SQL constructs.
- The embedding form: `EXEC SQL` … `;`, single- or multi-line.
- SQL-region lexical rules: `--` comments to end of line, `"` as the only string
  delimiter, no nesting.
- Classifying each construct as directive vs. statement, and enforcing the
  placement classes of §3 p.3-2.
- `#pragma SQL` recognition, its option set, and its mandatory position.
- Equivalent-of-`#pragma SQL` as a command-line option.
- Host-variable reference extraction (`:name`, `:*ptr`, `:struct.field`) from
  statement bodies — recognition and location only; typing belongs to 002.
- C emission: pass-through of C regions verbatim, replacement of SQL constructs
  by generated code, `#line` directives preserving original file and line.
- Listing output: `SQLMAP` and `WHENEVERLIST` equivalents.
- Diagnostics with original-source positions.
- `SQL SOURCE` directive and its interaction with line fidelity.

**Out of scope**

- Typing and declaring host variables → 002
- Runtime ABI definition → 003
- Semantics of any specific statement → 004, 007, 008
- `WHENEVER` code generation → 005 (001 provides scope tracking only)
- `INVOKE` expansion → 006
- The NonStop compilation pipeline (BIND, SQL compiler pass, program files) → 008
- `SQLMEM` → 008

## 3. Requirements

| ID | Requirement | Citation |
|----|-------------|----------|
| FR-001.1 | An embedded construct begins with the keywords `EXEC SQL` and ends at the first `;` not inside an SQL string or comment. | `[SQLPM/C §3 p.3-1]` |
| FR-001.2 | A construct may span any number of source lines with no continuation marker. | `[SQLPM/C §3 p.3-1]` |
| FR-001.3 | Nested constructs are rejected with a diagnostic. | `[SQLPM/C §3 p.3-1]` |
| FR-001.4 | Within an SQL region, `--` starts a comment ending at end of line. | `[SQLPM/C §3 p.3-1]` |
| FR-001.5 | Within an SQL region, C comment forms (`/* */`, `//`) are rejected with a diagnostic. | `[SQLPM/C §3 p.3-1]` |
| FR-001.6 | Within an SQL region, `"` is the only recognised string delimiter. | `[SQLPM/C §3 p.3-1]` |
| FR-001.7 | `#pragma SQL` is required before any SQL or C statement other than comments; its absence is an error. | `[SQLPM/C §3 p.3-2]` |
| FR-001.8 | The SQL pragma may instead be supplied as a command-line option, with identical effect. | `[SQLPM/C §3 p.3-2]` |
| FR-001.9 | The pragma accepts the options `SQLMAP`, `WHENEVERLIST`, `RELEASE1`, `RELEASE2`; unknown options are diagnosed, not ignored. | `[SQLPM/C §3 p.3-7]` |
| FR-001.10 | The pragma accepts `CHAR_AS_STRING` and `CHAR_AS_ARRAY`, whose values are exposed to 002 and 006. | `[SQLPM/C §2 pp.2-7, 2-9]` |
| FR-001.11 | `BEGIN`/`END DECLARE SECTION`, static `DECLARE CURSOR`, `INVOKE`, `INCLUDE STRUCTURES`, and `INCLUDE SQLCA`/`SQLSA`/`SQLDA` are accepted only in declaration position. | `[SQLPM/C §3 p.3-2]` |
| FR-001.12 | DML, DCL, DDL, DSL, transaction control, and dynamic SQL statements are accepted only in executable position. | `[SQLPM/C §3 p.3-2]` |
| FR-001.13 | `WHENEVER`, `SQL SOURCE`, and `CONTROL` directives are accepted in any position. | `[SQLPM/C §3 p.3-3]` |
| FR-001.14 | Placement violations produce a diagnostic naming the construct and the position class it requires. | `[SQLPM/C §3 p.3-2]` |
| FR-001.15 | Every recognised statement and directive in the §3 inventory is dispatched to an owning handler; an unrecognised construct is diagnosed with its keyword. | `[SQLPM/C §3 pp.3-3..3-6]` |
| FR-001.16 | Host-variable references are recognised as `:identifier`, with the colon preceding `*` for pointer host variables, and `:structname.field` for structure fields. | `[SQLPM/C §2 pp.2-6, 2-10]` |
| FR-001.17 | An identifier used as a host variable that is the left-hand side of a `#define` is rejected. | `[SQLPM/C §2 p.2-6]` |
| FR-001.18 | Emitted C contains `#line` directives such that every diagnostic from the C compiler and every debugger line reference resolves to the original source file and line. | derived from `[SQLPM/C §6 p.6-25]` listing correspondence |
| FR-001.19 | C regions are emitted byte-for-byte unchanged. | Principle II |
| FR-001.20 | With `SQLMAP`, a map of embedded statements is written to the listing, including the host object SQL version. | `[SQLPM/C §6 p.6-25]` `[SQLPM/C §9 p.9-3]` |
| FR-001.21 | With `WHENEVERLIST`, the active `WHENEVER` options are written to the listing after each processed statement. | `[SQLPM/C §3 p.3-7]` |
| FR-001.22 | `WHENEVER` scope state (condition → action, in source order) is tracked and exposed to 005 at each statement site. | `[SQLPM/C §9 p.9-6]` |
| FR-001.23 | `SQL SOURCE` includes further source; constructs from included source retain the included file's name and line numbers in diagnostics and `#line`. | `[SQLPM/C §3 p.3-3]` |
| FR-001.24 | A C label prefix may immediately precede an embedded statement; the label is emitted in the C region and does not affect the construct's position class. | `[SQL/B §16.4 SR2]`, HP manual silent — see Q4 |
| NFR-001.1 | Statement bodies are carried as opaque token streams except for host-variable references, the leading keyword, and constructs 004/006/007 explicitly claim. | Principle I, given `SQLRM` deferral |
| NFR-001.2 | The preprocessor is buildable and testable with a stub runtime and no database. | Principle V |
| NFR-001.3 | Every diagnostic carries original file, line, and column. | Principle III |

`[EXTERNAL]` items to resolve before Phase 2: the full option syntax of
`#pragma SQL` is documented in `CPG`, not here. FR-001.9 covers the four options
§3 names; the remainder must be enumerated from `CPG` or the option set frozen
and documented as a divergence.

## 4. Acceptance scenarios

### AS-001.1 — Multi-line statement with SQL comment
- **Given** a source file whose `EXEC SQL SELECT` spans five lines and contains a
  `--` comment and a `"` string containing a `;`
- **When** preprocessed
- **Then** exactly one SQL construct is recognised, its body excludes the
  comment, and the `;` inside the string does not terminate it
- **Test:** `tests/conformance/001/scan_multiline_comment_string.sqlc`

### AS-001.2 — Missing pragma
- **Given** a source file with `EXEC SQL` constructs and no `#pragma SQL`
- **When** preprocessed
- **Then** preprocessing fails with a diagnostic naming the missing pragma, and
  no output file is written
- **Test:** `tests/conformance/001/negative/no_pragma.sqlc`

### AS-001.3 — Placement enforcement
- **Given** an `EXEC SQL INCLUDE SQLCA` inside a function body's executable
  region, and an `EXEC SQL INSERT` in a file-scope declaration region
- **When** preprocessed
- **Then** two diagnostics, each naming the construct and its required position
  class
- **Test:** `tests/conformance/001/negative/placement.sqlc`

### AS-001.4 — Line fidelity
- **Given** a source file where a deliberate C syntax error sits on original
  line 142, after three multi-line embedded statements
- **When** preprocessed and compiled
- **Then** the C compiler reports the error at line 142 of the original file
- **Test:** `tests/conformance/001/line_fidelity.sqlc`

### AS-001.5 — C pass-through
- **Given** a source file containing C constructs that resemble SQL (a string
  literal `"EXEC SQL"`, a comment mentioning `EXEC SQL`, a macro named
  `EXEC_SQL`)
- **When** preprocessed
- **Then** no SQL construct is recognised and the output is byte-identical to the
  input apart from `#line` directives and the pragma's expansion
- **Test:** `tests/conformance/001/passthrough_lookalikes.sqlc`

### AS-001.6 — Host variable recognition
- **Given** statements referencing `:plain`, `:*ptr`, `:s.field`,
  `:v INDICATOR :i`, and `:v :i`
- **When** preprocessed
- **Then** all five forms are recognised with correct source spans, and the two
  indicator forms are recognised as equivalent
- **Test:** `tests/conformance/001/hostvar_forms.sqlc`

### AS-001.7 — WHENEVER scope tracking
- **Given** a file that sets `WHENEVER SQLERROR CALL`, then two statements, then
  `WHENEVER SQLERROR CONTINUE`, then two more statements
- **When** preprocessed with `WHENEVERLIST`
- **Then** the listing shows the correct active action after each of the four
  statements
- **Test:** `tests/conformance/001/whenever_scope.sqlc`

## 5. Diagnostics

| Code | Condition | Default policy | Citation |
|------|-----------|----------------|----------|
| `ESQLC-1001` | Nested `EXEC SQL` construct | error | `[§3 p.3-1]` |
| `ESQLC-1002` | Unterminated `EXEC SQL` construct at end of file | error | `[§3 p.3-1]` |
| `ESQLC-1003` | C comment inside an SQL region | error | `[§3 p.3-1]` |
| `ESQLC-1004` | Single-quoted string inside an SQL region | error | `[§3 p.3-1]` |
| `ESQLC-1005` | Missing `#pragma SQL` | error | `[§3 p.3-2]` |
| `ESQLC-1006` | `#pragma SQL` not first | error | `[§3 p.3-2]` |
| `ESQLC-1007` | Unknown SQL pragma option | error | `[§3 p.3-7]` |
| `ESQLC-1008` | Construct in wrong position class | error | `[§3 p.3-2]` |
| `ESQLC-1009` | Unrecognised statement or directive keyword | error | `[§3 pp.3-3..3-6]` |
| `ESQLC-1010` | Host variable name is a `#define` LHS | error | `[§2 p.2-6]` |
| `ESQLC-1011` | `SQL SOURCE` file not found | error | `[§3 p.3-3]` |
| `ESQLC-1012` | Recognised statement with no implementing handler yet | error | Principle III |

`ESQLC-1012` exists so that a partially-implemented preprocessor refuses work
rather than emitting a no-op. It is expected to fire constantly during Phases 1–3
and its message must name the owning feature.

## 6. Open questions

| # | Question | Blocks | Resolution |
|---|----------|--------|------------|
| Q1 | How is declaration vs. executable position determined without a full C parser? | FR-001.11..14 | Resolved in plan §1: brace-depth and statement-boundary tracking, no full parse |
| Q2 | Full `#pragma SQL` option set (deferred to `CPG`) | FR-001.9 | **RESOLVED** — frozen at the four §3 options plus the two character options. A new divergence is registered if `CPG` is later obtained and contradicts the freeze |
| Q3 | Does `SQL SOURCE` participate in `#include` guard semantics? | FR-001.23 | **RESOLVED** — textual inclusion with its own name and line tracking; no guard semantics |
| Q4 | May a C label prefix immediately precede an embedded statement (`retry: EXEC SQL …`)? | FR-001.1, FR-001.12 | **RESOLVED** — yes, supported. See FR-001.24 |

## 7. Constitution check

| Principle | Compliant? | Note |
|-----------|-----------|------|
| I manual is the contract | yes | All FRs cited; FR-001.18 marked derived; FR-001.24 cited to the ANSI standard where the HP manual is silent; Q2's `[EXTERNAL]` gap closed by a documented freeze |
| II source compatibility | yes | No new required syntax; pragma-as-CLI-option preserved; C regions byte-exact |
| III no silent semantic change | yes | `ESQLC-1012` prevents silent no-ops for unimplemented statements |
| IV manual-derived tests first | yes | Seven acceptance scenarios, each with a named fixture |
| V layered / frozen ABI | yes | NFR-001.2; preprocessor emits `esqlc_*` calls only |
| VI byte-exact structures | n/a | This feature generates no SQL structures |
| VII divergence registered | yes | Q2's freeze becomes a divergence entry if `CPG` contradicts it. No `DIV` created yet |
