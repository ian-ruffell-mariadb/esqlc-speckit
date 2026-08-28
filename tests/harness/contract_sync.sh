#!/usr/bin/env bash
# T006/T036 — every esqlc_ prototype in the header must appear in the contract
# document, and vice versa (FR-003.3).
set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
H="$ROOT/include/esqlc.h"
C="$ROOT/specs/003-runtime-mariadb-binding/contracts/esqlc-abi.md"
hdr=$(grep -oE '\besqlc_[a-z_]+\(' "$H" | tr -d '(' | sort -u)
doc=$(grep -oE '\besqlc_[a-z_]+\(' "$C" | tr -d '(' | sort -u)
miss_doc=$(comm -23 <(echo "$hdr") <(echo "$doc"))
miss_hdr=$(comm -13 <(echo "$hdr") <(echo "$doc"))
rc=0
[ -n "$miss_doc" ] && { echo "FAIL: in header, absent from contract:"; echo "$miss_doc"; rc=1; }
[ -n "$miss_hdr" ] && { echo "FAIL: in contract, absent from header:"; echo "$miss_hdr"; rc=1; }
[ $rc = 0 ] && echo "ok   contract_sync ($(echo "$hdr" | wc -l | tr -d ' ') entry points)"
exit $rc
