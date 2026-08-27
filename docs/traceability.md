# Traceability: manual coverage

One row per manual topic. `Owner` is the feature responsible; `Reqs` fills in as
specs are written; `Status` is `—` (unclaimed), `spec` (requirements written),
`planned`, `tested`, or `done`.

`/speckit.analyze` fails if any topic is owned by two features or by none.

## Section 1 — Introduction

| Topic | Owner | Reqs | Status |
|---|---|---|---|
| Orientation only; no normative content | — | — | n/a |

## Section 2 — Host Variables

| Topic | Owner | Reqs | Status |
|---|---|---|---|
| Declare section syntax and placement | 002 | | spec |
| Host variable naming rules | 002 | | spec |
| SQL↔C character type mapping (Table 2-1) | 002 | | spec |
| SQL↔C numeric/date-time type mapping (Table 2-2) | 002 | | spec |
| Data conversion and warning conditions | 002 | | spec |
| `CAST` in dynamic SQL | 007 | | spec |
| Host variable reference syntax, `INDICATOR`, `TYPE AS` | 002 | | spec |
| Fixed-length character rules (null terminator, blank padding) | 002 | | spec |
| Variable-length character (`VARCHAR`) struct form | 002 | | spec |
| Structures as host variables | 002 | | spec |
| Decimal data types and conversion routines | 002 | | spec |
| Fixed-point types, `SETSCALE`, C `fixed` | 002 | | spec |
| Date-time and INTERVAL host variables | 002 | | spec |
| Indicator variables for null values | 002 | | spec |
| `INVOKE` directive and generated structures | 006 | | spec |
| `INVOKE` with indicator variables | 006 | | spec |
| `INVOKE` with SQLCI | 008 | | spec |
| Character set association with host variables | 002 | | spec |

## Section 3 — Statements and Directives

| Topic | Owner | Reqs | Status |
|---|---|---|---|
| `EXEC SQL` embedding form and coding rules | 001 | | spec |
| Placement classes | 001 | | spec |
| `SQL` pragma and its options | 001 | | spec |
| `SQLMEM` pragma | 008 | | spec |
| Statement inventory (Table 3-1) — dispatch | 001 | | spec |
| DDL statements | 008 | | spec |
| DCL statements (`CONTROL *`, `LOCK`/`UNLOCK TABLE`, `FREE RESOURCES`) | 008 | | spec |
| DSL statements (`GET *`) | 008 | | spec |
| Transaction control (`BEGIN`/`COMMIT`/`ROLLBACK WORK`) | 003 | | spec |

## Section 4 — Data Retrieval and Modification

| Topic | Owner | Reqs | Status |
|---|---|---|---|
| Opening/closing tables and views | 003 | | spec |
| SQL error 8204 (lost open) and recovery | 008 | | spec |
| Single-row `SELECT` | 004 | | spec |
| Multirow `SELECT` | 004 | | spec |
| `INSERT` (incl. nulls, timestamps) | 004 | | spec |
| `UPDATE` (single, multiple, null columns) | 004 | | spec |
| `DELETE` (single, multiple) | 004 | | spec |
| Cursor lifecycle and steps | 004 | | spec |
| PAID requirements per statement | 008 | | spec |
| Cursor position rules (Table 4-2) | 004 | | spec |
| Cursor stability | 004 | | spec |
| VSBB | 008 | | spec |
| `DECLARE CURSOR` / `OPEN` / `FETCH` / `CLOSE` | 004 | | spec |
| Cursor `UPDATE` / `DELETE` | 004 | | spec |
| Foreign cursors | 008 | | spec |

## Section 5 — System Procedures

| Topic | Owner | Reqs | Status |
|---|---|---|---|
| `cextdecs` header dependency | 008 | | spec |
| SQL message file | 005 | | spec |
| `SQLCADISPLAY` | 005 | | spec |
| `SQLCAFSCODE` | 005 | | spec |
| `SQLCAGETINFOLIST` (+ error and item codes) | 005 | | spec |
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
| `INCLUDE STRUCTURES` and version selection | 005 | | spec |
| Default-to-version-2 behaviour and message | 005 | | spec |
| C compiler version check / error 11203 | 005 | | spec |
| Sharing structures (`EXTERNAL`) | 005 | | spec |
| `sqlcode` semantics | 005 | | spec |
| `WHENEVER` directive, actions, precedence | 005 | | spec |
| `SQLCA` declaration and access | 005 | | spec |
| `SQLSA` declaration, reset semantics, fields | 005 | | spec |

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
