# Reference: SQLCA, SQLSA, SQLDA

Normative sources: `[SQLPM/C §9 pp.9-1..9-18]`, `[SQLPM/C §10 pp.10-3..10-11]`,
`[SQLPM/C App. D]`. Layout is API (Constitution VI) — customer code indexes into
these, shares them `EXTERNAL`, and allocates extra copies using the length
constants.

## Version selection

```
EXEC SQL INCLUDE STRUCTURES <spec>... ;
```

where `<spec>` is one of `[ALL] VERSION v`, `{SQLCA|SQLSA|SQLDA} VERSION v`,
`SQLSA VERSION CURRENT`, or `{SQLCA|SQLSA} [EXTERNAL]`. Versions: 1, 2, 300, 340
or later; 330 applies to `SQLSA` only. Different versions per structure in one
directive are legal. `[§9 p.9-2]`

Behaviours the implementation must reproduce:

- **Omitting the directive yields version 2 structures** plus an informational
  compilation message warning that later-version features may produce incorrect
  results. Feature 005 must emit an equivalent message. `[§9 p.9-1]`
- Requesting a version the compiler cannot generate is **SQL error 11203**.
  `[§9 p.9-3]`
- `EXTERNAL` declares without allocating; exactly one non-`EXTERNAL`
  `INCLUDE SQLCA` / `INCLUDE SQLSA` must exist in the program. `[§9 p.9-3]`
- `SQLSA VERSION CURRENT` generates **both** version 300 and version 330 layouts
  for runtime version selection. It requires SQL/MP software 340+, the NMC
  compiler (or TNS C with `CPPSOURCE`), and a `SQLGETSYSTEMVERSION` declaration
  because the option emits a call to it. `[§9 p.9-2]`
- Version 330+ SQLSA carries a field-alignment pragma
  (`fieldalign cshared2`) over `SQLSA_TYPE_R330`, `DML_TYPE_R330`,
  `STATS_TYPE_R330`, `PREPARE_TYPE_R330`. `[§9 p.9-14]`

## SQLCA — communications area

Holds run-time status, errors, and warnings for the most recent statement or
directive. Up to **seven** error or warning codes from a single statement, in any
combination. `[§9 p.9-12]`

| Identifier | Value |
|---|---|
| `SQLCA_EYE_CATCHER` | `CA` |
| `SQLCA_LEN` | 430 |

The program initialises the eye-catcher; SQL/MP does not. Field-level content is
documented indirectly, via the `SQLCAGETINFOLIST` item codes in §5 — feature 005
must work from there, not from a field list in §9, because §9 does not provide
one.

Access procedures `[§9 p.9-12]`:

| Procedure | Purpose |
|---|---|
| `SQLCADISPLAY` | write messages to a file or terminal |
| `SQLCATOBUFFER` | write messages into a program record area |
| `SQLCAGETINFOLIST` | write a selected subset into a program area |
| `SQLCAFSCODE` | return file-system / disk-process / OS error detail |

All four are TAL procedures reached through `cextdecs`. They become `esqlc_*`
runtime functions here, with `cextdecs` shimmed — see feature 008.

## SQLSA — statistics area

Populated after `INSERT`, `UPDATE`, `DELETE`, `SELECT … INTO`, and cursor
`OPEN`/`CLOSE`/`FETCH` where the cursor's `DECLARE` carried a `SELECT`. For
dynamic SQL, populated after `PREPARE`, `DESCRIBE`, and `DESCRIBE INPUT`.
**Undefined** after DSL, DDL, DCL, and transaction control statements.
`[§9 p.9-13]`

Every statement **resets** it — including every `FETCH`, so cursor statistics
require program-side accumulators. `[§9 pp.9-13, 9-14]`

| Identifier | Value |
|---|---|
| `SQLSA_EYE_CATCHER` | `SA` |
| `SQLSA_LEN` | 838 (versions 300–325) or 1790 (version 330+) |

### Fields `[§9 pp.9-17..9-18]`

Header: `eye_catcher`, `version`.

`dml` substructure:

| Field | Meaning |
|---|---|
| `num_tables` | tables accessed by the statement; max 16 |
| `master_executor_elapsed_time` | master Executor CPU µs — v330+ only |
| `total_esp_cpu_time` | total ESP CPU µs — v330+ only |
| `total_sortprog_cpu_time` | total SORTPROG CPU µs — v330+ only |
| `stats[]` | `num_tables` valid entries, one per table |

`stats[]` entries: `table_name` (Guardian internal file name),
`records_accessed`, `records_used`, `disc_reads`, `messages`, `message_bytes`,
`waits` (lock waits or timeouts), `escalations` (record→file lock escalations),
`sqlsa_reserved`, `vsbb_write`, `vsbb_flushed`. The two VSBB flags use `-1` for
true and `0` for false.

`prepare` substructure (dynamic SQL only): `input_num`, `input_names_len`,
`output_num`, `output_names_len`, `name_map_len` (reserved),
`sql_statement_type`, `output_collations_len`.

`sql_statement_type` values, declared in the `sqlh` header:

| Value | Declaration | Statement |
|---|---|---|
| 1 | `_SQL_STATEMENT_SELECT` | cursor `SELECT` |
| 2 | `_SQL_STATEMENT_INSERT` | `INSERT` |
| 3 | `_SQL_STATEMENT_UPDATE` | `UPDATE` |
| 4 | `_SQL_STATEMENT_DELETE` | `DELETE` |
| 5 | `_SQL_STATEMENT_DDL` | any DDL |
| 6 | `_SQL_STATEMENT_CONTROL` | run-time `CONTROL TABLE` |
| 7 | `_SQL_STATEMENT_DCL` | `LOCK`, `UNLOCK`, `FREE RESOURCES` |
| 8 | `_SQL_STATEMENT_GET` | `GET VERSION…` |

Several `stats[]` fields are meaningless against MariaDB (`disc_reads`,
`messages`, `message_bytes`, `escalations`, both VSBB flags, and all three v330
CPU-time fields, which measure NonStop process structure). Feature 005 must
decide per field: derive an honest analogue, or return a documented sentinel.
Zero-filling silently is prohibited by Constitution III. This is `DIV-011`.

## SQLDA — descriptor area

Describes input parameters and output variables for dynamic SQL, alongside a
**names buffer** and a **collation buffer**. `[§10 pp.10-3..10-11]`

| Identifier | Value | Meaning |
|---|---|---|
| `SQLDA_EYE_CATCHER` | `D1` | program-initialised |
| `SQLDA_HEADER_LEN` | 4 | `eye_catcher` + `num_entries` |
| `SQLDA_SQLVAR_LEN` | 24 | one `sqlvar` entry |
| `SQLDA_NAMESBUF_OVHD_LEN` | 11 | names-buffer overhead: 2-byte length + 8-byte table name + 1-byte separator |

### Fields `[§10 pp.10-5..10-6]`

| Field | Meaning |
|---|---|
| `eye_catcher` | program sets it; SQL/MP never writes it |
| `num_entries` | capacity in parameters/variables |
| `sqlvar[]` | one entry per input parameter (`DESCRIBE INPUT`) or output variable (`DESCRIBE`) |
| `data_type` | see type codes below |
| `data_len` | encoding depends on type — see below |
| `precision` | encoding depends on type — see below |
| `null_info` | negative if the column permits null (input) or the returned row is null (output) |
| `var_ptr` | extended address of the data; **program-initialised**, never returned |
| `ind_ptr` | address of the null flag; `-1` at that location means null. A program not handling nulls sets this to an invalid address |
| `cprl_ptr` | output only: address of the collation used, or a negative integer if none |

`data_len` encoding:

| Type family | `data_len` |
|---|---|
| Fixed-length character | byte count |
| Variable-length character | maximum byte count |
| Decimal numeric | bits 0:7 scale, bits 8:15 byte length |
| Binary numeric | bits 0:7 scale, bits 8:15 byte length (2, 4, or 8) |
| Date-time / INTERVAL | bits 0:7 field range (qualifier code), bits 8:15 storage size |

`precision` encoding:

| Type family | `precision` |
|---|---|
| Binary numeric | numeric precision |
| Date-time / INTERVAL | bits 0:7 leading-field precision, bits 8:15 fraction precision (0 if no FRACTION field) |
| CHAR / VARCHAR | character-set ID |

### `data_type` codes

Character (0–127):

| 0 | `_SQLDT_ASCII_F` | fixed, single-byte |
| 1 | `_SQLDT_ASCII_F_UP` | fixed, single-byte, upshifted |
| 2 | `_SQLDT_DOUBLE_F` | fixed, double-byte |
| 64 | `_SQLDT_ASCII_V` | varying, single-byte |
| 65 | `_SQLDT_ASCII_V_UP` | varying, single-byte, upshifted |
| 66 | `_SQLDT_DOUBLE_V` | varying, double-byte |

Numeric (128–141):

| 130 | `_SQLDT_16BIT_S` | signed SMALLINT |
| 131 | `_SQLDT_16BIT_U` | unsigned SMALLINT |
| 132 | `_SQLDT_32BIT_S` | signed INT |
| 133 | `_SQLDT_32BIT_U` | unsigned INT |
| 134 | `_SQLDT_64BIT_S` | signed LARGEINT |
| 140 | `_SQLDT_REAL` | 32-bit float |
| 141 | `_SQLDT_DOUBLE` | 64-bit float |

Decimal (150–154):

| 150 | `_SQLDT_DEC_U` | unsigned |
| 151 | `_SQLDT_DEC_LSS` | leading sign separate (not an SQL type) |
| 152 | `_SQLDT_DEC_LSE` | ASCII, leading sign embedded |
| 153 | `_SQLDT_DEC_TSS` | trailing sign separate (not an SQL type) |
| 154 | `_SQLDT_DEC_TSE` | trailing sign embedded (not an SQL type) |

Date-time and INTERVAL (192–212): `192 _SQLDT_DATETIME` for general date-time,
then 195–212 for the INTERVAL start-to-end combinations
(`_SQL_DTINT_Y_Y` 195, `_MO_MO` 196, `_Y_MO` 197, `_D_D` 198, `_H_H` 199,
`_D_H` 200, `_MI_MI` 201, `_H_MI` 202, `_D_MI` 203, `_S_S` 204, `_MI_S` 205,
`_H_S` 206, `_D_S` 207, `_F_F` 208, `_S_F` 209, `_MI_F` 210, `_H_F` 211,
`_D_F` 212).

### Date-time qualifier codes for `data_len` bits 0:7

Codes 1–28, `_SQL_DTINT_QUAL_*`: 1 Y_Y, 2 MO_MO, 3 D_D, 4 H_H, 5 MI_MI, 6 S_S,
7 F_F, 8 Y_MO, 9 Y_D, 10 Y_H, 11 Y_MI, 12 Y_S, 13 Y_F, 14 MO_D, 15 MO_H,
16 MO_MI, 17 MO_S, 18 MO_F, 19 D_H, 20 D_MI, 21 D_S, 22 D_F, 23 H_MI, 24 H_S,
25 H_F, 26 S_S, 27 S_F, 28 F_F.

Note codes 26 and 28 duplicate the ranges of 6 and 7. Reproduce the table as
published; do not "fix" it. Feature 007 must accept both encodings on input and
must document which it emits.

### Character-set IDs in `precision`

| 0 | `_SQL_CHARSETID_UNKNOWN` | single-byte, unknown |
| 1 | `_SQL_CHARSETID_KANJI` | Japanese (Shift-JIS) |
| 12 | `_SQL_CHARSETID_KSC5601` | Korean |
| 101–109 | `_SQL_CHARSETID_8859n` | ISO 8859/1 … 8859/9 |

At execution SQL/MP checks `precision` against the column's
`COLUMNS.CHARACTERSET` and errors on mismatch — except that a program expecting
`UNKNOWN` against any single-byte column is accepted. `[§10 p.10-11]`

## Input parameters

A parameter is `?` or `?name` and may appear anywhere a constant may. Programs
use `DESCRIBE INPUT` to learn about them. `[§10 p.10-11]`

## Legacy versions

App. D specifies version 1 and version 2 SQLDA and SQLSA layouts and the changes
between versions. Feature 007 owns version 300+; version 1 and 2 SQLDA support
is a scoped decision in 007's spec, since version 2 is the **default** when
`INCLUDE STRUCTURES` is omitted — so it cannot simply be dropped.
