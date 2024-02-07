#include "wikipath/common.h"

#include "wikipath/graph-header.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include <vector>

namespace wikipath {
namespace {

int64_t CountEdges(const std::vector<std::vector<index_t>> &edgelist) {
    int64_t edge_count = 0;
    for (const auto &adj : edgelist) edge_count += adj.size();
    return edge_count;
}

bool WriteInt(FILE *fp, uint32_t i) {
    return fwrite(&i, 4, 1, fp) == 1;
}

bool WriteInt(FILE *fp, int64_t i) {
    assert(i >= 0 && i <= 0xffffffff);
    return WriteInt(fp, (uint32_t) i);
}

bool WriteEdges(FILE *fp, const std::vector<std::vector<index_t>> &edgelist) {
    int64_t offset = 0;
    for (const auto &adj : edgelist) {
        if (!WriteInt(fp, offset)) return false;
        offset += adj.size();
    }
    if (!WriteInt(fp, offset)) return false;
    for (const auto &adj : edgelist) {
        for (index_t i : adj) {
            if (!WriteInt(fp, i)) return false;
        }
    }
    return true;
}

bool WriteGraphOutput(FILE *fp,
        const std::vector<std::vector<index_t>> &forward_edges,
        const std::vector<std::vector<index_t>> &backward_edges) {
    const int64_t vertex_count = forward_edges.size();  // includes vertex 0!
    const int64_t edge_count = CountEdges(forward_edges);

    // Write header.
    // An exquisite application of the for-case paradigm!
    // See: https://thedailywtf.com/articles/The_FOR-CASE_paradigm
    for (int i = 0; i < GRAPH_HEADER_FIELD_COUNT; ++i) {
        switch (i) {
        case GRAPH_HEADER_MAGIC:
            if (!WriteInt(fp, graph_header_magic_value)) return false;
            break;
        case GRAPH_HEADER_RESERVED:
            if (!WriteInt(fp, graph_header_reserved_value)) return false;
            break;
        case GRAPH_HEADER_VERTEX_COUNT:
            if (!WriteInt(fp, vertex_count)) return false;
            break;
        case GRAPH_HEADER_EDGE_COUNT:
            if (!WriteInt(fp, edge_count)) return false;
            break;
        default:
            abort();
        }
    }

    // Edge data
    if (!WriteEdges(fp, forward_edges)) return false;
    if (!WriteEdges(fp, backward_edges)) return false;
    return true;
}

}  // namespace

bool WriteGraphOutput(
        const char *filename,
        const std::vector<std::vector<index_t>> &outlinks,
        const std::vector<std::vector<index_t>> &inlinks) {
    assert(inlinks.size() == outlinks.size());
    FILE *fp = fopen(filename, "wb");
    if (fp == nullptr) return false;
    bool success = WriteGraphOutput(fp, outlinks, inlinks);
    if (fclose(fp) != 0) success = false;
    return success;
}

}  // namespace wikipath
