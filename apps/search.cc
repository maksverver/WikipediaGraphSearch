#include "wikipath/common.h"
#include "wikipath/reader.h"
#include "wikipath/searcher.h"

#include <cstdlib>
#include <iostream>

namespace wikipath {
namespace {

bool Search(const char *graph_filename, const char *start_arg, const char *finish_arg) {
    std::unique_ptr<Reader> reader = Reader::Open(graph_filename);
    if (reader == nullptr) return false;

    index_t start = reader->ParsePageArgument(start_arg);
    index_t finish = reader->ParsePageArgument(finish_arg);
    if (start == 0 || finish == 0) return false;

    std::cerr << "Searching shortest path from " << reader->PageRef(start) << " to " << reader->PageRef(finish) << "..." << std::endl;

    SearchStats stats;
    std::vector<index_t> path = FindShortestPath(reader->Graph(), start, finish, &stats);
    if (path.empty()) {
        std::cerr << "No path found!" << std::endl;
    } else {
        for (size_t i = 0; i < path.size(); ++i) {
            if (i == 0) {
                std::cout << reader->PageRef(path[i]) << '\n';
            } else {
                std::cout << reader->ForwardLinkRef(path[i - 1], path[i]) << '\n';
            }
        }
    }
    std::cerr << "Vertices reached:  " << stats.vertices_reached << '\n';
    std::cerr << "Vertices expanded: " << stats.vertices_expanded << '\n';
    std::cerr << "Edges expanded:    " << stats.edges_expanded << '\n';
    std::cerr << "Time taken:        " << stats.time_taken_ms << " ms\n";
    return true;
}

}  // namespace
}  // namespace wikipath

// Command line tool to search for shortest path in the Wikipedia graph.
int main(int argc, char *argv[]) {
    if (argc != 4) {
        std::cout << "Usage: " << argv[0] << " <wiki.graph> <Start|#id|?> <Finish|#id|?>" << std::endl;
        return EXIT_FAILURE;
    }
    return wikipath::Search(argv[1], argv[2], argv[3]) ? EXIT_SUCCESS : EXIT_FAILURE;
}
