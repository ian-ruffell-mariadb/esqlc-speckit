# Reference: NonStop-specific surface

Everything in the manual that describes the NonStop platform rather than the
embedded-SQL language. This is the material that cannot be implemented, only
shimmed, refused, or synthesised — and therefore the material most likely to
break a real customer program in a way nobody predicted.

Feature 008 owns this sheet. Every row needs an explicit policy
(`error` / `warn` / `ignore` / `emulate`) before 008 is `Ready`.

## Object naming

| Construct | Manual | Note |
|---|---|---|
| `\node.$volume.subvol.file` | §3 examples | Fully-qualified Guardian names appear directly in embedded DDL/DML |
| `=define-name` | §6, §7, App. C | TACL DEFINE indirection, e.g. `=shipments`; resolved at run time |
| `$vol` / partition names | App. C | Partition-level addressing for local autonomy |

TACL DEFINEs are the hard one: they are a run-time name-resolution layer with no
MariaDB analogue, and the manual's own examples use them constantly. 008 must
specify a mapping mechanism (configuration file? environment?) or refuse them.
This is `DIV-002`.

## Compilation and program objects (§6, §8)

| Concept | Note |
|---|---|
| Explicit SQL compilation as a separate pass after the C compiler and `BIND` | The SQL compiler writes a compiled plan into the program file |
| TNS / TNS/R (NMC) / TNS/E (CCOMP) compiler families | Three toolchains, differing pragma support |
| SQL program file format | Plans, timestamps, and version stamps embedded in the object |
| Program invalidation | A program's plan is invalidated by DDL changes, timestamp mismatch, file-label/catalog inconsistency |
| Automatic SQL recompilation | At run time, on invalidation, with its own error class |
| PCV / PFV / HOSV | Program catalog, program format, and host object SQL versions |
| `SQLMAP`, `WHENEVERLIST` listing output | Compiler listing artefacts |
| `c89` in OSS, `-Wsqlconnect`, `HP_NSK_CONNECT_MODE` | OSS-side toolchain surface |
| PC cross-compilation host environment | §6 |

**Nothing in this class exists in the target architecture.** There is no stored
plan, so there is no invalidation and no automatic recompilation. `GET VERSION OF
PROGRAM` has nothing to report. This is the largest single divergence in the
project (`DIV-020`) and 008's spec must state what `GET VERSION OF PROGRAM`
returns and what happens to programs whose error handling branches on
recompilation codes.

## Execution environment (§7)

| Concept | Note |
|---|---|
| Required access authority; process access ID (PAID) | §7, and PAID checks recur throughout §4 for every DML statement and cursor operation |
| TACL `RUN` command and DEFINEs at run time | §7 |
| Running at a low PIN | §7 |
| Pathway environment / Pathway servers | §7, §10 |
| SQL executor compatibility check | §7 |

PAID requirements are cited against `SELECT`, `INSERT`, `UPDATE`, `DELETE`,
cursors, `OPEN`, `FETCH`, `INVOKE`, and `UPDATE STATISTICS` — the manual treats
process identity as part of DML semantics. MariaDB's authorisation model is
per-connection user, not per-process. 008 must map this; 004 must not silently
ignore it.

## Locking, concurrency, and access paths

| Concept | Manual | MariaDB analogue |
|---|---|---|
| TMF transactions (`BEGIN`/`COMMIT`/`ROLLBACK WORK`) | §3 | InnoDB transactions — close, but not identical in isolation or in distributed scope |
| Cursor stability | §4 p.4-17 | partial; isolation-level dependent |
| Cursor position semantics (Table 4-2) | §4 p.4-16 | must be emulated exactly |
| VSBB (virtual sequential block buffering) | §4 pp.4-17..4-18 | none; SQLSA VSBB flags become sentinels |
| Record→file lock escalation | §9 `stats[]` | none |
| `CONTROL TABLE` / `CONTROL QUERY` / `CONTROL EXECUTOR` | §3 | optimiser hints, mostly unmappable |
| SQL error 8204 (lost open) and its recovery procedure | §4 pp.4-2..4-3 | none — cannot occur |
| Foreign cursors | §4 p.4-24 | none |

## System procedures (§5)

TAL procedures called through `cextdecs`, plus a SQL message file:

| Procedure | Class |
|---|---|
| `SQLCADISPLAY`, `SQLCATOBUFFER`, `SQLCAGETINFOLIST`, `SQLCAFSCODE` | error/warning access — **must be implemented** (feature 005) |
| `SQLSADISPLAY` | statistics display — must be implemented (005) |
| `SQLGETCATALOGVERSION`, `SQLGETOBJECTVERSION`, `SQLGETSYSTEMVERSION` | version enquiry — synthetic answers (008) |

`SQLCAGETINFOLIST` has its own error codes and item codes (§5 pp.5-11) — those
item codes are the only field-level documentation of SQLCA content anywhere in
the manual, so feature 005 depends on them.

`SQLGETSYSTEMVERSION` is not optional: `INCLUDE STRUCTURES SQLSA VERSION
CURRENT` generates a call to it.

## Character Processing Rules (§11)

Twenty-two `CPRL_*` procedures for collation-aware comparison, encode/decode,
case shifting, character classification, and collation object access:
`CPRL_ARE_`, `CPRL_AREALPHAS_`, `CPRL_ARENUMERICS_`, `CPRL_COMPARE_`,
`CPRL_COMPARE1ENCODED_`, `CPRL_COMPAREOBJECTS_`, `CPRL_DECODE_`, `CPRL_ENCODE_`,
`CPRL_DOWNSHIFT_`, `CPRL_UPSHIFT_`, `CPRL_GETALPHATABLE_`,
`CPRL_GETCHARCLASSTABLE_`, `CPRL_GETDOWNSHIFTTABLE_`, `CPRL_GETUPSHIFTTABLE_`,
`CPRL_GETNUMTABLE_`, `CPRL_GETSPECIALTABLE_`, `CPRL_GETFIRST_`, `CPRL_GETLAST_`,
`CPRL_GETNEXTINSEQUENCE_`, `CPRL_INFO_`, `CPRL_READOBJECT_`, plus shared return
codes.

These operate on SQL/MP **collation objects**, which are first-class database
objects (`CREATE COLLATION`, `ALTER COLLATION`) and appear in the SQLDA via
`cprl_ptr` and the collation buffer. MariaDB collations are not objects and are
not programmatically enumerable in this form.

008 must decide: implement over MariaDB collations where the semantics permit
(`UPSHIFT`/`DOWNSHIFT`/`COMPARE` plausibly can), refuse the object-access
procedures, or refuse the lot. `cprl_ptr` in the SQLDA forces at least a decision
on the "no collation used" negative-integer case.

## Memory (App. B)

`SQLMEM` pragma, user vs. extended data segment placement, per-statement virtual
memory estimates, stack-overflow avoidance, 16 KB real memory pages. Entirely
TNS-specific. Accept-and-ignore is the only sensible policy; registered as
`DIV-030`.

## Local autonomy (App. C)

Using a local partition, TACL DEFINEs, current statistics, and skipping
unavailable partitions. A distributed-database availability feature with no
single-instance MariaDB meaning. Requires an explicit refusal rather than silent
acceptance, since "skip unavailable partitions" changes result sets.

## Version management

`RELEASE1` / `RELEASE2` pragma options, structure versions 1/2/300/315/330/340,
catalog and object versions, `GET VERSION`, and the interaction between the C
compiler's HOSV and requestable structure versions. The manual defers the model
to `VMG` `[EXTERNAL]`.

Feature 008 must define a synthetic version story: what version this
implementation reports as, and what it does when a program requests a structure
version it does not implement (the manual's answer is SQL error 11203, which is
a good precedent to reuse).
