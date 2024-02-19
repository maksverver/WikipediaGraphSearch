#include "wikipath/common.h"
#include "wikipath/graph-writer.h"
#include "wikipath/metadata-writer.h"
#include "wikipath/parser.h"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace wikipath {
namespace {

// Only include pages in the main namespace (0).
const int include_namespace_id = 0;

// If true, the indexer excludes all redirect pages.
const bool exclude_redirects = true;

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

// Parses a link into the target page name and displayed text, discarding the
// section name (if included).
//
// Possible link forms:
//
//    [[Target]]
//    [[Target#anchor]]  (links to a "Target" subsection "anchor")
//    [[target]]  (links to Target but renders as "target")
//    [[Prefix:Target]]
//    [[#internal]]
//    [[Target|]]  (empty title renders as "Target")
//    [[Foo:Bar (Quux)|]]   (renders as "Bar)
//    [[:Foo]]   (renders as "Foo")
//    [[:Foo:Bar]]   (renders as "Foo:Bar")
//
// There is also something called the “inverse pipe trick”: on a page like
// "Foo (bar)" the link"[[|baz]]" would render as "baz" but link to page
// "Baz (bar)". This is extremely rarely used, and not currently supported by
// the indexer. (Currently these links are ignored, because the caller discards
// links with empty target; note that this also includes anchor-based links to
// sections of the current page like "[[#foo]]", which are much more common.)
//
//  Details:
//    https://www.mediawiki.org/wiki/Help:Links
//    https://en.wikipedia.org/wiki/Help:Link
//    https://en.wikipedia.org/wiki/Help:Pipe_trick
//    https://en.wikipedia.org/wiki/Help:Colon_trick
//
Link ParseLink(std::string_view text) {
    if (text.starts_with(':')) text.remove_prefix(1);
    Link link;
    std::string_view::size_type pipe_pos = text.find('|');
    std::string_view target_with_anchor = text.substr(0, pipe_pos);
    std::string_view::size_type hash_pos = target_with_anchor.find('#');
    link.target = target_with_anchor.substr(0, hash_pos);
    if (!link.target.empty() && islower(link.target.front())) {
        link.target.front() = toupper(link.target.front());
    }
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

bool IncludePage(const ParserCallback::Page &page) {
    if (page.title.empty()) {
        if (excluded_pages++ % exclude_log_interval == 0) {
            std::cerr << "Excluding page with empty title!\n";
        }
        return false;
    }
    if (exclude_redirects && !page.redirect.empty()) {
        if (excluded_pages++ % exclude_log_interval == 0) {
            std::cerr << "Excluding redirect from [" << page.title << "] to [" << page.redirect << "]\n";
        }
        return false;
    }
    if (auto ns = page.ParseNs(); ns) {
        if (*ns != include_namespace_id) {
            if (excluded_pages++ % exclude_log_interval == 0) {
                std::cerr << "Excluded page [" << page.title << "] in namespace " << *ns << "\n";
            }
            return false;
        }
    } else {
        // Print this unconditionally, since it should rarely/never happen!
        std::cerr << "No namespace defined for page [" << page.title << "]\n";
        return false;
    }
    return true;
}

struct ParsePageTitles : public ParserCallback {
    virtual void HandlePage(const Page &page) {
        if (!IncludePage(page)) return;
        if (page_index.find(page.title) != page_index.end()) {
            std::cerr << "Ignoring page with duplicate title: [" << page.title << "]\n";
        } else {
            assert(page_titles.size() < std::numeric_limits<index_t>::max());
            index_t i = page_titles.size();
            page_titles.push_back(page.title);
            page_index[page.title] = i;
            metadata_writer->InsertPage(i, page.title);
        }
    }
};

struct ParseLinks : public ParserCallback {
    ParseLinks() {
        outlinks = {{}};
    }

    virtual void HandlePage(const Page &page) {
        if (!IncludePage(page)) return;
        index_t i = GetPageIndex(page.title);
        if (i < outlinks.size()) {
            std::cerr << "Ignoring page with duplicate title: [" << page.title << "]\n";
            return;
        }
        assert(i == outlinks.size());

        std::vector<index_t> v;
        for (const auto &[target, title] : ExtractLinks(page.title, page.text)) {
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
