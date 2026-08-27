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

**Status:** proposed · **Feature:** 008 · **Citation:** `[SQLPM/C §6 p.6-6, §7 p.7-2, App. C]`

**NonStop:** `=name` in an embedded statement is resolved at run time through a
TACL DEFINE to a Guardian file name, so the same object can be repointed without
recompiling.

**Here:** to be decided — a configuration-file mapping, an environment-variable
convention, or refusal. The manual's own examples use `=defines` pervasively
(`FROM =shipments`), so refusal has a high blast radius.

**Rationale:** pending.

**Detection:** pending. **Migration:** pending.

> Blocks feature 008 reaching `Ready`. Also blocks any conformance test
> transcribed from a manual example, because those examples use `=name` freely —
> feature 001's fixtures must either resolve this or avoid `=name`.

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

## DIV-011 — SQLSA statistics without a NonStop executor

**Status:** proposed · **Feature:** 005 · **Citation:** `[SQLPM/C §9 pp.9-17..9-18]`

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

**Detection:** the sentinel value, to be fixed by 005's spec.

**Migration:** programs that log SQLSA statistics keep working; programs that
branch on `disc_reads` or the VSBB flags need review.

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
