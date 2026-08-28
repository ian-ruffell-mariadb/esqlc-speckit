// T060 — diagnostic registry and formatting. NFR-001.3.
#include "pp.h"
#include <cstdio>

namespace pp {

static void emit_line(const char *sev, const std::string &file,
                      const char *code, Pos p, const std::string &msg) {
    // file:line:col: severity: CODE: message
    std::fprintf(stderr, "%s:%d:%d: %s: %s: %s\n",
                 file.c_str(), p.line, p.col, sev, code, msg.c_str());
}

void Diag::error(const char *code, Pos p, const std::string &msg) {
    emit_line("error", file_, code, p, msg);
    ++errors_;
}

void Diag::info(const char *code, Pos p, const std::string &msg) {
    emit_line("info", file_, code, p, msg);
}

}  // namespace pp
