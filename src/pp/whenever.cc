// T470 — per-condition action table, set and superseded in source order.
// T471 — checks emitted in the published precedence order.
// T472 — CONTINUE emits nothing; CALL and GOTO/GO TO emit their forms.
//
// The precedence order is not incidental: §9 p.9-6's table states that when
// several conditions apply to one statement, they are processed in the order
// NOT FOUND, SQLERROR, SQLWARNING. Reordering them changes which handler runs.
#include "pp.h"
#include <cctype>
#include <cstring>

namespace pp {
namespace {

bool is_c_identifier(const std::string &s) {
    if (s.empty()) return false;
    if (!(std::isalpha((unsigned char)s[0]) || s[0] == '_')) return false;
    for (char c : s)
        if (!(std::isalnum((unsigned char)c) || c == '_')) return false;
    return true;
}

std::string upper_squashed(const std::string &s) {
    std::string o;
    bool sp = false;
    for (char c : s) {
        if (std::isspace((unsigned char)c)) { sp = !o.empty(); continue; }
        if (sp) { o += ' '; sp = false; }
        o += (char)std::toupper((unsigned char)c);
    }
    return o;
}

}  // namespace

bool whenever_applies(const std::string &keyword) {
    // SD-5 (provisional, narrows 005 Q9): §9 p.9-6 names DML, DCL and DDL, and
    // §3 lists transaction control as a separate fourth class — so BEGIN,
    // COMMIT and ROLLBACK WORK are excluded. If the manual's list turns out to
    // be an omission rather than an exclusion, this is the one line to change.
    if (keyword == "BEGIN WORK" || keyword == "COMMIT WORK" ||
        keyword == "ROLLBACK WORK")
        return false;
    // Directives are not statements and never carry checks.
    if (keyword == "BEGIN DECLARE SECTION" || keyword == "END DECLARE SECTION" ||
        keyword == "DECLARE CURSOR" || keyword == "WHENEVER" ||
        keyword.rfind("INCLUDE", 0) == 0)
        return false;
    return true;
}

void whenever_set(WheneverState &st, const Construct &k, Diag &d) {
    const std::string body = upper_squashed(k.body);   // "WHENEVER NOT FOUND GOTO :X"

    int idx;
    std::size_t after;
    if (body.compare(9, 9, "NOT FOUND") == 0) { idx = (int)WhenCond::NotFound;  after = 18; }
    else if (body.compare(9, 8, "SQLERROR") == 0) { idx = (int)WhenCond::SqlError; after = 17; }
    else if (body.compare(9, 10, "SQLWARNING") == 0) { idx = (int)WhenCond::SqlWarning; after = 19; }
    else {
        d.error("ESQLC-1009", k.pos,
                "WHENEVER condition must be NOT FOUND, SQLERROR, or SQLWARNING");
        return;
    }

    std::string rest = body.substr(after < body.size() ? after : body.size());
    while (!rest.empty() && rest.front() == ' ') rest.erase(rest.begin());

    WheneverEntry ent;
    if (rest.compare(0, 8, "CONTINUE") == 0) {
        ent.act = WhenAct::Continue;
    } else if (rest.compare(0, 4, "CALL") == 0 || rest.compare(0, 4, "GOTO") == 0 ||
               rest.compare(0, 5, "GO TO") == 0) {
        ent.act = (rest[0] == 'C') ? WhenAct::Call : WhenAct::Goto;
        // The scanner already captured `:target` as a host-variable reference,
        // so the identifier comes from there rather than from re-lexing.
        if (k.hostvars.empty()) {
            d.error("ESQLC-5008", k.pos,
                    "WHENEVER action requires a ':' prefixed target");
            return;
        }
        ent.target = k.hostvars.front().name;
        if (!is_c_identifier(ent.target)) {
            // The preprocessor cannot verify a C label or function exists — see
            // the plan's accepted risk. What it can check is that the name could
            // be one at all.
            d.error("ESQLC-5008", k.pos,
                    "WHENEVER action target '" + ent.target +
                    "' is not a valid C identifier");
            return;
        }
    } else {
        d.error("ESQLC-1009", k.pos,
                "WHENEVER action must be CONTINUE, CALL, GOTO, or GO TO");
        return;
    }
    st.e[idx] = ent;                     // supersedes only this condition
}

std::string whenever_checks(const WheneverState &st) {
    static const char *cond_test[] = {
        "sqlcode == 100",                       // NOT FOUND
        "sqlcode < 0",                          // SQLERROR
        "sqlcode > 0 && sqlcode != 100",        // SQLWARNING
    };
    std::string o;
    for (int i = 0; i < (int)WhenCond::Count; ++i) {
        const WheneverEntry &e = st.e[i];
        if (e.act == WhenAct::Continue) continue;   // CONTINUE emits nothing
        o += "  if (";
        o += cond_test[i];
        o += ") ";
        o += (e.act == WhenAct::Call) ? (e.target + "();") : ("goto " + e.target + ";");
        o += "\n";
    }
    return o;
}

}  // namespace pp
