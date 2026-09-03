/* Internal runtime state. Not installed; generated code never sees this. */
#ifndef ESQLC_RT_H
#define ESQLC_RT_H

#include "esqlc.h"
#include <mysql.h>
#include <pthread.h>
#include <stdbool.h>

/* FR-003.16/.18: exactly one connection per process, and every piece of
 * session state scoped alongside it. */
typedef struct {
    MYSQL        *conn;
    bool          connected;
    bool          init_attempted;
    bool          in_txn;

    pthread_t     owner;         /* FR-003.17 single-thread affinity */
    bool          owner_set;

    long          sqlcode;
    long          fs_code;

    /* resolved context, with the origin of each setting (FR-003.20) */
    char          host[256];   int host_src;
    char          user[128];   int user_src;
    char          db[128];     int db_src;
    unsigned      port;        int port_src;
    char          optfile[512];int optfile_src;
} esqlc_state_t;

esqlc_state_t *esqlc_rt_state(void);

/* context.c */
int  esqlc_rt_resolve(void);          /* env -> file -> compiled */
int  esqlc_rt_connect(void);          /* lazy; idempotent */

/* diag.c */
void esqlc_rt_set_ok(void);
void esqlc_rt_set_notfound(void);
void esqlc_rt_set_err_from_mysql(MYSQL *m);
void esqlc_rt_set_err_from_stmt(MYSQL_STMT *s);
void esqlc_rt_set_err_code(long code);

/* sqlca.c */
void esqlc_rt_sqlca_populate(long sqlcode, long fs_code, long rows);

/* sqlsa.c — reset stamps every field with its sentinel and is called at the
 * start of every statement; populate then fills only what the statement can
 * honestly supply. A statement class that leaves the SQLSA undefined simply
 * does not call populate, so FR-005.19 needs no separate path (Gate 5). */
void esqlc_rt_sqlsa_reset(void);
void esqlc_rt_sqlsa_populate(long rows_used,
                             const char *const *tables, int n_tables);
int  esqlc_rt_sqlsa_version(void);
int  esqlc_rt_sqlsa_num_tables(void);
void esqlc_rt_sqlsa_from_stmt(MYSQL_STMT *st, long rows_used);
void esqlc_rt_sqlsa_from_table(const char *table, long rows_used);

/* Gate 8 deliberately gives the runtime NO charset table.
 *
 * The plan had one, mapping the descriptor's charset id to a MariaDB charset
 * name, for the §10 p.10-11 ID check. That check turned out not to be
 * implementable — result metadata reports the result set's charset, never the
 * column's — and with it gone the table had no consumer at all. A mutation
 * remapping KSC5601 to sjis survived every test, which is what proved it dead.
 *
 * It is deleted rather than kept, because a table that looks authoritative and
 * is consulted by nothing is worse than no table. Under the binary client
 * charset the runtime does not need the name: bytes go verbatim and the
 * column's own set governs. The descriptor still carries the id, because
 * FR-002.4 requires the association to be recorded and feature 007 needs it for
 * the SQLDA — it is declarative at runtime, and DIV-055 says so.
 */

/* cursor.c */
void esqlc_rt_cursors_release_all(void);   /* FR-003.8 */

/* session.c */
int  esqlc_rt_check_thread(void);     /* 0 ok, non-zero refused */
int  esqlc_rt_ensure(void);           /* thread check + resolve + connect */

#endif
