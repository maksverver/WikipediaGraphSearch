#include "wikipath/parser.h"

#include <libxml/parser.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <iostream>
#include <map>
#include <set>
#include <optional>
#include <string>
#include <sstream>
#include <vector>

namespace wikipath {
namespace {

const char *GetAttr(const char* const* attrs, const char *key) {
    while (*attrs != nullptr) {
        if (strcmp(attrs[0], key) == 0) {
            return attrs[1];
        }
        attrs += 2;
    }
    return nullptr;
}

class Parser {
public:
    Parser(ParserCallback &callback) : callback(callback) {};

    void StartElement(const char* name, const char* const* attrs) {
        path.append("/");
        path.append(name);

        if (path == "/mediawiki/page") {
            BeginPage();
        } else if (path == "/mediawiki/page/redirect") {
            if (const char *title = GetAttr(attrs, "title"); title != nullptr) {
                page_redirect = title;
            }
        }
    }

    void EndElement(const char* name) {
        size_t n = strlen(name);
        assert(
            path.size() > n &&
            strcmp(path.c_str() + path.size() - n, name) == 0 &&
            path[path.size() - n - 1] == '/');

        if (path == "/mediawiki/page") {
            EndPage();
        }

        path.erase(path.size() - n - 1);
    }

    void Characters(const char *ch, int len) {
        if (path == "/mediawiki/page/title") {
            page_title.append(ch, len);
        } else if (path == "/mediawiki/page/revision/text") {
            page_text.append(ch, len);
        }
    }

    void BeginPage() {
        page_title.clear();
        page_text.clear();
        page_redirect.clear();
    }

    void EndPage() {
        callback.HandlePage(page_title, page_text, page_redirect);
    }

private:
    ParserCallback &callback;
    std::string path;
    std::string page_title;
    std::string page_text;
    std::string page_redirect;
};


void startElement(void *user_data, const xmlChar *name, const xmlChar **attrs) {
    ((Parser*) user_data)->StartElement((const char*) name, (const char* const*) attrs);
}

void endElement(void *user_data, const xmlChar *name) {
    ((Parser*) user_data)->EndElement((const char*) name);
}

void characters(void *user_data, const xmlChar *ch, int	len) {
    ((Parser*) user_data)->Characters((const char*) ch, len);
}

xmlEntityPtr getEntity(void *, const xmlChar *name) {
    return xmlGetPredefinedEntity(name);
}

void error(void *, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "Error occurred while parsing XML!\n");
    vfprintf(stderr, fmt, args);
    va_end(args);
}

}  // namespace

int ParseFile(const char *filename, ParserCallback &callback) {
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    xmlSAXHandler sax_handler = {
        .getEntity = getEntity,
        .startElement = startElement,
        .endElement = endElement,
        .characters = characters,
        .warning = error,
        .error = error,
        .fatalError = error,
    };
    #pragma GCC diagnostic pop
    Parser parser(callback);
    return xmlSAXUserParseFile(&sax_handler, &parser, filename);
}

}  // namespace wikipath
