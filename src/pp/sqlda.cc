// T1071-T1074 — SQLDA emission.
//
// Example 10-1 (§10 p.10-7) publishes the layout and p.10-3 the directive:
//
//     INCLUDE SQLDA ( sqlda-name [ , sqlvar-count ] … )
//
//     struct SQLDA_TYPE {
//        char  eye_catcher[2];
//        short num_entries;
//        struct SQLVAR_TYPE {
//           short data_type;  short data_len;
//           short precision;  short null_info;
//           long  var_ptr;    long  ind_ptr;
//           long  cprl_ptr;   long  reserved;
//        } sqlvar[sqlvar-count];
//     } sqlda-name;
//
// Three things about that, each of which would be wrong if guessed:
//
//   The count is the DIRECTIVE's. Not 1, not a fixed maximum — the SQLDA has no
//   documented cap, unlike the SQLSA's 16 tables. A count of 1 is idiomatic
//   because p.10-29 calls it "the SQLDA template" and p.10-30's malloc
//   arithmetic is then exact, but the program chooses.
//
//   NOT a C99 flexible member. `sqlvar[]` would make sizeof(SQLDA_TYPE) exclude
//   the array, and p.10-30's
//       sizeof(SQLDA_TYPE) + ((n - 1) * sizeof(SQLVAR_TYPE))
//   would then under-allocate by one entry — a heap overflow appearing at a
//   customer's column count and not at a fixture's.
//
//   The four address fields are `long` in the published text, which is 32-bit
//   on NonStop: 8 + 16 = 24, the published SQLDA_SQLVAR_LEN. DIV-040 widens
//   them to hold real pointers: 8 + 32 = 40, which is what FR-007.6 states.
//   A program using p.10-30's sizeof idiom is safe at either width because it
//   never names a byte count.
//
// The names and collation buffers are SIBLING arrays, not members — Example
// 10-1 declares `char names_buffer[name-string-size];` alongside.
//
// Kept in step with src/rt/rt_sqlda_offsets.h by tests/harness/sqlda_layout_sync.sh.
#include "pp.h"
#include <sstream>

namespace pp {

std::string sqlda_layout(const std::string &var, unsigned count,
                          const std::string &names_var, unsigned names_size) {
    std::ostringstream o;
    o << "/* --8<-- esqlc sqlda layout begin --8<-- */\n";
    o << "#include <stdint.h>\n";
    o << "#define SQLDA_EYE_CATCHER \"D1\"\n";
    o << "#define SQLDA_HEADER_LEN 4\n";
    // DIV-040: the published value is 24; these fields hold real pointers.
    o << "#define SQLDA_SQLVAR_LEN 40\n";
    o << "#define SQLDA_NAMESBUF_OVHD_LEN 11\n";
    o << "#define SQLDA_COLLBUF_OVHD_LEN 4\n\n";

    o << "struct SQLVAR_TYPE {\n"
         "    int16_t data_type;\n"
         "    int16_t data_len;\n"
         "    int16_t precision;\n"
         "    int16_t null_info;\n"
         "    void   *var_ptr;\n"
         "    void   *ind_ptr;\n"
         "    void   *cprl_ptr;\n"
         "    void   *reserved;\n"      // FR-007.6b: present and preserved
         "};\n\n";

    // No packing attribute, unlike the SQLSA. Four int16_t then four pointers
    // align naturally to 40 with no padding, so the total is reached without
    // one — and the assertions below prove that rather than assume it. If a
    // target ever needs packing, the sizeof assertion fails first.
    o << "struct SQLDA_TYPE {\n"
         "    char    eye_catcher[2];\n"
         "    int16_t num_entries;\n"
         "    struct SQLVAR_TYPE sqlvar[" << count << "];\n"
         "};\n";
    o << "struct SQLDA_TYPE " << var << ";\n";
    if (!names_var.empty()) {
        // FR-007.7a: (name-string-size + 11) * sqlvar-count.
        o << "char " << names_var << "["
          << (names_size + 11u) * count << "];\n";
    }
    o << "\n";

    // NFR-007.3 — sizeof AND offsetof on every field. Stricter than Gate 5's
    // bounded set for the SQLSA, and affordable: eight fields, not a 16-entry
    // array whose stride stands in for its members.
    o << "_Static_assert(sizeof(struct SQLVAR_TYPE) == SQLDA_SQLVAR_LEN, "
         "\"sqlvar is 40 under DIV-040\");\n";
    struct { const char *f; unsigned off; } fields[] = {
        {"data_type", 0}, {"data_len", 2}, {"precision", 4}, {"null_info", 6},
        {"var_ptr", 8},   {"ind_ptr", 16}, {"cprl_ptr", 24}, {"reserved", 32},
    };
    for (const auto &f : fields)
        o << "_Static_assert(offsetof(struct SQLVAR_TYPE, " << f.f
          << ") == " << f.off << ", \"" << f.f << "\");\n";
    // DIV-058, found by this assertion failing. SQLDA_HEADER_LEN is published
    // as 4 (Table 10-2) and, with the published 32-bit `long` address fields,
    // sqlvar does begin at 4. DIV-040 widens those fields to real pointers,
    // which raises SQLVAR_TYPE's alignment from 4 to 8 and moves sqlvar to
    // offset 8. Measured: published sqlvar=4 align=4, widened sqlvar=8 align=8.
    //
    // Natural alignment is kept rather than packing the structure, because
    // p.10-30's sizeof arithmetic stays correct automatically and because the
    // PROGRAM dereferences sqlda->sqlvar[i].var_ptr directly — packing would
    // hand it a misaligned pointer load, which faults on a strict-alignment
    // target. SQLDA_HEADER_LEN keeps its published value: Table 10-2 defines it
    // as the length of the header FIELDS, which is still 4.
    //
    // The assertion pins the real offset so a change is visible.
    o << "_Static_assert(offsetof(struct SQLDA_TYPE, sqlvar) == 8, "
         "\"sqlvar at 8: DIV-040's widening raises alignment, DIV-058\");\n";
    o << "_Static_assert(SQLDA_HEADER_LEN == 4, "
         "\"the header FIELDS are 4 bytes, as published\");\n";
    o << "/* --8<-- esqlc sqlda layout end --8<-- */\n";
    return o.str();
}

}  // namespace pp
