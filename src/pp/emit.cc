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

const char *type_macro(unsigned t) {
    switch (t) {
        case 1: return "ESQLC_T_CHAR_FIXED";
        case 2: return "ESQLC_T_INT";
        default: return "0";
    }
}

}  // namespace

std::string emit(const std::string &file, const ScanResult &sr,
                 std::vector<HostVar> &vars, Diag &d) {
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
    int  hv_seq = 0;
    std::map<std::string, CursorInfo> cursors;   // name -> declaration (T362)
    bool at_line_start = true;

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

    for (const auto &ch : sr.chunks) {
        if (!ch.is_sql) {
            line_directive(ch.pos.line);
            o << ch.c_text;
            track(ch.c_text);
            if (in_declare) {
                // The declarations themselves are valid C: emit verbatim above,
                // and additionally harvest descriptors from them.
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
        if (h->where != k.where) {
            d.error("ESQLC-1008", k.pos,
                    "'" + k.keyword + "' must appear in " +
                    (h->where == PosClass::Decl ? "declaration" : "executable") +
                    " position");
            continue;
        }

        if (k.keyword == "BEGIN DECLARE SECTION") { in_declare = true;  continue; }
        if (k.keyword == "END DECLARE SECTION")   { in_declare = false; continue; }

        line_directive(k.pos.line);

        if (k.keyword == "BEGIN WORK" || k.keyword == "COMMIT WORK" ||
            k.keyword == "ROLLBACK WORK") {
            const char *fn = k.keyword == "BEGIN WORK"  ? "esqlc_txn_begin"
                           : k.keyword == "COMMIT WORK" ? "esqlc_txn_commit"
                                                        : "esqlc_txn_rollback";
            o << "do { " << fn << "(); sqlcode = esqlc_sqlcode(); } while (0);\n";
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
                o << "do { esqlc_cursor_close(\"" << cname << "\");"
                  << " sqlcode = esqlc_sqlcode(); } while (0);\n";
                at_line_start = true;
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
                      << (b.second ? "ESQLC_DIR_OUT" : "ESQLC_DIR_IN") << ", 0 },\n";
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
            o << "  sqlcode = esqlc_sqlcode();\n";
            o << "} while (0);\n";
            at_line_start = true;
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
                  << (c.is_out ? "ESQLC_DIR_OUT" : "ESQLC_DIR_IN") << ", 0 },\n";
            }
            o << "  };\n";
            if (bad) { o << "} while (0);\n"; continue; }
            o << "  esqlc_stmt_exec(" << c_string_literal(sql) << ", "
              << sql.size() << ", " << arr << ", " << cls.size() << ");\n";
            o << "  sqlcode = esqlc_sqlcode();\n";
            o << "} while (0);\n";
            continue;
        }

        if (k.keyword == "INSERT") {
            std::string sql = collapse_ws(placeholderise(k));
            std::string arr = "__esqlc_hv_" + std::to_string(++hv_seq);
            o << "do {\n";
            if (k.hostvars.empty()) {
                o << "  esqlc_stmt_exec(" << c_string_literal(sql) << ", "
                  << sql.size() << ", 0, 0);\n";
            } else {
                o << "  esqlc_hostvar_t " << arr << "[" << k.hostvars.size() << "] = {\n";
                bool bad = false;
                for (const auto &ref : k.hostvars) {
                    const HostVar *hv = find(vars, ref.name);
                    if (!hv) {
                        d.error("ESQLC-1014", k.pos,
                                "host variable ':" + ref.name +
                                "' is not declared in a declare section");
                        bad = true;
                        continue;
                    }
                    o << "    { &" << hv->name << ", 0, " << type_macro(hv->type)
                      << ", " << hv->width << "u, " << hv->capacity << "u, 0, "
                      << (hv->is_signed ? 1 : 0) << ", ESQLC_DIR_IN, 0 },\n";
                }
                o << "  };\n";
                if (bad) { o << "} while (0);\n"; continue; }
                o << "  esqlc_stmt_exec(" << c_string_literal(sql) << ", "
                  << sql.size() << ", " << arr << ", "
                  << k.hostvars.size() << ");\n";
            }
            o << "  sqlcode = esqlc_sqlcode();\n";
            o << "} while (0);\n";
            continue;
        }
    }

    // T073: width and signedness assertions, so a drifting type breaks the
    // build rather than silently changing what is bound (Principle VI spirit).
    if (!vars.empty()) {
        o << "\n/* host variable layout assertions (NFR-002.2) */\n";
        for (const auto &hv : vars) {
            if (hv.type == 1)
                o << "_Static_assert(sizeof(" << hv.name << ") == " << hv.capacity
                  << ", \"" << hv.name << " capacity drifted\");\n";
            else
                o << "_Static_assert(sizeof(" << hv.name << ") == " << hv.width
                  << ", \"" << hv.name << " width drifted\");\n";
        }
    }
    return o.str();
}

}  // namespace pp
