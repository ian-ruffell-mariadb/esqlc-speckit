#!/usr/bin/env bash
# T1034 / T1047 — the emitter and the runtime must agree on the SQLDA layout.
#
# The descriptor is the program's storage, so the runtime addresses it by
# offset while the emitter declares it as a struct: two independent encodings
# of one published layout. Exactly the drift sqlsa_layout_sync.sh exists for,
# and this is the same shape.
#
# It also pins DIV-058. `sqlvar` sits at 8 rather than the published
# SQLDA_HEADER_LEN of 4, because DIV-040's widening raises SQLVAR_TYPE's
# alignment from 4 to 8. If either side ever "corrects" that to 4, this fails.
set -uo pipefail
PP="${1:?usage: sqlda_layout_sync.sh <esqlcpp> [libesqlc.a]}"
LIB="${2:-}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
FIX="$ROOT/tests/conformance/gate-1"
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
[ -n "$LIB" ] && [ -f "$LIB" ] || { echo "SKIP: runtime library not built"; exit 77; }

BEGIN='/* --8<-- esqlc sqlda layout begin --8<-- */'
END='/* --8<-- esqlc sqlda layout end --8<-- */'
"$PP" "$FIX/sqlda_layout.sqlc" --schema "$FIX/schema.cache" > "$TMP/emitted.c" \
  2>"$TMP/diag" || { echo "FAIL: preprocessing failed"; cat "$TMP/diag"; exit 1; }
awk -v b="$BEGIN" -v e="$END" 'index($0,b){f=1;next} index($0,e){f=0} f' \
  "$TMP/emitted.c" > "$TMP/layout.h"
[ -s "$TMP/layout.h" ] || { echo "FAIL: no bracketed sqlda layout block"; exit 1; }
cp "$ROOT/src/rt/rt_sqlda_offsets.h" "$TMP/" || exit 1

cat > "$TMP/probe.c" <<'PROBE'
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "layout.h"
#include "rt_sqlda_offsets.h"
struct pair { const char *field; size_t emitted; };
int main(void) {
    int bad = 0, n = 0, i, j, checked = 0;
    const struct pair want[] = {
        {"eye_catcher", offsetof(struct SQLDA_TYPE, eye_catcher)},
        {"num_entries", offsetof(struct SQLDA_TYPE, num_entries)},
        {"sqlvar",      offsetof(struct SQLDA_TYPE, sqlvar)},
        {"data_type",   offsetof(struct SQLVAR_TYPE, data_type)},
        {"data_len",    offsetof(struct SQLVAR_TYPE, data_len)},
        {"precision",   offsetof(struct SQLVAR_TYPE, precision)},
        {"null_info",   offsetof(struct SQLVAR_TYPE, null_info)},
        {"var_ptr",     offsetof(struct SQLVAR_TYPE, var_ptr)},
        {"ind_ptr",     offsetof(struct SQLVAR_TYPE, ind_ptr)},
        {"cprl_ptr",    offsetof(struct SQLVAR_TYPE, cprl_ptr)},
        {"reserved",    offsetof(struct SQLVAR_TYPE, reserved)},
        {"stride",      sizeof(struct SQLVAR_TYPE)},
    };
    const esqlc_sqlda_off_t *rt = esqlc_rt_sqlda_offsets(&n);
    const int nwant = (int)(sizeof want / sizeof want[0]);
    for (i = 0; i < nwant; i++) {
        int found = 0;
        for (j = 0; j < n; j++)
            if (strcmp(rt[j].field, want[i].field) == 0) {
                found = 1; checked++;
                if (rt[j].off != want[i].emitted) {
                    printf("FAIL %s: emitted %zu, runtime %u\n",
                           want[i].field, want[i].emitted, rt[j].off);
                    bad = 1;
                }
                break;
            }
        if (!found) {
            printf("FAIL %s: emitted at %zu, runtime has no entry\n",
                   want[i].field, want[i].emitted);
            bad = 1;
        }
    }
    for (j = 0; j < n; j++) {
        int found = 0;
        for (i = 0; i < nwant; i++)
            if (strcmp(rt[j].field, want[i].field) == 0) { found = 1; break; }
        if (!found) {
            printf("FAIL %s: runtime-only field\n", rt[j].field);
            bad = 1;
        }
    }
    /* DIV-058, pinned: sqlvar is at 8, not at the published header length. */
    if (offsetof(struct SQLDA_TYPE, sqlvar) != 8) {
        printf("FAIL sqlvar is at %zu, not 8 — DIV-058 says the widening moves "
               "it off SQLDA_HEADER_LEN\n", offsetof(struct SQLDA_TYPE, sqlvar));
        bad = 1;
    }
    if (!bad) printf("ok   sqlda_layout_sync: %d offsets agree; sqlvar at 8 "
                     "(DIV-058), stride 40 (DIV-040)\n", checked);
    return bad;
}
PROBE
CC="${CC:-cc}"
"$CC" -std=c11 -I"$TMP" -I"$ROOT/include" -o "$TMP/probe" "$TMP/probe.c" "$LIB" \
  $(command -v mariadb_config >/dev/null && mariadb_config --libs || echo "") \
  2>"$TMP/cc.log" || { echo "FAIL: probe did not compile"; sed -n '1,20p' "$TMP/cc.log"; exit 1; }
"$TMP/probe"
