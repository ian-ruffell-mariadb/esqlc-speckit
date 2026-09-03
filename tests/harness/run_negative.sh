#!/usr/bin/env bash
# T004 — negative runner. Asserts code, line AND column (NFR-001.3).
set -uo pipefail
PP="${1:?usage: run_negative.sh <esqlcpp>}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
DIR="$ROOT/tests/conformance/gate-1/negative"
fail=0; ran=0
# Gate 9: see run_golden.sh.
SCHEMA="$ROOT/tests/conformance/gate-1/schema.cache"

for src in "$DIR"/*.sqlc; do
  [ -e "$src" ] || continue
  exp="${src%.sqlc}.expected.diag"
  [ -e "$exp" ] || continue
  ran=$((ran+1))
  out="$("$PP" "$src" --schema "$SCHEMA" -o /dev/null 2>&1 >/dev/null)"
  # expected file holds lines of the form  CODE:line:col
  ok=1
  while IFS= read -r want; do
    [ -z "$want" ] && continue
    code="${want%%:*}"; rest="${want#*:}"; line="${rest%%:*}"; col="${rest##*:}"
    if ! printf '%s\n' "$out" | grep -q ":${line}:${col}: error: ${code}:"; then
      ok=0
      echo "  missing: ${code} at ${line}:${col}"
    fi
  done < "$exp"
  if [ "$ok" = 1 ]; then echo "ok   $(basename "$src")"
  else echo "FAIL $(basename "$src")"; printf '%s\n' "$out" | sed 's/^/    got: /'; fail=$((fail+1)); fi
done
echo "negative: $ran run, $fail failed"
exit $(( fail > 0 ))
