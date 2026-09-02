/* T677 — the mysql_info parse, and specifically its failure path.
 *
 * mysql_info returns a formatted server *message*, not an API result. The
 * format is long-standing and documented but is not a contract, so a reworded
 * or localised build would break the parse silently. The fallback must be the
 * SENTINEL and never zero: zero is a legitimate altered count, so returning it
 * for a failed parse turns a missing measurement into an untrue statistic.
 *
 * Mutation testing is why this file exists. The sentinel branch is unreachable
 * from any live fixture — a real UPDATE always gets a Changed: field — so a
 * mutant returning 0 there survived. Calling the parser directly reaches it. */
#include <stdio.h>

extern long esqlc_rt_parse_changed(const char *info, long matched);

int main(void) {
    struct { const char *info; long matched; long want; const char *why; } t[] = {
        { "Rows matched: 1  Changed: 1  Warnings: 0", 1,  1, "normal update" },
        { "Rows matched: 1  Changed: 0  Warnings: 0", 1,  0, "matched but unchanged" },
        { "Rows matched: 5  Changed: 3  Warnings: 0", 5,  3, "partial change" },
        { NULL,                                       7,  7, "no info line: INSERT/DELETE" },
        { "Records: 3  Duplicates: 0  Warnings: 0",   3, -1, "no Changed: field" },
        { "Rows matched: 1  Changed:   Warnings: 0",  1, -1, "Changed: with no number" },
        { "Changed: not-a-number",                    1, -1, "non-numeric" },
        { "",                                         2, -1, "empty info" },
    };
    int bad = 0;
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        long got = esqlc_rt_parse_changed(t[i].info, t[i].matched);
        if (got != t[i].want) {
            printf("FAIL %s: got %ld want %ld\n", t[i].why, got, t[i].want);
            bad = 1;
        }
    }
    if (!bad) printf("parse_changed: 8 cases ok, fallback is the sentinel\n");
    return bad;
}
