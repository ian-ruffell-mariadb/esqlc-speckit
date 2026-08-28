#!/usr/bin/env bash
# Every ESQLC-nnnn the code can emit must be registered in a spec or the
# divergence register. Two codes (1014, 2009) were invented during the Gate 1
# implementation and went unregistered until an ad-hoc audit found them — this
# makes the audit structural rather than something someone remembers to run.
set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
fail=0; n=0
for c in $(grep -rhoE 'ESQLC-[0-9]{4}' "$ROOT/src" | sort -u); do
  n=$((n+1))
  if ! grep -rql "$c" "$ROOT/specs" "$ROOT/docs" 2>/dev/null; then
    echo "FAIL $c is emitted by the code but registered in no spec"
    fail=$((fail+1))
  fi
done
echo "diag registry: $n codes emitted, $fail unregistered"
exit $(( fail > 0 ))
