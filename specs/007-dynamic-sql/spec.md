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
| FR-007.6 | `SQLDA_EYE_CATCHER` is `D1`; `SQLDA_HEADER_LEN` is 4; `SQLDA_NAMESBUF_OVHD_LEN` is 11; `SQLDA_COLLBUF_OVHD_LEN` is 4. `SQLDA_SQLVAR_LEN` is 40, not the published 24. | `[SQLPM/C §10 pp.10-5, 10-7]` `[DIV-040]` |
| FR-007.6a | A `sqlvar` entry comprises `data_type`, `data_len`, `precision`, `null_info` as 16-bit fields, followed by `var_ptr`, `ind_ptr`, `cprl_ptr`, and a `reserved` field; the header comprises a 2-byte eye-catcher and a 16-bit `num_entries`. | `[SQLPM/C §10 p.10-7]` |
| FR-007.6b | The `reserved` fourth address-width field is present and preserved; programs must not assume the entry ends after `cprl_ptr`. | `[SQLPM/C §10 p.10-7]` |
| FR-007.7 | The names-buffer overhead of 11 bytes comprises a 2-byte length, an 8-byte table name, and a 1-byte separator; the collation-buffer overhead of 4 bytes is the VARCHAR length field. | `[SQLPM/C §10 pp.10-5, 10-7]` |
| FR-007.7a | Buffer sizes follow the published formulas: names buffer `(name_string_size + 11) × sqlvar_count`, collation buffer `(max_collation_size + 4) × sqlvar_count`. | `[SQLPM/C §10 p.10-7]` |
| FR-007.7b | An ILP32 build mode preserves the published 24-byte `sqlvar` layout exactly, for programs that cannot absorb `DIV-040`. | `[DIV-040]` |
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
| FR-007.27a | The version 2 `sqlvar` comprises `data_type`, `data_len`, `precision`, `null_info` as 16-bit fields, then `var_ptr` and `ind_ptr` as address-width fields, then a 64-bit `reserved`. It has **no `cprl_ptr`**. | `[SQLPM/C App. D p.D-4]` |
| FR-007.27b | Legacy eye-catcher values are `DA` for version 1 and `D1` for version 2. | `[SQLPM/C App. D p.D-2]` — see Q8 on the source contradiction |
| FR-007.27c | Version-to-version additions are honoured: version 2 adds `precision`, `null_info`, and `ind_ptr` over version 1; version 300 and later add `cprl_ptr` and the user-defined collation buffer. | `[SQLPM/C App. D p.D-2]` |
| FR-007.27d | The version 2 names buffer is sized one byte longer than the computed length, per its own declaration form, rather than by the version 300+ overhead formula. | `[SQLPM/C App. D p.D-4]` |
| NFR-007.1 | Every `data_type` code has a round-trip test: describe, allocate, bind, execute, compare. | Principle IV |
| NFR-007.2 | Every `data_len` and `precision` packing rule has an encode/decode unit test independent of any database. | Principle IV |
| NFR-007.3 | Every generated `SQLDA` version carries `sizeof` and `offsetof` static assertions on every field, asserted against the layout of FR-007.6a. `SQLDA_HEADER_LEN` is asserted equal to 4 in all build modes; `SQLDA_SQLVAR_LEN` is asserted equal to 40 in the default build and 24 in the ILP32 build. | Principle VI, `[DIV-040]` |

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
| Q1 | `SQLDA` field offsets and whether 24 bytes is uniquely determined. | FR-007.6, NFR-007.3 | **RESOLVED** — §10 p.10-7 publishes the layout. Four 16-bit scalars + four 32-bit address/reserved fields = 24 exactly; header 2 + 2 = 4 exactly. Offsets are fully determined |
| Q2 | The address fields cannot hold a 64-bit pointer. What gives — the constant, or the representation? | FR-007.6 | **RESOLVED** — `DIV-040`: widen to 64-bit, `SQLDA_SQLVAR_LEN` becomes 40, with an ILP32 build mode (FR-007.7b) preserving 24. Justified by the manual's own instruction to use symbolic identifiers because values change between RVUs |
| Q3 | Version 1 and version 2 `SQLDA` layouts and the version-to-version changes from App. D. | FR-007.27 | **RESOLVED** — see FR-007.27a..27c. The v2 `sqlvar` is also 24 bytes but differently composed: two 32-bit address fields and one 64-bit `reserved`, versus v315's four 32-bit fields |
| Q8 | The manual contradicts itself on legacy eye-catcher values. Table D-1 maps v1 to `DA` and v2 to `D1`; Table D-2's field description states the reverse. Which is right? | FR-007.27b | **PROVISIONALLY RESOLVED** — follow Table D-1 (v1 = `DA`, v2 = `D1`). Example D-1, which is the *version 2* example, defines the eye-catcher as `D1`, corroborating Table D-1. Recorded as a source defect; confirm against `SQLRM` if it becomes available |
| Q4 | Which encoding does the implementation emit for the duplicated qualifier codes? | FR-007.19 | **RESOLVED** — emit the lower value (6 for second-to-second, 7 for fraction-to-fraction), accept both on input |
| Q5 | Does `WHENEVER` apply to dynamic statements? Shared with 005 Q5. | — | unresolved |
| Q6 | Collation buffer format. | FR-007.17, `ESQLC-7009` | **RESOLVED** — §10 p.10-7: collation names are VARCHAR items, buffer sized `(max_collation_size + 4) × sqlvar_count`, the 4 being the VARCHAR length field. New constant `SQLDA_COLLBUF_OVHD_LEN` |
| Q7 | Setting `data_len` to 0 is documented as a way to ignore scale, deliberately causing truncation. Is this a supported idiom to reproduce? | FR-007.11 | unresolved — `[SQLPM/C §10 pp.10-16, 10-25]`. It is deliberate data loss requested by the program, so Constitution III likely permits it, but it needs an explicit decision |

Q1, Q2, Q4, and Q6 are closed. The earlier concern that 24 bytes was
arithmetically impossible was based on an incomplete reading: the entry holds
**four** address-width fields, not three pointers, and they are declared as
integers rather than pointer types. The remaining real constraint is only that a
32-bit field cannot hold a 64-bit host address, which `DIV-040` settles.

Q3 is now known to be transcription of published layouts rather than
reconstruction. Q7 is newly surfaced by the same reading.

## 7. Constitution check

| Principle | Compliant? | Note |
|-----------|-----------|------|
| I manual is the contract | partial | Q1, Q2, Q4, Q6 resolved from §10 p.10-7. Q3 is transcription of published App. D layouts; Q5 and Q7 remain open |
| II source compatibility | yes | All statement forms and all published identifier names preserved; duplicate qualifier codes reproduced rather than tidied |
| III no silent semantic change | yes | Twelve diagnostics; FR-007.16 forbids dereferencing an invalid `ind_ptr` rather than treating it as a bug |
| IV manual-derived tests first | yes | NFR-007.1 per type code, NFR-007.2 database-free packing tests |
| V layered / frozen ABI | yes | Descriptor manipulation is program-side; the runtime consumes descriptors through the ABI |
| VI byte-exact structures | yes | Layout fully determined by FR-007.6a and asserted by NFR-007.3. Exact in the ILP32 mode; exact-by-published-identifier in the default mode via `DIV-040` |
| VII divergence registered | yes | `DIV-040` registered, `proposed` pending owner sign-off since it redefines a published constant |
