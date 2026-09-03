// Shared types for the esqlc preprocessor.
// Slice: specs/gate-1.md — six statement keywords, everything else ESQLC-1012.
#pragma once
#include <string>
#include <vector>
#include <cstddef>

namespace pp {

// ---- diagnostics (T060) ------------------------------------------------
struct Pos { int line = 0; int col = 0; };

class Diag {
public:
    explicit Diag(std::string file) : file_(std::move(file)) {}
    // NFR-001.3: every diagnostic carries file, line and column.
    void error(const char *code, Pos p, const std::string &msg);
    void info(const char *code, Pos p, const std::string &msg);
    int  errors() const { return errors_; }
private:
    std::string file_;
    int errors_ = 0;
};

// ---- scanning (T062-T064) ---------------------------------------------
// A :name reference, as byte offsets *within* Construct::body.
struct HostVarRef {
    std::size_t begin = 0, end = 0;   // [begin,end) covers ":name"
    std::string name;                 // without the colon
};

// FR-001.13: WHENEVER, SQL SOURCE and CONTROL are accepted in ANY position,
// so the position model needs a third value. Gate 4 found this: the table
// had only Decl and Exec, and WHENEVER registered as Decl was rejected
// everywhere a program actually writes it.
enum class PosClass { Decl, Exec, Any };

struct Construct {
    std::string keyword;              // leading keyword(s), uppercased
    std::string body;                 // text between "EXEC SQL" and ";"
    Pos pos;                          // position of the EXEC token
    PosClass where = PosClass::Decl;
    std::vector<HostVarRef> hostvars;

    // Landmarks, recorded in the same pass as the spans above so that an INTO
    // or FROM inside a "string" cannot be mistaken for one (Gate 2, T250).
    // Offsets into `body`, or npos. The body is still never parsed — these are
    // two more positions the lexer notes in passing, nothing more.
    static constexpr std::size_t npos = static_cast<std::size_t>(-1);
    std::size_t into_off = npos;
    std::size_t from_off = npos;

    // Gate 6, SD-9. The statement's table, read as a third landmark of the
    // same kind: the identifier after INTO, UPDATE or FROM as the leading
    // keyword dictates. Empty when the form did not yield a plain identifier —
    // a multi-table UPDATE, a delimited identifier, a leading subquery. Empty
    // is required rather than a best guess: `table_name` reads as
    // authoritative, so a plausible wrong name is undetectable where the
    // sentinel is visibly not-measured.
    std::string table;
};

// A chunk of the source: either verbatim C, or an embedded construct.
struct Chunk {
    bool is_sql = false;
    std::string c_text;               // when !is_sql
    Construct sql;                    // when is_sql
    Pos pos;
};

struct ScanResult {
    std::vector<Chunk> chunks;
    bool pragma_seen = false;
    Pos  pragma_pos;
    bool saw_code_before_pragma = false;
};

ScanResult scan(const std::string &src, Diag &d);

// ---- host variables (T067-T068) ---------------------------------------
struct HostVar {
    std::string name;
    unsigned    type = 0;             // ESQLC_T_*
    unsigned    width = 0;            // bytes on the wire
    unsigned    capacity = 0;         // declared array size
    unsigned short charset = 0;       // Gate 8; 0 = UNKNOWN (connection default)
    bool        is_signed = true;
    std::string c_decl;               // verbatim C declaration to re-emit
};

// Parse the interior of a declare section. Appends to `out`.
void parse_declare_section(const std::string &body, Pos at,
                           std::vector<HostVar> &out, Diag &d);

// ---- WHENEVER (Gate 4, T470-T472) --------------------------------------
// The directive emits nothing itself. It updates one entry per condition, and
// every subsequent *applicable* statement appends checks built from the table.
// State is per-condition: setting SQLERROR leaves NOT FOUND alone.
enum class WhenCond { NotFound = 0, SqlError = 1, SqlWarning = 2, Count = 3 };
enum class WhenAct  { Continue, Call, Goto };

struct WheneverEntry {
    WhenAct     act = WhenAct::Continue;   // CONTINUE emits nothing
    std::string target;                    // handler or label
};
struct WheneverState {
    WheneverEntry e[(int)WhenCond::Count];
};

// Apply one WHENEVER directive to the table.
void whenever_set(WheneverState &st, const Construct &k, Diag &d);

// The checks to append after an applicable statement, in the published
// precedence order: NOT FOUND, then SQLERROR, then SQLWARNING.
std::string whenever_checks(const WheneverState &st);

// SD-5: WHENEVER applies to DML, DCL and DDL, not to transaction control.
bool whenever_applies(const std::string &keyword);

// ---- SQLDA (Gate 10, T1071-T1074) --------------------------------------
// Example 10-1's layout at the directive's count. NOT a flexible member: §10
// p.10-30 allocates sizeof(SQLDA_TYPE) + ((n-1) * sizeof(SQLVAR_TYPE)), which
// under-allocates by one entry if sizeof excludes the array.
std::string sqlda_layout(const std::string &var, unsigned count,
                         const std::string &names_var, unsigned names_size);

// ---- schema cache (Gate 9, T960-T964) ----------------------------------
// The preprocessor reads a committed cache and never opens a socket, which is
// how FR-006.2e's "read access at preprocess time" and NFR-001.2's "Tier 1
// runs with no MariaDB" coexist (NFR-006.2, SD-15).
struct SchemaColumn {
    std::string name;         // as catalogued
    std::string sqltype;      // SMALLINT, CHAR, VARCHAR, ...
    unsigned    length = 0;   // column length, for character types
    bool        nullable = false;
    std::string charset;      // SQL/MP keyword, or UNKNOWN
};

struct Schema {
    // SD-16: the only staleness signal there is. The preprocessor cannot detect
    // a stale cache — it has no connection — so it stamps this into generated
    // output and leaves detection to the build system.
    std::string captured;
    std::vector<std::pair<std::string, std::vector<SchemaColumn>>> tables;

    const std::vector<SchemaColumn> *find(const std::string &table) const;
};

// Read the cache. `err` distinguishes the two failures the diagnostics keep
// separate: SchemaErr::None, Absent (no --schema at all, ESQLC-6002) and
// Unreadable (named but unusable, ESQLC-6008).
enum class SchemaErr { None, Absent, Unreadable };
SchemaErr schema_read(const std::string &path, Schema &out);

// ---- INVOKE (Gate 9, T966-T977) ----------------------------------------
// Generates the declaration text of §2 p.2-22. The text is then RE-PARSED by
// parse_declare_section rather than trusted, so there is one path to be right
// and Gates 7 and 8 are the test of what this emits.
std::string invoke_generate(const std::string &object, const std::string &tag,
                            const std::vector<SchemaColumn> &cols,
                            const std::string &captured, Pos at, Diag &d);

// ---- character sets (Gate 8, T860) -------------------------------------
// Mapped: MariaDB has it. Unmapped: the keyword is real, MariaDB has no
// counterpart (the gap is MariaDB's). Unspecified: the keyword is real and the
// manual names no encoding (the gap is the manual's). The last two get
// different diagnostics because they send a reader to different places.
enum class CsClass { Mapped, Unmapped, Unspecified };

struct CharsetKeyword {
    const char    *keyword;
    unsigned short id;       // project-internal; NOT the SQLDA id (002 Q7)
    CsClass        cls;
};

const CharsetKeyword *charset_lookup(const std::string &kw);
const CharsetKeyword *charset_table(int *n);

// ---- SQLSA (Gate 5, T560-T564) -----------------------------------------
// Both version families, always emitted together: the manual names the types
// distinctly and FR-005.26 has VERSION CURRENT generate both, so only the
// declared *variable* takes the selected version. Bracketed with markers that
// tests/harness/sqlsa_layout_sync.sh lifts, because this layout is also encoded
// in src/rt/rt_sqlsa_offsets.h and nothing else would notice them drifting.
std::string sqlsa_layout();

// FR-005.9: 1, 2, 300, 340+ for all three structures; 330 for SQLSA only.
bool sqlsa_version_ok(int v, bool is_sqlsa);

// ---- dispatch (T069) ---------------------------------------------------
struct Handler {
    const char *keyword;
    PosClass    where;
    const char *owning_feature;       // nullptr => implemented in this slice
};
const Handler *lookup(const std::string &keyword);

// ---- emission (T072-T074) ---------------------------------------------
std::string emit(const std::string &file, const ScanResult &sr,
                 std::vector<HostVar> &vars, Diag &d,
                 const std::string &schema_path);

}  // namespace pp
