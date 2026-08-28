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

enum class PosClass { Decl, Exec };

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
    bool        is_signed = true;
    std::string c_decl;               // verbatim C declaration to re-emit
};

// Parse the interior of a declare section. Appends to `out`.
void parse_declare_section(const std::string &body, Pos at,
                           std::vector<HostVar> &out, Diag &d);

// ---- dispatch (T069) ---------------------------------------------------
struct Handler {
    const char *keyword;
    PosClass    where;
    const char *owning_feature;       // nullptr => implemented in this slice
};
const Handler *lookup(const std::string &keyword);

// ---- emission (T072-T074) ---------------------------------------------
std::string emit(const std::string &file, const ScanResult &sr,
                 std::vector<HostVar> &vars, Diag &d);

}  // namespace pp
