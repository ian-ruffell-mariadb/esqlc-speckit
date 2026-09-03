// T070 — INSERT handler: body emitted with recorded spans replaced by '?',
//        plus an ordered descriptor array. The body is never parsed.
// T071 — BEGIN/COMMIT/ROLLBACK WORK handlers.
// T072 — verbatim C regions, generated blocks, #line restoration.
// T073 — width/signedness static assertions per host variable.
// T074 — only esqlc_-prefixed references in emitted code.
#include "pp.h"
#include <sstream>
#include <algorithm>
#include <map>
#include <cstring>
#include <cctype>

namespace pp {
namespace {

std::string c_string_literal(const std::string &s) {
    std::string o = "\"";
    for (char ch : s) {
        switch (ch) {
            case '"':  o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n";  break;
            case '\r': break;
            case '\t': o += " ";    break;
            default:   o += ch;
        }
    }
    o += "\"";
    return o;
}

std::string collapse_ws(const std::string &s) {
    std::string o;
    bool sp = false;
    for (char ch : s) {
        if (std::isspace((unsigned char)ch)) { sp = !o.empty(); continue; }
        if (sp) { o += ' '; sp = false; }
        o += ch;
    }
    return o;
}

const HostVar *find(const std::vector<HostVar> &v, const std::string &name) {
    // A structure field reference (:s.f) resolves on its full spelling.
    for (const auto &h : v) if (h.name == name) return &h;
    return nullptr;
}

// Replace each recorded host-variable span with '?'. This is splicing at
// offsets the lexer already found — no SQL parsing (NFR-001.1, FR-003.10).
std::string placeholderise(const Construct &k) {
    std::string out;
    std::size_t prev = 0;
    for (const auto &hv : k.hostvars) {
        out += k.body.substr(prev, hv.begin - prev);
        out += "?";
        prev = hv.end;
    }
    out += k.body.substr(prev);
    return out;
}

// A reference, classified. `ind` indexes the reference that serves as this
// one's indicator, or -1.
struct Classified {
    std::size_t ref;          // index into k.hostvars
    bool        is_out;
    int         ind = -1;     // index into k.hostvars, or -1
};

// Is the gap between two references an indicator association rather than a
// separate list item? `:v :i` and `:v INDICATOR :i` associate; `:a, :b` does
// not, because a comma separates list items (FR-002.15).
bool associates(const std::string &body, std::size_t from, std::size_t to) {
    std::string gap = body.substr(from, to - from);
    if (gap.find(',') != std::string::npos) return false;
    std::string up;
    for (char ch : gap) up += (char)std::toupper((unsigned char)ch);
    // strip whitespace
    std::string bare;
    for (char ch : up) if (!std::isspace((unsigned char)ch)) bare += ch;
    return bare.empty() || bare == "INDICATOR";
}

// Classify every reference by landmark. Applied ONLY by the SELECT handler:
// `INSERT INTO parts` also contains the INTO landmark, and letting this run
// there would silently turn Gate 1's inputs into outputs.
std::vector<Classified> classify(const Construct &k) {
    std::vector<Classified> out;
    const std::size_t lo = k.into_off;
    const std::size_t hi = (k.from_off == Construct::npos) ? k.body.size() : k.from_off;

    std::vector<bool> consumed(k.hostvars.size(), false);
    for (std::size_t i = 0; i < k.hostvars.size(); ++i) {
        if (consumed[i]) continue;
        const auto &r = k.hostvars[i];
        bool is_out = (lo != Construct::npos && r.begin >= lo && r.begin < hi);
        Classified c{i, is_out, -1};
        if (is_out && i + 1 < k.hostvars.size()) {
            const auto &n = k.hostvars[i + 1];
            if (n.begin < hi && associates(k.body, r.end, n.begin)) {
                c.ind = (int)(i + 1);
                consumed[i + 1] = true;
            }
        }
        out.push_back(c);
    }
    return out;
}

// Build the statement actually sent: the INTO clause is a binding instruction,
// not SQL, so it is removed; inputs become placeholders at their recorded
// spans. Still splicing at known offsets — nothing parses the body.
std::string select_sql(const Construct &k, const std::vector<Classified> &cls) {
    const std::size_t lo = k.into_off;
    const std::size_t hi = (k.from_off == Construct::npos) ? k.body.size() : k.from_off;
    std::vector<std::pair<std::size_t, std::size_t>> subs;   // spans to replace with ?
    for (const auto &c : cls)
        if (!c.is_out) subs.push_back({k.hostvars[c.ref].begin, k.hostvars[c.ref].end});

    std::string out;
    std::size_t i = 0;
    while (i < k.body.size()) {
        if (lo != Construct::npos && i == lo) { i = hi; continue; }   // drop INTO clause
        bool replaced = false;
        for (const auto &s : subs) {
            if (i == s.first) { out += '?'; i = s.second; replaced = true; break; }
        }
        if (replaced) continue;
        out += k.body[i++];
    }
    return out;
}

// ---- cursor support (T362-T368) ---------------------------------------
// What a DECLARE records for its later OPEN and FETCH. This is the first
// cross-construct state the preprocessor has held: DECLARE is a declaration
// carrying the text, OPEN is where that text runs.
struct CursorInfo {
    std::string symbol;                 // name of the emitted static const
    std::vector<std::string> inputs;    // host variables in the WHERE clause
    std::size_t sql_len = 0;
};

// Find a keyword at a token boundary, skipping "..." strings. The body has
// already had comments removed by the scanner, so only strings need care.
std::size_t find_kw(const std::string &b, const char *kw) {
    const std::size_t n = std::strlen(kw);
    for (std::size_t i = 0; i < b.size(); ++i) {
        if (b[i] == '"') {                       // skip a string literal
            ++i;
            while (i < b.size() && b[i] != '"') ++i;
            continue;
        }
        if (i + n > b.size()) break;
        bool hit = true;
        for (std::size_t k = 0; k < n; ++k)
            if (std::toupper((unsigned char)b[i + k]) != kw[k]) { hit = false; break; }
        if (!hit) continue;
        if (i > 0 && (std::isalnum((unsigned char)b[i - 1]) || b[i - 1] == '_')) continue;
        std::size_t after = i + n;
        if (after < b.size() && (std::isalnum((unsigned char)b[after]) || b[after] == '_')) continue;
        return i;
    }
    return std::string::npos;
}

// The identifier following a leading verb: OPEN <name>, CLOSE <name>,
// FETCH <name> INTO …, DECLARE <name> CURSOR …
std::string name_after_verb(const std::string &body, const char *verb) {
    std::size_t p = find_kw(body, verb);
    if (p == std::string::npos) return {};
    p += std::strlen(verb);
    while (p < body.size() && std::isspace((unsigned char)body[p])) ++p;
    std::string id;
    while (p < body.size() && (std::isalnum((unsigned char)body[p]) || body[p] == '_'))
        id += body[p++];
    return id;
}

// The bare word at or after `from`, uppercased. Used to read the token after
// VERSION, which is either a number or CURRENT.
std::string word_after(const std::string &b, std::size_t from) {
    while (from < b.size() && std::isspace((unsigned char)b[from])) ++from;
    std::string w;
    while (from < b.size() && (std::isalnum((unsigned char)b[from]) || b[from] == '_'))
        w += (char)std::toupper((unsigned char)b[from++]);
    return w;
}

// Like placeholderise, but an associated indicator reference is removed rather
// than turned into a placeholder: `SET weight = :w :ind` sends one parameter,
// not two. The indicator is a binding instruction, never SQL (FR-002.15).
std::string placeholderise_inputs(
        const Construct &k,
        const std::vector<std::pair<std::size_t, int>> &items) {
    std::vector<std::pair<std::size_t, std::size_t>> ph, drop;
    for (const auto &it : items) {
        ph.push_back({k.hostvars[it.first].begin, k.hostvars[it.first].end});
        if (it.second >= 0) {
            const auto &i = k.hostvars[(std::size_t)it.second];
            drop.push_back({i.begin, i.end});
        }
    }
    std::string out;
    std::size_t i = 0;
    while (i < k.body.size()) {
        bool done = false;
        for (const auto &p : ph)
            if (i == p.first) { out += '?'; i = p.second; done = true; break; }
        if (done) continue;
        for (const auto &p : drop)
            if (i == p.first) { i = p.second; done = true; break; }
        if (done) continue;
        out += k.body[i++];
    }
    return out;
}

// Like word_after but preserving case: an object name and a structure tag are
// identifiers, not keywords.
std::string word_after_raw(const std::string &b, std::size_t from) {
    while (from < b.size() && std::isspace((unsigned char)b[from])) ++from;
    std::string w;
    while (from < b.size() &&
           (std::isalnum((unsigned char)b[from]) || b[from] == '_' ||
            b[from] == '.' || b[from] == '=' || b[from] == '$' || b[from] == '\\'))
        w += b[from++];
    return w;
}

const char *type_macro(unsigned t) {
    switch (t) {
        case 1: return "ESQLC_T_CHAR_FIXED";
        case 2: return "ESQLC_T_INT";
        case 3: return "ESQLC_T_CHAR_VAR";   // Gate 7: VARCHAR structures
        case 5: return "ESQLC_T_FLOAT";      // Gate 7: float and double
        default: return "0";
    }
}


// T865 — remove the CHARACTER SET clause from emitted C.
//
// §1 p.1-2: "The C compiler accepts the CHARACTER SET clause in a host-variable
// declaration to associate a single-byte or double-byte character set such as
// Kanji or KSC5601 with a host variable." On NonStop the clause is a *C
// compiler extension*, so the preprocessor could leave it in place and the
// declaration would still compile.
//
// gcc and clang have no such extension. `char CHARACTER SET ISO88591 a[9];` is
// not valid C here, so the clause is consumed like `#pragma SQL` is (FR-001.7)
// rather than passed through like everything else in a C region (FR-001.19).
// The association is not lost: it has already been recorded in the descriptor's
// charset field, which is where the runtime reads it.
//
// Recorded in DIV-055 — it is the one place where a declaration a customer
// wrote is not emitted byte-for-byte, and Principle II makes that worth saying
// out loud rather than doing quietly.
std::string strip_charset_clauses(const std::string &c) {
    std::string out;
    std::size_t i = 0;
    while (i < c.size()) {
        // Match CHARACTER, whitespace, SET at a token boundary.
        if ((c[i] == 'C' || c[i] == 'c') &&
            (i == 0 || !(std::isalnum((unsigned char)c[i - 1]) || c[i - 1] == '_'))) {
            static const char kw[] = "CHARACTER";
            std::size_t j = i, n = 0;
            while (j < c.size() && n < 9 &&
                   std::toupper((unsigned char)c[j]) == kw[n]) { ++j; ++n; }
            if (n == 9) {
                std::size_t k = j;
                while (k < c.size() && std::isspace((unsigned char)c[k])) ++k;
                std::size_t m = k, sn = 0;
                static const char kset[] = "SET";
                while (m < c.size() && sn < 3 &&
                       std::toupper((unsigned char)c[m]) == kset[sn]) { ++m; ++sn; }
                if (sn == 3 && k != j) {
                    // Optional IS, then the keyword. Both consumed.
                    std::size_t q = m;
                    while (q < c.size() && std::isspace((unsigned char)c[q])) ++q;
                    std::size_t w = q;
                    while (w < c.size() && (std::isalnum((unsigned char)c[w]) || c[w] == '_')) ++w;
                    std::string first = c.substr(q, w - q);
                    std::string up;
                    for (char ch : first) up += (char)std::toupper((unsigned char)ch);
                    if (up == "IS") {
                        q = w;
                        while (q < c.size() && std::isspace((unsigned char)c[q])) ++q;
                        w = q;
                        while (w < c.size() && (std::isalnum((unsigned char)c[w]) || c[w] == '_')) ++w;
                    }
                    // Newlines inside the clause are preserved so #line stays
                    // honest; everything else becomes a single space.
                    for (std::size_t z = i; z < w; ++z)
                        if (c[z] == '\n') out += '\n';
                    out += ' ';
                    i = w;
                    continue;
                }
            }
        }
        out += c[i++];
    }
    return out;
}

}  // namespace

std::string emit(const std::string &file, const ScanResult &sr,
                 std::vector<HostVar> &vars, Diag &d,
                 const std::string &schema_path) {
    std::ostringstream o;

    // FR-001.7: the pragma is mandatory and must precede all SQL and C.
    if (!sr.pragma_seen) {
        d.error("ESQLC-1005", Pos{1, 1},
                "missing '#pragma SQL': a unit with embedded SQL must declare it");
        return {};
    }
    if (sr.saw_code_before_pragma) {
        d.error("ESQLC-1006", sr.pragma_pos,
                "'#pragma SQL' must precede all SQL and C statements");
    }

    o << "/* generated by esqlcpp from " << file << " — do not edit */\n";
    o << "#include \"esqlc.h\"\n";

    bool in_declare = false;
    // Gate 9. Loaded lazily: a unit with no INVOKE never reads the cache, so a
    // build without --schema is only an error where INVOKE needs it.
    Schema   schema;
    bool     schema_tried = false;
    SchemaErr schema_err  = SchemaErr::None;
    int  hv_seq = 0;
    std::map<std::string, CursorInfo> cursors;   // name -> declaration (T362)
    bool at_line_start = true;
    WheneverState when;          // Gate 4: per-condition action table
    bool sqlca_declared = false;
    bool sqlca_pending  = false;   // declared, not yet registered
    // Gate 5. The SQLSA exists only from version 300, so the selected version
    // is state the INCLUDE SQLSA handler needs and cannot infer.
    bool sqlsa_declared    = false;
    bool sqlsa_pending     = false;
    int  sqlsa_version     = 0;
    bool structures_seen   = false;
    int  structures_version = 2;   // FR-005.10: version 2 absent a directive

    // A #line directive must begin a line — only whitespace may precede the #.
    // An EXEC SQL sitting after other C on the same source line would otherwise
    // emit `... ; #line 17 "..."`, which is a syntax error. Gate 1's fixtures
    // all started their statements at column 1, so this never showed until
    // Gate 2 wrote `{ long s = sqlcode; EXEC SQL ROLLBACK WORK; }`.
    auto line_directive = [&](int line) {
        if (!at_line_start) o << "\n";
        o << "#line " << line << " \"" << file << "\"\n";
        at_line_start = true;
    };
    auto track = [&](const std::string &t) {
        if (!t.empty()) at_line_start = (t.back() == '\n');
    };
    // Close a generated statement block: refresh sqlcode, then append the
    // WHENEVER checks if this statement class carries them (SD-5).
    auto close_stmt = [&](const std::string &keyword) {
        o << "  sqlcode = esqlc_sqlcode();\n";
        if (whenever_applies(keyword)) o << whenever_checks(when);
        o << "} while (0);\n";
        at_line_start = true;
    };

    for (const auto &ch : sr.chunks) {
        if (!ch.is_sql) {
            line_directive(ch.pos.line);
            // Inside a declare section the CHARACTER SET clause is consumed
            // rather than passed through: it is a NonStop C compiler extension
            // (§1 p.1-2) that gcc and clang do not have. Everywhere else the C
            // region is emitted byte-for-byte (FR-001.19).
            const std::string ctext =
                in_declare ? strip_charset_clauses(ch.c_text) : ch.c_text;
            o << ctext;
            track(ctext);
            if (in_declare) {
                // Harvest descriptors from the declarations we just emitted.
                parse_declare_section(ch.c_text, ch.pos, vars, d);
            }
            continue;
        }

        const Construct &k = ch.sql;
        const Handler *h = lookup(k.keyword);
        if (!h) {
            d.error("ESQLC-1009", k.pos,
                    "unrecognised SQL statement or directive '" + k.keyword + "'");
            continue;
        }
        if (h->owning_feature) {
            d.error("ESQLC-1012", k.pos,
                    "'" + k.keyword + "' is not implemented in this slice; owned by "
                    "feature " + h->owning_feature);
            continue;
        }
        if (h->where != PosClass::Any && h->where != k.where) {
            d.error("ESQLC-1008", k.pos,
                    "'" + k.keyword + "' must appear in " +
                    (h->where == PosClass::Decl ? "declaration" : "executable") +
                    " position");
            continue;
        }

        // ---- INVOKE (T979-T984) -----------------------------------------
        if (k.keyword == "INVOKE") {
            if (!schema_tried) {
                schema_err = schema_read(schema_path, schema);
                schema_tried = true;
            }
            if (schema_err == SchemaErr::Absent) {
                d.error("ESQLC-6002", k.pos,
                        "INVOKE needs a schema cache and none was given; pass "
                        "--schema <file>. The preprocessor never opens a database "
                        "connection (NFR-001.2, NFR-006.2)");
                continue;
            }
            if (schema_err == SchemaErr::Unreadable) {
                d.error("ESQLC-6008", k.pos,
                        "cannot read the schema cache '" + schema_path + "'");
                continue;
            }
            // `INVOKE <object> AS <tag>` — the object is the first word of the
            // body, the tag the word after AS. Read as landmarks, in the shape
            // name_after_verb established, rather than by parsing.
            // The body includes the leading keyword, so skip it — the same
            // shape name_after_verb uses for OPEN/CLOSE/FETCH.
            std::size_t vp = find_kw(k.body, "INVOKE");
            std::string object =
                word_after_raw(k.body, vp == std::string::npos ? 0 : vp + 6);
            std::size_t ap = find_kw(k.body, "AS");
            std::string tag = (ap == std::string::npos)
                                ? object : word_after_raw(k.body, ap + 2);
            if (object.empty() || tag.empty()) {
                d.error("ESQLC-6001", k.pos,
                        "INVOKE needs an object name and a structure tag");
                continue;
            }
            if (object[0] == '=') {
                // FR-006.2d is out of slice: DIV-002 still has TACL DEFINEs
                // unresolved, and the manual's own examples use `=customer`.
                d.error("ESQLC-1012", k.pos,
                        "a class MAP DEFINE ('" + object + "') for an invoked "
                        "object is not implemented in this slice; DIV-002 has "
                        "TACL DEFINEs unresolved. Owned by feature 006");
                continue;
            }
            const std::vector<SchemaColumn> *cols = schema.find(object);
            if (!cols) {
                d.error("ESQLC-6001", k.pos,
                        "invoked object '" + object +
                        "' is not in the schema cache");
                continue;
            }
            std::string gen = invoke_generate(object, tag, *cols,
                                              schema.captured, k.pos, d);
            if (gen.empty()) continue;      // invoke_generate diagnosed

            // T981 — the #line for this construct, then the generated text.
            // Generated lines have no counterpart in the source, so the emitted
            // text carries none: the next verbatim C chunk restores the
            // directive from its own recorded position. Gates 1 and 2 both hit
            // the drifting-#line defect and generated multi-line output is the
            // largest opportunity for it yet.
            // The CHARACTER SET clause is a NonStop C compiler extension
            // (§1 p.1-2) that gcc and clang do not have, so it is stripped from
            // emitted C exactly as Gate 8 strips a hand-written one. The
            // association is not lost: the RE-PARSE below reads the clause
            // first and records the set in the descriptor.
            //
            // Order matters and cost me a build: parse the clause-bearing text,
            // emit the stripped text.
            parse_declare_section(gen, k.pos, vars, d);
            o << strip_charset_clauses(gen);
            at_line_start = true;

            continue;
        }

        if (k.keyword == "BEGIN DECLARE SECTION") { in_declare = true;  continue; }
        if (k.keyword == "END DECLARE SECTION")   { in_declare = false; continue; }

        line_directive(k.pos.line);

        // Registration is a *call*, so it may only be emitted where a call can
        // run. The flush used to trigger on the first construct of any kind,
        // which was fine until a fixture put a DECLARE CURSOR — a declaration
        // that emits no executable code — between the INCLUDE and the first
        // statement. The call then landed at file scope and the unit would not
        // compile. Gate 4 never saw it because no fixture had that ordering.
        const bool at_exec = (h->where == PosClass::Exec);

        if (sqlca_pending && at_exec) {
            // The SQLCA is declared at file scope, where no call can run, so
            // registration is emitted here — before the first statement that
            // could populate it. A dead helper function was the first attempt
            // and never executed.
            o << "esqlc_sqlca_register(&sqlca, SQLCA_LEN);\n";
            sqlca_pending = false;
            at_line_start = true;
        }

        if (sqlsa_pending && at_exec) {
            // Same reasoning as the SQLCA above: file scope runs no code, so
            // registration is emitted before the first statement that could
            // populate the area.
            o << "esqlc_sqlsa_register(&sqlsa, "
              << (sqlsa_version == 330 ? "SQLSA_LEN_R330" : "SQLSA_LEN")
              << ", " << sqlsa_version << ");\n";
            sqlsa_pending = false;
            at_line_start = true;
        }

        if (k.keyword == "BEGIN WORK" || k.keyword == "COMMIT WORK" ||
            k.keyword == "ROLLBACK WORK") {
            const char *fn = k.keyword == "BEGIN WORK"  ? "esqlc_txn_begin"
                           : k.keyword == "COMMIT WORK" ? "esqlc_txn_commit"
                                                        : "esqlc_txn_rollback";
            o << "do { " << fn << "();\n";
            close_stmt(k.keyword);          // SD-5: no checks for these
            continue;
        }

        // ---- WHENEVER (T470-T472, T475) ---------------------------------
        if (k.keyword == "WHENEVER") {
            whenever_set(when, k, d);      // emits nothing itself
            continue;
        }

        // ---- INCLUDE STRUCTURES (T477, T566) ----------------------------
        // Gate 5 implements version selection. FR-005.12: the directive must
        // precede any INCLUDE SQLCA/SQLSA/SQLDA.
        if (k.keyword == "INCLUDE STRUCTURES") {
            if (sqlca_declared || sqlsa_declared) {
                d.error("ESQLC-5001", k.pos,
                        "INCLUDE STRUCTURES must precede any INCLUDE SQLCA, "
                        "SQLSA or SQLDA directive");
                continue;
            }
            // The body is `[ALL] VERSION v` or per-structure `NAME VERSION v`,
            // possibly several. Only what the slice needs is read: which
            // structure, and which version.
            bool names_sqlsa = find_kw(k.body, "SQLSA") != std::string::npos;
            bool names_sqlca = find_kw(k.body, "SQLCA") != std::string::npos;
            std::size_t vp = find_kw(k.body, "VERSION");
            if (vp == std::string::npos) {
                d.error("ESQLC-1012", k.pos,
                        "INCLUDE STRUCTURES without VERSION is not implemented "
                        "in this slice; owned by feature 005 (diagnostics)");
                continue;
            }
            std::string tok = word_after(k.body, vp + 7);
            if (tok == "CURRENT") {
                // FR-005.26 is out of slice, and ESQLC-5007 is the correct
                // refusal rather than a generic ESQLC-1012: the registered
                // condition is "VERSION CURRENT without a reachable
                // SQLGETSYSTEMVERSION", and there is never a reachable one.
                d.error("ESQLC-5007", k.pos,
                        "SQLSA VERSION CURRENT requires SQLGETSYSTEMVERSION, "
                        "which has no analogue here");
                continue;
            }
            int v = 0;
            for (char c : tok) {
                if (c < '0' || c > '9') { v = -1; break; }
                v = v * 10 + (c - '0');
            }
            if (v <= 0) {
                d.error("ESQLC-5002", k.pos,
                        "unsupported structure version '" + tok +
                        "' (SQL error 11203)");
                continue;
            }
            // FR-005.9. 330 exists for the SQLSA only, so the two failures are
            // distinct diagnostics rather than one vague refusal.
            if (v == 330 && !names_sqlsa) {
                d.error("ESQLC-5003", k.pos,
                        std::string("version 330 is defined for SQLSA only, not for ") +
                        (names_sqlca ? "SQLCA" : "the named structure"));
                continue;
            }
            if (!sqlsa_version_ok(v, names_sqlsa)) {
                d.error("ESQLC-5002", k.pos,
                        "unsupported structure version " + tok +
                        " (SQL error 11203); accepted versions are 1, 2, 300, "
                        "330 (SQLSA only) and 340 or later");
                continue;
            }
            structures_version = v;
            structures_seen    = true;
            continue;
        }

        // ---- INCLUDE SQLSA (T567) ---------------------------------------
        if (k.keyword == "INCLUDE SQLSA") {
            if (sqlsa_declared) {
                d.error("ESQLC-5005", k.pos,
                        "more than one non-EXTERNAL SQLSA declaration");
                continue;
            }
            sqlsa_declared = true;
            line_directive(k.pos.line);
            // The SQLSA exists only from version 300. Absent an INCLUDE
            // STRUCTURES the default is version 2 (FR-005.10), which has no
            // SQLSA to declare, so this is a refusal and not an assumption.
            if (!structures_seen || structures_version < 300) {
                d.error("ESQLC-5002", k.pos,
                        "INCLUDE SQLSA requires INCLUDE STRUCTURES with version "
                        "300 or later; version 2 has no SQLSA");
                continue;
            }
            int v = (structures_version == 330) ? 330 : 300;
            o << sqlsa_layout();
            o << "struct " << (v == 330 ? "SQLSA_TYPE_R330" : "SQLSA_TYPE")
              << " sqlsa;\n";
            sqlsa_version = v;
            sqlsa_pending = true;   // registered at the first statement below
            at_line_start = true;
            continue;
        }

        // ---- INCLUDE SQLCA (T476, T477) ---------------------------------
        if (k.keyword == "INCLUDE SQLCA") {
            if (sqlca_declared) {
                d.error("ESQLC-5005", k.pos,
                        "more than one non-EXTERNAL SQLCA declaration");
                continue;
            }
            sqlca_declared = true;
            line_directive(k.pos.line);
            // FR-005.10: absent an INCLUDE STRUCTURES, version 2 is generated
            // and the compiler says so. Informational, not an error.
            //
            // Gate 4 emitted this unconditionally, which was correct then only
            // because INCLUDE STRUCTURES always failed — the directive could
            // never actually be honoured, so the message was never wrong. Gate
            // 5 makes version selection work, so the condition the requirement
            // states has to be honoured too, or every program that does the
            // right thing is told it did not.
            if (!structures_seen) {
                d.info("ESQLC-5006", k.pos,
                       "INCLUDE STRUCTURES directive for SQL is missing; SQL VERSION 2 "
                       "is assumed, which may produce incorrect results for features "
                       "introduced after version 2");
            }
            o << "#define SQLCA_EYE_CATCHER \"CA\"\n";
            o << "#define SQLCA_LEN 430\n";
            // DIV-041: the layout is this implementation's, so only the total
            // and the eye-catcher are fixed. Programs copy it with SQLCA_LEN
            // and share it EXTERNAL, so the total is load-bearing.
            o << "struct sqlca_type { char eye_catcher[2]; char sqlca_private[428]; };\n";
            o << "struct sqlca_type sqlca;\n";
            o << "_Static_assert(sizeof(struct sqlca_type) == SQLCA_LEN, \"SQLCA_LEN\");\n";
            o << "_Static_assert(offsetof(struct sqlca_type, eye_catcher) == 0, "
                 "\"eye-catcher leads\");\n";
            sqlca_pending = true;   // registered at the first statement below
            at_line_start = true;
            continue;
        }

        // ---- DECLARE <name> CURSOR FOR <select> (T362, T366, T368) ------
        if (k.keyword == "DECLARE CURSOR") {
            std::string cname = name_after_verb(k.body, "DECLARE");
            if (cname.empty()) {
                d.error("ESQLC-4005", k.pos, "cursor declaration has no name");
                continue;
            }
            if (cursors.count(cname)) {
                d.error("ESQLC-4006", k.pos,
                        "cursor '" + cname + "' is already declared");
                continue;
            }
            // FOR UPDATE is a clause of the cursor's statement, so the dispatch
            // table cannot see it; refuse it here (read-only slice).
            if (find_kw(k.body, "UPDATE") != std::string::npos) {
                d.error("ESQLC-1012", k.pos,
                        "FOR UPDATE cursors are not implemented in this slice; "
                        "owned by feature 004 (static DML & cursors)");
                continue;
            }
            std::size_t f = find_kw(k.body, "FOR");
            if (f == std::string::npos) {
                d.error("ESQLC-1009", k.pos,
                        "cursor declaration has no FOR clause");
                continue;
            }
            std::size_t sql_begin = f + 3;

            CursorInfo ci;
            ci.symbol = "__esqlc_cur_" + cname + "_sql";
            // Everything after FOR is the statement; its references are inputs,
            // placeholderised at the spans the lexer recorded.
            std::string sql;
            std::size_t i = sql_begin;
            while (i < k.body.size()) {
                bool sub = false;
                for (const auto &ref : k.hostvars) {
                    if (ref.begin == i && ref.begin >= sql_begin) {
                        sql += '?';
                        ci.inputs.push_back(ref.name);
                        i = ref.end;
                        sub = true;
                        break;
                    }
                }
                if (!sub) sql += k.body[i++];
            }
            sql = collapse_ws(sql);
            ci.sql_len = sql.size();
            cursors[cname] = ci;

            // Emitted where the programmer wrote it. The cast-to-void self
            // reference keeps a declared-but-never-opened cursor from drawing
            // an unused-variable warning in a customer build (T368).
            o << "static const char " << ci.symbol << "[] = "
              << c_string_literal(sql) << ";\n";
            o << "enum { " << ci.symbol << "_used = (int)sizeof " << ci.symbol << " };\n";
            at_line_start = true;
            continue;
        }

        // ---- OPEN / FETCH / CLOSE (T363, T364, T365) --------------------
        if (k.keyword == "OPEN" || k.keyword == "FETCH" || k.keyword == "CLOSE") {
            std::string cname = name_after_verb(k.body, k.keyword.c_str());
            auto it = cursors.find(cname);
            if (it == cursors.end()) {
                d.error("ESQLC-4005", k.pos,
                        "cursor '" + cname + "' is not declared");
                continue;
            }
            const CursorInfo &ci = it->second;

            if (k.keyword == "CLOSE") {
                o << "do { esqlc_cursor_close(\"" << cname << "\");\n";
                close_stmt(k.keyword);
                continue;
            }

            // OPEN binds the cursor's recorded inputs; FETCH binds the
            // references in its own INTO list as outputs.
            std::vector<std::pair<std::string, bool>> binds;   // name, is_out
            if (k.keyword == "OPEN") {
                for (const auto &n : ci.inputs) binds.push_back({n, false});
            } else {
                if (k.into_off == Construct::npos) {
                    d.error("ESQLC-1009", k.pos, "FETCH has no INTO clause");
                    continue;
                }
                for (const auto &ref : k.hostvars)
                    if (ref.begin >= k.into_off) binds.push_back({ref.name, true});
            }

            std::string arr = "__esqlc_hv_" + std::to_string(++hv_seq);
            o << "do {\n";
            bool bad = false;
            if (binds.empty()) {
                o << "  esqlc_hostvar_t *" << arr << " = 0;\n";
            } else {
                o << "  esqlc_hostvar_t " << arr << "[" << binds.size() << "] = {\n";
                for (const auto &b : binds) {
                    const HostVar *hv = find(vars, b.first);
                    if (!hv) {
                        d.error("ESQLC-1014", k.pos,
                                "host variable ':" + b.first +
                                "' is not declared in a declare section");
                        bad = true;
                        continue;
                    }
                    o << "    { &" << hv->name << ", 0, " << type_macro(hv->type)
                      << ", " << hv->width << "u, " << hv->capacity << "u, 0, "
                      << (hv->is_signed ? 1 : 0) << ", "
                      << (b.second ? "ESQLC_DIR_OUT" : "ESQLC_DIR_IN")
                      << ", " << hv->charset << " },\n";   // Gate 8
                }
                o << "  };\n";
            }
            if (bad) { o << "} while (0);\n"; at_line_start = true; continue; }

            if (k.keyword == "OPEN")
                o << "  esqlc_cursor_open(\"" << cname << "\", " << ci.symbol
                  << ", " << ci.sql_len << ", " << arr << ", " << binds.size() << ");\n";
            else
                o << "  esqlc_cursor_fetch(\"" << cname << "\", " << arr
                  << ", " << binds.size() << ");\n";
            close_stmt(k.keyword);
            continue;
        }

        if (k.keyword == "SELECT") {
            // A SELECT with no INTO is a cursor specification, which this
            // slice does not implement. Refuse by name rather than guess.
            if (k.into_off == Construct::npos) {
                d.error("ESQLC-1012", k.pos,
                        "SELECT without an INTO clause is a cursor specification; "
                        "not implemented in this slice, owned by feature 004");
                continue;
            }
            std::vector<Classified> cls = classify(k);
            std::string sql = collapse_ws(select_sql(k, cls));
            std::string arr = "__esqlc_hv_" + std::to_string(++hv_seq);

            o << "do {\n";
            o << "  esqlc_hostvar_t " << arr << "[" << cls.size() << "] = {\n";
            bool bad = false;
            for (const auto &c : cls) {
                const auto &ref = k.hostvars[c.ref];
                const HostVar *hv = find(vars, ref.name);
                if (!hv) {
                    d.error("ESQLC-1014", k.pos,
                            "host variable ':" + ref.name +
                            "' is not declared in a declare section");
                    bad = true;
                    continue;
                }
                std::string ind = "0";
                if (c.ind >= 0) {
                    const auto &iref = k.hostvars[(std::size_t)c.ind];
                    const HostVar *ihv = find(vars, iref.name);
                    if (!ihv) {
                        d.error("ESQLC-1014", k.pos,
                                "indicator variable ':" + iref.name +
                                "' is not declared in a declare section");
                        bad = true;
                        continue;
                    }
                    ind = "&" + ihv->name;
                }
                o << "    { &" << hv->name << ", " << ind << ", "
                  << type_macro(hv->type) << ", " << hv->width << "u, "
                  << hv->capacity << "u, 0, " << (hv->is_signed ? 1 : 0) << ", "
                  << (c.is_out ? "ESQLC_DIR_OUT" : "ESQLC_DIR_IN")
                  << ", " << hv->charset << " },\n";       // Gate 8
            }
            o << "  };\n";
            if (bad) { o << "} while (0);\n"; continue; }
            // NULL table: a SELECT has result-set metadata, so the runtime
            // reads its table names from there (Gate 5's path) and needs no
            // landmark. The landmark exists for DML, which has no metadata.
            o << "  esqlc_stmt_exec(" << c_string_literal(sql) << ", "
              << sql.size() << ", " << arr << ", " << cls.size() << ", NULL);\n";
            close_stmt(k.keyword);
            continue;
        }

        // ---- INSERT / UPDATE / DELETE (T664, T665, T666, T667, T669) ----
        // One handler. The three differ only in which landmark carries the
        // table, which the scanner has already resolved, and in the clause each
        // must refuse. Every reference is an input; indicators associate on the
        // input side exactly as they do on the output side (FR-002.15).
        if (k.keyword == "INSERT" || k.keyword == "UPDATE" || k.keyword == "DELETE") {
            // WHERE CURRENT OF is a clause, not a keyword, so the dispatch
            // table cannot see it — the same reason FOR UPDATE is refused
            // inside the DECLARE CURSOR handler. Positioned operations need
            // 004 Q3 and Q7, and Q3 needs SQLRM.
            if (k.keyword != "INSERT") {
                std::size_t cur = find_kw(k.body, "CURRENT");
                if (cur != std::string::npos) {
                    d.error("ESQLC-1012", k.pos,
                            "positioned " + k.keyword + " (WHERE CURRENT OF) is not "
                            "implemented in this slice; owned by feature 004 "
                            "(static DML & cursors)");
                    continue;
                }
            }

            // Pair each reference with an associated indicator, if any. `:v :i`
            // and `:v INDICATOR :i` associate; `:a, :b` does not, because a
            // comma separates list items.
            std::vector<std::pair<std::size_t, int>> items;   // value, indicator
            {
                std::vector<bool> used(k.hostvars.size(), false);
                for (std::size_t i = 0; i < k.hostvars.size(); ++i) {
                    if (used[i]) continue;
                    int ind = -1;
                    if (i + 1 < k.hostvars.size() &&
                        associates(k.body, k.hostvars[i].end, k.hostvars[i + 1].begin)) {
                        ind = (int)(i + 1);
                        used[i + 1] = true;
                    }
                    items.push_back({i, ind});
                }
            }

            std::string sql = collapse_ws(placeholderise_inputs(k, items));
            std::string arr = "__esqlc_hv_" + std::to_string(++hv_seq);
            // SD-9: the landmark, or NULL. Never a guess.
            std::string tbl = k.table.empty() ? std::string("NULL")
                                              : c_string_literal(k.table);
            o << "do {\n";
            if (items.empty()) {
                o << "  esqlc_stmt_exec(" << c_string_literal(sql) << ", "
                  << sql.size() << ", 0, 0, " << tbl << ");\n";
            } else {
                o << "  esqlc_hostvar_t " << arr << "[" << items.size() << "] = {\n";
                bool bad = false;
                for (const auto &it : items) {
                    const auto &ref = k.hostvars[it.first];
                    const HostVar *hv = find(vars, ref.name);
                    if (!hv) {
                        d.error("ESQLC-1014", k.pos,
                                "host variable ':" + ref.name +
                                "' is not declared in a declare section");
                        bad = true;
                        continue;
                    }
                    std::string ind = "0";
                    if (it.second >= 0) {
                        const auto &iref = k.hostvars[(std::size_t)it.second];
                        const HostVar *ihv = find(vars, iref.name);
                        if (!ihv) {
                            d.error("ESQLC-1014", k.pos,
                                    "indicator ':" + iref.name +
                                    "' is not declared in a declare section");
                            bad = true;
                            continue;
                        }
                        ind = "&" + ihv->name;
                    }
                    o << "    { &" << hv->name << ", " << ind << ", "
                      << type_macro(hv->type)
                      << ", " << hv->width << "u, " << hv->capacity << "u, 0, "
                      << (hv->is_signed ? 1 : 0) << ", ESQLC_DIR_IN, "
                      << hv->charset << " },\n";           // Gate 8
                }
                o << "  };\n";
                if (bad) { o << "} while (0);\n"; continue; }
                o << "  esqlc_stmt_exec(" << c_string_literal(sql) << ", "
                  << sql.size() << ", " << arr << ", "
                  << items.size() << ", " << tbl << ");\n";
            }
            close_stmt(k.keyword);
            continue;
        }
    }

    // T073: width and signedness assertions, so a drifting type breaks the
    // build rather than silently changing what is bound (Principle VI spirit).
    if (!vars.empty()) {
        o << "\n/* host variable layout assertions (NFR-002.2) */\n";
        for (const auto &hv : vars) {
            if (hv.type == 3) {
                // T770 — the VARCHAR layout, proved without offsetof.
                //
                // The structure is ANONYMOUS: `struct { short len; char
                // val[n]; } v;` has no tag, so offsetof(struct tag, val) — what
                // the plan called for — cannot be written in portable C11.
                //
                // sizeof on a member expression works, and the three
                // assertions together are a stronger proof than offsetof
                // would have been: if sizeof(len) is 2, sizeof(val) is n, and
                // sizeof the whole structure is exactly 2 + n, then there is no
                // padding anywhere, so len is at 0 and val is at 2. The runtime
                // reads val at offset 2 and this is what licenses that.
                o << "_Static_assert(sizeof(" << hv.name << ".len) == 2, \""
                  << hv.name << ".len must be short (FR-002.21)\");\n";
                o << "_Static_assert(sizeof(" << hv.name << ".val) == " << hv.capacity
                  << ", \"" << hv.name << ".val capacity drifted\");\n";
                // The runtime reads `val` at offset 2, so that offset is the
                // assertion that matters. `sizeof(struct) == 2 + capacity` was
                // the first attempt and is wrong: the structure aligns to 2
                // because of `len`, so an odd capacity rounds up and a correct
                // layout fails the check. offsetof on the variable's own type
                // is the direct statement, and __typeof__ is needed because the
                // program's structure is anonymous — it has no tag to name.
                o << "_Static_assert(__builtin_offsetof(__typeof__(" << hv.name
                  << "), val) == 2, \"" << hv.name
                  << ".val must sit at offset 2 — the runtime reads it there\");\n";
            } else if (hv.type == 1) {
                o << "_Static_assert(sizeof(" << hv.name << ") == " << hv.capacity
                  << ", \"" << hv.name << " capacity drifted\");\n";
            } else {
                // T772 — the same width the descriptor claims. This is the
                // assertion that rejected a hand-declared `long` described as
                // 32 bits, and it must keep doing so.
                o << "_Static_assert(sizeof(" << hv.name << ") == " << hv.width
                  << ", \"" << hv.name << " width drifted\");\n";
            }
        }
    }
    return o.str();
}

}  // namespace pp
