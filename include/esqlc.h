/* esqlc.h — the only header generated C includes.
 *
 * Constitution V: no MariaDB type, macro, or symbol may appear here. Generated
 * code calls esqlc_* entry points and nothing else. tests/harness/abi_isolation
 * compiles emitted C with no MariaDB header reachable to keep this true.
 *
 * Contract: specs/003-runtime-mariadb-binding/contracts/esqlc-abi.md
 * Kept in sync by tests/harness/contract_sync.sh.
 */
#ifndef ESQLC_H
#define ESQLC_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- host variable direction ---------------------------------------- */
#define ESQLC_DIR_IN   1
#define ESQLC_DIR_OUT  2   /* declared, unused until Gate 2 */

/* ---- host variable type families ------------------------------------ */
#define ESQLC_T_CHAR_FIXED   1   /* Gate 1 */
#define ESQLC_T_INT          2   /* Gate 1 — width in `width` */
#define ESQLC_T_CHAR_VAR     3
#define ESQLC_T_DECIMAL      4
#define ESQLC_T_FLOAT        5
#define ESQLC_T_DATETIME     6
#define ESQLC_T_INTERVAL     7

/* Host variable descriptor.
 *
 * `width` vs `capacity` is the load-bearing distinction (FR-002.30/.31).
 * For CHAR(18) declared char[19]: width 18, capacity 19. The runtime binds
 * exactly `width` bytes verbatim from `addr` — never strlen, never truncate at
 * a null byte, never pad. A single `length` field is how a strlen-based bind
 * sneaks in and silently diverges from SQL/MP.
 */
typedef struct {
    void          *addr;       /* the host variable itself                    */
    short         *ind_addr;   /* indicator, or NULL (unused in Gate 1)       */
    unsigned       type;       /* ESQLC_T_*                                   */
    unsigned       width;      /* bytes on the wire: 2/4/8, or column length  */
    unsigned       capacity;   /* declared array size, incl. terminator byte  */
    signed char    scale;      /* SETSCALE / C fixed; 0 in Gate 1             */
    unsigned char  is_signed;
    unsigned char  direction;  /* ESQLC_DIR_*                                 */
    unsigned short charset;    /* SQLDA charset id; 0 = UNKNOWN (slice SD-1)  */
} esqlc_hostvar_t;

/* Principle VI: layout is API, so it is asserted, not commented.
 * Requires C11 / C++11 — see the implementation note in gate-1-tasks.md. */
_Static_assert(sizeof(esqlc_hostvar_t) <= 40, "descriptor grew unexpectedly");
_Static_assert(offsetof(esqlc_hostvar_t, addr) == 0, "addr must lead");

/* ---- context resolution origins ------------------------------------- */
#define ESQLC_SRC_UNSET        0
#define ESQLC_SRC_ENVIRONMENT  1
#define ESQLC_SRC_FILE         2
#define ESQLC_SRC_COMPILED     3

/* ---- entry points ---------------------------------------------------- *
 * Every entry point returns int: 0 success, non-zero a transport failure.
 * That is distinct from sqlcode, which reports the SQL outcome. A statement
 * that legitimately finds no rows returns 0 with sqlcode 100.
 */

int  esqlc_context_ensure(void);
int  esqlc_context_origin(const char *setting);
int  esqlc_name_resolve(const char *sqlmp_name, char *out, size_t out_len);

int  esqlc_session_begin(const char *unit_name, int structures_version);
int  esqlc_session_end(void);

int  esqlc_txn_begin(void);
int  esqlc_txn_commit(void);
int  esqlc_txn_rollback(void);

int  esqlc_stmt_exec(const char *body, size_t body_len,
                     const esqlc_hostvar_t *vars, int var_count);

/* Cursors (Gate 3). A cursor is long-lived state spanning three statements,
 * which one-shot esqlc_stmt_exec cannot express. Identified by name; see the
 * contract's note that this pre-judges 004 Q5 (unit vs function scope). */
int  esqlc_cursor_open(const char *name,
                       const char *sql, size_t sql_len,
                       const esqlc_hostvar_t *vars, int var_count);
int  esqlc_cursor_fetch(const char *name,
                        const esqlc_hostvar_t *vars, int var_count);
int  esqlc_cursor_close(const char *name);

/* Diagnostic area (Gate 4). Registration rather than runtime-held state, so a
 * copied or EXTERNAL-shared SQLCA carries real data — see the contract. */
int  esqlc_sqlca_register(void *sqlca, size_t len);
int  esqlc_sqlca_getinfolist(const void *sqlca, int error_index,
                             const int *items, int n_items,
                             void *buf, size_t buf_len);

/* Statistics area (Gate 5). No accessors, and that is the difference from the
 * SQLCA: DIV-041 could hide that layout because the manual publishes none, but
 * §9 pp.9-15..9-16 publish this one, so programs index its fields by name and
 * an accessor would force a source change Principle II forbids.
 *
 * `version` is 300 or 330 and `len` must equal that version's SQLSA_LEN — two
 * published layouts mean the runtime cannot infer which it was handed. */
int  esqlc_sqlsa_register(void *sqlsa, size_t len, int version);

long esqlc_sqlcode(void);
int  esqlc_fs_detail(long *fs_code);

#ifdef __cplusplus
}
#endif
#endif /* ESQLC_H */
