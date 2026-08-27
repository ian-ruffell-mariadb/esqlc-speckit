# Reference: SQL/MP ↔ C data type mapping

Normative source: `[SQLPM/C §2 pp.2-3..2-16]`, `[SQLPM/C §2 p.2-25]`.
This sheet is a working distillation for implementers. Where it and the manual
disagree, the manual wins and this file is a bug.

## Character types

| SQL/MP declaration | Generated C |
|---|---|
| `CHAR(l)` / `PIC X(l)` | `char v[l+1]` |
| `CHARACTER(l) CHARACTER SET cs` | `char CHARACTER SET cs v[l+1]` |
| `NATIONAL CHARACTER(l)` | `char CHARACTER SET defcharset v[l+1]` |
| `VARCHAR(l)` | `struct { short len; char val[l+1]; } v` |
| `VARCHAR(l) CHARACTER SET cs` | as above, `val` carrying the character set |
| `NATIONAL CHARACTER VARYING(l)` | as above, `val` in `defcharset` |

The trailing `+1` byte is a **placeholder** for a null terminator, not a
guarantee one is present. Character-set keywords are `KANJI`, `KSC5601`,
`ISO8859n` (n = 1–9), and `UNKNOWN`. Absent a `CHARACTER SET` clause the set is
`UNKNOWN`. `defcharset` is the system default multibyte set, `KANJI` unless
changed at sysgen.

Rules that fall out of this and are frequent sources of customer bugs — so the
conformance suite must pin all four:

1. On retrieval into a `char` array, no null terminator is appended. The program
   appends it. `[§2 p.2-7]`
2. Fixed-length character columns are blank-padded in the database. An
   under-length array must be blank-padded by the program before `INSERT`, or a
   null byte is stored and comparisons stop matching. `[§2 p.2-8]`
3. A `VARCHAR` length field is `short`, never `int`, when declared by hand.
   `[§2 p.2-9]`
4. Whether `val` carries the extra byte depends on the SQL pragma's
   `CHAR_AS_STRING` option; `CHAR_AS_ARRAY` suppresses `INVOKE`'s extra byte.
   `[§2 pp.2-7, 2-9]`

## Numeric types

| SQL/MP | C |
|---|---|
| `NUMERIC(1..4, s) SIGNED`, `SMALLINT SIGNED` | `short` |
| `NUMERIC(1..4, s) UNSIGNED`, `SMALLINT UNSIGNED` | `unsigned short` |
| `NUMERIC(5..9, s) SIGNED`, `INTEGER SIGNED` | `long` |
| `NUMERIC(5..9, s) UNSIGNED`, `INTEGER UNSIGNED` | `unsigned long` |
| `NUMERIC(10..18, s) SIGNED`, `LARGEINT SIGNED` | `long long` |
| `PIC 9(l-s)V9(s) COMP` | as `NUMERIC` |
| `DECIMAL(l, s)`, `PIC 9(l-s)V9(s)` | `decimal[l+1]` |
| `FLOAT(1..22 bits)`, `REAL` | `float` |
| `FLOAT(23..54 bits)`, `DOUBLE PRECISION` | `double` |

`DECIMAL` precision `l` is 1–18. A `decimal` array is at most 20 bytes (19 + null
terminator), or 21 with a separate sign. `unsigned long long` is **not
supported** — not as a column type, and not even as an unrelated variable
elsewhere in a unit containing embedded SQL under the Guardian/OSS compilers.
`[§2 pp.2-4, 2-5]`

> **Portability note.** `long` is 32-bit on the NonStop C compilers this mapping
> was written for and 64-bit on LP64 Linux. This mapping is by *width*, not by
> C type name: feature 002 must emit width-exact types and register the
> divergence. This is `DIV-001`.

## Decimal handling

A SQL/MP `DECIMAL` is an ASCII digit string — a fixed-length character string
constrained to digits — so every fixed-length character rule above also applies.
Additionally `[§2 p.2-11]`:

- Declare one byte more than the digits stored.
- Append a null terminator before treating it as a C string.
- Right-justify and left-pad with ASCII `'0'` to the column's length before
  `INSERT`.
- HP C cannot do arithmetic on decimal strings directly; conversion goes through
  `dec_to_longlong` / `longlong_to_dec`. SQL/MP accepts **only** the embedded
  leading sign format, though `longlong_to_dec` can produce others.

## Fixed-point (scaled) types

C has no fixed-point type, so scale is communicated out of band `[§2 p.2-11]`:

- Fixed-point → floating-point host variable: converted, **warning** for
  precision loss.
- Fixed-point → integer host variable: integral part stored, **warning** for the
  discarded fraction.
- `SETSCALE(:hostvar, s)` makes SQL/MP treat a host variable as having scale `s`;
  a variable declared as C type `fixed` is also treated as scaled.

Both warnings are mandatory, per Constitution III. Silently truncating scale is
the single most damaging thing this implementation could do.

## Date-time and INTERVAL

| SQL/MP | C |
|---|---|
| `DATETIME`, `TIMESTAMP`, `DATE`, `TIME` | `char v[l+1]` |
| `INTERVAL` | `char v[l+1]` — the extra byte holds the sign |

Date-time values live in character host variables; the SQL type is asserted at
the point of use with the `TYPE AS` clause `[§2 pp.2-6, 2-7]`:

```
:hostvar [INDICATOR :ind]
         [TYPE AS { DATETIME [start TO] end | DATE | TIME | TIMESTAMP
                  | INTERVAL start [(leading-precision)] [TO end] }]
```

## Host-variable reference syntax

- Prefix with `:`. For a pointer host variable, the colon precedes the `*`.
- `hostvar` is any valid C identifier that is not the left-hand side of a
  `#define`.
- `INDICATOR` is optional as a keyword; `:hostvar :ind` is equivalent to
  `:hostvar INDICATOR :ind`.
- Indicator variables are `short`. On output: `-1` null, `0` not null. On input:
  any value `< 0` inserts null. `[§2 p.2-6]`
- A structure name is usable as a host variable **only** for `VARCHAR`. For any
  other structure the fields are the host variables, referenced
  `:struct.field`. `[§2 p.2-9]`

## Conversion rules

`[§2 p.2-5]`

- Conversion happens within character types and within numeric types, never
  between the two families.
- Character → shorter character: right-truncated, **warning** in `sqlcode`.
- Character → longer character: right-padded with blanks.
- Numeric: converts across signedness and across precisions.
- Input value too large for the column: **error 8300**; `SQLCADISPLAY` also
  surfaces file-system error 1031.
- `CAST` converts parameter types (character and numeric only) in dynamic SQL.
  `[EXTERNAL — SQLRM]`

## Implementation checklist for feature 002

- [ ] Width-exact emission, not type-name-exact (`DIV-001`)
- [ ] `CHAR_AS_STRING` / `CHAR_AS_ARRAY` both honoured
- [ ] `unsigned long long` rejected with a clear diagnostic
- [ ] All four truncation/scale warnings implemented and tested
- [ ] `TYPE AS` parsed and propagated to the runtime
- [ ] `SETSCALE` and C `fixed` both recognised
- [ ] `VARCHAR` struct name usable as a host variable; other struct names not
- [ ] Character-set keyword set complete, `UNKNOWN` default
