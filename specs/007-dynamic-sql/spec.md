# Feature Spec: Dynamic SQL & SQLDA

**ID:** 007-dynamic-sql · **Status:** Clarifying
**Manual coverage:** §10 pp.10-1..10-50; §2 p.2-5 (`CAST`); App. D (legacy descriptors)
**Depends on:** 001, 002, 003, 004, 005

## 1. Problem

Dynamic SQL is the largest single feature in the manual and the one that exposes
the most implementation surface to customer code. A program builds a statement at
run time, `PREPARE`s it, asks `DESCRIBE INPUT` and `DESCRIBE` what parameters and
columns it has, allocates buffers accordingly, fills in an `SQLDA` by hand, and
executes. Every field encoding in that descriptor is therefore API: programs read
`data_type`, decode the packed `data_len` and `precision` fields, set `var_ptr`
and `ind_ptr` themselves, and allocate using the published length constants.

The encodings are dense — `data_len` and `precision` are bit-packed differently
per type family, the type codes span four numbered ranges, and the date-time
qualifier table contains published duplicates that must be reproduced rather than
tidied. Alongside the `SQLDA` sit a names buffer with its own 11-byte overhead
formula and a collation buffer whose entries `cprl_ptr` points into.

Version 2 descriptors cannot be treated as legacy-optional, because version 2 is
what `INCLUDE STRUCTURES`'s absence generates.

## 2. Scope

**In scope**

- `PREPARE`, `EXECUTE`, `EXECUTE IMMEDIATE`, `DESCRIBE`, `DESCRIBE INPUT`,
  `RELEASE`.
- Dynamic `DECLARE CURSOR`, `OPEN … USING`, `FETCH`, `CLOSE`.
- Statements prepared to a name and to a host variable.
- `SQLDA`: all fields, all encodings, all published constants.
- Names buffer, including the overhead formula and `DESCRIBE INPUT`'s effect on
  it.
- Collation buffer and `cprl_ptr` semantics, including the no-collation case.
- All `data_type` codes across the four ranges.
- Date-time and INTERVAL qualifier codes, reproduced as published.
- Character-set IDs in `precision`, and the execution-time character-set check
  including the unknown-set exemption.
- Null handling via `ind_ptr`, and the invalid-address convention for programs
  that ignore nulls.
- Program-side dynamic allocation of descriptors and buffers, sized from `SQLSA`
  `prepare` fields.
- `CAST` in dynamic statements.
- Version 300+ descriptors, plus version 1 and 2 from App. D.

**Out of scope**

- `SQLSA` itself → 005 (007 consumes its `prepare` substructure)
- CPRL procedures → 008
- Dynamic SQL Pathway servers → 008
- Statement syntax → runtime translation, per NFR-001.1

## 3. Requirements

| ID | Requirement | Citation |
|----|-------------|----------|
| FR-007.1 | `PREPARE` compiles a statement held in a host variable and associates it with an SQL identifier or a host-variable name. | `[SQLPM/C §10 p.10-2]` |
| FR-007.2 | `EXECUTE` runs a prepared statement; `EXECUTE IMMEDIATE` compiles and runs one from a host variable in a single step. | `[SQLPM/C §10 p.10-2]` |
| FR-007.3 | `DESCRIBE INPUT` returns one `sqlvar` entry per input parameter; `DESCRIBE` returns one per output variable. | `[SQLPM/C §10 pp.10-2, 10-5]` |
| FR-007.4 | `RELEASE` deallocates a statement prepared through a host variable. | `[SQLPM/C §10 p.10-2]` |
| FR-007.5 | Dynamic `DECLARE CURSOR` associates a cursor with a prepared `SELECT`; `OPEN` accepts a `USING` clause supplying parameter values. | `[SQLPM/C §10 pp.10-2, 10-20]` |
| FR-007.6 | `SQLDA_EYE_CATCHER` is `D1`; `SQLDA_HEADER_LEN` is 4; `SQLDA_SQLVAR_LEN` is 24; `SQLDA_NAMESBUF_OVHD_LEN` is 11. | `[SQLPM/C §10 p.10-5]` |
| FR-007.7 | The names-buffer overhead of 11 bytes comprises a 2-byte length, an 8-byte table name, and a 1-byte separator. | `[SQLPM/C §10 p.10-5]` |
| FR-007.8 | `eye_catcher` and `var_ptr` are program-initialised; the implementation never writes to them. | `[SQLPM/C §10 pp.10-5, 10-6]` |
| FR-007.9 | `num_entries` states the descriptor's capacity in parameters or variables. | `[SQLPM/C §10 p.10-5]` |
| FR-007.10 | `data_len` for fixed-length character is the byte count; for variable-length character, the maximum byte count. | `[SQLPM/C §10 p.10-6]` |
| FR-007.11 | `data_len` for decimal and binary numeric packs scale in bits 0:7 and byte length in bits 8:15, the latter restricted to 2, 4, or 8 for binary numeric. | `[SQLPM/C §10 p.10-6]` |
| FR-007.12 | `data_len` for date-time and INTERVAL packs the field-range qualifier in bits 0:7 and the storage size in bits 8:15. | `[SQLPM/C §10 p.10-6]` |
| FR-007.13 | `precision` holds numeric precision for binary numeric; leading-field precision in bits 0:7 and fraction precision in bits 8:15 for date-time and INTERVAL, with bits 8:15 zero when there is no fraction field; and the character-set ID for CHAR and VARCHAR. | `[SQLPM/C §10 p.10-6]` |
| FR-007.14 | `null_info` is negative when an input parameter's column permits nulls, and negative when an output row returned is null. | `[SQLPM/C §10 p.10-6]` |
| FR-007.15 | `ind_ptr` addresses a flag set to `-1` for null, by the program on input and by the implementation on output. | `[SQLPM/C §10 p.10-6]` |
| FR-007.16 | A program that does not process nulls may set `ind_ptr` to an invalid address, and the implementation must not dereference it in that case. | `[SQLPM/C §10 p.10-6]` |
| FR-007.17 | `cprl_ptr` is unset for input columns; for output columns it addresses the collation used, or holds a negative integer when no collation was used. | `[SQLPM/C §10 p.10-6]` |
| FR-007.18 | All published `data_type` values are supported with their `_SQLDT_*` names: character 0, 1, 2, 64, 65, 66; numeric 130–134, 140, 141; decimal 150–154; date-time and INTERVAL 192 and 195–212. | `[SQLPM/C §10 pp.10-8..10-9]` |
| FR-007.19 | The date-time and INTERVAL qualifier codes 1–28 are reproduced exactly as published, including the entries whose ranges duplicate earlier ones; the implementation accepts either encoding on input and documents which it emits. | `[SQLPM/C §10 p.10-10]` |
| FR-007.20 | Character-set IDs 0, 1, 12, and 101–109 are supported with their `_SQL_CHARSETID_*` names. | `[SQLPM/C §10 p.10-11]` |
| FR-007.21 | At execution, `precision`'s character-set ID is checked against the column's character set and a mismatch is an error — except that a program expecting the unknown set against a single-byte column is accepted. | `[SQLPM/C §10 p.10-11]` |
| FR-007.22 | Input parameters are recognised as `?` or `?name` and may appear wherever a constant may. | `[SQLPM/C §10 p.10-11]` |
| FR-007.23 | Descriptor and buffer sizes are derivable from the `SQLSA` `prepare` fields — `input_num`, `input_names_len`, `output_num`, `output_names_len`, `output_collations_len`. | `[SQLPM/C §9 p.9-18]`, `[SQLPM/C §10 p.10-29]` |
| FR-007.24 | `DESCRIBE INPUT`'s documented effect on the names buffer is reproduced. | `[SQLPM/C §10 p.10-18]` |
| FR-007.25 | `CAST` converts a parameter between character types or between numeric types in a dynamic statement. | `[SQLPM/C §2 p.2-5]` `[EXTERNAL — SQLRM]` |
| FR-007.26 | Version 300 and later `SQLDA` layouts are generated per `INCLUDE STRUCTURES`. | `[SQLPM/C §9 p.9-2]` |
| FR-007.27 | Version 1 and version 2 `SQLDA` layouts are generated, version 2 being the default when `INCLUDE STRUCTURES` is absent. | `[SQLPM/C App. D pp.D-3..D-8]`, `[SQLPM/C §9 p.9-1]` |
| NFR-007.1 | Every `data_type` code has a round-trip test: describe, allocate, bind, execute, compare. | Principle IV |
| NFR-007.2 | Every `data_len` and `precision` packing rule has an encode/decode unit test independent of any database. | Principle IV |
| NFR-007.3 | Every generated `SQLDA` version carries `sizeof` and `offsetof` static assertions, and `SQLDA_SQLVAR_LEN` is asserted equal to 24. | Principle VI |

## 4. Acceptance scenarios

### AS-007.1 — Full dynamic round trip
- **Given** a `SELECT` with two parameters built at run time
- **When** prepared, described both ways, buffers allocated from `SQLSA`
  `prepare` fields, parameters bound, and the statement executed through a cursor
- **Then** the rows returned match the equivalent static query
- **Test:** `tests/conformance/007/dynamic_roundtrip.sqlc`

### AS-007.2 — Packing round trip for every type family
- **Given** each of fixed character, varying character, decimal, binary numeric,
  date-time, and INTERVAL
- **When** `data_len` and `precision` are encoded and decoded
- **Then** the recovered scale, byte length, qualifier, storage size, leading
  precision, fraction precision, and character-set ID all match
- **Test:** `tests/conformance/007/packing.c`

### AS-007.3 — Invalid ind_ptr not dereferenced
- **Given** a descriptor whose `ind_ptr` is deliberately an invalid address and
  whose result set contains no nulls
- **When** executed
- **Then** it completes without a fault
- **Test:** `tests/conformance/007/ind_ptr_invalid.sqlc`

### AS-007.4 — Character-set mismatch and its exemption
- **Given** a single-byte column, described once with a matching set ID and once
  with the unknown set ID, and once with a deliberately wrong set ID
- **When** executed
- **Then** the first two succeed and the third errors
- **Test:** `tests/conformance/007/charset_check.sqlc`

### AS-007.5 — Version 2 default descriptor
- **Given** a dynamic program with `INCLUDE SQLDA` and no `INCLUDE STRUCTURES`
- **When** preprocessed and run
- **Then** a version 2 descriptor is generated and the program works against it
- **Test:** `tests/conformance/007/sqlda_v2_default.sqlc`

### AS-007.6 — Duplicate qualifier codes accepted
- **Given** descriptors using qualifier code 6 and code 26 for second-to-second,
  and 7 and 28 for fraction-to-fraction
- **When** executed
- **Then** all four are accepted and behave identically
- **Test:** `tests/conformance/007/qualifier_duplicates.sqlc`

### AS-007.7 — No collation used
- **Given** an output column with no collation
- **When** described
- **Then** `cprl_ptr` holds a negative integer rather than a pointer
- **Test:** `tests/conformance/007/cprl_none.sqlc`

## 5. Diagnostics

| Code | Condition | Default policy | Citation |
|------|-----------|----------------|----------|
| `ESQLC-7001` | `EXECUTE` of an unprepared or released statement name | error | `[§10 p.10-2]` |
| `ESQLC-7002` | `num_entries` smaller than the parameter or column count reported by `PREPARE` | error | `[§9 p.9-18]` |
| `ESQLC-7003` | `var_ptr` null or unset at execution | error | `[§10 p.10-6]` |
| `ESQLC-7004` | Unrecognised `data_type` value | error | `[§10 pp.10-8..10-9]` |
| `ESQLC-7005` | `data_len` byte length not 2, 4, or 8 for a binary numeric entry | error | `[§10 p.10-6]` |
| `ESQLC-7006` | Unrecognised date-time or INTERVAL qualifier code | error | `[§10 p.10-10]` |
| `ESQLC-7007` | Character-set ID mismatch, outside the unknown-set exemption | error | `[§10 p.10-11]` |
| `ESQLC-7008` | Names buffer smaller than `input_names_len` or `output_names_len` | error | `[§9 p.9-18]` |
| `ESQLC-7009` | Collation buffer smaller than `output_collations_len` | error | `[§9 p.9-18]` |
| `ESQLC-7010` | Eye-catcher not initialised by the program | warn | `[§10 p.10-5]` |
| `ESQLC-7011` | `RELEASE` of a statement prepared to a name rather than a host variable | error | `[§10 p.10-2]` |
| `ESQLC-7012` | Dynamic statement class not supported by this implementation | error | Principle III |

## 6. Open questions

| # | Question | Blocks | Resolution |
|---|----------|--------|------------|
| Q1 | `SQLDA` field offsets. `SQLDA_HEADER_LEN` 4 and `SQLDA_SQLVAR_LEN` 24 constrain the layout tightly — is it uniquely determined? Two 2-byte header fields plus a 24-byte entry containing three pointers and four scalars needs checking against the target's pointer width. | FR-007.6, NFR-007.3 | unresolved — and note that `var_ptr` is documented as an *extended address* on NonStop, which is not a 64-bit pointer |
| Q2 | If `SQLDA_SQLVAR_LEN` 24 cannot hold three 64-bit pointers plus four scalars, what gives — the constant, or the pointer representation? | FR-007.6 | unresolved — this is a hard conflict and needs a registered divergence either way |
| Q3 | Version 1 and version 2 `SQLDA` layouts and the version-to-version changes from App. D. | FR-007.27 | pending — transcribe App. D during `/speckit.plan` |
| Q4 | Which encoding does the implementation emit for the duplicated qualifier codes? | FR-007.19 | unresolved — pick the lower value and document |
| Q5 | Does `WHENEVER` apply to dynamic statements? Shared with 005 Q5. | — | unresolved |
| Q6 | Collation buffer format. `output_collations_len` gives its size but the manual's structure for its contents needs locating. | FR-007.17, `ESQLC-7009` | pending — §10 pp.10-3..10-11 |

Q1 and Q2 are the critical path. `SQLDA_SQLVAR_LEN` = 24 with three pointers is
arithmetically impossible on a 64-bit target, so either the descriptor uses
32-bit offsets rather than native pointers, or the constant must change and a
divergence must be registered. This must be settled before any code is written,
because it determines the shape of every dynamic SQL program's allocation logic.

## 7. Constitution check

| Principle | Compliant? | Note |
|-----------|-----------|------|
| I manual is the contract | partial | Q3 and Q6 close by transcription; Q1/Q2 are a genuine conflict between published constants and the target platform |
| II source compatibility | yes | All statement forms and all published identifier names preserved; duplicate qualifier codes reproduced rather than tidied |
| III no silent semantic change | yes | Twelve diagnostics; FR-007.16 forbids dereferencing an invalid `ind_ptr` rather than treating it as a bug |
| IV manual-derived tests first | yes | NFR-007.1 per type code, NFR-007.2 database-free packing tests |
| V layered / frozen ABI | yes | Descriptor manipulation is program-side; the runtime consumes descriptors through the ABI |
| VI byte-exact structures | at risk | NFR-007.3 requires it; Q1/Q2 may make it unachievable for `sqlvar`. Resolve before implementation |
| VII divergence registered | partial | Q2 will require one. None created yet |
