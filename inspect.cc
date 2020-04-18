#include "common.h"
#include "reader.h"

#include <assert.h>
#include <stdlib.h>

#include <iostream>
#include <sstream>
#include <string>
#include <utility>

namespace {

std::unique_ptr<Reader> reader;

void Inspect(index_t page_id) {
    std::cout << reader->PageRef(page_id) << '\n';

    std::cout << "Outgoing links:\n";
    for (index_t i : reader->Graph().ForwardEdges(page_id)) {
        std::cout << " -> " << reader->ForwardLinkRef(page_id, i) << '\n';
    }

    std::cout << "Incoming links:\n";
    for (index_t i : reader->Graph().BackwardEdges(page_id)) {
        std::cout << " <- " << reader->BackwardLinkRef(i, page_id) << '\n';
    }
}

}  // namespace

// Simple tool to debug-print vertices of the graph. Mostly for debugging purposes.
int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::cout << "Usage: " << argv[0] << " <wiki.graph> <PageTitle|#page_id|?>" << std::endl;
        return EXIT_FAILURE;
    }

    reader = Reader::Open(argv[1]);
    if (reader == nullptr) {
        return EXIT_FAILURE;
    }
    index_t page_id = reader->ParsePageArgument(argv[2]);
    if (page_id == 0) {
        return EXIT_FAILURE;
    }

    Inspect(page_id);
    return EXIT_SUCCESS;
}
