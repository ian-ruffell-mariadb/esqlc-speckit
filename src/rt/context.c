/* T075  — env -> file -> compiled resolution; esqlc_context_origin (FR-003.4/.19)
 * T075a — an unresolvable connection reports via sqlcode, never aborts (FR-003.5)
 * T076  — credentials route through MariaDB's own option file only (FR-003.21)
 *
 * There is no connect statement in SQL/MP, so this module invents the context
 * a program never supplies. See specs/003 "Why there is nothing to map".
 */
#include "rt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Compile-time defaults — lowest precedence (FR-003.19). */
#ifndef ESQLC_DEFAULT_HOST
#define ESQLC_DEFAULT_HOST "127.0.0.1"
#endif
#ifndef ESQLC_DEFAULT_PORT
#define ESQLC_DEFAULT_PORT 3306
#endif
#ifndef ESQLC_DEFAULT_DB
#define ESQLC_DEFAULT_DB ""
#endif

static esqlc_state_t g_state;

esqlc_state_t *esqlc_rt_state(void) { return &g_state; }

static void set_str(char *dst, size_t cap, int *src_out,
                    const char *val, int src) {
    snprintf(dst, cap, "%s", val ? val : "");
    *src_out = src;
}

/* Minimal key=value reader. Sections are ignored; first match wins. */
static int file_lookup(const char *path, const char *key,
                       char *out, size_t cap) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[1024];
    int found = 0;
    size_t klen = strlen(key);
    while (fgets(line, sizeof line, f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') ++p;
        if (*p == '#' || *p == ';' || *p == '[' || *p == '\n') continue;
        if (strncmp(p, key, klen) != 0) continue;
        char *q = p + klen;
        while (*q == ' ' || *q == '\t') ++q;
        if (*q != '=') continue;
        ++q;
        while (*q == ' ' || *q == '\t') ++q;
        size_t n = strlen(q);
        while (n && (q[n - 1] == '\n' || q[n - 1] == '\r' || q[n - 1] == ' ')) q[--n] = 0;
        snprintf(out, cap, "%s", q);
        found = 1;
        break;
    }
    fclose(f);
    return found;
}

int esqlc_rt_resolve(void) {
    esqlc_state_t *s = esqlc_rt_state();

    /* FR-003.21: a password may never arrive through our own channels. It is
     * MariaDB's option file or nothing. Refusing loudly beats accepting it. */
    if (getenv("ESQLC_PASSWORD") || getenv("ESQLC_PASSWD")) {
        fprintf(stderr,
                "ESQLC-3009: credentials supplied through ESQLC_PASSWORD are "
                "refused; use a MariaDB option file (FR-003.21)\n");
        esqlc_rt_set_err_code(-3009);
        return -1;
    }

    const char *cfg = getenv("ESQLC_CONFIG_FILE");
    set_str(s->optfile, sizeof s->optfile, &s->optfile_src,
            cfg ? cfg : "", cfg ? ESQLC_SRC_ENVIRONMENT : ESQLC_SRC_UNSET);

    char buf[512];
    const char *e;

    /* host */
    if ((e = getenv("ESQLC_HOST")))
        set_str(s->host, sizeof s->host, &s->host_src, e, ESQLC_SRC_ENVIRONMENT);
    else if (cfg && file_lookup(cfg, "host", buf, sizeof buf))
        set_str(s->host, sizeof s->host, &s->host_src, buf, ESQLC_SRC_FILE);
    else
        set_str(s->host, sizeof s->host, &s->host_src, ESQLC_DEFAULT_HOST, ESQLC_SRC_COMPILED);

    /* user */
    if ((e = getenv("ESQLC_USER")))
        set_str(s->user, sizeof s->user, &s->user_src, e, ESQLC_SRC_ENVIRONMENT);
    else if (cfg && file_lookup(cfg, "user", buf, sizeof buf))
        set_str(s->user, sizeof s->user, &s->user_src, buf, ESQLC_SRC_FILE);
    else
        set_str(s->user, sizeof s->user, &s->user_src, "", ESQLC_SRC_COMPILED);

    /* database */
    if ((e = getenv("ESQLC_DATABASE")))
        set_str(s->db, sizeof s->db, &s->db_src, e, ESQLC_SRC_ENVIRONMENT);
    else if (cfg && file_lookup(cfg, "database", buf, sizeof buf))
        set_str(s->db, sizeof s->db, &s->db_src, buf, ESQLC_SRC_FILE);
    else
        set_str(s->db, sizeof s->db, &s->db_src, ESQLC_DEFAULT_DB, ESQLC_SRC_COMPILED);

    /* port */
    if ((e = getenv("ESQLC_PORT"))) { s->port = (unsigned)atoi(e); s->port_src = ESQLC_SRC_ENVIRONMENT; }
    else if (cfg && file_lookup(cfg, "port", buf, sizeof buf)) { s->port = (unsigned)atoi(buf); s->port_src = ESQLC_SRC_FILE; }
    else { s->port = ESQLC_DEFAULT_PORT; s->port_src = ESQLC_SRC_COMPILED; }

    return 0;
}

int esqlc_rt_connect(void) {
    esqlc_state_t *s = esqlc_rt_state();
    if (s->connected) return 0;
    if (s->init_attempted && !s->connected) return -1;
    s->init_attempted = true;

    if (esqlc_rt_resolve() != 0) return -1;

    s->conn = mysql_init(NULL);
    if (!s->conn) { esqlc_rt_set_err_code(-3001); return -1; }

    /* FR-003.21: let libmariadb read credentials from the option file itself.
     * The runtime never holds a password. */
    if (s->optfile[0])
        mysql_options(s->conn, MYSQL_READ_DEFAULT_FILE, s->optfile);
    mysql_options(s->conn, MYSQL_READ_DEFAULT_GROUP, "esqlc");

    if (!mysql_real_connect(s->conn, s->host,
                            s->user[0] ? s->user : NULL,
                            NULL,
                            s->db[0] ? s->db : NULL,
                            s->port, NULL, 0)) {
        /* FR-003.5: report and return; never abort the process. */
        fprintf(stderr, "ESQLC-3001: cannot resolve a connection: %s\n",
                mysql_error(s->conn));
        esqlc_rt_set_err_code(-3001);
        mysql_close(s->conn);
        s->conn = NULL;
        return -1;
    }
    /* Explicit transaction control only; COMMIT/ROLLBACK WORK must mean
     * something (DIV-010, FR-003.6). */
    mysql_autocommit(s->conn, 1);

    /* T255 / DIV-052 — SQL/MP fixed-length character columns are always
     * blank-padded, and §2 p.2-8 ties comparison behaviour to that. MariaDB
     * strips trailing blanks from CHAR on retrieval unless this mode is set.
     *
     * APPEND rather than assign: replacing sql_mode wholesale would clobber
     * every other mode the server or the deployment configured. */
    {
        static const char q[] =
            "SET SESSION sql_mode = CONCAT(@@sql_mode, ',PAD_CHAR_TO_FULL_LENGTH')";
        if (mysql_real_query(s->conn, q, (unsigned long)(sizeof q - 1)) != 0) {
            fprintf(stderr,
                    "ESQLC-3001: cannot set PAD_CHAR_TO_FULL_LENGTH: %s\n",
                    mysql_error(s->conn));
            esqlc_rt_set_err_code(-3001);
            mysql_close(s->conn);
            s->conn = NULL;
            return -1;
        }
    }

    s->connected = true;
    return 0;
}

int esqlc_context_ensure(void) { return esqlc_rt_ensure(); }

int esqlc_context_origin(const char *setting) {
    esqlc_state_t *s = esqlc_rt_state();
    if (!setting) return ESQLC_SRC_UNSET;
    if (!strcmp(setting, "host"))     return s->host_src;
    if (!strcmp(setting, "user"))     return s->user_src;
    if (!strcmp(setting, "database")) return s->db_src;
    if (!strcmp(setting, "port"))     return s->port_src;
    return ESQLC_SRC_UNSET;
}

/* DIV-002: name mapping. Gate 1 uses directly-mapped names, so the identity
 * path is all that is exercised; an unmapped Guardian/DEFINE form is refused
 * rather than passed through (FR-003.23). */
int esqlc_name_resolve(const char *name, char *out, size_t cap) {
    if (!name || !out) return -1;
    if (name[0] == '=' || name[0] == '\\' || name[0] == '$') {
        fprintf(stderr, "ESQLC-3007: no mapping for '%s'\n", name);
        esqlc_rt_set_err_code(-3007);
        return -1;
    }
    snprintf(out, cap, "%s", name);
    return 0;
}
