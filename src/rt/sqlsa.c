/* T570-T574 — the SQLSA runtime.
 *
 * Registration rather than runtime-held state, for the reason §9 p.9-13 makes
 * explicit: the manual tells programs to save values immediately after a
 * statement, and to declare more than one SQLSA where needed. Both idioms
 * require the data to live in the program's own storage, so the runtime writes
 * there rather than answering accessor calls.
 *
 * Reset and "undefined" are one mechanism. FR-005.20 wants every statement to
 * reset the structure; FR-005.19 wants it not left accidentally meaningful
 * after a statement class that leaves it undefined. Stamping every field with
 * its sentinel at the start of each statement and then filling only what that
 * statement can honestly supply satisfies both — a statement that supplies
 * nothing leaves sentinels, which is exactly the required behaviour, with no
 * second code path to get wrong.
 */
#include "rt.h"
#include "rt_sqlsa_offsets.h"
#include <string.h>

static void       *g_sqlsa;
static size_t      g_len;
static int         g_version;

/* Little-endian writers at an explicit offset. The layout is packed, so a
 * field may sit at an odd address and a cast-and-assign would be a misaligned
 * store — undefined behaviour, and a real fault on some targets. */
static void put16(void *p, unsigned off, int16_t v) {
    memcpy((char *)p + off, &v, 2);
}
static void put32(void *p, unsigned off, int32_t v) {
    memcpy((char *)p + off, &v, 4);
}
static void put64(void *p, unsigned off, int64_t v) {
    memcpy((char *)p + off, &v, 8);
}
static int16_t get16(const void *p, unsigned off) {
    int16_t v; memcpy(&v, (const char *)p + off, 2); return v;
}

int esqlc_sqlsa_register(void *sqlsa, size_t len, int version) {
    unsigned want;
    if (!sqlsa) return 1;
    if (version == 300) want = SQLSA_V300_LEN;
    else if (version == 330) want = SQLSA_V330_LEN;
    else return 1;                     /* only the two published families */
    /* A mismatch means the program and the runtime disagree about the
     * structure. That is an error, not something to truncate into. */
    if (len != want) return 1;

    g_sqlsa   = sqlsa;
    g_len     = len;
    g_version = version;

    memcpy((char *)sqlsa + SQLSA_OFF_EYE, "SA", 2);
    put16(sqlsa, SQLSA_OFF_VERSION, (int16_t)version);
    esqlc_rt_sqlsa_reset();
    return 0;
}

/* One stats[] entry, wholly sentinel. */
static void stamp_entry(char *e, int v330) {
    memset(e + (v330 ? SQLSA_V330_S_TABLE_NAME : SQLSA_V300_S_TABLE_NAME),
           SQLSA_CHAR_SENTINEL, 24);                       /* SD-8 */
    if (v330) {
        put64(e, SQLSA_V330_S_RECS_ACCESSED, -1);
        put64(e, SQLSA_V330_S_RECS_USED,     -1);
        put64(e, SQLSA_V330_S_DISC_READS,    -1);
        put64(e, SQLSA_V330_S_MESSAGES,      -1);
        put64(e, SQLSA_V330_S_MSG_BYTES,     -1);
        put32(e, SQLSA_V330_S_WAITS,         -1);
        put32(e, SQLSA_V330_S_ESCALATIONS,   -1);
        /* The VSBB flags are booleans, not counts. There is no VSBB here, so
         * they read FALSE rather than a sentinel: -1 already means true. */
        put16(e, SQLSA_V330_S_VSBB_WRITE,   SQLSA_VSBB_FALSE);
        put16(e, SQLSA_V330_S_VSBB_FLUSHED, SQLSA_VSBB_FALSE);
    } else {
        put32(e, SQLSA_V300_S_RECS_ACCESSED, -1);
        put32(e, SQLSA_V300_S_RECS_USED,     -1);
        put32(e, SQLSA_V300_S_DISC_READS,    -1);
        put32(e, SQLSA_V300_S_MESSAGES,      -1);
        put32(e, SQLSA_V300_S_MSG_BYTES,     -1);
        put16(e, SQLSA_V300_S_WAITS,         -1);
        put16(e, SQLSA_V300_S_ESCALATIONS,   -1);
        memset(e + SQLSA_V300_S_RESERVED, 0, 4);
    }
}

/* T572 — FR-005.20 and FR-005.19. Called at the start of every statement, and
 * left alone by statement classes that leave the area undefined. */
void esqlc_rt_sqlsa_reset(void) {
    int v330;
    unsigned stats_off, stride;
    char *p = g_sqlsa;
    int i;

    if (!p) return;                     /* no INCLUDE SQLSA in this program */
    v330      = (g_version == 330);
    stats_off = v330 ? SQLSA_V330_OFF_STATS : SQLSA_V300_OFF_STATS;
    stride    = v330 ? SQLSA_V330_STRIDE    : SQLSA_V300_STRIDE;

    /* num_tables is a count, so -1 is out of domain and means "not measured".
     * This is what makes an undefined SQLSA detectable rather than plausible. */
    put16(p, v330 ? SQLSA_V330_OFF_NUM_TABLES : SQLSA_V300_OFF_NUM_TABLES, -1);
    if (v330) {
        put64(p, 6,  -1);               /* master_executor_elapsed_time */
        put64(p, 14, -1);               /* total_esp_cpu_time */
        put64(p, 22, -1);               /* total_sortprog_cpu_time */
        memset(p + 30, 0, 32);          /* filler */
    }
    for (i = 0; i < SQLSA_MAX_TABLES; i++)
        stamp_entry(p + stats_off + (unsigned)i * stride, v330);
}

/* T574 — populate what this statement can honestly supply.
 *
 * `tables` and `n_tables` come from result-set metadata, never from parsing the
 * statement: NFR-001.1 makes statement bodies opaque to the preprocessor, and
 * parsing them here would undo the decision that has kept it tractable. DML
 * returns no metadata, so n_tables is 0 there and table_name stays at its
 * character sentinel.
 *
 * DIV-011: records_used is the only counter with an honest analogue. Everything
 * else in stats[] measures NonStop process and disk-process structure that has
 * no counterpart, so it keeps the sentinel stamped above.
 */
void esqlc_rt_sqlsa_populate(long rows_used,
                             const char *const *tables, int n_tables) {
    int v330, i, n;
    unsigned stats_off, stride;
    char *p = g_sqlsa;

    if (!p) return;
    v330      = (g_version == 330);
    stats_off = v330 ? SQLSA_V330_OFF_STATS : SQLSA_V300_OFF_STATS;
    stride    = v330 ? SQLSA_V330_STRIDE    : SQLSA_V300_STRIDE;

    n = n_tables;
    if (n < 0) n = 0;
    if (n > SQLSA_MAX_TABLES) n = SQLSA_MAX_TABLES;   /* FR-005.22 */

    /* A statement that ran touched at least one table even when metadata does
     * not say which, so the count is honest where the name is not. */
    put16(p, v330 ? SQLSA_V330_OFF_NUM_TABLES : SQLSA_V300_OFF_NUM_TABLES,
          (int16_t)(n > 0 ? n : 1));

    for (i = 0; i < (n > 0 ? n : 1); i++) {
        char *e = p + stats_off + (unsigned)i * stride;
        if (i < n && tables && tables[i]) {
            size_t l = strlen(tables[i]);
            if (l > 24) l = 24;
            memset(e, ' ', 24);                 /* blank-padded, as published */
            memcpy(e, tables[i], l);
        }
        /* rows_used is attributed to the statement, so it lands on entry 0.
         * Splitting it across joined tables would be an invention. */
        if (i == 0) {
            if (v330) put64(e, SQLSA_V330_S_RECS_USED, (int64_t)rows_used);
            else      put32(e, SQLSA_V300_S_RECS_USED, (int32_t)rows_used);
        }
    }
}


/* T574 — table names from result-set metadata.
 *
 * Never from parsing the statement: NFR-001.1 makes statement bodies opaque to
 * the preprocessor, and parsing them here would undo the one decision that has
 * kept it tractable across five gates. `org_table` is the underlying table
 * rather than an alias, which is what the manual's table_name means.
 *
 * DML returns no result-set metadata, so n_tables is 0 there and table_name
 * keeps the character sentinel SD-8 stamped. That gap is real and named in the
 * slice's non-proof list.
 */
void esqlc_rt_sqlsa_from_stmt(MYSQL_STMT *st, long rows_used) {
    const char *names[SQLSA_MAX_TABLES];
    int n = 0;
    MYSQL_RES *meta;

    if (!g_sqlsa) return;
    meta = st ? mysql_stmt_result_metadata(st) : NULL;
    if (meta) {
        unsigned nf = mysql_num_fields(meta);
        MYSQL_FIELD *f = mysql_fetch_fields(meta);
        for (unsigned i = 0; i < nf && n < SQLSA_MAX_TABLES; i++) {
            const char *t = f[i].org_table && f[i].org_table[0]
                          ? f[i].org_table : f[i].table;
            int dup = 0, j;
            if (!t || !t[0]) continue;
            /* One entry per distinct table, not one per column. */
            for (j = 0; j < n; j++)
                if (strcmp(names[j], t) == 0) { dup = 1; break; }
            if (!dup) names[n++] = t;
        }
    }
    esqlc_rt_sqlsa_populate(rows_used, names, n);
    if (meta) mysql_free_result(meta);
}

/* T679 — populate from an explicit table name (SD-9's landmark) rather than
 * from result-set metadata. DML has no metadata, which is why Gate 5 left
 * table_name at the character sentinel for the whole DML path; the scanner
 * landmark is the source it was missing. NULL still means the sentinel. */
void esqlc_rt_sqlsa_from_table(const char *table, long rows_used) {
    const char *names[1];
    if (!g_sqlsa) return;
    if (table && table[0]) {
        names[0] = table;
        esqlc_rt_sqlsa_populate(rows_used, names, 1);
    } else {
        esqlc_rt_sqlsa_populate(rows_used, NULL, 0);
    }
}

int esqlc_rt_sqlsa_version(void) { return g_sqlsa ? g_version : 0; }

/* The table tests/harness/sqlsa_layout_sync.sh reads. Every entry here must
 * have a counterpart in the emitted struct and vice versa; the harness checks
 * both directions, so a field added on one side only fails loudly. */
static const esqlc_sqlsa_off_t kOff300[] = {
    {"eye_catcher",      SQLSA_OFF_EYE},
    {"version",          SQLSA_OFF_VERSION},
    {"dml",              SQLSA_OFF_UNION},
    {"prepare",          SQLSA_OFF_UNION},
    {"num_tables",       SQLSA_V300_OFF_NUM_TABLES},
    {"stats0",           SQLSA_V300_OFF_STATS},
    {"stats1",           SQLSA_V300_OFF_STATS + SQLSA_V300_STRIDE},
    {"table_name",       SQLSA_V300_OFF_STATS + SQLSA_V300_S_TABLE_NAME},
    {"records_accessed", SQLSA_V300_OFF_STATS + SQLSA_V300_S_RECS_ACCESSED},
    {"records_used",     SQLSA_V300_OFF_STATS + SQLSA_V300_S_RECS_USED},
    {"disc_reads",       SQLSA_V300_OFF_STATS + SQLSA_V300_S_DISC_READS},
    {"messages",         SQLSA_V300_OFF_STATS + SQLSA_V300_S_MESSAGES},
    {"message_bytes",    SQLSA_V300_OFF_STATS + SQLSA_V300_S_MSG_BYTES},
    {"waits",            SQLSA_V300_OFF_STATS + SQLSA_V300_S_WAITS},
    {"escalations",      SQLSA_V300_OFF_STATS + SQLSA_V300_S_ESCALATIONS},
    {"total",            SQLSA_V300_LEN},
    {"stride",           SQLSA_V300_STRIDE},
};

static const esqlc_sqlsa_off_t kOff330[] = {
    {"eye_catcher",      SQLSA_OFF_EYE},
    {"version",          SQLSA_OFF_VERSION},
    {"dml",              SQLSA_OFF_UNION},
    {"prepare",          SQLSA_OFF_UNION},
    {"num_tables",       SQLSA_V330_OFF_NUM_TABLES},
    {"stats0",           SQLSA_V330_OFF_STATS},
    {"stats1",           SQLSA_V330_OFF_STATS + SQLSA_V330_STRIDE},
    {"table_name",       SQLSA_V330_OFF_STATS + SQLSA_V330_S_TABLE_NAME},
    {"records_accessed", SQLSA_V330_OFF_STATS + SQLSA_V330_S_RECS_ACCESSED},
    {"records_used",     SQLSA_V330_OFF_STATS + SQLSA_V330_S_RECS_USED},
    {"disc_reads",       SQLSA_V330_OFF_STATS + SQLSA_V330_S_DISC_READS},
    {"messages",         SQLSA_V330_OFF_STATS + SQLSA_V330_S_MESSAGES},
    {"message_bytes",    SQLSA_V330_OFF_STATS + SQLSA_V330_S_MSG_BYTES},
    {"waits",            SQLSA_V330_OFF_STATS + SQLSA_V330_S_WAITS},
    {"escalations",      SQLSA_V330_OFF_STATS + SQLSA_V330_S_ESCALATIONS},
    {"vsbb_write",       SQLSA_V330_OFF_STATS + SQLSA_V330_S_VSBB_WRITE},
    {"vsbb_flushed",     SQLSA_V330_OFF_STATS + SQLSA_V330_S_VSBB_FLUSHED},
    {"total",            SQLSA_V330_LEN},
    {"stride",           SQLSA_V330_STRIDE},
};

const esqlc_sqlsa_off_t *esqlc_rt_sqlsa_offsets(int version, int *n) {
    if (version == 300) {
        *n = (int)(sizeof kOff300 / sizeof kOff300[0]);
        return kOff300;
    }
    if (version == 330) {
        *n = (int)(sizeof kOff330 / sizeof kOff330[0]);
        return kOff330;
    }
    *n = 0;
    return NULL;
}

/* Unused-warning silencer for the one reader we keep for debugging. */
int esqlc_rt_sqlsa_num_tables(void) {
    if (!g_sqlsa) return 0;
    return get16(g_sqlsa, g_version == 330 ? SQLSA_V330_OFF_NUM_TABLES
                                           : SQLSA_V300_OFF_NUM_TABLES);
}
