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
# every undefined symbol must be esqlc_-prefixed or libc (FR-003.1)
bad=$(nm -u "$TMP/out.o" | sed 's/^ *//' | awk '{print $NF}' \
      | sed 's/^_//' | grep -vE '^(esqlc_|memcpy|printf|fprintf|__|_)' || true)
if [ -n "$bad" ]; then echo "FAIL: non-esqlc undefined symbols:"; echo "$bad"; exit 1; fi
echo "ok   abi_isolation"
