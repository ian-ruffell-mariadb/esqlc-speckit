#!/usr/bin/env bash
# T003 — golden-file runner. .sqlc -> .expected.c, whitespace-normalised.
set -uo pipefail
PP="${1:?usage: run_golden.sh <esqlcpp>}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
DIR="$ROOT/tests/conformance/gate-1"
fail=0; ran=0
# Normalise whitespace, and collapse any embedded source path to its basename
# so the comparison does not depend on the cwd the tool was invoked from.
norm() {
  sed -e 's#[A-Za-z0-9_./-]*/\([A-Za-z0-9_-]*\.sqlc\)#\1#g' \
      -e 's/[[:space:]]\+/ /g' -e 's/^ //' -e 's/ $//' -e '/^$/d' "$1"
}
for src in "$DIR"/*.sqlc; do
  [ -e "$src" ] || continue
  exp="${src%.sqlc}.expected.c"
  [ -e "$exp" ] || continue
  ran=$((ran+1))
  act="${src%.sqlc}.actual.c"
  if ! "$PP" "$src" -o "$act" 2>"${src%.sqlc}.actual.diag"; then
    echo "FAIL $(basename "$src"): preprocessing failed"
    cat "${src%.sqlc}.actual.diag"; fail=$((fail+1)); continue
  fi
  if diff -u <(norm "$exp") <(norm "$act") >/dev/null; then
    echo "ok   $(basename "$src")"
  else
    echo "FAIL $(basename "$src")"
    diff -u <(norm "$exp") <(norm "$act") | head -40
    fail=$((fail+1))
  fi
done
echo "golden: $ran run, $fail failed"
exit $(( fail > 0 ))
