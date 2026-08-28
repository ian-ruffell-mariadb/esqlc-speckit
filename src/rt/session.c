/* T077 — lazy connect, one connection per process, teardown (FR-003.16, NFR-003.3)
 * T078 — single-thread affinity check (FR-003.17)
 *
 * A NonStop program is a process, and Pathway servers are processes, so
 * per-process is the faithful scope. MariaDB's client handle is not safe for
 * concurrent use, and silently sharing it corrupts results — so a second
 * thread is refused rather than tolerated (Constitution III).
 */
#include "rt.h"
#include <stdio.h>
#include <string.h>

int esqlc_rt_check_thread(void) {
    esqlc_state_t *s = esqlc_rt_state();
    pthread_t self = pthread_self();
    if (!s->owner_set) {
        s->owner = self;
        s->owner_set = true;
        return 0;
    }
    if (!pthread_equal(s->owner, self)) {
        fprintf(stderr,
                "ESQLC-3006: the esqlc runtime was entered from a second thread; "
                "single-threaded execution is the supported model (FR-003.17)\n");
        esqlc_rt_set_err_code(-3006);
        return -1;
    }
    return 0;
}

int esqlc_rt_ensure(void) {
    if (esqlc_rt_check_thread() != 0) return -1;
    if (esqlc_rt_connect() != 0) return -1;
    return 0;
}

int esqlc_session_begin(const char *unit_name, int structures_version) {
    (void)unit_name;
    /* The structures version is accepted now and consumed by feature 005; the
     * default-to-version-2 rule means the runtime must eventually know it. */
    (void)structures_version;
    return esqlc_rt_ensure();
}

int esqlc_session_end(void) {
    esqlc_state_t *s = esqlc_rt_state();
    if (s->conn) {
        if (s->in_txn) {
            mysql_rollback(s->conn);   /* FR-003.8: release on teardown */
            s->in_txn = false;
        }
        mysql_close(s->conn);
        s->conn = NULL;
    }
    s->connected = false;
    s->init_attempted = false;
    s->owner_set = false;
    return 0;
}
