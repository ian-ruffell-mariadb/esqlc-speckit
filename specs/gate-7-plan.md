# Gate 7 plan — host variable type breadth

**Slice:** [specs/gate-7.md](gate-7.md) · **Specs:** 001, 002, 003, 004, 005 ·
**Planned under Principle VIII** (002, 004, 005 are `Clarifying`)

Slice conditions verified: enumerated subset (8 in-scope plus 17 carried),
avoidance table covering all 35 open questions, four provisional decisions, and
a specific non-proof section.

## 1. Approach

**Take every host variable's width from `sizeof` of the C type the program
actually declared, bind a `VARCHAR` structure through its own address with the
layout asserted, and treat the wider integers as untested rather than unbuilt.**

Two checks against the code changed this plan before it was written.

**A hand-declared `long` host variable does not compile today.** `decl.cc` maps
`long` to width 4, following `DIV-001`'s reading that NonStop `long` is 32-bit,
and the emitter then asserts `sizeof(long) == 4`, which fails on any LP64 host:

```
error: static assertion failed due to requirement 'sizeof (big) == 4'
```

That is `NFR-002.2` earning its place — the assertion caught a descriptor
claiming four bytes of an eight-byte variable, which would otherwise have bound
the low half silently. But it also means `DIV-001`'s **Detection** field is
wrong where it says programs that spell out `long` themselves *"are unaffected
in width"*. They are affected, and they do not build.

The resolution is that width comes from the **host compiler's** `sizeof`, not
from the NonStop mapping. `DIV-001`'s advice to use width-exact types is
guidance for *generated* declarations — `INVOKE`, feature 006 — not for what a
program hand-writes. A hand-written `long` is 64 bits here, the divergence from
NonStop's 32-bit `long` is exactly what `DIV-001` already accepts, and the
descriptor must describe the variable that exists rather than the one the
manual assumed.

**`int` and 32-bit binding may already work.** `decl.cc` maps `int` to width 4
and `exec.c` already binds widths 2, 4 and 8. Nothing in six gates has ever
exercised it. So part of this slice is proving what is there, which is worth
saying plainly rather than discovering halfway through: the traceability note
*16-bit only* describes the test coverage, not the code.

**A `VARCHAR` structure binds through its own address.** `FR-002.6` fixes the
member order and names — `short len` then `char val[]` — so `addr` points at
the structure and the runtime reaches `len` at offset 0 and `val` at offset 2.
That is the published mapping rather than a guess, and the emitter asserts it
per variable so a drift breaks the build.

This is what keeps the ABI still. A second address in `esqlc_hostvar_t` would
be the obvious alternative and it does not fit: the descriptor is 33 bytes
padded to 40 and `_Static_assert(sizeof <= 40)` has held since Gate 1, so
another pointer would push it to 48 and break the one assertion that has
guarded the layout all along.

## 2. Alternatives rejected

**Keep `long` at width 4 to match NonStop.** Rejected: it does not compile, and
if the assertion were removed it would bind four bytes of an eight-byte
variable — correct for small values on a little-endian host and wrong in a way
no test would notice.

**Emit `int32_t` in place of the program's `long`.** This is what `DIV-001`
prescribes for generated code, and it is a source change to customer code,
which Principle II forbids. The preprocessor describes declarations; it does
not rewrite them.

**Add a second address to the descriptor for `VARCHAR`.** Rejected on the
40-byte assertion above. Reaching `val` at a known offset from the structure's
address costs nothing and keeps the interface frozen.

**Two descriptors per `VARCHAR`, one for `len` and one for `val`.** Rejected:
it would break the one-placeholder-per-descriptor invariant `FR-001.16` asserts,
and a `VARCHAR` is one parameter.

**Let the runtime discover the struct layout from metadata.** There is no
metadata for a host variable — it is a C declaration, not a result column.

## 3. Components

| Component | Path | Change | Slice scope |
|-----------|------|--------|-------------|
| Declaration parser | `src/pp/decl.cc` | width from `sizeof`; `long long`; `float`/`double`; `VARCHAR` struct recognition | the eight in-scope rows |
| Shared types | `src/pp/pp.h` | `HostVar` gains the `val` capacity for a `VARCHAR` | — |
| Emitter | `src/pp/emit.cc` | `VARCHAR` descriptors and layout assertions; float/double descriptors | — |
| Runtime: exec | `src/rt/exec.c` | bind `float`/`double`; bind `VARCHAR` via the struct address; map 1264 to 8300 | — |
| ABI header | `include/esqlc.h` | **no change** | — |
| Schema | `tests/conformance/gate-1/schema.sql` | a typed table: `INTEGER`, `BIGINT`, `REAL`, `DOUBLE`, `VARCHAR`, `TIMESTAMP` | — |
| Harness | `tests/harness/run_tier2.sh` | the Gate 7 cases | — |

Seven components, no new source files, **no ABI surface**.

**Stubs that must fail loudly.** `DECIMAL`, `SETSCALE`, C `fixed`, `TYPE AS`,
`INTERVAL` and any `CHARACTER SET` clause keep their `ESQLC-1012` refusal naming
feature 002. A structure that is not the `VARCHAR` shape is refused under
`FR-002.20` rather than bound as its first member — the failure mode where a
program passes a record and gets its first field silently.

## 4. Runtime ABI surface

**No new entry points and no signature change.** The first slice since Gate 2
to add nothing.

Three declared-but-unused constants become live:

| Constant | Status after this slice |
|---|---|
| `ESQLC_T_CHAR_VAR` (3) | **used** — `VARCHAR` structures |
| `ESQLC_T_FLOAT` (5) | **used** — `float` and `double`, distinguished by `width` |
| `ESQLC_T_DATETIME` (6) | **still unused**, deliberately |

`ESQLC_T_FLOAT` carries both C types, with `width` 4 or 8 separating them —
the same by-width dispatch the integer family already uses, rather than a
second constant.

`ESQLC_T_DATETIME` stays unused because `FR-002.13` binds a date-time *column*
into a *character* host variable, so the descriptor is `ESQLC_T_CHAR_FIXED`.
Reaching for the date-time constant is the obvious wrong move and a test pins
against it.

## 5. Data structures

The `VARCHAR` structure is a layout the runtime addresses by offset, so
Principle VI applies in full. Emitted per `VARCHAR` host variable:

```c
_Static_assert(offsetof(struct <tag>, len) == 0,  "VARCHAR len must lead");
_Static_assert(offsetof(struct <tag>, val) == 2,  "VARCHAR val follows len");
_Static_assert(sizeof(((struct <tag> *)0)->len) == 2, "VARCHAR len must be short");
_Static_assert(sizeof(<name>) >= 3, "VARCHAR structure too small");
```

The `len` size assertion is `FR-002.21` made structural as well as diagnosed:
p.2-9 says to declare it `short` and not `int`, and the parser rejects `int`,
but the assertion catches a typedef that resolves to something else.

No other layout is introduced. `esqlc_hostvar_t` is untouched, and its
`sizeof <= 40` and `offsetof(addr) == 0` assertions continue to hold.

## 6. Requirement → component map

| Requirement | Component(s) | Test |
|-------------|--------------|------|
| NFR-001.1 opaque bodies | emit | `opaque_body_unchanged` |
| FR-002.1 declare-section scope | decl | `types_declare_section` |
| FR-002.2 any C identifier | decl | `types_declare_section` |
| FR-002.3 `CHAR(l)` → `char v[l+1]` | decl | `insert` (carried) |
| FR-002.6 `VARCHAR` → `short len` + `char val[]` | decl, emit | `varchar_layout`, `rt/varchar_roundtrip` |
| FR-002.9 integer widths 16/32/64 | decl | `int_widths`, `rt/int_widths_roundtrip` |
| FR-002.10 `float` / `double` | decl, rt/exec | `float_widths`, `rt/float_roundtrip` |
| FR-002.12 `unsigned long long` refused | decl | `negative/unsigned_long_long` |
| FR-002.13 date-time → `char` | rt/exec | `rt/timestamp_to_char` |
| FR-002.15 indicator association | emit | `update_indicator_assoc` (carried) |
| FR-002.16 negative indicator means null | rt/exec | `rt/varchar_null` |
| FR-002.20 only a `VARCHAR` structure is usable | decl | `negative/struct_not_varchar` |
| FR-002.21 `len` must be `short`, not `int` | decl, emit | `negative/varchar_len_int`, `varchar_layout` |
| FR-002.22 conversion within families | rt/exec | `rt/negative/cross_family` (carried) |
| FR-002.25 too-large input → 8300 | rt/exec | `rt/int_overflow_8300` |
| FR-002.28 no terminator on retrieval | rt/exec | `rt/timestamp_to_char` |
| FR-002.30 `width` bytes bound verbatim | rt/exec | `rt/varchar_roundtrip` |
| FR-002.31 padding is the program's job | rt/exec | `underfilled_stores_null` (carried) |
| FR-003.1 `esqlc_*` calls only | emit | `abi_only_symbols` |
| FR-003.2 no MariaDB type in the header | include/esqlc.h | `abi_isolation` |
| FR-003.3 signatures mirrored in the contract | contract | `contract_sync` |
| FR-003.10 values bound, never interpolated | emit | `int_widths` |
| NFR-002.1 a round-trip per mapping row | schema, harness | `rt/int_widths_roundtrip`, `rt/float_roundtrip`, `rt/varchar_roundtrip` |
| NFR-002.2 widths asserted statically | emit | `int_widths`, `varchar_layout` |
| NFR-003.2 no interpolation ever | emit | `rt/update_injection_literal` (carried) |

**25 requirements, all mapped exactly once. Zero unmapped.**

`NFR-002.1` is mapped but partial by design: it asks for a round-trip per
mapping row, and the rows still out of scope have no test because they have no
implementation. The slice document states this.

## 7. Test strategy

**Tier 1.** Widths and layouts, all without a server: each integer type's
descriptor width, `float` against `double`, the `VARCHAR` offset assertions, and
the four refusals. The strongest check here is that **emitted C compiles** — a
descriptor whose width disagrees with `sizeof` fails the static assertion, which
is how the `long` defect surfaced in the first place.

**Tier 2.** Round-trips, because `NFR-002.1` asks for them and because a width
that is wrong outbound can look right inbound. Plus `int_overflow_8300`, which
is the only fixture that proves the widening has a boundary.

**Mutation, Phase D′.** Map `long long` to width 4 and `int_widths` must fail;
bind `MYSQL_TYPE_LONG` for a `double` and `float_roundtrip` must fail; read
`val` at offset 0 instead of 2 and `varchar_roundtrip` must fail; accept an
`int` length field and `varchar_len_int` must fail; report 1264 unmapped and
`int_overflow_8300` must fail.

The `val` offset mutation is the one to watch. Reading `val` at offset 0 yields
the length bytes as the first two characters of the string — plausible-looking
data, not a crash — which is the failure class that survived in Gates 5 and 6.

## 8. Risks

**`long` changes width, and that is a source-visible change even though no
source changes.** A NonStop program using `long` for an `INTEGER` column got 32
bits; here it gets 64. Values in range behave identically, values that relied on
32-bit wraparound do not, and `%ld` format strings are unaffected because the
C type is unchanged. This is `DIV-001` operating as accepted, but its
**Detection** field currently claims the hand-declared case is unaffected, and
that has to be corrected rather than left as a comfortable inaccuracy.

**MariaDB's out-of-range is a hard error only under `STRICT_TRANS_TABLES`.**
Verified as error 1264, SQLSTATE 22003, on the test server — but a server
without strict mode *truncates and warns* instead, and then `FR-002.25`'s 8300
would silently become a stored wrong value. The runtime already appends to
`@@sql_mode` for `DIV-052`; whether it must also assert strict mode is a
decision this slice has to make, not inherit.

**`float` equality is not a test.** Round-tripping a `double` through a
`DOUBLE PRECISION` column is exact in principle and fragile in practice.
Fixtures compare within a tolerance, and a tolerance that is too loose proves
nothing — the values are chosen so that a wrong *width* (binding a `float` as a
`double` or vice versa) changes the result far beyond any tolerance.

**A `VARCHAR` structure's `val` offset is 2 by convention, not by standard.**
Every ABI this project targets puts a `char` array immediately after a `short`,
but nothing in C guarantees it. The assertion is the guard, and it must be
emitted per variable rather than assumed once — which is why it is in section 5
and not a comment.

**`int` may already work, which makes its test a regression guard rather than a
proof of new behaviour.** Worth stating so nobody reads a green
`int_widths_roundtrip` as evidence the slice built something it did not.

## 9. Divergences introduced

**`DIV-001` is amended, not joined.** Its **Detection** field says programs
that hand-declare `long` *"are unaffected in width"*. They are affected: on
LP64 the variable is eight bytes where NonStop's was four, and until this slice
the descriptor claimed four and the unit did not compile. The field is corrected
and the hand-declared case is stated explicitly.

**One new: `DIV-054` — error 8300 without its file-system detail.**
`FR-002.25` pairs SQL error 8300 with a file-system detail of 1031. MariaDB
reports 1264 with no equivalent detail, so the `sqlcode` is faithful and
`SQLCAFSCODE` returns a sentinel rather than 1031. Detection: an out-of-range
insert sets `sqlcode` to -8300 and `esqlc_fs_detail` reports the sentinel.
Migration: a program that branches on `sqlcode` is unaffected; one that branches
on the file-system detail needs review.
