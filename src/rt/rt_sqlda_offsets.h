/* T1081 — the runtime's view of the SQLDA layout.
 *
 * The descriptor is the PROGRAM's storage, so the runtime addresses it by
 * offset. That makes the layout encoded twice — once as a C struct in
 * src/pp/sqlda.cc, once as these constants — which is the drift the SQLSA had.
 * tests/harness/sqlda_layout_sync.sh compares them. Do not edit without it.
 *
 * DIV-040 widens the four address fields to hold real pointers: 24 published,
 * 40 here. DIV-058 is the consequence — that widening raises SQLVAR_TYPE's
 * alignment from 4 to 8, so sqlvar begins at 8 rather than at the published
 * SQLDA_HEADER_LEN of 4. Both values are real and both are asserted in the
 * generated declaration.
 */
#ifndef ESQLC_RT_SQLDA_OFFSETS_H
#define ESQLC_RT_SQLDA_OFFSETS_H

#define SQLDA_OFF_EYE          0
#define SQLDA_OFF_NUM_ENTRIES  2
#define SQLDA_HDR_FIELDS_LEN   4    /* Table 10-2's published value */
#define SQLDA_OFF_SQLVAR       8    /* DIV-058: not 4 — alignment */
#define SQLDA_SQLVAR_STRIDE   40    /* DIV-040: not the published 24 */

/* Within a sqlvar. Example 10-1's order. */
#define SQLVAR_OFF_DATA_TYPE   0
#define SQLVAR_OFF_DATA_LEN    2
#define SQLVAR_OFF_PRECISION   4
#define SQLVAR_OFF_NULL_INFO   6
#define SQLVAR_OFF_VAR_PTR     8    /* the PROGRAM's — never written */
#define SQLVAR_OFF_IND_PTR    16    /* the PROGRAM's — never written */
#define SQLVAR_OFF_CPRL_PTR   24    /* out of slice; never written */
#define SQLVAR_OFF_RESERVED   32    /* FR-007.6b: preserved, never written */

/* Table 10-4's published data_type values. The *names* come from the sqlh file
 * the project does not have; the *values* are published here, which is why
 * FR-007.18 is implementable while FR-007.20's charset IDs are not (002 Q7).
 * Only the numeric family is in Gate 10's scope. */
#define SQLDT_16BIT_S  130
#define SQLDT_16BIT_U  131
#define SQLDT_32BIT_S  132
#define SQLDT_32BIT_U  133
#define SQLDT_64BIT_S  134          /* no unsigned counterpart is published */
#define SQLDT_REAL     140
#define SQLDT_DOUBLE   141

typedef struct { const char *field; unsigned off; } esqlc_sqlda_off_t;
const esqlc_sqlda_off_t *esqlc_rt_sqlda_offsets(int *n);

/* SD-17 — data_len packs scale in bits 0:7 and byte length in bits 8:15
 * (FR-007.11a). The manual gives the bit positions without saying which end is
 * bit 0, so this reads 0:7 as the low-order byte. Split out as pure functions
 * so a unit test can assert encode and decode SEPARATELY: a round trip would
 * pass with both halves wrong in the same direction. */
static inline int esqlc_datalen_pack(int bytes, int scale) {
    return ((bytes & 0xFF) << 8) | (scale & 0xFF);
}
static inline int esqlc_datalen_bytes(int packed) { return (packed >> 8) & 0xFF; }
static inline int esqlc_datalen_scale(int packed) { return packed & 0xFF; }

#endif
