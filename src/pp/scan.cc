// T062 — region split and SQL lexical rules.
// T063 — host-variable span capture, in the SAME lexer pass so that a :name
//        inside a "string" or after -- is excluded by construction, not by a
//        later filter. This is the design bet the slice rests on.
// T064 — the body is carried as an opaque span; nothing parses it.
// Also carries the brace-depth position tracker (plan component context.cc)
// and pragma detection (plan component pragma.cc) — see the consolidation note
// in the implementation report.
#include "pp.h"
#include <cctype>
#include <cstring>

namespace pp {
namespace {

bool ident_char(char c) { return std::isalnum((unsigned char)c) || c == '_'; }

// Case-insensitive match of `word` at src[i], on identifier boundaries.
bool word_at(const std::string &s, std::size_t i, const char *word) {
    std::size_t n = 0;
    while (word[n]) {
        if (i + n >= s.size()) return false;
        if (std::toupper((unsigned char)s[i + n]) != word[n]) return false;
        ++n;
    }
    if (i + n < s.size() && ident_char(s[i + n])) return false;
    if (i > 0 && ident_char(s[i - 1])) return false;
    return true;
}

struct Cursor {
    const std::string &s;
    std::size_t i = 0;
    int line = 1, col = 1;
    explicit Cursor(const std::string &src) : s(src) {}
    bool eof() const { return i >= s.size(); }
    char cur() const { return s[i]; }
    char peek(std::size_t k = 1) const { return i + k < s.size() ? s[i + k] : '\0'; }
    Pos pos() const { return Pos{line, col}; }
    void bump() {
        if (s[i] == '\n') { ++line; col = 1; } else { ++col; }
        ++i;
    }
};

// Uppercased leading keyword sequence. Multi-word forms the slice cares about
// are matched longest-first so "BEGIN DECLARE SECTION" beats "BEGIN".
std::string leading_keyword(const std::string &body) {
    static const char *multi[] = {
        "BEGIN DECLARE SECTION", "END DECLARE SECTION",
        "INCLUDE STRUCTURES", "INCLUDE SQLCA", "INCLUDE SQLSA", "INCLUDE SQLDA",
        "DECLARE CURSOR", "BEGIN WORK", "COMMIT WORK", "ROLLBACK WORK",
        "EXECUTE IMMEDIATE", "DESCRIBE INPUT", "WHENEVER", "SQL SOURCE",
        nullptr
    };
    // Normalise leading whitespace/newlines into single spaces for matching.
    std::string norm;
    for (std::size_t k = 0; k < body.size() && norm.size() < 64; ++k) {
        char c = body[k];
        if (std::isspace((unsigned char)c)) {
            if (!norm.empty() && norm.back() != ' ') norm.push_back(' ');
        } else {
            norm.push_back((char)std::toupper((unsigned char)c));
        }
    }
    while (!norm.empty() && norm.front() == ' ') norm.erase(norm.begin());
    for (int m = 0; multi[m]; ++m) {
        std::size_t L = std::strlen(multi[m]);
        if (norm.compare(0, L, multi[m]) == 0 &&
            (norm.size() == L || !ident_char(norm[L])))
            return multi[m];
    }
    std::size_t e = norm.find(' ');
    return e == std::string::npos ? norm : norm.substr(0, e);
}

}  // namespace

ScanResult scan(const std::string &src, Diag &d) {
    ScanResult out;
    Cursor c(src);
    std::string ctext;
    Pos ctext_pos = c.pos();
    int brace_depth = 0;
    bool any_code = false;

    auto flush_c = [&]() {
        if (!ctext.empty()) {
            Chunk ch; ch.is_sql = false; ch.c_text = ctext; ch.pos = ctext_pos;
            out.chunks.push_back(std::move(ch));
            ctext.clear();
        }
        ctext_pos = c.pos();
    };

    while (!c.eof()) {
        // --- C comments: copied verbatim, never scanned for EXEC SQL -------
        if (c.cur() == '/' && c.peek() == '*') {
            while (!c.eof() && !(c.cur() == '*' && c.peek() == '/')) { ctext += c.cur(); c.bump(); }
            if (!c.eof()) { ctext += "*/"; c.bump(); c.bump(); }
            continue;
        }
        if (c.cur() == '/' && c.peek() == '/') {
            while (!c.eof() && c.cur() != '\n') { ctext += c.cur(); c.bump(); }
            continue;
        }
        // --- C string / char literals: copied verbatim (FR-001.19, AS-001.5)
        if (c.cur() == '"' || c.cur() == '\'') {
            char q = c.cur();
            ctext += c.cur(); c.bump();
            while (!c.eof() && c.cur() != q) {
                if (c.cur() == '\\' && c.peek()) { ctext += c.cur(); c.bump(); }
                ctext += c.cur(); c.bump();
            }
            if (!c.eof()) { ctext += c.cur(); c.bump(); }
            any_code = true;
            continue;
        }
        // --- #pragma SQL ---------------------------------------------------
        if (c.col == 1 || (c.cur() == '#' && !any_code)) {
            if (c.cur() == '#') {
                std::size_t save = c.i;
                Pos p = c.pos();
                std::size_t j = c.i + 1;
                while (j < src.size() && std::isspace((unsigned char)src[j]) && src[j] != '\n') ++j;
                if (word_at(src, j, "PRAGMA")) {
                    std::size_t k = j + 6;
                    while (k < src.size() && std::isspace((unsigned char)src[k]) && src[k] != '\n') ++k;
                    if (word_at(src, k, "SQL")) {
                        // Consume the whole line; emit nothing.
                        while (!c.eof() && c.cur() != '\n') c.bump();
                        out.pragma_seen = true;
                        out.pragma_pos = p;
                        if (any_code) out.saw_code_before_pragma = true;
                        continue;
                    }
                }
                c.i = save;  // not our pragma; fall through as C text
            }
        }
        // --- EXEC SQL ------------------------------------------------------
        if (word_at(src, c.i, "EXEC")) {
            std::size_t j = c.i + 4;
            while (j < src.size() && std::isspace((unsigned char)src[j])) ++j;
            if (word_at(src, j, "SQL")) {
                flush_c();
                Construct k;
                k.pos = c.pos();
                k.where = (brace_depth == 0) ? PosClass::Decl : PosClass::Exec;
                // advance past EXEC SQL
                while (c.i < j + 3) c.bump();
                // --- body, in one pass -------------------------------------
                std::string body;
                bool terminated = false;
                while (!c.eof()) {
                    // SQL comment: -- to end of line (FR-001.4)
                    if (c.cur() == '-' && c.peek() == '-') {
                        while (!c.eof() && c.cur() != '\n') c.bump();
                        continue;
                    }
                    // C comment inside an SQL region is an error (ESQLC-1003)
                    if (c.cur() == '/' && (c.peek() == '*' || c.peek() == '/')) {
                        d.error("ESQLC-1003", c.pos(),
                                "C comment inside an embedded SQL statement; use -- instead");
                        while (!c.eof() && c.cur() != '\n') c.bump();
                        continue;
                    }
                    // SQL string: only " is a delimiter (FR-001.6)
                    if (c.cur() == '"') {
                        body += c.cur(); c.bump();
                        while (!c.eof() && c.cur() != '"') { body += c.cur(); c.bump(); }
                        if (!c.eof()) { body += c.cur(); c.bump(); }
                        continue;   // :name inside a string is never captured
                    }
                    if (c.cur() == '\'') {
                        d.error("ESQLC-1004", c.pos(),
                                "single-quoted string in an embedded SQL statement; use \"");
                        body += c.cur(); c.bump();
                        continue;
                    }
                    if (c.cur() == ';') { c.bump(); terminated = true; break; }
                    if (word_at(src, c.i, "EXEC")) {
                        std::size_t q = c.i + 4;
                        while (q < src.size() && std::isspace((unsigned char)src[q])) ++q;
                        if (word_at(src, q, "SQL")) {
                            d.error("ESQLC-1001", c.pos(), "nested EXEC SQL construct");
                            break;
                        }
                    }
                    // --- host variable reference (FR-001.16) ---------------
                    if (c.cur() == ':' && c.peek() != ':') {
                        std::size_t bstart = body.size();
                        std::string ref = ":";
                        c.bump();
                        if (!c.eof() && c.cur() == '*') { ref += '*'; c.bump(); }
                        std::string name;
                        while (!c.eof() && (ident_char(c.cur()) || c.cur() == '.')) {
                            name += c.cur(); c.bump();
                        }
                        if (name.empty()) { body += ref; continue; }
                        ref += name;
                        body += ref;
                        HostVarRef hv;
                        hv.begin = bstart;
                        hv.end   = body.size();
                        hv.name  = name;
                        k.hostvars.push_back(hv);
                        continue;
                    }
                    body += c.cur(); c.bump();
                }
                if (!terminated) {
                    d.error("ESQLC-1002", k.pos,
                            "unterminated EXEC SQL construct: missing ';'");
                }
                k.body = body;
                k.keyword = leading_keyword(body);
                Chunk ch; ch.is_sql = true; ch.sql = k; ch.pos = k.pos;
                out.chunks.push_back(std::move(ch));
                // The following C text begins *after* the construct. flush_c()
                // stamped ctext_pos before the construct was scanned, so it is
                // stale by exactly the construct's length — restamp it, or
                // every line number after an embedded statement is wrong
                // (FR-001.18).
                ctext_pos = c.pos();
                continue;
            }
        }
        // --- ordinary C ----------------------------------------------------
        if (c.cur() == '{') ++brace_depth;
        if (c.cur() == '}') { if (brace_depth > 0) --brace_depth; }
        if (!std::isspace((unsigned char)c.cur()) && c.cur() != '#') any_code = true;
        ctext += c.cur();
        c.bump();
    }
    flush_c();
    return out;
}

}  // namespace pp
