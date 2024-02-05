#include "wikipath/common.h"
#include "wikipath/graph-writer.h"
#include "wikipath/metadata-writer.h"
#include "wikipath/parser.h"

#include <assert.h>
#include <cstdlib>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <sstream>
#include <unordered_set>
#include <unordered_map>
#include <vector>

namespace wikipath {
namespace {

// If true, the indexer excludes all redirect pages.
const bool exclude_redirects = true;

// Set of namespace prefixes to exclude. For example, pages with a title of
// "Category:Foo" would be excluded by this. The list here is derived from the
// namespaces for the English Wikipedia, defined at:
// https://en.wikipedia.org/wiki/Wikipedia:Namespace
const std::unordered_set<std::string_view> exclude_namespaces = {
                         "Talk",
    "User",              "User talk",
    "Wikipedia",         "Wikipedia talk",
    "File",              "File talk",
    "MediaWiki",         "MediaWiki talk",
    "Template",          "Template talk",
    "Help",              "Help talk",
    "Category",          "Category talk",
    "Portal",            "Portal talk",
    "Draft",             "Draft talk",
    "TimedText",         "TimedText talk",
    "Module",            "Module talk",
    "Book",              "Book talk",
    "Education Program", "Education Program talk",
    "Gadget",            "Gadget talk",
    "Gadget definition", "Gadget definition talk",
    "Special",
    "Media",
};

// Log only 1 out of every 1000 messages about excluded pages.
const int exclude_log_interval = 1000;

std::vector<std::string> page_titles = {""};
std::unordered_map<std::string, index_t> page_index = {{"", 0}};

std::vector<std::vector<index_t>> outlinks;
std::vector<std::vector<index_t>> inlinks;

int64_t excluded_pages = 0;
int64_t total_links = 0;
int64_t unique_valid_links = 0;

struct Link {
    std::string target;
    std::optional<std::string> anchor;
    std::optional<std::string> title;
};

struct Edge {
    index_t page_i;
    index_t page_j;
    std::string link_title;
};

std::vector<Edge> edges;

std::unique_ptr<MetadataWriter> metadata_writer;

//  Possible link forms:
//    [[Target]]
//    [[Prefix:Target]]
//    [[Prefix:Target#anchor]]
//    [[#internal]]
//    [[Target|]]  (empty title renders as "Target")
//    [[Foo:Bar (Quux)|]]   (renders as "Bar)
//    [[:Foo]]   (renders as "Foo")
//    [[:Foo:Bar]]   (renders as "Foo:Bar")
//
//  Details:
//    https://www.mediawiki.org/wiki/Help:Links
//    https://en.wikipedia.org/wiki/Help:Pipe_trick
//    https://en.wikipedia.org/wiki/Help:Colon_trick
Link ParseLink(std::string_view text) {
    if (text.starts_with(':')) text.remove_prefix(1);
    Link link;
    std::string_view::size_type pipe_pos = text.find('|');
    std::string_view target_with_anchor = text.substr(0, pipe_pos);
    std::string_view::size_type hash_pos = target_with_anchor.find('#');
    link.target = target_with_anchor.substr(0, hash_pos);
    if (hash_pos != std::string_view::npos) {
        link.anchor = target_with_anchor.substr(hash_pos + 1);
    }
    if (pipe_pos != std::string_view::npos) {
        link.title = text.substr(pipe_pos + 1);
    }
    return link;
}

// Note: links may also be nested, e.g.:
// [[File:Paolo Monti - Servizio fotografico (Napoli, 1969) - BEIC 6353768.jpg|thumb|upright=.7|[[Zeno of Citium]] (c. 334 – c. 262 BC), whose ''[[Republic (Zeno)|Republic]]'' inspired [[Peter Kropotkin]]{{sfn|Marshall|1993|p=70}}]]

std::map<std::string, std::optional<std::string>> ExtractLinks(const std::string &current_page, std::string_view text) {
    std::map<std::string, std::optional<std::string>> links;
    std::string_view::size_type pos = 0;
    std::vector<std::string_view::size_type> starts;
    while (pos + 1 < text.size()) {
        if (text[pos] == '[' && text[pos + 1] == '[') {
            pos += 2;
            starts.push_back(pos);
        } else if (text[pos] == ']' && text[pos + 1] == ']') {
            if (!starts.empty()) {
                std::string_view::size_type start = starts.back();
                starts.pop_back();
                Link link = ParseLink(text.substr(start, pos - start));
                // Ignore self links, and links to media files.
                if (!link.target.empty() && link.target != current_page) {
                    // Only keep the first occurrence of each link target.
                    if (links.find(link.target) == links.end()) {
                        links[std::move(link.target)] = std::move(link.title);
                    }
                }
                ++total_links;
            }
            pos += 2;
        } else {
            ++pos;
        }
    }
    return links;
}

index_t GetPageIndex(const std::string &title) {
    auto it = page_index.find(title);
    return it != page_index.end() ? it->second : 0;
}

bool IncludePage(const std::string_view &title, const std::string_view &redirect) {
    if (title.empty()) {
        if (excluded_pages++ % exclude_log_interval == 0) {
            std::cerr << "Excluding page with empty title!\n";
        }
        return false;
    }
    if (exclude_redirects && !redirect.empty()) {
        if (excluded_pages++ % exclude_log_interval == 0) {
            std::cerr << "Excluding redirect from [" << title << "] to [" << redirect << "]\n";
        }
        return false;
    }
    std::string_view::size_type i = title.find(':');
    if (i != std::string_view::npos && exclude_namespaces.find(title.substr(0, i)) != exclude_namespaces.end()) {
        if (excluded_pages++ % exclude_log_interval == 0) {
            std::cerr << "Excluded page in namespace [" << title << "]\n";
        }
        return false;
    }
    return true;
}

struct ParsePageTitles : public ParserCallback {
    virtual void HandlePage(const std::string &title, const std::string &, const std::string &redirect) {
        if (!IncludePage(title, redirect)) return;
        if (page_index.find(title) != page_index.end()) {
            std::cerr << "Ignoring page with duplicate title: [" << title << "]\n";
        } else {
            index_t i = page_titles.size();
            assert(i > 0);  // detect overflow
            page_titles.push_back(title);
            page_index[title] = i;
            metadata_writer->InsertPage(i, title);
        }
    }
};

struct ParseLinks : public ParserCallback {
    ParseLinks() {
        outlinks = {{}};
    }

    virtual void HandlePage(const std::string &title, const std::string &text, const std::string &redirect) {
        if (!IncludePage(title, redirect)) return;
        index_t i = GetPageIndex(title);
        if (i < outlinks.size()) {
            std::cerr << "Ignoring page with duplicate title: [" << title << "]\n";
            return;
        }
        assert(i == outlinks.size());

        std::vector<index_t> v;
        for (const auto &[target, title] : ExtractLinks(title, text)) {
            index_t j = GetPageIndex(target);
            assert(i != j);
            if (j > 0) {
                ++unique_valid_links;
                v.push_back(j);
                metadata_writer->InsertLink(i, j, title);
            }
        }
        std::sort(v.begin(), v.end());
        outlinks.push_back(std::move(v));
    }
};

}  // namespace

bool RunIndexer(
        const std::string &pages_filename,
        const std::string &graph_filename,
        const std::string &metadata_filename) {

    metadata_writer = MetadataWriter::Create(metadata_filename.c_str());
    if (metadata_writer == nullptr) {
        std::cerr << "Could not create metadata output file [" << metadata_filename << "]\n";
        return false;
    }

    // Process the XML input file.

    {   // Pass 1: extract all article titles, and assign them a number.
        ParsePageTitles extract_page_titles;
        if (ParseFile(pages_filename.c_str(), extract_page_titles) != 0) {
            std::cerr << "Failed to parse [" << pages_filename << "]\n";
            return false;
        }
        std::cout << "Included pages: " << page_titles.size() - 1 << '\n';
        std::cout << "Excluded pages: " << excluded_pages << '\n';
    }

    {   // Pass 2: extract all outgoing links to existing articles.
        ParseLinks extract_links;
        if (ParseFile(pages_filename.c_str(), extract_links) != 0) {
            std::cerr << "Failed to parse [" << pages_filename << "]\n";
            return false;
        }
        std::cout << "Total links: " << total_links << '\n';
        std::cout << "Unique valid links: " << unique_valid_links << '\n';
    }

    {   // Pass 3: build inverted index of incoming links per article.
        assert(outlinks.size() == page_titles.size());
        inlinks.assign(page_titles.size(), {});
        for (index_t i = 0; i < outlinks.size(); ++i) {
            for (index_t j : outlinks[i]) {
                assert(j < inlinks.size());
                inlinks[j].push_back(i);
            }
        }
        for (std::vector<index_t> &elem : inlinks) {
            std::sort(elem.begin(), elem.end());
        }
    }

    // Delete MetadataWriter, which causes transaction to be committed.
    metadata_writer = nullptr;

    if (!WriteGraphOutput(graph_filename.c_str(), outlinks, inlinks)) {
        std::cerr << "Could not write graph output file [" << graph_filename << "]\n";
        return false;
    }

    return true;
}

}  // namespace wikipath

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <pages-articles.xml>" << std::endl;
        return EXIT_FAILURE;
    }

    std::string pages_filename(argv[1]);
    std::string base_filename = pages_filename;
    base_filename = base_filename.substr(0, base_filename.rfind('.'));
    std::string graph_filename = base_filename + ".graph";
    std::string metadata_filename = base_filename + ".metadata";
    if (std::filesystem::exists(graph_filename)) {
        std::cerr << "Graph output file already exists [" << graph_filename << "]\n";
        return EXIT_FAILURE;
    }
    if (std::filesystem::exists(metadata_filename)) {
        std::cerr << "Metadata output file already exists [" << metadata_filename << "]\n";
        return EXIT_FAILURE;
    }

    if (!wikipath::RunIndexer(pages_filename, graph_filename, metadata_filename)) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
