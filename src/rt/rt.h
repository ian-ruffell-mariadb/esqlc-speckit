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

/* cursor.c */
void esqlc_rt_cursors_release_all(void);   /* FR-003.8 */

/* session.c */
int  esqlc_rt_check_thread(void);     /* 0 ok, non-zero refused */
int  esqlc_rt_ensure(void);           /* thread check + resolve + connect */

#endif
