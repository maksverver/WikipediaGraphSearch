#include "graph-reader.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {

enum HeaderFields {
    HEADER_MAGIC,
    HEADER_FILE_SIZE,
    HEADER_GRAPH_SIZE,
    HEADER_FORWARD_EDGE_INDEX_OFFSET,
    HEADER_BACKWARD_EDGE_INDEX_OFFSET,
    HEADER_FIELD_COUNT
};

struct FdCloser {
    const int fd;
    ~FdCloser() { close(fd); }
};

}  // namespace

GraphReader::GraphReader(uint32_t *data, size_t data_len)
        : data(data), data_len(data_len),
          forward_index(&data[data[HEADER_FORWARD_EDGE_INDEX_OFFSET]]),
          backward_index(&data[data[HEADER_BACKWARD_EDGE_INDEX_OFFSET]]),
          size(data[HEADER_GRAPH_SIZE]) {}

GraphReader::~GraphReader() {
    munmap((void*) data, data_len);
}

std::unique_ptr<GraphReader> GraphReader::Open(const char *filename) {
    int fd = open(filename, O_RDONLY);
    if (fd < 0) return nullptr;
    FdCloser fd_closer{fd};

    // Read file header.
    uint32_t header[HEADER_FIELD_COUNT] = {};
    if (read(fd, &header, sizeof(header)) != sizeof(header)) return nullptr;
    if (header[HEADER_MAGIC] != 0x57494b49) return nullptr;
    size_t data_len = header[HEADER_FILE_SIZE] * sizeof(uint32_t);
    // Detect overflow in data_len if size_t is 32 bits:
    if (data_len / sizeof(uint32_t) != header[HEADER_FILE_SIZE]) return nullptr;

    // Map entire file into memory.
    void *data = mmap(nullptr, data_len, PROT_READ, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED) return nullptr;

    return std::unique_ptr<GraphReader>(new GraphReader((uint32_t*) data, data_len));
}
