// T961-T964 — the schema cache reader.
//
// Tab-separated, one column per line, because the preprocessor has no
// third-party dependencies and a JSON parser would be either a new dependency
// or ~150 lines of new attack surface for five fields per column (SD-15,
// amended at plan stage). This is about thirty lines.
//
// The preprocessor never opens a socket. That is the whole point: FR-006.2e
// wants read access to the invoked object at preprocess time and NFR-001.2
// forbids depending on MariaDB, and NFR-006.2's optional-by-cache is what
// reconciles them.
#include "pp.h"
#include <fstream>
#include <sstream>

namespace pp {

const std::vector<SchemaColumn> *Schema::find(const std::string &table) const {
    for (const auto &t : tables)
        if (t.first == table) return &t.second;
    return nullptr;
}

SchemaErr schema_read(const std::string &path, Schema &out) {
    // Absent and Unreadable are deliberately different. One is a missing
    // compiler option (ESQLC-6002); the other a named file that cannot be used
    // (ESQLC-6008). Collapsing them would send a reader to the wrong place.
    if (path.empty()) return SchemaErr::Absent;
    std::ifstream f(path);
    if (!f) return SchemaErr::Unreadable;

    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        if (line.rfind("!captured", 0) == 0) {
            std::size_t tab = line.find('\t');
            if (tab != std::string::npos) out.captured = line.substr(tab + 1);
            continue;
        }
        std::vector<std::string> fld;
        std::istringstream ls(line);
        std::string cell;
        while (std::getline(ls, cell, '\t')) fld.push_back(cell);
        if (fld.size() < 6) continue;      // a short line is not a column

        SchemaColumn c;
        c.name     = fld[1];
        c.sqltype  = fld[2];
        c.length   = (unsigned)std::strtoul(fld[3].c_str(), nullptr, 10);
        c.nullable = (fld[4] == "Y" || fld[4] == "y");
        c.charset  = fld[5];

        bool found = false;
        for (auto &t : out.tables)
            if (t.first == fld[0]) { t.second.push_back(c); found = true; break; }
        if (!found) out.tables.push_back({fld[0], {c}});
    }
    return SchemaErr::None;
}

}  // namespace pp
