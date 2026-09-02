/* T040 — NFR-003.1: the runtime must be drivable directly, with no
   preprocessor involved. Proves the two halves are genuinely separable. */
#include "esqlc.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    short num = 4242;
    char  desc[19];
    memcpy(desc, "DIRECT ABI CALL   ", 18);
    desc[18] = '\0';

    if (esqlc_session_begin("direct_abi", 2) != 0) { puts("session_begin failed"); return 1; }

    esqlc_hostvar_t hv[2] = {
        { &num,  0, ESQLC_T_INT,        2u, 2u,  0, 1, ESQLC_DIR_IN, 0 },
        { desc,  0, ESQLC_T_CHAR_FIXED, 18u, 19u, 0, 1, ESQLC_DIR_IN, 0 },
    };
    const char *sql = "INSERT INTO parts (part_num, part_desc) VALUES (?, ?)";

    if (esqlc_txn_begin() != 0) { puts("txn_begin failed"); return 1; }
    /* NULL table: a hand-written ABI caller has no scanner landmark to pass,
       and NULL means the SQLSA reports the sentinel rather than a guess. */
    if (esqlc_stmt_exec(sql, strlen(sql), hv, 2, NULL) != 0) {
        printf("exec failed sqlcode=%ld\n", esqlc_sqlcode());
        return 1;
    }
    if (esqlc_sqlcode() != 0) { printf("sqlcode=%ld\n", esqlc_sqlcode()); return 1; }
    if (esqlc_txn_commit() != 0) { puts("commit failed"); return 1; }
    esqlc_session_end();
    return 0;
}
