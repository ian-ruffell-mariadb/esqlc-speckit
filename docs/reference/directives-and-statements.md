# Reference: embeddable statements, directives, and pragmas

Normative source: `[SQLPM/C §3 pp.3-1..3-7]`. Syntax of individual statements is
deferred by the manual to `SQLRM` — every entry below marked `[EXTERNAL]`
needs a project decision before it is implementable.

## Embedding form

```
EXEC SQL <statement-or-directive> ;
```

Coding rules the preprocessor must enforce `[§3 pp.3-1..3-2]`:

- No nesting of statements or directives.
- Inside an embedded statement, only SQL comments (`--` to end of line). C
  comments are invalid there.
- Only `"` as the string delimiter.
- A statement may occupy one line or many; the terminating `;` is the delimiter.

## Placement classes

Where a construct may appear is part of the contract `[§3 p.3-2]`:

| Class | Constructs |
|---|---|
| Before all SQL and C statements (comments excepted) | `#pragma SQL` |
| With C variable declarations | `BEGIN`/`END DECLARE SECTION`, static `DECLARE CURSOR`, `INVOKE`, `INCLUDE STRUCTURES`, `INCLUDE SQLCA`/`SQLSA`/`SQLDA` |
| With C executable statements | DML, DCL, DDL, DSL, transaction control, dynamic SQL (incl. dynamic `DECLARE CURSOR`) |
| Anywhere | `WHENEVER`, `SQL SOURCE`, `CONTROL` directives |

`INCLUDE STRUCTURES` additionally must precede any `INCLUDE SQLCA`/`SQLSA`/
`SQLDA`, and for a multi-procedure compilation unit belongs in the global
declarations or the first procedure's declarations, whence it applies to the
whole unit. `[§9 p.9-1]`

## Statement inventory

### Data declaration directives — owned by 001/002/005/006/007

`BEGIN DECLARE SECTION`, `END DECLARE SECTION`, `INCLUDE STRUCTURES`,
`INCLUDE SQLCA`, `INCLUDE SQLDA`, `INCLUDE SQLSA`, `INVOKE`.

All are documented in SQLPM/C itself — no `[EXTERNAL]` gap.

### DML — owned by 004 and 007

`SELECT`, `INSERT`, `UPDATE`, `DELETE`, `DECLARE CURSOR`, `OPEN`, `FETCH`,
`CLOSE`. Documented in both SQLPM/C §4 and SQLRM.

### Error checking directives — owned by 005

`WHENEVER`.

### Dynamic SQL — owned by 007

`PREPARE`, `EXECUTE`, `EXECUTE IMMEDIATE`, `DESCRIBE`, `DESCRIBE INPUT`,
`RELEASE`, plus dynamic `DECLARE CURSOR`/`OPEN`/`FETCH`/`CLOSE`.

### Transaction control — owned by 003

`BEGIN WORK`, `COMMIT WORK`, `ROLLBACK WORK`. These start/commit/roll back a
**TMF** transaction. `[EXTERNAL — SQLRM]` and the primary semantic-gap area
against MariaDB: see `DIV-010`.

### DCL — owned by 008

| Statement | Note |
|---|---|
| `CONTROL EXECUTOR` | single vs. parallel executors |
| `CONTROL QUERY` | first-rows vs. all-rows optimisation, hash join, execution-time name resolution |
| `CONTROL TABLE` | locks, opens, buffers, access paths, join method and sequence |
| `FREE RESOURCES` | closes cursors, releases the program's locks |
| `LOCK TABLE` | locks a table (or a view's underlying tables) and its indexes |
| `UNLOCK TABLE` | releases locks on **nonaudited** tables and views |

Almost none of this maps onto MariaDB directly. `CONTROL *` are optimiser and
access-path hints with no MariaDB equivalent; `LOCK`/`UNLOCK TABLE` overlap
partially. Policy per statement is decided in 008, not improvised.

### DDL — owned by 008 (pass-through)

`ALTER CATALOG`, `ALTER COLLATION`, `ALTER INDEX`, `ALTER PROGRAM`,
`ALTER TABLE`, `ALTER VIEW`, `COMMENT`, `CREATE`, `DROP`, `HELP TEXT`,
`UPDATE STATISTICS`. All `[EXTERNAL — SQLRM]`.

The catalog/collation/program object classes have no MariaDB counterpart.
`ALTER PROGRAM` and program objects tie into §8 program invalidation, which does
not exist in this architecture at all — see `DIV-020`.

### DSL — owned by 008

`GET CATALOG OF SYSTEM`, `GET VERSION`, `GET VERSION OF PROGRAM`. The latter
returns PCV / PFV / HOSV of an SQL program file. All version-management concepts
that this implementation must answer synthetically or refuse.

## C compiler pragmas

`[§3 p.3-7]`

| Pragma | Purpose |
|---|---|
| `SQL` | Declares that the unit contains embedded SQL. Options include `SQLMAP` (emit an SQL map into the listing), `WHENEVERLIST` (log active `WHENEVER` options after each statement), and `RELEASE1`/`RELEASE2` (feature/structure version and the SQL/MP software version the program may run on). |
| `SQLMEM` | Places SQL internal structures in the user or extended data segment. TNS-only; ignored by the TNS/R native-mode compiler. |

`#pragma SQL` is mandatory and must precede all SQL and C statements; it may
instead be supplied as a compiler option. Also relevant from §6: `CHAR_AS_STRING`
/ `CHAR_AS_ARRAY` (see the type-mapping sheet), `CPPSOURCE`, and the OSS-side
`-Wsqlconnect` option with the `HP_NSK_CONNECT_MODE` environment variable
`[§6 pp.6-33..6-34]`.

`SQLMEM` and the memory-estimation guidance of App. B describe TNS segment
layout and have no analogue here — accept and ignore, registered as `DIV-030`.

## Preprocessor implications (feature 001)

1. The scanner needs only enough SQL awareness to find `EXEC SQL`, respect
   `--` comments and `"` strings, and locate the terminating `;`. Statement
   bodies are otherwise opaque token streams.
2. Placement is checkable without a full C parser, but the preprocessor must
   distinguish declaration context from executable context to enforce the table
   above. 001's plan must state how.
3. `WHENEVER` is lexically scoped over subsequent statements in source order and
   is a preprocessor-time construct — it emits inline checks, it is not a runtime
   call. `[§9 p.9-6]`
4. `SQL SOURCE` pulls in additional source; it interacts with `#include` and with
   line-number fidelity in emitted `#line` directives.
