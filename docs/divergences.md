# Divergence register

Every intentional difference from the manual's specified behaviour, per
Constitution VII. An entry must exist **before** the code that causes it merges.

Required fields: ID, manual citation, NonStop behaviour, this implementation's
behaviour, rationale, runtime detection, migration advice.

Status values: `proposed`, `accepted`, `implemented`, `withdrawn`.

---

## DIV-001 — Host variable integer widths

**Status:** accepted · **Feature:** 002 · **Citation:** `[SQLPM/C §2 p.2-4]`

**NonStop:** `NUMERIC(5..9)` and `INTEGER` map to C `long`; `NUMERIC(10..18)` and
`LARGEINT` map to `long long`. On the NonStop C compilers `long` is 32-bit.

**Here:** the mapping is by **width**, not by type name. Generated declarations
use width-exact types (`int32_t`, `uint32_t`, `int64_t`, …) so that an `INTEGER`
host variable is 32 bits on LP64 Linux, where `long` is 64 bits.

**Rationale:** preserving the type *name* would silently double the width of
every `INTEGER` host variable and break any program that relies on wraparound,
`sizeof`, struct layout, or `%ld` format strings.

**Detection:** `sizeof(hostvar)` is unchanged from NonStop; `typeof` differs.
Programs that spell out `long` in their own declarations rather than using
`INVOKE` are unaffected in width but will see a type mismatch if they pass a host
variable to a function declared with `long`.

**Migration:** compile with `-Wconversion`. `INVOKE`-generated declarations need
no change. Hand-written declare sections using `long` for `INTEGER` columns
continue to work on LP64 but waste 4 bytes and must not be assumed 32-bit.

---

## DIV-002 — TACL DEFINE name resolution

**Status:** accepted · **Feature:** 003, 006, 008 · **Citation:** `[SQLPM/C §6 p.6-6, §7 p.7-2, §2 p.2-19, App. C]`

**NonStop:** `=name` in an embedded statement is resolved at run time through a
TACL DEFINE to a Guardian file name, so the same object can be repointed without
recompiling. Two classes matter: class `CATALOG` for a catalog, class `MAP` for
an object (table, view, index, partition). Propagation into a new process is
governed by `DEFMODE` — `ON` propagates the parent's current DEFINE set from its
process file segment, `OFF` propagates only `=_DEFAULTS`. `INVOKE` additionally
accepts a class MAP DEFINE for the invoked object name, but **not** for the
structure tag `[§2 p.2-19]`.

**Here:** resolved by a configuration-file mapping layer with two sections
mirroring the two DEFINE classes — one for catalogs, one for objects — under the
precedence chain of FR-003.19 (environment, then file, then compile-time
defaults). An unmapped name is a diagnostic (`ESQLC-3007`), never a silent
pass-through to MariaDB.

**Rationale:** the value of a DEFINE is late binding — repointing an object
without recompiling — and a configuration file preserves exactly that, while
requiring no change to customer source. Refusal was the alternative and has an
unacceptable blast radius: the manual's own examples use `=name` pervasively
(`FROM =shipments`).

**Detection:** names resolve from configuration rather than from the process's
inherited DEFINE set. A program relying on a DEFINE created dynamically at run
time by a Guardian procedure has no equivalent and gets `ESQLC-3007`.

**Migration:** translate the deployment's DEFINE set into the configuration
file's mapping sections. Static DEFINE sets translate mechanically;
programmatically created DEFINEs need redesign.

> Open sub-question (003 Q6): whether `DEFMODE`'s inherit-all vs
> inherit-only-defaults distinction needs an analogue. Programs may depend on
> being able to disable mapping wholesale.

---

## DIV-010 — TMF transactions on InnoDB

**Status:** proposed · **Feature:** 003 · **Citation:** `[SQLPM/C §3 p.3-6]` `[EXTERNAL — SQLRM]`

**NonStop:** `BEGIN`/`COMMIT`/`ROLLBACK WORK` bracket a TMF transaction — a
distributed, process-scoped transaction that can span nodes and non-SQL
resources, and whose isolation and lock escalation behaviour is TMF's.

**Here:** mapped to an InnoDB transaction on the single connection.

**Rationale:** it is the only available primitive. The semantics overlap for the
single-node, single-resource case that most application code assumes.

**Detection:** differences surface as isolation-level behaviour, lock wait
timeouts instead of TMF lock waits, and the absence of cross-resource atomicity.

**Migration:** programs relying on multi-node or multi-resource TMF atomicity
require redesign, not a recompile. Feature 003's spec must enumerate which
observable behaviours change.

---

## DIV-012 — PROGID privilege elevation is refused

**Status:** accepted · **Feature:** 003 · **Citation:** `[SQLPM/C §7 pp.7-1..7-2]`

**NonStop:** authorisation for a database object is decided by the process access
ID (PAID) plus the group list of the creator access ID (CAID). PAID depends on
the program file's `PROGID` attribute: with it off, PAID equals the invoking
user's ID; with it on, PAID equals the **program owner's** ID, letting one user
temporarily gain a controlled subset of another's privileges.

**Here:** PAID maps to the MariaDB user on the process's single connection.
`PROGID` elevation has no analogue and is not emulated. A program that depends on
it is diagnosed (`ESQLC-3008`).

**Rationale:** the available emulation would be to let configuration name a
second, more-privileged MariaDB user that the runtime silently switches to. That
moves a real privilege boundary into a config file and is a
privilege-escalation bug waiting to be written. Refusing is honest; approximating
a security boundary is not a compatibility decision.

**Detection:** the diagnostic. There is no silent-failure path — the program does
not run rather than running with the wrong identity.

**Migration:** grant the required privileges to the invoking user directly, or run
the program as a MariaDB user that already holds them. The elevation itself has to
be re-expressed as MariaDB grants; there is no mechanical translation.

---

## DIV-011 — SQLSA statistics without a NonStop executor

**Status:** accepted · **Feature:** 005 · **Citation:** `[SQLPM/C §9 pp.9-17..9-18]`

**NonStop:** `stats[]` reports `disc_reads`, `messages`, `message_bytes`,
`waits`, `escalations`, `vsbb_write`, `vsbb_flushed`, and (v330+)
`master_executor_elapsed_time`, `total_esp_cpu_time`,
`total_sortprog_cpu_time` — all measurements of NonStop process and disk-process
structure.

**Here:** fields with an honest MariaDB analogue are populated
(`records_accessed`, `records_used`, `num_tables`, `waits`). Fields with none
return a documented sentinel, not zero.

**Rationale:** zero is a valid measurement and would be indistinguishable from
"not measured", which Constitution III forbids. A sentinel is detectable.

**Detection:** the sentinel value, fixed by Gate 5. A numeric field with no
analogue carries `-1` in its own declared width (SD-7); a character field
carries `?` to its full width (SD-8). Zero is deliberately not used: zero is a
legitimate statistic, so a program cannot tell it from "not measured", which is
what Constitution III forbids.

Implemented as a stamp at the start of every statement, with population then
filling only what the statement can honestly supply. That single mechanism also
gives FR-005.19 — a statement class that leaves the area undefined simply does
not populate, so it reads as sentinels rather than as the previous statement's
plausible numbers.

**As built (Gate 5):** `num_tables`, `table_name` and `records_used` carry real
values on the cursor path. `records_accessed`, `disc_reads`, `messages`,
`message_bytes`, `waits` and `escalations` are all sentinel — more of `stats[]`
than the original entry anticipated. `table_name` is also sentinel on the DML
path, because `INSERT`/`UPDATE`/`DELETE` return no result-set metadata to read
it from and parsing the statement would violate NFR-001.1.

**Migration:** programs that log SQLSA statistics keep working; programs that
branch on `disc_reads` or the VSBB flags need review.

**Layout note (resolved 2026-08-27):** both SQLSA layouts are now fully known
from §9 pp.9-15..9-16 and validate exactly against the published 838 and 1790
sizes under packed alignment. Two facts this surfaced, neither visible in the
Table 9-5 field list: the VSBB flags exist **only** in v330+ (pre-330 that slot
is `sqlsa_reserved[4]`), and the five `stats[]` counters widen from 32-bit to
64-bit across the version boundary while `waits`/`escalations` widen from 16-bit
to 32-bit. Sentinel values must therefore be chosen per width, not once.

---

## DIV-020 — No SQL program objects, invalidation, or recompilation

**Status:** proposed · **Feature:** 008 · **Citation:** `[SQLPM/C §6, §8, §7 p.7-7]`

**NonStop:** the SQL compiler stores a compiled plan and version stamps in the
program file. Plans are invalidated by DDL change or timestamp mismatch, and
automatically recompiled at run time with a distinct error class. `GET VERSION OF
PROGRAM` reports PCV, PFV, and HOSV.

**Here:** statements are prepared against MariaDB at run time. There is no stored
plan, so invalidation and automatic recompilation do not exist, and there are no
program version stamps.

**Rationale:** architectural. Reproducing a stored-plan model on MariaDB would be
a project of its own with no benefit to the customer.

**Detection:** recompilation-related `sqlcode` values never occur.
`GET VERSION OF PROGRAM` returns a synthetic answer to be specified by 008.

**Migration:** error handlers that branch on recompilation codes become dead
code — harmless, but the operational procedures around program invalidation
(§8's prevention advice) become moot and should be removed from runbooks.

---

## DIV-030 — TNS memory placement pragmas accepted and ignored

**Status:** proposed · **Feature:** 008 · **Citation:** `[SQLPM/C §3 p.3-7, App. B]`

**NonStop:** `SQLMEM` places SQL internal structures in the user or extended data
segment. TNS-only; the TNS/R native compiler already ignores it.

**Here:** parsed, validated, and ignored, with an informational diagnostic on
first occurrence.

**Rationale:** there is no user/extended segment distinction. The NonStop
native-mode compiler sets the precedent of ignoring it, so this is consistent
with a supported NonStop configuration rather than novel behaviour.

**Detection:** the informational diagnostic. No runtime effect.

**Migration:** none. App. B's memory estimates and stack-overflow advice do not
apply.

---

## DIV-040 — SQLDA address fields widened to 64 bits

**Status:** proposed · **Feature:** 007 · **Citation:** `[SQLPM/C §10 pp.10-5, 10-7]`

**NonStop:** a `sqlvar` entry is 24 bytes: four `short` descriptor scalars
(`data_type`, `data_len`, `precision`, `null_info`) followed by four `long`
fields (`var_ptr`, `ind_ptr`, `cprl_ptr`, `reserved`). The `long` fields hold
32-bit NonStop extended addresses. `SQLDA_SQLVAR_LEN` is 24 and
`SQLDA_HEADER_LEN` is 4.

**Here:** the four address/reserved fields become 64-bit, making a `sqlvar` entry
40 bytes, and `SQLDA_SQLVAR_LEN` is redefined to 40. `SQLDA_HEADER_LEN` stays 4.

**Rationale:** a 32-bit field cannot hold a host pointer on LP64, and §10's
entire documented workflow has the *program* allocate buffers and assign their
addresses to `var_ptr` and `ind_ptr`. The alternatives are worse:

- Keeping 24 bytes and storing 32-bit offsets into a runtime-owned arena would
  require rewriting the allocation logic of every dynamic SQL program — a far
  larger breach of Principle II than a changed constant.
- Building ILP32-only would preserve the layout exactly but constrain deployment.
  Offered instead as an optional build mode (see below).

Decisively, the manual instructs programs to use the symbolic identifiers rather
than the literal values, *because the values can change in a new RVU*
(§10 p.10-5, and identically for SQLSA at §9 p.9-14). A program that obeyed that
instruction recompiles correctly against a redefined `SQLDA_SQLVAR_LEN`. This
divergence is therefore within the manual's own stated contract.

**Detection:** `SQLDA_SQLVAR_LEN` is 40, not 24. `sizeof` a `sqlvar` entry is 40.
A program that hard-coded 24 — contrary to the manual's instruction —
under-allocates its descriptor and will fail `ESQLC-7002` rather than corrupt
memory, because `num_entries` is validated against `PREPARE`'s reported count.

**Migration:** recompile. Programs using the symbolic identifiers need no source
change. Programs with a literal 24 must switch to the identifier; a lint check
for the literal should ship with the toolchain. For programs that cannot be
recompiled at all, an ILP32 build mode preserving the published 24-byte layout is
a supported fallback.

> **Open for sign-off.** This changes a published constant, so it stays
> `proposed` until the project owner confirms. The ILP32 fallback exists
> specifically so that confirmation is reversible.

---

## DIV-041 — SQLCA layout is implementation-private

**Status:** accepted · **Feature:** 005 · **Citation:** `[SQLPM/C §9 p.9-12]`, `[SQLPM/C §5 pp.5-11..5-12]`

**NonStop:** the SQLCA is 430 bytes with eye-catcher `CA`. The manual publishes
no field list and no offsets — only the total size, the `EXTERNAL` declaration
form, and a 29-entry content inventory expressed as `SQLCAGETINFOLIST` item
codes.

**Here:** this project defines its own 430-byte layout covering all 29 documented
items. Direct field access is **unsupported**; `SQLCADISPLAY`,
`SQLCATOBUFFER`, `SQLCAGETINFOLIST`, and `SQLCAFSCODE` are the supported access
paths, and the item codes and their error codes are honoured exactly.

**Rationale:** no conforming program can depend on fields the manual never
documents, so choosing a layout breaks nothing that was specified. Holding the
size at 430 keeps `SQLCA_LEN`-based allocation and `EXTERNAL` sharing working,
which is what programs actually rely on.

**Detection:** the accessor procedures behave as documented. A program that
reverse-engineered offsets from real NonStop output and indexes the structure
directly reads the wrong bytes.

**Migration:** replace direct field access with `SQLCAGETINFOLIST` calls. Note
that item 22 reports errors positive and warnings negative — the inverse of
`sqlcode` — so bridging code must flip the sign.

**Strengthened by Gate 4 (2026-09-01).** The runtime writes into the program's
own registered `SQLCA` rather than holding its own state, so the two things §9
p.9-3 describes both work: a copy taken with `SQLCA_LEN` carries real data, and
an `EXTERNAL`-shared area is the one being populated. An
accessor-reads-runtime-state design would have made every saved copy an empty
430-byte husk while still passing a naive copy test — mutation T463 proved the
distinction, but only after a stale-entry bug was fixed that had been masking it.

---

## DIV-042 — Conversion warning codes are chosen, not inherited

**Status:** proposed · **Feature:** 002, 005 · **Citation:** `[SQLPM/C §2 pp.2-5, 2-11]`

**NonStop:** §2 specifies four conditions that must return a warning through
`sqlcode` — character right-truncation, fixed-point to floating-point precision
loss, fixed-point to integer fraction loss, and (as an error) input value too
large for a column, which *is* numbered, 8300. It never states the `sqlcode`
values for the three warnings.

**Here:** pending resolution. Three routes, in order of preference:

1. Source the real values from the SQL/MP Reference Manual or the SQL message
   file, and withdraw this divergence.
2. Derive the values from the standard SQLSTATE warning subclasses and publish the
   mapping, additionally exposing `SQLSTATE` as an opt-in host variable.
   ISO/IEC 9075-5:1999 confirms class `01` is the warning class and sorts it into
   category W, the same category `WHENEVER SQLWARNING` tests — but its Table 10
   lists only the subclasses Part 5 adds, so the specific values need Part 2
   (SQL/Foundation). See [ansi-conformance.md](reference/ansi-conformance.md).
3. Failing both, assign values from a documented reserved block and publish them
   as this implementation's contract.

Route 2 is preferred over route 3 because standard-derived values are defensible
and stable where arbitrary ones are neither.

**Rationale:** warning codes are what customer error handlers branch on. Route 1
preserves existing handlers. Route 2 is acceptable only because the alternative —
returning success and silently truncating — is prohibited by Constitution III.
Choosing loudly beats converting silently.

**Detection:** documented once chosen. Programs branching on specific warning
values will not match until updated.

**Migration:** none under route 1. Under route 2, error handlers testing specific
warning values need remapping; handlers testing only `sqlcode > 0 && != 100`
(the `WHENEVER SQLWARNING` shape) are unaffected, which is the common case.

> **Blocked on an external document.** This is the one conflict of the three that
> the C manual cannot settle. Obtaining `SQLRM` closes it.

---

## DIV-052 — MariaDB strips trailing blanks from CHAR on retrieval

**Status:** accepted · **Feature:** 002, 004 · **Citation:** `[SQLPM/C §2 p.2-8]`
**Found:** Gate 1 implementation · **Resolved:** Gate 2, option 1, 2026-08-28

**NonStop:** fixed-length character columns are *always* blank-padded in the
database, and the manual makes program behaviour depend on it — an under-padded
array stores its null byte and "comparison operations fail".

**Here:** storage is faithful — Gate 1 proved exactly `width` bytes reach the
column verbatim, embedded null bytes included. **Retrieval is not.** MariaDB
strips trailing spaces from a `CHAR(n)` on `SELECT` unless the session sets
`PAD_CHAR_TO_FULL_LENGTH`. The same row reads back as 12 bytes or 18 depending
only on `sql_mode`.

**Decision (2026-08-28): option 1.** The runtime sets `PAD_CHAR_TO_FULL_LENGTH`
on its own session at connect. It makes every retrieval path faithful at once
with no per-fetch cost, requires no customer source change, and the mode is
narrow — it affects only fixed-length character retrieval padding. Gate 1's
probes confirmed empirically that it yields the faithful 18-byte result.

Option 2 stays the documented fallback if the session mode proves insufficient,
for instance where a deployment's option file overrides `sql_mode` after connect.
Gate 2 criterion 3 is the check that would catch that.

**Implemented** in `src/rt/context.c` at connect, appending to `@@sql_mode` via
`CONCAT` rather than assigning, so other modes survive. Its detector is Gate 2's
`select_into` assertion, which compares the full 18-byte value including its six
trailing blanks — if the mode were ever lost, that comparison fails rather than
silently returning a short string.

**Options considered:**

1. The runtime sets `PAD_CHAR_TO_FULL_LENGTH` on its own session at connect,
   making retrieval faithful without touching customer source. Cheapest, but it
   changes a session-wide mode a program might otherwise rely on.
2. The runtime pads to the column width itself on output binding, leaving
   `sql_mode` alone. More surgical, needs column metadata on every fetch.
3. Accept the divergence and document it. Rejected as-is: §2 p.2-8 ties
   comparison behaviour to padding, so a silently short value is exactly the
   class of failure Constitution III forbids.

**Detection:** `length()` of a retrieved fixed-length column is shorter than the
declared column width whenever the value has trailing blanks.

**Migration:** none for insert-side code. Retrieval-side comparisons are the
risk, and cannot be assessed until Gate 2 chooses among the options above.

> Gate 1 does **not** hit this — the slice has no `SELECT`. Registered now
> because it is visible now, and would otherwise be discovered as a bug during
> feature 004.

---

## DIV-051 — Post-DELETE cursor position is pinned to one of two permitted outcomes

**Status:** accepted · **Feature:** 004 · **Citation:** `[SQLPM/C §4 p.4-16]`

**NonStop:** the cursor position table says a positioned `DELETE` leaves the
cursor "between rows", then elaborates that it is positioned *either* between
rows *or* before the next row and after the preceding row. The two phrasings are
not obviously distinct, and the manual commits to neither.

**Here:** one interpretation is chosen, documented, and applied consistently:
the cursor sits immediately before the row that followed the deleted one, so the
next `FETCH` returns that row.

**Rationale:** an implementation cannot be ambiguous. Choosing the reading under
which the next `FETCH` returns the following row preserves the natural
delete-while-scanning loop, which is the dominant use of positioned `DELETE`.

**Detection:** a program that deletes the current row and then expects the next
`FETCH` to re-return the *same* ordinal position would differ — but no such
program can have been portable, since the manual permits both readings.

**Migration:** none expected. Programs relying on the other reading were already
relying on unspecified behaviour.

---

## DIV-050 — INVOKE indicator-name collision is diagnosed, not reproduced

**Status:** accepted · **Feature:** 006 · **Citation:** `[SQLPM/C §2 p.2-22]`

**NonStop:** for a column name of 30 or 31 characters under the default `_I`
indicator suffix, the suffix is truncated away, so the generated indicator
variable ends up with **the same name as its host variable**. The manual
documents this and advises using `PREFIX` or `NULL STRUCTURE` to avoid it.

**Here:** the collision is a hard error (`ESQLC-6007`) naming the column and
recommending `PREFIX` or `NULL STRUCTURE`.

**Rationale:** the NonStop behaviour is a defect the manual documents rather than
a feature. Two distinct declarations sharing a name cannot both be emitted, so
faithfully reproducing it is not even possible in generated C — the outcomes are
a compile error with a confusing message, or one declaration silently winning.
An explicit diagnostic naming the fix is strictly better than both.

**Detection:** preprocessing fails where NonStop's C compiler would have failed
later and less clearly. No runtime difference — affected programs could not have
worked correctly on NonStop either.

**Migration:** add a `PREFIX` or `NULL STRUCTURE` clause to the `INVOKE`, which
is what the manual advises regardless of platform.

---

## Template

```
## DIV-nnn — short title

**Status:** proposed · **Feature:** NNN · **Citation:** `[SQLPM/C §n p.n-n]`

**NonStop:** what the manual specifies.

**Here:** what this implementation does.

**Rationale:** why.

**Detection:** how a running program can tell.

**Migration:** what an affected program's owner should do.
```
