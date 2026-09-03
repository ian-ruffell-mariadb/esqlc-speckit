# Feature Spec: INVOKE schema-derived structures

**ID:** 006-invoke-schema-gen · **Status:** Clarifying
**Manual coverage:** §2 pp.2-18..2-24; App. A (sample database, as fixture source)
**Depends on:** 001, 002, 003

## 1. Problem

`INVOKE` generates a C structure describing a table or view, so that a program's
host variables track the schema instead of duplicating it by hand. It is how real
programs are written — the manual's own advantages list is essentially "you stop
making transcription errors" — and it is the reason hand-written declare sections
are the exception rather than the rule in customer code.

It also means the preprocessor needs schema access at preprocess time. That is a
different capability from anything in 001–005: a connection, or a schema cache,
during compilation rather than at run time. And the structures it generates must
follow 002's mapping exactly, including the extra null-terminator byte and its
suppression under `CHAR_AS_ARRAY`, plus a parallel indicator structure when
requested.

## 2. Scope

**In scope**

- The `INVOKE` directive in declaration position.
- Schema retrieval at preprocess time, and its caching.
- Structure generation: field naming derived from column names, types per 002.
- The extra byte appended to character fields, and its suppression under
  `CHAR_AS_ARRAY`.
- `VARCHAR` columns generating the nested `{ short len; char val[]; }` form.
- Indicator variable generation for `INVOKE`ed structures.
- The App. A sample database, reproduced as a MariaDB schema for use as the
  conformance suite's fixture.

**Out of scope**

- The type mapping itself → 002
- `INVOKE` with SQLCI → 008
- Dynamic descriptors → 007
- Guardian object naming and TACL DEFINEs in the invoked name → 008 (`DIV-002`)

## 3. Requirements

| ID | Requirement | Citation |
|----|-------------|----------|
| FR-006.1 | `INVOKE` is accepted in declaration position and generates a structure describing the named table or view. | `[SQLPM/C §2 p.2-18]`, `[SQLPM/C §3 p.3-2]` |
| FR-006.2 | Generated field names are the column names, lowercased; generated types follow the 002 mapping. | `[SQLPM/C §2 pp.2-19..2-22]` |
| FR-006.2a | The generated structure tag is the object name with `_type` appended. | `[SQLPM/C §2 pp.2-21..2-22]` |
| FR-006.2b | `CHARACTER SET` is emitted inline in the field declaration, before the identifier, for columns carrying a character set. | `[SQLPM/C §2 p.2-22]` |
| FR-006.2c | `NCHAR(l)` generates a character array and `NCHAR VARYING(l)` generates the two-field VARCHAR structure, both in the system default multibyte character set. | `[SQLPM/C §2 pp.2-20..2-22]` |
| FR-006.2d | A class MAP DEFINE is accepted for the invoked object name, but not for the structure tag. | `[SQLPM/C §2 p.2-19]` `[DIV-002]` |
| FR-006.2e | `INVOKE` requires read access to the invoked object at preprocess time. | `[SQLPM/C §2 p.2-19]` |
| FR-006.3 | Character fields receive the extra null-terminator placeholder byte unless the SQL pragma specifies `CHAR_AS_ARRAY`. | `[SQLPM/C §2 p.2-7]` |
| FR-006.4 | A `VARCHAR` column generates a nested structure whose group name derives from the column name, with subordinate `len` (`short`) and `val` (character array), `val` carrying the extra byte when `CHAR_AS_STRING` is in effect. | `[SQLPM/C §2 p.2-9]` |
| FR-006.5 | A two-byte `short` indicator variable is generated automatically for every column that permits nulls, and precedes its host variable in the generated structure. | `[SQLPM/C §2 p.2-22]` |
| FR-006.5a | An indicator's name is the column name plus an optional prefix and a suffix, governed by the `PREFIX`, `SUFFIX`, and `NULL STRUCTURE` clauses. | `[SQLPM/C §2 p.2-22]` |
| FR-006.5b | With neither prefix nor suffix specified, the suffix `_I` is appended. | `[SQLPM/C §2 p.2-22]` |
| FR-006.5c | For a column name of 30 or 31 characters under the default suffix, SQL/MP truncates the `_I`, producing an indicator name identical to the host variable name. This implementation must **diagnose** that collision rather than reproduce it. | `[SQLPM/C §2 p.2-22]` `[DIV-050]` |
| FR-006.5d | Generated output carries a comment naming the invoked object and the timestamp of the definition used. | `[SQLPM/C §2 pp.2-21..2-22]` |
| FR-006.6 | A referenced table or view that does not exist, or is unreadable, is a preprocess-time error naming the object. | Principle III |
| FR-006.7 | The schema used for generation is recorded in the listing output so that a compiled program's assumed schema is auditable. | derived from `[SQLPM/C §6 p.6-25]` |
| FR-006.8 | Individual generated fields are referenceable as `:structname.field`; the structure name itself is referenceable only for the `VARCHAR` case. | `[SQLPM/C §2 pp.2-9, 2-10]` |
| NFR-006.1 | The App. A sample database is available as a MariaDB schema and seed dataset, and is the fixture base for features 004, 005, and 007. | Principle IV |
| NFR-006.2 | Schema access at preprocess time is optional-by-cache: a build can run against a cached schema with no live database. | — |

## 4. Acceptance scenarios

### AS-006.1 — Generated structure matches a hand-written one
- **Given** a table exercising every type in the 002 mapping
- **When** declared once by `INVOKE` and once by hand
- **Then** the two structures have identical layout, verified by `offsetof` and
  `sizeof` assertions on every field
- **Test:** `tests/conformance/006/invoke_vs_hand.sqlc`

### AS-006.2 — CHAR_AS_ARRAY suppresses the extra byte
- **Given** a `CHAR(20)` column
- **When** `INVOKE`d under `CHAR_AS_STRING` and again under `CHAR_AS_ARRAY`
- **Then** the field is 21 bytes and 20 bytes respectively
- **Test:** `tests/conformance/006/char_as_array.sqlc`

### AS-006.3 — VARCHAR nesting
- **Given** a `VARCHAR(26)` column
- **When** `INVOKE`d
- **Then** a nested structure with a `short len` and a 27-byte `val` is generated,
  and the group name is usable directly as a host variable
- **Test:** `tests/conformance/006/invoke_varchar.sqlc`

### AS-006.4 — Missing table
- **Given** an `INVOKE` of a nonexistent table
- **When** preprocessed
- **Then** an error naming the object, and no output file
- **Test:** `tests/conformance/006/negative/invoke_missing.sqlc`

### AS-006.5 — Offline build from cache
- **Given** a populated schema cache and no reachable database
- **When** preprocessed
- **Then** it succeeds and produces the same output as the online build
- **Test:** `tests/conformance/006/offline_cache.sh`

## 5. Diagnostics

| Code | Condition | Default policy | Citation |
|------|-----------|----------------|----------|
| `ESQLC-6001` | Invoked object does not exist | error | Principle III |
| `ESQLC-6002` | No schema source available (no connection, no cache) | error | Principle III |
| `ESQLC-6003` | Column type has no mapping in the 002 table | error | `[§2 pp.2-3..2-4]` |
| `ESQLC-6004` | Column name cannot be transformed into a valid, unique C identifier | error | `[§2 p.2-19]` |
| `ESQLC-6005` | Cached schema is stale relative to a reachable database | **not implementable** — needs a reachable database; see below | Principle III |
| `ESQLC-6006` | `INVOKE` outside declaration position | **redundant** — see below | `[§3 p.3-2]` |
| `ESQLC-6007` | Indicator name would collide with its host variable (30/31-char column under default suffix) | error | `[§2 p.2-22]` `[DIV-050]` |
| `ESQLC-6008` | No read access to the invoked object at preprocess time | error | `[§2 p.2-19]` |

**Three of these eight codes have something to say about them, and it is worth
saying together rather than as three footnotes.**

`ESQLC-6006` is **redundant with `ESQLC-1008`**. The dispatch table already
enforces declaration position for every construct and reports `ESQLC-1008`
("'INVOKE' must appear in declaration position") with file, line and column.
Gate 9 kept that rather than adding a per-construct duplicate: a second code
saying the same thing gives a reader two things to look up and the maintainer
two places to keep in step.

`ESQLC-6005` ("cached schema is stale relative to a reachable database") is
**unreachable by design**. It needs a reachable database, and the preprocessor
has none — NFR-001.2 forbids the dependency and SD-16 accepts the consequence.
It is registered so the number is not re-allocated.

This is the second unreachable code in the project, after 002's `ESQLC-2015`,
and the first redundant one. The pattern to watch is a registry that accumulates
codes which can never fire: each is individually defensible and collectively
they make the registry a worse guide than it looks. `diag_registry` checks that
every *emitted* code is registered; nothing checks the converse, and these three
are why that gap is now known.

`ESQLC-6005` is a warning rather than an error so that offline builds remain
possible, but it must be loud: a stale schema produces structures that mismatch
the database at run time, which is exactly the failure mode `INVOKE` exists to
prevent.

## 6. Open questions

| # | Question | Blocks | Resolution |
|---|----------|--------|------------|
| Q1 | Exact column-name-to-field-name transformation rule. | FR-006.2, `ESQLC-6004` | **RESOLVED** — field names are the column names, lowercased. Structure tag is `<object>_type`. See FR-006.2a..2c |
| Q2 | Exact `INVOKE` syntax, including how indicator generation is requested. | FR-006.1, FR-006.5 | **PARTIALLY RESOLVED** — the `PREFIX`, `SUFFIX`, and `NULL STRUCTURE` clauses are confirmed, and a class MAP DEFINE is accepted for the object name. The full production is still deferred to `SQLRM` `[EXTERNAL]` |
| Q3 | How are indicator variables named and associated with fields? | FR-006.5 | **RESOLVED** — see FR-006.5a..5d. Includes the 30/31-character truncation defect |
| Q4 | Schema cache format, invalidation, and whether it belongs in version control. | NFR-006.2, `ESQLC-6005` | unresolved — a build-reproducibility decision, not a manual question. **Precedent found:** SQL/MP's own cross-compiler reaches the host catalog at compile time via `-Wsqlhost`, `-Wsqluser`, and a connect mode (`-Wsqlconnect` / `HP_NSK_CONNECT_MODE`, defaulting to `secure_warn`). That is the direct analogue of this feature's preprocess-time schema access, and its option shape is worth mirroring. `[SQLPM/C §6 pp.6-33..6-34]` |
| Q5 | Does `INVOKE` generate anything for a protection view differently from a table? | FR-006.1 | unresolved |
| Q6 | `INVOKE` accepts a class MAP DEFINE for the object name but **not** for the structure tag. Does the tag then have to be given explicitly whenever a DEFINE is used? | FR-006.1, `DIV-002` | unresolved — `[SQLPM/C §2 p.2-19]` states the restriction without stating the consequence |

Q1–Q3 are transcription tasks against pages this spec already identifies, not
research. They are listed as questions because guessing a naming rule produces
structures that compile and then mismatch the database.

## 7. Constitution check

| Principle | Compliant? | Note |
|-----------|-----------|------|
| I manual is the contract | partial | Q1–Q3 close by transcription; FR-006.7 is derived and marked |
| II source compatibility | yes | `INVOKE` syntax unchanged; both pragma char options honoured |
| III no silent semantic change | yes | Missing schema errors rather than generating a guess; stale cache warns loudly |
| IV manual-derived tests first | yes | AS-006.1 is the strongest available check — cross-validation against 002 |
| V layered / frozen ABI | yes | Generation is preprocess-time only; no new runtime surface |
| VI byte-exact structures | yes | AS-006.1 asserts `offsetof` and `sizeof` per field |
| VII divergence registered | yes | None introduced. `DIV-002` applies if the invoked name is a TACL DEFINE |
