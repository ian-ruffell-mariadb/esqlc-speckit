// T069 — handler table and position enforcement.
//
// Six keywords are implemented in this slice. Everything else in the §3
// inventory is listed here with its owning feature so ESQLC-1012 can name it,
// per Constitution III: a recognised statement with no handler must refuse,
// never silently no-op.
#include "pp.h"
#include <cstring>

namespace pp {

static const Handler kHandlers[] = {
    // --- implemented in this slice (owning_feature == nullptr) ----------
    {"BEGIN DECLARE SECTION", PosClass::Decl, nullptr},
    {"END DECLARE SECTION",   PosClass::Decl, nullptr},
    {"BEGIN WORK",            PosClass::Exec, nullptr},
    {"COMMIT WORK",           PosClass::Exec, nullptr},
    {"ROLLBACK WORK",         PosClass::Exec, nullptr},
    {"INSERT",                PosClass::Exec, nullptr},
    {"WHENEVER",              PosClass::Any,  nullptr},   // Gate 4, FR-001.13
    {"INCLUDE SQLCA",         PosClass::Decl, nullptr},   // Gate 4
    {"INCLUDE STRUCTURES",    PosClass::Decl, nullptr},   // Gate 4: ordering only
    {"SELECT",                PosClass::Exec, nullptr},   // Gate 2: single-row only
    {"DECLARE CURSOR",        PosClass::Decl, nullptr},   // Gate 3: read-only
    {"OPEN",                  PosClass::Exec, nullptr},
    {"FETCH",                 PosClass::Exec, nullptr},
    {"CLOSE",                 PosClass::Exec, nullptr},

    // --- recognised, deliberately unimplemented -------------------------
    // FOR UPDATE is refused inside the DECLARE CURSOR handler rather than
    // here: it is a clause of the cursor's statement, not a keyword of its
    // own, so the dispatch table cannot see it.
    {"UPDATE",             PosClass::Exec, "004 (static DML & cursors)"},
    {"DELETE",             PosClass::Exec, "004 (static DML & cursors)"},
    {"INCLUDE SQLSA",      PosClass::Decl, "005 (diagnostics)"},
    {"INVOKE",             PosClass::Decl, "006 (INVOKE schema generation)"},
    {"INCLUDE SQLDA",      PosClass::Decl, "007 (dynamic SQL)"},
    {"PREPARE",            PosClass::Exec, "007 (dynamic SQL)"},
    {"EXECUTE",            PosClass::Exec, "007 (dynamic SQL)"},
    {"EXECUTE IMMEDIATE",  PosClass::Exec, "007 (dynamic SQL)"},
    {"DESCRIBE",           PosClass::Exec, "007 (dynamic SQL)"},
    {"DESCRIBE INPUT",     PosClass::Exec, "007 (dynamic SQL)"},
    {"RELEASE",            PosClass::Exec, "007 (dynamic SQL)"},
    {"CONTROL",            PosClass::Decl, "008 (NonStop compatibility surface)"},
    {"LOCK",               PosClass::Exec, "008 (NonStop compatibility surface)"},
    {"UNLOCK",             PosClass::Exec, "008 (NonStop compatibility surface)"},
    {"FREE",               PosClass::Exec, "008 (NonStop compatibility surface)"},
    {"GET",                PosClass::Exec, "008 (NonStop compatibility surface)"},
    {"CREATE",             PosClass::Exec, "008 (NonStop compatibility surface)"},
    {"DROP",               PosClass::Exec, "008 (NonStop compatibility surface)"},
    {"ALTER",              PosClass::Exec, "008 (NonStop compatibility surface)"},
    {"COMMENT",            PosClass::Exec, "008 (NonStop compatibility surface)"},
    {"HELP",               PosClass::Exec, "008 (NonStop compatibility surface)"},
    {"SQL SOURCE",         PosClass::Decl, "001 (deferred: not in Gate 1 scope)"},
    {nullptr, PosClass::Decl, nullptr},
};

const Handler *lookup(const std::string &keyword) {
    for (int i = 0; kHandlers[i].keyword; ++i)
        if (keyword == kHandlers[i].keyword) return &kHandlers[i];
    return nullptr;
}

}  // namespace pp
