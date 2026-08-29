#!/usr/bin/env bash
# T347 — the streaming guard.
#
# A SQL/MP cursor streams; without STMT_ATTR_CURSOR_TYPE the client
# materialises the whole result set. That defect has NO functional symptom at
# fixture scale — every Gate 3 test passes on five rows either way — and the
# failure would arrive on a customer's real table.
#
# The task asked for a runtime assertion that the attribute was accepted. That
# is not implementable: the client library offers no way to read the attribute
# back. What IS checkable is that the code still sets it and still treats a
# refusal as an error, so this guard is structural, in the manner of
# diag_registry. Deleting the attribute call fails here and nowhere else,
# which is exactly what mutation T353 asserts.
set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
C="$ROOT/src/rt/cursor.c"
# Strip comments before grepping. The first version of this guard did not,
# and a mutation that changed only the comment left it happily passing — the
# same defect as Gate 2's FR-003.1 check matching identifiers inside comments.
CODE=$(sed -e 's|//.*||' "$C" | perl -0pe 's{/\*.*?\*/}{}gs')
rc=0
printf '%s' "$CODE" | grep -q "mysql_stmt_attr_set" || {
  echo "FAIL: cursor.c no longer calls mysql_stmt_attr_set"; rc=1; }
printf '%s' "$CODE" | grep -q "STMT_ATTR_CURSOR_TYPE" || {
  echo "FAIL: cursor.c no longer sets STMT_ATTR_CURSOR_TYPE; the result set"
  echo "      would be buffered rather than streamed, with no test to notice"
  rc=1; }
printf '%s' "$CODE" | grep -q "CURSOR_TYPE_READ_ONLY" || {
  echo "FAIL: cursor.c no longer requests a read-only cursor"; rc=1; }
grep -q "ESQLC-4010" "$C" || {
  echo "FAIL: a refused cursor type is no longer treated as an error"; rc=1; }
[ $rc = 0 ] && echo "ok   streaming_guard (attribute set, refusal is an error)"
exit $rc
