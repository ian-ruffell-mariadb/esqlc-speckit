# Feature Spec: Runtime library & MariaDB binding

**ID:** 003-runtime-mariadb-binding · **Status:** Clarifying
**Manual coverage:** §3 p.3-6 (transaction control); §4 pp.4-2..4-3 (opens); §7 p.7-3 (OSS execution)
**Depends on:** 001, 002

## 1. Problem

Everything the preprocessor emits has to land somewhere. That somewhere is a
runtime library exposing a stable `esqlc_*` ABI, translating opaque statement
bodies and typed host-variable descriptors into MariaDB client calls, and
carrying the connection, transaction, and statement state that embedded SQL
assumes is implicit.

Embedded SQL/MP has no connect statement. A program simply runs statements
against a database determined by its environment — Guardian names, TACL DEFINEs,
and system catalogs. There is no equivalent implicit context on MariaDB, so this
feature has to invent one without introducing new required syntax into customer
source, which Principle II forbids.

Transaction semantics are the second problem. `BEGIN`/`COMMIT`/`ROLLBACK WORK`
bracket a TMF transaction, which is not an InnoDB transaction. Most single-node
application code will not notice; code relying on cross-node or cross-resource
atomicity will, and must be told rather than allowed to discover it.

## 2. Scope

**In scope**

- The `esqlc_*` ABI: versioning, header, and contracts directory.
- Connection acquisition without a connect statement: configuration precedence,
  lifetime, and thread scope.
- Statement translation: opaque body plus typed host-variable descriptors to a
  MariaDB prepared statement.
- Host-variable binding in both directions, applying 002's conversion and warning
  rules.
- Transaction control: `BEGIN WORK`, `COMMIT WORK`, `ROLLBACK WORK`.
- Implicit table open/close behaviour and its lifecycle.
- `sqlcode` as the primary status channel (structures come in 005).
- The stub runtime used by 001's tests, kept ABI-identical.

**Out of scope**

- `SQLCA` / `SQLSA` / `SQLDA` population → 005, 007
- Cursor mechanics → 004
- Guardian naming and TACL DEFINEs → 008 (`DIV-002`)
- Locking statements and `CONTROL *` → 008
- CPRL → 008

## 3. Requirements

| ID | Requirement | Citation |
|----|-------------|----------|
| FR-003.1 | The runtime exposes a versioned C ABI whose entry points are all prefixed `esqlc_`; generated code calls nothing else. | Principle V |
| FR-003.2 | The ABI header declares no MariaDB types and requires no MariaDB header to compile against. | Principle V |
| FR-003.3 | Every ABI entry point signature is recorded in `specs/003-runtime-mariadb-binding/contracts/` and changes there are versioned. | Principle V |
| FR-003.4 | A program with no connect statement obtains a connection from configuration; no source change is required to supply connection details. | Principle II, `[SQLPM/C §7 p.7-3]` |
| FR-003.5 | Configuration precedence is deterministic and documented, and a failure to resolve a connection is reported through `sqlcode` at the first statement, not by aborting the process. | Principle III |
| FR-003.6 | `BEGIN WORK` starts a transaction; `COMMIT WORK` commits it; `ROLLBACK WORK` rolls it back. | `[SQLPM/C §3 p.3-6]` `[DIV-010]` |
| FR-003.7 | A statement executed outside an explicit transaction behaves as SQL/MP's autocommit equivalent, and that behaviour is documented rather than inherited from MariaDB defaults. | `[EXTERNAL — SQLRM]` |
| FR-003.8 | `COMMIT WORK` and `ROLLBACK WORK` free resources held by the transaction, including open cursors. | `[SQLPM/C §3 p.3-6]` |
| FR-003.9 | Tables and views are opened implicitly on first use and the open persists for the program's execution unless closed. | `[SQLPM/C §4 p.4-2]` |
| FR-003.10 | An opaque statement body plus its host-variable descriptor list is translated to a parameterised statement; host-variable references are replaced by parameter placeholders, never by textual value interpolation. | Principle III, NFR-001.1 |
| FR-003.11 | Input binding applies 002's conversion rules and returns the specified warnings through `sqlcode`. | `[SQLPM/C §2 p.2-5]` |
| FR-003.12 | Output binding writes into host variables without appending null terminators to character arrays. | `[SQLPM/C §2 p.2-7]` |
| FR-003.13 | `sqlcode` is set after every statement: `0` success, `100` not found, negative for errors, positive non-`100` for warnings. | `[SQLPM/C §9 p.9-6]` |
| FR-003.14 | Input value too large for a column yields `sqlcode` 8300 with file-system detail 1031 retrievable. | `[SQLPM/C §2 p.2-5]` |
| FR-003.15 | The stub runtime used by 001's tests implements the same ABI and records calls without a database. | NFR-001.2 |
| NFR-003.1 | The runtime is testable in isolation, driven directly by handwritten C against the ABI. | Principle V |
| NFR-003.2 | No entry point interpolates host-variable text into SQL under any circumstance. | Principle III |
| NFR-003.3 | Connection and statement state is explicitly scoped; the scoping rule (per-process, per-thread) is documented, not emergent. | — |

## 4. Acceptance scenarios

### AS-003.1 — No-connect program runs
- **Given** a program with `#pragma SQL`, a declare section, and one `INSERT`,
  with connection details supplied only by configuration
- **When** run
- **Then** the row is inserted and `sqlcode` is `0`
- **Test:** `tests/conformance/003/implicit_connect.sqlc`

### AS-003.2 — Unresolvable connection reports, does not abort
- **Given** deliberately invalid configuration
- **When** the first statement executes
- **Then** the process is still running and `sqlcode` is negative
- **Test:** `tests/conformance/003/negative/no_connection.sqlc`

### AS-003.3 — Transaction round trip
- **Given** `BEGIN WORK`, an `INSERT`, `ROLLBACK WORK`
- **When** run, then the table is queried from a second connection
- **Then** the row is absent
- **Test:** `tests/conformance/003/txn_rollback.sqlc`

### AS-003.4 — Parameterisation, not interpolation
- **Given** a host variable whose value contains `'; DROP TABLE`
- **When** used in an `INSERT`
- **Then** the value is stored literally and no second statement executes
- **Test:** `tests/conformance/003/parameterised.sqlc`

### AS-003.5 — ABI isolation
- **Given** the generated C from any 001 fixture
- **When** compiled with only the `esqlc` ABI header available and no MariaDB
  headers on the include path
- **Then** it compiles
- **Test:** `tests/conformance/003/abi_isolation.sh`

## 5. Diagnostics

| Code | Condition | Default policy | Citation |
|------|-----------|----------------|----------|
| `ESQLC-3001` | Connection cannot be resolved from configuration | error via `sqlcode` | Principle III |
| `ESQLC-3002` | `COMMIT`/`ROLLBACK WORK` with no active transaction | warn | `[EXTERNAL — SQLRM]` |
| `ESQLC-3003` | Nested `BEGIN WORK` | error | `[EXTERNAL — SQLRM]` |
| `ESQLC-3004` | Statement body contains a construct the translator cannot parameterise | error | Principle III |
| `ESQLC-3005` | Host variable count or type mismatch against the statement's parameters | error | Principle III |

## 6. Open questions

| # | Question | Blocks | Resolution |
|---|----------|--------|------------|
| Q1 | What is SQL/MP's behaviour for a DML statement outside `BEGIN WORK` on an audited table — implicit transaction, or error? | FR-003.7 | unresolved, `[EXTERNAL — SQLRM]`. Determines whether autocommit is faithful or a divergence |
| Q2 | Is nested `BEGIN WORK` an error in SQL/MP, or does TMF support subtransactions? | FR-003.6, `ESQLC-3003` | unresolved, `[EXTERNAL — SQLRM]` |
| Q3 | What does §4's implicit open lifecycle actually guarantee, given error 8204 (lost open) exists? Can the runtime reopen transparently? | FR-003.9 | unresolved — interacts with 008's 8204 policy |
| Q4 | Connection scope: per-process, or per-thread? Customer programs are largely single-threaded Pathway servers, which argues per-process. | NFR-003.3 | unresolved |
| Q5 | Which configuration mechanism? This is also the natural home for `DIV-002` TACL DEFINE mapping. | FR-003.4, .5 | unresolved — decide jointly with 008 |

Q1 and Q2 both need `SQLRM`. Until they are answered, transaction semantics are
guesses, and guesses here corrupt data rather than merely annoying users.

## 7. Constitution check

| Principle | Compliant? | Note |
|-----------|-----------|------|
| I manual is the contract | partial | Transaction semantics are `[EXTERNAL]`; Q1, Q2 open |
| II source compatibility | yes | FR-003.4 forbids adding a connect statement to customer source |
| III no silent semantic change | yes | NFR-003.2; `ESQLC-3004` refuses rather than approximating |
| IV manual-derived tests first | yes | Five scenarios, one requiring a second connection to verify isolation |
| V layered / frozen ABI | yes | This feature *is* the ABI; FR-003.1..3 and AS-003.5 enforce it |
| VI byte-exact structures | n/a | No SQL structures here |
| VII divergence registered | partial | `DIV-010` proposed. Q1/Q3/Q5 will likely add more |
