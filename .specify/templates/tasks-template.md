# Tasks: [NAME]

**Spec:** `specs/NNN-slug/spec.md` · **Plan:** `specs/NNN-slug/plan.md`

Rules:
- Tests before implementation, per Constitution IV. A `T`-task that writes code
  must name the already-failing test it makes pass.
- `[P]` marks tasks with no ordering dependency on each other — safe to run in
  parallel.
- Each task names its requirement IDs. A task with none is scope creep.

## Phase A — Fixtures and harness

| ID | Task | Reqs | Deps |
|----|------|------|------|
| T001 | … | — | — |

## Phase B — Failing tests

| ID | Task | Reqs | Deps |
|----|------|------|------|
| T010 | … | FR-NNN.1 | T001 |

## Phase C — Implementation

| ID | Task | Reqs | Makes pass | Deps |
|----|------|------|-----------|------|
| T020 | … | FR-NNN.1 | T010 | T010 |

## Phase D — Diagnostics

One task per row of the spec's diagnostics table.

## Phase E — Documentation and registry

| ID | Task | Reqs | Deps |
|----|------|------|------|
| T090 | Update `docs/traceability.md` for the sections this feature closes | — | Phase C |
| T091 | Add/confirm `DIV-nnn` entries in `docs/divergences.md` | — | Phase C |

## Exit criteria

- [ ] Every spec requirement has a passing test
- [ ] Every diagnostic has a negative test that fires it
- [ ] `docs/traceability.md` rows for this feature's sections are no longer `—`
- [ ] No unresolved spec open questions
- [ ] `/speckit.analyze` clean
