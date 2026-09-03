// T860 — the character-set keyword table.
//
// §2 p.2-24 fixes the keywords: ISO8859n (n = 1..9), KANJI, KSC5601, UNKNOWN,
// in uppercase. What the manual does NOT fix is the numeric id: §10 p.10-6 puts
// the character-set ID in the SQLDA's `precision` field, and p.10-11 says those
// declarations come from the `sqlh` header, which this project does not have.
// So these ids are project-internal, private to the runtime, and 002 Q7 records
// that feature 007 needs the published values for the SQLDA rather than these.
//
// Three classes, and the distinction between the last two is load-bearing:
//
//   Mapped       MariaDB has this character set.
//   Unmapped     the keyword is real and MariaDB has no counterpart. The gap is
//                MariaDB's, so ESQLC-2013 sends the reader to its charset list.
//   Unspecified  the keyword is real and the MANUAL names no encoding. KANJI is
//                a script; sjis, cp932, ujis and eucjpms all render it and
//                differ in byte length and repertoire. The gap is the manual's,
//                so ESQLC-2014 sends the reader to SQLRM. Collapsing the two
//                codes would send either reader to the wrong place.
//
// Kept in step with src/rt/charset.c by tests/harness/charset_sync.sh.
#include "pp.h"

namespace pp {

static const CharsetKeyword kTable[] = {
    // keyword       id   class
    {"UNKNOWN",       0,  CsClass::Mapped},       // p.2-24: unknown single-byte,
                                                  // equivalent to no clause. This
                                                  // is what resolves SD-1.
    {"ISO88591",      1,  CsClass::Mapped},       // latin1 — cp1252, approximate
    {"ISO88592",      2,  CsClass::Mapped},       // latin2
    {"ISO88593",      3,  CsClass::Unmapped},
    {"ISO88594",      4,  CsClass::Unmapped},
    {"ISO88595",      5,  CsClass::Unmapped},
    {"ISO88596",      6,  CsClass::Unmapped},
    {"ISO88597",      7,  CsClass::Mapped},       // greek
    {"ISO88598",      8,  CsClass::Mapped},       // hebrew
    {"ISO88599",      9,  CsClass::Mapped},       // latin5
    {"KANJI",        50,  CsClass::Unspecified},  // SD-14
    {"KSC5601",      51,  CsClass::Mapped},       // euckr
    {nullptr,         0,  CsClass::Mapped},
};

const CharsetKeyword *charset_lookup(const std::string &kw) {
    for (int i = 0; kTable[i].keyword; ++i)
        if (kw == kTable[i].keyword) return &kTable[i];
    return nullptr;
}

const CharsetKeyword *charset_table(int *n) {
    int c = 0;
    while (kTable[c].keyword) ++c;
    *n = c;
    return kTable;
}

}  // namespace pp
