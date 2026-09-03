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


CHECK_RE = re.compile(r"if \((sqlcode[^)]*)\)\s*([A-Za-z_][\w]*\(\)|goto \w+);")


def assert_whenever(out: str) -> None:
    """whenever_conditions.sqlc — the published precedence order."""
    found = [m.group(1).strip() for m in CHECK_RE.finditer(out)]
    want = ["sqlcode == 100", "sqlcode < 0", "sqlcode > 0 && sqlcode != 100"]
    # FR-005.5: §9 p.9-6's table fixes this order. Reordering changes which
    # handler runs when several conditions apply to one statement.
    check("FR-005.5", found[:3] == want,
          f"checks must be NOT FOUND, SQLERROR, SQLWARNING in order; got {found[:3]}")
    check("FR-005.3", len(found) >= 3, "all three conditions must emit a check")


def assert_whenever_continue(out: str) -> None:
    """whenever_actions.sqlc — CONTINUE emits nothing."""
    # The fixture sets CALL, then GOTO, then GO TO, then CONTINUE, with one
    # statement after each. Four statements, but only three checks.
    n_checks = len(CHECK_RE.findall(out))
    check("FR-005.4", n_checks == 3,
          f"CONTINUE must emit no check at all; expected 3 checks, got {n_checks}")


def assert_whenever_applies_to(out: str) -> None:
    """whenever_applies_to.sqlc — SD-5: not on transaction control."""
    # The emitted order is BEGIN WORK, INSERT, COMMIT WORK. Exactly one check
    # must exist, and it must sit in the INSERT's block.
    n = len(CHECK_RE.findall(out))
    check("FR-005.7", n == 1,
          f"exactly one check expected — after the INSERT only (SD-5); got {n}")
    after_commit = out.split("esqlc_txn_commit")[-1]
    check("FR-005.7", not CHECK_RE.search(after_commit),
          "no check may follow COMMIT WORK (SD-5)")


def assert_sqlca(out: str) -> None:
    """The generated SQLCA: total is API, eye-catcher leads."""
    check("FR-005.14", "#define SQLCA_LEN 430" in out,
          "SQLCA_LEN must be 430")
    check("FR-005.14", '#define SQLCA_EYE_CATCHER "CA"' in out,
          "the eye-catcher must be CA")
    check("FR-005.14", "sizeof(struct sqlca_type) == SQLCA_LEN" in out,
          "the 430-byte total must be statically asserted — programs copy it")
    check("FR-005.14", "esqlc_sqlca_register(&sqlca, SQLCA_LEN)" in out,
          "the SQLCA must be registered, or the runtime writes nowhere")


def assert_sqlsa_sizes(out: str) -> None:
    """T530 — both published totals, exactly, and the packing that reaches them.

    v300 measures 840 unpacked against a published 838, so the packing is not
    optional at either version. FR-005.27 documents the pragma for the four
    *_R330 types only, which understates it.
    """
    check("FR-005.16", "#define SQLSA_LEN 838" in out,
          "SQLSA_LEN must be 838 at version 300")
    check("FR-005.16", "#define SQLSA_LEN_R330 1790" in out,
          "SQLSA_LEN_R330 must be 1790")
    check("FR-005.16", '#define SQLSA_EYE_CATCHER "SA"' in out,
          "the eye-catcher must be SA")
    check("FR-005.16", "sizeof(struct SQLSA_TYPE) == SQLSA_LEN" in out,
          "the 838-byte total must be statically asserted")
    check("FR-005.16", "sizeof(struct SQLSA_TYPE_R330) == SQLSA_LEN_R330" in out,
          "the 1790-byte total must be statically asserted")
    check("FR-005.27", "__attribute__((packed))" in out or "#pragma pack" in out,
          "both layouts need packing; 838 and 1790 are unreachable without it")
    # Both type declarations exist regardless of the selected version: the
    # manual names them distinctly and FR-005.26 has VERSION CURRENT emit both.
    check("FR-005.16", "struct SQLSA_TYPE " in out and "struct SQLSA_TYPE_R330 " in out,
          "both version families must be declared")


def assert_sqlsa_layout(out: str) -> None:
    """T531-T535 — the union, the widths, VSBB, and fixed-width integers."""
    check("FR-005.21a",
          "offsetof(struct SQLSA_TYPE, u.dml) == offsetof(struct SQLSA_TYPE, u.prepare)"
          in out.replace("\n", " ").replace("  ", " "),
          "dml and prepare are arms of a union, not coexisting members")
    check("FR-005.21b", "int32_t records_accessed" in out,
          "v300 counters are 32-bit")
    check("FR-005.21b", "int64_t records_accessed" in out,
          "v330 counters are 64-bit")
    check("FR-005.21b", "int16_t waits" in out and "int32_t waits" in out,
          "waits widens 16->32 across the version boundary")
    check("FR-005.21c", "sqlsa_reserved" in out,
          "v300 has sqlsa_reserved where v330 has the VSBB flags")
    check("FR-005.21c", "vsbb_write" in out and "vsbb_flushed" in out,
          "v330 carries the VSBB flags")
    check("FR-005.23", "#define SQLSA_VSBB_TRUE (-1)" in out,
          "the VSBB flags use -1 for true")
    check("FR-005.23", "#define SQLSA_VSBB_FALSE 0" in out,
          "the VSBB flags use 0 for false")
    # T535. A native `long` is 4 bytes on ILP32 and 8 on LP64, so the published
    # layout only holds by accident on one of them. sizeof alone would not
    # catch this on the machine where it is wrong.
    body = out[out.find("SQLSA_TYPE"):] if "SQLSA_TYPE" in out else ""
    check("FR-005.21",
          not re.search(r"^\s*(unsigned\s+)?long\s+\w+\s*(\[|;)", body, re.M),
          "every integer field must be a fixed-width type, never a native long")


def assert_sqlsa_registered(out: str) -> None:
    """T542 — registration is emitted before the first statement.

    Asserted on a fixture that has statements. The layout fixture has none, and
    a program with no statements needs no registration: there is nothing that
    could populate the area. That is correct behaviour, not a missing call.
    """
    check("FR-005.17", "esqlc_sqlsa_register(&sqlsa, SQLSA_LEN, 300)" in out,
          "the SQLSA must be registered, or the runtime writes nowhere")


def assert_sqlsa_emission(out: str) -> None:
    """T536, T540, T541 — ABI-only calls and the sync markers."""
    check("NFR-005.1", "--8<-- esqlc sqlsa layout begin --8<--" in out
          and "--8<-- esqlc sqlsa layout end --8<--" in out,
          "the layout block must be bracketed so sqlsa_layout_sync can lift it")
    called = {m for m in CALL_RE.findall(out)}
    leaked = {c for c in called if c.startswith(("mysql_", "mariadb_"))}
    check("FR-003.1", not leaked, f"emitted C called MariaDB directly: {leaked}")



LANDMARK_RE = re.compile(r'esqlc_stmt_exec\(\s*"(?:[^"\\]|\\.)*"\s*,\s*\d+\s*,'
                         r'[^;]*?,\s*(?P<table>NULL|"[A-Za-z0-9_]+")\s*\)')


def landmarks(out: str) -> list[str]:
    """The table argument of each esqlc_stmt_exec call, in source order."""
    return [m.group("table") for m in LANDMARK_RE.finditer(out)]


def assert_table_landmark(out: str) -> None:
    """T630, T631 — the three DML forms yield their table; a literal does not."""
    got = landmarks(out)
    check("FR-005.22", len(got) == 4,
          f"expected four statements to carry a table argument, got {len(got)}: {got}")
    check("FR-005.22", all(t == '"parts"' for t in got),
          f'every statement here targets `parts`; got {got}')
    # T631. The fourth statement stores the text "UPDATE widgets SET x". The
    # landmark runs in the scanner, which already skips string literals while
    # recording INTO and FROM, so `widgets` must never be read as a table.
    check("NFR-001.1", '"widgets"' not in out,
          "a table-like token inside a string literal was read as a landmark")


def assert_table_landmark_absent(out: str) -> None:
    """T632 — the hard forms yield nothing.

    A wrong table name is worse than none: `table_name` reads as authoritative,
    so a plausible name attributed to the wrong table is undetectable, whereas
    the sentinel is visibly "not measured".
    """
    got = landmarks(out)
    check("FR-005.22", len(got) == 3,
          f"expected three statements, got {len(got)}")
    check("FR-005.22", all(t == "NULL" for t in got),
          f"a multi-table UPDATE, a delimited identifier and a leading "
          f"subquery must all yield NULL; got {got}")


def assert_update_placeholders(out: str) -> None:
    """T633, T634, T635 — placeholders, verbatim bodies, ABI-only calls."""
    execs = EXEC_RE.findall(out)
    check("FR-003.10", len(execs) >= 2, "both statements must reach esqlc_stmt_exec")
    for sql, _ in execs:
        check("FR-003.10", ":" not in sql,
              f"no host-variable reference may survive into the statement: {sql!r}")
    # The UPDATE has three references, the DELETE one.
    if execs:
        check("FR-003.10", execs[0][0].count("?") == 3,
              f"each host variable becomes one placeholder: {execs[0][0]!r}")
    check("NFR-001.1", "SET weight = ?" in out and "DELETE FROM parts WHERE" in out,
          "statement bodies must pass through verbatim apart from placeholders")
    stray = {n for n in CALL_RE.findall(code_only(out))
             if not n.startswith("esqlc_")
             and n not in {"main", "memcpy", "printf", "if", "while", "do", "return",
                           "sizeof", "_Static_assert", "int", "long", "char",
                           "short", "for", "switch", "strncmp"}}
    check("FR-003.1", not stray, f"only esqlc_* calls may be emitted; found {sorted(stray)}")


def assert_input_indicator(out: str) -> None:
    """T639 — `:w :ind` associates on the input side; a comma does not."""
    d = descriptors(out)
    # weight carries w_ind; part_desc and part_num do not.
    byname = {x["name"]: x for x in d}
    check("FR-002.15", "weight" in byname and byname["weight"]["ind"] == "&w_ind",
          f"`:weight :w_ind` must associate; got {byname.get('weight', {}).get('ind')}")
    check("FR-002.15", "part_desc" in byname and byname["part_desc"]["ind"] == "0",
          "a comma separates list items, so part_desc has no indicator")
    check("FR-002.15", len(d) == 3,
          f"three descriptors expected — the indicator is not one of them; got {len(d)}")



def assert_positioned_refused(pp: str) -> None:
    """T637, T638 — the refusal must name the positioned operation.

    The negative harness compares code, line and column only, and
    `ESQLC-1012` at that position is satisfied equally by "UPDATE is not
    implemented" and by "positioned UPDATE is not implemented". Before Phase C
    those fixtures therefore passed against no implementation at all, which is
    a broken test rather than a green one. This asserts the reason.
    """
    for fixture, verb in (("update_where_current_of", "UPDATE"),
                          ("delete_where_current_of", "DELETE")):
        r = subprocess.run([pp, str(FIX / "negative" / f"{fixture}.sqlc")],
                           capture_output=True, text=True)
        msg = r.stderr
        check("FR-001.15", "ESQLC-1012" in msg,
              f"{fixture}: must be refused")
        check("FR-001.15",
              "CURRENT OF" in msg or "positioned" in msg.lower(),
              f"{fixture}: the refusal must name the positioned operation, not "
              f"merely '{verb}'; got {msg.strip()[:120]!r}")



def compiles(out: str, tag: str) -> bool:
    """Compile emitted C. The strongest Tier 1 check in Gate 7.

    A descriptor whose width disagrees with sizeof fails its own NFR-002.2
    assertion, which is exactly how the hand-declared `long` defect surfaced:
    decl.cc claimed width 4 and the assertion measured 8.
    """
    import tempfile, os
    with tempfile.TemporaryDirectory() as d:
        c = os.path.join(d, "t.c")
        with open(c, "w") as f:
            f.write(out)
        r = subprocess.run(["cc", "-std=c11", "-I", str(ROOT / "include"),
                            "-c", c, "-o", os.path.join(d, "t.o")],
                           capture_output=True, text=True)
        if r.returncode != 0:
            failures.append(f"NFR-002.2: {tag} emitted C does not compile: "
                            f"{r.stderr.strip().splitlines()[0] if r.stderr else '?'}")
        return r.returncode == 0


def assert_int_widths(out: str) -> None:
    """T730, T731, T732 — widths from sizeof, not from the type's spelling."""
    d = {x["name"]: x for x in descriptors(out)}
    import ctypes
    want = {"w16": 2, "w32": 4, "wlong": ctypes.sizeof(ctypes.c_long),
            "w64": 8}
    for n, w in want.items():
        check("FR-002.9", n in d and int(d[n]["width"]) == w,
              f"{n} must be width {w} (sizeof on this host), got "
              f"{d.get(n, {}).get('width')}")
        check("FR-002.9", n in d and d[n]["type"] == "ESQLC_T_INT",
              f"{n} must bind as an integer type")
    # T731 — the emitted assertion must agree with sizeof, or nothing compiles.
    check("NFR-002.2", "_Static_assert" in out, "width assertions must be emitted")
    compiles(out, "int_widths")
    # T732
    for sql, _ in EXEC_RE.findall(out):
        check("FR-003.10", ":" not in sql,
              f"no host-variable reference may survive: {sql!r}")


def assert_float_widths(out: str) -> None:
    """T733 — one family, two widths, and NOT the date-time constant."""
    d = {x["name"]: x for x in descriptors(out)}
    check("FR-002.10", "f4" in d and d["f4"]["type"] == "ESQLC_T_FLOAT",
          "float must bind as ESQLC_T_FLOAT")
    check("FR-002.10", "f4" in d and int(d["f4"]["width"]) == 4,
          f"float must be width 4, got {d.get('f4', {}).get('width')}")
    check("FR-002.10", "f8" in d and d["f8"]["type"] == "ESQLC_T_FLOAT",
          "double must bind as ESQLC_T_FLOAT — one family, separated by width")
    check("FR-002.10", "f8" in d and int(d["f8"]["width"]) == 8,
          f"double must be width 8, got {d.get('f8', {}).get('width')}")
    # code_only, because the fixture's own comment mentions the constant and a
    # raw text scan matches that. Fourth time a guard in this project has
    # matched a comment rather than code; code_only exists for exactly this.
    check("FR-002.10", "ESQLC_T_DATETIME" not in code_only(out),
          "ESQLC_T_DATETIME must stay unused until TYPE AS")
    compiles(out, "float_widths")


def assert_varchar_layout(out: str) -> None:
    """T734, T735 — the descriptor and the asserted layout."""
    d = {x["name"]: x for x in descriptors(out)}
    check("FR-002.6", "vc" in d and d["vc"]["type"] == "ESQLC_T_CHAR_VAR",
          "a VARCHAR structure must bind as ESQLC_T_CHAR_VAR")
    # SD-10: capacity is the declared val size, width is capacity - 1.
    check("FR-002.6", "vc" in d and int(d["vc"]["capacity"]) == 27,
          f"capacity is the declared val size, 27; got {d.get('vc', {}).get('capacity')}")
    check("FR-002.6", "vc" in d and int(d["vc"]["width"]) == 26,
          f"width is capacity - 1 (SD-10); got {d.get('vc', {}).get('width')}")
    # The structure is anonymous, so offsetof(struct tag, val) — which the plan
    # called for — cannot be written in portable C11. sizeof on a member
    # expression can, and these three together are a stronger proof: len is 2,
    # val is n, and the whole structure is exactly 2 + n, so there is no padding
    # anywhere and val must be at offset 2. That is what licenses the runtime
    # reading val at offset 2.
    flat = " ".join(out.split())
    check("FR-002.21", "sizeof(vc.len) == 2" in flat,
          "sizeof(len) must be asserted as 2 — p.2-9 requires short, not int")
    check("NFR-002.2", "sizeof(vc.val) == 27" in flat,
          "sizeof(val) must be asserted at the declared capacity")
    check("NFR-002.2", "__typeof__(vc), val) == 2" in flat,
          "val's offset must be asserted as 2 — the runtime reads it there")
    compiles(out, "varchar_layout")


def assert_types_declare_section(out: str) -> None:
    """T736 — scope and identifier rules survive the new type table."""
    names = {x["name"] for x in descriptors(out)}
    check("FR-002.1", "outside_the_section" not in names,
          "a declaration outside the declare section must not be harvested")
    check("FR-002.2", "_leading_underscore" in names,
          "a leading-underscore identifier is a valid host variable name")
    check("FR-002.2", "MiXeD_Case99" in names,
          "a mixed-case identifier with digits is a valid host variable name")
    compiles(out, "types_declare_section")



def assert_declare_with_comments(out: str) -> None:
    """C comments in a declare section must not stop declarations being read.

    Principle II: a program that comments its declarations is valid C, and most
    real programs do. The tokenizer used to emit a `/` token and refuse the
    declaration with ESQLC-1012, naming punctuation as an unsupported type.

    All four declarations must be harvested, with their widths intact — a
    comment between a type and its name would silently produce a wrong
    descriptor rather than an error.
    """
    d = {x["name"]: x for x in descriptors(out)}
    for n, w in (("part_num", 2), ("part_desc", 18), ("qty", 4), ("weight", 2)):
        check("FR-002.1", n in d, f"{n} must be harvested despite the comments")
        if n in d:
            check("FR-002.2", int(d[n]["width"]) == w,
                  f"{n} width {d[n]['width']}, want {w} — a comment must not "
                  f"disturb the declaration it sits beside")
    # A comment is not a declaration: exactly four, no phantom fifth.
    check("FR-002.1", len(descriptors(out)) == 4,
          f"four declarations expected, got {len(descriptors(out))}")
    # Line fidelity of the emitted C. Note what this does and does not guard:
    # #line directives come from the SCANNER's positions, not from decl.cc's
    # tokenizer, so this cannot observe the tokenizer's line tracking at all.
    # A mutation dropping adv() inside a comment survives this check and is
    # caught by negative/declare_error_after_comment, where a diagnostic after
    # a four-line comment is reported three lines early. Both tests are needed
    # and they guard different things.
    src = (FIX / "declare_with_comments.sqlc").read_text().splitlines()
    want = next(i for i, l in enumerate(src, 1) if "INSERT INTO parts" in l)
    emitted = {int(n) for n in re.findall(
        r'#line\s+(\d+)\s+"[^"]*declare_with_comments\.sqlc"', out)}
    check("FR-001.18", want in emitted,
          f"the INSERT is on source line {want}; emitted #line numbers are "
          f"{sorted(emitted)} — a comment shifted the line count")
    compiles(out, "declare_with_comments")



# Gate 8 charset ids. Deliberately project-internal: §10 p.10-6 puts the real
# character-set ID in the SQLDA's `precision` field and p.10-11 says the values
# come from the `sqlh` header, which this project does not have (002 Q7).
CS_UNKNOWN, CS_8859_1, CS_8859_2, CS_8859_7, CS_8859_8, CS_8859_9, CS_KSC5601 = (
    0, 1, 2, 7, 8, 9, 51)


def assert_charset_clause(out: str) -> None:
    """T830, T831, T832, T842 — the infix clause reaches the descriptor."""
    d = {x["name"]: x for x in descriptors(out)}
    check("FR-002.4", "c_l2" in d and int(d["c_l2"]["charset"]) == CS_8859_2,
          f"ISO88592 must reach the descriptor; got "
          f"{d.get('c_l2', {}).get('charset')}")
    # T831 — `CHARACTER SET IS` is the same clause (p.2-24: "[ IS ] are
    # keywords that must precede the character set name").
    check("FR-002.4", "c_greek" in d and int(d["c_greek"]["charset"]) == CS_8859_7,
          f"CHARACTER SET IS must be accepted identically; got "
          f"{d.get('c_greek', {}).get('charset')}")
    check("FR-002.4", "plain" in d and int(d["plain"]["charset"]) == CS_UNKNOWN,
          "an absent clause is UNKNOWN, i.e. 0")
    # T832 — the clause must not disturb the array it interrupts.
    for n in ("c_l2", "c_greek", "plain"):
        check("FR-002.3", n in d and int(d[n]["capacity"]) == 9
              and int(d[n]["width"]) == 8,
              f"{n} must stay capacity 9 / width 8 with the clause present")
    for sql, _ in EXEC_RE.findall(out):
        check("FR-003.10", ":" not in sql,
              f"no host-variable reference may survive: {sql!r}")
    compiles(out, "charset_clause")


def assert_charset_varchar(out: str) -> None:
    """T833, T834, T835 — the clause inside Gate 7's positional shape check."""
    d = {x["name"]: x for x in descriptors(out)}
    check("FR-002.6", "v_kr" in d and d["v_kr"]["type"] == "ESQLC_T_CHAR_VAR",
          "a VARCHAR structure with a charset clause must still be recognised — "
          "Gate 7's shape check is positional and this inserts three tokens")
    check("FR-002.6", "v_kr" in d and int(d["v_kr"]["charset"]) == CS_KSC5601,
          f"KSC5601 must reach the descriptor; got "
          f"{d.get('v_kr', {}).get('charset')}")
    check("FR-002.6", "v_kr" in d and int(d["v_kr"]["capacity"]) == 11,
          "capacity is still the declared val size (SD-10)")
    flat = " ".join(out.split())
    check("NFR-002.2", "sizeof(v_kr.len) == 2" in flat
          and "__typeof__(v_kr), val) == 2" in flat,
          "the VARCHAR layout assertions must survive the clause")
    check("FR-002.15", "v_kr" in d and d["v_kr"]["ind"] == "&v_ind",
          f"the indicator must still associate; got {d.get('v_kr', {}).get('ind')}")
    compiles(out, "charset_varchar")


def assert_charset_keywords(out: str) -> None:
    """T836, T837 — one id per set, and UNKNOWN is 0."""
    d = {x["name"]: x for x in descriptors(out)}
    want = {"a": CS_8859_1, "b": CS_8859_2, "c": CS_8859_7, "d": CS_8859_8,
            "e": CS_8859_9, "f": CS_KSC5601, "g": CS_UNKNOWN, "h": CS_UNKNOWN}
    for n, cs in want.items():
        check("FR-002.8", n in d and int(d[n]["charset"]) == cs,
              f"{n} must carry charset {cs}; got {d.get(n, {}).get('charset')}")
    # T837. p.2-24 makes UNKNOWN and an absent clause equivalent, which is what
    # resolves SD-1 after seven gates of carrying it as provisional.
    check("FR-002.8", "g" in d and "h" in d
          and d["g"]["charset"] == d["h"]["charset"],
          "UNKNOWN and an absent clause must be indistinguishable (p.2-24)")
    # Distinct sets must not collide onto one id.
    ids = {int(d[n]["charset"]) for n in ("a", "b", "c", "d", "e", "f") if n in d}
    check("FR-002.8", len(ids) == 6,
          f"six distinct sets must yield six distinct ids; got {sorted(ids)}")
    compiles(out, "charset_keywords")


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
    out = emit(pp, FIX / "whenever_conditions.sqlc")
    if out:
        assert_whenever(out)
    out = emit(pp, FIX / "whenever_actions.sqlc")
    if out:
        assert_whenever_continue(out)
    out = emit(pp, FIX / "whenever_applies_to.sqlc")
    if out:
        assert_whenever_applies_to(out)
    out = emit(pp, FIX / "rt" / "whenever_flow.sqlc")
    if out:
        assert_sqlca(out)
    out = emit(pp, FIX / "sqlsa_sizes.sqlc")
    if out:
        assert_sqlsa_sizes(out)
        assert_sqlsa_layout(out)
        assert_sqlsa_emission(out)
    out = emit(pp, FIX / "rt" / "sqlsa_cursor_stats.sqlc")
    if out:
        assert_sqlsa_registered(out)
    out = emit(pp, FIX / "table_landmark.sqlc")
    if out:
        assert_table_landmark(out)
    out = emit(pp, FIX / "table_landmark_absent.sqlc")
    if out:
        assert_table_landmark_absent(out)
    out = emit(pp, FIX / "update_placeholders.sqlc")
    if out:
        assert_update_placeholders(out)
    out = emit(pp, FIX / "update_indicator_assoc.sqlc")
    if out:
        assert_input_indicator(out)
    assert_positioned_refused(pp)
    out = emit(pp, FIX / "int_widths.sqlc")
    if out:
        assert_int_widths(out)
    out = emit(pp, FIX / "float_widths.sqlc")
    if out:
        assert_float_widths(out)
    out = emit(pp, FIX / "varchar_layout.sqlc")
    if out:
        assert_varchar_layout(out)
    out = emit(pp, FIX / "types_declare_section.sqlc")
    if out:
        assert_types_declare_section(out)
    out = emit(pp, FIX / "declare_with_comments.sqlc")
    if out:
        assert_declare_with_comments(out)
    out = emit(pp, FIX / "charset_clause.sqlc")
    if out:
        assert_charset_clause(out)
    out = emit(pp, FIX / "charset_varchar.sqlc")
    if out:
        assert_charset_varchar(out)
    out = emit(pp, FIX / "charset_keywords.sqlc")
    if out:
        assert_charset_keywords(out)

    for f in failures:
        print(f"FAIL {f}")
    print(f"spec assertions: {checks} checked, {len(failures)} failed")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
