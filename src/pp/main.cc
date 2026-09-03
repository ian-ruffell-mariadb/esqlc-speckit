// esqlcpp — the esqlc preprocessor driver.
//
//   esqlcpp <input.sqlc> [-o output.c] [--schema cache]
//
// Exit status: 0 clean, 1 diagnostics emitted (no output written), 2 usage.
// NFR-001.2: links no MariaDB library and needs no database.
#include "pp.h"
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>

int main(int argc, char **argv) {
    std::string in, out, schema;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-o" && i + 1 < argc) { out = argv[++i]; }
        // T911 — accepted here so the Tier 1 harnesses can pass it; T965 gives
        // it meaning. INVOKE reads a committed cache and the preprocessor never
        // opens a socket, which is how FR-006.2e's "read access at preprocess
        // time" and NFR-001.2's "no MariaDB at Tier 1" coexist (NFR-006.2).
        else if (a == "--schema" && i + 1 < argc) { schema = argv[++i]; }
        else if (a.rfind("-", 0) == 0)  { std::fprintf(stderr, "esqlcpp: unknown option %s\n", a.c_str()); return 2; }
        else if (in.empty())            { in = a; }
        else                            { std::fprintf(stderr, "esqlcpp: unexpected argument %s\n", a.c_str()); return 2; }
    }
    if (in.empty()) {
        std::fprintf(stderr,
                     "usage: esqlcpp <input.sqlc> [-o output.c] [--schema cache]\n");
        return 2;
    }

    std::ifstream f(in, std::ios::binary);
    if (!f) { std::fprintf(stderr, "esqlcpp: cannot open %s\n", in.c_str()); return 2; }
    std::ostringstream ss; ss << f.rdbuf();
    const std::string src = ss.str();

    pp::Diag d(in);
    pp::ScanResult sr = pp::scan(src, d);
    std::vector<pp::HostVar> vars;
    std::string emitted = pp::emit(in, sr, vars, d, schema);

    if (d.errors() > 0) return 1;      // no output on error

    if (out.empty()) { std::cout << emitted; return 0; }
    std::ofstream g(out, std::ios::binary);
    if (!g) { std::fprintf(stderr, "esqlcpp: cannot write %s\n", out.c_str()); return 2; }
    g << emitted;
    return 0;
}
