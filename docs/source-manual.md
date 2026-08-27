# Source manual: provenance and citation

## The document

| | |
|---|---|
| Title | HP NonStop SQL/MP Programming Manual for C |
| Part number | 429847-008 |
| Published | August 2012 |
| Product version | NonStop SQL/MP G06 and H01 |
| Supported RVUs | J06.03+, H06.03+, G06.00+, D46.00+ |
| Length | 331 PDF pages |
| Source URL | `http://nonstoptools.com/manuals/SqlMp-C-Reference.pdf` |
| Size | 1,130,780 bytes (pinned in `fetch-manual.sh`) |
| Copyright | © 2012 Hewlett-Packard Development Company, L.P. |

## Not vendored

The PDF is **not** committed. `manual/` is git-ignored. Fetch it locally:

```bash
./.specify/scripts/fetch-manual.sh && ./.specify/scripts/extract-manual.py
```

Specs **cite** the manual; they do not reproduce it. Restating factual API data
— type-mapping tables, constant values, structure field names, SQLDA type codes
— is what a specification is for. Copying prose, worked examples, or listings is
not, and reviewers should reject it.

## Citation convention

`[SQLPM/C §9 p.9-6]` — section 9, manual page label 9-6.

Manual page labels are per-section (`9-6` = section 9, page 6) and do **not**
match PDF page indices, because front matter occupies roughly the first 34 PDF
pages and the offset shifts across sections. To resolve a label:

```bash
./.specify/scripts/extract-manual.py --find 9-6     # -> PDF page index
./.specify/scripts/extract-manual.py --page 186     # -> that page's text
```

Abbreviations used in citations, matching the manual's own:

| Tag | Manual |
|-----|--------|
| `SQLPM/C` | SQL/MP Programming Manual for C (this document) |
| `SQLRM` | SQL/MP Reference Manual |
| `CPG` | C/C++ Programmer's Guide |
| `VMG` | SQL/MP Version Management Guide |

Anything cited to `SQLRM`, `CPG`, or `VMG` is marked `[EXTERNAL]` in specs: the
manual defers the detail, so this project must make and record its own decision
before that requirement is implementable.

## Section map

| § | Title | Owning feature |
|---|-------|----------------|
| 1 | Introduction | — (orientation only) |
| 2 | Host Variables | 002, 006 |
| 3 | SQL/MP Statements and Directives | 001 |
| 4 | Data Retrieval and Modification | 004 |
| 5 | SQL/MP System Procedures | 005, 008 |
| 6 | Explicit Program Compilation | 001, 008 |
| 7 | Program Execution | 003, 008 |
| 8 | Program Invalidation and Automatic SQL Recompilation | 008 |
| 9 | Error and Status Reporting | 005 |
| 10 | Dynamic SQL Operations | 007 |
| 11 | Character Processing Rules (CPRL) Procedures | 008 |
| A | SQL/MP Sample Database | 006 (test fixtures) |
| B | Memory Considerations | 008 |
| C | Maximizing Local Autonomy | 008 |
| D | Converting C Programs | 002, 007 |

Per-requirement coverage: [traceability.md](traceability.md).

## Second-order sources

The manual repeatedly defers statement *syntax* to the SQL/MP Reference Manual.
This project therefore has a genuine gap: it specifies the **embedding** and
**host-language binding** completely, but the SQL grammar itself only to the
extent SQLPM/C describes it. Feature 001's plan resolves this by treating
non-directive statement bodies as opaque token streams to be translated by the
runtime, not parsed by the preprocessor. Where a statement's body must be
understood (`DECLARE CURSOR`, `SELECT … INTO`, host-variable extraction), the
grammar needed is minimal and specified in 001 and 004.
