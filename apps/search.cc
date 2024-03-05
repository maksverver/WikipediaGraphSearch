#include "wikipath/annotated-dag.h"
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

void PrintFirstPath(const AnnotatedDag &dag) {
    dag.EnumeratePaths([&dag](std::span<const AnnotatedLink*> links) {
        std::cout << dag.Start()->Ref() << '\n';
        for (const AnnotatedLink *link : links) {
            std::cout << link->ForwardRef() << '\n';
        }
        return false;  // stop enumerating after first result
    });
}

void PrintAllPaths(const AnnotatedDag &dag) {
    dag.EnumeratePaths([&dag](std::span<const AnnotatedLink*> links) {
        std::cout << dag.Start()->Ref();
        for (const AnnotatedLink *link : links) {
            std::cout << " -> " << link->ForwardRef();
        }
        std::cout << '\n';
        return true;  // enumerate more
    });
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
        const std::string text = reader.LinkText(v, w);
        std::cout << v << " -> " << w;
        if (text != dest_title) std::cout << " [label=" << DotQuotedString(text) << "]";
        std::cout << ";\n";
    }
    std::cout << "}\n";
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
    if (auto dag = FindShortestPathDag(reader->Graph(), start, finish, &stats)) {
        AnnotatedDag annotated_dag(reader.get(), start, finish, *dag);

        switch (output_type) {
        case DagOutputType::NONE:  // already handled above
            assert(false);
            return false;

        case DagOutputType::COUNT:
            std::cout << annotated_dag.CountPaths() << '\n';
            break;

        case DagOutputType::PATH:
            PrintFirstPath(annotated_dag);
            break;

        case DagOutputType::PATHS:
            PrintAllPaths(annotated_dag);
            break;

        case DagOutputType::EDGES:
            PrintEdges(*reader, *dag);
            break;

        case DagOutputType::DOT:
            PrintDot(*reader, *dag);
            break;
        }
    } else {
        switch (output_type) {
        case DagOutputType::COUNT:
            // For output consistency, output 0 when no path is found.
            std::cout << 0 << '\n';
            break;

        case DagOutputType::PATHS:
        case DagOutputType::EDGES:
            // Empty output when no path is found.
            break;

        default:
            std::cerr << "No path found!\n";
        }
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
