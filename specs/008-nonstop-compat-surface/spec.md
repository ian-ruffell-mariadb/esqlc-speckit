# Feature Spec: NonStop compatibility surface

**ID:** 008-nonstop-compat-surface · **Status:** Clarifying
**Manual coverage:** §5 (version procedures, `cextdecs`); §6 (toolchain); §7 (execution); §8 (invalidation); §11 (CPRL); App. B; App. C; App. D (`RELEASE1`/`RELEASE2`); plus the §3/§4 constructs listed below
**Depends on:** 001–007

## 1. Problem

Roughly a third of the manual describes the NonStop platform rather than the
embedded-SQL language: Guardian object names and TACL DEFINEs, a separate SQL
compilation pass producing program objects with stored plans, program
invalidation and automatic recompilation, process access IDs, VSBB, lock
escalation, optimiser `CONTROL` statements, the Pathway environment, collation
objects with 22 procedures over them, memory segment placement, and partition-
level availability controls.

None of it can be implemented as specified. All of it appears in customer source.
The purpose of this feature is to ensure that every one of those constructs meets
a **decision** rather than a default — because the failure mode here is not a
missing feature, it is a program that compiles, runs, and quietly does something
other than what it did on NonStop.

This is the feature that determines whether the project is trustworthy.

## 2. Scope

**In scope** — one stated policy (`error` / `warn` / `ignore` / `emulate`) per
construct, plus a test proving that policy fires:

- Guardian object naming (`\node.$vol.subvol.file`) and TACL DEFINEs (`=name`).
- DDL statements: the whole §3 inventory, including the catalog, collation, and
  program object classes.
- DCL statements: `CONTROL EXECUTOR`, `CONTROL QUERY`, `CONTROL TABLE`,
  `FREE RESOURCES`, `LOCK TABLE`, `UNLOCK TABLE`.
- DSL statements: `GET CATALOG OF SYSTEM`, `GET VERSION`,
  `GET VERSION OF PROGRAM`.
- `CONTROL` directives, and the static/dynamic distinction §6 draws.
- PAID / process access requirements as cited across §4 and §7.
- SQL error 8204 (lost open) and its recovery procedure.
- VSBB, cursor stability access modes, foreign cursors.
- `SQLGETCATALOGVERSION`, `SQLGETOBJECTVERSION`, `SQLGETSYSTEMVERSION`.
- `cextdecs` shimming, and the Guardian procedures §5 lists as returning SQL
  information.
- Program invalidation, automatic recompilation, and their run-time errors.
- The compilation toolchain: TNS / TNS/R NMC / TNS/E CCOMP, `BIND`, the SQL
  compiler pass, SQL program file format, OSS `c89`, `-Wsqlconnect`,
  `HP_NSK_CONNECT_MODE`, and PC cross-compilation.
- Execution environment: access authority, TACL `RUN`, low PIN, interactive vs
  programmatic commands, Pathway, SQL executor compatibility.
- CPRL: all 22 procedures and their return codes.
- `SQLMEM` and App. B memory considerations.
- App. C local autonomy, including skipping unavailable partitions.
- Version management: `RELEASE1` / `RELEASE2`, and the synthetic version story.
- `INVOKE` with SQLCI.
- Dynamic SQL Pathway servers.

**Out of scope**

- Anything owned by 001–007.

## 3. Requirements

| ID | Requirement | Citation |
|----|-------------|----------|
| FR-008.1 | Every construct listed in scope has exactly one recorded policy, discoverable from a single table in this spec. | Principle III |
| FR-008.2 | No construct in scope reaches the runtime without a policy; a construct with no policy is `ESQLC-8001`, not a no-op. | Principle III |
| FR-008.3 | `ignore` is not used for any construct that can change a result set, a lock, or a transaction boundary. | Constitution III |
| FR-008.4 | Guardian fully-qualified object names are recognised and mapped by a documented rule, or rejected. | `[SQLPM/C §3 p.3-1]` |
| FR-008.5 | TACL DEFINE references (`=name`) are resolved through a documented mechanism, or rejected with a diagnostic naming the define. | `[SQLPM/C §6 p.6-6]`, `[SQLPM/C §7 p.7-2]`, `[SQLPM/C App. C]` `[DIV-002]` |
| FR-008.6 | `CONTROL EXECUTOR`, `CONTROL QUERY`, and `CONTROL TABLE` each have a stated policy; where accepted, the accepted subset is enumerated and the remainder diagnosed. | `[SQLPM/C §3 p.3-5]` |
| FR-008.7 | `LOCK TABLE`, `UNLOCK TABLE`, and `FREE RESOURCES` have stated policies, with `FREE RESOURCES` at minimum closing cursors and releasing locks. | `[SQLPM/C §3 p.3-5]` |
| FR-008.8 | DDL statements over object classes with no MariaDB counterpart — catalogs, collations, programs — are diagnosed, not silently accepted. | `[SQLPM/C §3 p.3-4]` |
| FR-008.9 | `GET VERSION` and `GET VERSION OF PROGRAM` return documented synthetic values, and what they mean is stated. | `[SQLPM/C §3 p.3-6]` `[DIV-020]` |
| FR-008.10 | `GET CATALOG OF SYSTEM` has a stated policy. | `[SQLPM/C §3 p.3-6]` |
| FR-008.11 | `SQLGETSYSTEMVERSION` is implemented, because `INCLUDE STRUCTURES SQLSA VERSION CURRENT` generates a call to it. | `[SQLPM/C §9 p.9-2]`, `[SQLPM/C §5 p.5-19]` |
| FR-008.12 | `SQLGETCATALOGVERSION` and `SQLGETOBJECTVERSION` return documented synthetic values. | `[SQLPM/C §5 pp.5-18..5-19]` |
| FR-008.13 | A `cextdecs` shim satisfies `#include <cextdecs(...)>` for the procedures this project implements and diagnoses requests for those it does not. | `[SQLPM/C §5 p.5-2]`, `[SQLPM/C §11 p.11-2]` |
| FR-008.14 | PAID / process access requirements are addressed by a stated mapping onto MariaDB's per-connection authorisation, and the difference is registered. | `[SQLPM/C §7 p.7-1]` and the per-statement citations in `[SQLPM/C §4]` |
| FR-008.15 | SQL error 8204 cannot occur; the recovery path is documented as dead code and `GET`-style probes for it behave predictably. | `[SQLPM/C §4 pp.4-2..4-3]` `[DIV-020]` |
| FR-008.16 | VSBB constructs are accepted and ignored, with the `SQLSA` VSBB flags returning sentinels per 005. | `[SQLPM/C §4 pp.4-17..4-18]` `[DIV-011]` |
| FR-008.17 | Cursor stability access modes each map to a documented MariaDB isolation level, or are rejected. | `[SQLPM/C §4 p.4-17]` |
| FR-008.18 | Foreign cursors have a stated policy. | `[SQLPM/C §4 p.4-24]` |
| FR-008.19 | Program invalidation and automatic recompilation are documented as non-occurring, and no `sqlcode` in their range is ever returned. | `[SQLPM/C §8]` `[DIV-020]` |
| FR-008.20 | The toolchain differences §6 documents are addressed by a single documented build flow, and constructs specific to a NonStop compiler family are diagnosed rather than ignored. | `[SQLPM/C §6 pp.6-1..6-37]` |
| FR-008.21 | `SQLMEM` is parsed, validated, and ignored with an informational diagnostic on first occurrence. | `[SQLPM/C §3 p.3-7]`, `[SQLPM/C App. B]` `[DIV-030]` |
| FR-008.22 | `RELEASE1` and `RELEASE2` pragma options are honoured to the extent they select structure versions, and their software-version-targeting aspect is documented as synthetic. | `[SQLPM/C §3 p.3-7]`, `[SQLPM/C App. D p.D-8]` |
| FR-008.23 | Each of the 22 CPRL procedures has a stated policy; where implemented over MariaDB collations, the semantic difference is registered; where not, the diagnostic names the procedure. | `[SQLPM/C §11 pp.11-1..11-23]` |
| FR-008.24 | CPRL return codes follow the published scheme for procedures that are implemented. | `[SQLPM/C §11 p.11-2]` |
| FR-008.25 | App. C local autonomy constructs are rejected rather than ignored, because skipping unavailable partitions changes result sets. | `[SQLPM/C App. C pp.C-1..C-3]`, Constitution III |
| FR-008.26 | Pathway constructs, including dynamic SQL Pathway servers, have a stated policy. | `[SQLPM/C §7 p.7-6]`, `[SQLPM/C §10 p.10-36]` |
| FR-008.27 | `INVOKE` with SQLCI has a stated policy. | `[SQLPM/C §2 p.2-24]` |
| FR-008.28 | Low-PIN execution, interactive vs programmatic commands, and SQL executor compatibility checks have stated policies. | `[SQLPM/C §7 pp.7-4..7-7]` |
| NFR-008.1 | The policy table is machine-readable and the diagnostic layer is generated from it, so a construct cannot be added to the parser without a policy. | FR-008.2 |
| NFR-008.2 | Every policy has a test that fires it. Policies with no test are treated as absent. | Principle IV |

## 4. Acceptance scenarios

### AS-008.1 — No construct without a policy
- **Given** the full construct list from §3, §4 (008-owned rows), §5, §6, §7, §8,
  §11, App. B, and App. C
- **When** each is preprocessed
- **Then** each produces its stated policy's outcome, and none is silently
  accepted as a no-op
- **Test:** `tests/conformance/008/policy_matrix/*.sqlc`, one file per construct

### AS-008.2 — Policy table drives the diagnostics
- **Given** a construct added to the parser with no policy-table entry
- **When** the build runs
- **Then** the build fails
- **Test:** `tests/conformance/008/policy_completeness.sh`

### AS-008.3 — Local autonomy rejected, not ignored
- **Given** a statement requesting that unavailable partitions be skipped
- **When** preprocessed
- **Then** an error, because accepting it would change result sets
- **Test:** `tests/conformance/008/negative/skip_partitions.sqlc`

### AS-008.4 — SQLGETSYSTEMVERSION works
- **Given** `INCLUDE STRUCTURES SQLSA VERSION CURRENT`
- **When** compiled and run
- **Then** the generated `SQLGETSYSTEMVERSION` call returns the documented
  synthetic version and the correct `SQLSA` layout is selected
- **Test:** `tests/conformance/008/sqlsa_version_current.sqlc`

### AS-008.5 — CPRL diagnostics name the procedure
- **Given** a call to each unimplemented CPRL procedure
- **When** compiled
- **Then** each diagnostic names the specific procedure, not the CPRL family
- **Test:** `tests/conformance/008/negative/cprl_unimplemented.sqlc`

### AS-008.6 — Ignored constructs are visible
- **Given** `SQLMEM` and a VSBB construct
- **When** preprocessed
- **Then** both are accepted and both emit an informational diagnostic
- **Test:** `tests/conformance/008/ignored_visible.sqlc`

## 5. Diagnostics

| Code | Condition | Default policy | Citation |
|------|-----------|----------------|----------|
| `ESQLC-8001` | Construct with no policy-table entry | error (build-time failure) | Principle III |
| `ESQLC-8002` | Unresolved TACL DEFINE reference | error | `[§6 p.6-6]` |
| `ESQLC-8003` | Guardian object name not mappable | error | `[§3 p.3-1]` |
| `ESQLC-8004` | `CONTROL` option outside the accepted subset | error | `[§3 p.3-5]` |
| `ESQLC-8005` | DDL over an unsupported object class | error | `[§3 p.3-4]` |
| `ESQLC-8006` | Unimplemented system procedure requested through `cextdecs` | error | `[§5 p.5-2]` |
| `ESQLC-8007` | Unimplemented CPRL procedure, named | error | `[§11 p.11-1]` |
| `ESQLC-8008` | Local autonomy construct | error | `[App. C]` |
| `ESQLC-8009` | Compiler-family-specific construct | error | `[§6 p.6-2]` |
| `ESQLC-8010` | Pathway construct | policy TBD | `[§7 p.7-6]` |
| `ESQLC-8011` | `SQLMEM` accepted and ignored | info | `[§3 p.3-7]` `[DIV-030]` |
| `ESQLC-8012` | VSBB construct accepted and ignored | info | `[§4 p.4-17]` `[DIV-011]` |
| `ESQLC-8013` | Program-object or version construct answered synthetically | info | `[§8]` `[DIV-020]` |

## 6. Open questions

Unusually many, by design — this feature is where the project's honest limits get
decided, and every one of these is a judgement call rather than a lookup.

| # | Question | Blocks | Resolution |
|---|----------|--------|------------|
| Q1 | TACL DEFINE mechanism: configuration file, environment convention, or rejection? | FR-008.5 | unresolved — `DIV-002`. High blast radius: the manual's own examples use `=name` pervasively |
| Q2 | Guardian name mapping rule: what does `\node.$vol.subvol.file` become? | FR-008.4 | unresolved |
| Q3 | Which `CONTROL TABLE` / `CONTROL QUERY` options, if any, map onto MariaDB hints usefully enough to accept? | FR-008.6 | unresolved |
| Q4 | PAID: is per-connection MariaDB user sufficient, or do programs depend on process identity in ways that require more? | FR-008.14 | unresolved |
| Q5 | Cursor stability access modes: what are they, and which isolation level for each? Shared with 004 Q3. | FR-008.17 | unresolved |
| Q6 | CPRL: implement the shift and compare procedures over MariaDB collations, or refuse the whole family? A partial implementation may mislead more than a refusal. | FR-008.23 | unresolved |
| Q7 | Synthetic version numbers: what does this implementation report for system, catalog, object, PCV, PFV, and HOSV? | FR-008.9, .12, .22 | unresolved |
| Q8 | Pathway: is any support in scope, or is it declared a non-target? | FR-008.26 | unresolved |
| Q9 | Do any `sqlcode` ranges need reserving so that recompilation-era codes provably never occur? | FR-008.19 | unresolved |
| Q10 | `INVOKE` with SQLCI — is this a build-time integration worth supporting at all? | FR-008.27 | unresolved |

## 7. Constitution check

| Principle | Compliant? | Note |
|-----------|-----------|------|
| I manual is the contract | yes | Every requirement cited. The manual specifies behaviour this project cannot provide; naming that honestly is compliance, not violation |
| II source compatibility | partial by design | Some constructs will be rejected. Principle II permits this where the divergence is registered, and rejection is the alternative to wrong behaviour |
| III no silent semantic change | yes | FR-008.2 and NFR-008.1 make silence structurally impossible; FR-008.3 constrains `ignore` |
| IV manual-derived tests first | yes | NFR-008.2 treats an untested policy as absent |
| V layered / frozen ABI | yes | Shims sit behind the ABI; `cextdecs` is a header shim, not generated-code coupling |
| VI byte-exact structures | n/a | Structures owned by 005 and 007 |
| VII divergence registered | partial | `DIV-002`, `DIV-011`, `DIV-020`, `DIV-030` all live here. Q1–Q10 will each produce or close one |
