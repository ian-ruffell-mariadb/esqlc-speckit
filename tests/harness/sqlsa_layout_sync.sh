#!/usr/bin/env bash
# T523 / T536 — the emitter and the runtime must agree on the SQLSA layout.
#
# The preprocessor emits a real C struct that programs index by name. The
# runtime cannot include preprocessor output, so it writes the same structure
# by offset. That is two independent encodings of one published layout, which
# is exactly the drift Principle VI exists to prevent: add a field to one side
# only and the runtime silently corrupts the program's own memory.
#
# This compiles a probe that holds both — the emitted struct, and the runtime's
# offset table — and compares them field by field, for both version families.
#
# Skips (77) when the runtime library is absent, matching the Tier 2 idiom.
set -uo pipefail

PP="${1:?usage: sqlsa_layout_sync.sh <esqlcpp> [libesqlc.a]}"
LIB="${2:-}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
FIX="$ROOT/tests/conformance/gate-1"
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT

if [ -z "$LIB" ] || [ ! -f "$LIB" ]; then
  echo "SKIP: runtime library not built"; exit 77
fi

# The emitter brackets its SQLSA declarations so this harness can lift them out
# verbatim rather than re-deriving them. Marker text is asserted below: if the
# emitter stops emitting it, this fails loudly instead of silently checking
# nothing — the failure mode that has bitten this project four times.
BEGIN='/* --8<-- esqlc sqlsa layout begin --8<-- */'
END='/* --8<-- esqlc sqlsa layout end --8<-- */'

"$PP" "$FIX/sqlsa_sizes.sqlc" > "$TMP/emitted.c" 2>"$TMP/diag" || {
  echo "FAIL: preprocessing sqlsa_sizes.sqlc failed"; cat "$TMP/diag"; exit 1; }

awk -v b="$BEGIN" -v e="$END" '
  index($0,b){f=1; next} index($0,e){f=0} f' "$TMP/emitted.c" > "$TMP/layout.h"

if [ ! -s "$TMP/layout.h" ]; then
  echo "FAIL: emitter produced no bracketed SQLSA layout block."
  echo "      Expected markers around the declarations:"
  echo "        $BEGIN"
  echo "        $END"
  exit 1
fi

cat > "$TMP/probe.c" <<'PROBE'
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "layout.h"
#include "rt_sqlsa_offsets.h"

struct pair { const char *field; size_t emitted; };

int main(void) {
    int bad = 0, checked = 0;

    const struct pair v300[] = {
        {"eye_catcher",     offsetof(struct SQLSA_TYPE, eye_catcher)},
        {"version",         offsetof(struct SQLSA_TYPE, version)},
        {"dml",             offsetof(struct SQLSA_TYPE, u.dml)},
        {"prepare",         offsetof(struct SQLSA_TYPE, u.prepare)},
        {"num_tables",      offsetof(struct SQLSA_TYPE, u.dml.num_tables)},
        {"stats0",          offsetof(struct SQLSA_TYPE, u.dml.stats[0])},
        {"stats1",          offsetof(struct SQLSA_TYPE, u.dml.stats[1])},
        {"table_name",      offsetof(struct SQLSA_TYPE, u.dml.stats[0].table_name)},
        {"records_accessed",offsetof(struct SQLSA_TYPE, u.dml.stats[0].records_accessed)},
        {"records_used",    offsetof(struct SQLSA_TYPE, u.dml.stats[0].records_used)},
        {"disc_reads",      offsetof(struct SQLSA_TYPE, u.dml.stats[0].disc_reads)},
        {"messages",        offsetof(struct SQLSA_TYPE, u.dml.stats[0].messages)},
        {"message_bytes",   offsetof(struct SQLSA_TYPE, u.dml.stats[0].message_bytes)},
        {"waits",           offsetof(struct SQLSA_TYPE, u.dml.stats[0].waits)},
        {"escalations",     offsetof(struct SQLSA_TYPE, u.dml.stats[0].escalations)},
        {"total",           sizeof(struct SQLSA_TYPE)},
        {"stride",          sizeof(struct STATS_TYPE)},
    };
    const struct pair v330[] = {
        {"eye_catcher",     offsetof(struct SQLSA_TYPE_R330, eye_catcher)},
        {"version",         offsetof(struct SQLSA_TYPE_R330, version)},
        {"dml",             offsetof(struct SQLSA_TYPE_R330, u.dml)},
        {"prepare",         offsetof(struct SQLSA_TYPE_R330, u.prepare)},
        {"num_tables",      offsetof(struct SQLSA_TYPE_R330, u.dml.num_tables)},
        {"stats0",          offsetof(struct SQLSA_TYPE_R330, u.dml.stats[0])},
        {"stats1",          offsetof(struct SQLSA_TYPE_R330, u.dml.stats[1])},
        {"table_name",      offsetof(struct SQLSA_TYPE_R330, u.dml.stats[0].table_name)},
        {"records_accessed",offsetof(struct SQLSA_TYPE_R330, u.dml.stats[0].records_accessed)},
        {"records_used",    offsetof(struct SQLSA_TYPE_R330, u.dml.stats[0].records_used)},
        {"disc_reads",      offsetof(struct SQLSA_TYPE_R330, u.dml.stats[0].disc_reads)},
        {"messages",        offsetof(struct SQLSA_TYPE_R330, u.dml.stats[0].messages)},
        {"message_bytes",   offsetof(struct SQLSA_TYPE_R330, u.dml.stats[0].message_bytes)},
        {"waits",           offsetof(struct SQLSA_TYPE_R330, u.dml.stats[0].waits)},
        {"escalations",     offsetof(struct SQLSA_TYPE_R330, u.dml.stats[0].escalations)},
        {"vsbb_write",      offsetof(struct SQLSA_TYPE_R330, u.dml.stats[0].vsbb_write)},
        {"vsbb_flushed",    offsetof(struct SQLSA_TYPE_R330, u.dml.stats[0].vsbb_flushed)},
        {"total",           sizeof(struct SQLSA_TYPE_R330)},
        {"stride",          sizeof(struct STATS_TYPE_R330)},
    };

    struct { int version; const struct pair *p; int n; } fams[] = {
        {300, v300, (int)(sizeof v300 / sizeof v300[0])},
        {330, v330, (int)(sizeof v330 / sizeof v330[0])},
    };

    for (int f = 0; f < 2; f++) {
        int n = 0;
        const esqlc_sqlsa_off_t *rt = esqlc_rt_sqlsa_offsets(fams[f].version, &n);
        if (!rt || n == 0) {
            printf("FAIL v%d: runtime exposes no offset table\n", fams[f].version);
            bad = 1; continue;
        }
        for (int i = 0; i < fams[f].n; i++) {
            const char *name = fams[f].p[i].field;
            size_t want = fams[f].p[i].emitted;
            int found = 0;
            for (int j = 0; j < n; j++) {
                if (strcmp(rt[j].field, name) == 0) {
                    found = 1; checked++;
                    if (rt[j].off != want) {
                        printf("FAIL v%d %s: emitted %zu, runtime %u\n",
                               fams[f].version, name, want, rt[j].off);
                        bad = 1;
                    }
                    break;
                }
            }
            if (!found) {
                printf("FAIL v%d %s: emitted at %zu, runtime has no entry\n",
                       fams[f].version, name, want);
                bad = 1;
            }
        }
        /* And the reverse: a runtime entry the emitter does not produce. */
        for (int j = 0; j < n; j++) {
            int found = 0;
            for (int i = 0; i < fams[f].n; i++)
                if (strcmp(rt[j].field, fams[f].p[i].field) == 0) { found = 1; break; }
            if (!found) {
                printf("FAIL v%d %s: runtime-only field, not in the emitted struct\n",
                       fams[f].version, rt[j].field);
                bad = 1;
            }
        }
    }

    if (!bad) printf("ok   sqlsa_layout_sync: %d offsets agree across both versions\n", checked);
    return bad;
}
PROBE

cp "$ROOT/src/rt/rt_sqlsa_offsets.h" "$TMP/" 2>/dev/null || {
  echo "FAIL: src/rt/rt_sqlsa_offsets.h missing — the runtime exposes no offset table"
  exit 1; }

CC="${CC:-cc}"
if ! "$CC" -std=c11 -I"$TMP" -I"$ROOT/include" -o "$TMP/probe" "$TMP/probe.c" "$LIB" \
      $(command -v mariadb_config >/dev/null && mariadb_config --libs || echo "") \
      2>"$TMP/cc.log"; then
  echo "FAIL: probe did not compile"; sed -n '1,25p' "$TMP/cc.log"; exit 1
fi

"$TMP/probe"
