#include "wikipath/graph-reader.h"

#include "wikipath/graph-header.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <thread>

namespace wikipath {
namespace {

struct FdCloser {
    const int fd;
    ~FdCloser() { close(fd); }
};

static bool MLock(void *data, size_t data_len) {
    auto start = std::chrono::steady_clock::now();
    if (mlock(data, data_len) != 0) {
        perror("mlock");
        return false;
    }
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
    std::cerr << "mlock() succeeded in " << elapsed_ms.count() / 1000.0 << " s\n";
    return true;
}

}  // namespace

GraphReader::GraphReader(void *data, size_t data_len) {
    this->data = data;
    this->data_len = data_len;

    const uint32_t *words = reinterpret_cast<const uint32_t*>(data);

    vertex_count = words[GRAPH_HEADER_VERTEX_COUNT];
    edge_count = words[GRAPH_HEADER_EDGE_COUNT];

    const uint32_t *forward_edges_index = words + GRAPH_HEADER_FIELD_COUNT;
    const uint32_t *forward_edges_edges = forward_edges_index + vertex_count + 1;
    const uint32_t *backward_edges_index = forward_edges_edges + edge_count;
    const uint32_t *backward_edges_edges = backward_edges_index + vertex_count + 1;
    const uint32_t *end_of_file = backward_edges_edges + edge_count;

    forward_edges = edges_index_t{
        .index = forward_edges_index,
        .edges = forward_edges_edges,
    };

    backward_edges = edges_index_t{
        .index = backward_edges_index,
        .edges = backward_edges_edges,
    };

    // A few sanity checks. It's not feasible to check the entire file.
    assert(data_len / sizeof(uint32_t) == static_cast<size_t>(end_of_file - words));
    assert(forward_edges_index[0] == 0);
    assert(forward_edges_index[vertex_count] == edge_count);
    assert(backward_edges_index[0] == 0);
    assert(backward_edges_index[vertex_count] == edge_count);
}

GraphReader::~GraphReader() {
    munmap(data, data_len);
}

std::unique_ptr<GraphReader> GraphReader::Open(const char *filename, OpenOptions options) {
    int fd = open(filename, O_RDONLY);
    if (fd < 0) return nullptr;
    FdCloser fd_closer{fd};

    // Read file header.
    uint32_t header[GRAPH_HEADER_FIELD_COUNT] = {};
    if (read(fd, &header, sizeof(header)) != sizeof(header)) return nullptr;
    if (header[GRAPH_HEADER_MAGIC] != graph_header_magic_value) return nullptr;
    uint64_t file_size =
        uint64_t{GRAPH_HEADER_FIELD_COUNT} * 4 +
        uint64_t{header[GRAPH_HEADER_VERTEX_COUNT] + 1} * 8 +
        uint64_t{header[GRAPH_HEADER_EDGE_COUNT]} * 8;
    if (file_size > std::numeric_limits<size_t>::max()) return nullptr;
    size_t data_len = file_size;

    // Map entire file into memory.
    int mmap_flags = MAP_PRIVATE;
    if (options.mlock == OpenOptions::MLock::POPULATE) mmap_flags |= MAP_POPULATE;
    void *data = mmap(nullptr, data_len, PROT_READ, mmap_flags, fd, 0);
    if (data == MAP_FAILED) return nullptr;

    // Lock file into memory, if requested:
    switch (options.mlock) {
        case OpenOptions::MLock::NONE:
        case OpenOptions::MLock::POPULATE:
            break;

        case OpenOptions::MLock::FOREGROUND:
            if (!MLock(data, data_len)) {
                munmap(data, data_len);
                return nullptr;
            }
            break;

        case OpenOptions::MLock::BACKGROUND:
            {
                std::thread mlock_thread(&MLock, data, data_len);
                mlock_thread.detach();
            }
            break;
    }

    return std::unique_ptr<GraphReader>(
        new GraphReader(data, data_len));
}

}  // namespace wikipath
