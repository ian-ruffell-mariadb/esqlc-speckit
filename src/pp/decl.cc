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




// T862, T863, T864 — the infix CHARACTER SET clause.
//
// §2 p.2-22 writes `char CHARACTER SET ISO88591 type_picx1[11];` and p.2-25
// writes `struct { short len; char CHARACTER SET KANJI val[10]; }`, so the
// clause sits between the type keyword and the NAME. It is the first infix
// construct this parser has met, and Gate 7's VARCHAR shape check is
// positional, so both callers consume it here rather than each coping alone.
//
// `CHARACTER SET [ IS ] charset` per p.2-24 — IS is optional.
//
// Consumes the clause if present and returns the keyword's table entry, or
// nullptr for "no clause" (which p.2-24 makes equivalent to UNKNOWN). `*bad`
// receives the offending token when a keyword is present but unrecognised, so
// the caller can name it in ESQLC-2006.
const CharsetKeyword *take_charset(const std::vector<Tok> &t, std::size_t &i,
                                   std::string *bad, Pos *at) {
    if (i + 2 >= t.size()) return nullptr;
    if (t[i].text != "CHARACTER" || t[i + 1].text != "SET") return nullptr;
    std::size_t j = i + 2;
    if (j < t.size() && t[j].text == "IS") ++j;      // p.2-24: optional
    if (j >= t.size()) return nullptr;
    *at  = t[j].pos;
    *bad = t[j].text;
    const CharsetKeyword *cs = charset_lookup(t[j].text);
    i = j + 1;                                       // consumed either way, so
                                                     // the caller can diagnose
                                                     // and keep parsing
    return cs;
}

}  // namespace

void parse_declare_section(const std::string &body, Pos at,
                           std::vector<HostVar> &out, Diag &d) {
    auto t = tokenize(body, at);
    std::size_t i = 0;
    while (i < t.size()) {
        // Skip stray semicolons the caller left in.
        if (t[i].text == ";") { ++i; continue; }

        // T871 — NATIONAL CHARACTER [VARYING] means the system default
        // multibyte character set, which §2 p.2-3 says is KANJI "unless it is
        // otherwise set or changed during system generation". SD-14 refuses
        // KANJI because the manual names no encoding for it, so these refuse
        // with it — and "system generation" has nothing to consult here even if
        // it did not.
        //
        // ESQLC-1012 rather than ESQLC-2014: FR-002.5 and FR-002.7 are out of
        // this slice, and 1012 is how the project refuses anything out of
        // scope. The message names the dependency so the reason is not a
        // mystery — the previous behaviour reported "scalar char host variable
        // is not implemented", which is true and useless.
        if (t[i].text == "NATIONAL" ||
            (t[i].text == "char" && i + 1 < t.size() && t[i + 1].text == "NATIONAL")) {
            d.error("ESQLC-1012", t[i].pos,
                    "NATIONAL CHARACTER uses the system default multibyte character "
                    "set, which is KANJI (§2 p.2-3); KANJI has no chosen encoding "
                    "here (ESQLC-2014), so NATIONAL CHARACTER is not implemented. "
                    "Owned by feature 002");
            while (i < t.size() && t[i].text != ";") ++i;
            continue;
        }

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
        // T980 — a TAGGED structure is a record, not a VARCHAR.
        //
        // Gate 7 taught this parser one structure shape: the anonymous
        // `struct { short len; char val[n]; }` of a VARCHAR column. Gate 9's
        // INVOKE generates a *named* one whose fields are each a host variable,
        // referenced as `:tag.field` (FR-006.8) — so the two shapes must be
        // told apart before Gate 7's check runs, or a generated record is
        // rejected as "not a VARCHAR structure".
        //
        // Not in the plan's component list. Recorded as a deviation: the plan
        // said re-parse the generated text through decl.cc, and did not notice
        // decl.cc would have to learn a second shape to do it.
        //
        // The interior is parsed by recursing into this same function and
        // prefixing each harvested name with the variable's, so a nested
        // VARCHAR group inside a record goes down Gate 7's path unchanged.
        if (t[i].text == "struct" && i + 2 < t.size() &&
            t[i + 1].text != "{" && t[i + 2].text == "{") {
            std::size_t open = i + 2, depth = 0, j = open;
            for (; j < t.size(); ++j) {
                if (t[j].text == "{") ++depth;
                else if (t[j].text == "}") { if (--depth == 0) break; }
            }
            if (j >= t.size() || j + 1 >= t.size()) {
                d.error("ESQLC-2003", t[i].pos,
                        "unterminated structure declaration");
                break;
            }
            const std::string var = t[j + 1].text;
            // Rebuild the interior text from its tokens. Spacing is
            // insignificant to the tokenizer, and `[`/`]`/`;` must not be
            // glued to their neighbours.
            std::string inner;
            for (std::size_t z = open + 1; z < j; ++z) {
                inner += t[z].text;
                inner += ' ';
            }
            std::vector<HostVar> fields;
            parse_declare_section(inner, t[i].pos, fields, d);
            for (auto &f : fields) {
                f.name = var + "." + f.name;      // FR-006.8
                out.push_back(f);
            }
            i = j + 2;
            while (i < t.size() && t[i].text != ";") ++i;
            continue;
        }

        if (t[i].text == "struct") {
            Pos sp = t[i].pos;
            // T864 — a moving cursor rather than fixed offsets.
            //
            // Gate 7 matched this shape positionally, which was fine until the
            // CHARACTER SET clause inserted three tokens (four with IS) into
            // the middle of it. §2 p.2-25 writes exactly that:
            //     struct { short len; char CHARACTER SET KANJI val[10]; }
            // so the interruption is the manual's own example, not an edge case.
            std::size_t p = i;
            auto want = [&](const char *lit) {
                if (p < t.size() && t[p].text == lit) { ++p; return true; }
                return false;
            };
            std::string cs_bad; Pos cs_at{}; const CharsetKeyword *cs = nullptr;
            bool had_clause = false;
            bool shape = want("struct") && want("{");
            // The length field's TYPE is consumed as *any* token and checked
            // below. Requiring "short" here made ESQLC-2002 unreachable: a
            // `int len` field failed the shape match and came out as
            // ESQLC-2003 "not a VARCHAR structure", which is the wrong
            // diagnostic and lost the specific advice p.2-9 gives.
            std::size_t len_type = p;
            if (shape) {
                shape = (p + 1 < t.size());
                if (shape) { ++p; ++p; }             // the type, then `len`
                shape = shape && want(";") && want("char");
            }
            if (shape) {
                had_clause = (p + 1 < t.size() && t[p].text == "CHARACTER"
                                               && t[p + 1].text == "SET");
                cs = take_charset(t, p, &cs_bad, &cs_at);
            }
            std::size_t val_tok = p;
            std::string n_tok;
            if (shape) {
                shape = (p < t.size() && t[p].text == "val");
                if (shape) ++p;
                shape = shape && want("[");
                if (shape && p < t.size()) { n_tok = t[p].text; ++p; } else shape = false;
                shape = shape && want("]") && want(";") && want("}");
            }
            if (!shape || p >= t.size()) {
                d.error("ESQLC-2003", sp,
                        "only a VARCHAR structure (short len; char val[n];) may be "
                        "used as a host variable");
                while (i < t.size() && t[i].text != ";") ++i;
                continue;
            }
            // T764 (Gate 7) — FR-002.21: the length field must be `short`.
            if (t[len_type].text != "short") {
                d.error("ESQLC-2002", t[len_type].pos,
                        "a hand-declared VARCHAR length field must be 'short', not '" +
                        t[len_type].text + "'");
                while (i < t.size() && t[i].text != ";") ++i;
                continue;
            }
            if (had_clause && !cs) {
                d.error("ESQLC-2006", cs_at,
                        "unrecognised character-set keyword '" + cs_bad +
                        "'; expected ISO8859n (n = 1-9), KANJI, KSC5601 or UNKNOWN");
                while (i < t.size() && t[i].text != ";") ++i;
                continue;
            }
            if (cs && cs->cls == CsClass::Unmapped) {
                d.error("ESQLC-2013", cs_at,
                        "character set " + cs_bad + " has no MariaDB counterpart; "
                        "ISO88591, ISO88592, ISO88597, ISO88598, ISO88599 and "
                        "KSC5601 are supported");
                while (i < t.size() && t[i].text != ";") ++i;
                continue;
            }
            if (cs && cs->cls == CsClass::Unspecified) {
                d.error("ESQLC-2014", cs_at,
                        "character set KANJI names a script and the manual specifies "
                        "no encoding; MariaDB offers sjis, cp932, ujis and eucjpms, "
                        "which differ in byte length and repertoire, so no mapping "
                        "is chosen");
                while (i < t.size() && t[i].text != ";") ++i;
                continue;
            }
            {
                long n = std::strtol(n_tok.c_str(), nullptr, 10);
                if (n <= 1) {
                    d.error("ESQLC-2009", t[val_tok].pos,
                            "VARCHAR val array size must exceed 1");
                    while (i < t.size() && t[i].text != ";") ++i;
                    continue;
                }
                HostVar hv;
                hv.name      = t[p].text;
                hv.type      = T_CHAR_VAR;
                hv.charset   = cs ? cs->id : 0;
                hv.capacity  = (unsigned)n;
                hv.width     = (unsigned)n - 1;      // SD-10
                hv.is_signed = true;
                hv.c_decl    = "struct { short len; char val[" + n_tok + "]; } " +
                               hv.name + ";";
                out.push_back(hv);
            }
            i = p + 1;
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
            // char [CHARACTER SET cs] name[n];
            if (i + 1 >= t.size()) break;
            std::size_t k = i + 1;
            std::string cs_bad; Pos cs_at{};
            bool had_clause = (k + 1 < t.size() && t[k].text == "CHARACTER"
                                                && t[k + 1].text == "SET");
            const CharsetKeyword *cs = take_charset(t, k, &cs_bad, &cs_at);
            if (had_clause && !cs) {
                d.error("ESQLC-2006", cs_at,
                        "unrecognised character-set keyword '" + cs_bad +
                        "'; expected ISO8859n (n = 1-9), KANJI, KSC5601 or UNKNOWN");
                while (i < t.size() && t[i].text != ";") ++i;
                continue;
            }
            if (cs && cs->cls == CsClass::Unmapped) {
                // The gap is MariaDB's. Distinct from ESQLC-2014 so the reader
                // is sent to MariaDB's charset list rather than to SQLRM.
                d.error("ESQLC-2013", cs_at,
                        "character set " + cs_bad + " has no MariaDB counterpart; "
                        "ISO88591, ISO88592, ISO88597, ISO88598, ISO88599 and "
                        "KSC5601 are supported");
                while (i < t.size() && t[i].text != ";") ++i;
                continue;
            }
            if (cs && cs->cls == CsClass::Unspecified) {
                // The gap is the manual's: KANJI names a script, not an
                // encoding, and MariaDB offers four that differ in byte length
                // and repertoire. Guessing would store different characters
                // than the program wrote, silently. SD-14, DIV-055.
                d.error("ESQLC-2014", cs_at,
                        "character set KANJI names a script and the manual specifies "
                        "no encoding; MariaDB offers sjis, cp932, ujis and eucjpms, "
                        "which differ in byte length and repertoire, so no mapping "
                        "is chosen");
                while (i < t.size() && t[i].text != ";") ++i;
                continue;
            }
            HostVar hv;
            hv.charset = cs ? cs->id : 0;   // p.2-24: no clause == UNKNOWN == 0
            hv.name = t[k].text;
            std::size_t j = k + 1;
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
