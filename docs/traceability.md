# Traceability: manual coverage

One row per manual topic. `Owner` is the feature responsible; `Reqs` fills in as
specs are written; `Status` is `—` (unclaimed), `spec` (requirements written),
`planned`, **`partial`**, `tested`, or `done`.

**`partial` means a gate slice exercised part of the topic, not all of it.** It
exists because four vertical slices have deliberately covered corners of many
topics — a slice proves the spine, never the breadth. Marking those `tested`
would overstate coverage, and leaving them `spec` understates it. Where a row is
`partial`, the gap is named.

Coverage as of Gate 9 (2026-09-03): Gate 1 insert · Gate 2 retrieval ·
Gate 3 read-only cursors · Gate 4 WHENEVER and the SQLCA · Gate 5 the SQLSA ·
Gate 6 searched UPDATE and DELETE.

`/speckit.analyze` fails if any topic is owned by two features or by none.

## Section 1 — Introduction

| Topic | Owner | Reqs | Status |
|---|---|---|---|
| Orientation only; no normative content | — | — | n/a |

## Section 2 — Host Variables

| Topic | Owner | Reqs | Status |
|---|---|---|---|
| Declare section syntax and placement | 002 | G1 | tested |
| Host variable naming rules | 002 | G1 | tested |
| SQL↔C character type mapping (Table 2-1) | 002 | G1/G8: char[] and VARCHAR with a charset; no NCHAR, no CHAR_AS_ARRAY | partial |
| SQL↔C numeric/date-time type mapping (Table 2-2) | 002 | G7: 16/32/64-bit, float, double; no decimal or fixed-point (002 Q2/Q3) | partial |
| Data conversion and warning conditions | 002 | G2: cross-family refused; warnings never fired (DIV-042) | partial |
| `CAST` in dynamic SQL | 007 | | spec |
| Host variable reference syntax, `INDICATOR`, `TYPE AS` | 002 | G1/G2/G6/G7: refs + INDICATOR both directions; no TYPE AS | partial |
| Fixed-length character rules (null terminator, blank padding) | 002 | G1 insert-side, G2 retrieval-side | tested |
| Variable-length character (`VARCHAR`) struct form | 002 | G7 | tested |
| Structures as host variables | 002 | G7: VARCHAR shape only; any other structure refused (ESQLC-2003) | partial |
| Decimal data types and conversion routines | 002 | | spec |
| Fixed-point types, `SETSCALE`, C `fixed` | 002 | | spec |
| Date-time and INTERVAL host variables | 002 | G7: date-time column into char; no TYPE AS, no INTERVAL | partial |
| Indicator variables for null values | 002 | G2 | tested |
| `INVOKE` directive and generated structures | 006 | G9: base tables from a committed cache; no NCHAR, no MAP DEFINE, no PREFIX/SUFFIX | partial |
| `INVOKE` with indicator variables | 006 | G9: one per nullable column, preceding it, spelled _i | tested |
| `INVOKE` with SQLCI | 008 | | spec |
| Character set association with host variables | 002 | G8: ISO8859-1/2/7/8/9 and KSC5601; KANJI and 8859-3/4/5/6 refused (DIV-055) | partial |

## Section 3 — Statements and Directives

| Topic | Owner | Reqs | Status |
|---|---|---|---|
| `EXEC SQL` embedding form and coding rules | 001 | G1 | tested |
| Placement classes | 001 | G1, G4 (PosClass::Any) | tested |
| `SQL` pragma and its options | 001 | G1: bare pragma; option set frozen, untested | partial |
| `SQLMEM` pragma | 008 | | spec |
| Statement inventory (Table 3-1) — dispatch | 001 | G1-G4 via ESQLC-1012 | tested |
| DDL statements | 008 | | spec |
| DCL statements (`CONTROL *`, `LOCK`/`UNLOCK TABLE`, `FREE RESOURCES`) | 008 | | spec |
| DSL statements (`GET *`) | 008 | | spec |
| Transaction control (`BEGIN`/`COMMIT`/`ROLLBACK WORK`) | 003 | G1: shape only; semantics open (003 Q1/Q2) | partial |

## Section 4 — Data Retrieval and Modification

| Topic | Owner | Reqs | Status |
|---|---|---|---|
| Opening/closing tables and views | 003 | | spec |
| SQL error 8204 (lost open) and recovery | 008 | | spec |
| Single-row `SELECT` | 004 | G2 | tested |
| Multirow `SELECT` | 004 | | spec |
| `INSERT` (incl. nulls, timestamps) | 004 | G1/G6/G7: plain, nulls via indicator; no timestamp on the write path | partial |
| `UPDATE` (single, multiple, null columns) | 004 | G6 searched; positioned needs 004 Q3/Q7 | partial |
| `DELETE` (single, multiple) | 004 | G6 searched; positioned needs 004 Q3/Q7 | partial |
| Cursor lifecycle and steps | 004 | G3 read-only | tested |
| PAID requirements per statement | 008 | | spec |
| Cursor position rules (Table 4-2) | 004 | G3: specified rows only; 004 Q6/Q7 open | partial |
| Cursor stability | 004 | | spec |
| VSBB | 008 | | spec |
| `DECLARE CURSOR` / `OPEN` / `FETCH` / `CLOSE` | 004 | G3 | tested |
| Cursor `UPDATE` / `DELETE` | 004 | | spec |
| Foreign cursors | 008 | | spec |

## Section 5 — System Procedures

| Topic | Owner | Reqs | Status |
|---|---|---|---|
| `cextdecs` header dependency | 008 | | spec |
| SQL message file | 005 | | spec |
| `SQLCADISPLAY` | 005 | | spec |
| `SQLCAFSCODE` | 005 | G4 | tested |
| `SQLCAGETINFOLIST` (+ error and item codes) | 005 | G4: numeric items only | partial |
| `SQLCATOBUFFER` | 005 | | spec |
| `SQLSADISPLAY` | 005 | | spec |
| `SQLGETCATALOGVERSION` / `OBJECTVERSION` / `SYSTEMVERSION` | 008 | | spec |
| Guardian procedures returning SQL info (Table 5-2) | 008 | | spec |

## Section 6 — Explicit Program Compilation

| Topic | Owner | Reqs | Status |
|---|---|---|---|
| Compilation pipeline shape | 001 | | spec |
| Guardian development flow, TACL DEFINEs, SQL pragma placement | 008 | | spec |
| TNS / TNS/R NMC / TNS/E CCOMP compilers | 008 | | spec |
| `BIND` step | 008 | | spec |
| SQL compiler invocation and options | 008 | | spec |
| SQL program file format | 008 | | spec |
| SQL compiler listings (`SQLMAP`, `WHENEVERLIST`) | 001 | | spec |
| OSS flow, `c89`, `-Wsqlconnect`, `HP_NSK_CONNECT_MODE` | 008 | | spec |
| PC host environment | 008 | | spec |
| `CONTROL` directives, static vs dynamic | 008 | | spec |
| Compatible compilation tools / version matching | 008 | | spec |

## Section 7 — Program Execution

| Topic | Owner | Reqs | Status |
|---|---|---|---|
| Required access authority | 008 | | spec |
| TACL DEFINEs at run time | 008 | | spec |
| `RUN` command | 008 | | spec |
| OSS execution | 003 | | spec |
| Low PIN execution | 008 | | spec |
| Interactive vs programmatic commands | 008 | | spec |
| Pathway environment | 008 | | spec |
| SQL executor compatibility | 008 | | spec |

## Section 8 — Program Invalidation and Recompilation

| Topic | Owner | Reqs | Status |
|---|---|---|---|
| Program invalidation causes and prevention | 008 | | spec |
| SQL compiler validation functions | 008 | | spec |
| File-label/catalog inconsistencies | 008 | | spec |
| Automatic SQL recompilation and its run-time errors | 008 | | spec |

## Section 9 — Error and Status Reporting

| Topic | Owner | Reqs | Status |
|---|---|---|---|
| `INCLUDE STRUCTURES` and version selection | 005 | G5: VERSION n; not EXTERNAL, not VERSION CURRENT | partial |
| Default-to-version-2 behaviour and message | 005 | G4 | tested |
| C compiler version check / error 11203 | 005 | | spec |
| Sharing structures (`EXTERNAL`) | 005 | | spec |
| `sqlcode` semantics | 005 | G1-G4 | tested |
| `WHENEVER` directive, actions, precedence | 005 | G4: SQLWARNING never fired (DIV-042) | partial |
| `SQLCA` declaration and access | 005 | G4: private layout, accessor-only (DIV-041) | partial |
| `SQLSA` declaration, reset semantics, fields | 005 | G5: v300 populated, v330 layout only; prepare arm never populated | partial |

## Section 10 — Dynamic SQL

| Topic | Owner | Reqs | Status |
|---|---|---|---|
| Dynamic statement inventory | 007 | | spec |
| `SQLDA`, names buffer, collation buffer | 007 | | spec |
| `SQLDA` field encodings and type codes | 007 | | spec |
| Character-set ID checking | 007 | | spec |
| Input parameters and output variables | 007 | | spec |
| Null values in dynamic SQL | 007 | | spec |
| Dynamic memory allocation of descriptors | 007 | | spec |
| Dynamic SQL cursors | 007 | | spec |
| Dynamic SQL program development sequence | 007 | | spec |
| Dynamic SQL Pathway server | 008 | | spec |

## Section 11 — CPRL

| Topic | Owner | Reqs | Status |
|---|---|---|---|
| All 22 `CPRL_*` procedures and return codes | 008 | | spec |

## Appendices

| Topic | Owner | Reqs | Status |
|---|---|---|---|
| A — sample database (test fixture source) | 006 | | spec |
| B — memory considerations, `SQLMEM` | 008 | | spec |
| C — maximising local autonomy | 008 | | spec |
| D — converting C programs, legacy SQLDA v1/v2 | 007 | | spec |
| D — `RELEASE1`/`RELEASE2` options | 008 | | spec |
