#ifndef WIKIPATH_PARSER_H_INCLUDED
#define WIKIPATH_PARSER_H_INCLUDED

#include <climits>
#include <cstdlib>
#include <optional>
#include <string>

namespace wikipath {

class ParserCallback {
public:
    struct Page {
        const std::string &title;
        const std::string &ns;
        const std::string &text;
        const std::string &redirect;

        std::optional<long> ParseNs() const {
            char *end = nullptr;
            long i = strtol(ns.c_str(), &end, 10);
            if (*end == '\0' && i > LONG_MIN && i < LONG_MAX) return i;
            return {};  // parse error
        }
    };

    virtual void HandlePage(const Page &page) = 0;
};

int ParseFile(const char *filename, ParserCallback &callback);

}  // namespace wikipath

#endif  // ndef WIKIPATH_PARSER_H_INCLUDED
