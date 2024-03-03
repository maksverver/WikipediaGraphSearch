#include "wikipath/common.h"
#include "wikipath/reader.h"
#include "wikipath/searcher.h"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <ranges>
#include <string_view>
#include <utility>
#include <unordered_map>

namespace {

enum class DagOutputType {
    NONE,   // Don't use DAG algorithm and print a single path (like PATH).
    COUNT,  // Output total number of paths.
    PATH,   // Output a single path.
    PATHS,  // Output all paths, one per line.
    EDGES,  // Output the edges in the DAG, one per line.
    DOT,    // Output the DAG in GraphViz DOT file format.
};

bool ParseDagOutputType(std::string_view sv, DagOutputType &mode) {
    if (sv == "count") return mode = DagOutputType::COUNT, true;
    if (sv == "path")  return mode = DagOutputType::PATH,  true;
    if (sv == "paths") return mode = DagOutputType::PATHS, true;
    if (sv == "edges") return mode = DagOutputType::EDGES, true;
    if (sv == "dot")   return mode = DagOutputType::DOT,   true;
    return false;
}

}  // namespace

namespace wikipath {
namespace {

void DumpSearchStats(const SearchStats stats) {
    std::cerr << "Vertices reached:  " << stats.vertices_reached << '\n';
    std::cerr << "Vertices expanded: " << stats.vertices_expanded << '\n';
    std::cerr << "Edges expanded:    " << stats.edges_expanded << '\n';
    std::cerr << "Time taken:        " << stats.time_taken_ms << " ms\n";
}


bool SearchClassic(Reader &reader, index_t start, index_t finish) {
    SearchStats stats;
    std::vector<index_t> path = FindShortestPath(reader.Graph(), start, finish, &stats);
    if (path.empty()) {
        std::cerr << "No path found!\n";
    } else {
        for (size_t i = 0; i < path.size(); ++i) {
            if (i == 0) {
                std::cout << reader.PageRef(path[i]) << '\n';
            } else {
                std::cout << reader.ForwardLinkRef(path[i - 1], path[i]) << '\n';
            }
        }
    }
    DumpSearchStats(stats);
    return true;
}

index_t Source(const std::pair<index_t, index_t> &edge) { return edge.first; }
index_t Destination(const std::pair<index_t, index_t> &edge) { return edge.second; }

int64_t CountPaths(const std::vector<std::pair<index_t, index_t>> &dag, index_t start, index_t finish) {
    std::unordered_map<index_t, int64_t> count_by_vertex;
    std::vector<index_t> todo;
    count_by_vertex[start] = 1;
    todo.push_back(start);
    for (size_t i = 0; i < todo.size(); ++i) {
        index_t v = todo[i];
        for (index_t w :
                std::ranges::equal_range(dag, v, {}, Source) |
                std::views::transform(Destination)) {
            auto &count = count_by_vertex[w];
            if (count == 0) todo.push_back(w);
            count += count_by_vertex[v];
        }
    }
    return count_by_vertex[finish];
}

void PrintPath(
        Reader &reader, const std::vector<std::pair<index_t, index_t>> &dag,
        index_t start, index_t finish) {
    index_t v = start;
    std::cout << reader.PageRef(v) << '\n';
    while (v != finish) {
        auto it = std::ranges::lower_bound(dag, v, {}, Source);
        assert(it != dag.end());
        assert(it->first == v);
        index_t w = it->second;
        std::cout << reader.ForwardLinkRef(v, w) << '\n';
        v = w;
    }
}

void PrintEdges(Reader &reader, const std::vector<std::pair<index_t, index_t>> &dag) {
    for (auto [v, w] : dag) {
        std::cout << reader.PageRef(v) << " -> " << reader.ForwardLinkRef(v, w) << '\n';
    }
}

struct DotQuotedString {
    std::string_view s;
};

// Prints a DOT quoted string.
std::ostream &operator<<(std::ostream &os, DotQuotedString qs) {
    os << '"';
    for (char ch : qs.s) {
        // Only '"' needs to be escaped in the file format, although the `dot`
        // command line tool also interprets escape sequences like '\n'.
        // Fortunately backslashes occuring in Wikipedia titles are extremely
        // rare (though not nonexistent: pages like "\o/", and "\nnn" exist).
        //
        // Source: https://graphviz.org/doc/info/lang.html
        if (ch == '"') std::cout << '\\';
        os << ch;
    }
    os << '"';
    return os;
}

void PrintDot(Reader &reader, const std::vector<std::pair<index_t, index_t>> &dag) {
    std::unordered_map<index_t, std::string> title_by_page;
    std::vector<index_t> todo;
    auto add_vertex = [&](size_t v) -> std::string_view {
        std::string &title = title_by_page[v];
        if (title.empty()) {
            title = reader.PageTitle(v);
            todo.push_back(v);
            std::cout << v << " [label=" << DotQuotedString(title) << "];\n";
        }
        return title;
    };
    std::cout << "digraph dag {\n";
    for (auto [v, w] : dag) {
        add_vertex(v);
        const auto &dest_title = add_vertex(w);
        const std::string text = reader.LinkText(v, w).value_or("unknown");
        std::cout << v << " -> " << w;
        if (text != dest_title) std::cout << " [label=" << DotQuotedString(text) << "]";
        std::cout << ";\n";
    }
    std::cout << "}\n";
}

template <class CallbackT>
void DfsPaths(
        const std::vector<std::pair<index_t, index_t>> &dag,
        index_t v, index_t finish,
        std::vector<index_t> &path,
        const CallbackT &callback) {
    path.push_back(v);
    if (v == finish) {
        callback(path);
    } else {
        for (index_t w :
                std::ranges::equal_range(dag, v, {}, Source) |
                std::views::transform(Destination)) {
            DfsPaths(dag, w, finish, path, callback);
        }
    }
    path.pop_back();
}

void PrintPaths(
        Reader &reader, const std::vector<std::pair<index_t, index_t>> &dag,
        index_t start, index_t finish) {
    std::vector<index_t> path;
    DfsPaths(dag, start, finish, path, [&reader](const std::vector<index_t> &path) {
        assert(path.size() > 0);
        std::cout << reader.PageRef(path[0]);
        for (size_t i = 1; i < path.size(); ++i) {
            std::cout << " -> " << reader.ForwardLinkRef(path[i - 1], path[i]);
        }
        std::cout << '\n';
    });
    assert(path.empty());
}

}  // namespace
}  // namespace wikipath

namespace {

bool Main(
        DagOutputType output_type,
        const char *graph_filename,
        const char *start_arg,
        const char *finish_arg) {
    using namespace wikipath;

    std::unique_ptr<Reader> reader = Reader::Open(graph_filename);
    if (reader == nullptr) return false;

    index_t start = reader->ParsePageArgument(start_arg);
    index_t finish = reader->ParsePageArgument(finish_arg);
    if (start == 0 || finish == 0) return false;

    std::cerr << "Searching shortest path from " << reader->PageRef(start) << " to " << reader->PageRef(finish) << "..." << std::endl;

    if (output_type == DagOutputType::NONE) {
        return SearchClassic(*reader, start, finish);
    }

    SearchStats stats;
    auto opt_dag = FindShortestPathDag(reader->Graph(), start, finish, &stats);

    switch (output_type) {
    case DagOutputType::NONE:  // already handled above
        assert(false);
        return false;

    case DagOutputType::COUNT:
        std::cout << (opt_dag ? CountPaths(*opt_dag, start, finish) : 0) << '\n';
        break;

    case DagOutputType::PATH:
        if (!opt_dag) {
            std::cerr << "No path found!\n";
        } else {
            PrintPath(*reader, *opt_dag, start, finish);
        }
        break;

    case DagOutputType::PATHS:
        if (opt_dag) PrintPaths(*reader, *opt_dag, start, finish);
        break;

    case DagOutputType::EDGES:
        if (opt_dag) PrintEdges(*reader, *opt_dag);
        break;

    case DagOutputType::DOT:
        if (!opt_dag) {
            std::cerr << "No path found!\n";
        } else {
            PrintDot(*reader, *opt_dag);
        }
        break;
    }

    DumpSearchStats(stats);
    return true;
}

}  // namespace

// Command line tool to search for shortest path in the Wikipedia graph.
int main(int argc, char *argv[]) {
    DagOutputType mode = DagOutputType::NONE;
    if (!(argc == 4 || (argc == 5 && ParseDagOutputType(argv[4], mode)))) {
        std::cout << "Usage: " << argv[0] << " <wiki.graph> <Start|#id|?> <Finish|#id|?> [<dag-output>]\n\n"
            "If <dag-output> is present, the DAG-based algorithm is used instead of the classic\n"
            "algorithm. The value of <dag-output> determines what is printed:\n"
            "\n"
            "  count    total number of shortest paths\n"
            "  path     a single shortest path, same as the classic algorithm\n"
            "  paths    all shortest paths, one per line\n"
            "  edges    the edges in the DAG, one per line\n"
            "  dot      the DAG in GraphViz DOT format\n"
            "\n"
            "If <dag-output> is missing, then a single shortest path is printed, calculated using an older\n"
            "algorithm. The output is similar to \"path\", but slightly faster because it only calculates a single\n"
            "path and not the entire DAG of shortest paths.\n" << std::flush;
        return EXIT_FAILURE;
    }
    return Main(mode, argv[1], argv[2], argv[3]) ? EXIT_SUCCESS : EXIT_FAILURE;
}
