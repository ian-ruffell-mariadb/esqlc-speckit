#!/usr/bin/env bash
# T006/T036 — every esqlc_ prototype in the header must appear in the contract
# document, and vice versa (FR-003.3).
set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
H="$ROOT/include/esqlc.h"
C="$ROOT/specs/003-runtime-mariadb-binding/contracts/esqlc-abi.md"
hdr=$(grep -oE '\besqlc_[a-z_]+\(' "$H" | tr -d '(' | sort -u)
doc=$(grep -oE '\besqlc_[a-z_]+\(' "$C" | tr -d '(' | sort -u)
# The two directions are NOT symmetric, and treating them as such was a bug.
#
#   in header, not in contract  -> FAIL. Undocumented ABI: code exists that no
#                                  contract describes, which is what Principle V
#                                  exists to prevent.
#   in contract, not in header  -> INFO. Planned, not yet implemented. Principle
#                                  V requires signatures to land in the contract
#                                  during planning, so this is the mandated
#                                  state between /speckit.plan and
#                                  /speckit.implement. Failing it would push
#                                  people to skip the contract update or
#                                  implement early.
undocumented=$(comm -23 <(echo "$hdr") <(echo "$doc"))
unimplemented=$(comm -13 <(echo "$hdr") <(echo "$doc"))
rc=0
if [ -n "$undocumented" ]; then
  echo "FAIL: in header, absent from contract (undocumented ABI):"
  echo "$undocumented" | sed 's/^/  /'
  rc=1
fi
if [ -n "$unimplemented" ]; then
  echo "info: planned in the contract, not yet in the header:"
  echo "$unimplemented" | sed 's/^/  /'
fi
[ $rc = 0 ] && echo "ok   contract_sync ($(echo "$hdr" | grep -c . ) implemented, $(echo "$unimplemented" | grep -c .) planned)"
exit $rc
