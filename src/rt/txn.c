/* T082 — begin, commit, rollback; commit and rollback release resources
 *        (FR-003.6, FR-003.8).
 *
 * DIV-010 records the gap: these map onto InnoDB transactions, which are not
 * TMF transactions. 003 Q1 (behaviour outside an explicit transaction) and
 * Q2 (nesting) both remain open pending SQLRM, so this module deliberately
 * does no more than the three explicit operations the slice exercises.
 */
#include "rt.h"
#include <stdio.h>

int esqlc_txn_begin(void) {
    esqlc_state_t *s = esqlc_rt_state();
    if (esqlc_rt_ensure() != 0) return -1;

    /* 003 Q2 is open: SQL/MP's behaviour on a nested BEGIN WORK is unknown.
     * Refusing is the only choice that cannot silently corrupt a transaction
     * boundary (Constitution III). Revisit when Q2 closes. */
    if (s->in_txn) {
        fprintf(stderr, "ESQLC-3003: BEGIN WORK inside an active transaction\n");
        esqlc_rt_set_err_code(-3003);
        return -1;
    }
    if (mysql_real_query(s->conn, "START TRANSACTION", 17) != 0) {
        esqlc_rt_set_err_from_mysql(s->conn);
        return -1;
    }
    s->in_txn = true;
    esqlc_rt_set_ok();
    return 0;
}

int esqlc_txn_commit(void) {
    esqlc_state_t *s = esqlc_rt_state();
    if (esqlc_rt_ensure() != 0) return -1;
    if (!s->in_txn) {
        fprintf(stderr, "ESQLC-3002: COMMIT WORK with no active transaction\n");
        esqlc_rt_set_err_code(-3002);
        return -1;
    }
    if (mysql_commit(s->conn) != 0) {
        esqlc_rt_set_err_from_mysql(s->conn);
        return -1;
    }
    s->in_txn = false;
    esqlc_rt_set_ok();
    return 0;
}

int esqlc_txn_rollback(void) {
    esqlc_state_t *s = esqlc_rt_state();
    if (esqlc_rt_ensure() != 0) return -1;
    if (!s->in_txn) {
        fprintf(stderr, "ESQLC-3002: ROLLBACK WORK with no active transaction\n");
        esqlc_rt_set_err_code(-3002);
        return -1;
    }
    if (mysql_rollback(s->conn) != 0) {
        esqlc_rt_set_err_from_mysql(s->conn);
        return -1;
    }
    s->in_txn = false;
    esqlc_rt_set_ok();
    return 0;
}
