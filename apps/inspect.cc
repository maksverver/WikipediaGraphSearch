#include "wikipath/common.h"
#include "wikipath/reader.h"

#include <assert.h>
#include <cstdlib>
#include <stdlib.h>

#include <iostream>
#include <sstream>
#include <string>
#include <utility>

namespace wikipath {
namespace {

bool Inspect(const char *graph_filename, const char *page) {
    std::unique_ptr<Reader> reader = Reader::Open(graph_filename);
    if (reader == nullptr) return false;
    index_t page_id = reader->ParsePageArgument(page);
    if (page_id == 0) return false;

    std::cout << reader->PageRef(page_id) << '\n';

    std::cout << "Outgoing links:\n";
    for (index_t i : reader->Graph().ForwardEdges(page_id)) {
        std::cout << " -> " << reader->ForwardLinkRef(page_id, i) << '\n';
    }

    std::cout << "Incoming links:\n";
    for (index_t i : reader->Graph().BackwardEdges(page_id)) {
        std::cout << " <- " << reader->BackwardLinkRef(i, page_id) << '\n';
    }
    return true;
}

}  // namespace
}  // namespace wikipath

// Simple tool to debug-print vertices of the graph. Mostly for debugging purposes.
int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::cout << "Usage: " << argv[0] << " <wiki.graph> <PageTitle|#page_id|?>" << std::endl;
        return EXIT_FAILURE;
    }
    return wikipath::Inspect(argv[1], argv[2]) ? EXIT_SUCCESS : EXIT_FAILURE;
}
