# Technical Plan: Gate 1

**Slice:** `specs/gate-1.md` · **Status:** Draft
**Specs:** 001 (Ready), 002 (Clarifying), 003 (Clarifying) — planned under
Principle VIII

Principle VIII preconditions verified before planning: enumerated requirement
subset ✓, 15-row avoidance table covering every open question in 001/002/003 plus
005 Q8 ✓, two scoped decisions marked provisional ✓, non-proof section ✓.

## 1. Approach

Build the thinnest complete spine: a single-pass region scanner feeding a handler
table, an emitter, and a runtime implementing only the context / session /
transaction / execute portion of the ABI. Six statement keywords get real
handlers. **Every other construct hits `ESQLC-1012` naming its owning feature** —
the slice's correctness depends as much on what it loudly refuses as on what it
does.

The central design decision is how a host-variable reference becomes a bound
parameter without the preprocessor parsing SQL. **The scanner already records the
byte span of every `:name` reference while lexing the body** (it must, to respect
`--` comments and `"` strings). So the preprocessor emits the body with each
recorded span replaced by a `?` placeholder, plus an ordered descriptor array.
Substitution happens at spans the lexer found, never by understanding the
statement.

That resolves the apparent tension between NFR-001.1 (bodies stay opaque) and
FR-003.10 (values must be parameterised, never interpolated): the body is never
*parsed*, only *spliced at known offsets*, and the runtime receives finished
placeholder text plus typed values. The runtime does no SQL text manipulation at
all, which keeps FR-003.10 and NFR-003.2 structurally true rather than
enforced by review.

Languages: **C++17 for the preprocessor, C99 for the runtime.** The runtime must
be C — it is a C ABI consumed by generated C, and Principle V forbids leaking any
other vocabulary into it. The preprocessor has no such constraint and is mostly
string and container work, where C++ removes a large amount of incidental
allocation code. The split mirrors MariaDB's own.

## 2. Alternatives rejected

| Alternative | Why rejected |
|-------------|--------------|
| Runtime rewrites `:name` → `?` at execution | Puts SQL text manipulation in the runtime, making NFR-003.2 a review rule instead of a structural fact, and forces the runtime to re-lex the body it was told to treat as opaque. |
| Preprocessor parses the statement to find parameter positions | Needs an SQL/MP grammar this project does not have (`SQLRM`), and NFR-001.1 exists precisely to avoid owning one. |
| Named parameters passed through to MariaDB | MariaDB's C API binds positionally. Named binding would mean building a mapping layer for no gain. |
| C for the preprocessor too | Single toolchain is appealing, but a scanner with span tracking, a handler table and golden-file diffing in C99 is a lot of hand-rolled buffer code — cost with no architectural benefit. |
| Skip the runtime, emit MariaDB API calls directly | Violates Principle V outright, and would make the preprocessor untestable without a database. |

## 3. Components

| Component | Path | Responsibility | Slice scope |
|-----------|------|----------------|-------------|
| Diagnostics | `src/pp/diag.{cc,h}` | code registry, `file:line:col` | full |
| Source reader | `src/pp/source.cc` | file read, line/col tracking | no `SQL SOURCE` |
| Region scanner | `src/pp/scan.cc` | C/SQL split, `--` and `"` handling, `;` terminator, host-var span capture | full for gate forms |
| Context tracker | `src/pp/context.cc` | brace depth → position class | decl vs exec only |
| Pragma handler | `src/pp/pragma.cc` | `#pragma SQL`, mandatory position | bare pragma only |
| Declare-section handler | `src/pp/decl.cc` | parse `short` and `char[n]`, build descriptors | two type rows |
| Statement handlers | `src/pp/stmt.cc` | `INSERT`, `BEGIN`/`COMMIT`/`ROLLBACK WORK` | six keywords |
| Dispatcher | `src/pp/dispatch.cc` | keyword → handler, else `ESQLC-1012` | full |
| Emitter | `src/pp/emit.cc` | verbatim C, generated blocks, `#line` | full |
| **Runtime: context** | `src/rt/context.c` | env → file → compiled resolution, origin reporting | full |
| **Runtime: session** | `src/rt/session.c` | lazy connect, thread check, teardown | full |
| **Runtime: txn** | `src/rt/txn.c` | begin / commit / rollback | full |
| **Runtime: exec** | `src/rt/exec.c` | bind descriptors, execute, set `sqlcode` | input binding only |
| **Runtime: diag** | `src/rt/diag.c` | `sqlcode`, fs detail | `sqlcode` only |
| ABI header | `include/esqlc.h` | the only header generated C includes | — |
| Stub runtime | `tests/stub/esqlc_stub.c` | records calls, no database | full |

15 components. The five `src/rt/*` files are the first real implementation of the
003 contract.

## 4. Runtime ABI surface

All entry points already exist in
[`specs/003-runtime-mariadb-binding/contracts/esqlc-abi.md`](003-runtime-mariadb-binding/contracts/esqlc-abi.md)
except the host-variable descriptor, which that contract explicitly deferred to
"when 002 reaches `Ready`". The slice needs it now, so it is defined here and
**must land in the contracts file in the same change** (Principle V).

```c
/* Direction of a host variable in a statement. */
#define ESQLC_DIR_IN   1
#define ESQLC_DIR_OUT  2   /* declared, unused in this slice */

/* Type family. Only the two marked (gate) are handled in this slice;
   the rest are declared so the enum is stable, and hit ESQLC-1012. */
#define ESQLC_T_CHAR_FIXED   1   /* (gate) */
#define ESQLC_T_INT          2   /* (gate) — width in `width` */
#define ESQLC_T_CHAR_VAR     3
#define ESQLC_T_DECIMAL      4
#define ESQLC_T_FLOAT        5
#define ESQLC_T_DATETIME     6
#define ESQLC_T_INTERVAL     7

typedef struct {
    void       *addr;        /* the host variable itself                     */
    short      *ind_addr;    /* indicator, or NULL — unused in this slice    */
    unsigned    type;        /* ESQLC_T_*                                    */
    unsigned    width;       /* bytes: 2/4/8 for ints, column length for char */
    unsigned    capacity;    /* declared array size incl. terminator byte     */
    signed char scale;       /* SETSCALE / fixed; 0 in this slice             */
    unsigned char is_signed;
    unsigned char direction; /* ESQLC_DIR_*                                  */
    unsigned short charset;  /* SQLDA charset id; 0 = UNKNOWN (SD-1)         */
} esqlc_hostvar_t;
```

`width` versus `capacity` is the whole of FR-002.30. For `CHAR(18)` declared as
`char[19]`, `width` is 18 and `capacity` 19. The runtime binds exactly `width`
bytes from `addr` **verbatim** — no `strlen`, no truncation at a null, no
padding. Keeping the two numbers separate makes the correct behaviour the
structurally easy one; a single `length` field is how a `strlen`-based binding
sneaks in.

No other new entry points. The slice calls `esqlc_session_begin`,
`esqlc_txn_begin`/`_commit`/`_rollback`, `esqlc_stmt_exec`, `esqlc_sqlcode`,
`esqlc_session_end`.

## 5. Data structures

No SQL/MP structures are generated in this slice — no `SQLCA`, `SQLSA`, or
`SQLDA` — so Constitution VI imposes no layout obligations. `INCLUDE SQLCA` and
friends are dispatched to `ESQLC-1012`.

`esqlc_hostvar_t` is ABI, so it carries assertions even though it is not a
manual-specified structure:

```c
_Static_assert(sizeof(esqlc_hostvar_t) <= 40, "descriptor grew unexpectedly");
_Static_assert(offsetof(esqlc_hostvar_t, addr) == 0, "addr must lead");
```

The `<= 40` bound is deliberate: it makes an accidental field addition visible in
review rather than silently changing the array stride that generated code walks.

## 6. Requirement → component map

Every requirement in the slice's scoped set, exactly once.

| Requirement | Component(s) | Test |
|-------------|--------------|------|
| FR-001.1 `EXEC SQL`…`;` | scan | `gate-1/scan_basic` |
| FR-001.2 multi-line | scan | `gate-1/scan_multiline` |
| FR-001.7 pragma required | pragma | `gate-1/negative/no_pragma` |
| FR-001.11 decl position | context, dispatch | `gate-1/negative/decl_in_exec` |
| FR-001.12 exec position | context, dispatch | `gate-1/negative/exec_in_decl` |
| FR-001.15 dispatch + unknown | dispatch | `gate-1/negative/unimplemented` |
| FR-001.16 host-var forms | scan | `gate-1/hostvar_spans` |
| FR-001.18 `#line` fidelity | emit | `gate-1/line_fidelity` |
| FR-001.19 C verbatim | emit | `gate-1/passthrough` |
| NFR-001.1 opaque bodies | scan, dispatch | `gate-1/opaque_body` |
| NFR-001.2 no-DB testable | stub runtime | whole pp suite, no MariaDB |
| NFR-001.3 diag positions | diag | every negative case |
| FR-002.1 declare section | decl | `gate-1/declare_section` |
| FR-002.2 valid C identifier | decl | `gate-1/declare_section` |
| FR-002.3 `CHAR(l)` → `char[l+1]` | decl | `gate-1/type_char` |
| FR-002.9 integer widths | decl | `gate-1/type_int_widths` |
| FR-002.30 verbatim `width` bytes | rt/exec | `gate-1/rt/char_verbatim` |
| FR-002.31 no silent repair | rt/exec | `gate-1/rt/underfilled_stores_null` |
| NFR-002.2 static width asserts | decl, emit | `gate-1/static_asserts` |
| FR-003.1 `esqlc_*` only | emit, ABI header | `gate-1/abi_only_symbols` |
| FR-003.2 no MariaDB types | ABI header | `gate-1/abi_isolation` |
| FR-003.3 signatures in contracts | — (process) | `gate-1/contract_sync` |
| FR-003.4 no connect statement | rt/context | `gate-1/rt/implicit_connect` |
| FR-003.5 config failure via `sqlcode` | rt/context | `gate-1/rt/negative/bad_config` |
| FR-003.6 begin/commit/rollback | rt/txn | `gate-1/rt/txn_commit`, `txn_rollback` |
| FR-003.8 commit/rollback frees | rt/txn | `gate-1/rt/txn_rollback` |
| FR-003.10 parameterised, not interpolated | scan, emit, rt/exec | `gate-1/rt/injection_literal` |
| ~~FR-003.12~~ no terminator on output | — | **removed from scope — see below** |
| FR-003.13 `sqlcode` classes | rt/diag | `gate-1/rt/sqlcode_zero`, `sqlcode_error` |
| FR-003.16 one connection | rt/session | `gate-1/rt/single_connection` |
| FR-003.17 thread check | rt/session | `gate-1/rt/negative/second_thread` |
| FR-003.19 precedence | rt/context | `gate-1/rt/config_precedence` |
| FR-003.21 credentials routed | rt/context | `gate-1/rt/negative/creds_in_source` |
| NFR-003.1 runtime testable alone | rt/* | `gate-1/rt/direct_abi` |
| NFR-003.2 never interpolates | rt/exec | `gate-1/rt/injection_literal` |
| NFR-003.3 explicit scoping | rt/session | `gate-1/rt/single_connection` |

**Planning found one requirement the slice cannot test: FR-003.12** (output
binding appends no null terminator). Like FR-002.28 before it, it governs
retrieval, and the gate has no `SELECT`.

Surfaced by building the map rather than by inspection, and reported rather than
quietly mapped to a weak test, per the command's stop rule. **Fix applied:**
FR-003.12 removed from the slice's scoped set and deferred to Gate 2 alongside
FR-002.28. The row above is kept struck through so the finding stays visible
instead of vanishing from history.

That leaves 35 scoped requirements, all mapped exactly once.

## 7. Test strategy

Two tiers, because they have different prerequisites.

**Tier 1 — preprocessor, no database.** Golden-file: `.sqlc` → `.expected.c`,
whitespace-normalised, asserting emitted `esqlc_*` calls, descriptor array
contents, and `#line` placement. Negative: `.expected.diag` asserting code, line,
**and column**. Runs in CI with no MariaDB present, which is NFR-001.2's real
test.

**Tier 2 — runtime, live MariaDB.** Schema fixture from `specs/gate-1.md`.
Verification queries run on a **second connection** so committed state is
observed independently of the connection under test. Three cases carry most of
the value:

- `underfilled_stores_null` — the FR-002.31 check. A `strlen`-based binding
  passes every other test and fails this one.
- `txn_rollback` — proves the transaction is real, not autocommit.
- `injection_literal` — a host variable containing `'; DROP TABLE` stored
  literally, proving FR-003.10.

**Tier 0 — compile-only.** `abi_isolation` compiles emitted C with only
`include/esqlc.h` reachable and no MariaDB header on the include path.
`contract_sync` diffs the ABI header's declarations against the contracts
document and fails on drift.

## 8. Risks

| Risk | Impact | Mitigation |
|------|--------|------------|
| A `:name`-shaped sequence inside a body string literal or `--` comment gets captured as a host-variable span, corrupting the emitted statement | Silently wrong SQL — the worst class of failure here | Span capture runs in the *same* lexer pass as comment and string handling, never as a separate regex over the body. `hostvar_spans` includes `"a :b"` and `-- :c` cases |
| MariaDB's client binds `CHAR` using `strlen`-like semantics and strips trailing blanks or stops at a null | Divergence from SQL/MP on the single most common column type, invisible until a customer's comparison stops matching | `width`/`capacity` split in the descriptor; `underfilled_stores_null` asserts the null byte reaches the column; bind with explicit length always |
| `short` is bound as MariaDB's default integer width, widening `SMALLINT` silently | `DIV-001` claims width fidelity and would be false | `width` field drives the bind type explicitly; `type_int_widths` asserts per-width round trips |
| Emitted `#line` interacts badly with the host C preprocessor after macro expansion | Debuggability regresses, and criterion 7 is unfalsifiable by inspection | `line_fidelity` compiles with the pinned real compiler and asserts the *reported* line, never inspects `#line` text |
| Position-class heuristic misfires on the gate fixture's own shape | Gate blocked by a false diagnostic | Gate fixture is deliberately conventional; 001's edge-case corpus (T023) stays out of slice scope |
| Config precedence resolves credentials from an unintended source in CI | Tests pass against the wrong database, or a credential leaks into logs | `esqlc_context_origin` asserted per setting in `config_precedence`; no test may set credentials via environment |

## 9. Divergences introduced

None new. The slice inherits `DIV-001` (width-exact types) and `DIV-002`
(name mapping), and operates under provisional slice decisions SD-1 (`UNKNOWN`
charset binds as connection default) and SD-2 (program-declared `long sqlcode`).

Both SDs must be revisited when 002 Q4 and 005 Q8 close.
