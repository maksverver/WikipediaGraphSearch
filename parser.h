#ifndef PARSER_H_INCLUDED
#define PARSER_H_INCLUDED

#include <string>

class ParserCallback {
public:
    virtual void HandlePage(const std::string &title, const std::string &text, const std::string &redirect) = 0;
};

int ParseFile(const char *filename, ParserCallback &callback);

#endif  // ndef PARSER_H_INCLUDED
