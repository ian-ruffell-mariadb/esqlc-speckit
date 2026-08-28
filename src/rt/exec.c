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

static int bind_input(MYSQL_BIND *b, unsigned long *len,
                      const esqlc_hostvar_t *v) {
    b->buffer  = v->addr;
    b->is_null = NULL;
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

int esqlc_stmt_exec(const char *body, size_t body_len,
                    const esqlc_hostvar_t *vars, int var_count) {
    esqlc_state_t *s = esqlc_rt_state();
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
            int hv_numeric  = (v->type == ESQLC_T_INT);
            if (col_numeric != hv_numeric) {
                esqlc_rt_set_err_code(-2004);   /* ESQLC-2004 */
                rc = -1; goto done;
            }

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
        if (mysql_stmt_affected_rows(st) == 0) esqlc_rt_set_notfound();
        else                                   esqlc_rt_set_ok();
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
        }
        esqlc_rt_set_ok();
    }

done:
    if (meta) mysql_free_result(meta);
    free(pb); free(pl); free(rb); free(rl); free(rnull);
    free((void *)outv);
    mysql_stmt_close(st);
    return rc;
}
