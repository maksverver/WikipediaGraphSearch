#include "wikipath/annotated-dag.h"
#include "wikipath/common.h"
#include "wikipath/random.h"
#include "wikipath/reader.h"
#include "wikipath/searcher.h"

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <sstream>
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

bool ParseDagOutputType(std::string_view sv, DagOutputType &type) {
    if (sv == "count") return type = DagOutputType::COUNT, true;
    if (sv == "path")  return type = DagOutputType::PATH,  true;
    if (sv == "paths") return type = DagOutputType::PATHS, true;
    if (sv == "edges") return type = DagOutputType::EDGES, true;
    if (sv == "dot")   return type = DagOutputType::DOT,   true;
    return false;
}

bool ParseLinkOrder(std::string_view sv, wikipath::LinkOrder &order) {
    if (sv == "id")    return order = wikipath::LinkOrder::ID,    true;
    if (sv == "title") return order = wikipath::LinkOrder::TITLE, true;
    if (sv == "text")  return order = wikipath::LinkOrder::TEXT,  true;
    return false;
}

enum class EnumerationMethod {
    RECURSIVE,
    ITERATIVE,
};

bool ParseEnumerationMethod(std::string_view sv, EnumerationMethod &type) {
    if (sv == "recursive") return type = EnumerationMethod::RECURSIVE, true;
    if (sv == "iterative") return type = EnumerationMethod::ITERATIVE, true;
    return false;
}

}  // namespace

namespace wikipath {
namespace {

void DumpSearchStats(const SearchStats stats) {
    std::cerr << "Vertices reached:  " << stats.vertices_reached << '\n';
    std::cerr << "Vertices expanded: " << stats.vertices_expanded << '\n';
    std::cerr << "Edges expanded:    " << stats.edges_expanded << '\n';
    std::cerr << "Search time:       " << stats.time_taken_ms << " ms\n";
}

bool SearchClassic(Reader &reader, index_t start, index_t finish) {
    SearchStats stats;
    std::vector<index_t> path = FindShortestPath(reader.Graph(), start, finish, &stats);
    DumpSearchStats(stats);
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
    return true;
}

struct RecursiveEnumerator {
    template <class CallbackT>
    bool operator()(const AnnotatedDag &dag, LinkOrder order, int64_t skip, CallbackT callback) {
        return dag.EnumeratePaths(std::move(callback), skip, order);
    }
};

struct IterativeEnumerator {
    template <class CallbackT>
    bool operator()(const AnnotatedDag &dag, LinkOrder order, int64_t skip, CallbackT callback) {
        for (PathEnumerator e(dag, skip, order); e.HasPath(); e.Advance()) {
            if (!callback(e.Path())) return false;
        }
        return true;
    }
};

template <class EnumerateT> struct PathPrinter {
    // Prints a single path through the DAG. It is either the first path in
    // the given link order, or if random == true, a randomly selected path.
    void operator()(EnumerateT enumerate, const AnnotatedDag &dag, LinkOrder order, bool random) {
        int64_t skip = 0;
        if (random) {
            int64_t path_count = dag.CountPaths();
            skip = RandInt<int64_t>(0, path_count - 1);
            std::cerr << "Randomly selected path " << skip + 1 << " of " << path_count << ".\n";
        }
        enumerate(dag, order, skip, [&dag](AnnotatedDag::path_t path) {
            std::cout << dag.Start()->Ref() << '\n';
            for (const AnnotatedLink *link : path) {
                std::cout << link->ForwardRef() << '\n';
            }
            return false;  // stop enumerating after first result
        });
    }
};

template <class EnumerateT> struct PathsPrinter {
    // Prints multiple paths through the DAG in the given link order, after
    // skipping the first `skip` paths, and stopping after printing `max` paths.
    void operator()(EnumerateT enumerate, const AnnotatedDag &dag, LinkOrder order, int64_t skip, int64_t max) {
        if (max <= 0) return;
        if (skip < 0) skip = 0;
        enumerate(dag, order, skip, [&dag, &max](AnnotatedDag::path_t path) {
            std::cout << dag.Start()->Ref();
            for (const AnnotatedLink *link : path) {
                std::cout << " -> " << link->ForwardRef();
            }
            std::cout << '\n';
            return --max > 0;
        });
    }
};

// Helper function to invoke the above functors with different enumerator
// implementations depending on the EnumerationMethod argument.
template <template <class> class F, typename ...Args>
void CallWithEnumerator(EnumerationMethod em, Args&&...args) {
    switch (em) {
        case EnumerationMethod::RECURSIVE:
            return F<RecursiveEnumerator>{}({}, std::forward<Args>(args)...);
        case EnumerationMethod::ITERATIVE:
            return F<IterativeEnumerator>{}({}, std::forward<Args>(args)...);
    }
    assert(false);  // unreachable
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

bool StripPrefix(std::string_view &sv, std::string_view prefix) {
    if (!sv.starts_with(prefix)) return false;
    sv.remove_prefix(prefix.size());
    return true;
}

template <class T>
bool ParseArg(std::string_view sv, T &value) {
  std::istringstream iss((std::string(sv)));
  return (iss >> value) && iss.peek() == std::istringstream::traits_type::eof();
}

struct Options {
    const char *graph_filename = nullptr;
    const char *start = nullptr;
    const char *finish = nullptr;
    DagOutputType output_type = DagOutputType::NONE;
    wikipath::LinkOrder order = wikipath::DEFAULT_LINK_ORDER;
    EnumerationMethod enumerate = EnumerationMethod::RECURSIVE;
    bool random = false;
    int64_t skip = 0;
    int64_t max = std::numeric_limits<int64_t>::max();

    bool Parse(int argc, char *argv[]) {
        if (argc < 4) {
            std::cerr << "Missing required arguments.\n";
            return false;
        }
        graph_filename = argv[1];
        start = argv[2];
        finish = argv[3];
        if (argc == 4) return true;
        if (!ParseDagOutputType(argv[4], output_type)) {
            std::cerr << "Invalid DAG output type: " << argv[4] << '\n';
            return false;
        }
        for (int i = 5; i < argc; ++i) {
            std::string_view arg(argv[i]);
            if (output_type == DagOutputType::PATH && arg == "--random") {
                random = true;
            } else if (output_type == DagOutputType::PATHS && StripPrefix(arg, "--skip=")) {
                if (!ParseArg(arg, skip)) {
                    std::cerr << "Could not parse --skip value: " << arg << '\n';
                    return false;
                }
            } else if (output_type == DagOutputType::PATHS && StripPrefix(arg, "--max=")) {
                if (!ParseArg(arg, max)) {
                    std::cerr << "Could not parse --max value: " << arg << '\n';
                    return false;
                }
            } else if ((output_type == DagOutputType::PATH || output_type == DagOutputType::PATHS)
                    && StripPrefix(arg, "--order=")) {
                if (!ParseLinkOrder(arg, order)) {
                    std::cerr << "Could not parse --order value: " << arg << '\n';
                    return false;
                }
            } else if ((output_type == DagOutputType::PATH || output_type == DagOutputType::PATHS)
                    && StripPrefix(arg, "--enumerate=")) {
                if (!ParseEnumerationMethod(arg, enumerate)) {
                    std::cerr << "Could not parse --enumerate value: " << arg << '\n';
                    return false;
                }
            } else {
                std::cerr << "Unrecognized argument: " << arg << '\n';
                return false;
            }
        }
        if (skip < 0) {
            std::cerr << "Invalid value for --skip: " << skip << '\n';
            return false;
        }
        if (max < 0) {
            std::cerr << "Invalid value for --max: " << max << '\n';
            return false;
        }
        return true;
    };
};

void PrintUsage(const char *argv0) {
    std::cout << "Usage: " << argv0 << " <wiki.graph> <Start|#id|?> <Finish|#id|?> [<dag-output>]\n\n"
        "If <dag-output> is present, the DAG-based algorithm is used instead of the classic\n"
        "algorithm. The value of <dag-output> determines what is printed:\n"
        "\n"
        "  count    total number of shortest paths\n"
        "  path     a single shortest path, same as the classic algorithm\n"
        "  paths    all shortest paths, one per line\n"
        "  edges    the edges in the DAG, one per line\n"
        "  dot      the DAG in GraphViz DOT format\n"
        "\n"
        "When <dag-output> is \"path\", the following options are available:\n"
        "\n"
        "  --random     select a path uniformly at random\n"
        "\n"
        "When <dag-output> is \"paths\", the following options are available:\n"
        "\n"
        "  --skip=<N>   skip the first N paths\n"
        "  --max=<N>    print at most N paths\n"
        "\n"
        "When <dag-output> is \"path\" or \"paths\", the following options are available:\n"
        "\n"
        "   --order=<key>  order paths lexicographically by the given key; one of:\n"
        "                       \"id\"     page id (default)\n"
        "                       \"title\"  page title\n"
        "                       \"text\"   link text\n"
        "   --enumerate=<method>  selects the method used to enumerate paths; either \n"
        "                         \"recursive\" (default) or \"iterative\".\n"
        "\n"
        "If <dag-output> is missing, then a single shortest path is printed, calculated using\n"
        "an older algorithm. The output is similar to \"path\", but slightly faster because it\n"
        "only calculates a single path and not the entire DAG of shortest paths.\n"
        << std::flush;
}

bool Main(const Options &options) {
    using namespace wikipath;

    std::unique_ptr<Reader> reader = Reader::Open(options.graph_filename);
    if (reader == nullptr) return false;

    index_t start = reader->ParsePageArgument(options.start);
    index_t finish = reader->ParsePageArgument(options.finish);
    if (start == 0 || finish == 0) return false;

    std::cerr << "Searching shortest path from " << reader->PageRef(start) << " to " << reader->PageRef(finish) << "..." << std::endl;

    if (options.output_type == DagOutputType::NONE) {
        return SearchClassic(*reader, start, finish);
    }

    SearchStats stats;
    auto dag = FindShortestPathDag(reader->Graph(), start, finish, &stats);
    DumpSearchStats(stats);

    if (dag) {
        AnnotatedDag annotated_dag(reader.get(), start, finish, *dag);

        switch (options.output_type) {
        case DagOutputType::NONE:  // already handled above
            assert(false);
            return false;

        case DagOutputType::COUNT:
            std::cout << annotated_dag.CountPaths() << '\n';
            break;

        case DagOutputType::PATH:
            CallWithEnumerator<PathPrinter>(options.enumerate, annotated_dag, options.order, options.random);
            break;

        case DagOutputType::PATHS:
            CallWithEnumerator<PathsPrinter>(options.enumerate, annotated_dag, options.order, options.skip, options.max);
            break;


        case DagOutputType::EDGES:
            PrintEdges(*reader, *dag);
            break;

        case DagOutputType::DOT:
            PrintDot(*reader, *dag);
            break;
        }
    } else {
        switch (options.output_type) {
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
    return true;
}

}  // namespace

// Command line tool to search for shortest path in the Wikipedia graph.
int main(int argc, char *argv[]) {
    Options args;
    if (!args.Parse(argc, argv)) {
        PrintUsage(argv[0]);
        return EXIT_FAILURE;
    }

    return Main(args) ? EXIT_SUCCESS : EXIT_FAILURE;
}
