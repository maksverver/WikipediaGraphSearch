#ifndef WIKIPATH_PARSER_H_INCLUDED
#define WIKIPATH_PARSER_H_INCLUDED

#include <string>

namespace wikipath {

class ParserCallback {
public:
    virtual void HandlePage(const std::string &title, const std::string &text, const std::string &redirect) = 0;
};

int ParseFile(const char *filename, ParserCallback &callback);

}  // namespace wikipath

#endif  // ndef WIKIPATH_PARSER_H_INCLUDED
