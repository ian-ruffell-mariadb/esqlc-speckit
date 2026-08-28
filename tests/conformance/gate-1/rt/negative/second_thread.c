/* T052 — FR-003.17. A second thread entering the ABI must be refused, not
   silently sharing a client handle that is not safe for concurrent use. */
#include "esqlc.h"
#include <pthread.h>
#include <stdio.h>

static int second_rc = 0;

static void *other(void *arg) {
    (void)arg;
    second_rc = esqlc_txn_begin();     /* must be refused */
    return NULL;
}

int main(void) {
    if (esqlc_session_begin("second_thread", 2) != 0) {
        puts("session_begin failed on the owning thread");
        return 1;
    }
    pthread_t t;
    pthread_create(&t, NULL, other, NULL);
    pthread_join(t, NULL);
    printf("second thread rc=%d sqlcode=%ld\n", second_rc, esqlc_sqlcode());
    esqlc_session_end();
    return second_rc == 0 ? 1 : 0;     /* refusal is success */
}
