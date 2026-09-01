// T560-T564 — SQLSA layout emission.
//
// The inverse of the SQLCA. DIV-041 made that structure an opaque blob because
// the manual publishes no layout for it, so nothing could be conforming to.
// This layout *is* published, twice, in §9 pp.9-15..9-16, and programs index
// its fields by name — `sqlsa.u.dml.stats[0].records_used` is source a customer
// writes. So it is emitted as real named fields, and Principle VI applies with
// no escape.
//
// Two facts about the published declarations, both established by compiling
// them rather than by reading them:
//
//   The manual's `long` is 32-bit TNS. Emitting the declarations verbatim on an
//   LP64 host gives an 8-byte long and a structure nowhere near 838, so every
//   integer field is emitted as an explicit intN_t.
//
//   Both families need packing, not just v330. FR-005.27 records the alignment
//   pragma for the four *_R330 types, which reads as though v300 needs none:
//
//        v300   natural 840   packed 838   published 838
//        v330   natural 1864  packed 1790  published 1790
//
//   v300 misses by exactly the two bytes a compiler inserts after num_tables
//   before the 4-aligned stats[]. FR-005.27 understates its own requirement;
//   raised as a finding against 005 rather than silently implemented.
//
// Both type declarations are emitted regardless of the selected version. The
// manual names them distinctly, and FR-005.26 has VERSION CURRENT generate
// both, so this is the manual's own model rather than an invention. Only the
// variable takes the selected version.
#include "pp.h"
#include <sstream>

namespace pp {

// Kept in step with src/rt/rt_sqlsa_offsets.h by tests/harness/sqlsa_layout_sync.sh.
// That harness is not optional: this file and the runtime encode one layout
// twice, and nothing else would notice them drifting apart.
static const char *kStats300 =
    "struct STATS_TYPE {\n"
    "    char    table_name[24];\n"
    "    int32_t records_accessed;\n"
    "    int32_t records_used;\n"
    "    int32_t disc_reads;\n"
    "    int32_t messages;\n"
    "    int32_t message_bytes;\n"
    "    int16_t waits;\n"
    "    int16_t escalations;\n"
    "    char    sqlsa_reserved[4];\n"      // FR-005.21c: VSBB lives here at v330
    "} ESQLC_PACKED;\n";

static const char *kStats330 =
    "struct STATS_TYPE_R330 {\n"
    "    char    table_name[24];\n"
    "    int64_t records_accessed;\n"
    "    int64_t records_used;\n"
    "    int64_t disc_reads;\n"
    "    int64_t messages;\n"
    "    int64_t message_bytes;\n"
    "    int32_t waits;\n"
    "    int32_t escalations;\n"
    "    int16_t vsbb_write;\n"
    "    int16_t vsbb_flushed;\n"
    "    char    filler[32];\n"
    "} ESQLC_PACKED;\n";

// The prepare arm is identical in both families. It is emitted and asserted but
// never populated in this slice: dynamic SQL is feature 007.
static const char *kPrepare =
    "    int16_t input_num;\n"
    "    int16_t input_names_len;\n"
    "    int16_t output_num;\n"
    "    int16_t output_names_len;\n"
    "    int16_t name_map_len;\n"
    "    int16_t sql_statement_type;\n"
    "    int32_t output_collations_len;\n";

std::string sqlsa_layout() {
    std::ostringstream o;
    o << "/* --8<-- esqlc sqlsa layout begin --8<-- */\n";
    o << "#include <stdint.h>\n";
    o << "#define ESQLC_PACKED __attribute__((packed))\n";
    o << "#define SQLSA_EYE_CATCHER \"SA\"\n";
    o << "#define SQLSA_LEN 838\n";
    o << "#define SQLSA_LEN_R330 1790\n";
    // FR-005.23. The one sentinel convention the manual does publish, and the
    // precedent SD-7 follows for the fields it does not.
    o << "#define SQLSA_VSBB_TRUE (-1)\n";
    o << "#define SQLSA_VSBB_FALSE 0\n\n";

    o << kStats300 << "\n";
    o << "struct DML_TYPE {\n"
         "    int16_t num_tables;\n"
         "    struct STATS_TYPE stats[16];\n"
         "} ESQLC_PACKED;\n\n";
    o << "struct PREPARE_TYPE {\n" << kPrepare << "} ESQLC_PACKED;\n\n";
    o << "struct SQLSA_TYPE {\n"
         "    char    eye_catcher[2];\n"
         "    int16_t version;\n"
         "    union { struct DML_TYPE dml; struct PREPARE_TYPE prepare; } u;\n"
         "} ESQLC_PACKED;\n\n";

    o << kStats330 << "\n";
    o << "struct DML_TYPE_R330 {\n"
         "    int16_t num_tables;\n"
         "    int64_t master_executor_elapsed_time;\n"
         "    int64_t total_esp_cpu_time;\n"
         "    int64_t total_sortprog_cpu_time;\n"
         "    char    filler[32];\n"
         "    struct STATS_TYPE_R330 stats[16];\n"
         "} ESQLC_PACKED;\n\n";
    o << "struct PREPARE_TYPE_R330 {\n" << kPrepare << "} ESQLC_PACKED;\n\n";
    o << "struct SQLSA_TYPE_R330 {\n"
         "    char    eye_catcher[2];\n"
         "    int16_t version;\n"
         "    union { struct DML_TYPE_R330 dml; struct PREPARE_TYPE_R330 prepare; } u;\n"
         "} ESQLC_PACKED;\n\n";

    // T563 — the bounded assertion set. Asserting all 16 stats[] entries
    // field-by-field would be 100+ assertions restating one stride, so: every
    // header field, both arms at one offset, all of stats[0], and stats[1]
    // against stats[0] to pin the stride, with sizeof pinning the total. A
    // layout error cannot escape that set; the other 14 entries follow.
    o << "_Static_assert(sizeof(struct SQLSA_TYPE) == SQLSA_LEN, \"SQLSA_LEN 838\");\n";
    o << "_Static_assert(sizeof(struct SQLSA_TYPE_R330) == SQLSA_LEN_R330, "
         "\"SQLSA_LEN_R330 1790\");\n";
    o << "_Static_assert(sizeof(struct STATS_TYPE) == 52, \"v300 stats stride\");\n";
    o << "_Static_assert(sizeof(struct STATS_TYPE_R330) == 108, \"v330 stats stride\");\n";
    o << "_Static_assert(offsetof(struct SQLSA_TYPE, eye_catcher) == 0, "
         "\"eye-catcher leads\");\n";
    o << "_Static_assert(offsetof(struct SQLSA_TYPE_R330, eye_catcher) == 0, "
         "\"eye-catcher leads\");\n";
    o << "_Static_assert(offsetof(struct SQLSA_TYPE, version) == 2, \"version follows\");\n";
    // FR-005.21a: arms of a union, not coexisting substructures.
    o << "_Static_assert(offsetof(struct SQLSA_TYPE, u.dml) == "
         "offsetof(struct SQLSA_TYPE, u.prepare), \"dml/prepare is a union\");\n";
    o << "_Static_assert(offsetof(struct SQLSA_TYPE_R330, u.dml) == "
         "offsetof(struct SQLSA_TYPE_R330, u.prepare), \"dml/prepare is a union\");\n";
    o << "_Static_assert(offsetof(struct SQLSA_TYPE, u.dml.num_tables) == 4, "
         "\"num_tables at 4\");\n";
    o << "_Static_assert(offsetof(struct SQLSA_TYPE, u.dml.stats[0]) == 6, "
         "\"stats[0] at 6\");\n";
    o << "_Static_assert(offsetof(struct SQLSA_TYPE, u.dml.stats[1]) - "
         "offsetof(struct SQLSA_TYPE, u.dml.stats[0]) == 52, \"v300 stride\");\n";
    o << "_Static_assert(offsetof(struct SQLSA_TYPE_R330, u.dml.stats[1]) - "
         "offsetof(struct SQLSA_TYPE_R330, u.dml.stats[0]) == 108, \"v330 stride\");\n";
    // Every field of stats[0], v300.
    o << "_Static_assert(offsetof(struct STATS_TYPE, table_name) == 0, \"table_name\");\n";
    o << "_Static_assert(offsetof(struct STATS_TYPE, records_accessed) == 24, "
         "\"records_accessed\");\n";
    o << "_Static_assert(offsetof(struct STATS_TYPE, records_used) == 28, "
         "\"records_used\");\n";
    o << "_Static_assert(offsetof(struct STATS_TYPE, disc_reads) == 32, \"disc_reads\");\n";
    o << "_Static_assert(offsetof(struct STATS_TYPE, messages) == 36, \"messages\");\n";
    o << "_Static_assert(offsetof(struct STATS_TYPE, message_bytes) == 40, "
         "\"message_bytes\");\n";
    o << "_Static_assert(offsetof(struct STATS_TYPE, waits) == 44, \"waits\");\n";
    o << "_Static_assert(offsetof(struct STATS_TYPE, escalations) == 46, \"escalations\");\n";
    o << "_Static_assert(offsetof(struct STATS_TYPE, sqlsa_reserved) == 48, "
         "\"sqlsa_reserved, where v330 has VSBB\");\n";
    // And the v330 fields that differ in kind rather than only in width.
    o << "_Static_assert(offsetof(struct STATS_TYPE_R330, vsbb_write) == 72, "
         "\"vsbb_write\");\n";
    o << "_Static_assert(offsetof(struct STATS_TYPE_R330, vsbb_flushed) == 74, "
         "\"vsbb_flushed\");\n";
    o << "/* --8<-- esqlc sqlsa layout end --8<-- */\n";
    return o.str();
}

// FR-005.9. Versions 1, 2, 300, 340 or later for all three structures, and
// additionally 330 for the SQLSA only.
bool sqlsa_version_ok(int v, bool is_sqlsa) {
    if (v == 1 || v == 2 || v == 300) return true;
    if (v >= 340) return true;
    if (v == 330) return is_sqlsa;      // ESQLC-5003 for the others
    return false;                        // ESQLC-5002
}

}  // namespace pp
