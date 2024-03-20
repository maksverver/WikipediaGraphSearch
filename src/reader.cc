#include "wikipath/reader.h"

#include "wikipath/pipe-trick.h"
#include "wikipath/random.h"

#include <assert.h>
#include <stdlib.h>

#include <iostream>
#include <optional>
#include <sstream>

namespace wikipath {
namespace {

std::string StripExtension(std::string s) {
    return s.substr(0, s.rfind('.'));
}

std::string LinkRef(index_t page_id, std::string_view title, std::string_view link_target, std::string_view link_text) {
    std::ostringstream oss;
    oss << '#' << page_id;
    oss << " (" << title;
    if (link_text != link_target) {
        oss << "; displayed as: " << link_text;
    }
    oss << ")";
    return oss.str();
}

}  // namespace

std::string PageRef(index_t id, std::string_view title) {
    std::ostringstream oss;
    oss << '#' << id << " (" << title << ")";
    return oss.str();
}

std::string ForwardLinkRef(
        index_t to_page_id, std::string_view to_title,
        std::string_view link_text) {
    return LinkRef(to_page_id, to_title, to_title, link_text);
}

std::string BackwardLinkRef(
        index_t from_page_id, std::string_view from_title,
        std::string_view to_title, std::string_view link_text) {
    return LinkRef(from_page_id, from_title, to_title, link_text);
}

std::unique_ptr<Reader> Reader::Open(const char *graph_filename) {
    std::unique_ptr<GraphReader> graph_reader = GraphReader::Open(graph_filename);
    if (graph_reader == nullptr) {
        std::cerr << "Could not open graph file [" << graph_filename << "]\n";
        return nullptr;
    }

    std::string metadata_filename = StripExtension(graph_filename) + ".metadata";
    std::unique_ptr<MetadataReader> metadata_reader = MetadataReader::Open(metadata_filename.c_str());
    if (metadata_reader == nullptr) {
        std::cerr << "Could not open metadata file [" << metadata_filename << "]\n";
        return nullptr;
    }

    return std::unique_ptr<Reader>(new Reader(std::move(graph_reader), std::move(metadata_reader)));
}

Reader::Reader(std::unique_ptr<GraphReader> graph, std::unique_ptr<MetadataReader> metadata)
        : graph(std::move(graph)), metadata(std::move(metadata)) {
}

index_t Reader::RandomPageId() const {
    index_t size = graph->VertexCount();
    if (size < 2) {
        std::cerr << "Graph is empty!\n";
        return 0;
    }
    // To keep things interesting, we only select pages with both at least one
    // incoming and one outgoing link. In particular, most disambiguation pages
    // have no incoming links, and so they cannot be the destination of a
    // shortest path.
    //
    // Note: we make only 20 attempts to find a suitable page, to keep an upper
    // bound on the time taken by this function.
    index_t result = 0;
    for (int attempt = 0; attempt < 20; ++attempt) {
        result = RandInt<index_t>(1, size - 1);
        if (graph->ForwardEdges(result).empty()) continue;
        if (graph->BackwardEdges(result).empty()) continue;
        break;
    }
    return result;
}

index_t Reader::ParsePageArgument(const char *arg) const {
    if (arg == nullptr || !*arg) {
        std::cerr << "Invalid page reference: empty string!\n";
        return 0;
    }

    // Numerical page reference: "#123" where 123 is a page id.
    if (arg[0] == '#') {
        const char *begin = &arg[1];
        char *end = nullptr;
        long long i = strtoll(begin, &end, 10);
        if (!*begin || *end) {
           std::cerr << "Page id [" << arg << "] is malformed.\n";
           return 0;
        }
        if (!IsValidPageId(i)) {
           std::cerr << "Page id [" << arg << "] is out of range!\n";
           return 0;
        }
        return (index_t) i;
    }

    // Random page: "?"
    if (arg[0] == '?' && arg[1] == '\0') {
        return RandomPageId();
    }

    // Lookup page by title.
    std::optional<MetadataReader::Page> page = metadata->GetPageByTitle(arg);
    if (!page) {
        std::cerr << "Page with title [" << arg << "] not found! (Note: titles are case-sensitive.)\n";
    }
    return page ? page->id : 0;
}

std::string Reader::PageTitle(index_t id) const {
    std::optional<MetadataReader::Page> page = metadata->GetPageById(id);
    return page ? page->title : "untitled";
}

std::string Reader::PageRef(index_t id) const {
    return wikipath::PageRef(id, PageTitle(id));
}

std::string Reader::LinkText(index_t from_page_id, index_t to_page_id) const {
    std::optional<MetadataReader::Link> link = metadata->GetLink(from_page_id, to_page_id);
    if (!link) return "unknown";
    if (link->title && !link->title->empty()) return *link->title;   // [[Foo|Bar]] -> "Bar"
    std::string target_title = PageTitle(to_page_id);
    if (!link->title) return target_title;  // [[Foo]] -> "Foo"
    assert(link->title->empty());
    return std::string(ResolvePipeTrick(target_title));  // [[cat:Foo (bar)]] -> "Foo"
}

std::string Reader::ForwardLinkRef(index_t from_page_id, index_t to_page_id) const {
    return wikipath::ForwardLinkRef(to_page_id, PageTitle(to_page_id), LinkText(from_page_id, to_page_id));
}

std::string Reader::BackwardLinkRef(index_t from_page_id, index_t to_page_id) const {
    return wikipath::BackwardLinkRef(from_page_id, PageTitle(from_page_id),
            PageTitle(to_page_id), LinkText(from_page_id, to_page_id));
}

}  // namespace wikipath
