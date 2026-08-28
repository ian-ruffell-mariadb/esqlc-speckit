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
    {"SELECT",                PosClass::Exec, nullptr},   // Gate 2: single-row only

    // --- recognised, deliberately unimplemented -------------------------
    {"UPDATE",             PosClass::Exec, "004 (static DML & cursors)"},
    {"DELETE",             PosClass::Exec, "004 (static DML & cursors)"},
    {"DECLARE CURSOR",     PosClass::Decl, "004 (static DML & cursors)"},
    {"OPEN",               PosClass::Exec, "004 (static DML & cursors)"},
    {"FETCH",              PosClass::Exec, "004 (static DML & cursors)"},
    {"CLOSE",              PosClass::Exec, "004 (static DML & cursors)"},
    {"WHENEVER",           PosClass::Decl, "005 (diagnostics)"},
    {"INCLUDE STRUCTURES", PosClass::Decl, "005 (diagnostics)"},
    {"INCLUDE SQLCA",      PosClass::Decl, "005 (diagnostics)"},
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
