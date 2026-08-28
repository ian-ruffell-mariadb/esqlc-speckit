# Feature Spec: Declare sections & host-variable type mapping

**ID:** 002-host-variables · **Status:** Clarifying
**Manual coverage:** §2 pp.2-1..2-26 (except `INVOKE`, → 006); App. D type notes
**Depends on:** 001

## 1. Problem

Host variables are the only channel between C and the database, and their
declared type determines every conversion, every warning, and every silent
corruption. The manual's mapping is precise about widths, about the extra byte
appended to character arrays, about `VARCHAR` becoming a two-field structure, and
about the four conversion conditions that must produce warnings. Getting any of
these wrong produces a program that runs and returns wrong data.

Two things make this harder than a lookup table. First, the mapping was written
for a compiler where `long` is 32 bits, so a name-faithful mapping is a
width-unfaithful one on the target platform. Second, C has no fixed-point type,
so scale is carried out of band via `SETSCALE` and the `fixed` type — meaning a
scaled column can round-trip correctly or lose its fraction depending on a
declaration the preprocessor must notice.

## 2. Scope

**In scope**

- `BEGIN`/`END DECLARE SECTION` and the naming rules for host variables.
- The complete character type mapping, including `CHARACTER SET` and `NATIONAL
  CHARACTER` clauses and the character-set keyword set.
- The complete numeric, decimal, date-time, and INTERVAL type mapping.
- The trailing null-terminator placeholder byte, and `CHAR_AS_STRING` /
  `CHAR_AS_ARRAY`.
- `VARCHAR` as a `{ short len; char val[]; }` structure, and the rule that only a
  `VARCHAR` structure name is usable as a host variable.
- Indicator variables: type, null encoding on input and output.
- `TYPE AS` for date-time and INTERVAL host variables.
- `SETSCALE` and C `fixed` for scaled values.
- All conversion rules and the four mandatory warning conditions.
- Rejection of `unsigned long long`.

**Out of scope**

- `INVOKE`-generated declarations → 006
- Substituting host variable values into statements at run time → 003
- Dynamic SQL descriptors → 007
- The `CAST` function → 007

## 3. Requirements

| ID | Requirement | Citation |
|----|-------------|----------|
| FR-002.1 | Host variable declarations are recognised only between `BEGIN DECLARE SECTION` and `END DECLARE SECTION`. | `[SQLPM/C §2 p.2-1]` |
| FR-002.2 | A host variable name is any valid C identifier. | `[SQLPM/C §2 pp.2-2, 2-6]` |
| FR-002.3 | `CHAR(l)` and `PIC X(l)` map to `char v[l+1]`, the final byte a null-terminator placeholder. | `[SQLPM/C §2 p.2-3]` |
| FR-002.4 | `CHARACTER(l) CHARACTER SET cs` and `PIC X(l) CHARACTER SET cs` carry the character set into the declaration. | `[SQLPM/C §2 p.2-3]` |
| FR-002.5 | `NATIONAL CHARACTER(l)` uses the system default multibyte character set. | `[SQLPM/C §2 p.2-3]` |
| FR-002.6 | `VARCHAR(l)` maps to a structure with a `short len` and a `char val[l+1]`, in that order and with those names. | `[SQLPM/C §2 pp.2-3, 2-9]` |
| FR-002.7 | `NATIONAL CHARACTER VARYING(l)` maps as `VARCHAR` with `val` in the default multibyte set. | `[SQLPM/C §2 p.2-3]` |
| FR-002.8 | Recognised character-set keywords are `KANJI`, `KSC5601`, `ISO8859n` (n = 1..9), and `UNKNOWN`; absent a clause, the set is `UNKNOWN`. | `[SQLPM/C §2 p.2-3]` |
| FR-002.9 | Integer types map by width: 16-bit for `SMALLINT`/`NUMERIC(1..4)`, 32-bit for `INTEGER`/`NUMERIC(5..9)`, 64-bit signed for `LARGEINT`/`NUMERIC(10..18)`, with signedness preserved. | `[SQLPM/C §2 p.2-4]` `[DIV-001]` |
| FR-002.10 | `FLOAT(1..22 bits)` and `REAL` map to `float`; `FLOAT(23..54 bits)` and `DOUBLE PRECISION` map to `double`. | `[SQLPM/C §2 p.2-4]` |
| FR-002.11 | `DECIMAL(l, s)` and `PIC 9(l-s)V9(s)` map to a `decimal` array of `l+1`, with `l` in 1..18. | `[SQLPM/C §2 p.2-4]` |
| FR-002.12 | `unsigned long long` is rejected with a diagnostic, both as a host variable and — matching the manual's compiler restriction — anywhere in a unit containing embedded SQL. | `[SQLPM/C §2 p.2-5]` |
| FR-002.13 | Date-time types (`DATETIME`, `TIMESTAMP`, `DATE`, `TIME`) map to `char v[l+1]`. | `[SQLPM/C §2 p.2-4]` |
| FR-002.14 | `INTERVAL` maps to `char v[l+1]` where the extra byte holds the sign. | `[SQLPM/C §2 p.2-4]` |
| FR-002.15 | Host variable references accept an optional `INDICATOR` keyword; `:v :i` and `:v INDICATOR :i` are equivalent. | `[SQLPM/C §2 p.2-6]` |
| FR-002.16 | Indicator variables are `short`. On output, `-1` means null and `0` means not null. On input, any negative value inserts null. | `[SQLPM/C §2 pp.2-6, 2-17]` |
| FR-002.17 | `TYPE AS` accepts `DATETIME [start TO] end`, `DATE`, `TIME`, `TIMESTAMP`, and `INTERVAL start [(leading-precision)] [TO end]`, and propagates the asserted type to the runtime. | `[SQLPM/C §2 pp.2-6, 2-7]` |
| FR-002.18 | `SETSCALE` marks a host variable as carrying a given scale for the statement in which it appears. | `[SQLPM/C §2 pp.2-11, 2-12]` |
| FR-002.19 | A host variable declared with C type `fixed` is treated as scaled. | `[SQLPM/C §2 p.2-7]` |
| FR-002.20 | Only a `VARCHAR` structure's name is usable as a host variable; for any other structure, fields are host variables and must be referenced `:struct.field`. | `[SQLPM/C §2 pp.2-9, 2-10]` |
| FR-002.21 | A hand-declared `VARCHAR` length field must be `short`; `int` is rejected. | `[SQLPM/C §2 p.2-9]` |
| FR-002.22 | Conversion is performed within the character family and within the numeric family, never between them; a cross-family attempt is an error. | `[SQLPM/C §2 p.2-5]` |
| FR-002.23 | Character to shorter character right-truncates and returns a warning in `sqlcode`. | `[SQLPM/C §2 p.2-5]` |
| FR-002.24 | Character to longer character right-pads with blanks. | `[SQLPM/C §2 p.2-5]` |
| FR-002.25 | An input value too large for its column returns error 8300; the file-system detail (1031) is available through `SQLCAFSCODE`. | `[SQLPM/C §2 p.2-5]` |
| FR-002.26 | Fixed-point to floating-point transfer converts and returns a precision-loss warning. | `[SQLPM/C §2 p.2-11]` |
| FR-002.27 | Fixed-point to integer transfer stores the integral part and returns a data-loss warning. | `[SQLPM/C §2 p.2-11]` |
| FR-002.28 | On retrieval into a character array, no null terminator is appended. | `[SQLPM/C §2 p.2-7]` |
| FR-002.29 | `CHAR_AS_ARRAY` suppresses the extra byte that `CHAR_AS_STRING` implies for generated character declarations. | `[SQLPM/C §2 pp.2-7, 2-9]` |
| FR-002.30 | On input, a fixed-length character host variable transmits exactly the column's length in bytes, taken verbatim from the array — the runtime does **not** scan for a null terminator, does not truncate at one, and does not pad. An under-filled array therefore stores its null byte, exactly as SQL/MP does. | `[SQLPM/C §2 p.2-8]` |
| FR-002.31 | Blank-padding an under-filled array before insertion is the **program's** responsibility, not the runtime's; the runtime must not silently repair it. | `[SQLPM/C §2 p.2-8]`, Constitution III |
| NFR-002.1 | Every mapping row is covered by a round-trip conformance test: declare, insert, retrieve, compare. | Principle IV |
| NFR-002.2 | Width, signedness, and `sizeof` of every generated declaration are asserted statically. | Principle VI |

## 4. Acceptance scenarios

### AS-002.1 — Width fidelity across the type table
- **Given** a declare section with one host variable per row of the numeric
  mapping table
- **When** compiled
- **Then** static assertions confirm the width and signedness of each, and
  `INTEGER` is 32 bits on LP64
- **Test:** `tests/conformance/002/widths.sqlc`

### AS-002.2 — No terminator on retrieval
- **Given** a `char v[11]` host variable and a 10-character column whose value
  fills it
- **When** a row is retrieved
- **Then** `v[10]` is unmodified from its pre-call value — the runtime does not
  write a terminator
- **Test:** `tests/conformance/002/no_terminator.sqlc`

### AS-002.3 — Truncation warning
- **Given** a `char v[6]` receiving a 10-character column value
- **When** retrieved
- **Then** the value is right-truncated and `sqlcode` carries a warning
- **Test:** `tests/conformance/002/truncate_warn.sqlc`

### AS-002.4 — Scale loss warnings both ways
- **Given** a `NUMERIC(9,2)` column and both a `double` and an `int32_t` host
  variable
- **When** each retrieves the same scaled value
- **Then** the `double` retrieval warns for precision and the integer retrieval
  warns for the discarded fraction, and `SETSCALE` on the integer case preserves
  the fraction with no warning
- **Test:** `tests/conformance/002/scale_warn.sqlc`

### AS-002.5 — VARCHAR structure
- **Given** a `VARCHAR(26)` column and both an `INVOKE`-free hand declaration and
  a reference to the structure name as a host variable
- **When** used for insert and retrieve
- **Then** both work, and an `int` length field is rejected at preprocess time
- **Test:** `tests/conformance/002/varchar.sqlc`,
  `tests/conformance/002/negative/varchar_int_len.sqlc`

### AS-002.6 — Indicator round trip
- **Given** a nullable column
- **When** inserted with a negative indicator, then retrieved
- **Then** the stored value is null and the retrieval indicator is exactly `-1`
- **Test:** `tests/conformance/002/indicator.sqlc`

### AS-002.7 — Cross-family conversion refused
- **Given** a `char` host variable against an `INTEGER` column
- **When** used
- **Then** an error, not a coercion
- **Test:** `tests/conformance/002/negative/cross_family.sqlc`

## 5. Diagnostics

| Code | Condition | Default policy | Citation |
|------|-----------|----------------|----------|
| `ESQLC-2001` | `unsigned long long` in a unit with embedded SQL | error | `[§2 p.2-5]` |
| `ESQLC-2002` | Hand-declared `VARCHAR` length field not `short` | error | `[§2 p.2-9]` |
| `ESQLC-2003` | Non-`VARCHAR` structure name used as a host variable | error | `[§2 p.2-9]` |
| `ESQLC-2004` | Cross-family (character↔numeric) conversion requested | error | `[§2 p.2-5]` |
| `ESQLC-2005` | `DECIMAL` precision outside 1..18 | error | `[§2 p.2-4]` |
| `ESQLC-2006` | Unknown character-set keyword | error | `[§2 p.2-3]` |
| `ESQLC-2007` | Host variable declaration outside a declare section | error | `[§2 p.2-1]` |
| `ESQLC-2008` | `TYPE AS` qualifier combination not valid for the target type | error | `[§2 p.2-6]` |
| `ESQLC-2009` | Character host variable array size of 1 or less — no room for a column byte plus the terminator placeholder, so the on-the-wire width would be zero or negative | error | `[§2 p.2-7]` |
| `ESQLC-2010` | Character value right-truncated on retrieval | warn (runtime, via `sqlcode`) | `[§2 p.2-5]` |
| `ESQLC-2011` | Fixed-point precision loss to floating point | warn (runtime) | `[§2 p.2-11]` |
| `ESQLC-2012` | Fixed-point fraction discarded to integer | warn (runtime) | `[§2 p.2-11]` |

`ESQLC-2010..2012` are runtime warnings surfaced through `sqlcode`, not
preprocessor diagnostics. Their `sqlcode` values must match SQL/MP's — an
unresolved question below.

## 6. Open questions

| # | Question | Blocks | Resolution |
|---|----------|--------|------------|
| Q1 | What are SQL/MP's actual `sqlcode` warning values for truncation and the two scale-loss conditions? The manual names the conditions but not the codes. | FR-002.23, .26, .27 | unresolved — `DIV-042`. Confirmed absent from the C manual: it carries exactly one numbered warning (4315, unrelated — similarity checks). Needs `SQLRM` or the SQL message file, else values are chosen and published |
| Q2 | Does `SETSCALE`'s effect persist beyond the statement containing it? | FR-002.18 | unresolved |
| Q3 | Is C `fixed` a real HP C type this project should implement, or a NonStop compiler extension to be shimmed? | FR-002.19 | unresolved — `[EXTERNAL — CPG]` |
| Q4 | How are `KANJI`, `KSC5601`, and the ISO 8859 sets mapped onto MariaDB character sets, and what happens for `UNKNOWN`? | FR-002.4, .8 | unresolved — likely a new divergence |
| Q5 | Are a C storage class (`auto`/`extern`/`static`), a class modifier (`const`/`volatile`), and an initialiser permitted on a host variable definition inside a declare section? | FR-002.1, .2 | unresolved — ISO/IEC 9075-5:1999 §16.4 permits all three; the HP manual is silent. Customer code plausibly uses `static`, so silence is not safe to read as prohibition |
| Q6 | Are comma-separated multiple declarators permitted in one host variable definition? | FR-002.1 | unresolved — the standard permits them throughout §16.4; the HP manual's examples only ever show one per statement |

Q1 is the important one. Warning codes are what customer error handlers branch
on, so inventing them silently would breach Principle III; inventing them
*loudly*, as a registered divergence with a documented value, is acceptable.

## 7. Constitution check

| Principle | Compliant? | Note |
|-----------|-----------|------|
| I manual is the contract | partial | Q1 and Q3 are unresolved gaps; status stays `Clarifying` until they are closed or converted to divergences |
| II source compatibility | yes | Declaration syntax unchanged; `DIV-001` changes emitted type names but not widths |
| III no silent semantic change | yes | All four conversion warnings are requirements, not best-effort; cross-family conversion errors rather than coerces |
| IV manual-derived tests first | yes | NFR-002.1 requires a round trip per mapping row |
| V layered / frozen ABI | yes | Conversion and warning generation live in the runtime; the preprocessor only describes types to it |
| VI byte-exact structures | yes | NFR-002.2; `VARCHAR` field order and names fixed by FR-002.6 |
| VII divergence registered | partial | `DIV-001` accepted, `DIV-042` proposed for Q1. Q4 will produce one |
