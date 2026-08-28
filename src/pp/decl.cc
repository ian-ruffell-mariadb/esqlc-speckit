// T067 — parse `short`  -> 16-bit signed descriptor (FR-002.9)
// T068 — parse `char[n]` -> width = n-1, capacity = n (FR-002.1/.2/.3)
//
// Slice scope is two type rows. Every other type reaches ESQLC-1012 via the
// unsupported-type path rather than being guessed at.
#include "pp.h"
#include <cctype>

namespace pp {
namespace {

// ESQLC_T_* mirrored from include/esqlc.h. Kept as literals here so the
// preprocessor does not include the runtime ABI header.
constexpr unsigned T_CHAR_FIXED = 1;
constexpr unsigned T_INT        = 2;

struct Tok { std::string text; Pos pos; };

std::vector<Tok> tokenize(const std::string &s, Pos base) {
    std::vector<Tok> t;
    int line = base.line, col = base.col;
    std::size_t i = 0;
    auto adv = [&](char ch) { if (ch == '\n') { ++line; col = 1; } else ++col; };
    while (i < s.size()) {
        char ch = s[i];
        if (std::isspace((unsigned char)ch)) { adv(ch); ++i; continue; }
        if (std::isalnum((unsigned char)ch) || ch == '_') {
            Tok tk; tk.pos = Pos{line, col};
            while (i < s.size() && (std::isalnum((unsigned char)s[i]) || s[i] == '_')) {
                tk.text += s[i]; adv(s[i]); ++i;
            }
            t.push_back(tk);
            continue;
        }
        Tok tk; tk.pos = Pos{line, col}; tk.text = std::string(1, ch);
        t.push_back(tk); adv(ch); ++i;
    }
    return t;
}

bool is_int_keyword(const std::string &w, unsigned *width, bool *is_signed) {
    // DIV-001: mapping is by WIDTH, not by C type name.
    if (w == "short")    { *width = 2; *is_signed = true;  return true; }
    if (w == "int")      { *width = 4; *is_signed = true;  return true; }
    if (w == "long")     { *width = 4; *is_signed = true;  return true; }
    return false;
}

}  // namespace

void parse_declare_section(const std::string &body, Pos at,
                           std::vector<HostVar> &out, Diag &d) {
    auto t = tokenize(body, at);
    std::size_t i = 0;
    while (i < t.size()) {
        // Skip stray semicolons the caller left in.
        if (t[i].text == ";") { ++i; continue; }

        // FR-002.12 / slice: unsigned long long is rejected outright.
        if (t[i].text == "unsigned" && i + 2 < t.size() &&
            t[i + 1].text == "long" && t[i + 2].text == "long") {
            d.error("ESQLC-2001", t[i].pos,
                    "unsigned long long is not supported in a unit containing embedded SQL");
            while (i < t.size() && t[i].text != ";") ++i;
            continue;
        }

        bool is_signed = true;
        std::string type_word = t[i].text;
        if (type_word == "unsigned") {
            is_signed = false;
            if (i + 1 < t.size()) { ++i; type_word = t[i].text; }
        } else if (type_word == "signed") {
            if (i + 1 < t.size()) { ++i; type_word = t[i].text; }
        }

        unsigned width = 0; bool sgn = is_signed;
        if (type_word == "char") {
            // char name[n];
            if (i + 1 >= t.size()) break;
            HostVar hv;
            hv.name = t[i + 1].text;
            std::size_t j = i + 2;
            if (j < t.size() && t[j].text == "[") {
                if (j + 2 < t.size() && t[j + 2].text == "]") {
                    long n = std::strtol(t[j + 1].text.c_str(), nullptr, 10);
                    if (n <= 1) {
                        d.error("ESQLC-2009", t[j + 1].pos,
                                "character host variable array size must exceed 1");
                        while (i < t.size() && t[i].text != ";") ++i;
                        continue;
                    }
                    hv.type     = T_CHAR_FIXED;
                    hv.capacity = (unsigned)n;
                    // FR-002.3: the final byte is a null-terminator placeholder,
                    // so the column length on the wire is n-1 (FR-002.30).
                    hv.width    = (unsigned)n - 1;
                    hv.is_signed = true;
                    hv.c_decl   = "char " + hv.name + "[" + t[j + 1].text + "];";
                    out.push_back(hv);
                    i = j + 3;
                    while (i < t.size() && t[i].text != ";") ++i;
                    continue;
                }
            }
            // A scalar `char` host variable is not in slice scope.
            d.error("ESQLC-1012", t[i].pos,
                    "scalar char host variable is not implemented in this slice "
                    "(owned by feature 002)");
            while (i < t.size() && t[i].text != ";") ++i;
            continue;
        }

        if (is_int_keyword(type_word, &width, &sgn)) {
            if (!is_signed) sgn = false;
            if (i + 1 >= t.size()) break;
            HostVar hv;
            hv.name      = t[i + 1].text;
            hv.type      = T_INT;
            hv.width     = width;
            hv.capacity  = width;
            hv.is_signed = sgn;
            hv.c_decl    = (sgn ? "" : "unsigned ") + type_word + " " + hv.name + ";";
            // Reject an array of integers: not a slice form.
            if (i + 2 < t.size() && t[i + 2].text == "[") {
                d.error("ESQLC-1012", t[i + 2].pos,
                        "array of integer host variables is not implemented in this "
                        "slice (owned by feature 002)");
                while (i < t.size() && t[i].text != ";") ++i;
                continue;
            }
            out.push_back(hv);
            i += 2;
            while (i < t.size() && t[i].text != ";") ++i;
            continue;
        }

        // Any other type: refuse by name rather than guess (Constitution III).
        d.error("ESQLC-1012", t[i].pos,
                "host variable type '" + type_word +
                "' is not implemented in this slice (owned by feature 002)");
        while (i < t.size() && t[i].text != ";") ++i;
    }
}

}  // namespace pp
