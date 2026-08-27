#!/usr/bin/env python3
"""Extract per-page text from the SQL/MP C manual for citation lookup.

Writes manual/manual.txt (page-delimited) and manual/pages.json ({pdf_page: text}).
Both are git-ignored. Output is a local research aid only — do not commit it and
do not paste manual prose into specs; specs cite, they do not quote.

Note the two page numbering schemes:
  * PDF page index    1..331   (what this script emits)
  * manual page label e.g. 9-6 (what specs cite)
Front matter occupies PDF pages 1..~34, so the offset is not constant across
sections. Use --find to locate a label.

Requires: pypdf  (pip install pypdf)
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PDF = ROOT / "manual" / "SqlMp-C-Reference.pdf"
OUT_TXT = ROOT / "manual" / "manual.txt"
OUT_JSON = ROOT / "manual" / "pages.json"


def extract() -> dict[int, str]:
    try:
        from pypdf import PdfReader
    except ImportError:
        sys.exit("pypdf not installed:  pip install pypdf")
    if not PDF.exists():
        sys.exit(f"{PDF} missing — run .specify/scripts/fetch-manual.sh first")
    reader = PdfReader(str(PDF))
    return {i + 1: (page.extract_text() or "") for i, page in enumerate(reader.pages)}


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--find", metavar="LABEL",
                    help="locate a manual page label, e.g. 9-6 or 10-10")
    ap.add_argument("--page", type=int, metavar="N",
                    help="print PDF page N")
    args = ap.parse_args()

    pages = extract()

    if args.find:
        pat = re.compile(rf"(?m)^\s*{re.escape(args.find)}\s*$")
        hits = [n for n, t in pages.items() if pat.search(t)]
        print(f"manual page {args.find} -> PDF page(s) {hits or 'not found'}")
        return

    if args.page:
        print(pages.get(args.page, "<no such page>"))
        return

    OUT_TXT.write_text(
        "".join(f"\n\n===== PDF PAGE {n} =====\n{t}" for n, t in sorted(pages.items()))
    )
    OUT_JSON.write_text(json.dumps({str(k): v for k, v in pages.items()}))
    print(f"{len(pages)} pages -> {OUT_TXT.relative_to(ROOT)}, "
          f"{OUT_JSON.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
