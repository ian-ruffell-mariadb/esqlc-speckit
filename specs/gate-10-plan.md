# Gate 10 plan — the `SQLDA`

**Slice:** [specs/gate-10.md](gate-10.md) · **Specs:** 001, 002, 003, 005, 007 ·
**Planned under Principle VIII** (002, 005, 007 are `Clarifying`) · **Phase 3**

Slice conditions verified: enumerated subset (19 in-scope plus 7 carried),
avoidance table covering all 44 open questions across the five specs, four
provisional decisions, and a specific non-proof section.

## 1. Approach

**Generate the descriptor from the directive's own parameter list, at the count
the program asks for, and have the runtime write only the four fields the
manual gives it.**

Example 10-1 (§10 p.10-7) publishes the layout and p.10-3 publishes the
directive, and between them they correct the slice document.

**The `sqlvar` count is a directive parameter, not a fixed 1.** p.10-3:

```
INCLUDE SQLDA ( sqlda-name [ , sqlvar-count ] … )
```

and the worked examples pass it — `INCLUDE SQLDA (dummy_da, 1, dummy_namesbuf,
1)`, `INCLUDE SQLDA (osqlda, 1)`. Example 10-1 declares
`} sqlvar[sqlvar-count];`, a placeholder the generator fills in.

The slice document said the structure *"declares `sqlvar[1]`, not a fixed
array"*. That is wrong as stated, and the correction matters: **the program
chooses.** A count of `1` is idiomatic — p.10-29 calls it *"the SQLDA
template"* — precisely because p.10-30's `sizeof(SQLDA_TYPE) + ((n - 1) *
sizeof(SQLVAR_TYPE))` is then exact. A larger declared count still
*over*-allocates safely under that formula, so the arithmetic is not fragile;
it is only exact at 1. Corrected in the slice document in this change.

**The layout confirms `DIV-040` to the byte.** Example 10-1's `sqlvar` is four
`short`s and four `long`s: 8 + 16 = 24 on NonStop, matching the published
`SQLDA_SQLVAR_LEN`. Widened to real pointers it is 8 + 32 = 40, which is the
value FR-007.6 already states. `DIV-040` was reasoned before this page was read
and lands exactly.

**The runtime writes four fields and no others.** `DESCRIBE` fills `data_type`,
`data_len`, `precision`, `null_info`. FR-007.8 makes `eye_catcher` and `var_ptr`
the program's, and §10 p.10-59 has the program initialise `ind_ptr` too — *"even
when the program does not handle null values"*. So `DESCRIBE` reads
`num_entries` and writes into `sqlvar[i]`'s first four fields, leaving the rest
byte-identical, and `EXECUTE` refuses a NULL `var_ptr` rather than allocating
helpfully.

**The names buffer is a sibling array, not part of the structure.** Example 10-1
declares `char names_buffer[name-string-size];` alongside. Sizing follows
FR-007.7a's published formula, and `DESCRIBE` fills it with `VARCHAR`-shaped
entries — which is where a real divergence appears; see section 9.

## 2. Alternatives rejected

**A C99 flexible array member (`sqlvar[]`).** The modern spelling, and wrong
here: `sizeof(struct SQLDA_TYPE)` would then exclude the array entirely and
p.10-30's arithmetic would under-allocate by one entry. A customer program using
the published idiom would corrupt its heap.

**A fixed `sqlvar[16]` mirroring the `SQLSA`'s `stats[16]`.** Rejected: the
`SQLSA` has a documented cap of 16 tables and the `SQLDA` has no cap at all —
its capacity is whatever the program asks for. Guessing a maximum would make
`sizeof` wrong for every program that guessed differently.

**Runtime-allocated descriptors, as the `SQLCA` and `SQLSA` are registered.**
Rejected because §10's entire model is the program allocating: p.10-30 mallocs,
p.10-37 allocates at compile time, and FR-007.8 reserves fields *to* the
program. Registration would take ownership the manual gives away.

**Reusing `esqlc_stmt_exec` for `EXECUTE`.** Rejected: that entry point takes a
descriptor array the preprocessor built from source it could see, plus Gate 6's
table landmark. A dynamic statement's text is unknown at compile time, so
neither exists — and the descriptor is the program's `SQLDA` rather than a
generated array.

## 3. Components

| Component | Path | Change | Slice scope |
|-----------|------|--------|-------------|
| `SQLDA` emission | `src/pp/sqlda.cc` | **new** — Example 10-1's layout, at the directive's count, plus the assertion set | version 300+ |
| Directive parsing | `src/pp/emit.cc` | `INCLUDE SQLDA (name, count[, nb, nbsize])`; the `PREPARE`/`DESCRIBE`/`EXECUTE` handlers | numeric describe |
| Dispatcher | `src/pp/dispatch.cc` | four keywords implemented; `RELEASE`, `DESCRIBE INPUT`, `EXECUTE IMMEDIATE` keep `ESQLC-1012` | — |
| Shared types | `src/pp/pp.h` | the `SQLDA` layout constants | — |
| Runtime: dynamic | `src/rt/dynamic.c` | **new** — prepare, describe, execute, keyed by name | — |
| Runtime: offsets | `src/rt/rt_sqlda_offsets.h` | **new** — the layout the runtime writes by | — |
| Runtime: SQLSA | `src/rt/sqlsa.c` | populate the `prepare` arm | `output_num`, `output_names_len` |
| ABI header | `include/esqlc.h` | three new entry points | — |
| Contract | `specs/003-…/contracts/` | the same three, same change (Principle V) | — |
| Layout sync guard | `tests/harness/sqlda_layout_sync.sh` | **new** — emitter offsets vs runtime offsets | both directions |

Ten components, three new files.

**Stubs that must fail loudly.** `RELEASE` (FR-007.4), `DESCRIBE INPUT`
(FR-007.24), `EXECUTE IMMEDIATE`, dynamic `DECLARE CURSOR` (FR-007.5) and `CAST`
(FR-007.25) keep `ESQLC-1012` naming feature 007. A described **character**
column is refused with `ESQLC-7012` naming 002 Q7's missing `sqlh` — not
described with a guessed charset ID, which is the whole reason the slice is
numeric-only.

## 4. Runtime ABI surface

**Three new entry points** — the first growth since Gate 3, and for the same
reason: a prepared statement is long-lived state addressed by name across three
statements, which a one-shot entry point cannot express.

```c
/* Compile a statement held in a host variable and associate it with a name.
   The text is carried verbatim from the program's buffer (NFR-001.1); the
   runtime parameterises `?` markers (FR-007.22) and never rewrites the text. */
int esqlc_prepare(const char *name, const char *sql, size_t sql_len);

/* Fill one sqlvar per output column. `sqlda` is the PROGRAM's storage,
   allocated per §10 p.10-30, and `num_entries` its capacity — validated against
   what PREPARE reported (ESQLC-7002) and never reallocated.

   Writes data_type, data_len, precision and null_info. Leaves eye_catcher,
   var_ptr and ind_ptr byte-identical: FR-007.8 reserves the first two to the
   program, and §10 p.10-59 has it initialise ind_ptr even when it handles no
   nulls. `names_buf` receives VARCHAR-shaped column names, or NULL to skip. */
int esqlc_describe(const char *name, void *sqlda, int num_entries, int version,
                   char *names_buf, size_t names_len);

/* Run a prepared statement, binding through the descriptor's var_ptr fields.
   Refuses a NULL var_ptr (ESQLC-7003) rather than allocating helpfully. */
int esqlc_execute(const char *name, void *sqlda, int num_entries, int version);
```

`version` appears on two of the three for the reason `esqlc_sqlsa_register`
needed it: more than one layout is published, so the runtime cannot infer which
it was handed. Only 300+ is implemented here, but the parameter exists so v1/v2
do not force a signature change later.

## 5. Data structures

NFR-007.3 asks for `sizeof` **and** `offsetof` assertions on every field, which
is stricter than Gate 5's bounded set — and affordable, because a `sqlvar` has
eight fields rather than a 16-entry array. Emitted per `INCLUDE SQLDA`:

```c
#define SQLDA_EYE_CATCHER      "D1"
#define SQLDA_HEADER_LEN         4
#define SQLDA_SQLVAR_LEN        40    /* DIV-040; the published value is 24 */
#define SQLDA_NAMESBUF_OVHD_LEN 11
#define SQLDA_COLLBUF_OVHD_LEN   4

_Static_assert(sizeof(struct SQLVAR_TYPE) == SQLDA_SQLVAR_LEN, "sqlvar 40");
_Static_assert(offsetof(struct SQLVAR_TYPE, data_type) == 0,  "data_type leads");
_Static_assert(offsetof(struct SQLVAR_TYPE, data_len)  == 2,  "data_len");
_Static_assert(offsetof(struct SQLVAR_TYPE, precision) == 4,  "precision");
_Static_assert(offsetof(struct SQLVAR_TYPE, null_info) == 6,  "null_info");
_Static_assert(offsetof(struct SQLVAR_TYPE, var_ptr)   == 8,  "var_ptr");
_Static_assert(offsetof(struct SQLVAR_TYPE, ind_ptr)   == 16, "ind_ptr");
_Static_assert(offsetof(struct SQLVAR_TYPE, cprl_ptr)  == 24, "cprl_ptr");
_Static_assert(offsetof(struct SQLVAR_TYPE, reserved)  == 32, "reserved is last");
_Static_assert(offsetof(struct SQLDA_TYPE, sqlvar) == SQLDA_HEADER_LEN,
               "sqlvar follows the 4-byte header");
```

The `reserved` assertion is the one worth having: FR-007.6b says programs *"must
not assume the entry ends after `cprl_ptr`"*, and it exists in the published
layout only as an undocumented fourth address field. Asserting its offset is
what stops a future edit reclaiming those eight bytes.

**No packing attribute, deliberately** — unlike the `SQLSA`. Four `short`s then
four 8-byte pointers align naturally to 40 with no padding, so the total is
reached without one. The assertions prove that rather than assume it, and if a
target ever needs packing the assertion fails first.

The runtime addresses the same layout by offset in `rt_sqlda_offsets.h`, which
is the `SQLSA`'s drift hazard again — hence `sqlda_layout_sync.sh`, built on the
same shape as `sqlsa_layout_sync.sh`.

## 6. Requirement → component map

| Requirement | Component(s) | Test |
|-------------|--------------|------|
| NFR-001.1 opaque bodies | emit | `opaque_body_unchanged` |
| FR-002.9 integer widths | pp/sqlda | `sqlda_datatypes` |
| FR-003.1 `esqlc_*` calls only | emit | `abi_only_symbols` |
| FR-003.2 no MariaDB type in the header | include/esqlc.h | `abi_isolation` |
| FR-003.3 signatures mirrored in the contract | contract | `contract_sync` |
| FR-005.18 the `SQLSA` `prepare` arm | rt/sqlsa | `rt/sqlda_prepare_arm` |
| NFR-002.2 host variable widths asserted | emit | `sqlda_prepare_hostvar` |
| FR-007.1 `PREPARE` from a host variable | dispatch, rt/dynamic | `rt/sqlda_prepare_describe` |
| FR-007.2 `EXECUTE` | dispatch, rt/dynamic | `rt/sqlda_execute` |
| FR-007.3 `DESCRIBE` one entry per column | rt/dynamic | `rt/sqlda_prepare_describe` |
| FR-007.6 the five constants | pp/sqlda | `sqlda_constants` |
| FR-007.6a the `sqlvar` field order | pp/sqlda | `sqlda_layout` |
| FR-007.6b `reserved` present and preserved | pp/sqlda, rt/dynamic | `sqlda_layout`, `rt/sqlda_reserved` |
| FR-007.7 the 11-byte overhead's composition | pp/sqlda | `sqlda_constants` |
| FR-007.7a the buffer size formulas | pp/sqlda | `sqlda_buffers` |
| FR-007.8 program-owned fields never written | rt/dynamic | `rt/sqlda_untouched` |
| FR-007.9 `num_entries` is capacity | rt/dynamic | `rt/sqlda_capacity` |
| FR-007.11a `data_len` packing, binary numeric | rt/dynamic | `rt/sqlda_datalen`, `sqlda_datalen_unit` |
| FR-007.13 `precision` for binary numeric | rt/dynamic | `rt/sqlda_precision` |
| FR-007.14 `null_info` negative for nullable | rt/dynamic | `rt/sqlda_nullinfo` |
| FR-007.15 `ind_ptr` and the null flag | rt/dynamic | `rt/sqlda_execute` |
| FR-007.18 the numeric `data_type` codes | rt/dynamic | `rt/sqlda_datatypes` |
| FR-007.22 `?` input parameters | rt/dynamic | `rt/sqlda_execute` |
| FR-007.23 sizes from the `SQLSA` | rt/sqlsa, rt/dynamic | `rt/sqlda_prepare_arm` |
| FR-007.26 version 300+ layouts | pp/sqlda | `sqlda_constants` |
| NFR-007.3 `sizeof` and `offsetof` per field | pp/sqlda | `sqlda_layout`, `sqlda_layout_sync` |

**26 requirements, all mapped exactly once. Zero unmapped.**

## 7. Test strategy

**Tier 1.** The whole layout obligation, without a server: the constants, every
field's offset, `sqlvar` at the directive's count, and the emitter-versus-runtime
offset comparison. Plus a **pure unit test** for `data_len` packing —
NFR-007.2 asks for one *independent of a database*, and SD-17's bit order is
exactly the kind of thing a round-trip can hide by encoding and decoding with
the same wrong convention. The unit test takes an integer and a scale and
asserts the 16-bit result, so encode and decode cannot agree on a mistake.

**Tier 2.** `PREPARE` → `DESCRIBE` → allocate → `EXECUTE` → compare, over three
numeric columns; the `SQLSA`'s `prepare` arm populated; and the four refusals.
The fixture that matters most is `rt/sqlda_untouched`: it writes recognisable
sentinels into `eye_catcher`, `var_ptr` and `ind_ptr` before `DESCRIBE` and
asserts all three survive byte-identical, which is the only way FR-007.8 is
observable.

**Mutation, Phase D′.** Swap SD-17's bit order and the `data_len` unit test must
fail; report `precision` in bits rather than digits and `sqlda_precision` must
fail; have `DESCRIBE` write `var_ptr` and `sqlda_untouched` must fail; emit
`sqlvar[]` as a flexible member and the `malloc`-arithmetic fixture must fail;
shift a runtime offset and `sqlda_layout_sync` must fail; skip the `num_entries`
check and `sqlda_capacity` must fail.

The flexible-member mutation is the one to watch: the unit still compiles, every
field still reads correctly, and only the *allocation size* is wrong by one
entry — a heap overflow that appears at a customer's scale and not at a
fixture's.

## 8. Risks

**The names buffer's 11-byte overhead assumes an 8-character table name.**
FR-007.7 says the 11 comprises *"the length (2 bytes), table name (8 bytes), and
period separator (1 byte)"*, which is a Guardian file name. MariaDB identifiers
run to 64 characters, so a name qualified with a real table name overflows the
per-entry budget the formula reserves. This is a genuine divergence and section 9
registers it — but it also means `DESCRIBE`'s names-buffer population has to
decide what to do with a long table name before any fixture can pass.

**`ESQLC-7010` is a warning about something the runtime cannot reliably see.**
"Eye-catcher not initialised by the program" assumes an uninitialised value is
distinguishable from a valid one. `"D1"` is two bytes; uninitialised stack
memory can hold `"D1"` by accident, and a program that zeroes its structure
gives `"\0\0"`, which is detectable. So the check is sound for zeroed memory and
unsound for garbage, and it is a warning rather than an error for exactly that
reason. Worth stating so it is not later "fixed" into an error.

**`num_entries` is a `short`, and the capacity check reads it from program
memory.** A program that sets it wrong — or not at all — makes the runtime trust
a number that decides how many 40-byte entries it writes. `ESQLC-7002` compares
it against `PREPARE`'s count, which catches *too small*; a `num_entries` larger
than the allocation is undetectable and would overflow the program's own buffer.
That asymmetry is inherent to the manual's model and cannot be closed here.

**`DIV-040`'s widening changes `sizeof(struct SQLVAR_TYPE)` from 24 to 40, and
p.10-30's arithmetic uses `sizeof`.** So a conforming program is safe and a
program with a hard-coded 24 under-allocates by 40% — silently, because the
first entries are written correctly and only the last overflow. This slice is
the first opportunity to check `DIV-040`'s migration note actually says that,
and it does not yet.

**Only `output_num` and `output_names_len` of the `prepare` arm are populated.**
The arm has six fields; `input_num`, `input_names_len` and `name_map_len` need
`DESCRIBE INPUT`, which is out of scope, and `sql_statement_type` needs
FR-005.24's published values 1–8, also out. So Gate 5's arm goes from wholly
sentinel to two-thirds sentinel, which is progress and not completion.

## 9. Divergences introduced

**One new: `DIV-057` — the names buffer's 8-byte table-name budget.**
FR-007.7's 11-byte overhead is `2 + 8 + 1`: a `VARCHAR` length, a Guardian table
name, and a period. MariaDB identifiers are up to 64 characters, so a
fully-qualified name does not fit the budget the published formula reserves.
Detection: a column from a table whose name exceeds 8 characters. The choice —
truncate the table qualifier, omit it, or refuse — is a slice decision the plan
makes at implementation and records, because all three are visible to a program
reading the buffer.

**`DIV-040` gains a note, not an amendment.** Its arithmetic is confirmed exactly
by Example 10-1 — four `short`s and four `long`s is 24 published and 40 widened
— and its **migration** field should say explicitly that p.10-30's `sizeof`
idiom is safe while a hard-coded 24 under-allocates. That is the difference
between a program that works and one that corrupts its heap, and the entry
currently leaves it implicit.

**The slice document is corrected** on `sqlvar[1]`: the count is a directive
parameter (p.10-3), `1` is idiomatic for the `malloc` template (p.10-29), and a
larger count over-allocates safely rather than breaking. Not a divergence; a
plan-stage correction.
