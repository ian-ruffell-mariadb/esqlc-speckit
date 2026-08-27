# Feature Spec: [NAME]

**ID:** NNN-slug · **Status:** Draft | Clarifying | Ready | In progress | Done
**Manual coverage:** [sections and page ranges this feature is responsible for]
**Depends on:** [feature IDs]

## 1. Problem

What in the manual does this feature make work, and what breaks in a customer
program if it does not exist? Two paragraphs maximum. No solution language.

## 2. Scope

**In scope**
- …

**Out of scope** (with the feature that owns it, or "not planned")
- …

## 3. Requirements

Every requirement: `FR-NNN.n` (functional) or `NFR-NNN.n` (non-functional), a
single testable assertion, and a citation. Mark `[EXTERNAL]` where the manual
defers to another HP manual, `[DIV-nnn]` where behaviour intentionally differs.

| ID | Requirement | Citation |
|----|-------------|----------|
| FR-NNN.1 | … | `[SQLPM/C §n p.n-n]` |

## 4. Acceptance scenarios

Given / When / Then, one per requirement group. Each scenario must be
mechanically checkable by the conformance suite — name the test file.

### AS-NNN.1 — [name]
- **Given** …
- **When** …
- **Then** …
- **Test:** `tests/conformance/…`

## 5. Diagnostics

Everything this feature must refuse or warn about, per Constitution III.

| Code | Condition | Default policy | Citation |
|------|-----------|----------------|----------|
| `ESQLC-nnnn` | … | error / warn / ignore | … |

## 6. Open questions

| # | Question | Blocks | Resolution |
|---|----------|--------|------------|
| Q1 | … | FR-NNN.n | unresolved |

Questions must be resolved or converted to registered divergences before status
becomes Ready.

## 7. Constitution check

| Principle | Compliant? | Note |
|-----------|-----------|------|
| I manual is the contract | | |
| II source compatibility | | |
| III no silent semantic change | | |
| IV manual-derived tests first | | |
| V layered / frozen ABI | | |
| VI byte-exact structures | | |
| VII divergence registered | | |
