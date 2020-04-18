#ifndef READER_H_INCLUDED
#define READER_H_INCLUDED

#include "common.h"
#include "graph-reader.h"
#include "metadata-reader.h"

#include <string>

// Wrapper around GraphReader and MetadataReader, for tools that need both.
//
// Additionally, this class contains a few common utility functions related to
// parsing input and formatting output.
class Reader {
public:
    // Opens the graph and metadata files and returns a reader, or prints an error
    static std::unique_ptr<Reader> Open(const char *graph_filename);

    // Returns a reference to the open GraphReader.
    GraphReader &Graph() { return *graph; }

    // Returns a reference to the open MetadataReader.
    MetadataReader &Metadata() { return *metadata; }

    // Parses a page CLI argument and converts it to a valid page id,
    // or prints an error and returns 0.
    //
    //   Title -> resolves the page by title
    //   "#123" -> parges page index as a number
    //   "?" -> selects a random page
    index_t ParsePageArgument(const char *arg);

    // Returns the title of the page, or the string "untitled" if the page is not found.
    std::string PageTitle(index_t id);

    // Returns a page reference of the form "#123 (Title)".
    std::string PageRef(index_t id);

    // Returns the text how the link to `to_page_id` is displayed on `from_page_id`.
    std::string LinkText(index_t from_page_id, index_t to_page_id);

    // Returns a reference to a target page of the form "#123 (ToTitle)", or
    // "#123 (ToTitle; displayed as Text)" if the target page is linked from the
    // source page with a link text different from the title of the target page.
    std::string ForwardLinkRef(index_t from_page_id, index_t to_page_id);

    // Similar to ForwardLinkRef, but for backward links. Note that the form
    // "123 (FromTitle; displayed as Text)" means the origin page with title
    // "FromTitle", has a link with text "Text" to the target page.
    std::string BackwardLinkRef(index_t from_page_id, index_t to_page_id);

private:
    Reader(std::unique_ptr<GraphReader> graph, std::unique_ptr<MetadataReader> metadata);

    std::unique_ptr<GraphReader> graph;
    std::unique_ptr<MetadataReader> metadata;
};

#endif  // ndef READER_H_INCLUDED
