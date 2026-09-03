/* T081 — sqlcode classes (FR-003.13).
 *
 *   0            success
 *   100          not found
 *   negative     error
 *   positive !=100  warning
 *
 * Error codes are the MariaDB errno negated, so a caller can recover the
 * underlying number. SQL/MP's own numbering is not invented here — DIV-042
 * covers the warning values and remains open.
 */
#include "rt.h"

void esqlc_rt_set_ok(void) {
    esqlc_state_t *s = esqlc_rt_state();
    s->sqlcode = 0;
    s->fs_code = 0;
    esqlc_rt_sqlca_populate(0, 0, 0);
}

void esqlc_rt_set_notfound(void) {
    esqlc_rt_state()->sqlcode = 100;
    esqlc_rt_sqlca_populate(100, 0, 0);
}

void esqlc_rt_set_err_code(long code) {
    esqlc_state_t *s = esqlc_rt_state();
    s->sqlcode = code < 0 ? code : -code;
    esqlc_rt_sqlca_populate(s->sqlcode, s->fs_code, 0);
}


/* T779, T781 — DIV-054. MariaDB's out-of-range condition is error 1264
 * (SQLSTATE 22003); SQL/MP's is 8300. The code a program branches on is
 * reproducible and is reproduced.
 *
 * The file-system detail is not. §2 p.2-5 pairs 8300 with a Guardian detail of
 * 1031, a numeric-overflow file-system error, and no MariaDB condition
 * corresponds to it. So the detail carries the sentinel rather than 1031:
 * fabricating a Guardian error number for a condition no Guardian file system
 * reported would be undetectable, because 1031 is exactly the value a program
 * would expect to see.
 */
#define ESQLC_MARIADB_OUT_OF_RANGE 1264
#define ESQLC_FS_SENTINEL          (-1)

static long map_sqlcode(unsigned e) {
    if (e == ESQLC_MARIADB_OUT_OF_RANGE) return -8300;   /* FR-002.25 */
    return e ? -(long)e : -1;
}

static long map_fs_detail(unsigned e) {
    if (e == ESQLC_MARIADB_OUT_OF_RANGE) return ESQLC_FS_SENTINEL;
    return (long)e;
}

void esqlc_rt_set_err_from_mysql(MYSQL *m) {
    esqlc_state_t *s = esqlc_rt_state();
    unsigned e = m ? mysql_errno(m) : 0;
    s->sqlcode = map_sqlcode(e);
    s->fs_code = map_fs_detail(e);
    esqlc_rt_sqlca_populate(s->sqlcode, s->fs_code, 0);
}

void esqlc_rt_set_err_from_stmt(MYSQL_STMT *st) {
    esqlc_state_t *s = esqlc_rt_state();
    unsigned e = st ? mysql_stmt_errno(st) : 0;
    s->sqlcode = map_sqlcode(e);
    s->fs_code = map_fs_detail(e);
    esqlc_rt_sqlca_populate(s->sqlcode, s->fs_code, 0);
}

long esqlc_sqlcode(void) { return esqlc_rt_state()->sqlcode; }

int esqlc_fs_detail(long *fs_code) {
    if (!fs_code) return -1;
    *fs_code = esqlc_rt_state()->fs_code;
    return 0;
}
