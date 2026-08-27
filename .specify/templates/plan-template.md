# Technical Plan: [NAME]

**Spec:** `specs/NNN-slug/spec.md` · **Status:** Draft | Approved | Superseded

## 1. Approach

The chosen design in three paragraphs. State the decision, not the survey.

## 2. Alternatives rejected

| Alternative | Why rejected |
|-------------|--------------|
| … | … |

## 3. Components

| Component | Path | Responsibility |
|-----------|------|----------------|
| … | `src/…` | … |

## 4. Runtime ABI surface

New or changed `esqlc_*` entry points, per Constitution V. Every signature here
must also land in `specs/003-runtime-mariadb-binding/contracts/`.

```c
/* … */
```

If this feature adds no ABI surface, say so explicitly.

## 5. Data structures

Layouts, sizes, and the static assertions that pin them (Constitution VI).

## 6. Requirement → component map

| Requirement | Component(s) | Test |
|-------------|--------------|------|
| FR-NNN.1 | … | … |

Every `FR`/`NFR` in the spec appears exactly once. A requirement with no row is
a planning defect.

## 7. Test strategy

- Golden-file preprocessor cases: …
- Runtime conformance cases: …
- Negative / diagnostic cases: …
- Fixtures required (schema, sample data): …

## 8. Risks

| Risk | Impact | Mitigation |
|------|--------|------------|
| … | … | … |

## 9. Divergences introduced

`DIV-nnn` entries this plan creates. Each must already exist in
`docs/divergences.md`.
