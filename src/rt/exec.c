/* T079 — bind descriptors positionally; bind exactly `width` bytes verbatim,
 *        never strlen, never truncate at a null, never pad (FR-002.30/.31).
 * T080 — execute the placeholder statement. No SQL text manipulation anywhere
 *        in the runtime (FR-003.10, NFR-003.2).
 *
 * The body arrives with '?' already in place of every host-variable reference,
 * spliced by the preprocessor at spans its lexer recorded. This module never
 * inspects, rewrites, or concatenates SQL text.
 */
#include "rt.h"
#include <stdlib.h>
#include <string.h>

int esqlc_stmt_exec(const char *body, size_t body_len,
                    const esqlc_hostvar_t *vars, int var_count) {
    esqlc_state_t *s = esqlc_rt_state();
    if (esqlc_rt_ensure() != 0) return -1;
    if (!body) { esqlc_rt_set_err_code(-3004); return -1; }

    MYSQL_STMT *st = mysql_stmt_init(s->conn);
    if (!st) { esqlc_rt_set_err_from_mysql(s->conn); return -1; }

    if (mysql_stmt_prepare(st, body, (unsigned long)body_len) != 0) {
        esqlc_rt_set_err_from_stmt(st);
        mysql_stmt_close(st);
        return -1;
    }

    unsigned long expected = mysql_stmt_param_count(st);
    if (expected != (unsigned long)(var_count < 0 ? 0 : var_count)) {
        /* FR-003.10 relies on the placeholder count matching the descriptor
         * array exactly; a mismatch is a preprocessor defect, not user error. */
        esqlc_rt_set_err_code(-3005);
        mysql_stmt_close(st);
        return -1;
    }

    MYSQL_BIND  *bind = NULL;
    unsigned long *lens = NULL;
    if (var_count > 0) {
        bind = (MYSQL_BIND *)calloc((size_t)var_count, sizeof *bind);
        lens = (unsigned long *)calloc((size_t)var_count, sizeof *lens);
        if (!bind || !lens) {
            free(bind); free(lens);
            esqlc_rt_set_err_code(-3001);
            mysql_stmt_close(st);
            return -1;
        }
        for (int i = 0; i < var_count; ++i) {
            const esqlc_hostvar_t *v = &vars[i];
            bind[i].buffer   = v->addr;
            bind[i].is_null  = NULL;
            switch (v->type) {
            case ESQLC_T_CHAR_FIXED:
                /* THE load-bearing line of the whole slice.
                 *
                 * `width` is the column length, not strlen(addr) and not
                 * `capacity`. Exactly `width` bytes leave the program, verbatim
                 * — including a null byte if the program left one there, which
                 * is what SQL/MP does (§2 p.2-8). A strlen here would pass every
                 * other Gate 1 test and silently diverge on the commonest
                 * column type. See tests rt/underfilled_stores_null. */
                lens[i]              = v->width;
                bind[i].buffer_type  = MYSQL_TYPE_STRING;
                bind[i].buffer_length = v->width;
                bind[i].length       = &lens[i];
                break;
            case ESQLC_T_INT:
                bind[i].length = NULL;
                bind[i].is_unsigned = v->is_signed ? 0 : 1;
                switch (v->width) {
                case 2: bind[i].buffer_type = MYSQL_TYPE_SHORT;    break;
                case 4: bind[i].buffer_type = MYSQL_TYPE_LONG;     break;
                case 8: bind[i].buffer_type = MYSQL_TYPE_LONGLONG; break;
                default:
                    free(bind); free(lens);
                    esqlc_rt_set_err_code(-3005);
                    mysql_stmt_close(st);
                    return -1;
                }
                break;
            default:
                /* Out-of-slice type reached the runtime: refuse (Principle III). */
                free(bind); free(lens);
                esqlc_rt_set_err_code(-3004);
                mysql_stmt_close(st);
                return -1;
            }
        }
        if (mysql_stmt_bind_param(st, bind) != 0) {
            esqlc_rt_set_err_from_stmt(st);
            free(bind); free(lens);
            mysql_stmt_close(st);
            return -1;
        }
    }

    int rc = 0;
    if (mysql_stmt_execute(st) != 0) {
        esqlc_rt_set_err_from_stmt(st);
        rc = -1;
    } else {
        my_ulonglong affected = mysql_stmt_affected_rows(st);
        if (affected == 0) esqlc_rt_set_notfound();   /* FR-003.13 */
        else               esqlc_rt_set_ok();
    }

    free(bind);
    free(lens);
    mysql_stmt_close(st);
    return rc;
}
