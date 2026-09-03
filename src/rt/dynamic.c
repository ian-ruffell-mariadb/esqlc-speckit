/* T1082-T1092 — dynamic SQL: prepare, describe, execute.
 *
 * The descriptor is the program's. §10's whole model is the program allocating
 * (p.10-30 mallocs, p.10-37 allocates at compile time) and FR-007.8 reserving
 * fields to it, so this module writes FOUR of a sqlvar's eight fields and
 * leaves the other four exactly as it found them:
 *
 *     writes   data_type, data_len, precision, null_info
 *     never    var_ptr, ind_ptr, cprl_ptr, reserved
 *
 * §10 p.10-59 has the program initialise ind_ptr itself, "even when the program
 * does not handle null values", and FR-007.6b says a program must not assume
 * the entry ends after cprl_ptr — so `reserved` is preserved too.
 */
#include "rt.h"
#include "rt_sqlda_offsets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_STMTS 16

typedef struct {
    char        name[32];
    MYSQL_STMT *st;
    int         in_use;
    int         out_cols;
} dyn_stmt_t;

static dyn_stmt_t g_stmts[MAX_STMTS];

static dyn_stmt_t *find_stmt(const char *name) {
    int i;
    for (i = 0; i < MAX_STMTS; ++i)
        if (g_stmts[i].in_use && strcmp(g_stmts[i].name, name) == 0)
            return &g_stmts[i];
    return NULL;
}

static void put16(void *p, unsigned off, int16_t v) {
    memcpy((char *)p + off, &v, 2);
}
static int16_t get16(const void *p, unsigned off) {
    int16_t v; memcpy(&v, (const char *)p + off, 2); return v;
}

int esqlc_prepare(const char *name, const char *sql, size_t sql_len) {
    esqlc_state_t *s = esqlc_rt_state();
    dyn_stmt_t *d;
    int i;

    esqlc_rt_sqlsa_reset();
    if (esqlc_rt_ensure() != 0) return -1;
    if (!name || !sql) { esqlc_rt_set_err_code(-3004); return -1; }

    d = find_stmt(name);
    if (!d) {
        for (i = 0; i < MAX_STMTS; ++i)
            if (!g_stmts[i].in_use) { d = &g_stmts[i]; break; }
        if (!d) { esqlc_rt_set_err_code(-3001); return -1; }
        memset(d, 0, sizeof *d);
        strncpy(d->name, name, sizeof d->name - 1);
        d->in_use = 1;
    } else if (d->st) {
        mysql_stmt_close(d->st);
        d->st = NULL;
    }

    d->st = mysql_stmt_init(s->conn);
    if (!d->st) { esqlc_rt_set_err_from_mysql(s->conn); return -1; }
    /* The text is the program's, carried verbatim. This module never inspects
     * or rewrites it (NFR-001.1); `?` markers are the server's to parameterise
     * (FR-007.22). */
    if (mysql_stmt_prepare(d->st, sql, (unsigned long)sql_len) != 0) {
        esqlc_rt_set_err_from_stmt(d->st);
        mysql_stmt_close(d->st); d->st = NULL;
        return -1;
    }
    {
        MYSQL_RES *meta = mysql_stmt_result_metadata(d->st);
        d->out_cols = meta ? (int)mysql_num_fields(meta) : 0;
        if (meta) {
            /* FR-007.23 / FR-005.18 — the SQLSA's prepare arm. Gate 5 emitted
             * that arm as layout only; this is where it first carries data. */
            unsigned long names_len = 0;
            unsigned nf = mysql_num_fields(meta);
            MYSQL_FIELD *f = mysql_fetch_fields(meta);
            unsigned j;
            for (j = 0; j < nf; ++j)
                names_len += (unsigned long)strlen(f[j].name) + 2;  /* VARCHAR len */
            esqlc_rt_sqlsa_prepare_arm(d->out_cols, (long)names_len);
            mysql_free_result(meta);
        }
    }
    esqlc_rt_set_ok();
    return 0;
}

/* Table 10-4's numeric family. A type outside it is ESQLC-7012 rather than a
 * guess: character entries need the charset ID that lives in the sqlh file
 * this project does not have (002 Q7), which is why Gate 10 is numeric-only. */
static int describe_one(void *sv, MYSQL_FIELD *f) {
    int dt, bytes;
    switch (f->type) {
    case MYSQL_TYPE_SHORT:    dt = SQLDT_16BIT_S; bytes = 2; break;
    case MYSQL_TYPE_LONG:     dt = SQLDT_32BIT_S; bytes = 4; break;
    case MYSQL_TYPE_LONGLONG: dt = SQLDT_64BIT_S; bytes = 8; break;
    case MYSQL_TYPE_FLOAT:    dt = SQLDT_REAL;    bytes = 4; break;
    case MYSQL_TYPE_DOUBLE:   dt = SQLDT_DOUBLE;  bytes = 8; break;
    default:                  return -1;
    }
    if (f->flags & UNSIGNED_FLAG) {
        if (dt == SQLDT_16BIT_S) dt = SQLDT_16BIT_U;
        else if (dt == SQLDT_32BIT_S) dt = SQLDT_32BIT_U;
        /* Table 10-4 publishes no 64-bit unsigned, which is consistent with
         * FR-002.12 rejecting `unsigned long long` as a host variable. */
    }
    put16(sv, SQLVAR_OFF_DATA_TYPE, (int16_t)dt);
    /* SD-17: scale in the low byte, byte length in the high. Scale is 0 for
     * every binary numeric type. */
    put16(sv, SQLVAR_OFF_DATA_LEN, (int16_t)esqlc_datalen_pack(bytes, 0));
    /* SD-18: decimal digits, not bits — SQL/MP's own type table maps
     * NUMERIC(1..4) to 16 bits, a digit count driving a width. */
    put16(sv, SQLVAR_OFF_PRECISION, (int16_t)f->decimals ? 0 :
          (int16_t)(bytes == 2 ? 5 : bytes == 4 ? 10 : 19));
    /* FR-007.14: negative when the column permits nulls. */
    put16(sv, SQLVAR_OFF_NULL_INFO,
          (f->flags & NOT_NULL_FLAG) ? 0 : (int16_t)-1);
    return 0;
}

int esqlc_describe(const char *name, void *sqlda, int num_entries, int version,
                   char *names_buf, size_t names_len) {
    dyn_stmt_t *d;
    MYSQL_RES  *meta;
    MYSQL_FIELD *f;
    unsigned nf, j;
    size_t used = 0;

    (void)version;
    esqlc_rt_sqlsa_reset();
    if (esqlc_rt_ensure() != 0) return -1;
    if (!sqlda) { esqlc_rt_set_err_code(-3004); return -1; }

    d = find_stmt(name);
    if (!d || !d->st) { esqlc_rt_set_err_code(-7001); return -1; }

    /* ESQLC-7010 — a warning, not an error, and deliberately so: an
     * uninitialised eye-catcher is only reliably distinguishable in zeroed
     * memory. Garbage that happens to read "D1" is undetectable. */
    if (memcmp((char *)sqlda + SQLDA_OFF_EYE, "D1", 2) != 0)
        fprintf(stderr, "ESQLC-7010: SQLDA eye-catcher not initialised by the "
                        "program (§10 p.10-59 shows the program setting it)\n");

    meta = mysql_stmt_result_metadata(d->st);
    if (!meta) { esqlc_rt_set_err_code(-7012); return -1; }
    nf = mysql_num_fields(meta);
    f  = mysql_fetch_fields(meta);

    /* ESQLC-7002 — capacity, checked against what PREPARE reported. Too small
     * is detectable; a num_entries LARGER than the allocation is not, and that
     * asymmetry is inherent to a model where the program allocates. */
    if (num_entries < (int)nf) {
        mysql_free_result(meta);
        esqlc_rt_set_err_code(-7002);
        return -1;
    }

    for (j = 0; j < nf; ++j) {
        void *sv = (char *)sqlda + SQLDA_OFF_SQLVAR
                                 + (size_t)j * SQLDA_SQLVAR_STRIDE;
        if (describe_one(sv, &f[j]) != 0) {
            mysql_free_result(meta);
            esqlc_rt_set_err_code(-7012);   /* not a guessed charset ID */
            return -1;
        }
        if (names_buf) {
            /* VARCHAR-shaped: a 2-byte length then the characters. DIV-057:
             * the published 11-byte overhead budgets 8 bytes for a Guardian
             * table name, and MariaDB identifiers run to 64 — so the qualifier
             * is OMITTED rather than truncated. A truncated qualifier looks
             * valid and is wrong; an absent one is what a single-table query
             * wanted anyway, and visibly incomplete for a join. */
            size_t l = strlen(f[j].name);
            if (used + 2 + l > names_len) {
                mysql_free_result(meta);
                esqlc_rt_set_err_code(-7008);
                return -1;
            }
            {
                int16_t n16 = (int16_t)l;
                memcpy(names_buf + used, &n16, 2);
                memcpy(names_buf + used + 2, f[j].name, l);
                used += 2 + l;
            }
        }
    }
    mysql_free_result(meta);
    put16(sqlda, SQLDA_OFF_NUM_ENTRIES, (int16_t)num_entries);
    esqlc_rt_set_ok();
    return 0;
}

int esqlc_execute(const char *name, void *sqlda, int num_entries, int version) {
    dyn_stmt_t *d;
    MYSQL_BIND *rb = NULL;
    unsigned long *rl = NULL;
    my_bool *rnull = NULL;
    int rc = 0, j, n;

    (void)version;
    esqlc_rt_sqlsa_reset();
    if (esqlc_rt_ensure() != 0) return -1;

    d = find_stmt(name);
    if (!d || !d->st) { esqlc_rt_set_err_code(-7001); return -1; }
    n = d->out_cols < num_entries ? d->out_cols : num_entries;

    if (mysql_stmt_execute(d->st) != 0) {
        esqlc_rt_set_err_from_stmt(d->st);
        return -1;
    }
    if (n == 0) { esqlc_rt_set_ok(); return 0; }

    rb    = calloc((size_t)n, sizeof *rb);
    rl    = calloc((size_t)n, sizeof *rl);
    rnull = calloc((size_t)n, sizeof *rnull);
    if (!rb || !rl || !rnull) { esqlc_rt_set_err_code(-3001); rc = -1; goto done; }

    for (j = 0; j < n; ++j) {
        const char *sv = (char *)sqlda + SQLDA_OFF_SQLVAR
                                       + (size_t)j * SQLDA_SQLVAR_STRIDE;
        void *vp; int dt, packed, bytes;
        memcpy(&vp, sv + SQLVAR_OFF_VAR_PTR, sizeof vp);
        /* ESQLC-7003 — refused, not allocated for. FR-007.8 makes var_ptr the
         * program's, so filling it in here would take ownership the manual
         * gives away. */
        if (!vp) { esqlc_rt_set_err_code(-7003); rc = -1; goto done; }

        dt     = get16(sv, SQLVAR_OFF_DATA_TYPE);
        packed = get16(sv, SQLVAR_OFF_DATA_LEN);
        bytes  = esqlc_datalen_bytes(packed);
        /* ESQLC-7005, and an independent check on SD-17: under the wrong bit
         * order a scale-0 entry decodes as byte length 0 and lands here. */
        if (bytes != 2 && bytes != 4 && bytes != 8) {
            esqlc_rt_set_err_code(-7005); rc = -1; goto done;
        }
        switch (dt) {
        case SQLDT_16BIT_S: case SQLDT_16BIT_U:
            rb[j].buffer_type = MYSQL_TYPE_SHORT; break;
        case SQLDT_32BIT_S: case SQLDT_32BIT_U:
            rb[j].buffer_type = MYSQL_TYPE_LONG; break;
        case SQLDT_64BIT_S:
            rb[j].buffer_type = MYSQL_TYPE_LONGLONG; break;
        case SQLDT_REAL:   rb[j].buffer_type = MYSQL_TYPE_FLOAT;  break;
        case SQLDT_DOUBLE: rb[j].buffer_type = MYSQL_TYPE_DOUBLE; break;
        default: esqlc_rt_set_err_code(-7004); rc = -1; goto done;
        }
        rb[j].is_unsigned = (dt == SQLDT_16BIT_U || dt == SQLDT_32BIT_U);
        rb[j].buffer      = vp;
        rb[j].buffer_length = (unsigned long)bytes;
        rb[j].length      = &rl[j];
        rb[j].is_null     = &rnull[j];
    }

    if (mysql_stmt_bind_result(d->st, rb) != 0) {
        esqlc_rt_set_err_from_stmt(d->st); rc = -1; goto done;
    }
    {
        int fr = mysql_stmt_fetch(d->st);
        if (fr == MYSQL_NO_DATA) { esqlc_rt_set_notfound(); goto done; }
        if (fr != 0 && fr != MYSQL_DATA_TRUNCATED) {
            esqlc_rt_set_err_from_stmt(d->st); rc = -1; goto done;
        }
        /* FR-007.15 — the implementation writes the null flag on output,
         * through the ind_ptr the PROGRAM supplied. */
        for (j = 0; j < n; ++j) {
            const char *sv = (char *)sqlda + SQLDA_OFF_SQLVAR
                                           + (size_t)j * SQLDA_SQLVAR_STRIDE;
            void *ip;
            memcpy(&ip, sv + SQLVAR_OFF_IND_PTR, sizeof ip);
            if (ip) { int16_t v = rnull[j] ? -1 : 0; memcpy(ip, &v, 2); }
        }
        esqlc_rt_set_ok();
    }
done:
    free(rb); free(rl); free(rnull);
    return rc;
}

/* The table tests/harness/sqlda_layout_sync.sh reads. */
static const esqlc_sqlda_off_t kOff[] = {
    {"eye_catcher", SQLDA_OFF_EYE},
    {"num_entries", SQLDA_OFF_NUM_ENTRIES},
    {"sqlvar",      SQLDA_OFF_SQLVAR},
    {"data_type",   SQLVAR_OFF_DATA_TYPE},
    {"data_len",    SQLVAR_OFF_DATA_LEN},
    {"precision",   SQLVAR_OFF_PRECISION},
    {"null_info",   SQLVAR_OFF_NULL_INFO},
    {"var_ptr",     SQLVAR_OFF_VAR_PTR},
    {"ind_ptr",     SQLVAR_OFF_IND_PTR},
    {"cprl_ptr",    SQLVAR_OFF_CPRL_PTR},
    {"reserved",    SQLVAR_OFF_RESERVED},
    {"stride",      SQLDA_SQLVAR_STRIDE},
};

const esqlc_sqlda_off_t *esqlc_rt_sqlda_offsets(int *n) {
    *n = (int)(sizeof kOff / sizeof kOff[0]);
    return kOff;
}
