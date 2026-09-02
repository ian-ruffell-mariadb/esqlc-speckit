#!/usr/bin/env bash
# Tier 2 — live-database conformance. Fixes the second process debt: these
# checks were previously run by hand and would have rotted.
#
# Skips with ctest's SKIP_RETURN_CODE (77) when no server is reachable, so
# NFR-001.2 still holds: the Tier 1 suite must run on a machine with no MariaDB.
#
# Configuration comes from the same ESQLC_* variables the runtime resolves, so
# the harness cannot accidentally test a different database than the code sees.
#   ESQLC_HOST ESQLC_PORT ESQLC_USER ESQLC_DATABASE
set -uo pipefail

PP="${1:?usage: run_tier2.sh <esqlcpp> <libesqlc.a>}"
LIB="${2:?usage: run_tier2.sh <esqlcpp> <libesqlc.a>}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
FIX="$ROOT/tests/conformance/gate-1"
RT="$FIX/rt"

: "${ESQLC_HOST:=127.0.0.1}"
: "${ESQLC_PORT:=3306}"
: "${ESQLC_USER:=root}"
: "${ESQLC_DATABASE:=esqlc_gate1}"
export ESQLC_HOST ESQLC_PORT ESQLC_USER ESQLC_DATABASE

MYSQL_BIN="$(command -v mariadb || command -v mysql || true)"
MDBCFG="$(command -v mariadb_config || command -v mysql_config || true)"
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT

if [ -z "$MYSQL_BIN" ] || [ -z "$MDBCFG" ]; then
  echo "SKIP: no MariaDB client tooling found"; exit 77
fi
mdb() { "$MYSQL_BIN" --protocol=tcp -h"$ESQLC_HOST" -P"$ESQLC_PORT" -u"$ESQLC_USER" \
        "$ESQLC_DATABASE" "$@" 2>/dev/null | grep -v '^WARNING' || true; }
if ! "$MYSQL_BIN" --protocol=tcp -h"$ESQLC_HOST" -P"$ESQLC_PORT" -u"$ESQLC_USER" \
     -e "select 1" >/dev/null 2>&1; then
  echo "SKIP: no server reachable at $ESQLC_HOST:$ESQLC_PORT"; exit 77
fi
"$MYSQL_BIN" --protocol=tcp -h"$ESQLC_HOST" -P"$ESQLC_PORT" -u"$ESQLC_USER" \
  -e "create database if not exists \`$ESQLC_DATABASE\`" >/dev/null 2>&1
mdb < "$FIX/schema.sql" >/dev/null

pass=0; fail=0
ok()   { echo "ok   $1"; pass=$((pass+1)); }
bad()  { echo "FAIL $1: $2"; fail=$((fail+1)); }

# Build a .sqlc fixture (preprocess, compile, link) or a plain .c fixture.
build() {  # build <src> <out>
  local src="$1" out="$2" cfile="$TMP/$(basename "${1%.*}").c"
  if [ "${src##*.}" = "sqlc" ]; then
    "$PP" "$src" -o "$cfile" || return 1
  else
    cfile="$src"
  fi
  cc -std=c11 -I"$ROOT/include" "$cfile" "$LIB" $("$MDBCFG" --libs) \
     -o "$out" 2>"$TMP/cc.err" || { cat "$TMP/cc.err"; return 1; }
}

run_case() {  # run_case <name> <src> <expected-exit>
  local name="$1" src="$2" want="${3:-0}"
  if ! build "$src" "$TMP/$name"; then bad "$name" "build failed"; return 1; fi
  "$TMP/$name" >"$TMP/$name.out" 2>&1
  local rc=$?
  if [ "$rc" != "$want" ]; then
    bad "$name" "exit $rc, expected $want"; sed 's/^/    /' "$TMP/$name.out"; return 1
  fi
  return 0
}

# --- T044/T046 commit path, verbatim bytes -------------------------------
mdb -e "delete from parts" >/dev/null
if run_case txn_commit "$FIX/insert.sqlc" 0; then
  got=$(mdb -N -e "set session sql_mode='PAD_CHAR_TO_FULL_LENGTH';
                   select concat(length(part_desc),':',hex(part_desc)) from parts where part_num=4102")
  want="18:484558204E55542C20384D4D202020202020"
  [ "$got" = "$want" ] && ok "txn_commit + char_verbatim (FR-003.6, FR-002.30)" \
                       || bad "txn_commit + char_verbatim" "got [$got] want [$want]"
fi

# --- T045 rollback leaves no trace ---------------------------------------
mdb -e "delete from parts" >/dev/null
if run_case txn_rollback "$RT/txn_rollback.sqlc" 0; then
  n=$(mdb -N -e "select count(*) from parts where part_num=555")
  [ "$n" = "0" ] && ok "txn_rollback (FR-003.6, FR-003.8)" \
                 || bad "txn_rollback" "$n rows survived a rollback"
fi

# --- T047 the one a strlen-based bind fails ------------------------------
mdb -e "delete from parts" >/dev/null
if run_case underfilled "$RT/underfilled_stores_null.sqlc" 0; then
  hx=$(mdb -N -e "set session sql_mode='PAD_CHAR_TO_FULL_LENGTH';
                  select hex(part_desc) from parts where part_num=7")
  want="414243004445464748494A4B4C4D4E4F5051"
  [ "$hx" = "$want" ] && ok "underfilled_stores_null (FR-002.31)" \
                      || bad "underfilled_stores_null" "got [$hx] want [$want]"
fi

# --- T048 value stored, never executed -----------------------------------
mdb -e "delete from parts" >/dev/null
if run_case injection "$RT/injection_literal.sqlc" 0; then
  t=$(mdb -N -e "show tables like 'parts'")
  v=$(mdb -N -e "set session sql_mode='PAD_CHAR_TO_FULL_LENGTH';
                 select part_desc from parts where part_num=9")
  if [ "$t" = "parts" ] && [ "${v:0:18}" = "'; DROP TABLE part" ]; then
    ok "injection_literal (FR-003.10, NFR-003.2)"
  else
    bad "injection_literal" "table=[$t] value=[$v]"
  fi
fi

# --- T049/T050 sqlcode classes -------------------------------------------
run_case sqlcode_error "$RT/sqlcode_error.sqlc" 0 && \
  ok "sqlcode_error negative on duplicate key (FR-003.13)"

# --- T040 runtime drivable without the preprocessor ----------------------
mdb -e "delete from parts" >/dev/null
if run_case direct_abi "$RT/direct_abi.c" 0; then
  n=$(mdb -N -e "select count(*) from parts where part_num=4242")
  [ "$n" = "1" ] && ok "direct_abi (NFR-003.1)" || bad "direct_abi" "row not committed"
fi

# --- T052 second thread refused ------------------------------------------
run_case second_thread "$RT/negative/second_thread.c" 0 && \
  ok "second_thread refused (FR-003.17)"

# --- T051 unresolvable connection reports, does not abort ----------------
if build "$RT/negative/bad_config.c" "$TMP/bad_config"; then
  ( ESQLC_PORT=1 ESQLC_HOST=127.0.0.1 "$TMP/bad_config" >"$TMP/bad.out" 2>&1 )
  rc=$?
  [ "$rc" = "0" ] && ok "bad_config reports via sqlcode (FR-003.5)" \
                  || { bad "bad_config" "exit $rc"; sed 's/^/    /' "$TMP/bad.out"; }
fi

# =====================================================================
# Gate 2 — retrieval. Each case re-seeds, because the Gate 1 cases above
# delete rows. Seeded rows: 4102 has a weight, 4103 has a null weight.
# =====================================================================
seed() { mdb < "$FIX/seed.sql" >/dev/null; }

# --- T230/T231 happy path, and the terminator byte must survive ----------
seed
if run_case select_into "$RT/select_into.sqlc" 0; then
  got=$(cat "$TMP/select_into.out")
  want="HEX NUT, 8MM      |42|0|AA"
  [ "$got" = "$want" ] && ok "select_into + no_terminator (FR-003.12, FR-004.1, FR-002.28)" \
                       || bad "select_into" "got [$got] want [$want]"
fi

# --- T235 DIV-052: trailing blanks must survive retrieval ----------------
# The expected value above is 18 bytes ending in six blanks; if MariaDB
# stripped them the comparison fails, so criterion 3 is covered by the same
# assertion rather than by trusting that sql_mode was set.

# --- T232 not found: sqlcode 100 and nothing written ---------------------
seed
run_case not_found "$RT/not_found.sqlc" 0 && \
  ok "not_found_untouched (FR-004.2, FR-003.13, FR-005.1)"

# --- T233 null column with an indicator ----------------------------------
seed
run_case null_indicator "$RT/null_indicator.sqlc" 0 && \
  ok "null_indicator = -1 (FR-002.16)"

# --- T234 null column with NO indicator ----------------------------------
seed
run_case null_no_ind "$RT/negative/null_no_indicator.sqlc" 0 && \
  ok "null_no_indicator yields 8423 (FR-005.2)"

# --- T237 cross-family refused -------------------------------------------
seed
run_case cross_family "$RT/negative/cross_family.sqlc" 0 && \
  ok "cross_family refused (FR-002.22)"

# =====================================================================
# Gate 3 — read-only cursors. Each case re-seeds; the seed gives 4102
# through 4106 so a cursor has a range to walk.
# =====================================================================
seed
run_case cursor_loop "$RT/cursor_loop.sqlc" 0 && \
  ok "cursor_loop: 3 rows in ORDER BY order, terminator intact (FR-004.13, FR-004.16, FR-002.28)"

seed
run_case fetch_exhausted "$RT/fetch_exhausted.sqlc" 0 && \
  ok "fetch_exhausted: 100 twice, host variables untouched (FR-004.14, SD-3)"

seed
run_case open_binds "$RT/open_binds_at_open.sqlc" 0 && \
  ok "open_binds_at_open: OPEN reads the current value (FR-004.12)"

seed
run_case close_reopen "$RT/close_then_reopen.sqlc" 0 && \
  ok "close_then_reopen: full set both times (FR-004.15)"

seed
run_case commit_frees "$RT/commit_frees_cursor.sqlc" 0 && \
  ok "commit_frees_cursor: fetch after commit errors (FR-003.8)"

seed
run_case cursor_order "$RT/negative/cursor_order.sqlc" 0 && \
  ok "cursor_order: all three out-of-order operations refused (FR-004.19)"

# =====================================================================
# Gate 4 — WHENEVER and the SQLCA.
# =====================================================================
seed
run_case whenever_flow "$RT/whenever_flow.sqlc" 0 && \
  ok "whenever_flow: handler on failure, GOTO on 100, CONTINUE disables (FR-005.1)"

seed
run_case sqlca_items "$RT/sqlca_items.sqlc" 0 && \
  ok "sqlca_items: numeric items readable (FR-005.14a, FR-005.30)"

seed
run_case sqlca_copy "$RT/sqlca_copy_survives.sqlc" 0 && \
  ok "sqlca_copy_survives: a copied SQLCA still reads (FR-005.14a)"

seed
run_case sqlca_seven "$RT/sqlca_seven_codes.sqlc" 0 && \
  ok "sqlca_seven_codes: capacity 7, all present codes retrievable (FR-005.15)"

seed
run_case sqlca_fscode "$RT/sqlca_fscode.sqlc" 0 && \
  ok "sqlca_fscode: file-system detail returned (FR-005.31)"

seed
run_case sqlca_misuse "$RT/negative/sqlca_misuse.sqlc" 0 && \
  ok "sqlca_misuse: 8511 / 8514 / 8515 (FR-005.30)"

# --- Gate 5 (T542-T547) SQLSA ---------------------------------------------
mdb < "$FIX/seed.sql" >/dev/null

run_case sqlsa_cursor_stats "$RT/sqlsa_cursor_stats.sqlc" 0 \
  && ok "sqlsa_cursor_stats (FR-005.17)"

# The accumulator idiom of §9 p.9-13. A missing per-statement reset makes the
# accumulated total overshoot the row count, so this is the one shape that
# catches a stale SQLSA instead of reading a plausible number.
run_case sqlsa_accumulate "$RT/sqlsa_accumulate.sqlc" 0 \
  && ok "sqlsa_accumulate — reset per FETCH (FR-005.20)"

run_case sqlsa_two_tables "$RT/sqlsa_two_tables.sqlc" 0 \
  && ok "sqlsa_two_tables — num_tables 2, stats[1] (FR-005.22)"

run_case sqlsa_sentinels "$RT/sqlsa_sentinels.sqlc" 0 \
  && ok "sqlsa_sentinels — never zero, SD-7 (FR-005.25)"

run_case sqlsa_sentinel_char "$RT/sqlsa_sentinel_char.sqlc" 0 \
  && ok "sqlsa_sentinel_char — SD-8 (FR-005.25)"

run_case sqlsa_after_commit "$RT/sqlsa_after_commit.sqlc" 0 \
  && ok "sqlsa_after_commit — undefined stays undefined (FR-005.19)"

# --- Gate 6 (T640-T650) searched UPDATE and DELETE -----------------------
mdb < "$FIX/seed.sql" >/dev/null

run_case update_rows "$RT/update_rows.sqlc" 0 \
  && ok "update_rows (FR-004.7)"

run_case update_zero_rows "$RT/update_zero_rows.sqlc" 0 \
  && ok "update_zero_rows — sqlcode 100, transport ok (FR-004.10)"

# The fixture the gate turns on. sqlcode is about rows FOUND (p.4-13),
# records_used about rows ALTERED (p.9-17), and a matched-but-unchanged UPDATE
# is where SQL/MP's two counts disagree by design. DIV-053.
run_case update_matched_unchanged "$RT/update_matched_unchanged.sqlc" 0 \
  && ok "update_matched_unchanged — found but not altered (DIV-053)"

run_case update_set_null "$RT/update_set_null.sqlc" 0 \
  && ok "update_set_null — input indicator (FR-004.8, FR-002.16)"

run_case delete_rows "$RT/delete_rows.sqlc" 0 \
  && ok "delete_rows — multi-row, records_used (FR-004.9)"

run_case delete_zero_rows "$RT/delete_zero_rows.sqlc" 0 \
  && ok "delete_zero_rows — sqlcode 100 (FR-004.10)"

run_case dml_sqlsa_stats "$RT/dml_sqlsa_stats.sqlc" 0 \
  && ok "dml_sqlsa_stats (FR-005.17)"

run_case dml_table_name "$RT/dml_table_name.sqlc" 0 \
  && ok "dml_table_name — landmark, SD-9 (FR-005.22)"

# T614 — FR-002.30 on the input side of an UPDATE, checked byte-exact.
mdb < "$FIX/seed.sql" >/dev/null
if run_case update_char_verbatim "$RT/update_char_verbatim.sqlc" 0; then
  hx=$(mdb -N -e "set session sql_mode='PAD_CHAR_TO_FULL_LENGTH';
                  select hex(part_desc) from parts where part_num=4102")
  want="414243004445464748494A4B4C4D4E4F5051"
  [ "$hx" = "$want" ] && ok "update_char_verbatim (FR-002.30)" \
                      || bad "update_char_verbatim" "got [$hx] want [$want]"
fi

# T622 — NFR-003.2 on the write path: stored, never executed.
mdb < "$FIX/seed.sql" >/dev/null
if run_case update_injection_literal "$RT/update_injection_literal.sqlc" 0; then
  t=$(mdb -N -e "show tables like 'parts'")
  [ "$t" = "parts" ] && ok "update_injection_literal (NFR-003.2)" \
                     || bad "update_injection_literal" "the table is gone"
fi

echo "tier2: $pass passed, $fail failed"
exit $(( fail > 0 ))
