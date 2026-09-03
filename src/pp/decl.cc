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
constexpr unsigned T_CHAR_VAR   = 3;   // Gate 7: VARCHAR structures
constexpr unsigned T_FLOAT      = 5;   // Gate 7: float and double

struct Tok { std::string text; Pos pos; };

std::vector<Tok> tokenize(const std::string &s, Pos base) {
    std::vector<Tok> t;
    int line = base.line, col = base.col;
    std::size_t i = 0;
    auto adv = [&](char ch) { if (ch == '\n') { ++line; col = 1; } else ++col; };
    while (i < s.size()) {
        char ch = s[i];
        if (std::isspace((unsigned char)ch)) { adv(ch); ++i; continue; }

        // C comments are whitespace. Skipping them here rather than anywhere
        // else matters for two reasons: `adv` keeps line and column accurate
        // through the comment, so a diagnostic after a multi-line comment still
        // points at the right place; and a `;` or a type keyword inside a
        // comment never becomes a token, so neither the parse nor the
        // error-recovery skip can be misled by comment text.
        //
        // Principle II. Without this, a declare section carrying an ordinary
        // comment was refused with "ESQLC-1012: host variable type '/'" — a
        // diagnostic naming punctuation as an unsupported type, for a program
        // that is valid C. Most real programs comment their declarations.
        if (ch == '/' && i + 1 < s.size() && s[i + 1] == '*') {
            adv(s[i]); ++i;
            adv(s[i]); ++i;
            while (i < s.size() && !(s[i] == '*' && i + 1 < s.size() && s[i + 1] == '/')) {
                adv(s[i]); ++i;
            }
            // An unterminated comment is left to the C compiler, which sees the
            // same text: the region is re-emitted verbatim. Consume to the end
            // rather than inventing a diagnostic the C compiler states better.
            if (i < s.size()) { adv(s[i]); ++i; }      // '*'
            if (i < s.size()) { adv(s[i]); ++i; }      // '/'
            continue;
        }
        if (ch == '/' && i + 1 < s.size() && s[i + 1] == '/') {
            while (i < s.size() && s[i] != '\n') { adv(s[i]); ++i; }
            continue;
        }

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

// T760 — width comes from the HOST COMPILER, not from the type's spelling.
//
// DIV-001 says the mapping is by width rather than by type name, and its advice
// to use width-exact types is guidance for *generated* declarations, where the
// preprocessor picks the type. It cannot apply to a declaration the customer
// wrote: rewriting that would be the source change Principle II forbids.
//
// This function previously mapped `long` to 4, following the manual's note that
// NonStop `long` is 32-bit. The result did not compile. The emitter asserts
// sizeof(hostvar) == width, and on any LP64 host sizeof(long) is 8:
//
//     error: static assertion failed due to requirement 'sizeof (big) == 4'
//
// The assertion was right. A descriptor claiming four bytes of an eight-byte
// variable would have bound the low half — correct for small values on a
// little-endian host, silently wrong for everything else. So the width is
// sizeof as this compiler sees it, and the divergence from NonStop's 32-bit
// `long` is what DIV-001 already accepts.
bool is_int_keyword(const std::string &w, unsigned *width, bool *is_signed) {
    if (w == "short")    { *width = sizeof(short);     *is_signed = true; return true; }
    if (w == "int")      { *width = sizeof(int);       *is_signed = true; return true; }
    if (w == "long")     { *width = sizeof(long);      *is_signed = true; return true; }
    if (w == "longlong") { *width = sizeof(long long); *is_signed = true; return true; }
    return false;
}

// T762 — float and double are one family separated by width, the same
// by-width dispatch the integer family uses.
bool is_float_keyword(const std::string &w, unsigned *width) {
    if (w == "float")  { *width = sizeof(float);  return true; }
    if (w == "double") { *width = sizeof(double); return true; }
    return false;
}


// Error recovery for a structured declaration. Skipping to the next `;` — the
// rule for a scalar — lands *inside* a struct, because `short len;` has one.
// The remainder was then re-parsed as declarations and produced a spurious
// ESQLC-1012 on the closing `}`. Skip past the brace first, then to the `;`.
static void skip_struct(const std::vector<Tok> &t, std::size_t &i) {
    while (i < t.size() && t[i].text != "}") ++i;
    while (i < t.size() && t[i].text != ";") ++i;
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

        // T763 — a VARCHAR structure, recognised by shape.
        //
        // FR-002.6 fixes the member order and the names, and §2 p.2-9 fixes the
        // length type: "declare the length as a short data type (and not an
        // int)". So the shape is the specification, and anything else is
        // refused under FR-002.20 rather than bound as its first member — the
        // failure mode where a program passes a record and silently gets one
        // field of it.
        if (t[i].text == "struct") {
            Pos sp = t[i].pos;
            if (i + 1 >= t.size() || t[i + 1].text != "{") {
                d.error("ESQLC-2003", sp,
                        "only a VARCHAR structure (short len; char val[n];) may be "
                        "used as a host variable");
                skip_struct(t, i);
                continue;
            }
            // short len ; char val [ n ] ; } name
            const char *want[] = {"{", nullptr, "len", ";", "char", "val", "["};
            bool shape = i + 12 < t.size();
            if (shape) {
                shape = t[i + 1].text == want[0] &&
                        t[i + 3].text == want[2] && t[i + 4].text == want[3] &&
                        t[i + 5].text == want[4] && t[i + 6].text == want[5] &&
                        t[i + 7].text == want[6] && t[i + 9].text == "]" &&
                        t[i + 10].text == ";" && t[i + 11].text == "}";
            }
            if (!shape) {
                d.error("ESQLC-2003", sp,
                        "only a VARCHAR structure (short len; char val[n];) may be "
                        "used as a host variable");
                skip_struct(t, i);
                continue;
            }
            // T764 — FR-002.21: the length field must be `short`, not `int`.
            if (t[i + 2].text != "short") {
                d.error("ESQLC-2002", t[i + 2].pos,
                        "a hand-declared VARCHAR length field must be 'short', not '" +
                        t[i + 2].text + "'");
                skip_struct(t, i);
                continue;
            }
            long n = std::strtol(t[i + 8].text.c_str(), nullptr, 10);
            if (n <= 1) {
                d.error("ESQLC-2009", t[i + 8].pos,
                        "VARCHAR val array size must exceed 1");
                skip_struct(t, i);
                continue;
            }
            HostVar hv;
            hv.name      = t[i + 12].text;
            hv.type      = T_CHAR_VAR;
            // SD-10: capacity is the declared val size; width is capacity - 1,
            // mirroring FR-002.3's treatment of char v[l+1]. Provisional — it
            // assumes the CHAR_AS_STRING shape (001 Q2).
            hv.capacity  = (unsigned)n;
            hv.width     = (unsigned)n - 1;
            hv.is_signed = true;
            hv.c_decl    = "struct { short len; char val[" + t[i + 8].text + "]; } " +
                           hv.name + ";";
            out.push_back(hv);
            i += 13;
            while (i < t.size() && t[i].text != ";") ++i;
            continue;
        }

        bool is_signed = true;
        std::string type_word = t[i].text;
        // `long long` arrives as two tokens. Collapsed here so the width table
        // stays a flat lookup rather than growing a parser.
        if (type_word == "long" && i + 1 < t.size() && t[i + 1].text == "long") {
            type_word = "longlong";
            ++i;
        }
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

        if (is_float_keyword(type_word, &width)) {
            if (i + 1 >= t.size()) break;
            HostVar hv;
            hv.name      = t[i + 1].text;
            hv.type      = T_FLOAT;
            hv.width     = width;
            hv.capacity  = width;
            hv.is_signed = true;
            hv.c_decl    = type_word + " " + hv.name + ";";
            out.push_back(hv);
            i += 2;
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
            hv.c_decl    = (sgn ? "" : "unsigned ") +
                           (type_word == "longlong" ? "long long" : type_word) +
                           " " + hv.name + ";";
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
