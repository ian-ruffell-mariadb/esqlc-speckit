#!/usr/bin/env python3
"""Spec-derived assertions over emitted C.

This is the fix for a real process debt. The golden .expected.c files were
snapshotted from actual output, so they assert what the code *does*, not what
the spec *requires* — they would enshrine a bug as happily as correct
behaviour. They are kept as regression guards; these assertions are the
specification tests.

Every assertion names the requirement it comes from. An assertion with no
requirement ID does not belong here.

Usage: spec_assertions.py <path-to-esqlcpp>
"""
from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
FIX = ROOT / "tests" / "conformance" / "gate-1"

DESC_RE = re.compile(
    r"\{\s*&(?P<name>\w+),\s*(?P<ind>&?\w+),\s*(?P<type>ESQLC_T_\w+),\s*"
    r"(?P<width>\d+)u,\s*(?P<capacity>\d+)u,\s*(?P<scale>-?\d+),\s*"
    r"(?P<signed>\d+),\s*(?P<dir>ESQLC_DIR_\w+),\s*(?P<charset>\d+)\s*\}"
)
EXEC_RE = re.compile(r'esqlc_stmt_exec\(\s*"(?P<sql>(?:[^"\\]|\\.)*)"\s*,\s*(?P<len>\d+)')
CALL_RE = re.compile(r"\b([a-z_][a-z0-9_]*)\s*\(")

failures: list[str] = []
checks = 0


def check(req: str, ok: bool, what: str) -> None:
    global checks
    checks += 1
    if not ok:
        failures.append(f"{req}: {what}")


def emit(pp: str, src: Path) -> str:
    r = subprocess.run([pp, str(src)], capture_output=True, text=True)
    if r.returncode != 0:
        failures.append(f"preprocessing {src.name} failed: {r.stderr.strip()}")
        return ""
    return r.stdout


def descriptors(out: str) -> list[dict]:
    return [m.groupdict() for m in DESC_RE.finditer(out)]


def code_only(src: str) -> str:
    """Strip comments and string literals.

    FR-003.1 constrains emitted *calls*. Scanning raw text also matches
    `parts (` inside an SQL string and `assertions (` inside a comment, which
    is how the first draft of this check produced a false failure.
    """
    out, i, n = [], 0, len(src)
    while i < n:
        c = src[i]
        if c == '/' and i + 1 < n and src[i + 1] == '*':
            j = src.find('*/', i + 2)
            i = n if j < 0 else j + 2
            out.append(' ')
        elif c == '/' and i + 1 < n and src[i + 1] == '/':
            j = src.find('\n', i)
            i = n if j < 0 else j
            out.append(' ')
        elif c == '"':
            i += 1
            while i < n and src[i] != '"':
                i += 2 if src[i] == '\\' else 1
            i += 1
            out.append(' ')
        else:
            out.append(c)
            i += 1
    return ''.join(out)


def assert_insert(out: str) -> None:
    """insert.sqlc — the primary gate fixture."""
    d = {x["name"]: x for x in descriptors(out)}

    # FR-001.7: the pragma is consumed, not passed through to the C compiler.
    check("FR-001.7", "#pragma SQL" not in out,
          "'#pragma SQL' must not survive into emitted C")

    # FR-001.19: C regions are emitted verbatim.
    check("FR-001.19", 'memcpy(part_desc, "HEX NUT, 8MM      ", 18);' in out,
          "C statements must be emitted byte-for-byte unchanged")

    # FR-001.18: emitted C carries #line directives back to the original file.
    check("FR-001.18", re.search(r'#line\s+\d+\s+"[^"]*insert\.sqlc"', out) is not None,
          "emitted C must carry #line directives naming the original source")

    # FR-002.3 / FR-002.30: CHAR(18) declared char[19] => capacity 19, width 18.
    # The extra byte is a null-terminator placeholder and is NOT sent.
    check("FR-002.3", "part_desc" in d and d["part_desc"]["capacity"] == "19",
          "char[19] must yield capacity 19")
    check("FR-002.30", "part_desc" in d and d["part_desc"]["width"] == "18",
          "char[19] for CHAR(18) must yield width 18, not 19 and not strlen")
    check("FR-002.30", "part_desc" in d
          and int(d["part_desc"]["capacity"]) - int(d["part_desc"]["width"]) == 1,
          "capacity must exceed width by exactly the terminator byte")
    check("FR-002.3", "part_desc" in d and d["part_desc"]["type"] == "ESQLC_T_CHAR_FIXED",
          "a char array host variable must bind as fixed-length character")

    # FR-002.9: integer mapping is by width, not by C type name (DIV-001).
    check("FR-002.9", "part_num" in d and d["part_num"]["width"] == "2",
          "short must yield a 16-bit descriptor")
    check("FR-002.9", "part_num" in d and d["part_num"]["signed"] == "1",
          "short must be marked signed")
    check("FR-002.9", "part_num" in d and d["part_num"]["type"] == "ESQLC_T_INT",
          "short must bind as an integer type")

    # FR-003.10 / NFR-003.2: values are parameterised, never interpolated.
    m = EXEC_RE.search(out)
    check("FR-003.10", m is not None, "an INSERT must reach esqlc_stmt_exec")
    if m:
        sql = m.group("sql")
        check("FR-003.10", ":" not in sql,
              f"no host-variable reference may survive into the statement: {sql!r}")
        check("FR-003.10", sql.count("?") == 2,
              f"each host variable must become one placeholder: {sql!r}")
        check("FR-003.10", "HEX NUT" not in sql,
              "a host variable's VALUE must never appear in the statement text")
        # FR-001.16: placeholders and descriptors must correspond one-to-one.
        check("FR-001.16", sql.count("?") == len(descriptors(out)),
              "placeholder count must equal descriptor count")

    # FR-003.6: transaction control maps onto the three ABI entry points.
    for fn, req in (("esqlc_txn_begin", "FR-003.6"),
                    ("esqlc_txn_commit", "FR-003.6"),
                    ("esqlc_txn_rollback", "FR-003.8")):
        check(req, fn in out, f"{fn} must be emitted")

    # FR-003.1: generated code calls esqlc_* and nothing else.
    known_c = {"main", "memcpy", "fprintf", "if", "while", "do", "return",
               "sizeof", "_Static_assert", "int", "long", "char", "short",
               "for", "switch"}
    stray = {n for n in CALL_RE.findall(code_only(out))
             if not n.startswith("esqlc_") and n not in known_c}
    check("FR-003.1", not stray, f"only esqlc_* calls may be emitted; found {sorted(stray)}")

    # NFR-002.2: width/capacity are pinned by static assertions.
    check("NFR-002.2", "_Static_assert" in out,
          "emitted C must assert host variable sizes so a drift breaks the build")


def assert_spans(out: str) -> None:
    """hostvar_spans.sqlc — the design bet: same-pass span capture."""
    m = EXEC_RE.search(out)
    check("FR-001.16", m is not None, "statement must reach esqlc_stmt_exec")
    if not m:
        return
    sql = m.group("sql")

    # FR-001.16: exactly the two real references become placeholders, despite
    # the source containing four colon-prefixed-looking sequences.
    check("FR-001.16", len(descriptors(out)) == 2,
          f"exactly two host variables expected, got {len(descriptors(out))}")
    check("FR-001.16", sql.count("?") == 2, f"expected two placeholders: {sql!r}")

    # FR-001.6: a :name inside a "string" is data, not a host variable.
    check("FR-001.6", ":alsonot" in sql,
          "a colon sequence inside a string literal must survive verbatim")

    # FR-001.4: a :name after -- is inside a comment and must not be captured.
    check("FR-001.4", ":notahostvar" not in sql,
          "a colon sequence in an SQL comment must not be captured")
    check("FR-001.4", "a comment mentioning" not in sql,
          "SQL comment text must not survive into the statement")


def assert_select(out: str) -> None:
    """rt/select_into.sqlc — Gate 2: direction by landmark, indicators."""
    d = {x["name"]: x for x in descriptors(out)}

    # FR-001.16: references inside the INTO region are outputs; everything
    # else — notably the WHERE-clause key — stays an input.
    check("FR-001.16", d.get("part_desc", {}).get("dir") == "ESQLC_DIR_OUT",
          "an INTO-list reference must be marked DIR_OUT")
    check("FR-001.16", d.get("weight", {}).get("dir") == "ESQLC_DIR_OUT",
          "an INTO-list reference must be marked DIR_OUT")
    check("FR-001.16", d.get("part_num", {}).get("dir") == "ESQLC_DIR_IN",
          "a WHERE-clause reference must stay DIR_IN")

    # FR-002.15: an indicator supplied in source becomes a real address;
    # one not supplied becomes 0, which the runtime reads as "no indicator".
    check("FR-002.15", d.get("weight", {}).get("ind") == "&weight_ind",
          "a supplied indicator must be emitted as its address")
    check("FR-002.15", d.get("part_desc", {}).get("ind") == "0",
          "an absent indicator must be emitted as 0")

    # FR-003.10: inputs are still parameterised, outputs are not placeholders.
    m = EXEC_RE.search(out)
    check("FR-003.10", m is not None, "the SELECT must reach esqlc_stmt_exec")
    if m:
        sql = m.group("sql")
        check("FR-003.10", ":" not in sql,
              f"no host-variable reference may survive into the statement: {sql!r}")
        check("FR-003.10", "INTO" not in sql.upper(),
              f"the INTO clause is a binding instruction, not SQL to send: {sql!r}")
        check("FR-003.10", sql.count("?") == 1,
              f"only the WHERE input becomes a placeholder: {sql!r}")


def assert_insert_directions(out: str) -> None:
    """insert.sqlc — the INSERT INTO landmark regression guard.

    `INSERT INTO parts` contains the INTO landmark. If direction
    classification leaked out of the SELECT handler, these would flip to
    DIR_OUT and Gate 1's working path would break silently.
    """
    ds = descriptors(out)
    check("FR-001.16", len(ds) == 2, f"expected two INSERT descriptors, got {len(ds)}")
    for x in ds:
        check("FR-001.16", x["dir"] == "ESQLC_DIR_IN",
              f"INSERT reference {x['name']} must stay DIR_IN despite 'INSERT INTO'")


def main() -> int:
    if len(sys.argv) < 2:
        print("usage: spec_assertions.py <esqlcpp>", file=sys.stderr)
        return 2
    pp = sys.argv[1]

    out = emit(pp, FIX / "insert.sqlc")
    if out:
        assert_insert(out)
    out = emit(pp, FIX / "hostvar_spans.sqlc")
    if out:
        assert_spans(out)
    out = emit(pp, FIX / "rt" / "select_into.sqlc")
    if out:
        assert_select(out)
    out = emit(pp, FIX / "insert.sqlc")
    if out:
        assert_insert_directions(out)

    for f in failures:
        print(f"FAIL {f}")
    print(f"spec assertions: {checks} checked, {len(failures)} failed")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
