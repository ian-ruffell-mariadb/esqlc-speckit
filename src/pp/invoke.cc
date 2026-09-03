// T966-T977 — the INVOKE generator.
//
// The output shape is not invented: §2 p.2-22 prints it verbatim. Three details
// come from there rather than from the requirement text, and each would
// otherwise have been guessed:
//
//   The indicator PRECEDES its field. FR-006.5 says so and p.2-22 shows it. A
//   structure with them after would compile and be wrong at every offset.
//
//   The suffix appears as `_i`, lowercase. FR-006.5b appends `_I` to the
//   catalogue name and FR-006.2 lowercases the identifier, so both are true —
//   but a fixture asserting `_I` would pass a wrong implementation.
//
//   The provenance comment is two lines, naming the object and the definition
//   timestamp. It is the only provenance a program carries until 001's listing
//   output exists (FR-006.7, out of this slice).
//
// The generated text is RE-PARSED by parse_declare_section rather than trusted,
// so there is one path to be right and Gates 7 and 8 are the test of what this
// emits: a VARCHAR group their shape check cannot read, or a CHARACTER SET
// clause their parser rejects, fails the build.
#include "pp.h"
#include <sstream>
#include <cctype>

namespace pp {
namespace {

std::string lower(const std::string &s) {
    std::string o;
    for (char c : s) o += (char)std::tolower((unsigned char)c);
    return o;
}

// T975 — a generated identifier that is a C keyword does not compile, and the
// error would point at generated text rather than at the customer's column
// name. Refusing here names the actual cause. ESQLC-6004.
bool is_c_keyword(const std::string &s) {
    static const char *kw[] = {
        "auto","break","case","char","const","continue","default","do","double",
        "else","enum","extern","float","for","goto","if","inline","int","long",
        "register","restrict","return","short","signed","sizeof","static",
        "struct","switch","typedef","union","unsigned","void","volatile","while",
        "_Bool","_Static_assert", nullptr };
    for (int i = 0; kw[i]; ++i) if (s == kw[i]) return true;
    return false;
}

bool valid_identifier(const std::string &s) {
    if (s.empty()) return false;
    if (!(std::isalpha((unsigned char)s[0]) || s[0] == '_')) return false;
    for (char c : s)
        if (!(std::isalnum((unsigned char)c) || c == '_')) return false;
    return true;
}

// The charset clause, or nothing for UNKNOWN. Emitted inline BEFORE the
// identifier, as p.2-22 writes it (FR-006.2b). Gate 8's parser reads it back.
std::string cs_clause(const std::string &charset) {
    if (charset.empty() || charset == "UNKNOWN") return "";
    return "CHARACTER SET " + charset + " ";
}

}  // namespace

std::string invoke_generate(const std::string &object, const std::string &tag,
                            const std::vector<SchemaColumn> &cols,
                            const std::string &captured, Pos at, Diag &d) {
    std::ostringstream o;

    // FR-006.5d, in p.2-22's shape.
    o << "/* Record Definition for table " << object << " */\n";
    o << "/* Definition current at " << (captured.empty() ? "(unknown)" : captured)
      << "  */\n";
    // FR-006.2a: the tag is the OBJECT's name with _type appended, not the AS
    // name. §2 p.2-22 invokes \NEWYORK.$DISK1.SQL.TYPESC2 and generates
    // `struct typesc2_type`, so it is the object's last component.
    std::string base = object;
    std::size_t dot = base.find_last_of(".$\\");
    if (dot != std::string::npos) base = base.substr(dot + 1);
    o << "struct " << lower(base) << "_type {\n";

    for (const auto &c : cols) {
        const std::string field = lower(c.name);
        if (!valid_identifier(field) || is_c_keyword(field)) {
            d.error("ESQLC-6004", at,
                    "column '" + c.name + "' becomes '" + field +
                    "', which is not a usable C identifier" +
                    (is_c_keyword(field) ? " (a C keyword)" : "") +
                    "; the generated structure would not compile");
            return {};
        }

        // T971, T972 — an indicator for each nullable column and no other,
        // PRECEDING its field.
        if (c.nullable) {
            const std::string ind = field + "_i";      // T973: lowercase
            // T974 / DIV-056. SQL/MP truncates the suffix at 30 and 31
            // characters, making the indicator's name equal its field's. That
            // is two members with one identifier, which no C compiler accepts —
            // on NonStop either — so it is refused rather than reproduced.
            if (field.size() >= 30) {
                d.error("ESQLC-6007", at,
                        "column '" + c.name + "' is " +
                        std::to_string(field.size()) +
                        " characters; SQL/MP truncates the default '_I' suffix at "
                        "30 and 31, making the indicator's name identical to the "
                        "host variable's. That cannot be generated as valid C, so "
                        "it is refused: shorten the column name (DIV-056)");
                return {};
            }
            o << "  short " << ind << ";\n";
        }

        if (c.sqltype == "SMALLINT") {
            o << "  short " << field << ";\n";
        } else if (c.sqltype == "INTEGER") {
            o << "  int " << field << ";\n";
        } else if (c.sqltype == "BIGINT") {
            o << "  long long " << field << ";\n";
        } else if (c.sqltype == "CHAR") {
            // FR-006.3 / SD-10: the extra byte under CHAR_AS_STRING.
            o << "  char " << cs_clause(c.charset) << field
              << "[" << (c.length + 1) << "];\n";
        } else if (c.sqltype == "VARCHAR") {
            // FR-006.4 — the nested group Gate 7 taught the preprocessor to
            // read, now written by it.
            o << "  struct { short len; char " << cs_clause(c.charset)
              << "val[" << (c.length + 1) << "]; } " << field << ";\n";
        } else {
            // T976 — refused by name, never bound as something near.
            d.error("ESQLC-6003", at,
                    "column '" + c.name + "' has type " + c.sqltype +
                    ", which has no mapping in feature 002's table");
            return {};
        }
    }

    o << "} " << tag << ";\n";
    return o.str();
}

}  // namespace pp
