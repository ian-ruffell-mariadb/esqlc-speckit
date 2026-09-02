/* T051 — FR-003.5. An unresolvable connection must report through sqlcode and
   leave the process running, never abort it. */
#include "esqlc.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    const char *sql = "INSERT INTO parts (part_num) VALUES (1)";
    int rc = esqlc_stmt_exec(sql, strlen(sql), 0, 0, NULL);
    long sc = esqlc_sqlcode();
    printf("rc=%d sqlcode=%ld (process still running)\n", rc, sc);
    return (rc != 0 && sc < 0) ? 0 : 1;
}
