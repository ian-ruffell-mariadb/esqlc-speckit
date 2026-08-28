#!/usr/bin/env bash
# T005/T037 — emitted C must compile with only include/esqlc.h reachable and
# NO MariaDB header on the include path (FR-003.2, Constitution V).
set -uo pipefail
PP="${1:?usage: abi_isolation.sh <esqlcpp>}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
src="$ROOT/tests/conformance/gate-1/insert.sqlc"
"$PP" "$src" -o "$TMP/out.c" || { echo "FAIL: preprocessing failed"; exit 1; }
if grep -qiE "mysql|mariadb" "$TMP/out.c"; then
  echo "FAIL: emitted C references MariaDB"; exit 1
fi
if ! cc -std=c11 -c "$TMP/out.c" -o "$TMP/out.o" -I"$ROOT/include" 2>"$TMP/err"; then
  echo "FAIL: emitted C does not compile against the ABI header alone"
  cat "$TMP/err"; exit 1
fi
# No undefined symbol may come from MariaDB (Constitution V, FR-003.2).
#
# This deliberately does NOT try to allowlist "acceptable" symbols. The first
# version did, and it was tuned to macOS: glibc exposes `stderr` as a plain
# undefined symbol where macOS mangles it to `___stderrp`, so the allowlist
# passed locally and failed on Linux for a symbol the gate fixture is entitled
# to use. The user's own C brings libc with it; that was never this test's
# business.
#
# What IS this test's business is that nothing MariaDB-shaped crossed the ABI.
# The complementary check — that every call the *preprocessor emitted* is
# esqlc_-prefixed — lives in spec_assertions.py, at source level, where it is
# both portable and precise.
syms=$(nm -u "$TMP/out.o" 2>/dev/null | sed 's/^ *//' | awk '{print $NF}' | sed 's/^_*//')
bad=$(printf '%s\n' "$syms" | grep -iE '^(mysql|mariadb|my_)' || true)
if [ -n "$bad" ]; then
  echo "FAIL: emitted object depends on MariaDB symbols:"; echo "$bad"; exit 1
fi
echo "ok   abi_isolation ($(printf '%s\n' "$syms" | grep -c '^esqlc_') esqlc_ symbols, 0 MariaDB)"
