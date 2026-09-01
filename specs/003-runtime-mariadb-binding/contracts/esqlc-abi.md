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
| Structures — `SQLCA` | **decided** | — (Gate 4; layout private, `DIV-041`) |
| Structures — `SQLSA` | **shape decided, population partial** | `records_accessed` source — `DIV-011` |
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

## Cursors — added by Gate 3

The first growth of the ABI. Gates 1 and 2 added no entry points; a cursor is
long-lived state spanning three statements, which one-shot `esqlc_stmt_exec`
cannot express.

```c
/* Execute the cursor's statement and position before the first row. `sql` is
   the text captured at DECLARE; `vars` are its input references, read now
   rather than at declaration time (FR-004.12). */
int esqlc_cursor_open(const char *name,
                      const char *sql, size_t sql_len,
                      const esqlc_hostvar_t *vars, int var_count);

/* Advance one row and write the output descriptors. sqlcode 100 at end of set,
   with host variables left untouched (FR-004.14). Idempotent once exhausted:
   a further fetch also returns 100 and writes nothing (slice decision SD-3,
   provisional — the manual does not specify the position after exhaustion). */
int esqlc_cursor_fetch(const char *name,
                       const esqlc_hostvar_t *vars, int var_count);

/* Terminate the cursor and release its result set (FR-004.15). The prepared
   statement survives, so a subsequent OPEN re-runs without re-preparing. */
int esqlc_cursor_close(const char *name);
```

Cursors are identified **by name**, not by an opaque handle. The alternative
puts more state in generated code for no gain at realistic cursor counts, and
the name is already the source language's identifier for the thing.

> **This pre-judges an open question.** 004 Q5 asks whether cursors are scoped to
> the compilation unit or the function. A single runtime table keyed by name
> assumes unit scope. Gate 3 exercises one cursor so nothing depends on it, but
> if Q5 resolves to function scope these signatures need a scope qualifier.
> Recorded here rather than discovered later.

`esqlc_hostvar_t` is reused unaltered on the cursor path: `direction`
distinguishes the `OPEN` inputs from the `FETCH` outputs, and `ind_addr` carries
its Gate 2 meaning.

## Diagnostic area — added by Gate 4

```c
/* Register the program's SQLCA so the runtime populates it after each
   statement. Emitted by INCLUDE SQLCA. `len` must equal SQLCA_LEN; a mismatch
   means program and runtime disagree about the structure, which is an error
   rather than something to truncate into. */
int esqlc_sqlca_register(void *sqlca, size_t len);

/* SQLCAGETINFOLIST: copy a caller-selected subset of one diagnostic entry into
   `buf`, packed in item order at each item's documented size. Returns the
   documented 8510-8517 codes on misuse.

   Takes the SQLCA explicitly, and an error index. Both are forced by the
   manual: error 8512 is "invalid SQLCA structure" and 8515 is "error entry
   index less than zero or greater than the number of errors", neither of which
   can arise unless both are parameters. An earlier draft of this signature had
   neither, which would also have made a copied SQLCA unreadable — the very
   thing registration exists to preserve. */
int esqlc_sqlca_getinfolist(const void *sqlca, int error_index,
                            const int *items, int n_items,
                            void *buf, size_t buf_len);
```

`SQLCAFSCODE` needs no new entry point; it maps onto `esqlc_fs_detail`.

## Statistics area — added by Gate 5

```c
/* Register the program's SQLSA so the runtime populates it after each
   statement. Emitted by INCLUDE SQLSA. `version` is 300 or 330 and `len` must
   equal that version's SQLSA_LEN — two published layouts mean the runtime
   cannot infer which one it was handed from the pointer alone. */
int esqlc_sqlsa_register(void *sqlsa, size_t len, int version);
```

No accessors, and that is the difference from the `SQLCA`. `DIV-041` made the
`SQLCA` layout private because the manual publishes none, so its fields could
only be reached through `esqlc_sqlca_getinfolist`. The `SQLSA` layout **is**
published, in §9 pp.9-15..9-16, so programs index its fields by name and an
accessor would force a source change Principle II forbids.

Registration rather than runtime-held state, for Gate 4's reason reinforced by
§9 p.9-13: the manual tells programs to save values immediately after a
statement, and to declare more than one `SQLSA` where needed. Both idioms
require the data to live in the program's own storage.

> **The runtime writes this structure by offset**, since it cannot include
> preprocessor output. That makes the layout encoded twice — once in the
> emitter, once in the runtime — which is the drift Principle VI exists to
> prevent. `tests/harness/sqlsa_layout_sync.sh` compares the two and is part of
> the contract in practice, if not in signature.

> **Why registration rather than runtime-held state.** `DIV-041` makes the SQLCA
> layout private, which invites an accessor-reads-runtime-state design. That would
> silently break the two things §9 p.9-3 says programs do with the structure —
> copy it using `SQLCA_LEN`, and share it `EXTERNAL` across modules — because
> every saved copy would be an empty 430-byte husk. Writing into the program's
> own storage keeps the layout private *and* the data where the manual says it is.

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

### `esqlc_hostvar_t`

Defined by the Gate 1 plan under Principle VIII, ahead of 002 reaching `Ready`.
Fields the slice exercises are frozen; the rest are declared so the layout and
the type-family numbering are stable, and any unexercised type reaches
`ESQLC-1012` rather than a wrong bind.

```c
#define ESQLC_DIR_IN   1
#define ESQLC_DIR_OUT  2   /* declared, unused until Gate 2 */

#define ESQLC_T_CHAR_FIXED   1   /* Gate 1 */
#define ESQLC_T_INT          2   /* Gate 1 — width in `width` */
#define ESQLC_T_CHAR_VAR     3
#define ESQLC_T_DECIMAL      4
#define ESQLC_T_FLOAT        5
#define ESQLC_T_DATETIME     6
#define ESQLC_T_INTERVAL     7

typedef struct {
    void       *addr;
    short      *ind_addr;    /* indicator, or NULL */
    unsigned    type;        /* ESQLC_T_*                                     */
    unsigned    width;       /* bytes on the wire: 2/4/8, or column length    */
    unsigned    capacity;    /* declared array size, incl. terminator byte    */
    signed char scale;       /* SETSCALE or C `fixed`                         */
    unsigned char is_signed;
    unsigned char direction; /* ESQLC_DIR_*                                   */
    unsigned short charset;  /* SQLDA charset id; 0 = UNKNOWN                 */
} esqlc_hostvar_t;

_Static_assert(sizeof(esqlc_hostvar_t) <= 40, "descriptor grew unexpectedly");
_Static_assert(offsetof(esqlc_hostvar_t, addr) == 0, "addr must lead");
```

**`width` and `capacity` are deliberately separate**, and this is the load-bearing
detail of the whole descriptor. For `CHAR(18)` declared `char[19]`, `width` is 18
and `capacity` 19. The runtime binds exactly `width` bytes verbatim from `addr` —
no `strlen`, no truncation at a null byte, no padding (FR-002.30, FR-002.31). A
single `length` field is precisely how a `strlen`-based binding sneaks in and
silently diverges from SQL/MP on the commonest column type.

**Live as of Gate 2** (no signature change — behaviour only):

- `direction` — an `ESQLC_DIR_OUT` entry anywhere in the array makes
  `esqlc_stmt_exec` execute as a singleton select, fetching at most one row.
  This is why the field was defined in Gate 1 despite being unused then: it
  meant retrieval needed no new entry point.
- `ind_addr` — non-`NULL` means the program supplied an indicator, and the
  runtime writes `-1` for null or `0` for not-null. `NULL` means it did not, and
  a null column value is then SQL error 8423 (`ESQLC-4009`) rather than a
  silently zeroed variable.

Fields still not exercised: `scale` (pending 002 Q2/Q3), and `charset` beyond `0`
(pending 002 Q4; both gates bind `UNKNOWN` as the connection default per slice
decision SD-1).

## Open against this contract

| # | Question | Blocks |
|---|----------|--------|
| A1 | Does `esqlc_session_begin` need the pragma's option set, or only the structures version? | 001 Q2 |
| A2 | Is `esqlc_name_resolve` called by generated code, or internally by the runtime during statement translation? The latter keeps generated code smaller; the former makes name resolution testable without a database. | FR-003.23 |
| A3 | Does the thread check belong in every entry point or only in `esqlc_session_begin`? Per-call is safer and measurably slower. | FR-003.17 |
