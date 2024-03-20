#ifndef WIKIPATH_READER_H_INCLUDED
#define WIKIPATH_READER_H_INCLUDED

#include "common.h"
#include "graph-reader.h"
#include "metadata-reader.h"

#include <string>
#include <string_view>

namespace wikipath {

// Returns a page reference of the form "#123 (Title)".
std::string PageRef(index_t id, std::string_view title);

// Returns a forward link reference of the form "#123 (Title)" or
// "#123 (Title; displayed as: text)" if to_title != link_text.
std::string ForwardLinkRef(
        index_t to_page_id, std::string_view to_title,
        std::string_view link_text);

// Returns a backward link reference of the form "#123 (Title)" or
// "#123 (Title; displayed as: text)" if to_title != link_text.
std::string BackwardLinkRef(
        index_t from_page_id, std::string_view from_title,
        std::string_view to_title, std::string_view link_text);


// Wrapper around GraphReader and MetadataReader, for tools that need both.
//
// Additionally, this class contains a few common utility functions related to
// parsing input and formatting output.
class Reader {
public:
    // Opens the graph and metadata files and returns a reader, or prints an error
    static std::unique_ptr<Reader> Open(const char *graph_filename, GraphReader::OpenOptions graph_options);

    // Returns a reference to the open GraphReader.
    const GraphReader &Graph() const { return *graph; }

    // Returns a reference to the open MetadataReader.
    const MetadataReader &Metadata() const { return *metadata; }

    // Returns whether `id` is a valid page id.
    bool IsValidPageId(index_t id) const {
        return 0 < id && id < graph->VertexCount();
    }

    // Returns a random page id.
    //
    // To keep things interesting, this tries to find a page with at least 1
    // incoming link and 1 outgoing link.
    index_t RandomPageId() const;

    // Parses a page CLI argument and converts it to a valid page id,
    // or prints an error and returns 0.
    //
    //   Title -> resolves the page by title
    //   "#123" -> parges page index as a number
    //   "?" -> selects a random page
    index_t ParsePageArgument(const char *arg) const;

    // Returns the title of the page, or "untitled" if the page is not found.
    std::string PageTitle(index_t id) const;

    // Returns a page reference of the form "#123 (Title)".
    std::string PageRef(index_t id) const;

    // Returns the text how the link to `to_page_id` is displayed on
    // `from_page_id`, or "unknown" if the link is not found, or "untitled" if
    // the target page is not found.
    std::string LinkText(index_t from_page_id, index_t to_page_id) const;

    // Returns a reference to a target page of the form "#123 (ToTitle)", or
    // "#123 (ToTitle; displayed as Text)" if the target page is linked from the
    // source page with a link text different from the title of the target page.
    std::string ForwardLinkRef(index_t from_page_id, index_t to_page_id) const;

    // Similar to ForwardLinkRef, but for backward links. Note that the form
    // "123 (FromTitle; displayed as Text)" means the origin page with title
    // "FromTitle", has a link with text "Text" to the target page.
    std::string BackwardLinkRef(index_t from_page_id, index_t to_page_id) const;

private:
    Reader(std::unique_ptr<GraphReader> graph, std::unique_ptr<MetadataReader> metadata);

    std::unique_ptr<GraphReader> graph;
    std::unique_ptr<MetadataReader> metadata;
};

}  // namespace wikipath

#endif  // ndef WIKIPATH_READER_H_INCLUDED
