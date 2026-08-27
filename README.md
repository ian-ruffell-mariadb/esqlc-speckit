# esqlc-speckit

A spec-driven development kit for **ESQL/C on MariaDB** — an embedded-SQL C
preprocessor and runtime library that is source-compatible with HP NonStop
SQL/MP's embedded SQL interface for C.

## What this repo is

This repo contains **no implementation**. It contains the specifications,
constitution, traceability, and phased task plans from which the implementation
is built. The behavioural contract is the manual:

> *HP NonStop SQL/MP Programming Manual for C*, part number 429847-008,
> August 2012 (331 pp.) — see [docs/source-manual.md](docs/source-manual.md).

The manual is **not vendored** in this repo (it is HP copyright). Fetch it
locally with:

```bash
./.specify/scripts/fetch-manual.sh
```

## Goal

An unmodified `.sqlc` source file written against NonStop SQL/MP — declare
sections, `EXEC SQL` statements, `WHENEVER` directives, `INVOKE`, `SQLCA` /
`SQLSA` / `SQLDA`, static and dynamic SQL — preprocesses and links against
MariaDB, and behaves observably the same, or fails loudly where it cannot.

"Fails loudly where it cannot" is a first-class requirement, not a fallback.
See Principle III in [the constitution](.specify/memory/constitution.md).

## Layout

```
.specify/memory/constitution.md   Non-negotiable project principles
.specify/templates/               spec / plan / tasks templates
.specify/scripts/                 manual fetch + text extraction + feature scaffold
.claude/commands/                 /speckit.* slash commands
docs/source-manual.md             Provenance, citation convention, re-extraction
docs/reference/                   Distilled normative reference sheets
docs/reference/ansi-conformance.md  Gap analysis vs ISO/IEC 9075-5:1999
docs/traceability.md              Manual section -> spec coverage matrix
docs/divergences.md              Register of accepted behavioural divergences
specs/NNN-*/spec.md              Feature specs (what + why, testable)
specs/NNN-*/plan.md              Technical plan (how)
specs/NNN-*/tasks.md             Ordered, dependency-aware task list
tests/conformance/                Conformance suite layout (see 001 plan)
ROADMAP.md                        Phase order and gating
```

## The eight features

| # | Feature | Manual coverage |
|---|---------|-----------------|
| [001](specs/001-preprocessor-core/spec.md) | Preprocessor core & pipeline | §3, §6 (pragma/placement/listings) |
| [002](specs/002-host-variables/spec.md) | Declare sections & host-variable type mapping | §2, App. D |
| [003](specs/003-runtime-mariadb-binding/spec.md) | Runtime library & MariaDB binding | §7, transaction control in §3 |
| [004](specs/004-static-dml-cursors/spec.md) | Static DML & cursors | §4 |
| [005](specs/005-diagnostics/spec.md) | `sqlcode`, `SQLCA`, `SQLSA`, `WHENEVER` | §9, §5 |
| [006](specs/006-invoke-schema-gen/spec.md) | `INVOKE` schema-derived structures | §2 (INVOKE), App. A |
| [007](specs/007-dynamic-sql/spec.md) | Dynamic SQL & `SQLDA` | §10, App. D |
| [008](specs/008-nonstop-compat-surface/spec.md) | NonStop compatibility surface | §5, §6, §7, §8, §11, App. B, App. C |

Phase order and gating: [ROADMAP.md](ROADMAP.md).

## Workflow

```bash
./.specify/scripts/new-feature.sh "short feature name"
```

Then in Claude Code:

```
/speckit.specify   # fill in spec.md from the manual, cite every requirement
/speckit.plan      # derive plan.md
/speckit.tasks     # derive tasks.md
/speckit.analyze   # cross-check spec/plan/tasks against the constitution
/speckit.implement # execute tasks.md
```

Every requirement carries a manual citation. Requirements without one are
rejected at `/speckit.analyze` — see Principle I.
