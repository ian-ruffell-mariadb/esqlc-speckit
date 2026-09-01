#!/usr/bin/env python3
"""Verify every manual citation in the specs against the manual itself.

Principle I makes the manual the contract, so a citation pointing at the wrong
page is worse than no citation: it looks like evidence. This checks all of them.

Three checks:

  1. Resolvable — every `[SQLPM/C §n p.X-Y]` page label appears in the manual,
     exactly once. Catches invented and mistyped pages.
  2. Consistent — the section number in the citation matches the page label's
     own section. Catches `§9 p.4-16`, where one half was edited and not the
     other.
  3. Content — a curated set of load-bearing citations must match a pattern on
     the cited page. Catches a citation that survived a rewrite of the claim.

Skips with 77 (ctest SKIP_RETURN_CODE) when `manual/pages.json` is absent.
`manual/` is gitignored — the PDF is third-party copyright — so CI skips this
and it runs for anyone with a local copy.

Regenerate pages.json with: tools/extract-manual.py
"""

import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
PAGES = os.path.join(ROOT, "manual", "pages.json")

# Curated content checks: requirement -> (page label, regex, what it must say).
# Add a row whenever a citation carries real weight. These are the claims that
# would be expensive to get wrong.
CONTENT = [
    ("FR-002.3",   "2-3",  r"char\s+hostvar\s*\[|CHAR\s*\(",     "char host variable mapping"),
    ("FR-005.5",   "9-6",  r"sqlcode\s*==?\s*100",               "WHENEVER precedence"),
    ("FR-005.14",  "9-12", r"SQLCA_LEN\s*430",                   "SQLCA_LEN is 430"),
    ("FR-004.16",  "4-16", r"Determining the Cursor Position",   "cursor position section"),
    ("FR-004.11",  "4-18", r"DECLARE CURSOR",                    "DECLARE CURSOR"),
    ("FR-007.6b",  "10-7", r"reserved|cprl_ptr",                 "sqlvar reserved field"),
]

CITE = re.compile(r"\[SQLPM/C §(\d+|[A-D]) p\.([0-9A-D]+)-(\d+)\]")


def main():
    if not os.path.exists(PAGES):
        print("SKIP: manual/pages.json absent (manual/ is gitignored)")
        return 77

    with open(PAGES) as f:
        pages = json.load(f)

    # The index repeats page labels as entries, so restrict to body pages.
    # Continuation pages carry the running header rather than "Index", so cut
    # from where the index starts rather than filtering page by page.
    index_start = min((int(n) for n, t in pages.items()
                       if t and re.search(r"(?m)^\s*Index-\d+\s*$", t)),
                      default=10**6)
    body = {n: t for n, t in pages.items() if t and int(n) < index_start}

    # label -> pdf page, by finding the printed label on its own line
    def resolve(label):
        return sorted((n for n, t in body.items()
                       if re.search(rf"(?m)^\s*{re.escape(label)}\s*$", t)),
                      key=int)

    # Extraction drops some footers, so a label being absent does not mean the
    # page does not exist. Bound each section by the labels that did extract: a
    # citation inside that range is unverifiable, one outside it is wrong.
    extent = {}
    for n, t in body.items():
        for part, num in re.findall(r"(?m)^\s*([0-9A-D]+)-(\d+)\s*$", t):
            lo, hi = extent.get(part, (10**6, 0))
            extent[part] = (min(lo, int(num)), max(hi, int(num)))

    specs = []
    for d in sorted(os.listdir(os.path.join(ROOT, "specs"))):
        p = os.path.join(ROOT, "specs", d, "spec.md")
        if os.path.exists(p):
            specs.append(p)

    failures = []
    unverifiable = []
    seen = set()
    total = 0
    verified = 0

    for path in specs:
        rel = os.path.relpath(path, ROOT)
        with open(path) as f:
            for lineno, line in enumerate(f, 1):
                for sec, part, num in CITE.findall(line):
                    total += 1
                    label = f"{part}-{num}"
                    key = (sec, label)
                    if key in seen:
                        continue
                    seen.add(key)

                    hits = resolve(label)
                    if len(hits) == 0:
                        lo, hi = extent.get(part, (None, None))
                        if lo is not None and lo <= int(num) <= hi:
                            unverifiable.append(
                                f"{rel}:{lineno}: p.{label} — footer not extracted "
                                f"(§{part} spans {part}-{lo}..{part}-{hi})")
                        else:
                            span = f"§{part} spans {part}-{lo}..{part}-{hi}" if lo else \
                                   f"no page of §{part} was extracted"
                            failures.append(
                                f"{rel}:{lineno}: p.{label} is outside the manual ({span})")
                        continue
                    if len(hits) > 1:
                        failures.append(f"{rel}:{lineno}: p.{label} is ambiguous (pages {hits})")
                        continue
                    if part != sec:
                        failures.append(
                            f"{rel}:{lineno}: cites §{sec} but page label p.{label} "
                            f"belongs to section {part}")
                    verified += 1

    print(f"citations found: {total}  distinct: {len(seen)}  "
          f"resolved: {verified}  unverifiable: {len(unverifiable)}")
    for u in unverifiable:
        print(f"  info {u}")

    # Content checks
    content_ok = 0
    for req, label, pattern, desc in CONTENT:
        hits = resolve(label)
        if len(hits) != 1:
            failures.append(f"content check {req}: p.{label} did not resolve to one page ({hits})")
            continue
        text = pages[hits[0]] or ""
        if re.search(pattern, text, re.I):
            content_ok += 1
        else:
            failures.append(
                f"content check {req}: p.{label} (pdf {hits[0]}) does not mention {desc}")

    print(f"content checks: {content_ok}/{len(CONTENT)} verified")

    if failures:
        print()
        for f_ in failures:
            print(f"FAIL {f_}")
        print(f"\n{len(failures)} citation failure(s)")
        return 1

    print("all citations resolve, sections agree, content verified")
    return 0


if __name__ == "__main__":
    sys.exit(main())
