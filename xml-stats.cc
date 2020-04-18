#include <libxml/parser.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <iostream>
#include <map>
#include <string>
#include <sstream>

namespace {

std::map<std::string, int64_t> path_count;
std::string path;

void startElement(void *user_data, const xmlChar *name, const xmlChar **attrs) {
    path.append("/");
    path.append((char*) name);
    if (!path_count[path]++) {
        std::cout << path << '\n';
    }
}

void endElement(void *user_data, const xmlChar *name) {
    size_t n = strlen((char*) name);
    assert(
        path.size() > n &&
        strcmp(path.c_str() + path.size() - n, (char*) name) == 0 &&
        path[path.size() - n - 1] == '/');
    path.erase(path.size() - n - 1);
}

void characters(void *user_data, const xmlChar *ch, int	len) {
//    std::cout << '"' << std::string((char*) ch, len) << '"' << '\n';
}

xmlEntityPtr getEntity(void *user_data, const xmlChar *name) {
    return xmlGetPredefinedEntity(name);
}

void error(void *user_data, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "Error occurred while parsing XML!\n");
    vfprintf(stderr, fmt, args);
    va_end(args);
}

xmlSAXHandler sax_handler {
    .getEntity = getEntity,
    .startElement = startElement,
    .endElement = endElement,
    .characters = characters,
    .warning = error,
    .error = error,
    .fatalError = error,
};

}  // namespace

// Quick-and-dirty tool to analyze an XML file and count how often each tag occurs.
int main(int argc, char** argv) {
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <pages-articles.xml>\n";
        return EXIT_SUCCESS;
    }

    xmlSAXUserParseFile(&sax_handler, nullptr, argv[1]);

    std::cout << '\n';
    for (const auto &elem : path_count) {
      std::cout << elem.first << ": " << elem.second << '\n';
    }
    return EXIT_SUCCESS;
}
