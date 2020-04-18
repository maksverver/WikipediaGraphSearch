#include "reader.h"

#include <string.h>
#include <stdlib.h>

#include <algorithm>
#include <iostream>
#include <sstream>
#include <random>

namespace {

std::mt19937 CreateRng() {
    unsigned random_data[624];
    std::random_device source;
    std::generate(std::begin(random_data), std::end(random_data), std::ref(source));
    std::seed_seq seed_seq(std::begin(random_data), std::end(random_data));
    return std::mt19937(seed_seq);
}

std::mt19937 &Rng() {
    static std::mt19937 rng = CreateRng();
    return rng;
}

std::string StripExtension(std::string s) {
    return s.substr(0, s.rfind('.'));
}

}  // namespace

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

index_t Reader::ParsePageArgument(const char *arg) {
    index_t size = graph->Size();
    if (size < 2) {
        std::cerr << "Graph is empty!\n";
        return 0;
    }

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
        if (i < 1 || i >= graph->Size()) {
           std::cerr << "Page id [" << arg << "] is out of range!\n";
           return 0;
        }
        return (index_t) i;
    }

    // Random page: "?"
    if (arg[0] == '?' && arg[1] == '\0') {
        // Requires size > 1, which we verified at the top of the function.
        return std::uniform_int_distribution<index_t>(1, size - 1)(Rng());
    }

    // Lookup page by title.
    std::optional<MetadataReader::Page> page = metadata->GetPageByTitle(arg);
    if (!page) {
        std::cerr << "Page with title [" << arg << "] not found! (Note: titles are case-sensitive.)\n";
    }
    return page ? page->id : 0;
}

std::string Reader::PageTitle(index_t id) {
    std::optional<MetadataReader::Page> page = metadata->GetPageById(id);
    return page ? page->title : "untitled";
}

std::string Reader::PageRef(index_t id) {
    std::ostringstream oss;
    oss << '#' << id << " (" << PageTitle(id) << ")";
    return oss.str();
}

