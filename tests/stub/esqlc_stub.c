/* T007 — stub runtime. Implements the ABI, records calls, touches no database.
 *
 * NFR-001.2: this is what lets the whole Tier 1 suite build and run on a
 * machine with no MariaDB installed, which is also how Principle V stays
 * honest — if generated code ever needed a MariaDB symbol, this would not link.
 */
#include "esqlc.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#define MAXLOG 64
static char g_log[MAXLOG][512];
static int  g_n;
static long g_sqlcode;

static void note(const char *fmt, ...) {
    if (g_n >= MAXLOG) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g_log[g_n], sizeof g_log[0], fmt, ap);
    va_end(ap);
    ++g_n;
}

/* test accessors */
int  esqlc_stub_calls(void) { return g_n; }
const char *esqlc_stub_call(int i) { return (i >= 0 && i < g_n) ? g_log[i] : ""; }
void esqlc_stub_reset(void) { g_n = 0; g_sqlcode = 0; }

int esqlc_context_ensure(void) { note("context_ensure"); return 0; }
int esqlc_context_origin(const char *s) { (void)s; return ESQLC_SRC_COMPILED; }

int esqlc_name_resolve(const char *n, char *out, size_t cap) {
    note("name_resolve %s", n ? n : "(null)");
    snprintf(out, cap, "%s", n ? n : "");
    return 0;
}

int esqlc_session_begin(const char *u, int v) {
    note("session_begin %s %d", u ? u : "-", v);
    return 0;
}
int esqlc_session_end(void) { note("session_end"); return 0; }

int esqlc_txn_begin(void)    { note("txn_begin");    g_sqlcode = 0; return 0; }
int esqlc_txn_commit(void)   { note("txn_commit");   g_sqlcode = 0; return 0; }
int esqlc_txn_rollback(void) { note("txn_rollback"); g_sqlcode = 0; return 0; }

int esqlc_stmt_exec(const char *body, size_t len,
                    const esqlc_hostvar_t *vars, int n,
                    const char *table) {
    note("stmt_exec [%.*s] vars=%d table=%s", (int)len, body ? body : "", n,
         table ? table : "(none)");
    for (int i = 0; i < n; ++i)
        note("  hv%d type=%u width=%u capacity=%u signed=%u dir=%u",
             i, vars[i].type, vars[i].width, vars[i].capacity,
             vars[i].is_signed, vars[i].direction);
    g_sqlcode = 0;
    return 0;
}

long esqlc_sqlcode(void) { return g_sqlcode; }
int  esqlc_fs_detail(long *c) { if (c) *c = 0; return 0; }

/* Gate 10 — the stub satisfies the dynamic entry points so a stub-linked
 * fixture still links. It prepares nothing and describes nothing; NFR-001.2's
 * point is that the ABI is satisfiable with no database at all. */
int esqlc_prepare(const char *name, const char *sql, size_t sql_len) {
    (void)name; (void)sql; (void)sql_len; return 0;
}
int esqlc_describe(const char *name, void *sqlda, int num_entries, int version,
                   char *names_buf, size_t names_len) {
    (void)name; (void)sqlda; (void)num_entries; (void)version;
    (void)names_buf; (void)names_len; return 0;
}
int esqlc_execute(const char *name, void *sqlda, int num_entries, int version) {
    (void)name; (void)sqlda; (void)num_entries; (void)version; return 0;
}
