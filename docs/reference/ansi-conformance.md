# Reference: SQL/MP embedded C vs ISO/IEC 9075-5:1999

Comparison of the project's behavioural contract against
**ANSI/ISO/IEC 9075-5:1999, Database Language SQL — Part 5: Host Language
Bindings (SQL/Bindings)**, approved 9 December 1999, 265 pp.

Citation tag in specs: `[SQL/B §16.4]` — clause and subclause of Part 5.
The standard is fetched locally, not vendored (`manual/` is gitignored).

## The headline

**SQL/MP embedded C is pre-standard and cannot be made conformant without
breaking Principle II.** It predates SQL:1999 and, in the areas that matter
most — status reporting and dynamic SQL descriptors — implements models the
standard does not merely differ from but has *removed* or *replaced*.

This does not change the project's contract. Constitution I names the HP manual,
and the whole point of the project is that customer source recompiles unchanged.
The comparison is valuable for three narrower reasons:

1. It tells us precisely which SQL/MP constructs are proprietary extensions,
   which sharpens feature 008's policy decisions.
2. It offers a principled source of truth where the HP manual is silent — most
   usefully for `DIV-042`.
3. It defines what a future standards-conformant mode would have to add, if that
   is ever wanted.

**Recommendation: do not pursue conformance as a goal.** Record the gaps, use the
standard as a tiebreaker where the manual is silent, and revisit only if a
customer asks for a modernisation path off SQL/MP.

---

## 1. Status reporting — irreconcilable

| | Standard | SQL/MP |
|---|---|---|
| Mechanism | `SQLSTATE`, 5 chars: 2-char class + 3-char subclass | `sqlcode`, integer |
| `SQLCODE` | **Absent. Zero occurrences in the entire document.** | The entire model |
| Diagnostics detail | `GET DIAGNOSTICS` | `SQLCA` + system procedures |
| Statistics | not specified | `SQLSA` |

`SQLCODE` was deprecated in SQL-92 and **removed** by SQL:1999. It appears
nowhere in Part 5. Every SQL/MP program's control flow runs through `sqlcode`,
and `SQLCA`/`SQLSA` have no standard counterpart at all.

Condition categories do line up conceptually — the standard sorts SQLSTATE
classes into categories W (warning), N (not found), and X (exception)
`[SQL/B §16.2]`, which is the same three-way split `WHENEVER` uses. So the
*shape* of SQL/MP's diagnostics is standard-adjacent even though the
*representation* is not.

Class `01` is the warning class; Part 5 adds `01005` (insufficient item
descriptor areas) to it.

### Bearing on `DIV-042`

This is the useful find. The unresolved conversion-warning codes (character
right-truncation, the two fixed-point scale-loss cases) have a standard analogue:
they are class `01` warnings, and right-truncation is conventionally `01004`.

But **Part 5 does not contain the full SQLSTATE table** — its Table 10 lists only
what Part 5 *adds* to Part 2, marking the rest "all alternatives from ISO/IEC
9075-2". So this PDF confirms the *mechanism* and the warning class, not the
specific subclass values.

Practical effect on `DIV-042`: a third route opens, better than the "choose
arbitrary values" fallback —

3. Define the `sqlcode` warning values as a documented mapping from the standard
   SQLSTATE warning subclasses, and additionally expose `SQLSTATE` as an
   opt-in host variable. Standard-derived values are defensible and stable,
   which arbitrary ones are not.

This still needs 9075-2 (or `SQLRM`) for the subclass values. It does not close
`DIV-042`, but it improves the fallback and is worth recording.

---

## 2. Dynamic SQL descriptors — different model entirely

| | Standard | SQL/MP |
|---|---|---|
| Descriptor | Named area, server-side | `SQLDA` struct in program memory |
| Lifecycle | `ALLOCATE`/`DEALLOCATE DESCRIPTOR` | program `malloc`s it |
| Field access | `GET`/`SET DESCRIPTOR` statements | direct struct field access |
| Binding | `USING SQL DESCRIPTOR` | `var_ptr`/`ind_ptr` set by the program |
| `SQLDA` | **Zero occurrences** | central |

The standard replaced the program-allocated descriptor struct with statement-
mediated named descriptors. SQL/MP's `SQLDA` is the SQL-89-era model.

Both have `DESCRIBE INPUT`. That is roughly where the overlap ends.

Consequence for feature 007: none of its work is standards-aligned, and that is
correct given the contract. `DIV-040` (widening the address fields) is a
divergence from the *HP manual*, not from the standard, which has no opinion.

---

## 3. `WHENEVER` — near miss, with two real traps

Standard `[SQL/B §16.2]`:

- Conditions: `SQLEXCEPTION`, `SQLWARNING`, `NOT FOUND`, plus
  `SQLSTATE(class[,subclass])` and `CONSTRAINT <name>`
- Actions: `CONTINUE`, `GOTO target`, `GO TO target`
- Goto target may be a host label identifier **or an unsigned integer**

SQL/MP: conditions `SQLERROR`, `SQLWARNING`, `NOT FOUND`; actions
`CONTINUE`, `GOTO :label`, `GO TO :label`, `CALL :handler`.

| Difference | Detail |
|---|---|
| **`SQLERROR` vs `SQLEXCEPTION`** | Different keyword for the same category. SQL/MP's spelling is non-standard |
| **`CALL` action** | SQL/MP-only. The standard has no procedure-call action |
| Colon-prefixed targets | SQL/MP requires `:`; the standard does not use it on labels |
| Integer goto targets | Standard permits; SQL/MP does not |
| `SQLSTATE(...)` / `CONSTRAINT` conditions | Standard-only |

The scoping rule is materially the same in both: source-order, applying to
subsequent statements until superseded by another declaration for the same
condition or category. Feature 005's FR-005.6 is consistent with the standard
here.

---

## 4. C host variable types — SQL/MP is partly outside the grammar

Standard `<C numeric variable>` `[SQL/B §16.4]` permits exactly:
`long`, `short`, `float`, `double`. Note: no `int`, no `long long`, no unsigned
numerics.

Standard `<C character variable>`: `char`, `unsigned char`, `unsigned short`.

| SQL/MP type | Standard status |
|---|---|
| `short`, `float`, `double` | conformant |
| `long` | conformant (but see `DIV-001` on width) |
| `long long` (LARGEINT) | **outside the grammar** |
| `unsigned short`, `unsigned long` | **outside the grammar** |
| `decimal` array | **outside the grammar** |
| `fixed` | **outside the grammar** |
| `char[]` + `CHARACTER SET` | conformant, minus the optional `IS` keyword |

The standard writes `CHARACTER SET [IS] <charset>`; SQL/MP omits `IS`. SQL/MP
accepts a subset, which is harmless.

Standard types with **no SQL/MP counterpart**: `NCHAR`, `NCHAR VARYING`, `CLOB`,
`NCLOB`, `BLOB`, `BIT`, user-defined types, all four locator forms
(CLOB/BLOB/array/UDT), `REF`, and the whole `SQL TYPE IS …` syntax.

The standard also permits a `<C storage class>` (`auto`/`extern`/`static`) and a
`<C class modifier>` (`const`/`volatile`) on host variable definitions, and
initialisers. The HP manual documents none of these — worth a 002 question,
since customer code may well use `static` in a declare section and expect it to
work.

### VARCHAR — a material incompatibility

| | Standard | SQL/MP |
|---|---|---|
| Declared as | `VARCHAR hv[n];` — a keyword | `struct { short len; char val[n+1]; }` |
| Preprocessor action | textually replaces `VARCHAR` with `char`, adjusts length | generates the two-field struct |
| Result | a plain character array | a structure with a length field |

These are not variations on a theme; they are different data layouts reached by
different mechanisms. A program written against one will not compile — or worse,
will misbehave — against the other. Anything claiming both must pick, and this
project picks SQL/MP.

---

## 5. Embedding syntax — SQL/MP is a clean subset

| | Standard | SQL/MP |
|---|---|---|
| Prefix | `EXEC SQL` or `&SQL(` | `EXEC SQL` |
| Terminator | `END-EXEC`, `;`, or `)` — **optional** in the grammar | `;`, required |
| Declare section | `EXEC SQL BEGIN/END DECLARE SECTION` | same |
| Charset declaration | `SQL NAMES ARE <charset>` | absent |

SQL/MP restricting to `EXEC SQL` … `;` is a conformant subset. Good news for
feature 001: its scanner is stricter than the standard requires, not looser.

**Placement differs, though.** The standard says an embedded statement may appear
wherever a C statement may, *within a function block*, optionally after a label
prefix `[SQL/B §16.4 SR2]`. SQL/MP additionally allows declaration-position
constructs — `INVOKE`, `INCLUDE STRUCTURES`, `INCLUDE SQLCA`/`SQLSA`/`SQLDA`,
static `DECLARE CURSOR` — outside function blocks. Feature 001's position-class
model (FR-001.11..14) is therefore an SQL/MP requirement with no standard basis,
and the label-prefix allowance is something 001 does not currently handle.

> **Action for 001:** add an open question on whether a label prefix may precede
> an embedded statement. The standard permits it; the HP manual is silent; C
> programmers do write `retry: EXEC SQL …`.

---

## 6. Standard constructs absent from SQL/MP

`<embedded authorization declaration>` (`DECLARE SCHEMA` / `AUTHORIZATION`,
with `FOR STATIC ONLY | AND DYNAMIC`), `<embedded path specification>`,
`<embedded transform group specification>`, `<handler declaration>`,
`<SQL-invoked routine>`, `<temporary table declaration>`, `GET DIAGNOSTICS`.

Several are gated behind named conformance features (B051 enhanced execution
rights, F451/F461 character sets, F361 subprogram support, S071 SQL paths), so
even a standards-targeting implementation would not need all of them.

## 7. SQL/MP constructs absent from the standard

`INVOKE`, `INCLUDE STRUCTURES`, `INCLUDE SQLCA`/`SQLSA`/`SQLDA`, `SQL SOURCE`,
`CONTROL EXECUTOR`/`QUERY`/`TABLE`, `SETSCALE`, `TYPE AS`, `SQLDA` itself, all
SQL/MP system procedures, all 22 CPRL procedures, `#pragma SQL`, `#pragma SQLMEM`,
Guardian object naming, TACL DEFINEs, program invalidation and recompilation,
VSBB, and the version-management model.

Every item on this list is a proprietary extension. That is not a criticism —
much of it predates the relevant standard — but it does mean feature 008's scope
is precisely "the non-standard surface", which is a useful sanity check on that
spec's boundaries.

---

## Net effect on the project

| Item | Change |
|---|---|
| Constitution I | unchanged — the HP manual remains the contract |
| `DIV-042` | improved fallback: derive `sqlcode` warnings from standard SQLSTATE warning subclasses, and optionally expose `SQLSTATE`. Still needs 9075-2 or `SQLRM` for subclass values |
| 002 | new question: are `static`/`extern`/`const`/`volatile` and initialisers permitted on host variable definitions? Standard allows them; manual is silent |
| 001 | new question: may a label prefix precede an embedded statement? |
| 005 | `SQLERROR` vs `SQLEXCEPTION` confirmed as an SQL/MP spelling; `CALL` confirmed as an SQL/MP-only action. No change to requirements, but both are now cited rather than assumed |
| 007 | confirmed that no part of the `SQLDA` model is standards-aligned; `DIV-040` is a divergence from HP, not from ANSI |
| 008 | scope validated — its construct list is close to exactly the non-standard surface |

## What this PDF cannot settle

Part 5 covers host language bindings only. The full SQLSTATE value table, the
data type definitions, and general SQL semantics live in **Part 2
(SQL/Foundation)**, which is not in this document. Obtaining 9075-2 would close
the `DIV-042` subclass values. `SQLRM` remains the more direct route, since it
describes the actual SQL/MP codes rather than a standard this implementation is
not claiming to meet.
