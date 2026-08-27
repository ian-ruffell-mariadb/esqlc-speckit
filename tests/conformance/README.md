# Conformance suite

Layout is fixed by the specs; directories are created as features are
implemented.

```
001/                    preprocessor golden-file cases
001/negative/           expected-diagnostic cases
002/ … 008/             one directory per feature
008/policy_matrix/      one file per NonStop construct, per AS-008.1
stub/                   the no-database runtime stub (feature 001, T004)
fixtures/               App. A sample schema + seed data (feature 006, NFR-006.1)
```

Two runners, both required before any implementation task starts (001 tasks
T002, T003):

| Runner | Input | Assertion |
|---|---|---|
| golden-file | `X.sqlc` | emitted C matches `X.expected.c` after whitespace normalisation |
| negative | `X.sqlc` | diagnostics match `X.expected.diag` on **code, line, and column** |

A negative test that produces the right diagnostic code at the wrong source
position fails. Positions are the whole point — see Constitution III.

Runtime conformance tests need a live MariaDB and the App. A fixture schema.
Preprocessor tests must run green with no database present (NFR-001.2).
