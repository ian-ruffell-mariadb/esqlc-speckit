/* T479 — registration, validating len == SQLCA_LEN.
 * T480 — populate the registered storage after each statement.
 * T481 — getinfolist item copying, in item order at documented sizes.
 * T482 — item 22's sign inversion (SD-4).
 * T483 — the documented 8510-8517 misuse codes.
 *
 * DIV-041 makes the layout private, so the offsets below are this
 * implementation's, not the manual's. What is fixed is the 430-byte total and
 * the leading eye-catcher, because programs copy the area with SQLCA_LEN and
 * share it EXTERNAL. The runtime writes into the PROGRAM's storage rather than
 * keeping its own, so a copy is a real copy and not an empty husk.
 */
#include "rt.h"
#include <string.h>

#define SQLCA_TOTAL     430
#define SQLCA_CAPACITY    7     /* §9 p.9-12: up to seven codes per statement */

/* private layout */
#define OFF_EYE       0   /* char[2]  */
#define OFF_VERSION   2   /* int16    */
#define OFF_CAPACITY  4   /* int16    */
#define OFF_ACTUAL    6   /* int16    */
#define OFF_ROWS      8   /* int32    */
#define OFF_ENTRIES  12   /* 7 x { int32 code; int16 seq; int32 fs } = 70 */
#define ENTRY_STRIDE 10

static void *g_sqlca;          /* the program's storage, not ours */

static void put16(void *p, size_t off, short v) { memcpy((char *)p + off, &v, 2); }
static void put32(void *p, size_t off, int   v) { memcpy((char *)p + off, &v, 4); }
static short get16(const void *p, size_t off) { short v; memcpy(&v, (const char *)p + off, 2); return v; }
static int   get32(const void *p, size_t off) { int   v; memcpy(&v, (const char *)p + off, 4); return v; }

int esqlc_sqlca_register(void *sqlca, size_t len) {
    if (!sqlca) return 8510;                 /* required parameter missing */
    if (len != SQLCA_TOTAL) return 8512;     /* program and runtime disagree */
    g_sqlca = sqlca;
    memset(sqlca, 0, len);
    memcpy((char *)sqlca + OFF_EYE, "CA", 2);
    put16(sqlca, OFF_VERSION,  2);           /* FR-005.10: version 2 by default */
    put16(sqlca, OFF_CAPACITY, SQLCA_CAPACITY);
    put16(sqlca, OFF_ACTUAL,   0);
    return 0;
}

/* Called after every statement (T484). Refreshes the area in place. */
void esqlc_rt_sqlca_populate(long sqlcode, long fs_code, long rows) {
    void *p = g_sqlca;
    if (!p) return;                          /* no INCLUDE SQLCA in this program */
    put16(p, OFF_VERSION,  2);
    put16(p, OFF_CAPACITY, SQLCA_CAPACITY);
    put32(p, OFF_ROWS, (int)rows);
    if (sqlcode == 0 || sqlcode == 100) {
        /* Clear the entries, not just the count. Leaving stale bytes behind
           means a program reading item 22 after a SUCCESSFUL statement gets
           the previous statement's error number — current-looking data that is
           not current, which Constitution III forbids. Found by pursuing
           mutation T463, which could not discriminate while this bug stood. */
        put16(p, OFF_ACTUAL, 0);
        memset((char *)p + OFF_ENTRIES, 0, SQLCA_CAPACITY * ENTRY_STRIDE);
        return;
    }
    put16(p, OFF_ACTUAL, 1);                 /* MariaDB yields one at a time */
    size_t e = OFF_ENTRIES;
    put32(p, e + 0, (int)sqlcode);
    put16(p, e + 4, 1);                      /* sequence */
    put32(p, e + 6, (int)fs_code);
}

/* Documented item sizes (§5 pp.5-11..5-12). 0 means "not supported here". */
static int item_size(int item) {
    switch (item) {
    case 1: case 2: case 3: case 22: case 27: return 2;
    case 20:                                  return 4;
    default:                                  return 0;
    }
}

int esqlc_sqlca_getinfolist(const void *sqlca, int error_index,
                            const int *items, int n_items,
                            void *buf, size_t buf_len) {
    if (!sqlca || !items || !buf || n_items <= 0) return 8510;
    if (memcmp((const char *)sqlca + OFF_EYE, "CA", 2) != 0) return 8512;

    short actual = get16(sqlca, OFF_ACTUAL);
    if (error_index < 0 || (actual > 0 && error_index >= actual) ||
        (actual == 0 && error_index > 0))
        return 8515;                          /* error entry index out of range */

    /* Size the result before writing anything, so an undersized buffer is
       reported rather than partially filled. */
    size_t need = 0;
    for (int i = 0; i < n_items; ++i) {
        int sz = item_size(items[i]);
        if (sz == 0) return 8511;             /* invalid item code */
        need += (size_t)sz;
    }
    if (need > buf_len) return 8514;          /* insufficient buffer space */

    size_t e = OFF_ENTRIES + (size_t)error_index * ENTRY_STRIDE;
    char  *out = (char *)buf;
    for (int i = 0; i < n_items; ++i) {
        short v16; int v32;
        switch (items[i]) {
        case 1:  v16 = get16(sqlca, OFF_VERSION);  memcpy(out, &v16, 2); out += 2; break;
        case 2:  v16 = get16(sqlca, OFF_CAPACITY); memcpy(out, &v16, 2); out += 2; break;
        case 3:  v16 = get16(sqlca, OFF_ACTUAL);   memcpy(out, &v16, 2); out += 2; break;
        case 20: v32 = get32(sqlca, OFF_ROWS);     memcpy(out, &v32, 4); out += 4; break;
        case 22: {
            /* SD-4, narrowing 005 Q7: item 22 reports errors POSITIVE and
               warnings NEGATIVE — the inverse of sqlcode's convention. The
               manual states this plainly in the item table, so it is
               reproduced as published rather than silently normalised. */
            int raw = get32(sqlca, e + 0);
            /* The inversion is exactly a negation: sqlcode is negative for an
               error and positive for a warning, and item 22 is the reverse. */
            v16 = (short)(-raw);
            memcpy(out, &v16, 2); out += 2;
            break;
        }
        case 27: v16 = get16(sqlca, e + 4);        memcpy(out, &v16, 2); out += 2; break;
        default: return 8511;
        }
    }
    return 0;
}
