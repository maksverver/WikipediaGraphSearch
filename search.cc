#include "common.h"
#include "reader.h"
#include "searcher.h"

#include <iostream>

// Command line tool to search for shortest path in the Wikipedia graph.
int main(int argc, char *argv[]) {
    if (argc != 4) {
        std::cout << "Usage: " << argv[0] << " <wiki.graph> <Start|#id|?> <Finish|#id|?>" << std::endl;
        return EXIT_FAILURE;
    }

    std::unique_ptr<Reader> reader = Reader::Open(argv[1]);
    if (reader == nullptr) {
        return EXIT_FAILURE;
    }

    index_t start = reader->ParsePageArgument(argv[2]);
    index_t finish = reader->ParsePageArgument(argv[3]);
    if (start == 0 || finish == 0) {
        return EXIT_FAILURE;
    }

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
    return EXIT_SUCCESS;
}
