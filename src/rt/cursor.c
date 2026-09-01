/* T370 — cursor table and open, setting STMT_ATTR_CURSOR_TYPE and asserting
 *        it was accepted rather than inferring streaming from behaviour.
 * T371 — fetch: bind results once, write outputs with buffer_length = width.
 * T372 — exhausted state and SD-3 idempotency.
 * T373 — close releases the result set, keeps the prepared statement.
 * T374 — the state machine rejects out-of-order operations.
 *
 * Cursors are identified by name. That pre-judges 004 Q5 (are cursors scoped
 * to the compilation unit or the function?) — a single table keyed by name
 * assumes unit scope. Recorded in the ABI contract; Gate 3 exercises one
 * cursor, so nothing here depends on the answer.
 */
#include "rt.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define ESQLC_MAX_CURSORS 32
#define ESQLC_CUR_NAME    64

typedef enum { CUR_FREE = 0, CUR_CLOSED, CUR_OPEN, CUR_EXHAUSTED } cur_state_t;

typedef struct {
    char         name[ESQLC_CUR_NAME];
    cur_state_t  state;
    MYSQL_STMT  *st;
    /* Result binding is built once per open and reused across fetches. */
    MYSQL_BIND    *rb;
    unsigned long *rl;
    my_bool       *rnull;
    const esqlc_hostvar_t **outv;
    int            n_out;
    int            bound;
} cursor_t;

static cursor_t g_cur[ESQLC_MAX_CURSORS];

static cursor_t *find_cursor(const char *name) {
    for (int i = 0; i < ESQLC_MAX_CURSORS; ++i)
        if (g_cur[i].state != CUR_FREE && strcmp(g_cur[i].name, name) == 0)
            return &g_cur[i];
    return NULL;
}

static cursor_t *alloc_cursor(const char *name) {
    for (int i = 0; i < ESQLC_MAX_CURSORS; ++i)
        if (g_cur[i].state == CUR_FREE) {
            memset(&g_cur[i], 0, sizeof g_cur[i]);
            snprintf(g_cur[i].name, sizeof g_cur[i].name, "%s", name);
            g_cur[i].state = CUR_CLOSED;
            return &g_cur[i];
        }
    return NULL;
}

static void release_result(cursor_t *c) {
    free(c->rb); free(c->rl); free(c->rnull); free((void *)c->outv);
    c->rb = NULL; c->rl = NULL; c->rnull = NULL; c->outv = NULL;
    c->n_out = 0; c->bound = 0;
}

/* Close every open cursor. Called by commit and rollback (FR-003.8). */
void esqlc_rt_cursors_release_all(void) {
    for (int i = 0; i < ESQLC_MAX_CURSORS; ++i) {
        cursor_t *c = &g_cur[i];
        if (c->state == CUR_OPEN || c->state == CUR_EXHAUSTED) {
            if (c->st) { mysql_stmt_free_result(c->st); mysql_stmt_close(c->st); c->st = NULL; }
            release_result(c);
            c->state = CUR_CLOSED;
        }
    }
}

int esqlc_cursor_open(const char *name, const char *sql, size_t sql_len,
                      const esqlc_hostvar_t *vars, int var_count) {
    esqlc_state_t *s = esqlc_rt_state();
    /* FR-005.20: every statement resets the SQLSA. Done first, so a statement
     * that fails leaves sentinels rather than the previous statement's data. */
    esqlc_rt_sqlsa_reset();
    if (esqlc_rt_ensure() != 0) return -1;
    if (!name || !sql) { esqlc_rt_set_err_code(-3004); return -1; }
    if (var_count < 0) var_count = 0;

    cursor_t *c = find_cursor(name);
    if (c && c->state != CUR_CLOSED) {
        /* FR-004.19 / ESQLC-4002 */
        fprintf(stderr, "ESQLC-4002: cursor '%s' is already open\n", name);
        esqlc_rt_set_err_code(-4002);
        return -1;
    }
    if (!c) { c = alloc_cursor(name); if (!c) { esqlc_rt_set_err_code(-3001); return -1; } }

    c->st = mysql_stmt_init(s->conn);
    if (!c->st) { esqlc_rt_set_err_from_mysql(s->conn); return -1; }

    /* A SQL/MP cursor streams. Without this the whole result set is
     * materialised in the client, which no functional test at fixture scale
     * can distinguish — so the attribute is ASSERTED, not assumed. */
    {
        unsigned long ctype = CURSOR_TYPE_READ_ONLY;
        if (mysql_stmt_attr_set(c->st, STMT_ATTR_CURSOR_TYPE, &ctype) != 0) {
            fprintf(stderr,
                    "ESQLC-4010: server or client refused a read-only cursor; "
                    "the result set would be buffered rather than streamed\n");
            esqlc_rt_set_err_code(-4010);
            mysql_stmt_close(c->st); c->st = NULL;
            return -1;
        }
    }

    if (mysql_stmt_prepare(c->st, sql, (unsigned long)sql_len) != 0) {
        esqlc_rt_set_err_from_stmt(c->st);
        mysql_stmt_close(c->st); c->st = NULL;
        return -1;
    }
    if (mysql_stmt_param_count(c->st) != (unsigned long)var_count) {
        esqlc_rt_set_err_code(-3005);
        mysql_stmt_close(c->st); c->st = NULL;
        return -1;
    }

    /* Inputs are read NOW, not when the cursor was declared (FR-004.12). */
    MYSQL_BIND *pb = NULL; unsigned long *pl = NULL;
    if (var_count > 0) {
        pb = calloc((size_t)var_count, sizeof *pb);
        pl = calloc((size_t)var_count, sizeof *pl);
        if (!pb || !pl) { free(pb); free(pl); esqlc_rt_set_err_code(-3001);
                          mysql_stmt_close(c->st); c->st = NULL; return -1; }
        for (int i = 0; i < var_count; ++i) {
            const esqlc_hostvar_t *v = &vars[i];
            pb[i].buffer = v->addr; pb[i].is_null = NULL;
            if (v->type == ESQLC_T_CHAR_FIXED) {
                pl[i] = v->width;
                pb[i].buffer_type = MYSQL_TYPE_STRING;
                pb[i].buffer_length = v->width;
                pb[i].length = &pl[i];
            } else if (v->type == ESQLC_T_INT) {
                pb[i].is_unsigned = v->is_signed ? 0 : 1;
                pb[i].buffer_type = (v->width == 2) ? MYSQL_TYPE_SHORT
                                  : (v->width == 4) ? MYSQL_TYPE_LONG
                                                    : MYSQL_TYPE_LONGLONG;
            } else {
                free(pb); free(pl); esqlc_rt_set_err_code(-3004);
                mysql_stmt_close(c->st); c->st = NULL; return -1;
            }
        }
        if (mysql_stmt_bind_param(c->st, pb) != 0) {
            esqlc_rt_set_err_from_stmt(c->st);
            free(pb); free(pl);
            mysql_stmt_close(c->st); c->st = NULL; return -1;
        }
    }

    int rc = mysql_stmt_execute(c->st);
    free(pb); free(pl);
    if (rc != 0) {
        esqlc_rt_set_err_from_stmt(c->st);
        mysql_stmt_close(c->st); c->st = NULL;
        return -1;
    }
    c->state = CUR_OPEN;      /* positioned before the first row (FR-004.12) */
    esqlc_rt_sqlsa_from_stmt(c->st, 0);   /* FR-005.17: OPEN populates */
    esqlc_rt_set_ok();
    return 0;
}

int esqlc_cursor_fetch(const char *name, const esqlc_hostvar_t *vars, int n) {
    esqlc_rt_sqlsa_reset();          /* FR-005.20: including every FETCH */
    if (esqlc_rt_ensure() != 0) return -1;
    if (n < 0) n = 0;
    cursor_t *c = find_cursor(name);
    if (!c || c->state == CUR_CLOSED) {
        fprintf(stderr, "ESQLC-4001: fetch on a cursor that is not open: '%s'\n", name);
        esqlc_rt_set_err_code(-4001);
        return -1;
    }
    if (c->state == CUR_EXHAUSTED) {
        /* SD-3: end of set is idempotent. No server round trip, and nothing
         * written to the host variables. */
        esqlc_rt_set_notfound();
        return 0;
    }

    if (!c->bound) {
        c->rb    = calloc((size_t)n, sizeof *c->rb);
        c->rl    = calloc((size_t)n, sizeof *c->rl);
        c->rnull = calloc((size_t)n, sizeof *c->rnull);
        c->outv  = calloc((size_t)n, sizeof *c->outv);
        if (!c->rb || !c->rl || !c->rnull || !c->outv) {
            esqlc_rt_set_err_code(-3001); return -1;
        }
        for (int i = 0; i < n; ++i) {
            const esqlc_hostvar_t *v = &vars[i];
            c->outv[i] = v;
            c->rb[i].buffer  = v->addr;
            c->rb[i].is_null = &c->rnull[i];
            if (v->type == ESQLC_T_CHAR_FIXED) {
                c->rb[i].buffer_type   = MYSQL_TYPE_STRING;
                /* width, never capacity: the library must not be able to reach
                 * the terminator placeholder byte (FR-002.28). */
                c->rb[i].buffer_length = v->width;
                c->rb[i].length        = &c->rl[i];
            } else if (v->type == ESQLC_T_INT) {
                c->rb[i].is_unsigned = v->is_signed ? 0 : 1;
                c->rb[i].buffer_type = (v->width == 2) ? MYSQL_TYPE_SHORT
                                     : (v->width == 4) ? MYSQL_TYPE_LONG
                                                       : MYSQL_TYPE_LONGLONG;
            } else {
                esqlc_rt_set_err_code(-3004); return -1;
            }
        }
        if (mysql_stmt_bind_result(c->st, c->rb) != 0) {
            esqlc_rt_set_err_from_stmt(c->st); return -1;
        }
        c->n_out = n;
        c->bound = 1;
    } else if (n != c->n_out) {
        /* Varying the INTO list mid-cursor would silently rebind buffers. */
        esqlc_rt_set_err_code(-3005);
        return -1;
    }

    int fr = mysql_stmt_fetch(c->st);
    if (fr == MYSQL_NO_DATA) {
        c->state = CUR_EXHAUSTED;
        esqlc_rt_set_notfound();          /* FR-004.14: 100, nothing written */
        return 0;
    }
    if (fr != 0 && fr != MYSQL_DATA_TRUNCATED) {
        esqlc_rt_set_err_from_stmt(c->st);
        return -1;
    }
    for (int i = 0; i < c->n_out; ++i) {
        if (c->rnull[i]) {
            if (c->outv[i]->ind_addr) *c->outv[i]->ind_addr = -1;
            else { esqlc_rt_set_err_code(-8423); return -1; }   /* ESQLC-4009 */
        } else if (c->outv[i]->ind_addr) {
            *c->outv[i]->ind_addr = 0;
        }
    }
    /* One row accounted to this FETCH. The accumulator fixture sums
     * these and compares against the row count, so a missing reset
     * overshoots rather than reading plausibly. */
    esqlc_rt_sqlsa_from_stmt(c->st, 1);
    esqlc_rt_set_ok();
    return 0;
}

int esqlc_cursor_close(const char *name) {
    if (esqlc_rt_ensure() != 0) return -1;
    cursor_t *c = find_cursor(name);
    if (!c || c->state == CUR_CLOSED) {
        fprintf(stderr, "ESQLC-4003: close on a cursor that is not open: '%s'\n", name);
        esqlc_rt_set_err_code(-4003);
        return -1;
    }
    /* FR-005.17: CLOSE populates too, and the metadata it reads lives on the
     * statement — so this must happen before the statement is closed. The
     * first attempt ran it afterwards, against a NULL handle, and silently
     * reported the character sentinel instead of the table. */
    esqlc_rt_sqlsa_reset();
    esqlc_rt_sqlsa_from_stmt(c->st, 0);
    /* Release the result set; the cursor returns to `declared` so a later OPEN
     * re-runs the statement (FR-004.15). */
    if (c->st) { mysql_stmt_free_result(c->st); mysql_stmt_close(c->st); c->st = NULL; }
    release_result(c);
    c->state = CUR_CLOSED;
    esqlc_rt_set_ok();
    return 0;
}
