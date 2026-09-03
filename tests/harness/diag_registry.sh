#!/usr/bin/env bash
# The diagnostic registry, checked in both directions — but not naively.
#
# FORWARD: every ESQLC-nnnn the code can emit must be registered in a spec or
# the divergence register. Two codes (1014, 2009) were invented during the Gate
# 1 implementation and went unregistered until an ad-hoc audit found them; this
# made that audit structural.
#
# CONVERSE (Gate 10, T1035): the hazard is a code that can NEVER fire. Gate 9
# found three — two unreachable by design, one redundant with a generic
# diagnostic — and nothing was watching. Each was individually defensible;
# collectively they make the registry a worse guide than it looks, because a
# reader who trusts it goes hunting for a diagnostic that will never appear.
#
# The first draft of this check simply demanded that every registered code be
# emitted or allowlisted. Run once, it reported 42 "unexplained" codes out of
# 91 — because most are for features not yet built (008 alone registers 13 and
# has not started). That conflates two unlike things and would have forced a
# 42-line allowlist meaning "everything", which catches nothing.
#
# So the check keys off a marker the specs already carry. Gate 8 wrote "**not
# implementable here**" in ESQLC-2015's policy column and Gate 9 wrote
# "**redundant**" in ESQLC-6006's. What can go wrong with such a marker is that
# it LIES — the code is marked unreachable and the source emits it anyway, or
# vice versa — and that is what this fails on. Unmarked-and-unemitted is
# reported, never failed: it means "not built yet", which is the normal state of
# a project with four phases and two finished.
set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

emitted=$(grep -rhoE 'ESQLC-[0-9]{4}' "$ROOT/src" | sort -u)

# A registered code is a row of a diagnostics table, so a code merely discussed
# in prose is not mistaken for a registration.
rows=$(grep -rhoE '^\| `ESQLC-[0-9]{4}` \|[^|]*\|[^|]*\|' "$ROOT/specs" 2>/dev/null)
registered=$(grep -oE 'ESQLC-[0-9]{4}' <<<"$rows" | sort -u)

# Marked unreachable: the policy column says so. Both spellings the specs use.
marked=$(grep -iE 'not implementable|redundant' <<<"$rows" \
         | grep -oE 'ESQLC-[0-9]{4}' | sort -u)

fail=0; n=0
for c in $emitted; do
  n=$((n+1))
  if ! grep -rql "$c" "$ROOT/specs" "$ROOT/docs" 2>/dev/null; then
    echo "FAIL $c is emitted by the code but registered in no spec"
    fail=$((fail+1))
  fi
done

# A marker that lies, in either direction. This is the whole point of the
# converse check: an honest "never fires" is fine, a wrong one is a trap.
for c in $marked; do
  if grep -qx "$c" <<<"$emitted"; then
    echo "FAIL $c is marked unreachable or redundant in its spec, but the source"
    echo "     emits it. Either the marker is stale or the code should not be"
    echo "     there — a marker that lies is worse than none."
    fail=$((fail+1))
  fi
done

nreg=$(wc -w <<<"$registered" | tr -d ' ')
nmark=$(wc -w <<<"$marked" | tr -d ' ')
unemitted=0
for c in $registered; do
  grep -qx "$c" <<<"$emitted" || unemitted=$((unemitted+1))
done
notyet=$((unemitted - nmark))

echo "diag registry: $n emitted, $fail failures"
echo "  registered $nreg · emitted $(( nreg - unemitted )) · marked unreachable $nmark · not yet built $notyet"
if [ "$nmark" -gt 0 ]; then
  echo "  marked unreachable, and honest:"
  for c in $marked; do echo "    $c"; done
fi
exit $(( fail > 0 ))
