#ifndef READER_H_INCLUDED
#define READER_H_INCLUDED

#include "common.h"
#include "graph-reader.h"
#include "metadata-reader.h"

#include <string>

class Reader {
public:
    // Opens the graph and metadata files and returns a reader, or prints an error
    static std::unique_ptr<Reader> Open(const char *graph_filename);

    // Parses a page CLI argument and converts it to a valid page id,
    // or prints an error and returns 0.
    //
    //   Title -> resolves the page by title
    //   "#123" -> parges page index as a number
    //   "?" -> selects a random page
    index_t ParsePageArgument(const char *arg);

    GraphReader &Graph() { return *graph; }
    MetadataReader &Metadata() { return *metadata; }

    // Returns the title of the page, or the string "untitled" if the page is not found.
    std::string PageTitle(index_t id);

    // Returns a page reference of the form "#123 (Title)".
    std::string PageRef(index_t id);

private:
    Reader(std::unique_ptr<GraphReader> graph, std::unique_ptr<MetadataReader> metadata);

    std::unique_ptr<GraphReader> graph;
    std::unique_ptr<MetadataReader> metadata;
};

#endif  // ndef READER_H_INCLUDED
