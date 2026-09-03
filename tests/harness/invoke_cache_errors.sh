#!/usr/bin/env bash
# T922 / T927 / T945 / T950 — the two cache-source failures.
#
# These cannot be run_negative.sh fixtures: that harness passes --schema
# unconditionally (it has to, so every other INVOKE fixture works), and these
# two are precisely about the option being absent or naming an unreadable file.
#
# The two codes are deliberately distinct. ESQLC-6002 is "no schema source at
# all" — the build was not told where to look. ESQLC-6008 is "named, but I
# cannot read it" — the build was told, and the path or its permissions are
# wrong. Collapsing them would send a reader to the wrong place: one is a
# missing compiler option, the other a broken file.
set -uo pipefail
PP="${1:?usage: invoke_cache_errors.sh <esqlcpp>}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
FIX="$ROOT/tests/conformance/gate-1"
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
fail=0

cat > "$TMP/inv.sqlc" <<'EOF'
#pragma SQL
long sqlcode;
EXEC SQL INVOKE parts AS parts_rec;
int main(void) { return 0; }
EOF

expect() {  # expect <label> <code> <args...>
  local label="$1" code="$2"; shift 2
  local out; out="$("$PP" "$TMP/inv.sqlc" "$@" -o /dev/null 2>&1)"
  if grep -q "$code" <<<"$out"; then
    echo "ok   $label ($code)"
  else
    echo "FAIL $label: expected $code"
    sed 's/^/    /' <<<"$out" | head -3
    fail=1
  fi
}

# No --schema at all.
expect "no schema source" "ESQLC-6002"

# Named, but unreadable. chmod 000 is skipped when running as root, which can
# read it anyway — so the check would pass for the wrong reason.
UNREADABLE="$TMP/unreadable.cache"
cp "$FIX/schema.cache" "$UNREADABLE"
chmod 000 "$UNREADABLE"
if [ "$(id -u)" = "0" ] || head -c1 "$UNREADABLE" >/dev/null 2>&1; then
  echo "SKIP unreadable cache: this user can read a mode-000 file"
else
  expect "unreadable cache" "ESQLC-6008" --schema "$UNREADABLE"
fi
chmod 644 "$UNREADABLE" 2>/dev/null || true

# A path that does not exist is the same condition as unreadable: named and
# unusable. Distinct from not being named at all.
expect "absent cache file" "ESQLC-6008" --schema "$TMP/no-such.cache"

exit $fail
