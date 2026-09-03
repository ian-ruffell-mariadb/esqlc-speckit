/* T079 — bind inputs exactly `width` bytes verbatim (FR-002.30/.31).
 * T080 — execute the placeholder statement; no SQL text manipulation here.
 * T256 — result metadata; column count must equal the output-descriptor count.
 * T257 — bind outputs with buffer_length = width, never capacity.
 * T258 — fetch one row, and only when a row exists (FR-004.2).
 * T259 — write -1 / 0 to the indicator (FR-002.16).
 * T260 — a null column with no indicator is 8423, not a zeroed variable.
 * T261 — refuse a result column whose family disagrees with its descriptor.
 *
 * The body arrives with '?' already in place of every input reference and the
 * INTO clause already removed, both done by the preprocessor at spans its lexer
 * recorded. This module never inspects, rewrites, or concatenates SQL text.
 */
#include "rt.h"
#include <stdlib.h>
#include <string.h>

/* Type families, for the cross-family check of FR-002.22. */
static int is_numeric_field(enum enum_field_types t) {
    switch (t) {
    case MYSQL_TYPE_TINY:  case MYSQL_TYPE_SHORT:    case MYSQL_TYPE_INT24:
    case MYSQL_TYPE_LONG:  case MYSQL_TYPE_LONGLONG: case MYSQL_TYPE_FLOAT:
    case MYSQL_TYPE_DOUBLE: case MYSQL_TYPE_DECIMAL: case MYSQL_TYPE_NEWDECIMAL:
        return 1;
    default:
        return 0;
    }
}

/* A negative indicator means SQL NULL, and nothing is read from the buffer
 * (FR-002.16, FR-004.8). `is_null` had never been populated for an input bind
 * before Gate 6 — six gates of inputs and the field was always NULL — so the
 * failure mode this replaces is a runtime that stores the buffer contents and
 * only appears correct when the buffer happens to hold zero. */
static my_bool g_null_true = 1;

static int bind_input(MYSQL_BIND *b, unsigned long *len,
                      const esqlc_hostvar_t *v) {
    b->buffer  = v->addr;
    b->is_null = (v->ind_addr && *v->ind_addr < 0) ? &g_null_true : NULL;
    switch (v->type) {
    case ESQLC_T_CHAR_FIXED:
        /* THE load-bearing line of Gate 1. `width` is the column length, not
         * strlen(addr) and not `capacity`. Exactly `width` bytes leave the
         * program, verbatim, including a null byte the program left there. */
        *len              = v->width;
        b->buffer_type    = MYSQL_TYPE_STRING;
        b->buffer_length  = v->width;
        b->length         = len;
        return 0;
    case ESQLC_T_CHAR_VAR:
        /* T775 — a VARCHAR structure. `addr` is the structure itself, so `len`
         * is at offset 0 as a short and `val` at offset 2. FR-002.6 fixes that
         * order and those names, and the emitter asserts the offset per
         * variable, so this is the published mapping rather than a guess.
         *
         * The length sent is the program's `len`, clamped to the declared
         * capacity: padding is the program's business (FR-002.31), but a `len`
         * larger than `val` would read past the structure. */
        {
            short declared;
            memcpy(&declared, v->addr, sizeof declared);
            if (declared < 0) declared = 0;
            if ((unsigned)declared > v->capacity) declared = (short)v->capacity;
            *len             = (unsigned long)declared;
            b->buffer        = (char *)v->addr + 2;
            b->buffer_type   = MYSQL_TYPE_STRING;
            b->buffer_length = v->capacity;
            b->length        = len;
        }
        return 0;
    case ESQLC_T_FLOAT:
        /* T774 — one family, two widths, the same by-width dispatch the
         * integer family uses. */
        b->length = NULL;
        switch (v->width) {
        case 4: b->buffer_type = MYSQL_TYPE_FLOAT;  return 0;
        case 8: b->buffer_type = MYSQL_TYPE_DOUBLE; return 0;
        default: return -1;
        }
    case ESQLC_T_INT:
        b->length      = NULL;
        b->is_unsigned = v->is_signed ? 0 : 1;
        switch (v->width) {
        case 2: b->buffer_type = MYSQL_TYPE_SHORT;    return 0;
        case 4: b->buffer_type = MYSQL_TYPE_LONG;     return 0;
        case 8: b->buffer_type = MYSQL_TYPE_LONGLONG; return 0;
        default: return -1;
        }
    default:
        return -1;
    }
}

/* T676, T677 — the altered count, for records_used.
 *
 * DIV-053. sqlcode is about rows found and records_used about rows altered
 * (§4 p.4-13, §9 p.9-17), and for an UPDATE those differ. With
 * CLIENT_FOUND_ROWS, affected_rows gives matched; mysql_info gives
 * "Rows matched: N  Changed: M  Warnings: W" and M is what records_used wants.
 *
 * This parses a server *message*, not an API. The format is long-standing and
 * documented but is not a contract, so a parse failure must fall back to the
 * SENTINEL and never to zero — zero is a legitimate altered count, and a
 * broken parse reporting it would turn a missing measurement into an untrue
 * statistic. Returns -1 when nothing could be read. */
/* Split out as a pure function so the fallback is testable.
 *
 * Mutation testing found the sentinel branch unreachable from any fixture: for
 * a real UPDATE the server always emits a Changed: field, so `!p` never fires
 * and a mutant returning 0 there survived. A guard no test can reach is not a
 * guard. This takes the info string directly, so a crafted one exercises every
 * branch — tests/conformance/gate-1/rt/parse_changed.c. */
long esqlc_rt_parse_changed(const char *info, long matched) {
    const char *p;
    if (!info) return matched;      /* no info line: INSERT/DELETE, where the
                                     * two counts coincide */
    p = strstr(info, "Changed:");
    if (!p) return -1;              /* sentinel, never zero: zero is a
                                     * legitimate altered count, so returning
                                     * it for a failed parse would turn a
                                     * missing measurement into an untrue
                                     * statistic */
    p += 8;
    while (*p == ' ') ++p;
    if (*p < '0' || *p > '9') return -1;
    {
        long v = 0;
        while (*p >= '0' && *p <= '9') v = v * 10 + (*p++ - '0');
        return v;
    }
}

static long changed_rows(MYSQL *m, long matched) {
    return esqlc_rt_parse_changed(mysql_info(m), matched);
}

int esqlc_stmt_exec(const char *body, size_t body_len,
                    const esqlc_hostvar_t *vars, int var_count,
                    const char *table) {
    esqlc_state_t *s = esqlc_rt_state();
    /* FR-005.20: every statement resets the SQLSA, first, so a statement that
     * fails leaves sentinels rather than the previous statement's numbers. */
    esqlc_rt_sqlsa_reset();
    if (esqlc_rt_ensure() != 0) return -1;
    if (!body) { esqlc_rt_set_err_code(-3004); return -1; }
    if (var_count < 0) var_count = 0;

    /* Split by direction, preserving relative order. Input order must match
     * placeholder order; output order must match select-list order. */
    int n_in = 0, n_out = 0;
    for (int i = 0; i < var_count; ++i)
        (vars[i].direction == ESQLC_DIR_OUT) ? ++n_out : ++n_in;

    MYSQL_STMT *st = mysql_stmt_init(s->conn);
    if (!st) { esqlc_rt_set_err_from_mysql(s->conn); return -1; }
    if (mysql_stmt_prepare(st, body, (unsigned long)body_len) != 0) {
        esqlc_rt_set_err_from_stmt(st);
        mysql_stmt_close(st);
        return -1;
    }
    if (mysql_stmt_param_count(st) != (unsigned long)n_in) {
        esqlc_rt_set_err_code(-3005);
        mysql_stmt_close(st);
        return -1;
    }

    int rc = 0;
    MYSQL_BIND    *pb = NULL, *rb = NULL;
    unsigned long *pl = NULL, *rl = NULL;
    my_bool       *rnull = NULL;
    MYSQL_RES     *meta  = NULL;
    const esqlc_hostvar_t **outv = NULL;

    /* ---- inputs ------------------------------------------------------- */
    if (n_in > 0) {
        pb = calloc((size_t)n_in, sizeof *pb);
        pl = calloc((size_t)n_in, sizeof *pl);
        if (!pb || !pl) { esqlc_rt_set_err_code(-3001); rc = -1; goto done; }
        int j = 0;
        for (int i = 0; i < var_count; ++i) {
            if (vars[i].direction == ESQLC_DIR_OUT) continue;
            if (bind_input(&pb[j], &pl[j], &vars[i]) != 0) {
                esqlc_rt_set_err_code(-3005); rc = -1; goto done;
            }
            ++j;
        }
        if (mysql_stmt_bind_param(st, pb) != 0) {
            esqlc_rt_set_err_from_stmt(st); rc = -1; goto done;
        }
    }

    /* ---- outputs: metadata, family check, bind ------------------------ */
    if (n_out > 0) {
        meta = mysql_stmt_result_metadata(st);
        if (!meta) { esqlc_rt_set_err_from_stmt(st); rc = -1; goto done; }
        unsigned cols = mysql_num_fields(meta);
        if (cols != (unsigned)n_out) {
            /* A mismatch means the INTO list and the select list disagree —
             * a program error, not something to bind past. */
            esqlc_rt_set_err_code(-3005);
            rc = -1; goto done;
        }
        MYSQL_FIELD *fields = mysql_fetch_fields(meta);

        rb    = calloc((size_t)n_out, sizeof *rb);
        rl    = calloc((size_t)n_out, sizeof *rl);
        rnull = calloc((size_t)n_out, sizeof *rnull);
        outv  = calloc((size_t)n_out, sizeof *outv);
        if (!rb || !rl || !rnull || !outv) {
            esqlc_rt_set_err_code(-3001); rc = -1; goto done;
        }
        int j = 0;
        for (int i = 0; i < var_count; ++i) {
            if (vars[i].direction != ESQLC_DIR_OUT) continue;
            const esqlc_hostvar_t *v = &vars[i];
            outv[j] = v;

            /* FR-002.22: conversion happens within families, never between. */
            int col_numeric = is_numeric_field(fields[j].type);
            /* T774 — FR-002.22. float and double are numeric; a date-time
             * column is not, which is what lets FR-002.13 bind a TIMESTAMP
             * into a character host variable without tripping this check. */
            int hv_numeric  = (v->type == ESQLC_T_INT || v->type == ESQLC_T_FLOAT);
            if (col_numeric != hv_numeric) {
                esqlc_rt_set_err_code(-2004);   /* ESQLC-2004 */
                rc = -1; goto done;
            }

            /* T875 was planned here and is NOT implementable. Recorded
             * rather than removed silently.
             *
             * §10 p.10-11 says NonStop SQL/MP "checks the precision field to
             * ensure that the character-set ID matches the expected character
             * set of the parameter or column", and the plan assumed result
             * metadata could supply the column's set. Measured, it cannot:
             * MYSQL_FIELD.charsetnr reports the RESULT SET's charset, not the
             * column's own. For `select v_kr, c_l2` — a euckr column and a
             * latin2 column — both come back identical:
             *
             *     default client charset   both charsetnr 224  (utf8mb4)
             *     binary  client charset   both charsetnr 63   (binary)
             *
             * So the information was never there, before or after this slice.
             * Recovering it needs a per-statement information_schema.COLUMNS
             * query, which is the round trip the table-name landmark was
             * designed to avoid.
             *
             * It also belongs elsewhere: §10 p.10-11 is about the SQLDA's
             * precision field, which is dynamic SQL — feature 007 — and the
             * preprocessor cannot know a column's charset either without the
             * schema access NFR-001.2 forbids. Feature 006's INVOKE reads the
             * schema and is where this check can live.
             *
             * FR-002.22 is unaffected: it asks for the character/numeric FAMILY
             * check above, which works and is tested. DIV-055 records the gap.
             */

            rb[j].buffer  = v->addr;
            rb[j].is_null = &rnull[j];
            switch (v->type) {
            case ESQLC_T_CHAR_FIXED:
                rb[j].buffer_type   = MYSQL_TYPE_STRING;
                /* buffer_length is `width`, never `capacity`: the library must
                 * not be able to reach the terminator placeholder byte, so a
                 * helpful null terminator cannot land there (FR-002.28). */
                rb[j].buffer_length = v->width;
                rb[j].length        = &rl[j];
                break;
            case ESQLC_T_CHAR_VAR:
                /* T776 — retrieve into `val`; the retrieved length is written
                 * back into `len` after the fetch. The program's prior `len`
                 * is not consulted: it is an output, not a capacity limit. */
                rb[j].buffer        = (char *)v->addr + 2;
                rb[j].buffer_type   = MYSQL_TYPE_STRING;
                rb[j].buffer_length = v->capacity;
                rb[j].length        = &rl[j];
                break;
            case ESQLC_T_FLOAT:
                switch (v->width) {
                case 4: rb[j].buffer_type = MYSQL_TYPE_FLOAT;  break;
                case 8: rb[j].buffer_type = MYSQL_TYPE_DOUBLE; break;
                default: esqlc_rt_set_err_code(-3005); rc = -1; goto done;
                }
                break;
            case ESQLC_T_INT:
                rb[j].is_unsigned = v->is_signed ? 0 : 1;
                rb[j].length      = NULL;
                switch (v->width) {
                case 2: rb[j].buffer_type = MYSQL_TYPE_SHORT;    break;
                case 4: rb[j].buffer_type = MYSQL_TYPE_LONG;     break;
                case 8: rb[j].buffer_type = MYSQL_TYPE_LONGLONG; break;
                default: esqlc_rt_set_err_code(-3005); rc = -1; goto done;
                }
                break;
            default:
                esqlc_rt_set_err_code(-3004); rc = -1; goto done;
            }
            ++j;
        }
        if (mysql_stmt_bind_result(st, rb) != 0) {
            esqlc_rt_set_err_from_stmt(st); rc = -1; goto done;
        }
    }

    /* ---- execute ------------------------------------------------------ */
    if (mysql_stmt_execute(st) != 0) {
        esqlc_rt_set_err_from_stmt(st);
        rc = -1; goto done;
    }

    if (n_out == 0) {
        /* Under CLIENT_FOUND_ROWS this is rows MATCHED, which is the basis
         * sqlcode 100 needs: "No rows were found on a search condition". */
        my_ulonglong aff = mysql_stmt_affected_rows(st);
        long matched = (long)(aff == (my_ulonglong)-1 ? 0 : aff);
        /* records_used wants rows ALTERED, a different number for an UPDATE. */
        long altered = changed_rows(s->conn, matched);
        /* SD-9: the table comes from the scanner landmark, not from metadata —
         * DML has none — and NULL means the sentinel, never a guess. */
        esqlc_rt_sqlsa_from_table(table, altered);
        if (matched == 0) esqlc_rt_set_notfound();
        else              esqlc_rt_set_ok();
        goto done;
    }

    /* ---- fetch exactly one row, and only if there is one -------------- */
    {
        int fr = mysql_stmt_fetch(st);
        if (fr == MYSQL_NO_DATA) {
            /* FR-004.2: nothing has been written to any host variable, because
             * only fetch writes and this fetch found nothing. */
            esqlc_rt_set_notfound();
            goto done;
        }
        if (fr != 0 && fr != MYSQL_DATA_TRUNCATED) {
            esqlc_rt_set_err_from_stmt(st);
            rc = -1; goto done;
        }
        for (int j = 0; j < n_out; ++j) {
            if (rnull[j]) {
                if (outv[j]->ind_addr) {
                    *outv[j]->ind_addr = -1;        /* FR-002.16 */
                } else {
                    /* SQL error 8423: a null output with no indicator. Report
                     * it rather than leaving a zeroed variable behind, and do
                     * not remap it to 100 (FR-005.2). */
                    esqlc_rt_set_err_code(-8423);   /* ESQLC-4009, SQL error 8423 */
                    rc = -1; goto done;
                }
            } else if (outv[j]->ind_addr) {
                *outv[j]->ind_addr = 0;
            }
            /* T776 — a retrieved VARCHAR's length. Written whether or not the
             * program supplied an indicator, and only when the value is not
             * null: a null leaves `len` alone, because there is no length. */
            if (!rnull[j] && outv[j]->type == ESQLC_T_CHAR_VAR) {
                unsigned long got = rl[j];
                short n;
                if (got > outv[j]->capacity) got = outv[j]->capacity;
                n = (short)got;
                memcpy(outv[j]->addr, &n, sizeof n);
            }
        }
        esqlc_rt_sqlsa_from_stmt(st, 1);   /* one row retrieved */
        esqlc_rt_set_ok();
    }

done:
    if (meta) mysql_free_result(meta);
    free(pb); free(pl); free(rb); free(rl); free(rnull);
    free((void *)outv);
    mysql_stmt_close(st);
    return rc;
}
