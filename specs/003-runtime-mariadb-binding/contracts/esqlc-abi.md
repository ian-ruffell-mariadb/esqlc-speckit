# Runtime ABI contract

**ABI version:** 0.1.0-draft · **Owner:** feature 003 · **Status:** partial

The frozen surface between generated C and the runtime (Constitution V).
Generated code calls these entry points and nothing else — no MariaDB types, no
`mysql.h`, no MariaDB symbol ever appears in emitted code or in this header.

Changing a signature here is a minor-version event with a migration note.
Anything not yet listed is not yet callable: a preprocessor handler needing an
absent entry point must add it here in the same change.

## Coverage

| Area | Status | Blocked on |
|---|---|---|
| Context and connection | **decided** | — |
| Session lifecycle | **decided** | — |
| Diagnostics channel (`sqlcode`) | **decided** | warning values — `DIV-042` |
| Transaction control | **shape decided, semantics open** | 003 Q1, Q2 — `SQLRM` |
| Statement execution and binding | outline only | 002 conversion rules |
| Cursors | not started | feature 004, incl. Q6/Q7 |
| Structures (SQLCA/SQLSA) | not started | feature 005 |
| Descriptors (SQLDA) | not started | feature 007, `DIV-040` |

## Conventions

- Every entry point returns `int`: `0` success, non-zero a runtime failure code.
  This is the *transport* result, distinct from `sqlcode`, which reports the
  SQL outcome. A statement that legitimately finds no rows returns transport `0`
  with `sqlcode` `100`.
- No entry point allocates memory the caller must free. Buffers are caller-owned,
  consistent with §10's model where the program owns descriptor storage.
- No entry point interpolates host-variable text into SQL under any
  circumstances (NFR-003.2). Values travel as bound parameters only.
- Opaque handles are `esqlc_*_t` pointers; their contents are private.

## Context and connection

Resolution order is environment, then configuration file, then compile-time
defaults, highest precedence first (FR-003.19).

```c
/* Resolved lazily on first statement; explicit call is optional and only
   useful to surface configuration errors early. Idempotent. */
int esqlc_context_ensure(void);

/* Which source supplied a resolved setting — for diagnosability (FR-003.20).
   Returns one of the ESQLC_SRC_* values, or ESQLC_SRC_UNSET. */
int esqlc_context_origin(const char *setting);

#define ESQLC_SRC_UNSET        0
#define ESQLC_SRC_ENVIRONMENT  1
#define ESQLC_SRC_FILE         2
#define ESQLC_SRC_COMPILED     3

/* Resolve a Guardian object name or DEFINE reference through the mapping
   sections. Returns non-zero and sets sqlcode for an unmapped name; never
   passes an unresolved name through to the server (FR-003.23). */
int esqlc_name_resolve(const char *sqlmp_name, char *out, size_t out_len);
```

Credentials are never parameters here. They reach MariaDB through its own
option-file and credential mechanisms (FR-003.21), so no signature in this
contract can carry one.

## Session lifecycle

```c
/* Called once per process, emitted at the first embedded construct's site.
   Establishes the thread-affinity check of FR-003.17. */
int esqlc_session_begin(const char *unit_name, int structures_version);

/* Process exit. Closes cursors, rolls back any open transaction, releases the
   connection. Safe to call twice. */
int esqlc_session_end(void);
```

`esqlc_session_begin` takes the `INCLUDE STRUCTURES` version so the runtime
knows which SQLCA/SQLSA layout the program was compiled against — the
default-to-version-2 rule (FR-005.10) makes this non-optional.

## Diagnostics channel

```c
/* sqlcode for the most recent statement: 0 success, 100 not found,
   negative error, positive non-100 warning (FR-003.13). */
long esqlc_sqlcode(void);

/* File-system / OS detail behind a transport or 8300-class failure,
   backing SQLCAFSCODE (FR-003.14). */
int esqlc_fs_detail(long *fs_code);
```

`WHENEVER` generates inline checks against `esqlc_sqlcode()` in the order NOT
FOUND, SQLERROR, SQLWARNING (FR-005.5). It is a preprocessor construct and adds
no ABI surface of its own.

## Transaction control

Shape is settled; semantics are not. `DIV-010` records the TMF-to-InnoDB gap, and
003 Q1 (behaviour outside an explicit transaction) and Q2 (nested `BEGIN WORK`)
both need `SQLRM` before these are implementable.

```c
int esqlc_txn_begin(void);
int esqlc_txn_commit(void);    /* frees resources incl. open cursors */
int esqlc_txn_rollback(void);  /* likewise */
```

## Statement execution — outline

Not frozen. Recorded to fix the shape the preprocessor emits against, so
features 001 and 004 can proceed against a stub.

```c
/* Opaque statement body plus a typed host-variable descriptor list.
   The body is carried verbatim from source (NFR-001.1); the runtime
   parameterises it against the descriptors (FR-003.10). */
int esqlc_stmt_exec(const char *body, size_t body_len,
                    const esqlc_hostvar_t *vars, int var_count);
```

`esqlc_hostvar_t` must express everything 002 decides: width-exact type
(`DIV-001`), character set, scale from `SETSCALE` or C `fixed`, the `TYPE AS`
assertion, indicator pointer, and direction. Its definition lands here when 002
reaches `Ready`.

## Open against this contract

| # | Question | Blocks |
|---|----------|--------|
| A1 | Does `esqlc_session_begin` need the pragma's option set, or only the structures version? | 001 Q2 |
| A2 | Is `esqlc_name_resolve` called by generated code, or internally by the runtime during statement translation? The latter keeps generated code smaller; the former makes name resolution testable without a database. | FR-003.23 |
| A3 | Does the thread check belong in every entry point or only in `esqlc_session_begin`? Per-call is safer and measurably slower. | FR-003.17 |
